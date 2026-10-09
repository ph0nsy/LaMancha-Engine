/**
 * @file pch.h
 * @brief Pre-compiled header file.
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 * 
 * @details 
 * Contains large, stable, and frequently used headers that rarely change:
 * - Standard Library Headers
 * - Global Macros & Type definitions
 * - Platform-Specific APIs (like windows.h or pthread.h)
 * - Large, commonly used Third-Party Frameworks: If an included library isn't used in a file, it bloats the compilation memory.
 */

#pragma once

#include "platform/platform.h"

#if defined(LAMANCHA_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(LAMANCHA_PLATFORM_LINUX)
#include <unistd.h>
#include <fcntl.h>
#endif

#if defined(LAMANCHA_PLATFORM_APPLE)
#endif

#include "log.h"

#include <cstdarg>
#include <cstdio> 
#include <cstring>
#include <cstdlib>
#include <utility>
#include <new> // Placement new requires <new> for the declaration of ::operator new(size_t, void*).

#include "deps/tinycthread.h"


 /** @defgroup compiler_hints Compiler Hints and Optimization Macros
 *  @brief Platform-specific compiler directives for performance
 *  @{
 */

/**  @brief Runtime bounds checking (0=off, 1=on) */
#define LAMANCHA_DEBUG 1

#if defined(__GNUC__) || defined(__clang__)
/** @brief Force function inlining (GCC/Clang) */
#define LAMANCHA_INLINE __attribute__((always_inline)) inline
/** @brief Prevent function inlining */
#define LAMANCHA_NOINLINE __attribute__((noinline))
/** @brief Branch prediction hint: condition is likely true */
#define LAMANCHA_LIKELY(x) __builtin_expect(!!(x), 1) // The !! double-negation coerces any integer to a strict 0 or 1
/** @brief Branch prediction hint: condition is unlikely true */
#define LAMANCHA_UNLIKELY(x) __builtin_expect(!!(x), 0)
/** @brief Compute byte offset of member in struct at compile time */
#define LAMANCHA_OFFSETOF(type, member) __builtin_offsetof(type, member)
/** @brief Align variable/struct to n-byte boundary */
#define LAMANCHA_ALIGN(n) __attribute__((aligned(n)))
/** @brief Software prefetching */
#define LAMANCHA_PREFETCH(ptr) __builtin_prefetch((ptr), 0, 1)

#elif defined(_MSC_VER)
#define LAMANCHA_INLINE __forceinline
#define LAMANCHA_NOINLINE __declspec(noinline)
#define LAMANCHA_LIKELY(x) (x)
#define LAMANCHA_UNLIKELY(x) (x)
#define LAMANCHA_OFFSETOF(type, member) ((size_t)&((type*)0)->member)
#define LAMANCHA_ALIGN(n) __declspec(align(n))
#include <xmmintrin.h>
#define LAMANCHA_PREFETCH(ptr) _mm_prefetch((const char*)(ptr), _MM_HINT_T1)

#else
#define LAMANCHA_INLINE inline
#define LAMANCHA_NOINLINE
#define LAMANCHA_LIKELY(x) (x)
#define LAMANCHA_UNLIKELY(x) (x)
#define LAMANCHA_OFFSETOF(type, member) __offsetof(type, member)
#define LAMANCHA_ALIGN(n) __attribute__((aligned(n)))
#define LAMANCHA_PREFETCH(ptr) ((void)0)  // no-op on unknown platforms

#endif

