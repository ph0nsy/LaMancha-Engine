#pragma once
#include "core/pch.h"

namespace LaMancha
{
  static bool isWhitespace(char c)
  {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
  }

  template <usize N> struct FixedString;

  /**
   * @brief Non-owning view into a character string
   *
   * Holds a pointer and length referring to an existing string buffer.
   * Does not manage or copy the underlying memory. Suitable for passing
   * string data without allocation or copying.
   */
  struct StringView 
  {
    usize length = 0;
    const char* data = nullptr;

    StringView() = default;

    /**
     * @brief Constructs a StringView from a null-terminated C-string
     *
     * Computes the length automatically via strLen. The StringView does not
     * copy or own the underlying data.
     *
     * @param _s Pointer to a null-terminated C-string
     */
    StringView(const char* _s) : data(_s), length(_s ? Utils::strLen(_s) : 0) {}

    /**
     * @brief Constructs a StringView from a raw pointer and explicit length
     *
     * @param _s Pointer to the start of the string data
     * @param _len Number of characters in the view
     */
    StringView(const char* _s, usize _len) : data(_s), length(_len) {}

    /**
     * @brief Constructs a StringView from any FixedString<N>
     *
     * Allows implicit conversion from a FixedString of any capacity without
     * requiring knowledge of the template parameter N at the call site.
     *
     * @tparam N Capacity of the source FixedString
     * @param _s The FixedString to view
     */
    template <usize N>
    StringView(const FixedString<N>& _s) : data(_s.data), length(_s.length) {}

    /**
     * @brief Assigns from another StringView
     * @param other The StringView to copy the pointer and length from
     * @return Reference to this StringView
     */
    StringView& operator=(const StringView& _other)
    {
      data = _other.data;
      length = _other.length;
      return *this;
    }

    /**
     * @brief Assigns from any FixedString<N>
     * @tparam N Capacity of the source FixedString
     * @param  other The FixedString to view
     * @return Reference to this StringView
     */
    template <usize N>
    StringView& operator=(const FixedString<N>& _other)
    {
      data = _other.data;
      length = _other.length;
      return *this;
    }

    /**
     * @brief Checks if another string is equal
     * @param  other The StringView to compare with
     * @return Whether they are equal or not
     */
    bool operator==(StringView _other) const
    {
      return length == _other.length && (memcmp(data, _other.data, length) == 0);
    }

    /**
     * @brief Checks whether the view refers to an empty or null string
     *
     * @return True if the length is zero or the data pointer is null
     */
    bool isEmpty() const { return length == 0 || data == nullptr; }

    /**
     * @brief Bounds-checked access to a single character
     *
     * Returns the character at index _idx wrapped in an Optional.
     * If the index is out of range, returns an empty Optional.
     *
     * @param _idx Zero-based index of the character to access
     * @return Optional<char> containing the character, or None if out of bounds
     */
    Optional<char> at(usize _idx) const
    {
      if (_idx >= length) { return Optional<char>::None(); }
      return Optional<char>::Some(static_cast<char>(data[_idx]));
    }

    /**
     * @brief Returns a sub-view into this view starting at offset
     *
     * If offset is beyond the end of the view, returns an empty StringView.
     * The returned view shares the same underlying memory.
     *
     * @param _offset Zero-based character offset to start the sub-view from
     * @return A StringView beginning at offset with adjusted length
     */
    StringView substr(usize _offset) const
    {
      if (_offset >= length) return StringView();
      return { data + _offset, length - _offset };
    }
    /**
     * @brief Returns a sub-view into this view starting at offset with a given length
     *
     * Clamps len so the sub-view does not extend beyond the end of this view.
     * The returned view shares the same underlying memory.
     *
     * @param _offset Zero-based character offset to start the sub-view from
     * @param _len Maximum number of characters to include in the sub-view
     * @return A StringView of up to len characters beginning at offset
     */
    StringView substr(usize _offset, usize _len) const
    {
      if (_offset >= length) return StringView();
      usize available = length - _offset;
      return { data + _offset, _len < available ? _len : available };
    }

