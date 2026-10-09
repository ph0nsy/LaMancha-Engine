/**
 * @file LevelArena.h
 * @brief Level-scoped bump-pointer allocator for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * @details
 * LevelArena holds data whose lifetime matches a game level. It is reset
 * at level unload and repopulated at level load. Between those two events
 * its contents are stable (no individual frees, no per-frame clearing).
 *
 * - Failure policy.
 * @c alloc() returns @c Optional<T*>::None() on overflow. Level overflow is a
 * legitimate runtime condition, levels may vary significantly in entity
 * count. The caller handles @a None by incrementing @c Sync::gTelemetry.oom_ecs_pool
 * (or a future oom_level_alloc counter), skipping the allocation, and
 * optionally pushing an eviction job to free @c IOArena assets that may be
 * consuming memory needed here.
 *
 * This is different from @c FrameArena (sizing bug) and
 * @c PermanentArena (startup bug). Level overflow is survivable.
 * 
 * - Construction.
 *   - @c fromExternal() in production (@c HeapAllocatorLM hands a slice).
 *   - @c fromHeap() for tests and bootstrap.
 *
 * - Thread ownership.
 *   - alloc() > @a LogicThread only (@c CoreAffinity::Logic )
 *   - reset() > @a LogicThread only, after @c SyncPoint handoff with @a MainThread
 *
 * - Level unload sequence:
 *   1. @a LogicThread decides to unload (scene manager, player exit, etc.)
 *   2. @a LogicThread signals Main Thread: "level ending"
 *   3. @a MainThread calls glDelete* for all level GL resources
 *   4. @a MainThread signals back: "GPU clean"
 *   5. @a LogicThread waits for "GPU clean" signal
 *   6. @a LogicThread calls levelArena.unload() (reset + telemetry clear)
 *   7. @a LogicThread begins loading next level
 *
 * The Utility/GC Thread is NOT involved in level arena reset. It handles
 * Lua GC, log draining, and asset eviction (not arena lifecycle).
 *
 * 
 * @note 
 * Use for:
 * - ECS component pool backing memory (PoolAllocatorLM sub-regions)
 * - Level geometry and collision data
 * - Navmesh data
 * - Level-specific Lua scripts and closures
 * - Level-specific audio event tables
 * - Any data that lives exactly as long as the current level
 *
 * @note 
 * Do not use for:
 * - Per-frame transients (use FrameArena)
 * - Engine-lifetime data (use PermanentArena)
 * - Assets with individual lifetimes (use IOArena + PoolAllocatorLM)
 */

#pragma once

#include "arena.h"
#include "scratchpad.h"
#include "sync.h"   ///< Sync::gTelemetry
#include "thread.h" ///< Thread::currentRole(), CoreAffinity

namespace LaMancha {
  struct LevelArenaLM
  {
    /**
     * @brief Create a LevelArena over an externally-owned memory block.
     * The arena does NOT own the memory.
     *
     * @param ptr   Start of the block. Must be non-null.
     * @param size  Capacity in bytes. Must be > 0.
     */
    static LevelArenaLM fromExternal(void* ptr, usize size) noexcept
    {
      LevelArenaLM la;
      la.m_arena = ArenaLM::fromExternal(ptr, size);
      return la;
    }

    /**
     * @brief Create a LevelArena that owns its backing block (malloc).
     * Use for tests and bootstrap only. Check capacity() > 0 after construction.
     *
     * @param size  Capacity in bytes.
     */
    static LevelArenaLM fromHeap(usize size) noexcept
    {
      LevelArenaLM la;
      la.m_arena = ArenaLM::fromHeap(size);
      return la;
    }

    // Non-copyable, movable.
    LevelArenaLM(const LevelArenaLM&) = delete;
    LevelArenaLM& operator=(const LevelArenaLM&) = delete;
    LevelArenaLM(LevelArenaLM&&) = default;
    LevelArenaLM& operator=(LevelArenaLM&&) = default;

    /**
     * @brief Allocate `size` bytes aligned to `align`.
     *
     * Returns None on overflow. The caller is responsible for handling None,
     * typically by incrementing telemetry and skipping or deferring the
     * allocation.
     *
     * @param size Bytes to allocate. Must be > 0.
     * @param align Alignment in bytes. Must be a power of two.
     */
    Optional<void*> alloc(usize size, usize align) noexcept
    {
#if LAMANCHA_DEBUG
      LAMANCHA_ASSERT(Thread::currentRole() == CoreAffinity::Logic, "LevelArena::alloc must only be called from the Logic Thread");
#endif
      Optional<void*> result = m_arena.alloc(size, align);
      if (!result)
      {
        // Level overflow is a runtime condition, not a crash.
        // Increment the ECS pool counter, level data is primarily ECS pools.
        // A future oom_level_alloc counter could be added to Telemetry for
        // finer granularity.
        Sync::gTelemetry.oom_ecs_pool.fetchAddRelaxed(1u);
      }
      return result;
    }

