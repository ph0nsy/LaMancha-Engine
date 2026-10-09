/**
 * @file sync.h
 * @brief Threading Primitives for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * @note thread.h includes this header
 */

#pragma once

#include "core/pch.h"
#include <atomic>

namespace LaMancha {
  namespace Sync {
    /**
     * @brief Thin wrapper around std::atomic<T> with intent-named operations
     *
     * Memory ordering is chosen by the operation name, not at the call site.
     * This makes incorrect ordering a compile-time naming error rather than
     * a runtime data race.
     *
     * Naming convention:
     * - load_      > read only
     * - store_     > write only
     * - fetchAdd_  > atomic increment, returns old value
     * - fetchSub_  > atomic decrement, returns old value
     * - fetchOr_   > atomic bitwise OR, returns old value
     * - fetchAnd_  > atomic bitwise AND, returns old value
     * - exchange_  > unconditional swap, returns old value
     * - casWeak_   > compare-and-swap, may spuriously fail, use in retry loops
     * - casStrong_ > compare-and-swap, never spuriously fails, use for one-shot decisions
     *
     * @note Decision Guide:
     * 
     * Am I just reading?
     * - Is it my own counter, only I write it?         > loadRelaxed
     * - Am I reading a flag another thread published?  > loadAcquire
     *
     * Am I just writing?
     * - Is no other thread waiting on this value?      > storeRelaxed
     * - Am I publishing data another thread will read? > storeRelease
     *
     * Am I doing a fetch_add / fetch_sub?
     * - Pure counter, no coordination?                 > _Relaxed
     * - Publishing a new count (seq number, etc)?      > _Release
     * - Claiming a slot in a shared structure?         > _AcqRel
     * - Ref-count decrement to zero check?             > fetchSubAcqRel
     *
     * Am I doing an exchange?
     * - Swap with no coordination?                     > exchangeRelaxed
     * - Spinlock acquire / ownership transfer?         > exchangeAcqRel
     *
     * Am I doing a compare-and-swap?
     * - Inside a retry loop, simple publish on win?    > casWeakRelease
     * - Inside a retry loop, need to see prior data?   > casWeakAcqRel
     * - One-shot decision, cannot retry?               > casStrongAcqRel
     */
    template<typename T>
    struct Atomic
    {
      Atomic() noexcept = default;
      explicit Atomic(T val) noexcept : m_val(val) {}

      Atomic(const Atomic&) = delete;
      Atomic& operator=(const Atomic&) = delete;

      /**
       * @brief Relaxed: reading own counter (producer reads its own tail, etc.)
       *
       * No ordering guarantee relative to other memory ops. Use when reading
       * your own counter that no other thread coordinates on.
       */
      T loadRelaxed() const noexcept { return m_val.load(std::memory_order_relaxed); }

      /**
       * @brief Acquire: reading another thread's published value
       *
       * All writes that happened before the paired storeRelease are visible
       * after this load returns. Guarantees all writes before the paired
       * release are visible here. Use when reading a flag that another thread
       * set after writing data.
       */
      T loadAcquire() const noexcept { return m_val.load(std::memory_order_acquire); }

      /**
       * @brief Strongest ordering.
       *
       * All threads agree on the order of all seq_cst ops.
       * Use when you genuinely need a total global order across multiple atomics.
       *
       * @note Rarely needed, prefer loadAcquire unless you are certain.
       * @note Cost of full memory barrier on ARM (DMB ISH instruction).
       */
      T loadSeqCst() const noexcept { return m_val.load(std::memory_order_seq_cst); }

      /**
       * @brief Relaxed: writing own counter, no cross-thread visibility needed
       *
       * No ordering guarantee. Compiler and CPU may reorder freely. Use when
       * writing a counter that only you read back.
       *
       */
      void storeRelaxed(T val) noexcept { m_val.store(val, std::memory_order_relaxed); }

