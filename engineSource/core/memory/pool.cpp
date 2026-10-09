#include "core/pch.h"
#include "pool.h"

using namespace LaMancha;

usize PoolAllocatorLM::computeEffectiveSlotSize(usize _slotSize, usize _slotAlign) noexcept
{
  LAMANCHA_ASSERT(_slotSize > 0, "PoolAllocatorLM::computeEffectiveSlotSize > Invalid size");
  LAMANCHA_ASSERT(_slotAlign > 0 && (_slotAlign & (_slotAlign - 1)) == 0, "PoolAllocatorLM::computeEffectiveSlotSize > Invalid alignment");

  // Each slot must be at least sizeof(void*) bytes so the
  // intrusive free list pointer fits inside a freed slot

  usize effective = _slotSize < sizeof(void*) ? sizeof(void*) : _slotSize;

  // Each slot must start at an address divisible by slotAlign so that the type stored
  // in the slot meets its alignment requirement. Since slots are laid out contiguously, 
  // the slot size itself must be a multiple of slotAlign (otherwise slot[1] would be 
  // misaligned).
  // 
  // (effective + slotAlign - 1) & ~(slotAlign - 1) is the standard power-of-two
  // round-up formula. See ArenaLM implementation for the full explanation.

  effective = (effective + _slotAlign - 1) & ~(_slotAlign - 1);

  // Example: T = u8 (size=1, align=1)
  // After size constraint:       max(1, 8) = 8
  // After alignment constraint:  roundUp(8, 1) = 8
  // Result:                      8 bytes per slot (7 bytes padding)
  //
  // Example: T = vec3 (size=12, align=4)
  // After size constraint:       max(12, 8) = 12
  // After alignment constraint:  roundUp(12, 4) = 12  (already a multiple of 4)
  // Result:                      12 bytes per slot (no padding)
  //
  // Example: T = SimdVec4 (size=16, align=16)
  // After size constraint:       max(16, 8) = 16
  // After alignment constraint:  roundUp(16, 16) = 16  (already aligned)
  // Result:                      16 bytes per slot (no padding)
  //
  // Example: T = struct { f32 x; f32 y; f32 z; } __attribute__((aligned(16))) (size=12, align=16)
  // After size constraint:       max(12, 8) = 12
  // After alignment constraint:  roundUp(12, 16) = 16
  // Result:                      16 bytes per slot (4 bytes padding to satisfy alignment)

  return effective;
}

usize PoolAllocatorLM::computeBlockSize(usize _slotSize, usize _slotAlign, usize _capacity) noexcept
{
  return computeEffectiveSlotSize(_slotSize, _slotAlign) * _capacity;
}

// Called by both factory functions after the backing block is obtained.
// Threads all slots together into the intrusive free list.
//
// The free list is a singly-linked list embedded in the slot memory itself.
// Each free slot stores one pointer: the address of the next free slot.
// The last free slot stores nullptr.
//
// After init, the layout in memory looks like this (capacity=4, slotSize=16):
// slot[0]: ptr > slot[1]
// slot[1]: ptr > slot[2]
// slot[2]: ptr > slot[3]
// slot[3]: ptr > nullptr
// m_freeHead > slot[0]
//
// alloc() pops slot[0]: m_freeHead becomes slot[1], returns slot[0].
// free(slot[0]) pushes slot[0]: slot[0].ptr = slot[1], m_freeHead = slot[0].

void PoolAllocatorLM::init(void* _block, usize _slotSize, usize _slotAlign, usize _capacity) noexcept
{
  m_block = static_cast<u8*>(_block);
  m_effectiveSlotSize = computeEffectiveSlotSize(_slotSize, _slotAlign);
  m_capacity = _capacity;
  m_usedSlots = 0;

#if LAMANCHA_DEBUG
  m_blockSize = m_effectiveSlotSize * _capacity;
#endif

  // Thread all slots into the free list.
  // We write a void* into the first sizeof(void*) bytes of each slot.
  // The last slot is an end of list sentinel (nullptr).

  for (usize i = 0; i < _capacity; ++i)
  {
    u8* thisSlot = m_block + i * m_effectiveSlotSize;
    void* nextSlot = (i + 1 < _capacity)
      ? static_cast<void*>(m_block + (i + 1) * m_effectiveSlotSize)
      : nullptr;

    // Write the next pointer into the slot's memory.
    // 
    // reinterpret_cast is safe her because we own this memory and it is at least
    // sizeof(void*) bytes (guaranteed by computeEffectiveSlotSize).

    *reinterpret_cast<void**>(thisSlot) = nextSlot;
  }

  m_freeHead = (_capacity > 0) ? static_cast<void*>(m_block) : nullptr;
}

