#include "core/pch.h"
#include "arena.h"

namespace LaMancha
{
  ArenaLM ArenaLM::fromExternal(void* _ptr, usize _size) noexcept
  {
    LAMANCHA_ASSERT(_ptr != nullptr, "ArenaLM::fromExternal > Pointer already initialized");
    LAMANCHA_ASSERT(_size > 0, "ArenaLM::fromExternal > Invalid size");

    ArenaLM a;
    a.m_base = static_cast<u8*>(_ptr);
    a.m_current = a.m_base;
    a.m_end = a.m_base + _size;
    a.m_ownsMemory = false;
    return a;
  }

  ArenaLM ArenaLM::fromHeap(usize _size) noexcept
  {
    LAMANCHA_ASSERT(_size > 0, "ArenaLM::fromHeap > Invalid size");

    void* block = malloc(_size);
    if (!block) 
    {
      // malloc failed. 
      // 
      // Return an empty arena (caller must check capacity() > 0).
      // We do not assert here because fromHeap is a bootstrap path and the caller
      // should handle allocation failure gracefully at that level.

      return ArenaLM{};
    }

    ArenaLM a;
    a.m_base = static_cast<u8*>(block);
    a.m_current = a.m_base;
    a.m_end = a.m_base + _size;
    a.m_ownsMemory = true;
    return a;
  }

  ArenaLM::~ArenaLM() noexcept
  {
    if (m_ownsMemory && m_base) 
    {
      free(m_base);
      m_base = nullptr;
      m_current = nullptr;
      m_end = nullptr;
    }
  }

  ArenaLM::ArenaLM(ArenaLM&& _other) noexcept
    : m_base(_other.m_base), m_current(_other.m_current)
    , m_end(_other.m_end), m_ownsMemory(_other.m_ownsMemory)
  {
    // Null out the source so its destructor does not free the block.

    _other.m_base = nullptr;
    _other.m_current = nullptr;
    _other.m_end = nullptr;
    _other.m_ownsMemory = false;
  }

  ArenaLM& ArenaLM::operator=(ArenaLM&& _other) noexcept
  {
    if (this != &_other)
    {
      // Free our current block if we own it.

      if (m_ownsMemory && m_base) { free(m_base); }

      m_base = _other.m_base;
      m_current = _other.m_current;
      m_end = _other.m_end;
      m_ownsMemory = _other.m_ownsMemory;

      _other.m_base = nullptr;
      _other.m_current = nullptr;
      _other.m_end = nullptr;
      _other.m_ownsMemory = false;
    }
    return *this;
  }

  Optional<void*> ArenaLM::alloc(usize _size, usize _align) noexcept
  {
    LAMANCHA_ASSERT(_size > 0, "ArenaLM::alloc > Invalid size");
    LAMANCHA_ASSERT(_align > 0 && (_align & (_align - 1)) == 0, "ArenaLM::alloc > Invalid alignment");

    // Round m_current up to the next multiple of align.
    //
    // Example: 
    // ptr=0x1003, align=16 (mask=0x0F)
    // ptr + mask = 0x1003 + 0x000F = 0x1012
    // & ~mask = 0x1012 & 0xFFF0 = 0x1010   // first 16-aligned address >= 0x1003
    //
    // Example: 
    // ptr=0x1010, align=16 (already aligned)
    // ptr + mask = 0x1010 + 0x000F = 0x101F
    // & ~mask = 0x101F & 0xFFF0 = 0x1010   // same address, no waste

    uptr raw = reinterpret_cast<uptr>(m_current);   // uptr = current address as an integer

    // mask = align - 1 (for example, align=16 > mask=0x0F)
    // ptr + mask = worst-case overshoot
    // & ~mask = clear the low bits > round down to alignment boundary

    uptr aligned = (raw + (_align - 1)) & ~(_align - 1);

    // Because we added (align-1) before clearing, rounding down gives us
    // the next aligned address AT OR AFTER ptr.

    u8* alignedPtr = reinterpret_cast<u8*>(aligned);
    if (alignedPtr + _size > m_end) { return Optional<void*>::None(); } // Check there is enough space after alignment padding.

    m_current = alignedPtr + _size;
    return Optional<void*>::Some(alignedPtr);
  }
}