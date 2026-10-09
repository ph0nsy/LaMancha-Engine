/**
 * @file PermanentArena.h
 * @brief Engine-lifetime bump-pointer allocator for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * @details
 * PermanentArena is ArenaLM with two constraints enforced:
 * - @c alloc() ASSERTS on overflow, same policy as @c FrameArena. Permanent
 * overflow is a startup sizing bug, not a runtime condition. There is
 * no graceful recovery path.
 * - @c reset() does not exist on this type. Calling @c reset() on permanent
 * storage is a design error. The interface simply does not expose it.
 * In debug, attempting to reach the underlying @c ArenaLM::reset() is
 * blocked by composition (m_arena is private).
 *
 * On overflow: assert unconditionally (debug and release).
 * No telemetry increment, permanent overflow happens at startup before
 * the engine is in a running state. The assert is the diagnostic.
 *
 * One PermanentArena per thread that needs engine-lifetime storage.
 * - @a MainThread: engine subsystem state, GL objects.
 * - @a LogicThread: ECS type registry, script VM state.
 * Constructed first, destroyed last (outlives every other allocator).
 *
 * @note Use for:
 * - Engine subsystem singletons (PhysicsSystem, AudioSystem, RenderSystem)
 * - Type registries (ECS component type table)
 * - Compiled shader programs (after GL compilation)
 * - String intern table
 * - Crash dump ring buffer (@c LogCrashRing )
 * - Asset hash map slot array
 * - Any data that must survive the entire session
 *
 * @note Do not use for:
 * - Level data (use @c LevelArena )
 * - Per-frame transients (use @c FrameArena )
 * - Assets with individual lifetimes (use @c IOArena )
 *
 * @note @c fromExternal() in production (HeapAllocatorLM hands a slice).
 * @note @c fromHeap() for tests and bootstrap. Check capacity() > 0 after fromHeap(),
 * malloc failure is handled by the caller at construction time, not inside the arena.
 */

#pragma once

#include "arena.h"

namespace LaMancha {
  struct PermanentArenaLM {
    /**
     * @brief Create a @c PermanentArena over an externally-owned memory block.
     *
     * The arena does NOT own the memory. The backing block must outlive
     * this arena (which, for permanent storage, means it outlives everything).
     *
     * @param ptr Start of the block. Must be non-null.
     * @param size Capacity in bytes. Must be > 0.
     */
    static PermanentArenaLM fromExternal(void* ptr, usize size) noexcept
    {
      PermanentArenaLM pa;
      pa.m_arena = ArenaLM::fromExternal(ptr, size);
      return pa;
    }

    /**
     * @brief Create a PermanentArena that owns its backing block (malloc).
     *
     * Use for tests and bootstrap before HeapAllocatorLM exists.
     * In production, prefer fromExternal().
     * Check capacity() > 0 after construction, malloc failure is not asserted
     * here because the logger may not yet be initialized at this call site.
     *
     * @param size Capacity in bytes.
     */
    static PermanentArenaLM fromHeap(usize size) noexcept
    {
      PermanentArenaLM pa;
      pa.m_arena = ArenaLM::fromHeap(size);
      return pa;
    }

    // Non-copyable, movable.
    PermanentArenaLM(const PermanentArenaLM&) = delete;
    PermanentArenaLM& operator=(const PermanentArenaLM&) = delete;
    PermanentArenaLM(PermanentArenaLM&&) = default;
    PermanentArenaLM& operator=(PermanentArenaLM&&) = default;

    /**
     * @brief Allocate `size` bytes aligned to `align`.
     *
     * Asserts unconditionally on overflow. Never returns null.
     * Permanent overflow is a startup sizing bug with no recovery path.
     *
     * @param size Bytes to allocate. Must be > 0.
     * @param align Alignment in bytes. Must be a power of two.
     */
    void* alloc(usize size, usize align) noexcept
    {
      Optional<void*> result = m_arena.alloc(size, align);
      if (!result)
      {
        // No telemetry increment, this fires at startup before the engine
        // is in a running state. The assert is the only diagnostic needed.
        LAMANCHA_ASSERT(false, "PermanentArena overflow: increase PERMANENT_ARENA_SIZE at startup");
        return nullptr;
      }
      return result.value();
    }

    /**
     * @brief Typed allocation. Returns a valid T* or asserts.
     * Memory is uninitialized. Use placement new if construction is needed.
     *
     * @code
     * PhysicsSystem* phys = permanentArena.alloc<PhysicsSystem>();
     * #pragma push_macro("new")
     * #undef new // Protect against redefinition
     * new (phys) PhysicsSystem{};
     * #pragma pop_macro("new")
     * @endcode
     */
    template <typename T>
    LAMANCHA_INLINE T* alloc() noexcept { return static_cast<T*>(alloc(sizeof(T), alignof(T))); }

    /**
     * @brief Typed array allocation. Returns a valid T* or asserts.
     * Memory is uninitialized.
     *
     * @param count Number of elements.
     */
    template <typename T>
    LAMANCHA_INLINE T* allocArray(usize count) noexcept { return static_cast<T*>(alloc(sizeof(T) * count, alignof(T))); }

    LAMANCHA_INLINE usize used() const noexcept { return m_arena.used(); }
    LAMANCHA_INLINE usize remaining() const noexcept { return m_arena.remaining(); }
    LAMANCHA_INLINE usize capacity() const noexcept { return m_arena.capacity(); }
    LAMANCHA_INLINE bool  isEmpty() const noexcept { return m_arena.isEmpty(); }

    /**
     * @brief Percentage of the arena used [0.0, 1.0].
     *
     * Useful at startup to verify that your PERMANENT_ARENA_SIZE constant
     * leaves a reasonable safety margin. Log this value during development.
     *
     * @code
     * InfoLog("Permanent arena usage: %.1f%%", permanentArena.usageRatio() * 100.0f);
     * @endcode
     */
    f32 usageRatio() const noexcept
    {
      usize cap = m_arena.capacity();
      if (cap == 0) { return 0.0f; }
      return static_cast<f32>(m_arena.used()) / static_cast<f32>(cap);
    }

    // There is no reset() on PermanentArena. If you find yourself wanting
    // to reset permanent storage, the data in question is not permanent,
    // use LevelArena or FrameArena instead.
    //
    // In debug builds, any attempt to call the underlying ArenaLM::reset()
    // is blocked by the private m_arena member. The compiler will tell you
    // at the call site that no such method exists on PermanentArena.

  private:
    PermanentArenaLM() noexcept;

    ArenaLM m_arena;
  };
}