/** 
  * @defgroup simd_infrastructure SIMD Infrastructure
  * @brief Platform-abstracted SIMD types and low-level operations
  *
  * SIMD = Single Instruction, Multiple Data. A normal scalar ADD takes two
  * registers and produces one result. A SIMD ADD takes two 128-bit registers,
  * each holding FOUR 32-bit floats side by side, and adds all four pairs in
  * ONE clock cycle.
  *
  * A 4x4 matrix multiply needs 64 multiplications and 48 additions for a
  * total of 112 scalar operations. With SIMD broadcasting (see LaManchaMath.h),
  * this collapses to 16 SIMD operations. At 60 fps with thousands of
  * transforms per frame, that difference compounds enormously.
  *
  * Both x86 SSE (XMM registers) and ARM NEON (Q registers) use 128-bit
  * wide hardware registers. Four 32-bit floats fit exactly in 128 bits.
  * This is why graphics math is built around 4-component vectors and 4x4
  * matrices, they are shaped to fill exactly one SIMD register.
  *
  * A CPU 'register' is storage that lives inside the processor itself, not
  * in RAM, not in cache. Register operations take 1 - 4 clock cycles. A cache
  * miss can take 100+ cycles. SIMD effectiveness depends on keeping the data
  * in registers for as many operations as possible before writing back.
  *
  * SIMD load instructions come in two flavors:
  *   - @c movaps / @c vld1q (aligned):   REQUIRES 16-byte aligned address
  *   - @c movups / @c vld1q (unaligned): works on any address, ~1 cycle slower
  *
  * We use @c LAMANCHA_SIMD_ALIGN to guarantee alignment, allowing the compiler
  * to emit the faster aligned variant. An unaligned movaps causes a #GP
  * (general protection fault) on x86, a hard crash.
  *
  *  @{
  */

/**
  * @brief Detect SIMD mode at compile time
  *
  * We define engine-internal types: @c lm_f32x4, @c lm_u8x16, @c lm_u16x8, @c lm_i32x4
  * and @c lm_u32x4 that map to the correct hardware register type for the current platform.
  *
  * Three modes:
  *   1. ARM NEON (__ARM_NEON defined)
  *   2. x86 SSE2 (__SSE2__ defined)
  *   3. Scalar fallback (LAMANCHA_SIMD_NONE defined)
  *
  * @note SSE2 is baseline for all x86-64 CPUs (AMD64 specification, 2003)
  * @note NEON is baseline for all ARMv8/ARM64 CPUs (mandatory since 2011)
  */
#if defined(__ARM_NEON) || defined(__ARM_NEON__)

#include <arm_neon.h>

/** @brief 128-bit SIMD register type (4x f32) */
using lm_f32x4 = float32x4_t;
using lm_u8x16 = uint8x16_t;
using lm_u16x8 = uint16x8_t;
using lm_i32x4 = int32x4_t;
using lm_u32x4 = uint32x4_t;
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)

#include <emmintrin.h> // this header has generated errors before

using lm_f32x4 = __m128;
using lm_u8x16 = __m128i;
using lm_u16x8 = __m128i;
using lm_i32x4 = __m128i;
using lm_u32x4 = __m128i;

#else

/** @brief SIMD disabled - all operations fall back to scalar loops */
#define LAMANCHA_SIMD_NONE

/** @brief Dummy type when SIMD unavailable (never actually used) */
using lm_f32x4 = void*;
using lm_u8x16 = void*;
using lm_u16x8 = void*;
using lm_i32x4 = void*;
using lm_u32x4 = void*;
#endif

/**
 * @brief SIMD alignment requirement (16 bytes for 128-bit registers)
 *
 * Use this for any structure containing SIMD data (Vec<3>, Vec<4>, Mat, etc.)
 * to ensure proper alignment for vld1q_f32/_mm_load_ps instructions.
 */
#define LAMANCHA_SIMD_ALIGN LAMANCHA_ALIGN(16)

 /** @} */ // end of simd_infrastructure group

/** @brief Compile-time sizeof wrapper for consistency */
#define LAMANCHA_SIZEOF(x) sizeof(x)

/**
 * @brief Runtime sizeof via pointer arithmetic
 *
 * Computes the actual memory footprint of an instance, including vtable
 * pointers and padding. Useful for debugging struct layouts.
 *
 * Formula: (&x + 1) points one-past-the-end, subtracting &x gives size
 */
#define LAMANCHA_SIZEOF_INSTANCE(x) ((char*)(&x + 1) - (char*)&x)

