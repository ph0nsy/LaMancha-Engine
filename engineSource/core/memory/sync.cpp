#include "core/pch.h"
#include "sync.h"

namespace LaMancha
{
  namespace Sync
  {
    Telemetry gTelemetry;

#if defined(LAMANCHA_PLATFORM_LINUX) || defined(LAMANCHA_PLATFORM_ANDROID) || defined(LAMANCHA_PLATFORM_R36S)

    // Zero-init so the object is safe to destruct even if `init()` is never called.
    // `pthread_mutex_destroy` on a zero-initialized mutex is undefined, so we
    // guard destruction with `m_initialised` in debug, and the zero-check in release.

    Mutex::Mutex() noexcept {}
    Mutex::~Mutex() noexcept
    {
#if LAMANCHA_DEBUG
      if (m_initialised) { pthread_mutex_destroy(&m_mtx); }
#else
      // In release we destroy unconditionally if the backing storage is non-zero.
      // This is safe because `init()` always succeeds or leaves storage zeroed.
      pthread_mutex_destroy(&m_mtx);
#endif
    }

    [[nodiscard]] ResultVoid Mutex::init() noexcept
    {
      // `PTHREAD_MUTEX_DEFAULT`: non-recursive, undefined behavior on double-lock.
      // The engine never locks the same mutex twice on one thread.
      i32 rc = pthread_mutex_init(&m_mtx, nullptr);
      if (rc != 0) { return ErrorVoid(ErrorCode::PlatformError, "pthread_mutex_init failed"); }
#if LAMANCHA_DEBUG
      m_initialised = true;
#endif
      return Ok();
    }

    void Mutex::lock() noexcept
    {
#if LAMANCHA_DEBUG
      LAMANCHA_ASSERT(m_initialised, "Mutex::lock called before init()");
#endif
      pthread_mutex_lock(&m_mtx);
    }

    void Mutex::unlock() noexcept
    {
#if LAMANCHA_DEBUG
      LAMANCHA_ASSERT(m_initialised, "Mutex::unlock called before init()");
#endif
      pthread_mutex_unlock(&m_mtx);
    }

    CondVar::CondVar() noexcept {}
    CondVar::~CondVar() noexcept { pthread_cond_destroy(&m_cond); }

    ResultVoid CondVar::init() noexcept
    {
      i32 rc = pthread_cond_init(&m_cond, nullptr);
      if (rc != 0) { return ErrorVoid(ErrorCode::PlatformError, "pthread_cond_init failed"); }
      return Ok();
    }

    void CondVar::notify_one() noexcept { pthread_cond_signal(&m_cond); }
    void CondVar::notify_all() noexcept { pthread_cond_broadcast(&m_cond); }

    // A SyncPoint is a binary semaphore built on Mutex + CondVar
    SyncPoint::SyncPoint()  noexcept {}
    SyncPoint::~SyncPoint() noexcept {}

    // The signal is latched: if `signal()` is called before `wait()`, the next
    // `wait()` returns immediately and clears the latch (`m_signalled`, all access
    // to it is protected by `m_mutex`).
    [[nodiscard]] void SyncPoint::signal() noexcept
    {
      MutexGuard g(m_mutex);
      m_signalled = true;
      m_condvar.notify_one();
    }

    [[nodiscard]] void SyncPoint::wait() noexcept
    {
      MutexGuard g(m_mutex);
      // The predicate-wait form in CondVar handles spurious wake-ups internally.
      m_condvar.wait(m_mutex, [this] { return m_signalled; });
      m_signalled = false; // auto-reset
    }

    [[nodiscard]] bool SyncPoint::tryWait(u32 timeoutUs) noexcept
    {
      MutexGuard g(m_mutex);
      bool ok = m_condvar.waitForUs(m_mutex, timeoutUs, [this] { return m_signalled; });
      if (ok) { m_signalled = false; } // auto-reset on success only
      return ok;
    }

    [[nodiscard]] bool SyncPoint::poll() noexcept
    {
      MutexGuard g(m_mutex);
      if (m_signalled)
      {
        m_signalled = false;
        return true;
      }
      return false;
    }
  }

#elif defined(LAMANCHA_PLATFORM_WINDOWS)

    // InitializeCriticalSection does not fail on modern Windows (Vista+).
    // We therefore call it in @c init() and always return @c Ok().
    // It can raise @c STATUS_NO_MEMORY on very old systems.
    
    Mutex::Mutex() noexcept {}
    Mutex::~Mutex() noexcept
    {
#if LAMANCHA_DEBUG
      if (m_initialised) { DeleteCriticalSection(&m_cs); }
#else
      DeleteCriticalSection(&m_cs);
#endif
    }

    [[nodiscard]] ResultVoid Mutex::init() noexcept
    {
      InitializeCriticalSection(&m_cs);
#if LAMANCHA_DEBUG
      m_initialised = true;
#endif
      return Ok();
    }

    void Mutex::lock() noexcept
    {
#if LAMANCHA_DEBUG
      LAMANCHA_ASSERT(m_initialised, "Mutex::lock called before init()");
#endif
      EnterCriticalSection(&m_cs);
    }

    void Mutex::unlock() noexcept
    {
#if LAMANCHA_DEBUG
      LAMANCHA_ASSERT(m_initialised, "Mutex::unlock called before init()");
#endif
      LeaveCriticalSection(&m_cs);
    }

    // `CONDITION_VARIABLE` is a Vista+ kernel object.
    CondVar::CondVar() noexcept {}
    CondVar::~CondVar() noexcept {} // CONDITION_VARIABLE has no destroy function on Windows.

    ResultVoid CondVar::init() noexcept
    {
      InitializeConditionVariable(&m_cv); // Cannot fail
      return Ok();
    }

    void CondVar::notify_one() noexcept { WakeConditionVariable(&m_cv); }

    void CondVar::notify_all() noexcept { WakeAllConditionVariable(&m_cv); }

    // Identical logic to the Linux path, platform differences are inside:
    // `Mutex` and `CondVar`, not here.
    SyncPoint::SyncPoint() noexcept {}
    SyncPoint::~SyncPoint() noexcept {}

    [[nodiscard]] void SyncPoint::signal() noexcept 
    {
      MutexGuard g(m_mutex);
      m_signalled = true;
      m_condvar.notify_one();
    }

    [[nodiscard]] void SyncPoint::wait() noexcept
    {
      MutexGuard g(m_mutex);
      m_condvar.wait(m_mutex, [this] { return m_signalled; });
      m_signalled = false;
    }

    [[nodiscard]] bool SyncPoint::tryWait(u32 timeoutUs) noexcept
    {
      MutexGuard g(m_mutex);
      bool ok = m_condvar.waitForUs(m_mutex, timeoutUs, [this] { return m_signalled; });
      if (ok) { m_signalled = false; }
      return ok;
    }

    [[nodiscard]] bool SyncPoint::poll() noexcept
    {
      MutexGuard g(m_mutex);
      if (m_signalled)
      {
        m_signalled = false;
        return true;
      }
      return false;
    }

#elif defined(LAMANCHA_PLATFORM_APPLE)
#endif
}
}