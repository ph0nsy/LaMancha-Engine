/**
 * @file arena.h
 * @brief Bump-pointer arena allocator for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * @details
 * An arena is a flat region of memory with a single bump pointer.
 * Allocation is O(1): align the pointer, advance it, return the old position.
 * There is no per-object free. The only release operation is reset(), which
 * moves the pointer back to the start, making the entire region available again.
 *
 * - Failure policy.
 * @c alloc() returns @c Optional<void*>::None() when the arena is full.
 * The typed helper @c alloc<T>() follows the same policy.
 * Callers that receive None must handle it: skip the operation or
 * @c LAMANCHA_ASSERT if the allocation was for a critical resource.
 * @a ArenaLM itself never touches @a Telemetry; that responsibility belongs
 * to the subsystem using the arena.
 * 
 * - Thread ownership.
 * Each arena is owned by exactly one thread. Do not share arenas
 * across threads without external synchronization.
 *
 * - Memory ownership.
 * @a ArenaLM never owns its backing memory when created via @c fromExternal().
 * The caller (@a HeapAllocatorLM, or a parent arena) owns the block and is
 * responsible for its lifetime. @c ArenaLM::fromHeap() is the exception:
 * it calls malloc once and owns the result, freeing it on destruction.
 * Use @c fromHeap() only for testing or bootstrap code before @a HeapAllocatorLM exists.
 *
 * - Specializations:
 *   - @a PermanentArena (never reset, engine lifetime)
 *   - @a LevelArena (reset on level unload)
 *   - @a FrameArena (reset every frame, asserts on overflow, no Optional)
 *   - @a ScratchpadArena (sub-region of FrameArena, manually save/restore)
 */

#pragma once

#include "core/pch.h"

namespace LaMancha {
 
  /**
   * @brief Bump-pointer arena allocator.
   *
   * Do not construct directly use the factory functions:
   * - @c ArenaLM::fromExternal(ptr,size) which is owned by caller, no cleanup
   * - @c ArenaLM::fromHeap(size) which is owned by this arena, freed on ~ArenaLM
   */
  struct ArenaLM {
    /**
     * @brief Create an arena over an externally-owned memory block.
     *
     * @details
     * The arena does NOT own the memory. The caller must ensure the block
     * outlives the arena. No cleanup is performed on destruction.
     *
     * This is destined for production, @a HeapAllocatorLM will hand a slice here.
     *
     * @param _ptr Start of the memory block. Must be non-null.
     * @param _size Size in bytes. Must be > 0.
     */
    static ArenaLM fromExternal(void* _ptr, usize _size) noexcept;

    /**
     * @brief Create an arena that allocates its own backing block via malloc.
     *
     * @details
     * The arena owns the block and will free it in its destructor.
     * Use this for tests and bootstrap code before @a HeapAllocatorLM exists.
     * In production code, prefer @c fromExternal().
     *
     * @param _size Size in bytes to allocate.
     * @returns Empty (zero-capacity) arena if malloc fails on an arena with m_ownsMemory = true. 
     * Check @c capacity() > 0 after construction.
     */
    static ArenaLM fromHeap(usize _size) noexcept;

    /** @brief Frees backing memory if and only if this arena was created via fromHeap(). */
    ~ArenaLM() noexcept;

    // Movable (so factory functions can return by value) but not copyable (explicit ownership semantics).
    ArenaLM(const ArenaLM&) = delete;
    ArenaLM& operator=(const ArenaLM&) = delete;
    ArenaLM(ArenaLM&& _other) noexcept;
    ArenaLM& operator=(ArenaLM&& _other) noexcept;

    /**
     * @brief Allocate @c _size bytes aligned to @c _align : O(1).
     *
     * @param _size   Bytes to allocate.
     * @param _align  Required alignment in bytes. Must be a power of two.
     * @returns @c Optional<PoolAllocatorLM>::None() if there is insufficient space.
     * @note Alignment must be a power of two.
     */
    Optional<void*> alloc(usize _size, usize _align) noexcept;