    /**
     * @brief Writes the contents of this view into a FixedString buffer
     *
     * Copies up to N-1 characters from this view into @p out, overwriting
     * its existing contents. The result is always null-terminated.
     *
     * @tparam N Capacity of the target FixedString
     * @param out The FixedString to write into
     */
    template <usize N>
    void copyInto(FixedString<N>& out_) const
    {
      out_.set(length, data); // delegates to FixedString's bounds-safe set
    }

    /**
     * @brief Checks whether this view begins with the given prefix
     * @param prefix The StringView to test against the start of this view
     * @return True if this view starts with @p prefix
     */
    bool startsWith(StringView prefix) const
    {
      if (prefix.length > length) { return false; }
      return memcmp(data, prefix.data, prefix.length) == 0;
    }

    /**
     * @brief Checks whether this view ends with the given suffix
     * @param suffix The StringView to test against the end of this view
     * @return True if this view ends with @p suffix
     */
    bool endsWith(StringView suffix) const
    {
      if (suffix.length > length) { return false; }
      usize offset = length - suffix.length;
      return memcmp(data + offset, suffix.data, suffix.length) == 0;
    }

    /**
     * @brief Returns a view with leading whitespace removed
     * @return A StringView starting at the first non-whitespace character
     */
    StringView trimLeft() const
    {
      usize start = 0;
      while (start < length && isWhitespace(data[start])) { ++start; }
      return { data + start, length - start };
    }

    /**
     * @brief Returns a view with trailing whitespace removed
     * @return A StringView ending at the last non-whitespace character
     */
    StringView trimRight() const
    {
      usize end = length;
      while (end > 0 && isWhitespace(data[end - 1])) { --end; }
      return { data, end };
    }

    /**
     * @brief Returns a view with both leading and trailing whitespace removed
     * @return A trimmed StringView
     */
    StringView trim() const { return trimLeft().trimRight(); }

    /**
     * @brief Returns a lowercased copy of this view as a CString
     * @tparam N Capacity of the output CString, defaults to 256
     * @return A CString containing the lowercased characters
     */
    template <usize N = 256>
    void toLower(FixedString<N>& out_) const
    {
      usize count = (length < N - 1) ? length : N - 1;
      for (usize i = 0; i < count; ++i)
      {
        out_.data[i] = (data[i] >= 'A' && data[i] <= 'Z') ? data[i] + 32 : data[i];
      }
      out_.length = count;
      out_.data[count] = '\0';
    }

    /**
     * @brief Returns an uppercased copy of this view as a CString
     * @tparam N Capacity of the output CString, defaults to 256
     * @return A CString containing the uppercased characters
     */
    template <usize N = 256>
    FixedString<N> toUpper() const
    {
      usize count = (length < N - 1) ? length : N - 1;
      char result[count] = {};
      for (usize i = 0; i < count; ++i)
      {
        result[i] = (data[i] >= 'A' && data[i] <= 'Z') ? data[i] - 32 : data[i];
      }
      result.length = count;
      result.data[count] = '\0';
      return result;
    }

    /**
     * @brief Copies this view into a FixedString
     * @tparam N Capacity of the output FixedString, defaults to 256
     * @return A FixedString containing a copy of this view's characters
     */
    template <usize N = 256>
    FixedString<N> toString() const
    {
      FixedString<N> result;
      result.set(length, data);
      return result;
    }
  };

  /**
   * @brief Fixed-size string container with no heap allocation
   *
   * Stores a null-terminated character string in a statically-sized internal
   * buffer. Useful in performance-critical or embedded contexts where dynamic
   * memory allocation is undesirable.
   *
   * @tparam N The maximum capacity of the string, including the null terminator
   */
  template <usize N>
  struct FixedString {
    usize length;   ///< Current length of the string, excluding the null terminator
    char data[N];   ///< Internal character buffer

