#include "core/pch.h"
#include "thread.h"

// Platform headers for affinity and priority.
// Inside <thread.h>, tinycthread already includes <pthread.h> / <windows.h> internally, 
// but we need <sched.h> for cpu_set_t on Linux.
#if defined(LAMANCHA_PLATFORM_LINUX) || defined(LAMANCHA_PLATFORM_ANDROID) || defined(LAMANCHA_PLATFORM_R36S)
#include <sched.h>   // cpu_set_t, CPU_ZERO, CPU_SET, pthread_setaffinity_np
#endif

namespace LaMancha
{
  // tinycthread defines the thread_local macro(line 181 of tinycthread.h) :
  // `#define thread_local _Thread_local` which maps to @c __thread on GCC/Clang 
  // and `__declspec(thread)` on MSVC.
  //
  // We use it here for a single u8-sized value. One TLS slot, one register
  // read per access: no sys call, no lock, no allocation.
  
  static thread_local CoreAffinity tl_role = CoreAffinity::Unknown;
   
  Thread::Thread() noexcept {}
  Thread::~Thread() noexcept
  {
    // A Thread destroyed while running means `join()` was forgotten.
    // Assert in debug; defensively join in release to avoid the OS thread
    // running with a dangling TrampolineArg pointer.
    LAMANCHA_ASSERT(!m_running.loadAcquire(), "Thread > Thread destroyed while still running. Call join() first");
    if (m_started && m_running.loadAcquire())
    {
      i32 res;
      thrd_join(m_handle, &res);
    }
  }

  // This is the actual function thrd_create calls on the new OS thread.
  i32 Thread::trampoline(void* rawArg) noexcept
  {
    TrampolineArg* ta = static_cast<TrampolineArg*>(rawArg);

    // Set thread-local role (currentRole() works from here forward).
    tl_role = ta->config.affinity;

    // User work (entry function).
    i32 result = ta->config.entry(ta->config.arg);

    // Mark thread as no longer running. Release store ensures the caller of
    // `join()` sees all writes the entry function made before observing
    // `m_running == false`.
    ta->owner->m_running.storeRelease(false);

    return result;
  }

  ResultVoid Thread::start(ThreadConfig conf) noexcept
  {
    if (!conf.entry)
    {
      return ErrorVoid(ErrorCode::InvalidArgument, "Thread::start, entry is null");
    }
    if (conf.affinity == CoreAffinity::Unknown)
    {
      return ErrorVoid(ErrorCode::InvalidArgument, "Thread::start, CoreAffinity::Unknown is not valid");
    }
    if (m_running.loadAcquire())
    {
      return ErrorVoid(ErrorCode::InvalidArgument, "Thread::start, thread is already running");
    }

    m_affinity = conf.affinity;

    // `m_trampolineArg` is a member so it lives until `join()` or `~Thread()`.
    // The trampoline reads it after thrd_create returns, so it must not
    // be a local variable inside `start()`.
    m_trampolineArg = { conf, this };

    // Create the OS thread via tinycthread: thrd_create returns
    // `thrd_success`, `thrd_nomem`, or `thrd_error`.
    i32 rc = thrd_create(&m_handle, trampoline, &m_trampolineArg);

    if (rc == thrd_nomem)
    {
      return ErrorVoid(ErrorCode::OutOfMemory, "Thread::start, kernel could not allocate thread stack");
    }
    if (rc != thrd_success)
    {
      return ErrorVoid(ErrorCode::PlatformError, "Thread::start, thrd_create failed");
    }

    m_started = true;

    // Set core affinity: `thrd_t` IS `pthread_t` on Linux and `HANDLE` on Windows
    // (no `native_handle()` indirection needed). We pass it directly to the
    // platform affinity API.
#if defined(LAMANCHA_PLATFORM_LINUX) || defined(LAMANCHA_PLATFORM_ANDROID) || defined(LAMANCHA_PLATFORM_R36S)
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(static_cast<i32>(conf.affinity), &cpuSet);

    i32 affinityRc = pthread_setaffinity_np(m_handle, sizeof(cpu_set_t), &cpuSet);
    if (affinityRc != 0)
    {
      // The thread runs correctly on any core, affinity is a performance
      // hint here. To make this fatal, join and return PlatformError instead.
      Logging::WarningLog("Thread::start, pthread_setaffinity_np failed (rc=%d), affinity not enforced", affinityRc);
    }

    // Utility Thread runs at slightly reduced priority so GC sweeps do not
    // compete with Logic and Main. The thread sets its own nice value because
    // `setpriority(PRIO_PROCESS)` on another thread requires `CAP_SYS_NICE`.
    // We signal intent here; the actual setpriority call belongs in the
    // Utility Thread's entry function after `tl_role` is set.

#elif defined(LAMANCHA_PLATFORM_WINDOWS)

    // On Windows `thrd_t` is `HANDLE` directly.
    DWORD_PTR mask = 1ULL << static_cast<i32>(conf.affinity);
    if (!SetThreadAffinityMask(m_handle, mask))
    {
      Logging::WarningLog("Thread::start, SetThreadAffinityMask failed (err=%lu)", GetLastError());
    }

    if (conf.affinity == CoreAffinity::Utility)
    {
      SetThreadPriority(m_handle, THREAD_PRIORITY_BELOW_NORMAL);
    }

#endif

    // Mark running after affinity is configured.
    // Release store: visible to any thread calling `isRunning()` with `loadAcquire`.
    m_running.storeRelease(true);
    return Ok();
  }

  ResultVoid Thread::join() noexcept {
    if (!m_started)
    {
      return ErrorVoid(ErrorCode::Unknown, "Thread::join, thread was never started");
    }

    // `thrd_join` blocks until the thread's entry function returns.
    // The second argument receives the value returned by the entry function.
    // We discard it, our result signaling goes through other channels
    // (Telemetry, SyncPoints, RingBuffers).
    i32 res;
    i32 rc = thrd_join(m_handle, &res);

    m_started = false;
    m_affinity = CoreAffinity::Unknown;

    // `m_running` is already false, the trampoline set it before returning.
    if (rc != thrd_success)
    {
      return ErrorVoid(ErrorCode::Unknown, "Thread::join, thrd_join failed");
    }

    return Ok();
  }

  CoreAffinity Thread::currentRole() noexcept { return tl_role; }
}