/**
 * @file log.h
 * @brief Logging functions for LaMancha Engine (both specialized and generic log functions).
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 * 
 * @details
 */

#pragma once

#define LOGFILE_LOC "./"
#define LOGFILE_NAME "LM_Log"
#define LOGFILE_PATH LOGFILE_LOC LOGFILE_NAME

namespace LaMancha {

  namespace Utils
  {
    static inline size_t strLen(const char* s) 
    {
      size_t n = 0; 
      while(s[n]) { ++n; } 
      return n;
    }
  }

  namespace Logging {
    enum LogLevel {
      Debug = 0,
      Info,
      Warning,
      Critical,
      Error,
      LOGLEVEL_COUNT
    };

    struct logStyle 
    {
      LogLevel level = LogLevel::Debug;             ///< LogLevel this style is related to
      const char* prefix = "[?]    ";               ///< String that precedes the message
      const char* colorCode_L = "\033[00;37m";      ///< Color of the message (only for Linux terminal)
      const unsigned short colorAttr_W = 0x0007;    ///< Color of the message (only for Windows terminal)
      bool toLogFile = false;                       ///< True to store the message in a text file
    };

    constexpr logStyle styles[LOGLEVEL_COUNT] = 
    {
      { LogLevel::Debug, "[DBG]  " },
      { LogLevel::Info, "[INFO] ", "\033[00;36m", 0x0003 },
      { LogLevel::Warning, "[WARN] ", "\033[00;33m", 0x000E, true },
      { LogLevel::Critical, "[CRIT] ", "\033[41;37m", 0x004F, true },
      { LogLevel::Error, "[ERR]  ", "\033[00;31m", 0x000C, true }
    };

    static constexpr int LOG_MSG_MAX = 512;         ///< Maximum formatted message length(bytes, incl.null)
    static constexpr int LOG_RING_SIZE = 256;       ///< Number of slots. Must be a power of two
    // At 512 bytes/slot × 256 slots = 128 KB total ring memory.

    static constexpr int LOG_CRASH_RING_SIZE = 64;  ///< Last-N entries kept in memory for crash dumps.

    struct LogEntry 
    {
      char msg[LOG_MSG_MAX];
      unsigned int level;
      int hour;
      int minute;
      bool valid; ///< set last by producer; read first by consumer
    };

    namespace Internal 
    {
      /** @brief Internal ring. Defined in log.cpp, not for direct use. */
      struct LogRing 
      {
        LogEntry slots[LOG_RING_SIZE];
        volatile unsigned int head;   ///< claimed by producers (atomic)
        char _pad[60];                ///< keep head/tail on separate cache lines
        volatile unsigned int tail;   ///< advanced by consumer only
      };

      struct CrashRing 
      {
        LogEntry slots[LOG_CRASH_RING_SIZE];
        volatile unsigned int head;   ///< wraps modulo LOG_CRASH_RING_SIZE
      };

      extern LogRing gRing;
      extern CrashRing gCrashRing;
    }

    /**
     * @brief Initialize the logging system.
     *
     * Opens the log file and starts the background writer thread. Must be called
     * once on the main thread before any other thread calls Log(). Sends an
     * initial Info entry so the file handle and ring are warm before concurrent use begins.
     */
    void logInit();

    /**
     * @brief Shut down the logging system.
     *
     * Drains remaining ring entries, joins the writer thread, and closes the log
     * file. Call once at clean shutdown.
     *
     * @note Called from Utility Thread after its loop exits or from Main Thread at shutdown.
     */
    void logShutdown();

    /**
     * @brief Drain pending log entries to disk. Called by the Utility Thread.
     *
     * Drains up to LOG_RING_SIZE entries per call and returns. If more entries
     * arrive than fit in one call, they are handled on the next Utility Thread
     * iteration. Never blocks.
     *
     * @return True if any entries were written (useful for skipping fsync
     * when the ring was already empty).
     */
    bool logDrainOnce();

    /**
     * @brief Drain pending log entries to disk. Called by the Utility Thread.
     *
     * Drains the entire ring before returning, guaranteeing all entries are on disk when
     * this function returns.
     *
     * @return True if any entries were written (useful for skipping fsync
     * when the ring was already empty).
     */
    bool logDrainOnceDebug();

    /**
     * @brief Write the crash dump buffer to disk using only async-signal-safe calls.
     *
     * Safe to call from a signal handler (SIGSEGV, SIGABRT, SIGFPE). Uses write()
     * only (no malloc, no stdio, no C++ runtime). Appends a "=== CRASH DUMP ===" header
     * then the last LOG_CRASH_RING_SIZE entries in chronological order to the log file.
     *
     * @param _crashFd An already-open file descriptor to write to. Pass the log fd, or STDERR_FILENO as a fallback.
     * @note crashFd is unused on Windows
     */
    void logWriteCrashDump();
    void logWriteCrashDump(int _crashFd = 0);

    /**
     * @brief General platform-agnostic log with time
     * @param _level Type of log (debug, warning, error...)
     * @param _msg C String with what to print
     * @param _addToFile Whether or not to add this particular message to the session's log file
     */
    void Log(LogLevel _level, const char* _msg);

    /**
     * @defgroup spl_log Specialized logging functions
     * @brief These allow for printf style formatting and log type readability
     * @{
     */

    /** @brief Useful debug messages. */
    void DebugLog(const char* _format, ...);  

    /** @brief Noting program relevant information. */
    void InfoLog(const char* _format, ...);      

    /** @brief Warning about dubious but acceptable behavior. */
    void WarningLog(const char* _format, ...);    
    
    /** @brief Warning about dangerous behavior. */
    void CriticalLog(const char* _format, ...);   
    
    /** @brief Warning about program breaking behavior. */
    void ErrorLog(const char* _format, ...);      

    /** @} end of spl_log group */
  }
}