    FixedString() : length(0) { data[0] = '\0'; }

    /**
     * @brief Assigns from another FixedString of the same capacity
     * @param other The source FixedString
     * @return Reference to this FixedString
     */
    FixedString& operator=(const FixedString& other)
    {
      set(other.length, other.data);
      return *this;
    }

    /**
     * @brief Assigns from a FixedString of a different capacity
     * @tparam M Capacity of the source FixedString
     * @param  other The source FixedString
     * @return Reference to this FixedString
     */
    template <usize M>
    FixedString& operator=(const FixedString<M>& other)
    {
      set(other.length, other.data);
      return *this;
    }

    /**
     * @brief Assigns from a StringView
     * @param sv The StringView to copy from
     * @return Reference to this FixedString
     */
    FixedString& operator=(StringView sv)
    {
      set(sv.length, sv.data);
      return *this;
    }

    /**
     * @brief Assigns from a null-terminated C-string
     * @param str Pointer to the null-terminated source string
     * @return Reference to this FixedString
     */
    FixedString& operator=(const char* str)
    {
      set(str);
      return *this;
    }

    /**
     * @brief Sets the string contents by copying from a CString
     *
     * Copies characters from string into the internal buffer up to a maximum
     * of N-1 characters, then null-terminates the result.
     *
     * @param _str Pointer to the null-terminated source string
     */
    void set(usize _len, const char* _str)
    {
      length = 0;
      while (length < N - 1 && length < _len) {
        data[length] = _str[length];
        length++;
      }
      data[length] = '\0';
    }

    /**
     * @brief Sets the string contents from a StringView
     * @param sv The StringView to copy from
     */
    void set(StringView sv) { set(sv.length, sv.data); }

    /**
     * @brief Sets the string contents using printf-style formatted output
     *
     * Writes a formatted string into the internal buffer with the provided
     * variadic arguments. Output is clamped to N-1 characters and always
     * null-terminated. If formatting fails, the buffer is set to an empty string.
     *
     * @param _fmt printf-style format string
     * @param ... Variadic arguments matching the format specifiers in _fmt
     */
    void set(const char* _fmt, ...)
    {
      va_list args;
      va_start(args, _fmt);
      i32 result = vsnprintf(data, N, _fmt, args);
      va_end(args);

      length = 0;
      if (result < 0) {
        data[0] = '\0';
      }
      else {
        length = (static_cast<usize>(result) >= N) ? N - 1 : static_cast<usize>(result);
      }
      data[length] = '\0';
    }

    /**
     * @brief Appends a StringView to the current contents
     *
     * Copies up to StringView.length characters from the view into the buffer
     * starting at the current end of the string. Silently truncates if the
     * combined length would exceed N-1 characters.
     *
     * @param _view The StringView to append
     */
    void append(StringView _view)
    {
      usize i = 0;
      while (i < _view.length && length < N - 1) {
        data[length++] = _view.data[i++];
      }
      data[length] = '\0';
    }

    /**
     * @brief Appends a formatted string to the current contents
     *
     * Writes printf-style formatted output into the remaining buffer capacity
     * starting at the current end of the string. Silently truncates if the
     * result would exceed N-1 characters.
     *
     * @param _fmt printf-style format string
     * @param ... Variadic arguments matching the format specifiers in _fmt
     */
    void append(const char* _fmt, ...)
    {
      usize remaining = N - length;
      if (remaining <= 1) return;
      va_list args;
      va_start(args, _fmt);
      i32 result = vsnprintf(data + length, remaining, _fmt, args);
      va_end(args);
      if (result > 0) {
        length += (static_cast<usize>(result) >= remaining)
          ? remaining - 1
          : static_cast<usize>(result);
      }
      data[length] = '\0';
    }

