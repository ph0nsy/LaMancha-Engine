/*
 *   Foundational type definitions for LaMancha Engine
 *   Copyright (C) 2025 Alonso Moreno <ph0nsy>
 *
 *
 */

#pragma once #include < cstdio>  // For fprintf (temporary)

namespace LaMancha {

#define MASK8 0xFF
#define MASK16 0xFFFF
#define MASK32 0xFFFFFFFF
#define PI 3.14159265358 f

//  PLATFORM DETECTION
// ====================

#if defined(_WIN32) || defined(_WIN64)
#define LAMANCHA_PLATFORM_WINDOWS
#elif defined(__linux__)
#define LAMANCHA_PLATFORM_LINUX
#if defined(__arm__) || defined(__aarch64__)
#define LAMANCHA_PLATFORM_ARM
#ifdef __ARM_ARCH_8A
#define LAMANCHA_PLATFORM_R36S  // Detect Rockchip RK3326
#endif
#endif
#elif defined(__APPLE__)
#define LAMANCHA_PLATFORM_MACOS  // Maybe somewhere down the line
#endif

//  COMPILER HINTS
// ================

#if defined(__GNUC__) || defined(__clang__)
#define LAMANCHA_INLINE __attribute__((always_inline)) inline
#define LAMANCHA_NOINLINE __attribute__((noinline))
#define LAMANCHA_LIKELY(x) __builtin_expect(!!(x), 1)
#define LAMANCHA_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define LAMANCHA_OFFSETOF(type, member) __builtin_offsetof(type, member)
#elif defined(_MSC_VER)
#define LAMANCHA_INLINE __forceinline
#define LAMANCHA_NOINLINE __declspec(noinline)
#define LAMANCHA_LIKELY(x) (x)
#define LAMANCHA_UNLIKELY(x) (x)
#define LAMANCHA_OFFSETOF(type, member) __builtin_offsetof(type, member)
#else
#define LAMANCHA_INLINE inline
#define LAMANCHA_NOINLINE
#define LAMANCHA_LIKELY(x) (x)
#define LAMANCHA_UNLIKELY(x) (x)
#define LAMANCHA_OFFSETOF(type, member) __offsetof(type, member)
#endif

#define LAMANCHA_SIZEOF(x) ((char*)(&x + 1) - (char*)&x)

//  LOGGING MACROS (temporary)
// ============================

#ifdef LAMANCHA_DEBUG
#define LM_LOG(fmt, ...) fprintf(stdout, "[INFO] "
fmt "\n", # #__VA_ARGS__)
#define LM_WARN(fmt, ...) fprintf(stderr, "[WARN] "
fmt "\n", # #__VA_ARGS__)
#define LM_ERR(fmt, ...) fprintf(stderr, "[ERR]  "
fmt "\n", # #__VA_ARGS__)
#else
#define LM_LOG(fmt, ...)
#define LM_WARN(fmt, ...)
#define LM_ERR(fmt, ...)
#endif

//  ABORT / TRAP
// ==============

#if defined(_MSC_VER)
#define LAMANCHA_TRAP() __debugbreak()  // Windows/MSVC: Breakpoint (Int 3)
#elif defined(__GNUC__) || defined(__clang__)
#define LAMANCHA_TRAP() \
  __builtin_trap()  // Linux/R36S (GCC/Clang): illegal instruction (ud2
// on x86, similar on ARM)
#else
#define LAMANCHA_TRAP() *(volatile i32*)0 = 0  // Fallback
#endif

//  ASSERTIONS
// ============

#ifdef LAMANCHA_DEBUG
#define LAMANCHA_ASSERT(condition, message) if (!(condition)) {
\
fprintf(stderr, "Assertion failed: %s\n  File: %s\n  Line: %d\n", \
message, __FILE__, __LINE__);
LAMANCHA_TRAP();
}
#else
#define LAMANCHA_ASSERT(condition, message) ((void)0)
#endif

//  PRIMITIVE TYPES (Platform-Agnostic)
// =====================================

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

using i8 = char;
using i16 = short;
using i32 = int;
using i64 = long long;

using f32 = float;
using f64 = double;

#ifndef NULL
#define NULL 0
#endif

// Size types
#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || \
    defined(__aarch64__)
// 64-bit architectures
using usize = unsigned long long;
using isize = long long;
using uptr = unsigned long long;
#else
// 32-bit architectures
using usize = unsigned int;
using isize = int;
using uptr = unsigned int;
#endif

template <typename T, usize N>
struct ArrayLM {
  T data[N];

  LAMANCHA_INLINE usize size() const { return N; }

  T& operator[](usize idx) {
    LAMANCHA_ASSERT(idx < N, "Array index out of bounds");
    return data[idx];
  }

  const T& operator[](usize idx) const {
    LAMANCHA_ASSERT(idx < N, "Array index out of bounds");
    return data[idx];
  }

  T* begin() { return &data[0]; }
  T* end() { return &data[N]; }
};
/*
template < typename T >
struct ArrayListLM {};

template < typename T1, typename T2 >
struct HashMapLM {};
*/
//  HANDLES (32-bit typed IDs with generation counter)
// ====================================================