Optional<PoolAllocatorLM> PoolAllocatorLM::fromArena(ArenaLM& _arena, usize _slotSize, usize _slotAlign, usize _capacity) noexcept
{
  LAMANCHA_ASSERT(_capacity > 0, "PoolAllocatorLM::fromArena > Invalid capacity");

  usize blockSize = computeBlockSize(_slotSize, _slotAlign, _capacity);

  Optional<void*> block = _arena.alloc(blockSize, _slotAlign);
  if (!block) { return Optional<PoolAllocatorLM>::None(); }

  PoolAllocatorLM pool;
  pool.init(block.value(), _slotSize, _slotAlign, _capacity);
  return Optional<PoolAllocatorLM>::Some(move(pool));
}

Optional<PoolAllocatorLM> PoolAllocatorLM::fromExternal(void* _ptr, usize _blockSize, usize _slotSize, usize _slotAlign, usize _capacity) noexcept
{
  LAMANCHA_ASSERT(_ptr != nullptr, "PoolAllocatorLM::fromExternal > Pointer already initialized");
  LAMANCHA_ASSERT(_capacity > 0, "PoolAllocatorLM::fromExternal > Invalid capacity");

  usize required = computeBlockSize(_slotSize, _slotAlign, _capacity);
  if (_blockSize < required) // Caller did not provide enough memory.
  {
    // Return None rather than silently truncating capacity since a truncated pool 
    // would produce confusing behavior at runtime.

    return Optional<PoolAllocatorLM>::None();
  }

  PoolAllocatorLM pool;
  pool.init(_ptr, _slotSize, _slotAlign, _capacity);
  return Optional<PoolAllocatorLM>::Some(move(pool));
}

PoolAllocatorLM::PoolAllocatorLM(PoolAllocatorLM&& other) noexcept
  : m_block(other.m_block), m_freeHead(other.m_freeHead)
  , m_effectiveSlotSize(other.m_effectiveSlotSize)
  , m_capacity(other.m_capacity), m_usedSlots(other.m_usedSlots)
#if LAMANCHA_DEBUG
  , m_blockSize(other.m_blockSize)
#endif
{
  // Null out the source since it no longer manages this block.

  other.m_block = nullptr;
  other.m_freeHead = nullptr;
  other.m_effectiveSlotSize = 0;
  other.m_capacity = 0;
  other.m_usedSlots = 0;
#if LAMANCHA_DEBUG
  other.m_blockSize = 0;
#endif
}

PoolAllocatorLM& PoolAllocatorLM::operator=(PoolAllocatorLM&& _other) noexcept
{
  if (this != &_other)
  {
    m_block = _other.m_block;
    m_freeHead = _other.m_freeHead;
    m_effectiveSlotSize = _other.m_effectiveSlotSize;
    m_capacity = _other.m_capacity;
    m_usedSlots = _other.m_usedSlots;
#if LAMANCHA_DEBUG
    m_blockSize = _other.m_blockSize;
#endif

    _other.m_block = nullptr;
    _other.m_freeHead = nullptr;
    _other.m_effectiveSlotSize = 0;
    _other.m_capacity = 0;
    _other.m_usedSlots = 0;
#if LAMANCHA_DEBUG
    _other.m_blockSize = 0;
#endif
  }
  return *this;
}

Optional<void*> PoolAllocatorLM::alloc() noexcept
{
  if (!m_freeHead)
  {
    // Pool is exhausted. 
    // Caller handles None.

    return Optional<void*>::None();
  }

  // Pop the free list head.
  // 
  // The current head slot stores a pointer to the next free slot in its first
  // sizeof(void*) bytes. We read that pointer, advance the head, and return
  // the slot to the caller.

  void* slot = m_freeHead;
  m_freeHead = *reinterpret_cast<void**>(slot);
  ++m_usedSlots;

  return Optional<void*>::Some(slot);
}

void PoolAllocatorLM::free(void* _ptr) noexcept {
  LAMANCHA_ASSERT(_ptr != nullptr, "PoolAllocatorLM::free > Pointer already initialized");

#if LAMANCHA_DEBUG
  // Bounds check.
  // _ptr must lie within the backing block and be slot-aligned.

  u8* p = static_cast<u8*>(_ptr);
  usize offset = static_cast<usize>(p - m_block);
  LAMANCHA_ASSERT(p >= m_block && p < m_block + m_blockSize, "PoolAllocatorLM::free > Pointer is outside this pool's block");
  LAMANCHA_ASSERT(offset % m_effectiveSlotSize == 0, "PoolAllocatorLM::free > Pointer is not aligned to a slot boundary");
#endif

  // Push onto the free list head.
  // 
  // We write the current head pointer into the first sizeof(void*) bytes
  // of the returned slot, then make that slot the new head.

  * reinterpret_cast<void**>(_ptr) = m_freeHead;
  m_freeHead = _ptr;
  --m_usedSlots;
}