/** @} end of compiler_hints group */

#if defined(_MSC_VER)
/** @brief Debugger breakpoint (Windows/MSVC: INT 3 instruction) */
#define LAMANCHA_TRAP() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
/** @brief Debugger breakpoint (GCC/Clang: illegal instruction trap) */
#define LAMANCHA_TRAP()  __builtin_trap()
#else
/** @brief Debugger breakpoint fallback (null pointer write) */
#define LAMANCHA_TRAP() *(volatile i32*)0 = 0
#endif

 /**
  * @brief Runtime assertion macro with automatic trap on failure.
  *
  * @details
  * If condition is false:
  * 1. Prints diagnostic via ErrorLog (file, line)
  * 2. Calls LAMANCHA_TRAP() to halt execution immediately
  *
  * The macro expands __FILE__ and __LINE__ predefined macros:
  * - __FILE__: source file path as string literal
  * - __LINE__: current line number as integer
  *
  * Example:
  * @code
  *     LAMANCHA_ASSERT(index < arraySize, "Index out of bounds");
  *     LAMANCHA_ASSERT(ptr != nullptr, "Pointer already initialized");
  * @endcode
  *
  * @param condition Expression to evaluate (must be true)
  * @note In release builds, this is wrapped in #if LAMANCHA_DEBUG
  */
#define LAMANCHA_ASSERT(condition, msg) if (!(condition)) { LaMancha::Logging::ErrorLog("Assertion failed: %s [%s:%d]", msg, __FILE__, __LINE__); LAMANCHA_TRAP(); }

/**
 * @defgroup primitive_types Primitive Type Aliases
 * @brief Fixed-width integer and floating-point types
 *
 * @note @c <stdint.h> / @c <cstdint> is part of the C standard and is available everywhere. 
 * However, it pulls in compiler / @a libc headers that can define macros we don't want.
 * @note If the need ever rises to run on a DSP or exotic embedded target where int is not 32 bits,
 *  replace these with @c <stdint.h> types and recompile.
 *  @{
 */

/**
 * @name Unsigned Integer Types
 * @{
 */
using u8 = unsigned char;         ///< 8-bit unsigned integer (0 to 255)
using u16 = unsigned short;       ///< 16-bit unsigned integer (0 to 65,535)
using u32 = unsigned int;         ///< 32-bit unsigned integer (0 to 4,294,967,295)
using u64 = unsigned long long;   ///< 64-bit unsigned integer (0 to 18,446,744,073,709,551,615)
/** @} */

/**
 * @name Signed Integer Types
 * @{
 */
using i8 = signed char;     ///< 8-bit signed integer (-128 to 127)
using i16 = short;          ///< 16-bit signed integer (-32,768 to 32,767)
using i32 = int;            ///< 32-bit signed integer (-2,147,483,648 to 2,147,483,647)
using i64 = long long;      ///< 64-bit signed integer
/** @} */

/**
 * @name Floating-Point Types
 * @{
 */
using f32 = float;   ///< 32-bit IEEE 754 single precision (~7 decimal digits, ±3.4e38)
using f64 = double;  ///< 64-bit IEEE 754 double precision (~15 decimal digits, ±1.7e308)
/** @} */

/** @} */ // end of primitive_types group

/** @defgroup size_types Size and Pointer Types
 *  @brief Platform-dependent size types matching pointer width
 *
 *  The detection macros:
 *    _WIN64        --> MSVC 64-bit Windows build
 *    __x86_64__    --> GCC/Clang x86-64
 *    __ppc64__     --> 64-bit PowerPC (rare, but used in older consoles/servers)
 *    __aarch64__   --> 64-bit ARM (Apple Silicon, ARMv8 Android, etc.)
 *  @{
 */

#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(__aarch64__)
 /**
  * @brief Unsigned size type (64-bit on 64-bit platforms, 32-bit on 32-bit)
  *
  * Use for array sizes, memory sizes, and loop indices. Equivalent to size_t
  * but without including <stddef.h>.
  */
