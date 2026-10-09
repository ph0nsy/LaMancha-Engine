#include "log.h"
#include "core/pch.h"

#if defined(LAMANCHA_PLATFORM_WINDOWS)

namespace LaMancha::Logging::Internal {
  LogRing gRing = {};
  CrashRing gCrashRing = {};
}

namespace {
  /** @brief File handle (RAII wrapper). */
  struct FileHandleOwner
  {
    HANDLE h = INVALID_HANDLE_VALUE;

    /**
     * @brief Runs at static destruction time, closing and flushing the handle.
     * @note Even if Log_Shutdown() is never called (unexpected termination).
     */
    ~FileHandleOwner() {
      if (h != INVALID_HANDLE_VALUE)
      {
        FlushFileBuffers(h);
        CloseHandle(h);
        h = INVALID_HANDLE_VALUE;
      }
    }
  };

  static FileHandleOwner sLogFile;

  /**
   * @brief Write one entry to console and file.
   * @note Called from Utility Thread only
   */
  static void writeEntry(const LaMancha::Logging::LogEntry& _e)
  {
    using namespace LaMancha::Logging;

    unsigned int idx = (_e.level < LOGLEVEL_COUNT) ? _e.level : static_cast<unsigned int>(LogLevel::Debug);
    const logStyle& style = styles[idx];

    char timeBuf[16];
    DWORD timeLen = wsprintfA(timeBuf, "[%02d:%02d] ", _e.hour, _e.minute);

    DWORD msgLen = 0;
    while (_e.msg[msgLen]) { ++msgLen; }

    DWORD prefixLen = 0;
    while (style.prefix[prefixLen]) { ++prefixLen; }

    // Debugger output
    OutputDebugStringA(timeBuf);
    OutputDebugStringA(style.prefix);
    OutputDebugStringA(_e.msg);
    OutputDebugStringA("\n");

    // Terminal output
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE)
    {
      DWORD written;
      SetConsoleTextAttribute(hOut, style.colorAttr_W);
      WriteConsoleA(hOut, timeBuf, timeLen, &written, NULL);
      WriteConsoleA(hOut, style.prefix, prefixLen, &written, NULL);
      WriteConsoleA(hOut, _e.msg, msgLen, &written, NULL);
      WriteConsoleA(hOut, "\n", 1, &written, NULL);
      SetConsoleTextAttribute(hOut, 0x0007); // Reset to default
    }

    // Log file output (no color codes in the file)
    if (style.toLogFile && sLogFile.h != INVALID_HANDLE_VALUE)
    {
      DWORD bw;
      WriteFile(sLogFile.h, timeBuf, timeLen, &bw, NULL);
      WriteFile(sLogFile.h, style.prefix, prefixLen, &bw, NULL);
      WriteFile(sLogFile.h, _e.msg, msgLen, &bw, NULL);
      WriteFile(sLogFile.h, "\n", 1, &bw, NULL);
      // `FlushFileBuffers` called once per drain cycle in the thread, not per entry.
    }
  }
} // end of anonymous namespace

void LaMancha::Logging::logInit()
{
  SYSTEMTIME st;
  GetLocalTime(&st);

  char fullPath[256];
  wsprintfA(fullPath, "%s_%04d-%02d-%02d.log", LOGFILE_PATH, st.wYear, st.wMonth, st.wDay);
  sLogFile.h = CreateFileA(
    fullPath, 
    FILE_APPEND_DATA, FILE_SHARE_READ, 
    NULL, OPEN_ALWAYS, 
    FILE_ATTRIBUTE_NORMAL, NULL);

  // Warm the ring on the main thread before any other thread is spawned.
  Log(LogLevel::Info, "Logging system initialized");
}

void LaMancha::Logging::logShutdown()
{
  using namespace Internal;

  bool wroteAny = false;
  while (gRing.tail != gRing.head)
  {
    unsigned int slot = gRing.tail & (LOG_RING_SIZE - 1);
    LogEntry& e = gRing.slots[slot];
    if (e.valid)
    {
      writeEntry(e);
      e.valid = false;
      ++gRing.tail;
      wroteAny = true;
    }
    else { break; }
  }
  if (wroteAny && sLogFile.h != INVALID_HANDLE_VALUE)
  {
    FlushFileBuffers(sLogFile.h);
  }
  // `sLogFile` destructor closes and flushes the handle.
}