      /**
       * @brief Released: publishing data
       *
       * Makes all preceding writes visible to any thread that does a
       * paired acquire load of this atomic.
       *
       * All writes before this store are committed before this store is visible.
       * Use when publishing data to another thread (write data first, then flag).
       *
       */
      void storeRelease(T val) noexcept { m_val.store(val, std::memory_order_release); }

      /**
       * @brief Read-Modify-Write. Weak Compare And Swap (may spuriously fail even if *this == expected).
       *
       * - On success: *this = desired, returns true.
       *
       * - On failure: expected = *this (current value), returns false.
       *
       * Use when inside a retry loop (spurious failure just means one extra
       * iteration, which is cheaper than the stronger guarantee of casStrong).
       *
       * Ordering:
       *
       * - Release on success (publish your write)
       *
       * - Relaxed on failure (you didn't write anything, so no ordering needed).
       *
       * @note On failure, expected is updated to the current value
       * @note Used in SPSCQueue: compare-and-swap in the retry loop.
       * SPSC queue advancing write index only if it matches expectation.
       */
      bool casWeakRelease(T& expected, T desired) noexcept
      {
        return m_val.compare_exchange_weak(
          expected, desired,
          std::memory_order_release,
          std::memory_order_relaxed);
      }

      /**
       * @brief Weak Compare And Swap (with acq_rel on success).
       *
       * Use when inside a retry loop where you need to see prior writes on
       * success.
       *
       * Example:
       *
       * Lock-free stack push where you read the old top,
       * write the new node's next pointer, then CAS the top pointer.
       * The acquire on success ensures you see the old top's data.
       * The release ensures the new node's next pointer is visible before
       * the top pointer update is seen.
       */
      bool casWeakAcqRel(T& expected, T desired) noexcept
      {
        return m_val.compare_exchange_weak(
          expected, desired,
          std::memory_order_acq_rel,
          std::memory_order_acquire);
      }

      /**
       * @brief Strong Compare And Swap (never spuriously fails).
       *
       * Use when making a one-shot decision where a spurious failure is
       * not acceptable (you cannot retry, or retry has a visible side effect).
       *
       * Ordering:

       * - On success: acq_rel (you see prior writes and publish yours).
       *
       * - On failure: acquire (you need to see the current value accurately).
       *
       * Example:
       * Claiming a unique worker thread ID from a global pool. You call this
       * once; if it fails, you take a different code path entirely.
       *
       * @note Costs more than casWeak on some architectures (LL/SC loop internally).
       */
      bool casStrongAcqRel(T& expected, T desired) noexcept
      {
        return m_val.compare_exchange_strong(
          expected, desired,
          std::memory_order_acq_rel,
          std::memory_order_acquire);
      }

      /**
       * @brief Atomically -> old = *this; *this = val; return old.
       *
       * No ordering beyond atomicity.
       *
       * Use when swapping a value with no cross-thread coordination needed.
       */
      T exchangeRelaxed(T val) noexcept { return m_val.exchange(val, std::memory_order_relaxed); }

      /**
       * @brief Exchange with acquire and release.
       *
       * Use when implementing a spinlock acquisition. The acq_rel ensures you see
       * what the previous holder wrote(acquire), and your writes in the critical
       * section are visible to the next holder(release).
       *
       * @code
       *
       *   while (lock.exchangeAcqRel(1) == 1) { spin }
       *   // critical section, you see all writes from prior lock holders
       *   lock.storeRelease(0);
       *
       * @endcode
       */
      T exchangeAcqRel(T val) noexcept { return m_val.exchange(val, std::memory_order_acq_rel); }

      /**
       * @brief fetch_add with acquire semantics on the read half.
       *
       * Use when claiming a slot index and needing to see what the previous
       * occupant wrote to that slot before you write to it.
       *
       * @note It's less common, fetchAddAcqRel is usually what you want for slot claiming.
       */
      T fetchAddAcquire(T val) noexcept { return m_val.fetch_add(val, std::memory_order_acquire); }

