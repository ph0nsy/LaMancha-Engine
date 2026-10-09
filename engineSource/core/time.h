/**
 * @file time.h
 * @brief Time information manager for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 */

#pragma once

#include "pch.h"

namespace LaMancha
{
  /**
   * @brief High-resolution time in microseconds
   *
   * Use for frame timing, profiling, and animation deltas. Microsecond precision
   * is sufficient for games (1µs = 0.001ms = 0.000001s).
   *
   * @note Wraps after ~584,942 years (2^64 microseconds)
   * @note For wall-clock time, pair with a separate epoch timestamp
   *
   * Example:
   * @code
   * Time start = getCurrentTime();
   * doWork();
   * Time end = getCurrentTime();
   * f64 milliseconds = (end.micros - start.micros) / 1000.0;
   * @endcode
   */
  struct Time 
  {
    Time() noexcept : total(0.0), dt(0.f) {}
    Time(f64 _sec) noexcept : total(_sec), dt(0.f) {}

    /** @brief Update time variables */
    void update(f64 _time) { dt = static_cast<f32>(total - _time); total = _time; }

    /** @return Time elapsed as seconds */
    constexpr f64 asSeconds() const { return total; }

    /** @return Time elapsed as miliseconds */
    constexpr f64 asMilliseconds() const { return total * 1000.0f; }

    /** @return Seconds difference between frames */
    constexpr f32 deltaTime() const { return dt; }

    /** @return Seconds difference between frames */
    constexpr f32 deltaTimeMs() const { return dt * 1000.0f; }

  private:
    f64 total = 0.0;   ///< Time since program start
    f32 dt = 0.f;      ///< Time since last frame
  };

  /** @} end of engine_types group */

}