bool LaMancha::Logging::logDrainOnce()
{
  using namespace Internal;

  bool wroteAny = false;

  // Drain up to `LOG_RING_SIZE` entries this call; return promptly
  // so GC and other Utility Thread work is not starved by a log burst.
  unsigned int limit = LOG_RING_SIZE;
  while (limit-- && gRing.tail != gRing.head) {
    unsigned int slot = gRing.tail & (LOG_RING_SIZE - 1);
    LogEntry& e = gRing.slots[slot];
    if (e.valid) {
      writeEntry(e);
      e.valid = false;
      ++gRing.tail;
      wroteAny = true;
    }
    else { break; }
  }

  if (wroteAny && sLogFile.h != INVALID_HANDLE_VALUE)
  {
    FlushFileBuffers(sLogFile.h);
  }
  return wroteAny;
}

bool LaMancha::Logging::logDrainOnceDebug()
{
  using namespace Internal;

  bool wroteAny = false;

  // Drain the entire ring before returning, log writes are latency bound.
  while (gRing.tail != gRing.head)
  {
    unsigned int slot = gRing.tail & (LOG_RING_SIZE - 1);
    LogEntry& e = gRing.slots[slot];
    if (e.valid)
    {
      writeEntry(e);
      e.valid = false;
      ++gRing.tail;
      wroteAny = true;
    }
    else { break; }
  }
  if (wroteAny && sLogFile.h != INVALID_HANDLE_VALUE)
  {
    FlushFileBuffers(sLogFile.h);
  }
  return wroteAny;
}

void LaMancha::Logging::Log(LogLevel _level, const char* _msg)
{
  using namespace Internal;

  unsigned int idx = static_cast<unsigned int>(InterlockedIncrement(reinterpret_cast<volatile LONG*>(&gRing.head))) - 1;
  idx &= (LOG_RING_SIZE - 1);
  LogEntry& slot = gRing.slots[idx];

  SYSTEMTIME st;
  GetLocalTime(&st);
  slot.hour = st.wHour;
  slot.minute = st.wMinute;
  slot.level = static_cast<unsigned int>(_level);

  int i = 0;
  while (_msg[i] && i < LOG_MSG_MAX - 1)
  {
    slot.msg[i] = _msg[i];
    ++i;
  }
  slot.msg[i] = '\0';

  // Mark valid LAST. Plain store is sufficient on x86/x64 (TSO).
  // However, on ARM, we use `__atomic_store_n(&slot.valid, true, __ATOMIC_RELEASE)`.
  slot.valid = true;

  // Written unconditionally on every `Log()` call. Wraps by modulo.
  // No valid flag needed, crash dump reads all slots sequentially.
  unsigned int crashIdx = static_cast<unsigned int>(InterlockedIncrement(reinterpret_cast<volatile LONG*>(&gCrashRing.head))) - 1;
  crashIdx &= (LOG_CRASH_RING_SIZE - 1);
  LogEntry& crashSlot = gCrashRing.slots[crashIdx];
  crashSlot.hour = st.wHour;
  crashSlot.minute = st.wMinute;
  crashSlot.level = (unsigned int)_level;
  i = 0;
  while (_msg[i] && i < LOG_MSG_MAX - 1)
  {
    crashSlot.msg[i] = _msg[i];
    ++i;
  }
  crashSlot.msg[i] = '\0';
}

