/**
 * @file pool.h
 * @brief Fixed-size slot pool allocator for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * @details
 * A pool allocator manages a fixed number of same-sized slots. 
 * Allocation is O(1): pop the free list head. 
 * Deallocation is O(1): push back onto the free list. 
 * No fragmentation is possible because every slot is the same size.
 *
 * - Two types are provided:
 *   1. @a PoolAllocatorLM: untyped base. Slot size and alignment are runtime
 *   values. Returns @c void*. Used by @a IOArena for asset pools where the type is 
 *   not known at compile time.
 *   2. @a TypedPool<T>: typed wrapper over @a PoolAllocatorLM. Slot size and
 *   alignment are @c sizeof(T)/alignof(T). Returns @c T*. Used by the ECS for 
 *   per-component-type pools.
 *
 * - Free list: Intrusive.
 * Freed slots store a pointer to the next free slot in their own memory.
 * Zero overhead when slots are occupied. Requires each slot to be at least 
 * sizeof(void*) bytes. The effective slot size formula handles this automatically:
 * @code
 *     effective = max(slotSize, sizeof(void*))
 *     effective = roundUpToAlign(effective, slotAlign)
 * @endcode
 * For types smaller than sizeof(void*), the slot is silently padded.
 * The padding bytes are never visible to the caller.
 *
 * - Failure policy.
 * @c alloc() returns @c Optional<void*>::None() when the pool is exhausted. The 
 * caller handles None (typically by incrementing @c Sync::gTelemetry.oom_ecs_pool 
 * and skipping or deferring the operation. @a PoolAllocatorLM never touches 
 * @a Telemetry directly.
 *
 * - Thread ownership.
 * Single-owner. No synchronization. Each pool is owned by exactly one thread.
 * Cross-thread operations (Asset Thread allocs, Utility Thread frees) are 
 * coordinated one level up by @a IOArena via @a GCJobQueue. See @c ioArena.h 
 * for the coordination pattern.
 *
 * - Memory ownership:
 *   - @c fromArena(): Pool allocates its backing block from an ArenaLM at
 *   construction. The arena owns the memory. The pool holds a pointer into it. 
 *   Pool destruction does not free memory, the arena reset handles that.
 *   - @c fromExternal(): Pool is given an external pointer and size. Caller
 *   owns and manages the memory lifetime.
 *
 * - Construction:
 * @code
 *   // ECS pool backed by LevelArena
 *   Optional<PoolAllocatorLM> pool = PoolAllocatorLM::fromArena(levelArena.m_arena,
 *                                    sizeof(TransformComponent),
 *                                    alignof(TransformComponent),
 *                                    4096);
 *   LAMANCHA_ASSERT(pool.hasValue, "");
 *
 *   // Typed convenience wrapper
 *   Optional<TypedPool<TransformComponent>> tpool = TypedPool<TransformComponent>::fromArena(levelArena.m_arena, 4096);
 * @endcode
 */

#pragma once

#include "arena.h"
#include "construct.h"

namespace LaMancha {
  struct PoolAllocatorLM {
    /**
     * @brief Create a pool by allocating its backing block from an ArenaLM.
     *
     * @details
     * Computes the effective slot size (accounts for minimum intrusive pointer size and 
     * alignment padding), then allocates capacity * effectiveSlotSize bytes from the arena. 
     * The pool does not own the memory, the arena does.
     *
     * @param _arena Arena to allocate the backing block from.
     * @param _slotSize Size of one slot in bytes. Will be padded to at least sizeof(void*) and rounded to slotAlign.
     * @param _slotAlign Required alignment per slot. Must be a power of two.
     * @param _capacity Maximum number of slots.
     * 
     * @returns @c Optional<PoolAllocatorLM>::None() if the arena cannot satisfy the allocation.
     */
    static Optional<PoolAllocatorLM> fromArena(ArenaLM& _arena, usize _slotSize, usize _slotAlign, usize _capacity) noexcept;

    /**
     * @brief Create a pool over an externally-owned memory block.
     *
     * @details
     * The caller owns and manages the memory. Pool destruction does nothing. @c _blockSize
     * must be >= capacity * effectiveSlotSize(slotSize, slotAlign).
     *
     * @param _ptr Start of the backing block. Must be non-null.
     * @param _blockSize Size of the backing block in bytes.
     * @param _slotSize Size of one slot in bytes.
     * @param _slotAlign Required alignment per slot. Must be a power of two.
     * @param _capacity Maximum number of slots.
     * 
     * @returns @c Optional<PoolAllocatorLM>::None() if blockSize is insufficient for the requested capacity.
     * 
     * @note 
     * Use @c computeBlockSize() to calculate the required size before calling.
     */
    static Optional<PoolAllocatorLM> fromExternal(void* _ptr, usize _blockSize, usize _slotSize, usize _slotAlign, usize _capacity) noexcept;

