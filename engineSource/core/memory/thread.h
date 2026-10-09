/**
 * @file thread.h
 * @brief Thread class for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * Wraps TinyCThread with our own engine-specific behaviour:
 *
 * - Core affinity: each thread is pinned to a specific core at @c start().
 * @a CoreAffinity values map directly to physical core indices (0-3 on R36S).
 *
 * - Thread role identity: a @c thread_local @a CoreAffinity is set inside the thread
 * before calling the user entry point. Any code running on that thread can call
 * @c Thread::currentRole() to query its identity without a syscall.
 *
 * - @c Thread-safe isRunning(): @c m_running is an @a Atomic<bool> to prevent data
 * races when one thread checks @c isRunning() while another calls @c join().
 *
 * TinyCThread is used for the thread lifecycle (create, join, native_handle).
 * Affinity and priority are set via @c native_handle() after thread creation,
 * since TinyCThread does not expose these directly.
 *
 * If TinyCThread is replaced later, only thread.cpp changes. The header
 * and all call sites remain the same.
 *
 * @note thread.h already includes sync.h
 */

#pragma once

#include "sync.h"
#include "deps/tinycthread.h"

namespace LaMancha {
  /**
   * @brief Maps a thread's purpose to a physical core index.
   *
   * Values are used both as a logical role identifier (queryable via
   * Thread::currentRole()) and as a core index for affinity pinning.
   * On the R36S (4-core Cortex-A35), these map 1:1 to cores 0-3.
   *
   * Unknown is the initial state before a thread sets its own role.
   * It is also the role of threads not managed by ThreadLM (like the
   * OS audio callback thread).
   */
  enum CoreAffinity : u8
  {
    Main = 0,       ///< Render pipeline, input, timing
    Logic = 1,      ///< ECS systems, AI, Lua, project manager
    Asset = 2,      ///< Asset loading, decompression
    Utility = 3,    ///< GC sweeps, log drain, profiler
    Unknown = 255   ///< Not an engine-managed thread
  };

  /**
   * @brief Configuration passed to Thread::start().
   *
   * @param entry C-style function pointer. Takes a void* userdata argument.
   * No heap allocation needed, closures are not supported by design. Use a
   * pointer to a struct for multiple arguments.
   * @param arg Passed as the single argument to entry(). May be null if the
   * entry function does not need it.
   * @param affinity Determines which physical core this thread runs on AND
   * the value returned by Thread::currentRole() from within the thread.
   */
  struct ThreadConfig
  {
    CoreAffinity affinity;
    i32(*entry)(void*);
    void* arg;
  };

  /**
   * @brief Platform-agnostic thread abstraction backed by TinyThread++.
   *
   * @code
   *   Thread t;
   *   ResultVoid r = t.start({ CoreAffinity::Logic, myEntryFn, &myData });
   *   // Thread is running
   *   t.join();
   * @endcode
   *
   * @note Non-copyable and non-movable. A Thread object represents exactly one
   * OS thread. If you need to pass the Thread around, pass a pointer to it.
   */
  struct Thread {
    Thread()  noexcept;
    ~Thread() noexcept;

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    /**
     * @brief Create and start the thread with the given configuration.
     *
     * Sets core affinity and thread priority after the OS thread is created.
     * The thread's CoreAffinity is available via Thread::currentRole() from
     * within the thread immediately when entry() begins executing.
     *
     * @returns Ok() on success
     * @returns Error(InvalidArgument) if entry is null
     * @returns Error(InvalidArgument) if affinity is CoreAffinity::Unknown
     * @returns Error(OutOfMemory) if the kernel cannot allocate the stack
     * @returns Error(PlatformError) if thread creation or affinity fails
     */
    [[nodiscard]]
    ResultVoid start(ThreadConfig _conf) noexcept;

    /**
     * @brief Block until the thread finishes and release OS resources.
     *
     * After join() returns, isRunning() is false and start() may be called
     * again to reuse this Thread object.
     *
     * @returns Ok() on success
     * @returns Error(Unknown) if the thread was never started or already joined
     */
    [[nodiscard]]
    ResultVoid join() noexcept;

    /**
     * @brief Query the CoreAffinity of the currently executing thread.
     *
     * Set automatically by start() before calling the user entry function.
     * Readable from any code running on an engine-managed thread.
     * Returns CoreAffinity::Unknown on threads not created by Thread::start().
     *
     * Cost: one TLS register read (no syscall, no lock, no atomic).
     *
     * Use for debug ownership checks:
     * @code
     * LAMANCHA_ASSERT(Thread::currentRole() == CoreAffinity::Logic, "LevelArena must only be allocated from the Logic Thread");
     * @endcode
     */
    static CoreAffinity currentRole() noexcept;

    /**
     * @brief True if start() has been called and join() has not yet returned.
     *
     * Reading this from a thread other than the one that called start()/join()
     * is safe but the result is advisory, the thread may finish immediately
     * after the check returns true.
     *
     * @note Thread-safe since m_running is an Atomic<bool>.
     */
    LAMANCHA_INLINE bool isRunning() const noexcept { return m_running.loadAcquire(); }
    /**
     * @brief The affinity/role this thread was started with.
     *
     * Returns CoreAffinity::Unknown if start() has not been called.
     * Same value as Thread::currentRole() when called from within the thread.
     */
    LAMANCHA_INLINE CoreAffinity affinity() const noexcept { return m_affinity; }

  private:

    // Packages ThreadConfig + the Thread* itself for the trampoline.
    // Stored as a member so its lifetime covers thread startup.
    struct TrampolineArg {
      ThreadConfig  config;
      Thread* owner; ///< so trampoline can set m_running = false on exit
    };

    // Internal trampoline: sets thread-local role then calls user entry.
    // Passed to tthread::thread as the actual entry function.
    static i32 trampoline(void* rawArg) noexcept;

    Sync::Atomic<bool>  m_running{ false };
    CoreAffinity        m_affinity{ CoreAffinity::Unknown };
    TrampolineArg       m_trampolineArg{};
    thrd_t              m_handle{};         ///< pthread_t / HANDLE
    bool                m_started = false;  ///< thrd_t valid only when true
  };
}