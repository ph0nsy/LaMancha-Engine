/**
 * @file HeapAllocatorLM.h
 * @brief Per-thread OS memory region for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * HeapAllocatorLM is the ONLY place in the engine that calls the OS
 * for memory. Every other allocator (ArenaLM, PoolAllocatorLM, etc.)
 * receives a slice of a HeapAllocatorLM region via fromExternal().
 *
 * One instance per thread created at startup from the OS independently. 
 * No locking between threads, no shared state.
 *
 * Platform implementations:
 * Linux / R36S: @c mmap(MAP_ANONYMOUS|MAP_PRIVATE) + @c mlock (advisory)
 * Windows: @c VirtualAlloc(MEM_RESERVE|MEM_COMMIT) + @c VirtualLock (advisory)
 * 
 * Attempt to pins pages in physical RAM, preventing OS paging.
 * Failure is logged and the engine continues.
 * On Linux, mlock requires CAP_IPC_LOCK or sufficient RLIMIT_MEMLOCK.
 *
 * Thread safety: One instance per thread, never shared.
 *
 * @code
 *   // Thread startup
 *   Optional<HeapAllocatorLM> heap = HeapAllocatorLM::create(32_MB);
 *   LAMANCHA_ASSERT(heap.hasValue, "Heap allocation was unsuccessfull.");
 *
 *   // Carve sub-regions for each arena this thread owns
 *   Optional<void*> levelBlock = heap.value.alloc(16_MB, 64);
 *   Optional<void*> frameBlock = heap.value.alloc(4_MB, 64);
 *   Optional<void*> scriptBlock = heap.value.alloc(4_MB, 64);
 *
 *   LevelArena levelArena = LevelArena::fromExternal(levelBlock.value, ...);
 *   FrameArena frameArena = FrameArena::fromExternal(frameBlock.value, ...);
 *   ArenaLM scriptArena = ArenaLM::fromExternal(scriptBlock.value, ...);
 *
 *   // Thread shutdown
 *   heap.value.destroy();   // or let destructor handle it
 * @endcode
 */

#pragma once

#include "core/pch.h"

namespace LaMancha {

  struct HeapAllocatorLM {

    /**
     * @brief Allocate a memory region from the OS and optionally pin it.
     *
     * Reserves and commits the full region immediately. All pages are
     * resident after this call (or after first touch on systems
     * that defer commit despite MAP_POPULATE).
     *
     * @param size  Total size in bytes. Rounded up to page boundary internally.
     * Typical values per thread:
     *  Main Thread: 48 MB (render arena + frame arena + permanent)
     *  Logic Thread: 64 MB (level arena + script arena + ECS pools)
     *  Asset Thread: 32 MB (IO arena + load scratch)
     *  Utility Thread: 8 MB (log buffers + GC scratch)
     *
     * @returns None if the OS cannot satisfy the allocation.
     */
    static Optional<HeapAllocatorLM> create(usize _size) noexcept;

    /**
     * @brief Release the OS memory region.
     *
     * Safe to call multiple times (second call is a no-op).
     * Also called by the destructor, so explicit destroy() is optional.
     */
    void destroy() noexcept;

    ~HeapAllocatorLM() noexcept { destroy(); }

    // Non-copyable. Moving transfers ownership.
    HeapAllocatorLM(const HeapAllocatorLM&) = delete;
    HeapAllocatorLM& operator=(const HeapAllocatorLM&) = delete;
    HeapAllocatorLM(HeapAllocatorLM&&) noexcept;
    HeapAllocatorLM& operator=(HeapAllocatorLM&&) noexcept;

    // ─── Sub-region allocation ───────────────────────────────────────────────

    /**
     * @brief Carve an aligned sub-region from this allocator's block.
     *
     * Bump-pointer allocation into the OS region. This is only called at
     * thread startup to divide the region between arenas.
     *
     * @param size Bytes to allocate.
     * @param align Alignment in bytes. Must be a power of two.
     *
     * @returns Pointer to the sub-region, or None if insufficient space.
     */
    Optional<void*> alloc(usize _size, usize _align) noexcept;

    LAMANCHA_INLINE usize totalSize() const noexcept { return m_size; }
    LAMANCHA_INLINE usize usedSize() const noexcept { return static_cast<usize>(m_current - m_base); }
    LAMANCHA_INLINE usize remaining() const noexcept { return m_size - usedSize(); }
    LAMANCHA_INLINE bool isLocked() const noexcept { return m_locked; }

  private:
    HeapAllocatorLM() noexcept = default;

    u8* m_base = nullptr;
    u8* m_current = nullptr;
    usize m_size = 0;
    bool m_locked = false;   ///< true if mlock/VirtualLock succeeded
  };

} // namespace LaMancha