    /**
     * @brief Compute the backing block size needed for @c fromExternal().
     * 
     * @details
     * @code
     *   usize needed = PoolAllocatorLM::computeBlockSize(sizeof(TextureAsset), alignof(TextureAsset), 512);
     *   void* block  = myBuffer;   // must be >= needed bytes
     *   Optional<PoolAllocatorLM> pool = PoolAllocatorLM::fromExternal(block, needed, sizeof(TextureAsset),alignof(TextureAsset), 512);
     * @endcode
     * 
     * @param _slotSize Size of one slot in bytes.
     * @param _slotAlign Required alignment per slot. Must be a power of two.
     * @param _capacity Maximum number of slots.
     */
    static usize computeBlockSize(usize _slotSize, usize _slotAlign, usize _capacity) noexcept;
    
    PoolAllocatorLM() noexcept = default;

    // Non-copyable. Moving transfers ownership of the free list state.
    PoolAllocatorLM(const PoolAllocatorLM&) = delete;
    PoolAllocatorLM& operator=(const PoolAllocatorLM&) = delete;
    PoolAllocatorLM(PoolAllocatorLM&&) noexcept;
    PoolAllocatorLM& operator=(PoolAllocatorLM&&) noexcept;

    ~PoolAllocatorLM() noexcept = default;   // does not free (arena owns memory)

    /**
     * @brief Allocate one slot. Free list pop: O(1).
     *
     * @details
     * Use placement new for construction if the type requires it. The caller is 
     * responsible for handling @c None():
     * 
     * @code
     *   Optional<void*> slot = pool.alloc();
     *   if (!slot) 
     *   {
     *       Sync::gTelemetry.oom_ecs_pool.fetchAddRelaxed(1u);
     *       return;   // skip this entity this frame
     *   }
     *   // use slot.value()
     * @endcode
     * 
     * @returns @c Optional<void*>::None() when the pool is exhausted. Memory is uninitialized.
     */
    Optional<void*> alloc() noexcept;

    /**
     * @brief Return a slot to the pool. Free list pop: O(1).
     *
     * @details
     * @a ptr must have been returned by alloc() on this pool instance. Passing a pointer 
     * from a different pool, a dangling pointer, or a pointer into the middle of a slot is
     * undefined behavior. In debug builds, ptr is checked to lie within the pool's backing block.
     *
     * Does NOT call any destructor. Call the destructor explicitly before @c free() if the slot 
     * holds a type that requires it:
     * @code
     *   myObj->~MyType();
     *   pool.free(myObj);
     * @endcode
     *
     * @param _ptr Pointer previously returned by alloc().
     */
    void free(void* _ptr) noexcept;

    /** @brief Number of currently allocated (live) slots. */
    LAMANCHA_INLINE usize usedSlots() const noexcept { return m_usedSlots; }

    /** @brief Number of available (free) slots. */
    LAMANCHA_INLINE usize freeSlots() const noexcept { return m_capacity - m_usedSlots; }

    /** @brief Total slot capacity. */
    LAMANCHA_INLINE usize capacity() const noexcept { return m_capacity; }

    /** @brief True if no slots are available. */
    LAMANCHA_INLINE bool isFull() const noexcept { return m_usedSlots == m_capacity; }

    /** @brief True if no slots are in use. */
    LAMANCHA_INLINE bool isEmpty() const noexcept { return m_usedSlots == 0; }

    /** @brief Effective slot size after padding (for diagnostics). */
    LAMANCHA_INLINE usize effectiveSlotSize() const noexcept { return m_effectiveSlotSize; }

    /**
     * @brief Percentage of slots in use [0.0, 1.0].
     * Useful for tuning pool capacity at startup.
     */
    f32 usageRatio() const noexcept
    {
      if (m_capacity == 0) { return 0.0f; }
      return static_cast<f32>(m_usedSlots) / static_cast<f32>(m_capacity);
    }

    /**
     * @brief Get a slot pointer by dense index. O(1).
     *
     * @param _index  Slot index in range [0, capacity).
     * @return @c nullptr if index is out of range.
     *
     * @note
     * Used by IOArena to reach a slot directly from a handle's index field
     * without going through the hash map.
     */
    LAMANCHA_INLINE void* getByIndex(usize _index) noexcept
    {
      if (_index >= m_capacity) { return nullptr; }
      return static_cast<void*>(m_block + _index * m_effectiveSlotSize);
    }