void LaMancha::Logging::logWriteCrashDump()
{
  using namespace Internal;

  // On Windows a crash handler is typically a structured exception handler,
  // not a POSIX signal. WriteFile is safe to call from an SEH handler.
  if (sLogFile.h == INVALID_HANDLE_VALUE) { return; }

  const char header[] = "\r\n=== [ CRASH DUMP ] ===\r\n";

  DWORD bw;
  WriteFile(sLogFile.h, header, sizeof(header) - 1, &bw, NULL);

  // Walk the crash ring from oldest to newest.
  // 'head' points to the slot that will be written next, so 'head' is the oldest.
  unsigned int start = gCrashRing.head & (LOG_CRASH_RING_SIZE - 1);
  for (unsigned int n = 0; n < LOG_CRASH_RING_SIZE; ++n)
  {
    unsigned int si = (start + n) & (LOG_CRASH_RING_SIZE - 1);
    const LogEntry& e = gCrashRing.slots[si];
    if (e.msg[0] == '\0') { continue; } // Empty slot (early in session)

    char timeBuf[16];
    DWORD timeLen = wsprintfA(timeBuf, "[%02d:%02d] ", e.hour, e.minute);
    unsigned int li = (e.level < LOGLEVEL_COUNT) ? e.level : 0u;

    DWORD msgLen = 0;   while (e.msg[msgLen]) { ++msgLen; }
    DWORD pfxLen = 0;   while (styles[li].prefix[pfxLen]) { ++pfxLen; }

    WriteFile(sLogFile.h, timeBuf, timeLen, &bw, NULL);
    WriteFile(sLogFile.h, styles[li].prefix, pfxLen, &bw, NULL);
    WriteFile(sLogFile.h, e.msg, msgLen, &bw, NULL);
    WriteFile(sLogFile.h, "\r\n", 2, &bw, NULL);
  }

  FlushFileBuffers(sLogFile.h);
}

#elif defined(LAMANCHA_PLATFORM_LINUX) || defined(LAMANCHA_PLATFORM_ANDROID) || defined(LAMANCHA_PLATFORM_R36S)

namespace LaMancha::Logging::Internal {
  LogRing   gRing = {};
  CrashRing gCrashRing = {};
}

namespace {
  static int sLogFd = -1;

  // Completely separate write paths. No double-write, no color in file.
  static void writeEntry(const LaMancha::Logging::LogEntry& _e)
  {
    using namespace LaMancha::Logging;
    unsigned int idx = (_e.level < LOGLEVEL_COUNT) ? _e.level : static_cast<unsigned int>(LogLevel::Debug);
    const logStyle& style = styles[idx];

    char timeBuf[16];
    int timeLen = snprintf(timeBuf, sizeof(timeBuf), "[%02d:%02d] ", _e.hour, _e.minute);
    if (timeLen < 0 || timeLen >= static_cast<int>(sizeof(timeBuf)))
    {
      timeLen = static_cast<int>(sizeof(timeBuf)) - 1;
    }

    size_t msgLen = Utils::strLen(_e.msg);
    size_t prefixLen = Utils::strLen(style.prefix);
    size_t colorLen = Utils::strLen(style.colorCode_L);

    // Console output: color > timestamp > prefix > message > reset and newline
    write(1, style.colorCode_L, colorLen);
    write(1, timeBuf, static_cast<size_t>(timeLen));
    write(1, style.prefix, prefixLen);
    write(1, _e.msg, msgLen);
    write(1, "\033[0m\n", 5);

    // File output (no ANSI codes): timestamp > prefix > message > newline
    if (style.toLogFile && sLogFd != -1) {
      write(sLogFd, timeBuf, static_cast<size_t>(timeLen));
      write(sLogFd, style.prefix, prefixLen);
      write(sLogFd, _e.msg, msgLen);
      write(sLogFd, "\n", 1);
      // `fsync` called once per `Log_DrainOnce()`, not per entry.
    }
  }
} // anonymous namespace

void LaMancha::Logging::logInit()
{
  time_t now = time(NULL);
  struct tm* tm_ = localtime(&now);

  char fullPath[256];
  snprintf(fullPath, sizeof(fullPath), 
    "%s_%04d-%02d-%02d.log", LOGFILE_PATH, 
    tm_->tm_year + 1900, tm_->tm_mon + 1, tm_->tm_mday);
  sLogFd = open(fullPath, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);

  // Warm the ring on the main thread before any other thread is spawned.
  Log(LogLevel::Info, "Logging system initialized");
}

