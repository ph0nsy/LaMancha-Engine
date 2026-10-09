/**
 * @file iniReader.h
 * @brief INI file parser for LaMancha Engine configuration loading
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 *
 * Parses @c .ini files at initialization time using @a inih (ini.h/ini.c).
 * Stores parsed data in a two-level section key-value structure backed
 * by a fixed string pool. The caller populates config structs via
 * typed getters, then the IniReader goes out of scope, no persistent
 * allocation survives past the loading function.
 *
 * File is opened, parsed, and closed inside load(). Structs are
 * populated immediately after. The IniReader is then destroyed.
 *
 * Replacing the allocator later:
 * IniStringPool owns one char buf[LM_INI_POOL_SIZE]. When the arena
 * exists, replace buf + fill with a pointer into the arena and a bump
 * counter. The rest of this file is untouched.
 *
 * @note IniStringPool's one char buf[LM_INI_POOL_SIZE] and fill will be replaced
 * with a pointer into the arena and a bump counter once the Arena Allocator is implemented.
 */

#pragma once

#include "core/pch.h"

#define LM_INI_POOL_SIZE (8 * 1024) /// Total bytes for all string data
#define LM_INI_MAX_SECTIONS 32u     /// Max distinct [section] headers
#define LM_INI_MAX_ENTRIES 24u      /// Max key=value pairs per section
#define LM_INI_MAX_LIST 8u          /// Max elements in a comma-separated list value

namespace LaMancha {
  namespace Utils {
    /**
     * @brief Case-insensitive strcmp for short strings (key comparison).
     * Only lowercases ASCII letters; non-alpha chars compared directly.
     */
    bool strEqI(const char* _a, const char* _b);

    /**
     * @brief Bump-allocator string storage for IniReader.
     *
     * All string data (section names, keys, values) is copied here during
     * parsing. Pointers into this buffer are stable for the lifetime of the
     * pool. When the arena allocator exists, replace buf/fill with an arena
     * pointer and offset, the interface stays identical.
     *
     * @note fill is a debug-visible watermark; assert on overflow in debug builds.
     */
    struct IniStringPool {
      char buf[LM_INI_POOL_SIZE];
      u32 fill = 0;

      /**
       * @brief Copy a null-terminated string into the pool.
       * @param str Source string (inih-owned, valid only during callback).
       * @return Pointer into buf that remains valid until the pool is destroyed.
       *         Returns nullptr on overflow (debug: asserts before reaching this).
       */
      const char* push(const char* _str);

      /**
       * @brief Reset the pool (invalidates all previously returned pointers).
       */
      LAMANCHA_INLINE void reset() { fill = 0; }

      /**
       * @brief Remaining free bytes (useful for debug sizing).
       */
      LAMANCHA_INLINE u32 remaining() const { return LM_INI_POOL_SIZE - fill; }
    };

    /**
     * @brief One key = value pair inside a section (part of two-level storage).
     *
     * Both pointers point into an IniStringPool; they are never owning.
     */
    struct IniEntry 
    {
      const char* key = nullptr;
      const char* value = nullptr;
    };

    /**
     * @brief One [section] block and its key=value pairs (part of two-level storage).
     *
     * name may include dots (like "[audio.windows]"); the IniReader treats the
     * full string as an opaque identifier. Subsection semantics live in the
     * caller's iteration logic (see IniReader::iterateSections).
     */
    struct IniSection {
      const char* name = nullptr;
      IniEntry entries[LM_INI_MAX_ENTRIES] = {};
      u32 count = 0;

      /**
       * @brief Find a key inside this section.
       * @return Pointer to IniEntry, or nullptr if not found.
       */
      const IniEntry* find(const char* _key) const;

      /** @brief True if another entry can be appended. */
      LAMANCHA_INLINE bool hasRoom() const { return count < LM_INI_MAX_ENTRIES; }
    };