    /**
     * @brief Typed allocation helper.
     *
     * @details
     * Allocates @c sizeof(T) bytes aligned to @c alignof(T).
     * Does NOT call @a T 's constructor, the memory is uninitialized.
     * Use placement new if construction is needed.
     *
     * @code
     *    auto result = arena.alloc<Transform>();
     *    if (result) {
     *      #pragma push_macro("new")
     *      #undef new // Protect against redefinition
     *      new (result.value()) Transform{};  // placement new
     *      #pragma pop_macro("new")
     *    }
     * @endcode
     */
    template <typename T>
    LAMANCHA_INLINE Optional<T*> alloc() noexcept
    {
      Optional<void*> raw = alloc(sizeof(T), alignof(T));
      if (!raw) { return Optional<T*>::None(); }
      return Optional<T*>::Some(static_cast<T*>(raw.value()));
    }

    /**
     * @brief Typed array allocation helper.
     *
     * Allocates @c sizeof(T) @c * @c _count bytes aligned to @c alignof(T).
     * Memory is uninitialized. Use placement placement new on each element if needed.
     */
    template <typename T>
    LAMANCHA_INLINE Optional<T*> allocArray(usize _count) noexcept
    {
      Optional<void*> raw = alloc(sizeof(T) * _count, alignof(T));
      if (!raw) { return Optional<T*>::None(); }
      return Optional<T*>::Some(static_cast<T*>(raw.value()));
    }

    /**
     * @brief Save the current bump pointer position.
     *
     * @details
     * @code
     *    usize mark = arena.save();
     *    // temporary allocations
     *    arena.restore(mark); // all allocations since save() are gone
     * @endcode
     *
     * @note 
     * Pair with @c restore() to implement a scratchpad. This is how @a ScratchpadArena
     * works (not as a separate type), just a save/restore pattern over a 
     * @a FrameArena sub-region.
     */
    LAMANCHA_INLINE usize save() const noexcept { return static_cast<usize>(m_current - m_base); }

    /**
     * @brief Restore the bump pointer to a previously saved position.
     * 
     * @details
     * All allocations made after the corresponding save() are invalidated.
     * The memory they occupied is immediately reusable.
     *
     * @param _mark Value returned by a prior save() call on this arena.
     */
    LAMANCHA_INLINE void restore(usize _mark) noexcept
    {
      LAMANCHA_ASSERT(_mark <= static_cast<usize>(m_end - m_base), "ArenaLM::restore > Bump pointer position out of bounds");
      m_current = m_base + _mark;
    }

    /**
     * @brief Reset the arena, making all memory available again. Moves the bump pointer back to the start: O(1).
     *
     * @details
     * Does not zero memory. 
     * Does not call any destructor.
     * All pointers previously returned by @c alloc() are invalidated.
     */
    LAMANCHA_INLINE void reset() noexcept { m_current = m_base; }

    /** @brief Bytes currently allocated. */
    LAMANCHA_INLINE usize used() const noexcept { return static_cast<usize>(m_current - m_base); }

    /** @brief Bytes still available. */
    LAMANCHA_INLINE usize remaining() const noexcept { return static_cast<usize>(m_end - m_current); }

    /** @brief Total capacity of the backing block. */
    LAMANCHA_INLINE usize capacity()  const noexcept { return static_cast<usize>(m_end - m_base); }

    /** @brief True if no bytes have been allocated since last reset(). */
    LAMANCHA_INLINE bool  isEmpty()   const noexcept { return m_current == m_base; }

    /** @brief True if the arena was created via fromHeap() and owns its block. */
    LAMANCHA_INLINE bool  ownsMemory() const noexcept { return m_ownsMemory; }

  private:
    ArenaLM() noexcept = default; // Use factory functions.

    u8* m_base = nullptr;         ///< Start of backing block
    u8* m_current = nullptr;      ///< Next free byte (bump pointer)
    u8* m_end = nullptr;          ///< One past last byte of backing block
    bool m_ownsMemory = false;    ///< True if destructor should free m_base
  };
}