void LaMancha::Logging::logShutdown()
{
  using namespace Internal;
  bool wroteAny = false;
  while (gRing.tail != gRing.head)
  {
    unsigned int slot = gRing.tail & (LOG_RING_SIZE - 1);
    LogEntry& e = gRing.slots[slot];
    bool valid = __atomic_load_n(&e.valid, __ATOMIC_ACQUIRE);
    if (valid)
    {
      writeEntry(e);
      __atomic_store_n(&e.valid, false, __ATOMIC_RELEASE);
      ++gRing.tail;
      wroteAny = true;
    }
    else { break; }
  }

  if (wroteAny && sLogFd != -1) { fsync(sLogFd); }
  if (sLogFd != -1) 
  { 
    fsync(sLogFd); 
    close(sLogFd); 
    sLogFd = -1; 
  }
}

bool LaMancha::Logging::logDrainOnce()
{
  using namespace Internal;

  bool wroteAny = false;

  // Drain up to `LOG_RING_SIZE` entries; yield so GC and profiler
  // are not starved by a burst of log messages.
  unsigned int limit = LOG_RING_SIZE;
  while (limit-- && gRing.tail != gRing.head)
  {
    unsigned int slot = gRing.tail & (LOG_RING_SIZE - 1);
    LogEntry& e = gRing.slots[slot];
    bool valid = __atomic_load_n(&e.valid, __ATOMIC_ACQUIRE);
    if (valid)
    {
      writeEntry(e);
      __atomic_store_n(&e.valid, false, __ATOMIC_RELEASE);
      ++gRing.tail;
      wroteAny = true;
    }
    else { break; }
  }

  if (wroteAny && sLogFd != -1) { fsync(sLogFd); }
  return wroteAny;
}

bool LaMancha::Logging::logDrainOnceDebug()
{
  using namespace Internal;

  bool wroteAny = false;

  // Drain everything (log writes are latency-bound)
  while (gRing.tail != gRing.head)
  {
    unsigned int slot = gRing.tail & (LOG_RING_SIZE - 1);
    LogEntry& e = gRing.slots[slot];
    bool valid = __atomic_load_n(&e.valid, __ATOMIC_ACQUIRE);
    if (valid)
    {
      writeEntry(e);
      __atomic_store_n(&e.valid, false, __ATOMIC_RELEASE);
      ++gRing.tail;
      wroteAny = true;
    }
    else { break; }
  }

  if (wroteAny && sLogFd != -1) { fsync(sLogFd); }
  return wroteAny;
}

void LaMancha::Logging::Log(LogLevel _level, const char* _msg)
{
  using namespace Internal;

  // Streaming ring
  unsigned int idx = __atomic_fetch_add(&gRing.head, 1u, __ATOMIC_RELAXED);
  idx &= (LOG_RING_SIZE - 1);
  LogEntry& slot = gRing.slots[idx];

  time_t now = time(NULL);
  struct tm* tm_ = localtime(&now);
  slot.hour = tm_->tm_hour;
  slot.minute = tm_->tm_min;
  slot.level = static_cast<unsigned int>(_level);

  int i = 0;
  while (_msg[i] && i < LOG_MSG_MAX - 1)
  {
    slot.msg[i] = _msg[i];
    ++i;
  }
  slot.msg[i] = '\0';

  __atomic_store_n(&slot.valid, true, __ATOMIC_RELEASE);

  // Written on every `Log()` call. No valid flag, crash dump reads all slots.
  unsigned int crashIndex = __atomic_fetch_add(&gCrashRing.head, 1u, __ATOMIC_RELAXED);
  crashIndex &= (LOG_CRASH_RING_SIZE - 1);
  LogEntry& crashSlot = gCrashRing.slots[crashIndex];
  crashSlot.hour = tm_->tm_hour;
  crashSlot.minute = tm_->tm_min;
  crashSlot.level = static_cast<unsigned int>(_level);
  i = 0;
  while (_msg[i] && i < LOG_MSG_MAX - 1)
  {
    crashSlot.msg[i] = _msg[i];
    ++i;
  }
  crashSlot.msg[i] = '\0';
}