    /**
     * @brief Typed allocation. Returns None on overflow.
     * Memory is uninitialised. Use placement new if construction is needed.
     *
     * @code
     * auto result = levelArena.alloc<TransformComponent>();
     * if (!result) {
     *     // handle overflow: skip entity, log warning, push GC eviction
     *     Sync::gTelemetry.oom_ecs_pool.fetchAddRelaxed(1u);
     *     return;
     * }
     * #pragma push_macro("new")
     * #undef new // Protect against redefinition
     * new (result.value()) TransformComponent{};
     * #pragma pop_macro("new")
     * @endcode
     */
    template <typename T>
    LAMANCHA_INLINE Optional<T*> alloc() noexcept
    {
      if (Optional<void*> raw = alloc(sizeof(T), alignof(T))) 
      { 
        return Optional<T*>::Some(static_cast<T*>(raw.value()));
      }
      return Optional<T*>::None(); 
    }

    /**
     * @brief Typed array allocation. Returns None on overflow.
     * Memory is uninitialised.
     *
     * @param count  Number of elements.
     */
    template <typename T>
    LAMANCHA_INLINE Optional<T*> allocArray(usize count) noexcept
    {
      if (Optional<void*> raw = alloc(sizeof(T) * count, alignof(T))) 
      { 
        return Optional<T*>::Some(static_cast<T*>(raw.value()));
      }
      return Optional<T*>::None(); 
    }

    /**
     * @brief Save the current bump pointer for within-level scratch work.
     *
     * Useful for temporary computations during level load (path baking,
     * navmesh generation scratch, geometry preprocessing) that should not
     * persist into the running level.
     *
     * @code
     * usize mark = levelArena.save();
     * auto* scratch = levelArena.allocArray<NavNode>(MAX_NODES);
     * if (!scratch) { // handle // return; }
     * // ... bake navmesh using scratch ...
     * levelArena.restore(mark);   // scratch memory gone, baked result stays
     * @endcode
     */
    LAMANCHA_INLINE usize save() const noexcept { return m_arena.save(); }
    LAMANCHA_INLINE void restore(usize mark) noexcept { m_arena.restore(mark); }

    /**
     * @brief Create a RAII ScratchpadArena borrowing from this LevelArena.
     *
     * Useful during level load for temporary computations (navmesh baking,
     * geometry preprocessing) that should not persist into the running level.
     *
     * @code
     * {
     *     ScratchpadArena scratch = levelArena.scratch();
     *     auto nodes = scratch.allocArray<NavNode>(MAX_NODES);
     *     if (!nodes) { return Error(ErrorCode::OutOfMemory); }
     *     // ... bake navmesh using nodes ...
     *     // baked result was stored earlier in levelArena, before this scratch
     * }   // scratch nodes returned to levelArena here
     * @endcode
     */
    LAMANCHA_INLINE ScratchpadArenaLM scratch() noexcept { return ScratchpadArenaLM(m_arena); }

    /**
     * @brief Reset the arena at level unload. Moves the bump pointer back to the start. O(1).
     *
     * @details
     * Call ONLY from the Logic Thread, AFTER the Main Thread has signaled
     * that all GPU resources from this level have been deleted. See the
     * level unload sequence in the file header.
     *
     * Does NOT zero memory. Does NOT call destructors.
     * All pointers previously returned by alloc() are invalidated.
     *
     * @note Increments m_levelIndex for debug tracking.
     */
    void unload() noexcept
    {
#if LAMANCHA_DEBUG
      LAMANCHA_ASSERT(Thread::currentRole() == CoreAffinity::Logic, "LevelArena::unload must only be called from the Logic Thread");
#endif
      m_arena.reset();
      m_levelIndex++;
    }

    LAMANCHA_INLINE usize used() const noexcept { return m_arena.used(); }
    LAMANCHA_INLINE usize remaining() const noexcept { return m_arena.remaining(); }
    LAMANCHA_INLINE usize capacity() const noexcept { return m_arena.capacity(); }
    LAMANCHA_INLINE bool  isEmpty() const noexcept { return m_arena.isEmpty(); }

    /**
     * @brief Number of times this arena has been unloaded.
     * 
     * @note Useful for debugging, tells which level generation a pointer
     * was allocated during.
     */
    LAMANCHA_INLINE u32 levelIndex() const noexcept { return m_levelIndex; }

    /**
     * @brief Percentage of the arena used [0.0, 1.0].
     *
     * @details
     * Log this at level load completion to verify sizing. If it consistently
     * exceeds 80%, increase LEVEL_ARENA_SIZE. If below 20%, reduce it.
     *
     * @code
     * InfoLog("Level arena usage after load: %.1f%%", levelArena.usageRatio() * 100.0f);
     * @endcode
     */
    f32 usageRatio() const noexcept
    {
      usize cap = m_arena.capacity();
      if (cap == 0) { return 0.0f; }
      return static_cast<f32>(m_arena.used()) / static_cast<f32>(cap);
    }

  private:
    LevelArenaLM() noexcept;

    ArenaLM m_arena;
    u32 m_levelIndex = 0;   ///< incremented each unload(), for debug
  };
}