      /**
       * @brief fetch_add with release semantics on the write half.
       *
       * Ordering doesn't matter for counters. Use when incrementing
       * a sequence number that other threads spin-wait on, and you
       * need them to see your prior writes after they see the new count.
       *
       *
       * Example:
       * Publishing N items to a SPSC queue by advancing the write counter.
       *
       * @note Used in telemetry counters
       */
      T fetchAddRelaxed(T val) noexcept { return m_val.fetch_add(val, std::memory_order_relaxed); }

      /**
       * @brief fetch_add with both acquire and release semantics.
       *
       * Use when claiming a slot in a multi-producer ring buffer.
       *
       * Example:
       * AtomicRingBufferLM::push (each producer claims a unique slot index).
       *
       * @note The acquire ensures you see the prior consumer's slot.ready=0 write.
       * @note The release ensures your subsequent data write is visible after the claim.
       */
      T fetchAddAcqRel(T val) noexcept { return m_val.fetch_add(val, std::memory_order_acq_rel); }

      /**
       * @brief No ordering beyond atomicity.
       *
       * Use when decrementing a counter with no cross-thread coordination.
       */
      T fetchSubRelaxed(T val) noexcept { return m_val.fetch_sub(val, std::memory_order_relaxed); }

      /**
       * @brief Release semantics on the write half
       *
       * Use when: releasing a resource (writes before this decrement must be visible
       * before the count update is seen).
       *
       * @note Used in GPUMemoryTracker::release: must be visible to the next request()
       * the GPU work is done before we reduce the in-flight byte count.
       */
      T fetchSubRelease(T val) noexcept { return m_val.fetch_sub(val, std::memory_order_release); }

      /**
       * @brief Acquire and release.
       *
       * Use when reference counting (decrement to zero means you must see all
       * prior increments, acquire, and publish the destruction, release).
       *
       * @code
       *
       *   if (obj.refCount.fetchSubAcqRel(1) == 1) { destroy(obj); }
       *
       * @endcode
       *
       * @note The == 1 check means: I decremented from 1 to 0, I am the last holder.
       * @note The acquire ensures all prior increments are visible before we destroy.
       */
      T fetchSubAcqRel(T val) noexcept { return m_val.fetch_sub(val, std::memory_order_acq_rel); }

      /**
       * @brief Atomically -> old = *this; *this |= bits; return old. No ordering beyond atomicity.
       * Use when setting a flag bit in a shared bitmask where no thread
       * coordinates on the result immediately.
       *
       * Example:
       * Marking an asset slot as "load in progress" in a flags word.
       */
      T fetchOrRelaxed(T bits) noexcept { return m_val.fetch_or(bits, std::memory_order_relaxed); }

      /**
       * @brief fetch_or with release (set a bit and publish prior writes).
       *
       * Use when setting a "ready" bit after writing data, so another thread
       * that reads the bit with loadAcquire sees the data too.
       */
      T fetchOrRelease(T bits) noexcept { return m_val.fetch_or(bits, std::memory_order_release); }

      /**
       * @brief Atomically -> old = *this; *this &= bits; return old. No ordering beyond atomicity.
       *
       * Use when clearing a flag bit in a shared bitmask.
       *
       * Example:
       * Clearing the "load in progress" bit after an asset finishes loading.
       */
      T fetchAndRelaxed(T bits) noexcept { return m_val.fetch_and(bits, std::memory_order_relaxed); }

      /**
       * @bried fetch_and with release (clear a bit and publish prior writes).
       *
       * Use when clearing a "busy" flag after finishing work on shared data.
       */
      T fetchAndRelease(T bits) noexcept { return m_val.fetch_and(bits, std::memory_order_release); }

    private:
      std::atomic<T> m_val{};
    };

    /**
     * @brief Maps to pthread_mutex_t (Linux) or CRITICAL_SECTION (Windows)
     *
     * Only used paired with CondVar or for the two SyncPoints.
     * @note Non-recursive, non-copyable, non-movable
     */
    struct Mutex
    {
      Mutex() noexcept;
      ~Mutex() noexcept;

