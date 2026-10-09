#include "core/pch.h"
#include "iniReader.h"
#include "deps/ini.h"

namespace LaMancha {
  namespace Utils {

    bool Utils::strEqI(const char* _a, const char* _b)
    {
      while (*_a && *_b)
      {
        char ca = (*_a >= 'A' && *_a <= 'Z') ? (*_a + 32) : *_a;
        char cb = (*_b >= 'A' && *_b <= 'Z') ? (*_b + 32) : *_b;
        if (ca != cb) { return false; }
        ++_a;
        ++_b;
      }
      return *_a == '\0' && *_b == '\0';
    }

    namespace { // Anonymous namespace for the sake of encapsulation and internal linkage
      /** @brief True if c is ASCII whitespace. Exists in FixedString */
      LAMANCHA_INLINE bool isSpace(char _c) { return _c == ' ' || _c == '\t' || _c == '\r' || _c == '\n'; }

      /** @brief Exact strcmp (section and key names are case-sensitive). */
      LAMANCHA_INLINE bool strEq(const char* _a, const char* _b)
      {
        while (*_a && *_b)
        {
          if (*_a != *_b) { return false; }
          ++_a;
          ++_b;
        }
        return *_a == '\0' && *_b == '\0';
      }

      /**
       * @brief Parse a null-terminated string as i32 via manual digit scan.
       *        Handles optional leading sign. No locale, no libc atoi.
       * @param str    Input string.
       * @param out    Written on success.
       * @return true if the entire string was a valid integer.
       */
      bool parseInt(const char* _str, i32& out_)
      {
        if (!_str || *_str == '\0') { return false; }

        const char* p = _str;
        bool negative = false;

        if (*p == '-') { negative = true; ++p; }
        else if (*p == '+') { ++p; }
        if (*p == '\0') { return false; }  // sign with no digits

        i32 result = 0;
        while (*p)
        {
          if (*p < '0' || *p > '9') { return false; }
          result = result * 10 + (*p - '0');
          ++p;
        }
        out_ = negative ? -result : result;
        return true;
      }

      /**
       * @brief Parse a null-terminated string as f32.
       *        Delegates to sscanf (acceptable at init time, not in hot paths).
       */
      bool parseFloat(const char* _str, f32& out_)
      {
        if (!_str || *_str == '\0') { return false; }
        return sscanf(_str, "%f", &out_) == 1;
      }

      /**
       * @brief Parse bool from string.
       * Accepts: true/false (case-insensitive), 1/0, yes/no (case-insensitive).
       */
      bool parseBool(const char* _str, bool& out_)
      {
        if (!_str || *_str == '\0') { return false; }
        if (strEqI(_str, "true") || strEqI(_str, "yes") || strEq(_str, "1"))
        {
          out_ = true;  return true;
        }
        if (strEqI(_str, "false") || strEqI(_str, "no") || strEq(_str, "0"))
        {
          out_ = false; return true;
        }
        return false;
      }
    } // End of anonymous namespace

    const char* IniStringPool::push(const char* _str)
    {
      if (LAMANCHA_UNLIKELY(!_str)) { return ""; }

      usize len = strLen(_str);   // does not count '\0'
      u32   need = static_cast<u32>(len) + 1u;

#if LAMANCHA_DEBUG
      LAMANCHA_ASSERT(fill + need <= LM_INI_POOL_SIZE, "IniStringPool::push > Memory needed avobe the Pool size.");
#else
      if (LAMANCHA_UNLIKELY(fill + need > LM_INI_POOL_SIZE))
      {
        // In release: return the last valid byte as an empty string.
        // The caller will get "" rather than a crash.
        return buf + fill;  // buf[fill] == '\0' because pool is zero-initialised
      }
#endif

      char* dest = buf + fill;
      memcpy(dest, _str, need);    // copies str + '\0'
      fill += need;
      return dest;
    }

    const IniEntry* IniSection::find(const char* _key) const
    {
      for (u32 i = 0; i < count; ++i)
      {
        if (entries[i].key && strEq(entries[i].key, _key))
        {
          return &entries[i];
        }
      }
      return nullptr;
    }