void LaMancha::Logging::logWriteCrashDump(int _crashFd)
{
  if (_crashFd < 0) { return; }

  using namespace Internal;

  // Uses only `write()`, safe to call from `SIGSEGV` / `SIGABRT` / `SIGFPE` handlers
  // (No malloc, no stdio, no C++ runtime calls).
  const char header[] = "\n=== [ CRASH DUMP ] ===\n";
  write(_crashFd, header, sizeof(header) - 1);

  // Walk from oldest to newest: 'head' is the next slot to be written,
  // so head+1 (mod size) is the oldest surviving entry.
  unsigned int start = (gCrashRing.head + 1) & (LOG_CRASH_RING_SIZE - 1);
  for (unsigned int n = 0; n < LOG_CRASH_RING_SIZE; ++n)
  {
    unsigned int si = (start + n) & (LOG_CRASH_RING_SIZE - 1);
    const LogEntry& e = gCrashRing.slots[si];
    if (e.msg[0] == '\0') { continue; } // empty slot (early in session)

    // Build timestamp inline, no snprintf (not async-signal-safe on all platforms).
    char timeBuf[12];
    timeBuf[0] = '[';
    timeBuf[1] = '0' + static_cast<char>(e.hour / 10);
    timeBuf[2] = '0' + static_cast<char>(e.hour % 10);
    timeBuf[3] = ':';
    timeBuf[4] = '0' + static_cast<char>(e.minute / 10);
    timeBuf[5] = '0' + static_cast<char>(e.minute % 10);
    timeBuf[6] = ']';
    timeBuf[7] = ' ';

    unsigned int li = (e.level < LOGLEVEL_COUNT) ? e.level : 0u;
    size_t msgLen = 0;
    while (e.msg[msgLen]) { ++msgLen; }
    size_t pfxLen = 0;
    while (styles[li].prefix[pfxLen]) { ++pfxLen; }

    write(_crashFd, timeBuf, 8);
    write(_crashFd, styles[li].prefix, pfxLen);
    write(_crashFd, e.msg, msgLen);
    write(_crashFd, "\n", 1);
  }

  // `fsync` is not async-signal-safe on all kernels; omit it here.
  // The OS will flush on process exit.
}
#elif defined(LAMANCHA_PLATFORM_MACOS) || defined(LAMANCHA_PLATFORM_IPHONE)
 // Maybe somewhere down the line
#endif

/**
  * @brief `LaMancha::Logging::Log()` wrapper processing args
  * @param _level Type of log (debug, warning, error...)
  * @param _format C String with format to fill with _args
  * @param _args Argument list to fill the message
  */
static inline void internalLogVarArg(LaMancha::Logging::LogLevel _level, const char* _format, va_list _args) {
  char buffer[512];
  vsnprintf(buffer, 512, _format, _args);
  LaMancha::Logging::Log(_level, buffer);
}

void LaMancha::Logging::DebugLog(const char* _fmt, ...)
{
  va_list args;
  va_start(args, _fmt);
  internalLogVarArg(LogLevel::Debug, _fmt, args);
  va_end(args);
}

void LaMancha::Logging::InfoLog(const char* _fmt, ...)
{
  va_list args;
  va_start(args, _fmt);
  internalLogVarArg(LogLevel::Info, _fmt, args);
  va_end(args);
}

void LaMancha::Logging::CriticalLog(const char* _fmt, ...) {
  va_list args;
  va_start(args, _fmt);
  internalLogVarArg(LogLevel::Critical, _fmt, args);
  va_end(args);
}

void LaMancha::Logging::WarningLog(const char* _fmt, ...)
{
  va_list args;
  va_start(args, _fmt);
  internalLogVarArg(LogLevel::Critical, _fmt, args);
  va_end(args);
}

void LaMancha::Logging::ErrorLog(const char* _fmt, ...)
{
  va_list args;
  va_start(args, _fmt);
  internalLogVarArg(LogLevel::Error, _fmt, args);
  va_end(args);
}