      Mutex(const Mutex&) = delete;
      Mutex& operator=(const Mutex&) = delete;
      Mutex(Mutex&&) = delete;
      Mutex& operator=(Mutex&&) = delete;

      /**
       * @brief Must be called once before lock() / unlock()
       * @returns Error(ErrorCode::PlatformError) if the OS refuses to initialise
       */
      [[nodiscard]]
      ResultVoid init() noexcept;

      void lock()   noexcept;
      void unlock() noexcept;

#if LAMANCHA_DEBUG
      bool m_initialised = false; ///< In debug: assert that init() was called before use.
#endif

#if defined(LAMANCHA_PLATFORM_LINUX)
      /**
       * @return Native handle for CondVar to use internally
       * @note Not part of the public API
       */
      pthread_mutex_t* native() noexcept { return &m_mtx; }
#elif defined(LAMANCHA_PLATFORM_WINDOWS)
      /**
       * @return Native handle for CondVar to use internally
       * @note Not part of the public API
       */
      CRITICAL_SECTION* native() noexcept { return &m_cs; }
#endif

    private:
#if defined(LAMANCHA_PLATFORM_LINUX)
      pthread_mutex_t  m_mtx;
#elif defined(LAMANCHA_PLATFORM_WINDOWS)
      CRITICAL_SECTION m_cs;
#endif

      friend struct CondVar;
      friend struct MutexGuard;
    };

    /**
      * @brief RAII lock
      * @note Same role as std::lock_guard
      */
    struct MutexGuard
    {
      explicit MutexGuard(Mutex& m) noexcept : m_mutex(m) { m_mutex.lock(); }
      ~MutexGuard() noexcept { m_mutex.unlock(); }

      MutexGuard(const MutexGuard&) = delete;
      MutexGuard& operator=(const MutexGuard&) = delete;

    private:
      Mutex& m_mutex;
    };

    /**
      * @brief Condition variable paired with Mutex
      *
      * The predicate-wait form is the ONLY wait interface exposed. Spurious
      * wakeup handling is enforced inside the wrapper, not at call sites.
      *
      * Example:
      * @code
      * MutexGuard g(mutex);
      * condvar.wait(mutex, [&]{ return flag; }); // blocks until flag is true
      * @endcode
      */
    struct CondVar
    {
      CondVar() noexcept;
      ~CondVar() noexcept;

      CondVar(const CondVar&) = delete;
      CondVar& operator=(const CondVar&) = delete;

      /**
       * @brief Must be called once before any other funciton
       * @returns Error(ErrorCode::PlatformError) if the OS refuses to initialise
       */
      [[nodiscard]]
      ResultVoid init() noexcept;

      /**
       * @brief Handles spurious wakeups internally (no need for a while loop)
       *
       * Atomically releases `mutex`, sleeps until `predicate()` returns true,
       * then re-acquires `mutex` before returning.
       */
      template<typename Predicate>
      void wait(Mutex& mutex, Predicate predicate) noexcept
      {
        while (!predicate())
        {
#if defined(LAMANCHA_PLATFORM_LINUX)
          pthread_cond_wait(&m_cond, mutex.native());
#elif defined(LAMANCHA_PLATFORM_WINDOWS)
          SleepConditionVariableCS(&m_cv, mutex.native(), INFINITE);
#endif
        }
      }