    IniSection* IniReader::findOrAddSection(const char* _sectionName)
    {
      // Search for existing section
      for (u32 i = 0; i < m_sectionCount; ++i)
      {
        if (m_sections[i].name && strEq(m_sections[i].name, _sectionName))
        {
          return &m_sections[i];
        }
      }

      // Add new
#if LAMANCHA_DEBUG
      LAMANCHA_ASSERT(m_sectionCount < LM_INI_MAX_SECTIONS, "IniReader::findOrAddSection > Tried to add section when full.");
#else
      if (LAMANCHA_UNLIKELY(m_sectionCount >= LM_INI_MAX_SECTIONS)) { return nullptr; }
#endif

      IniSection& sec = m_sections[m_sectionCount++];
      sec.name = m_pool.push(_sectionName);
      sec.count = 0;
      return &sec;
    }

    // Static inih callback
    i32 IniReader::iniHandler(void* _user, const char* _section, const char* _name, const char* _value)
    {
      IniReader* self = static_cast<IniReader*>(_user);

      IniSection* sec = self->findOrAddSection(_section ? _section : "");
      if (LAMANCHA_UNLIKELY(!sec)) { return 0; } // inih: returning 0 aborts parse
      if (LAMANCHA_UNLIKELY(!sec->hasRoom())) { return 0; }

      IniEntry& entry = sec->entries[sec->count++];
      entry.key = self->m_pool.push(_name ? _name : "");
      entry.value = self->m_pool.push(_value ? _value : "");

      return 1; // inih: 1 = continue
    }

    ResultVoid IniReader::load(const char* _filename)
    {
      unload();
      i32 r = ini_parse(_filename, iniHandler, this);
      if (r == -1)
      {
        return ErrorVoid(ErrorCode::FileNotFound, _filename);
      }
      if (r > 0) // r == first line number that failed
      {
        return ErrorVoid(ErrorCode::InvalidFormat, _filename);
      }
      return Ok();
    }

    void IniReader::unload()
    {
      m_pool.reset();
      m_sectionCount = 0;
      // Zero the section array so stale pointers don't linger
      for (u32 i = 0; i < LM_INI_MAX_SECTIONS; ++i)
      {
        m_sections[i] = IniSection{};
      }
    }

    const IniSection* IniReader::section(const char* _sectionName) const
    {
      for (u32 i = 0; i < m_sectionCount; ++i)
      {
        if (m_sections[i].name && strEq(m_sections[i].name, _sectionName))
        {
          return &m_sections[i];
        }
      }
      return nullptr;
    }

    const char* IniReader::getRaw(const char* _sectionName, const char* _key) const
    {
      const IniSection* sec = section(_sectionName);
      if (!sec) { return nullptr; }
      const IniEntry* e = sec->find(_key);
      return e ? e->value : nullptr;
    }

    const char* IniReader::getRaw(const IniSection& _sec, const char* _key) const
    {
      const IniEntry* e = _sec.find(_key);
      return e ? e->value : nullptr;
    }

    const char* IniReader::getString(const char* _sec, const char* _key, const char* fallback) const
    {
      const char* v = getRaw(_sec, _key);
      return (v && *v) ? v : fallback;
    }

    i32 IniReader::getInt(const char* _sec, const char* _key, i32 fallback) const
    {
      const char* v = getRaw(_sec, _key);
      if (!v) { return fallback; }
      i32 out = fallback;
      return parseInt(v, out) ? out : fallback;
    }

    f32 IniReader::getFloat(const char* _sec, const char* _key, f32 fallback) const
    {
      const char* v = getRaw(_sec, _key);
      if (!v) { return fallback; }
      f32 out = fallback;
      return parseFloat(v, out) ? out : fallback;
    }

    bool IniReader::getBool(const char* _sec, const char* _key, bool fallback) const
    {
      const char* v = getRaw(_sec, _key);
      if (!v) { return fallback; }
      bool out = fallback;
      return parseBool(v, out) ? out : fallback;
    }

    const char* IniReader::getString(const IniSection& _sec, const char* _key, const char* fallback) const
    {
      const char* v = getRaw(_sec, _key);
      return (v && *v) ? v : fallback;
    }

    i32 IniReader::getInt(const IniSection& _sec, const char* _key, i32 fallback) const
    {
      const char* v = getRaw(_sec, _key);
      if (!v) { return fallback; }
      i32 out = fallback;
      return parseInt(v, out) ? out : fallback;
    }