    /**
     * @return Empty string and reset length
     */
    void clear() { data[0] = '\0'; length = 0; }

    /**
     * @return Number of characters that can still be appended without truncation
     */
    usize remaining() const { return N - 1 - length; }

    /**
     * @brief Returns whether the buffer is completely full
     * @return True if no more characters can be appended without truncation
     */
    bool isFull() const { return length >= N - 1; }

    /**
     * @brief Returns a pointer to the null-terminated internal character buffer
     * @return Pointer to the internal CString
     */
    const char* cstr() const { return data; }

    /**
     * @brief Checks whether this string begins with the given prefix
     * @param prefix The StringView to test against the start of this string
     * @return True if this string starts with prefix
     */
    bool startsWith(StringView prefix) const { return StringView(*this).startsWith(prefix); }

    /**
     * @brief Checks whether this string ends with the given suffix
     * @param suffix The StringView to test against the end of this string
     * @return True if this string ends with suffix
     */
    bool endsWith(StringView suffix) const { return StringView(*this).endsWith(suffix); }

    /**
     * @brief Removes leading whitespace in place
     */
    void trimLeft()
    {
      usize start = 0;
      while (start < length && isWhitespace(data[start])) { ++start; }
      if (start > 0)
      {
        length -= start;
        memmove(data, data + start, length);
        data[length] = '\0';
      }
    }

    /**
     * @brief Removes trailing whitespace in place
     */
    void trimRight()
    {
      while (length > 0 && isWhitespace(data[length - 1])) { --length; }
      data[length] = '\0';
    }

    /**
     * @brief Removes both leading and trailing whitespace in place
     */
    void trim() { trimLeft(); trimRight(); }

    /**
     * @brief Converts all uppercase characters to lowercase in place
     */
    void toLower()
    {
      for (usize i = 0; i < length; ++i)
      {
        if (data[i] >= 'A' && data[i] <= 'Z') { data[i] += 32; }
      }
    }

    /**
     * @brief Converts all lowercase characters to uppercase in place
     */
    void toUpper()
    {
      for (usize i = 0; i < length; ++i)
      {
        if (data[i] >= 'a' && data[i] <= 'z') { data[i] -= 32; }
      }
    }
  };

  /**
   * @brief Concatenates two StringViews into a destination FixedString
   *
   * Writes A followed by B into out, overwriting any existing content.
   * Silently truncates if the combined length exceeds the buffer capacity.
   *
   * @tparam N Capacity of the destination FixedString
   * @param out The FixedString to write the result into
   * @param a The first StringView
   * @param b The second StringView
   */
  template <usize N>
  void concat(FixedString<N>& out_, StringView a, StringView b) {
    out_.set(a.length, a.data);   // safe bounded copy
    out_.append(b);
  }

  /**
   * @brief Concatenates three StringViews into a destination FixedString
   *
   * Writes A, B, and C sequentially into out_, overwriting any
   * existing content. Silently truncates if the combined length exceeds the
   * buffer capacity.
   *
   * @tparam N Capacity of the destination FixedString
   * @param out The FixedString to write the result into
   * @param a The first StringView
   * @param b The second StringView
   * @param c The third StringView
   */
  template <usize N>
  void concat(FixedString<N>& out_, StringView a, StringView b, StringView c) {
    out_.set(a.length, a.data);
    out_.append(b);
    out_.append(c);
  }

  using String32 = FixedString<32>;     ///< Fixed string with a 32-character capacity
  using String64 = FixedString<64>;     ///< Fixed string with a 64-character capacity
  using String128 = FixedString<128>;   ///< Fixed string with a 128-character capacity
  using String256 = FixedString<256>;   ///< Fixed string with a 256-character capacity
  using String512 = FixedString<512>;   ///< Fixed string with a 512-character capacity
  using String1024 = FixedString<1024>; ///< Fixed string with a 1024-character capacity
}