    /**
     * @brief Get the dense index of a slot pointer. O(1).
     *
     * @param _ptr  Pointer previously returned by alloc() on this pool.
     *
     * @note @c _ptr must point to the exact start of a slot.
     * @note Used by IOArena after alloc() to compute the handle index.
     */
    LAMANCHA_INLINE u16 indexOfSlot(void* _ptr) noexcept
    {
      LAMANCHA_ASSERT(
        static_cast<u8*>(_ptr) >= m_block &&
        static_cast<u8*>(_ptr) < m_block + m_capacity * m_effectiveSlotSize,
        "PoolAllocatorLM::indexOfSlot > pointer is outside this pool's block");

      return static_cast<u16>((static_cast<u8*>(_ptr) - m_block) / m_effectiveSlotSize);
    }

  private:

    // Internal init shared by both factory functions. Called after the backing block has been obtained.
    void init(void* _block, usize _slotSize, usize _slotAlign, usize capacity) noexcept;

    // Compute effective slot size: max(slotSize, sizeof(void*)) rounded up to the next multiple of slotAlign.
    static usize computeEffectiveSlotSize(usize _slotSize, usize _slotAlign) noexcept;

    u8* m_block = nullptr;          ///< start of backing memory block
    void* m_freeHead = nullptr;     ///< head of intrusive free list
    usize m_effectiveSlotSize = 0;  ///< padded slot size (>= sizeof(void*))
    usize m_capacity = 0;           ///< total number of slots
    usize m_usedSlots = 0;          ///< currently allocated slots

#if LAMANCHA_DEBUG
    usize  m_blockSize = 0;        ///< total backing block size (for bounds check)
#endif
  };

  /**
   * @brief Type-safe pool allocator for a specific type @c T.
   *
   * @details
   * Thin wrapper over @a PoolAllocatorLM that:
   * - Fixes slot size to sizeof(T) and alignment to alignof(T)
   * - Returns T* instead of void*
   * - Takes T* in free() instead of void*
   *
   * All logic lives in @a PoolAllocatorLM. @a TypedPool adds zero runtime overhead.
   *
   * Usage:
   * @code
   *   Optional<TypedPool<TransformComponent>> result = TypedPool<TransformComponent>::fromArena(levelArena, 4096);
   *   LAMANCHA_ASSERT(result.hasValue, "");
   *   TypedPool<TransformComponent>& pool = result.value();
   *
   *   Optional<TransformComponent*> slot = pool.alloc();
   *   if (!slot)
   *   {
   *       Sync::gTelemetry.oom_ecs_pool.fetchAddRelaxed(1u);
   *       return;
   *   }
   *   TransformComponent* t = slot.value();
   *   t->position = Vec3{ 0.0f, 0.0f, 0.0f };   // direct assignment: no new
   *   t->rotation = Quat::identity();
   *
   *   // later, when the entity is destroyed
   *   // release any handles t holds (an example: GPU resource handles)
   *   pool.free(slot.value());
   * @endcode
   * 
   * @tparam T  The type this pool holds. Must be at least 1 byte.
   */
  template <typename T>
  struct TypedPool
  {
    /**
     * @brief Create a @a TypedPool by allocating from an @a ArenaLM.
     *
     * @param _arena Arena to allocate the backing block from.
     * @param _capacity Maximum number of T slots.
     */
    static Optional<TypedPool<T>> fromArena(ArenaLM& _arena, usize _capacity) noexcept
    {
      Optional<PoolAllocatorLM> base = PoolAllocatorLM::fromArena(_arena, sizeof(T), alignof(T), _capacity);
      if (!base) { return Optional<TypedPool<T>>::None(); }

      TypedPool<T> tp;
      tp.m_pool = static_cast<PoolAllocatorLM&&>(base.value());
      return Optional<TypedPool<T>>::Some(static_cast<TypedPool<T>&&>(tp));
    }

    /**
     * @brief Create a @a TypedPool over an externally-owned block.
     *
     * @param _ptr Backing block pointer. Must be non-null.
     * @param _blockSize Size of the backing block in bytes.
     * @param _capacity Maximum number of T slots.
     */
    static Optional<TypedPool<T>> fromExternal(void* _ptr, usize _blockSize, usize _capacity) noexcept
    {
      Optional<PoolAllocatorLM> base = PoolAllocatorLM::fromExternal(_ptr, _blockSize, sizeof(T), alignof(T), _capacity);
      if (!base) { return Optional<TypedPool<T>>::None(); }

      TypedPool<T> tp;
      tp.m_pool = static_cast<PoolAllocatorLM&&>(base.value());
      return Optional<TypedPool<T>>::Some(static_cast<TypedPool<T>&&>(tp));
    }