    f32 IniReader::getFloat(const IniSection& _sec, const char* _key, f32 fallback) const
    {
      const char* v = getRaw(_sec, _key);
      if (!v) { return fallback; }
      f32 out = fallback;
      return parseFloat(v, out) ? out : fallback;
    }

    bool IniReader::getBool(const IniSection& _sec, const char* _key, bool fallback) const
    {
      const char* v = getRaw(_sec, _key);
      if (!v) { return fallback; }
      bool out = fallback;
      return parseBool(v, out) ? out : fallback;
    }

    u32 IniReader::splitList(const char* _value, const char* scratchOut_[], u32 _maxOut) const
    {
      if (!_value || *_value == '\0' || _maxOut == 0) { return 0; }

      u32 count = 0;
      const char* p = _value;

      while (*p && count < _maxOut)
      {
        // Skip leading whitespace
        while (*p && isSpace(*p)) { ++p; }
        if (*p == '\0') { break; }

        // Find comma or end
        const char* tokenStart = p;
        while (*p && *p != ',') { ++p; }
        const char* tokenEnd = p;

        // Trim trailing whitespace
        while (tokenEnd > tokenStart && isSpace(*(tokenEnd - 1))) { --tokenEnd; }

        // Copy into scratch buffer
        usize len = static_cast<usize>(tokenEnd - tokenStart);
        if (len >= 128u) { len = 127u; }  // hard cap per scratch slot
        char* dest = m_listScratch[count];
        memcpy(dest, tokenStart, len);
        dest[len] = '\0';
        scratchOut_[count++] = dest;

        if (*p == ',') { ++p; } // step past comma
      }
      return count;
    }

    u32 IniReader::getStringList(const char* _sec, const char* _key, const char* out_[], u32 _maxOut) const
    {
      const char* v = getRaw(_sec, _key);
      if (!v) { return 0u; }
      return splitList(v, out_, _maxOut < LM_INI_MAX_LIST ? _maxOut : LM_INI_MAX_LIST);
    }

    u32 IniReader::getIntList(const char* _sec, const char* _key, i32 out_[], u32 _maxOut) const
    {
      const char* tokens[LM_INI_MAX_LIST];
      u32 n = getStringList(_sec, _key, tokens, _maxOut < LM_INI_MAX_LIST ? _maxOut : LM_INI_MAX_LIST);
      u32 parsed = 0;
      for (u32 i = 0; i < n; ++i)
      {
        i32 v = 0;
        if (parseInt(tokens[i], v)) { out_[parsed++] = v; }
      }
      return parsed;
    }

    u32 IniReader::getFloatList(const char* _sec, const char* _key, f32 out_[], u32 _maxOut) const
    {
      const char* tokens[LM_INI_MAX_LIST];
      u32 n = getStringList(_sec, _key, tokens, _maxOut < LM_INI_MAX_LIST ? _maxOut : LM_INI_MAX_LIST);
      u32 parsed = 0;
      for (u32 i = 0; i < n; ++i)
      {
        f32 v = 0.f;
        if (parseFloat(tokens[i], v)) { out_[parsed++] = v; }
      }
      return parsed;
    }

#if LAMANCHA_DEBUG
    void IniReader::debugDump() const
    {
      Logging::InfoLog("[IniReader] %u section(s), pool %u/%u bytes\n",
        m_sectionCount, m_pool.fill, LM_INI_POOL_SIZE);
      for (u32 i = 0; i < m_sectionCount; ++i) {
        const IniSection& sec = m_sections[i];
        Logging::InfoLog("  [%s] (%u entries)\n", sec.name, sec.count);
        for (u32 j = 0; j < sec.count; ++j) {
          Logging::InfoLog("    %s = %s\n",
            sec.entries[j].key, sec.entries[j].value);
        }
      }
    }

    void IniReader::debugPoolUsage() const
    {
      Logging::InfoLog("[IniReader] Pool usage: %u / %u bytes (%.1f%%)\n",
        m_pool.fill, LM_INI_POOL_SIZE,
        100.f * static_cast<f32>(m_pool.fill) / static_cast<f32>(LM_INI_POOL_SIZE));
    }
#endif
  }
}