// Base handle structure (16-bit ID + 16-bit generation)
template <typename Tag>
struct Handle {
  u32 value;

  u16 id() const { return value & MASK16; }
  u16 generation() const { return (value >> 16) & MASK16; }

  static Handle create(u16 id, u16 gen) { return {(u32(gen) << 16) | id}; }

  bool isValid() const { return value != 0; }
  bool operator==(Handle other) const { return value == other.value; }
  bool operator!=(Handle other) const { return value != other.value; }
};

// Typed handles (prevent mixing entity/texture/sound IDs)
struct EntityTag {};
struct TextureTag {};
struct ShaderTag {};
struct SoundTag {};
struct SceneTag {};
struct ScriptTag {};
struct FontTag {};

using EntityHandle = Handle<EntityTag>;
using TextureHandle = Handle<TextureTag>;
using ShaderHandle = Handle<ShaderTag>;
using SoundHandle = Handle<SoundTag>;
using SceneHandle = Handle<SceneTag>;
using ScriptHandle = Handle<ScriptTag>;
using FontHandle = Handle<FontTag>;

//  COMPONENTS TYPES
// ==================

// Maximum component types supported (compile-time constant)
constexpr usize MAX_COMPONENT_TYPES = 64;
using ComponentID = u8;  // Index into component pools

// Component type registration
template <typename T>
struct ComponentType {
  static ComponentID id;
};

struct ComponentMask {
  u64 bits;  // Supports up to 64 component types
  inline void set(ComponentID id) { bits |= (1 ULL << id); }
  inline bool has(ComponentID id) { return bits & (1 ULL << id); }
};

struct Color {
  u8 r, g, b, a;

  Color() : r(255), g(255), b(255), a(255) {}
  Color(u8 _r, u8 _g, u8 _b, u8 _a = 255) : r(_r), g(_g), b(_b), a(_a) {}

  static Color fromFloat(f32 _r, f32 _g, f32 _b, f32 _a = 1.0 f) {
    f32 clamp01 = [](const f32 _val) {
      return (_val < 0.0 f) ? 0.0 f : ((_val > 1.0 f) ? 1.0 f : _val);
    };
    return Color(static_cast<u8>(clamp01(_r) * 255.0 f),
                 static_cast<u8>(clamp01(_g) * 255.0 f),
                 static_cast<u8>(clamp01(_b) * 255.0 f),
                 static_cast<u8>(clamp01(_a) * 255.0 f));
  }
  static Color fromU32(u32 _color) {
    return Color((_color >> 24) & MASK8,  // R
                 (_color >> 16) & MASK8,  // G
                 (_color >> 8) & MASK8,   // B
                 _color & MASK8           // A
    );
  }

  u32 toU32() const { return (r << 24) | (g << 16) | (b << 8) | a; }
};

//  RESULT TYPE (Error handling without exceptions)
// =================================================

enum class ErrorCode : u8 {
  None = 0,
  FileNotFound,
  OutOfMemory,
  InvalidFormat,
  ShaderCompileFailed,
  AudioDeviceError,
  ScriptError,
  Unknown = 255
};

template <typename T>
struct Result {
  T value;
  ErrorCode error;
  const char* message;

  bool isOk() const { return error == ErrorCode::None; }
  bool isError() const { return error != ErrorCode::None; }

  // Unwrap (crashes on error - for cases where failure is
  // unrecoverable)
  T unwrap() const {
    if (isError()) {
      // Propagate error somehow (needs further development)
      LM_ERR("Result::unwrap() failed: %s (code: %d)\n",
             message ? message : "Unknown error", static_cast<i32>(error));
      abort();  // Log and crash
    }
    return value;
  }

  // Unwrap with default value
  T unwrapDefault(const T& default_value) const {
    return isOk() ? value : default_value;
  }
};

//  MEMORY TYPES
// ==============

// Memory size literals
constexpr usize operator"" _KB(unsigned long long x) { return x * 1024; }
constexpr usize operator"" _MB(unsigned long long x) { return x * 1024 * 1024; }
constexpr usize operator"" _GB(unsigned long long x) {
  return x * 1024 * 1024 * 1024;
}

// Memory alignment helper
template <typename T>
constexpr usize alignOf() {
  return alignof(T);
}

// Aligned allocation size
constexpr usize alignSize(usize size, usize alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

//  STRING TYPES (Minimal, custom strings to avoid STL overhead)
// ==============================================================

// Fixed-size string (no heap allocation)
template <usize N>
struct FixedString {
  char data[N];
  usize length;

  FixedString() : length(0) { data[0] = '\0'; }

  void set(const char* str) {
    length = 0;
    while (str[length] && length < N - 1) {
      data[length] = str[length];
      length++;
    }
    data[length] = '\0';
  }

  const char* cstr() const { return data; }
};

// Common string sizes
using String32 = FixedString<32>;
using String64 = FixedString<64>;
using String128 = FixedString<128>;
using String256 = FixedString<256>;

//  TIME TYPES
// ============

struct Time {
  f64 total;  // Total elapsed time (seconds)
  f32 delta;  // Frame delta time (seconds)
  u64 frame;  // Frame counter

  Time() : total(0), delta(0), frame(0) {}
};

}  // namespace LaMancha