using usize = unsigned long long;

/**
 * @brief Signed size type (64-bit on 64-bit platforms, 32-bit on 32-bit)
 *
 * Use for pointer arithmetic differences. Equivalent to ptrdiff_t.
 */
using isize = long long;

/**
 * @brief Unsigned integer same width as pointers
 *
 * Safe for storing pointers as integers (for example, alignment checks).
 */
using uptr = unsigned long long;
#else
 // 32-bit architectures
using usize = unsigned int;
using isize = int;
using uptr = unsigned int;
#endif

/** @} */ // end of size_types group

/**
 * @defgroup bitmasks Common Bitmask Constants
 * @brief Pre-computed bit patterns for common operations
 * @{
 */

constexpr u8  U8_MAX = 0xFF;                   ///< Maximum u8 value (255)
constexpr u16 U16_MAX = 0xFFFF;                 ///< Maximum u16 value (65,535)
constexpr u32 U32_MAX = 0xFFFFFFFF;             ///< Maximum u32 value (4,294,967,295)
constexpr u64 U64_MAX = 0xFFFFFFFFFFFFFFFFULL;  ///< Maximum u64 value

constexpr i8  I8_MAX = 0x7F;                   ///< Maximum i8 value (127)
constexpr i16 I16_MAX = 0x7FFF;                 ///< Maximum i16 value (32,767)
constexpr i32 I32_MAX = 0x7FFFFFFF;             ///< Maximum i32 value (2,147,483,647)
constexpr i64 I64_MAX = 0x7FFFFFFFFFFFFFFFLL;   ///< Maximum i64 value

constexpr i8  I8_MIN = -128;                   ///< Minimum i8 value
constexpr i16 I16_MIN = -32768;                 ///< Minimum i16 value
constexpr i32 I32_MIN = (-I32_MAX - 1);         ///< Minimum i32 value (-2,147,483,648)
constexpr i64 I64_MIN = (-I64_MAX - 1LL);       ///< Minimum i64 value

/** @} */ // end of bitmasks group


enum class ErrorCode : u8 {
  None = 0,
  AudioDeviceError,
  FileNotFound,
  GPUBudgetExceeded,
  InvalidArgument,
  InvalidFormat,
  InvalidHandle,
  IOTimeout,
  OutOfMemory,
  OutOfBounds,
  PlatformError,
  ScriptError,
  ShaderCompileFailed,
  Unknown = 255
};

/** @brief Helper function to get underlying base type. */
template <typename T> struct remove_reference { using type = T; };
template <typename T> struct remove_reference<T&> { using type = T; };
template <typename T> struct remove_reference<T&&> { using type = T; };

template <typename T> using remove_reference_t = typename remove_reference<T>::type;

/** @brief Helper function to discard types. */
template <typename... T> struct make_void { using type = void; };
template <typename... T> using void_t = typename make_void<T...>::type;

/** @brief Casts to an rvalue reference */
template <typename T>
constexpr remove_reference_t<T>&& move(T&& _arg) noexcept
{
  return static_cast<remove_reference_t<T>&&>(_arg);
}

/** @brief Casts to deduced type (maintains lvalue/rvalue category) */
template <typename T>
constexpr T&& forward(remove_reference_t<T>& _arg) noexcept
{
  return static_cast<T&&>(_arg);
}

template <typename T>
constexpr T&& forward(remove_reference_t<T>&& _arg) noexcept
{
  return static_cast<T&&>(_arg);
}

// In-Place Tag
struct InPlaceTag {};
inline constexpr InPlaceTag inPlace{};

