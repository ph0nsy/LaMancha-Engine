/**
 * @file FrameArena.h
 * @brief Per-frame bump-pointer allocator for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * @details
 * FrameArena is ArenaLM with two constraints enforced:
 *
 * 1. @c alloc() ASSERTS on overflow, it never returns null.
 * Frame overflow is always a sizing bug. The fix is a larger arena,
 * not an error path at the call site.
 *
 * 2. @c reset() is called exactly once per frame, at the frame boundary
 * chosen at startup (either start-of-frame or end-of-frame, TODO: picking one
 * and never change it). @c FrameArena::beginFrame() is the named reset.
 *
 * - Failure policy.
 * Because @c alloc() never fails, callers receive a raw pointer and use it
 * directly. No Optional, no result check. This is intentional, it keeps
 * hot-path code (render command list building, ECS query result accumulation)
 * clean and branch-free.
 *
 * Before asserting, FrameArena increments @c Sync::gTelemetry.oom_frame_alloc .
 * In release builds where LAMANCHA_ASSERT is compiled out, this counter is
 * the only signal that an overflow occurred. The @a UtilityThread reads it
 * each frame; a non-zero value means the arena needs to be sized larger.
 *
 *
 * @code
 * void buildPathfindingGraph(FrameArena& arena)
 * {
 *   usize mark = arena.save();
 *   Node* nodes = arena.alloc<Node>(MAX_NODES);
 *   // ... compute ...
 *   arena.restore(mark);   // memory immediately reusable
 * }
 * @endcode
 *
 * @note 
 * Scratchpad pattern, functions that need temporary memory within a frame
 * use @c save() / @c restore() on the FrameArena directly. No separate type needed.
 * 
 * @note 
 * Main Thread owns one FrameArena (render commands, per-frame transients).
 * Logic Thread owns one FrameArena (ECS query scratch, Lua call buffers). 
 * Each is private to its thread.
 *
 */

#pragma once

#include "scratchpad.h"
#include "sync.h"   ///< Sync::gTelemetry

namespace LaMancha {
  struct FrameArenaLM {
    /**
     * @brief Create a FrameArena over an externally-owned memory block.
     * The arena does NOT own the memory. No cleanup on destruction.
     *
     * @param ptr   Start of the block. Must be non-null.
     * @param size  Capacity in bytes. Must be > 0.
     * @note Use in production (HeapAllocatorLM hands a slice).
     */
    static FrameArenaLM fromExternal(void* ptr, usize size) noexcept
    {
      FrameArenaLM fa;
      fa.m_arena = ArenaLM::fromExternal(ptr, size);
      return fa;
    }

    /**
     * @brief Create a FrameArena that owns its backing block (malloc).
     * Use for tests and bootstrap only. Check capacity() > 0 after construction.
     *
     * @param size  Capacity in bytes.
     * @note Use for tests.
     */
    static FrameArenaLM fromHeap(usize size) noexcept
    {
      FrameArenaLM fa;
      fa.m_arena = ArenaLM::fromHeap(size);
      return fa;
    }

    // Non-copyable, movable (same reasoning as ArenaLM).
    FrameArenaLM(const FrameArenaLM&) = delete;
    FrameArenaLM& operator=(const FrameArenaLM&) = delete;
    FrameArenaLM(FrameArenaLM&&) = default;
    FrameArenaLM& operator=(FrameArenaLM&&) = default;

    /**
     * @brief Reset the arena at the start of a new frame.
     *
     * Call exactly once per frame, at your chosen frame boundary.
     * All pointers previously returned by alloc() are invalidated.
     *
     * Does NOT zero memory. Does NOT call destructors.
     * All types stored here must be trivially destructible (no heap objects
     * in frame data).
     *
     * In debug builds, writes a canary pattern (an overwrite of the reset region
     * with recognizable garbage) over the region in debug so use-after-reset bugs
     * produce recognisable garbage rather than stale data.
     */
    void beginFrame() noexcept
    {
#if LAMANCHA_DEBUG
      // 0xCD is the MSVC debug heap fill pattern, widely recognised as
      // "this memory was freed". Any read of 0xCDCDCDCD after a beginFrame()
      // is a use-after-reset bug.
      if (m_arena.capacity() > 0)
      {
        // We write directly to m_arena's backing memory via the save/restore
        // mechanism: save position 0 from the base.
        // memset equivalent without including <string.h>:
        u8* base = static_cast<u8*>(m_arena.alloc(0, 1).valueOr(nullptr));
        // The arena is empty at this point (we reset below), so base == m_base.
        // We use the raw pointer only for the fill, not as an allocation.
        if (base)
        {
          usize cap = m_arena.capacity();
          for (usize i = 0; i < cap; ++i) { base[i] = 0xCD; }
        }
      }
#endif
      m_arena.reset();
      m_frameIndex++;
    }

