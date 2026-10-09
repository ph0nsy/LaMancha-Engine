/**
 * @file ScratchpadArena.h
 * @brief RAII temporary memory scope for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * A ScratchpadArena borrows a region from a parent arena for the duration
 * of a C++ scope. When it is destroyed, the borrowed region is returned to
 * the parent automatically, it is impossible to forget the restore.
 *
 * This is a RAII wrapper over the save()/restore() pattern that already
 * exists on FrameArena and LevelArena. It compiles to identical machine
 * code but eliminates the class of bug where restore() is forgotten,
 * skipped by an early return, or skipped by an exception path.
 *
 * Construction:
 *   Do not construct directly. Use the scratch() factory on the parent:
 *
 *   @code
 *   // From FrameArena, asserts on overflow
 *   {
 *       ScratchpadArena scratch = frameArena.scratch();
 *       f32* sines = scratch.allocArray<f32>(512);
 *       // ... compute ...
 *   }   // frameArena restored here, no manual restore needed
 *
 *   // From LevelArena, Optional on overflow
 *   {
 *       ScratchpadArena scratch = levelArena.scratch();
 *       auto result = scratch.alloc<NavNode>();
 *       if (!result) { return; }   // overflow handled here
 *       // ...
 *   }   // levelArena restored here
 *   @endcode
 *
 * Nesting:
 *   ScratchpadArenas can be nested. Inner scopes borrow from outer scopes.
 *   C++ guarantees inner variables are destroyed before outer ones, so the
 *   restore order is always correct.
 *
 *   @code
 *   {
 *       ScratchpadArena outer = frameArena.scratch();
 *       f32* workspace = outer.allocArray<f32>(1024);
 *       {
 *           ScratchpadArena inner = outer.scratch();
 *           u32* indices = inner.allocArray<u32>(256);
 *           // ... use indices to build workspace ...
 *       }   // inner restored: indices gone, workspace still valid
 *       // ... use workspace ...
 *   }   // outer restored: workspace gone
 *   @endcode
 *
 * Failure policy:
 *   alloc() returns Optional<void*> (works for both FrameArena and
 *   LevelArena parents).
 *
 *   allocAssert() asserts on overflow, use when you know the parent is
 *   a FrameArena and the allocation must succeed.
 *
 *   The scratchpad does not change the parent's failure contract. It only
 *   changes when the restore happens (automatically vs manually).
 *
 * Thread ownership:
 *   Inherited from the parent arena. A ScratchpadArena over a FrameArena
 *   must only be used on the thread that owns that FrameArena. The parent's
 *   debug ownership checks fire if this is violated.
 *
 * Lifetime rule:
 *   A ScratchpadArena MUST NOT outlive its parent arena. This is a
 *   use-after-free bug. In debug builds the parent pointer is checked on
 *   destruction to catch obvious violations (parent reset between scratch
 *   construction and destruction). In practice, keeping scratch variables
 *   stack-local and short-lived prevents this entirely.
 *
 * Non-copyable, non-movable:
 *   Copying a ScratchpadArena would create two objects that both try to
 *   restore the same mark, double-resetting the parent. Moving has the
 *   same problem unless carefully handled. Both are deleted.
 *   If you need to pass a scratch region to a helper function, pass a
 *   reference: void helper(ScratchpadArena& scratch).
 */

#pragma once

#include "core/memory/sync.h"
#include "core/memory/arena.h"

namespace LaMancha {
  // Forward declarations. ScratchpadArena is a friend of these so it can
  // access their internal ArenaLM through the scratch() factory.
  struct FrameArenaLM;
  struct LevelArenaLM;

  struct ScratchpadArenaLM
  {
    // Use FrameArena::scratch() or LevelArena::scratch().
    explicit ScratchpadArenaLM(ArenaLM& _arena) noexcept
      : m_arena(_arena), m_mark(_arena.save()) {}

    /**
     * @brief Restore the parent arena to the state it was in before this
     * scratchpad was constructed. All memory allocated through this
     * scratchpad is immediately available to the parent again.
     */
    ~ScratchpadArenaLM() noexcept
    {
#if LAMANCHA_DEBUG
      // The parent's current position must be >= our saved mark.
      // If it is less, the parent was reset (for example, beginFrame()) while this
      // scratchpad was alive (lifetime violation).
      LAMANCHA_ASSERT(m_arena.used() >= m_mark, "ScratchpadArenaLM outlived a parent arena reset");
#endif
      m_arena.restore(m_mark);
    }