/**
 * @brief Result type for error handling without exceptions
 *
 * Similar to Rust's Result<T, E>. Use for operations that can fail without
 * throwing exceptions (parsing, file I/O, resource loading, etc.)
 *
 * C++ exceptions add hidden code paths, increase binary size, and require stack unwinding
 * support. We disable them (-fno-exceptions on GCC/Clang, /EHs- on MSVC) because they are
 * unpredictable in performance-sensitive code and may be unavailable on some systems.
 *
 * @tparam T Value type on success
 * @note Check if result is true before accessing value (undefined behavior when false)
 *
 * Example:
 * @code
 * Result<i32> parseInt(const char* str) {
 *     i32 value;
 *     if (sscanf(str, "%d", &value) == 1) {
 *         return Ok(1);
 *     }
 *     return Error(ErrorCode::Unknown);
 * }
 *
 * auto result = parseInt("123");
 * if (result) {
 *     i32 n = result.value();  // Safe to access
 * } else {
 *     printf(result.message);
 * }
 * i32 n = result.valueOr(0); // Use default for error
 *
 * @endcode
 */
template <typename T>
struct Result {
  union {
    char m_empty;        ///< Dummy to keep union size non-zero
    T m_value;           ///< Valid only when ok result
  };
  ErrorCode m_error;     ///< Error identifier
  const char* message;   ///< Optional error string (null on error)

  // Deleted copy semantics to ensure safe ownership transfer
  Result(const Result&) = delete;
  Result& operator=(const Result&) = delete;

  /** @brief Error state constructor. */
  Result(ErrorCode _e, const char* _msg) noexcept : m_empty(0), m_error(_e), message(_msg) {}

  /** @brief Success state constructor (in-place). */
  template <typename... Args>
  explicit Result(InPlaceTag, Args&&... _args) noexcept : m_error(ErrorCode::None), message(nullptr)
  {
    #pragma push_macro("new") 
    #undef new // Protect against redefinition
    ::new (&m_value) T(forward<Args>(_args)...);
    #pragma pop_macro("new")
  }

  ~Result() { if (this) { m_value.~T(); } }

  /** @brief Move constructor. */
  Result(Result&& _other) noexcept : m_error(_other.m_error), message(_other.message)
  {
    if (m_error == ErrorCode::None)
    {
      #pragma push_macro("new") 
      #undef new // Protect against redefinition
      ::new (&m_value) T(move(_other.m_value));
      #pragma pop_macro("new")

      // Invalidate the moved-from object to prevent double-destruction
      // Cast to 'Unknown' error code is

      _other.m_error = ErrorCode::Unknown;
    }
    else { m_empty = 0; }
  }

  /** @brief Move assignment. */
  Result& operator=(Result&& _other) noexcept
  {
    if (this != &_other)
    {
      if (m_error == ErrorCode::None) { m_value.~T(); }

      m_error = _other.m_error;
      message = _other.message;

      if (m_error == ErrorCode::None)
      {

        #pragma push_macro("new") 
        #undef new // Protect against redefinition
        ::new (&m_value) T(move(_other.m_value));
        #pragma pop_macro("new") 

        _other.m_error = ErrorCode::Unknown;
      }
      else {
        m_empty = 0;
      }
    }
    return *this;
  }

  /** @brief Conversion function without implicit conversions. */
  explicit operator bool() const noexcept { return m_error == ErrorCode::None; }

  /** @brief Access value. */
  T& value() { return m_value; }
  const T& value() const { return m_value; }

  /**
   * @brief Get value or default if error
   * @param defaultValue Value to return on error
   * @return value if ok, defaultValue otherwise
   */
  constexpr T valueOr(const T& _defaultValue) const noexcept
  {
    return m_error == ErrorCode::None ? m_value : _defaultValue;
  }

};

/** @defgroup result Result helpers
 *  @brief These allow for return Ok(myTexture) or return Error<Texture>(ErrorCode::FileNotFound, path)
 *  @{
 */

 /**
  * @brief Successful Result<T>.
  * Uses forwarding to allow both @a lvalues and @a rvalues, avoiding unnecessary copies.
  */
template <typename T>
inline Result<remove_reference_t<T>> Ok(T&& _v)
{
  return Result<remove_reference_t<T>>(inPlace, forward<T>(_v));
}