      /**
       * @brief Timed wait
       *
       * @return false if the timeout expired before predicate was true
       * @note Used for the Logic to Render 14 ms handoff
       */
      template<typename Predicate>
      bool waitForUs(Mutex& mutex, u32 timeout_us, Predicate predicate) noexcept
      {
        if (predicate()) { return true; }
#if defined(LAMANCHA_PLATFORM_LINUX)
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += static_cast<long>(timeout_us) * 1000;
        if (ts.tv_nsec >= 1000000000L)
        {
          ts.tv_sec += 1;
          ts.tv_nsec -= 1000000000L;
        }
        while (!predicate())
        {
          i32 rc = pthread_cond_timedwait(&m_cond, mutex.native(), &ts);
          if (rc == ETIMEDOUT) { return false; }
        }
        return true;
#elif defined(LAMANCHA_PLATFORM_WINDOWS)
        DWORD ms = (timeout_us + 999) / 1000;
        while (!predicate())
        {
          if (!SleepConditionVariableCS(&m_cv, mutex.native(), ms)) { return false; }
        }
        return true;
#endif
      }

      void notify_one() noexcept;
      void notify_all() noexcept;

    private:
#if defined(LAMANCHA_PLATFORM_LINUX)
      pthread_cond_t   m_cond;
#elif defined(LAMANCHA_PLATFORM_WINDOWS)
      CONDITION_VARIABLE m_cv;
#endif
    };

    struct Telemetry
    {
      Atomic<u32> frame_drops{ 0 };
      Atomic<u32> oom_frame_alloc{ 0 };
      Atomic<u32> oom_ecs_pool{ 0 };
      Atomic<u32> oom_gpu{ 0 };
      Atomic<u32> oom_log_dropped{ 0 };
      Atomic<u32> gc_jobs_processed{ 0 };
      Atomic<u32> assets_loaded{ 0 };
      Atomic<u32> assets_evicted{ 0 };
    };

    extern Telemetry gTelemetry;

    /**
      * @brief Reusable, directional, binary signal between two threads
      *
      * One thread calls signal(). Another calls wait() or tryWait().
      * The signal is auto-reset; once a waiting thread receives it,
      * it returns to the unsignalled state automatically.
      *
      * @note Policy decisions (what to do on timeout, drop counting, etc.) are the caller's responsibility
      *
      * Example:
      * @code
      * if (!g_logic_ready.tryWait(14000)) {
      *   // timed out > handle frame drop here, in the caller
      *   drop_count.fetchAddRelaxed(1);
      * }
      * @endcode
      */
    struct SyncPoint
    {
      SyncPoint()  noexcept;
      ~SyncPoint() noexcept;

      SyncPoint(const SyncPoint&) = delete;
      SyncPoint& operator=(const SyncPoint&) = delete;

      /**
       * @brief Initialises the internal Mutex and CondVar
       *
       * Must be called before signal()/wait()/tryWait().
       *
       * @returns Error(ErrorCode::PlatformError) if the OS refuses to initialise
       */
      [[nodiscard]]
      ResultVoid init() noexcept
      {
        if (ResultVoid r = m_mutex.init(); !r) { return r; }
        if (ResultVoid r = m_condvar.init(); !r) { return r; }
        return Ok();
      }

      /**
       * @brief Wake the waiting thread
       *
       * Safe to call from any thread at any time. If no thread is currently waiting,
       * the signal is latched; the next call to wait() or tryWait() returns immediately.
       */
      void signal() noexcept;

      /**
       * @brief Block until signal() is called
       *
       * Returns immediately if a signal is already latched. Auto-resets the signal on return.
       */
      void wait() noexcept;

      /**
       * @brief Block for at most a give amount of microseconds
       *
       * Auto-resets the signal on successful return; does NOT reset on timeout,
       * signal remains pending if it arrives late.
       *
       * @param timeoutUs Microseconds to wait for
       * @return true if the signal arrived in time; false if the timeout expired
       */
      [[nodiscard]]
      bool tryWait(u32 timeoutUs) noexcept;

      /**
       * @brief Non-blocking check
       *
       * Useful for polling without committing to a full wait.
       *
       * @return true and resets if signalled, false otherwise
       */
      [[nodiscard]]
      bool poll() noexcept;

    private:
      Mutex   m_mutex{};
      CondVar m_condvar{};
      bool    m_signalled = false;
    };
  }
}