    /**
     * @brief Allocate 'size' bytes aligned to 'align'.
     *
     * Asserts and increments oom_frame_alloc telemetry on overflow.
     * In builds where the assert is compiled out, the return value on
     * overflow is whatever Optional<void*> returns.
     *
     * @param size Bytes to allocate. Must be > 0.
     * @param align Alignment in bytes. Must be a power of two.
     */
    void* alloc(usize size, usize align) noexcept
    {
      Optional<void*> result = m_arena.alloc(size, align);
      if (!result)
      {
        Sync::gTelemetry.oom_frame_alloc.fetchAddRelaxed(1u);
        LAMANCHA_ASSERT(false, "FrameArenaLM::Alloc > FrameArena overflow, increase arena size at startup");
      }
      return result.value();
    }

    /**
     * @brief Typed allocation. Returns a valid T* or asserts.
     * Memory is uninitialized. Use placement new if construction is needed.
     *
     * @code
     * RenderCmd* cmd = frameArena.alloc<RenderCmd>();
     * #pragma push_macro("new")
     * #undef new // Protect against redefinition
     * new (cmd) RenderCmd{ ... };   // placement new
     * #pragma pop_macro("new")
     * @endcode
     */
    template <typename T>
    LAMANCHA_INLINE T* alloc() noexcept { return static_cast<T*>(alloc(sizeof(T), alignof(T))); }

    /**
     * @brief Typed array allocation. Returns a valid T* or asserts.
     * Memory is uninitialised.
     *
     * @param count  Number of elements.
     */
    template <typename T>
    LAMANCHA_INLINE T* allocArray(usize count) noexcept
    {
      return static_cast<T*>(alloc(sizeof(T) * count, alignof(T)));
    }

    /**
     * @brief Create a RAII ScratchpadArena borrowing from this FrameArena.
     *
     * The scratchpad saves the current bump pointer on construction and
     * restores it on destruction, temporary allocations are returned
     * automatically when the scratchpad goes out of scope.
     *
     * @code
     * {
     *     ScratchpadArena scratch = frameArena.scratch();
     *     f32* sines = scratch.allocArrayAssert<f32>(512);
     *     // compute using sines
     * }   // sines memory returned to frameArena here
     * @endcode
     */
    LAMANCHA_INLINE ScratchpadArenaLM scratch() noexcept { return ScratchpadArenaLM(m_arena); }

    /**
     * @brief Save the current bump pointer for later restore.
     *
     * Use to implement temporary within-frame allocations that should not
     * persist to the end of the frame. The caller saves the mark, allocates,
     * uses the memory, then restores. The memory is immediately reusable.
     *
     * @code
     * usize mark = frameArena.save();
     * Transform* scratch = frameArena.allocArray<Transform>(count);
     * // ... use scratch ...
     * frameArena.restore(mark);
     * @endcode
     */
    LAMANCHA_INLINE usize save() const noexcept { return m_arena.save(); }
    LAMANCHA_INLINE void  restore(usize mark) noexcept { m_arena.restore(mark); }

    LAMANCHA_INLINE usize used() const noexcept { return m_arena.used(); }
    LAMANCHA_INLINE usize remaining() const noexcept { return m_arena.remaining(); }
    LAMANCHA_INLINE usize capacity() const noexcept { return m_arena.capacity(); }
    LAMANCHA_INLINE u32   frameIndex() const noexcept { return m_frameIndex; }

    /**
     * @brief Percentage of the arena used this frame [0.0, 1.0].
     * Useful for the profiler and for tuning arena size at startup.
     */
    f32 usageRatio() const noexcept
    {
      usize cap = m_arena.capacity();
      if (cap == 0) { return 0.0f; }
      return static_cast<f32>(m_arena.used()) / static_cast<f32>(cap);
    }

  private:
    FrameArenaLM() noexcept;

    ArenaLM m_arena;
    u32 m_frameIndex = 0;   ///< incremented each beginFrame(), useful for debugging
  };