/**
* @brief Failed Result<T>.
* Bypasses T's constructor entirely via the private @c Error constructor.
*/
template <typename T>
inline Result<T> Error(ErrorCode _e, const char* _msg = nullptr)
{
  return Result<T>(_e, _msg);;
}

/** @brief Used when a function can fail but has no meaningful Result<T> value. */
using ResultVoid = Result<void*>;

/** @brief Overload for ResultVoid > initializes the void* to nullptr cleanly.  */
inline ResultVoid Ok()
{
  return ResultVoid(inPlace, nullptr);
}

/** @brief Overload for ResultVoid with an error code and optional message. */
inline ResultVoid ErrorVoid(ErrorCode _e, const char* _msg = nullptr)
{
  return ResultVoid(_e, _msg);
}

/** @} */ // end of result group

/** @brief Handle a something that can return a value or not without it being an error */
template <typename T>
struct Optional
{
  union {
    char m_empty;   ///< Dummy to keep union size non-zero
    T m_value;      ///< Valid only when Ok result
  };
  bool m_hasValue = false;

  Optional() noexcept : m_empty(0) {}

  Optional(const T& _val) : m_hasValue(true) 
  {
    #pragma push_macro("new") 
    #undef new // Protect against redefinition
    ::new (&m_value) T(_val);
    #pragma pop_macro("new") 
  }

  Optional(T&& _val) : m_hasValue(true) 
  { 
    #pragma push_macro("new") 
    #undef new // Protect against redefinition
    ::new (&m_value) T(move(_val)); 
    #pragma pop_macro("new") 
  }

  ~Optional() { if (m_hasValue) { m_value.~T(); } }

  /** @brief In-place construction allows passing arguments directly to T's constructor */
  template <typename... Args>
  explicit Optional(InPlaceTag, Args&&... _args) : m_hasValue(true)
  {
    #pragma push_macro("new") 
    #undef new // Protect against redefinition
    ::new (&m_value) T(forward<Args>(_args)...);
    #pragma pop_macro("new") 
  }

  // Move semantics
  Optional(Optional&& _other) noexcept : m_hasValue(_other.m_hasValue)
  {
    if (m_hasValue) 
    {
      #pragma push_macro("new") 
      #undef new // Protect against redefinition 
      ::new (&m_value) T(move(_other.m_value));
      #pragma pop_macro("new")  
    }
  }

  Optional& operator=(Optional&& _other) noexcept {
    if (this != &_other)
    {
      if (m_hasValue) { m_value.~T(); }
      m_hasValue = _other.m_hasValue;
      if (m_hasValue) 
      {
        #pragma push_macro("new") 
        #undef new // Protect against redefinition
        ::new (&m_value) T(move(_other.m_value));
        #pragma pop_macro("new") 
      }
    }
    return *this;
  }

  static Optional Some(T& _val) { return Optional(move(_val)); }
  static Optional Some(T&& _val) { return Optional<T>(InPlaceTag{}, move(_val)); }
  static Optional None() { return Optional(); }

  /** @brief Access value. */
  T& value() { return m_value; }
  const T& value() const { return m_value; }

  /** @brief Access value with fallback. */
  const T& valueOr(const T& _defaultValue) const { return m_hasValue ? m_value : _defaultValue; }

  /** @brief Conversion function without implicit conversions. */
  explicit operator bool() const noexcept { return m_hasValue; }

};

/**
 * @defgroup memory Memory size literals
 * @brief Engine-defined literals let you write memory sizes as natural English
 *
 * @note 1_GB on a 32-bit system where usize = u32 will overflow. 1 GB = 2^30, which is
 * fine, but 4 GB = 2^32 would wrap to 0
 * @{
 */

constexpr usize operator"" _KB(unsigned long long x) { return x * 1024;  }
constexpr usize operator"" _MB(unsigned long long x) { return x * 1_KB * 1_KB; }
constexpr usize operator"" _GB(unsigned long long x) { return x * 1_MB * 1_KB; }

/** @} */ // end of memory group