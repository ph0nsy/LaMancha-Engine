#include "core/pch.h"
#include "core/memory/heap.h"

#if defined(LAMANCHA_PLATFORM_LINUX) || defined(LAMANCHA_PLATFORM_ANDROID) || defined(LAMANCHA_PLATFORM_R36S)
#include <sys/mman.h>   // mmap, munmap, mlock, PROT_*, MAP_*
#endif

namespace LaMancha {

  // Copy
  HeapAllocatorLM::HeapAllocatorLM(HeapAllocatorLM&& other) noexcept
    : m_base(other.m_base), m_current(other.m_current)
    , m_size(other.m_size), m_locked(other.m_locked)
  {
    other.m_base = nullptr;
    other.m_current = nullptr;
    other.m_size = 0;
    other.m_locked = false;
  }

  // Move
  HeapAllocatorLM& HeapAllocatorLM::operator=(HeapAllocatorLM&& other) noexcept {
    if (this != &other) 
    {
      destroy();
      m_base = other.m_base;
      m_current = other.m_current;
      m_size = other.m_size;
      m_locked = other.m_locked;
      other.m_base = nullptr;
      other.m_current = nullptr;
      other.m_size = 0;
      other.m_locked = false;
    }
    return *this;
  }

  // Called at thread startup to divide the OS region between arenas.
  // Uses the same alignment arithmetic as ArenaLM::alloc().
  // Not called during gameplay, only during thread initialisation.

  Optional<void*> HeapAllocatorLM::alloc(usize _size, usize _align) noexcept {
    LAMANCHA_ASSERT(m_base != nullptr, "HeapAllocatorLM::alloc > Called before create().");
    LAMANCHA_ASSERT(_size > 0, "HeapAllocatorLM::alloc > Invalid size.");
    LAMANCHA_ASSERT(_align > 0 && (_align & (_align - 1)) == 0, 
      "HeapAllocatorLM::alloc > Invalid alignment, expected power of 2.");

    uptr  raw = reinterpret_cast<uptr>(m_current);
    uptr  aligned = (raw + (_align - 1)) & ~(_align - 1);
    u8* ptr = reinterpret_cast<u8*>(aligned);

    if (ptr + _size > m_base + m_size) 
    {
      Logging::ErrorLog("HeapAllocatorLM: out of space, requested %zu bytes, "
        "%zu remaining. Increase thread memory budget.", _size, remaining());
      return Optional<void*>::None();
    }

    m_current = ptr + _size;
    return Optional<void*>::Some(static_cast<void*>(ptr));
  }

#if defined(LAMANCHA_PLATFORM_LINUX) || defined(LAMANCHA_PLATFORM_ANDROID) || defined(LAMANCHA_PLATFORM_R36S)
  Optional<HeapAllocatorLM> HeapAllocatorLM::create(usize _size) noexcept 
  {
    LAMANCHA_ASSERT(_size > 0, "HeapAllocatorLM::create > Invalid size.");

    // Round up to page boundary.
    static constexpr usize PAGE_SIZE = sysconf(_SC_PAGESIZE);
    usize aligned = (_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // MAP_ANONYMOUS: RAM.
    // MAP_PRIVATE: copy-on-write, not shared with other processes.
    // MAP_POPULATE: fault all pages in immediately rather than on first touch.
    // This avoids page-fault latency during gameplay. On kernels that do not 
    // support MAP_POPULATE, the flag is silently ignored.
    void* ptr = mmap(nullptr, aligned, PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);

    if (ptr == MAP_FAILED) 
    {
      Logging::CriticalLog("HeapAllocatorLM: mmap failed for %zu bytes", aligned);
      return Optional<HeapAllocatorLM>::None();
    }

    HeapAllocatorLM h;
    h.m_base = static_cast<u8*>(ptr);
    h.m_current = h.m_base;
    h.m_size = aligned;
    h.m_locked = false;

    // Attempt mlock.
    // Failure reasons on Linux:
    //  ENOMEM: process RLIMIT_MEMLOCK exceeded
    //  EPERM: no CAP_IPC_LOCK capability
    int lockResult = mlock(ptr, aligned);
    if (lockResult == 0) { h.m_locked = true; }
    else 
    {
      Logging::WarningLog("HeapAllocatorLM: mlock failed for %zu bytes, "
        "pages may be swapped under memory pressure. "
        "Engine continues normally.", aligned);
    }

    Logging::InfoLog("HeapAllocatorLM: %zu MB mapped at %p (locked: %s)",
      aligned / 1_MB, ptr, h.m_locked ? "yes" : "no");

    return Optional<HeapAllocatorLM>::Some(static_cast<HeapAllocatorLM&&>(h));
  }

  void HeapAllocatorLM::destroy() noexcept 
  {
    if (!m_base) { return; }

    if (m_locked) { munlock(m_base, m_size); }
    munmap(m_base, m_size);

    m_base = nullptr;
    m_current = nullptr;
    m_size = 0;
    m_locked = false;
  }

#elif defined(LAMANCHA_PLATFORM_WINDOWS)

  Optional<HeapAllocatorLM> HeapAllocatorLM::create(usize _size) noexcept 
  {
    LAMANCHA_ASSERT(_size > 0, "HeapAllocatorLM::create > Invalid size.");

    // VirtualAlloc reserves and commits in one call.
    // No manual rounding needed.
    // 
    // MEM_RESERVE: reserves the address range.
    // MEM_COMMIT: backs it with physical memory (or page file).
    // PAGE_READWRITE: read/write access.
    void* ptr = VirtualAlloc(nullptr, _size,
      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    if (!ptr) 
    {
      Logging::ErrorLog("HeapAllocatorLM: VirtualAlloc failed for %zu bytes (error %lu)", _size, GetLastError());
      return Optional<HeapAllocatorLM>::None();
    }

    HeapAllocatorLM h;
    h.m_base = static_cast<u8*>(ptr);
    h.m_current = h.m_base;
    h.m_size = _size;
    h.m_locked = false;

    // Attempt VirtualLock. Requires SeLockMemoryPrivilege. Log and continue on failure.
    if (VirtualLock(ptr, _size)) {
      h.m_locked = true;
    }
    else {
      Logging::InfoLog("HeapAllocatorLM: VirtualLock failed for %zu bytes "
        "(error %lu), pages may be paged out under pressure. "
        "Engine continues normally.", _size, GetLastError());
    }

    Logging::InfoLog("HeapAllocatorLM: %zu MB allocated at %p (locked: %s)",
      _size / 1_MB, ptr, h.m_locked ? "yes" : "no");

    return Optional<HeapAllocatorLM>::Some(static_cast<HeapAllocatorLM&&>(h));
  }

  void HeapAllocatorLM::destroy() noexcept {
    if (!m_base) { return; }

    if (m_locked) { VirtualUnlock(m_base, m_size); }

    // VirtualFree with MEM_RELEASE must pass 0 as size and the original
    // base address from VirtualAlloc.
    VirtualFree(m_base, 0, MEM_RELEASE);

    m_base = nullptr;
    m_current = nullptr;
    m_size = 0;
    m_locked = false;
  }
#endif
}