    // Non-copyable, non-movable.
    ScratchpadArenaLM(const ScratchpadArenaLM&) = delete;
    ScratchpadArenaLM& operator=(const ScratchpadArenaLM&) = delete;
    ScratchpadArenaLM(ScratchpadArenaLM&&) = delete;
    ScratchpadArenaLM& operator=(ScratchpadArenaLM&&) = delete;

    /**
     * @brief Allocate 'size' bytes aligned to 'align'.
     *
     * Returns None on overflow. Suitable for both FrameArena and LevelArena
     * parents.
     *
     * @param size Bytes to allocate. Must be > 0.
     * @param align Alignment in bytes. Must be a power of two.
     */
    LAMANCHA_INLINE Optional<void*> alloc(usize size, usize align) noexcept
    {
      return m_arena.alloc(size, align);
    }

    /**
     * @brief Typed allocation. Returns None on overflow.
     * Memory is uninitialised. Use placement new if construction is needed.
     */
    template <typename T>
    LAMANCHA_INLINE Optional<T*> alloc() noexcept
    {
      Optional<void*> raw = m_arena.alloc(sizeof(T), alignof(T));
      if (!raw) { return Optional<T*>::None(); }
      return Optional<T*>::Some(static_cast<T*>(raw.value()));
    }

    /**
     * @brief Typed array allocation. Returns None on overflow.
     * Memory is uninitialised.
     */
    template <typename T>
    LAMANCHA_INLINE Optional<T*> allocArray(usize count) noexcept
    {
      Optional<void*> raw = m_arena.alloc(sizeof(T) * count, alignof(T));
      if (!raw) { return Optional<T*>::None(); }
      return Optional<T*>::Some(static_cast<T*>(raw.value()));
    }

    /**
     * @brief Typed allocation that asserts on overflow.
     *
     * Use when the parent is a FrameArena and the allocation must succeed.
     * Increments Sync::gTelemetry.oom_frame_alloc before asserting so the
     * overflow is visible in telemetry even in release builds.
     *
     * @code
     * // Caller knows this is a frame scratch, overflow is a sizing bug
     * RenderCmd* cmd = scratch.allocAssert<RenderCmd>();
     * @endcode
     */
    template <typename T>
    LAMANCHA_INLINE T* allocAssert() noexcept
    {
      Optional<void*> raw = m_arena.alloc(sizeof(T), alignof(T));
      if (!raw)
      {
        Sync::gTelemetry.oom_frame_alloc.fetchAddRelaxed(1u);
        LAMANCHA_ASSERT(false, "ScratchpadArena overflow, increase parent arena size");
        return nullptr;
      }
      return static_cast<T*>(raw.value());
    }

    /**
     * @brief Typed array allocation that asserts on overflow.
     */
    template <typename T>
    LAMANCHA_INLINE T* allocArrayAssert(usize count) noexcept
    {
      Optional<void*> raw = m_arena.alloc(sizeof(T) * count, alignof(T));
      if (!raw)
      {
        Sync::gTelemetry.oom_frame_alloc.fetchAddRelaxed(1u);
        LAMANCHA_ASSERT(false, "ScratchpadArena overflow, increase parent arena size");
        return nullptr;
      }
      return static_cast<T*>(raw.value());
    }

    /**
     * @brief Create a nested ScratchpadArena borrowing from this one.
     *
     * The nested scratch is destroyed first (C++ stack order), so the
     * restore chain is always innermost-first. The nested scratch's
     * allocations are returned to this scratch (not directly to the
     * original parent) when it is destroyed.
     *
     * @code
     * ScratchpadArena outer = frameArena.scratch();
     * {
     *     ScratchpadArena inner = outer.scratch();
     *     // inner allocations don't outlive this block
     * }
     * // outer allocations still valid here
     * @endcode
     */
    LAMANCHA_INLINE ScratchpadArenaLM scratch() noexcept { return ScratchpadArenaLM(m_arena); }

    /** @brief Bytes allocated through this scratchpad (since construction). */
    LAMANCHA_INLINE usize used() const noexcept
    {
      usize current = m_arena.used();
      return current > m_mark ? current - m_mark : 0;
    }

    /** @brief Bytes still available in the parent arena. */
    LAMANCHA_INLINE usize remaining() const noexcept { return m_arena.remaining(); }

  private:
    ArenaLM& m_arena;   ///< reference to parent's backing ArenaLM
    usize m_mark;       ///< bump pointer position at construction time
    
    // FrameArena and LevelArena call the private constructor via scratch().
    friend struct FrameArenaLM;
    friend struct LevelArenaLM;
  };
}