    /** @brief Compute the backing block size needed for @c fromExternal(). */
    static usize computeBlockSize(usize _capacity) noexcept { return PoolAllocatorLM::computeBlockSize(sizeof(T), alignof(T), _capacity); }

    TypedPool() noexcept = default;

    // Non-copyable, movable.
    TypedPool(const TypedPool&) = delete;
    TypedPool& operator=(const TypedPool&) = delete;
    TypedPool(TypedPool&&) = default;
    TypedPool& operator=(TypedPool&&) = default;

    /**
     * @brief Allocate one @a T slot: O(1).
     * @returns @c Optional<void*>::None() when exhausted. Memory is uninitialized.
     */
    Optional<T*> alloc() noexcept
    {
      Optional<void*> raw = m_pool.alloc();
      if (!raw) { return Optional<T*>::None(); }
      return Optional<T*>::Some(static_cast<T*>(raw.value()));
    }

    /**
     * @brief Return a @a T slot to the pool: O(1).
     * @note Does NOT call @a T 's destructor. Call it explicitly first if needed.
     */
    void free(T* _ptr) noexcept { m_pool.free(static_cast<void*>(_ptr)); }

    /**
     * @brief Allocate a slot and construct a T in it. O(1).
     *
     * @details
     * Combines @c alloc() + @c constructAt() into one call. Use for types that have a non-trivial 
     * constructor (C++ classes). For plain data structs, use @c alloc() + direct field assignment instead.
     *
     * @code
     *   Optional<Enemy*> e = enemyPool.construct(startPos, health);
     *   if (!e) { return; }
     *   // e.value() is fully constructed
     * @endcode
     * 
     * @param _args Arguments forwarded to T's constructor.
     * @return @c Optional<void*>::None() when the pool is exhausted. Construction is not attempted and no slot is consumed.
     */
    template <typename... Args>
    Optional<T*> construct(Args&&... _args) noexcept
    {
      Optional<void*> raw = m_pool.alloc();
      if (!raw) { return Optional<T*>::None(); }
      T* obj = constructAt<T>(raw.value(), static_cast<Args&&>(_args)...);
      return Optional<T*>::Some(obj);
    }

    /**
     * @brief Destroy a @a T and return its slot to the pool: O(1).
     *
     * Calls @c destroyAt(ptr) then @c free(ptr) in the correct order. Use for types constructed via @c construct(). 
     * For trivially destructible types, call @c free() directly; @c destroyAt<T>() will static_assert if misused.
     *
     * @code
     *   enemyPool.destroy(e);   // destructor called, slot returned
     *   e = nullptr;            // prevent accidental use after destroy
     * @endcode
     * 
     * @param _ptr Object previously returned by construct(). Must not be null.
     */
    void destroy(T* _ptr) noexcept
    {
      destroyAt<T>(_ptr);                    // destructor first
      m_pool.free(static_cast<void*>(_ptr)); // then return slot
    }

    /** @brief Number of currently allocated (live) slots. */
    LAMANCHA_INLINE usize usedSlots() const noexcept { return m_pool.usedSlots(); }

    /** @brief Number of available (free) slots. */
    LAMANCHA_INLINE usize freeSlots() const noexcept { return m_pool.freeSlots(); }

    /** @brief Total slot capacity. */
    LAMANCHA_INLINE usize capacity() const noexcept { return m_pool.capacity(); }

    /** @brief True if no slots are available. */
    LAMANCHA_INLINE bool isFull() const noexcept { return m_pool.isFull(); }

    /** @brief True if no slots are in use. */
    LAMANCHA_INLINE bool isEmpty() const noexcept { return m_pool.isEmpty(); }

    /** @brief Effective slot size after padding (for diagnostics). */
    LAMANCHA_INLINE f32 usageRatio() const noexcept { return m_pool.usageRatio(); }

    /** @brief Get a typed slot pointer by index. */
    LAMANCHA_INLINE T* getByIndex(usize _index) noexcept { return static_cast<T*>(m_pool.getByIndex(_index)); }

    /** @brief Get the dense index of a typed slot pointer. */
    LAMANCHA_INLINE u16 indexOfSlot(T* _ptr) noexcept { return m_pool.indexOfSlot(static_cast<void*>(_ptr)); }


  private:
    PoolAllocatorLM m_pool;
  };
}