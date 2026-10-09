/**
 * @file platform.h
 * @brief Platform detection via hint definitions
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * @note NEVER add platform-specific code directly in other files.
 * @note You may expand this file by adding a macro here and check that macro everywhere else.
 */

#pragma once

#if defined(TARGET)
#if defined(TARGET_WINDOWS)
#define LAMANCHA_PLATFORM_WINDOWS
#define LAMANCHA_IS_EMBEDDED 0
#elif defined(TARGET_LINUX)
#define LAMANCHA_PLATFORM_LINUX
#define LAMANCHA_IS_EMBEDDED 0
#elif defined(TARGET_ANDROID)
#define LAMANCHA_PLATFORM_ANDROID
#define LAMANCHA_IS_EMBEDDED 1
#elif defined(TARGET_R36S)
#define LAMANCHA_PLATFORM_R36S
#define LAMANCHA_IS_EMBEDDED 1
#elif defined(TARGET_MAC)
#define LAMANCHA_PLATFORM_MACOS
#define LAMANCHA_IS_EMBEDDED 0
#elif defined(TARGET_IOS)
#define LAMANCHA_PLATFORM_IPHONE
#define LAMANCHA_IS_EMBEDDED 1
#else
#error "Unknown target platform provided via TARGET macro"
#endif

#else // Fallback platform detection
#if defined(_WIN32) || defined(_WIN64)
#define LAMANCHA_PLATFORM_WINDOWS
#define LAMANCHA_IS_EMBEDDED 0

#elif defined(__linux__)
#define LAMANCHA_PLATFORM_LINUX

#if defined(__aarch64__) || defined(_M_ARM64)
#define LAMANCHA_PLATFORM_ARM_64
#elif defined(__arm__) || defined(_M_ARM)
#define LAMANCHA_PLATFORM_ARM_32
#endif
#if defined(__ARM_ARCH_8A) // || defined(__e2k__) to target emulation of x86?
#define LAMANCHA_PLATFORM_ARM_V8A           ///< Detect ARMv8-A specifically
#endif

#if defined(__ANDROID__)
#define LAMANCHA_PLATFORM_ANDROID           ///< Detect Android
#endif

#if defined(LAMANCHA_PLATFORM_ARM_V8A) && defined(__RK3326__)
#define LAMANCHA_PLATFORM_R36S              ///< Detect Rockchip RK3326
#endif

#if defined(LAMANCHA_PLATFORM_ANDROID) || defined(LAMANCHA_PLATFORM_R36S)
#define LAMANCHA_IS_EMBEDDED 1
#else
#define LAMANCHA_IS_EMBEDDED 0
#endif

#elif defined(__APPLE__) || defined(__MACH__)
#define LAMANCHA_PLATFORM_APPLE
#include <TargetConditionals.h>
#if defined(TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR)
#define LAMANCHA_PLATFORM_IPHONE
#define LAMANCHA_IS_EMBEDDED 1
#elif defined(TARGET_OS_MAC)
#define LAMANCHA_PLATFORM_MACOS
#define LAMANCHA_IS_EMBEDDED 0
#endif
#else
#error "Unknown or unsupported platform"
#endif

#endif

#if defined(LAMANCHA_PLATFORM_APPLE)
#error "Apple platforms are currently declared but not supported by the engine."
#endif