    /**
     * @brief Temporary INI parser. Construct, populate structs, destroy.
     *
     * @code
     *   GraphicsConfig gfx{};
     *   AudioConfig    aud{};
     *   {
     *       IniReader r;
     *       if (r.load("game.ini")) {
     *           gfx.width  = r.getInt("graphics", "default_resolution_width", 1920);
     *           gfx.height = r.getInt("graphics", "default_resolution_height", 1080);
     *           // ... etc.
     *
     *           // Subsection iteration for audio.* array:
     *           r.iterateSections("audio.", [&](const IniSection& sec) {
     *               OsAudioConfig cfg{};
     *               cfg.frequency = r.getInt(sec, "frequency", 44100);
     *               // push cfg into your array
     *           });
     *       }
     *   } // r destroyed here, all string memory freed
     * @endcode
     */
    class IniReader
    {
      IniStringPool m_pool = {};
      IniSection m_sections[LM_INI_MAX_SECTIONS] = {};
      u32 m_sectionCount = 0;
      
      /** @brief Scratch buffer used by splitList to store trimmed token copies. */
      mutable char m_listScratch[LM_INI_MAX_LIST][128] = {}; // Size is one full pool entry per list slot

      /** @brief inih callback, static because inih is a C library
       * @param _user Our context.
       * @param _sec Section name.
       * @param _name Variable name.
       * @param _val Variable content.
       */
      static i32 iniHandler(void* _user, const char* _sec, const char* _name, const char* _val);

      /**
       * @brief Find or create a section slot for sectionName.
       *
       * @param _sectionName Section name.
       * @return Pointer to the section, or nullptr if LM_INI_MAX_SECTIONS reached.
       */
      IniSection* findOrAddSection(const char* _sectionName);

      /**
       * @brief Internal list splitter. Writes token divided C-strings into
       * scratchOut (caller-supplied, size maxOut).
       *
       * @param _valueToken values.
       * @param scratchOut_ C-strings.
       * @param _maxOut Capacity of scratchOut_[].
       * @return Token count
       * @note Shared by getStringList / getIntList / getFloatList
       */
      u32 splitList(const char* _value, const char* scratchOut_[], u32 _maxOut) const;

    public:
      IniReader() = default;
      ~IniReader() = default;

      IniReader(const IniReader&) = delete; // Non-copyable, the pool owns its buffer
      IniReader& operator=(const IniReader&) = delete; // Non-copyable, the pool owns its buffer

      /**
       * @brief Open, parse, and close _filename.
       *
       * inih calls the internal handler once per key=value pair. The file is
       * closed by inih before this function returns. On success the sections
       * array is populated and ready to query.
       *
       * @param _filename Path to the .ini file.
       * @return Ok() on success, Error(FileNotFound / InvalidFormat) otherwise.
       */
      ResultVoid load(const char* _filename);

      /** @brief Discard all parsed data (pool reset, section count zeroed). */
      void unload();

      /**
       * @brief Find a section by exact name.
       *
       * @param _sectionName Section name.
       * @return Pointer to IniSection, or nullptr if not found.
       */
      const IniSection* section(const char* _sectionName) const;

      /**
       * @brief Raw string value for (section, key).
       *
       * @param _sectionName Section name.
       * @param _key Key name.
       * @return The value string, or nullptr if section/key not found.
       */
      const char* getRaw(const char* _sectionName, const char* _key) const;

      /**
       * @brief Raw string value using a pre-fetched section (avoids re-scan).
       *
       * @param _sec Section struct.
       * @param _key Key name.
       * @return The value string, or nullptr if key not found.
       */
      const char* getRaw(const IniSection& _sec, const char* _key) const;

      /**
       * @brief Read a key as a C-string.
       *
       * @param _sec Section name.
       * @param _key Key name.
       * @param fallback Returned when the key is absent or the section missing.
       */
      const char* getString(const char* _sec, const char* _key, const char* fallback = "") const;

      /**
       * @brief Read a key as i32.
       *
       * @param _sec Section name.
       * @param _key Key name.
       * @param fallback Returned on parse failure or missing key.
       */
      i32  getInt(const char* _sec, const char* _key, i32 fallback = 0) const;

      /**
       * @brief Read a key as f32.
       *
       * @param _sec Section name.
       * @param _key Key name.
       * @param fallback Returned on parse failure or missing key.
       */
      f32  getFloat(const char* _sec, const char* _key, f32 fallback = 0.f) const;

      /**
       * @brief Read a key as bool.
       *
       * @param _sec Section name.
       * @param _key Key name.
       * Accepts: "true"/"false" (case-insensitive), "1"/"0", "yes"/"no".
       * @param fallback Returned on parse failure or missing key.
       */
      bool getBool(const char* _sec, const char* _key, bool fallback = false) const;

      const char* getString(const IniSection& _sec, const char* _key, const char* fallback = "") const;
      i32 getInt(const IniSection& _sec, const char* _key, i32 fallback = 0) const;
      f32 getFloat(const IniSection& _sec, const char* _key, f32 fallback = 0.f) const;
      bool getBool(const IniSection& _sec, const char* _key, bool fallback = false) const;

      /**
       * @brief Split a comma-separated value into an array of C-strings.
       *
       * Tokens are trimmed of leading/trailing whitespace. Results point into
       * a caller-supplied buffer to avoid any internal allocation.
       *
       * @param _sec Section name.
       * @param _key Key name.
       * @param out_ Caller-supplied array to receive token pointers.
       * Tokens point into a small internal scratch buffer on the IniReader;
       * copy them if you need them to outlive the next getStringList call.
       * @param _maxOut Capacity of out_[].
       * @return Number of tokens written to out (0 if key not found).
       */
      u32 getStringList(const char* _sec, const char* _key, const char* out_[], u32 _maxOut) const;

      /**
       * @brief Split a comma-separated value into an array of i32.
       *
       * @param _sec Section name.
       * @param _key Key name.
       * @param out_ Caller-supplied array.
       * @param _maxOut Capacity of out_[].
       * @return Number of values written (0 if key not found or parse fails).
       */
      u32 getIntList(const char* _sec, const char* _key, i32 out_[], u32 _maxOut) const;

      /** @brief Split a comma-separated value into an array of f32.
       *
       * @param _sec Section name.
       * @param _key Key name.
       * @param out_ Caller-supplied array.
       * @param _maxOut Capacity of out_[].
       * @return Number of values written (0 if key not found or parse fails).
       */
      u32 getFloatList(const char* _sec, const char* _key, f32 out_[], u32 _maxOut) const;

      /**
       * @brief Iterate all sections whose name starts with prefix.
       *
       * Used to build arrays from dot-subsections.
       *
       * @tparam Fn Any callable matching void(const IniSection&).
       * @param _prefix Prefix to match (for example, "audio."). Use "" to visit all.
       * @param _callback Called once per matching section. Signature: void(const IniSection&)
       * @return Number of sections visited.
       *
       * @note For example: iterateSections("audio.", callback) visits [audio.windows],
       * [audio.linux], [audio.r36s] in file order.
       */
      template<typename Fn>
      u32 iterateSections(const char* _prefix, Fn&& _callback) const
      {
        u32 visited = 0;
        const usize prefixLen = strLen(_prefix);

        for (u32 i = 0; i < m_sectionCount; ++i) {
          const IniSection& sec = m_sections[i];
          if (sec.name == nullptr) { continue; }

          // Empty prefix = visit all
          bool matches = (prefixLen == 0);
          if (!matches)
          {
            // Manual strncmp
            const char* n = sec.name;
            const char* p = _prefix;
            usize j = 0;
            for (; j < prefixLen; ++j) {
              if (n[j] == '\0' || n[j] != p[j]) { break; }
            }
            matches = (j == prefixLen);
          }

          if (matches)
          {
            _callback(sec);
            ++visited;
          }
        }
        return visited;
      }

#if LAMANCHA_DEBUG
      /** @brief Print all parsed sections and keys to the log (debug only). */
      void debugDump() const;

      /** @brief Log pool usage as a fraction of capacity. */
      void debugPoolUsage() const;
#endif
    };
  };
};