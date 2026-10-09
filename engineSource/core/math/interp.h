/**
 * @file interp.h
 * @brief Interpolation functions for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 */

#pragma once

#include "mathCore.h"

namespace LaMancha {
  namespace Math {
    // All functions expect T to be either a scalar (f32) or a Vec<N>
    namespace Interpolation {
      /** @brief Three easing variants define which part of the curve is eased */
      enum EEasingType { In, Out, InOut };

      /** @brief Linear interpolation: a*(1 - t) + b*t.
       *
       *  At t=0 the result is exactly _start; at t=1 it is exactly _end.
       *
       *  @note We use this form rather than a + t*(b-a) because it is more stable when t is near 0 or 1.
       */
      template <typename T>
      LAMANCHA_INLINE T Lerp(T _start, T _end, f32 _step) {
        return _start * (1.f - _step) + _end * _step;
      }

      /** @brief Lerp for 4 independent float pairs simultaneously.
       *
       *  Each of the 4 SIMD lanes has its own start, end, and t value.
       *
       *  @note Useful for blending 4 separate scalars (like RGBA color channels) at once.
       */
      LAMANCHA_INLINE SimdBlock4 simd_lerp(
        const SimdBlock4& _start,
        const SimdBlock4& _end,
        const SimdBlock4& _step)
      {
#ifndef LAMANCHA_SIMD_NONE
        // Compute (1 - t) for all 4 lanes using simd_fma:
        //   one_minus_t = simd_fma(-_step, one, one) = (-_step * 1) + 1 = 1 - t
        // Then: result = simd_fma(_end, _step, simd_fma(_start, one_minus_t, zero))
        //             = end*t + start*(1-t)
        SimdBlock4 one = broadcast(1.0f);
        SimdBlock4 zero{ 0 };
        // one_minus_t = simd_fma(-1, _step, one) but we lack negate, so use:
        // start_term  = simd_fma(_start, one, zero) then subtract _step*_start...
        // Simpler: compute 1-t as simd_fma(neg_step, one, one) isn't available without negate.
        // Use the subtract-as-fma identity: (1*1) + (-1 * _step) needs negative.
        // Cleanest available: load 1-t via fma on the complement.
        // one_minus_t[i] = 1.0f - _step[i]: use simd_fma with negated step isn't possible
        // without a negate helper, so we use the identity directly with two fma calls:
        //   start_term = fma(_start, one, zero)  = _start * 1        (load identity)
        //   result     = fma(_end, _step, fma(_start, one_minus_t, zero))
        // For one_minus_t we fall back to load+subtract-via-fma with a -1 broadcast:
        SimdBlock4 neg_one = broadcast(-1.0f);
        // one_minus_t = fma(neg_one, _step, one) = (-1 * _step) + 1 = 1 - t
        SimdBlock4 one_minus_t = simd_fma(neg_one, _step, one);
        // start_term = fma(_start, one_minus_t, zero) = _start * (1-t)
        SimdBlock4 start_term = simd_fma(_start, one_minus_t, zero);
        // result = fma(_end, _step, start_term) = _end*t + _start*(1-t)
        return simd_fma(_end, _step, start_term);
#else
        SimdBlock4 result{ 0 };
        for (i32 i = 0; i < 4; ++i) 
        {
          result.arr[i] = Lerp<f32>(_start.arr[i], _end.arr[i], _step.arr[i]);
        }
        return result;
#endif
      }

      /** @defgroup easing Easing curves
       *
       *  All easing functions follow the same pattern: Compute current [0, 1]
       *  and Lerp between _start and _end.
       *
       *  @note Formulas from https://easings.net/
       *  @{
       */

      template <typename T>
      LAMANCHA_INLINE T SineIterp(T _start, T _end, f32 _step, EEasingType _type = EEasingType::Out) {
        // sin(t*pi/2) accelerates smoothly from 0 to 1 (Out).
        // cos gives the mirror: decelerates smoothly (In).
        f32 current = 0.f;
        switch (_type) {
        case EEasingType::In:    current = 1.f - fCos((_step * PI) / 2.f); break;
        case EEasingType::InOut: current = -(fCos(_step * PI) - 1.f) / 2.f; break;
        default:                 current = fSin((_step * PI) / 2.f); break;
        }
        return Lerp<T>(_start, _end, current);
      }

      template <typename T>
      LAMANCHA_INLINE T QuadIterp(T _start, T _end, f32 _step, EEasingType _type = EEasingType::Out) {
        // t^2 curve - gentle acceleration/deceleration, cheap to compute.
        f32 current = 0.f;
        switch (_type) {
        case EEasingType::In:    current = _step * _step; break;
        case EEasingType::InOut: current = _step < 0.5f ? 2.f * _step * _step : 1.f - fPow(-2.f * _step + 2.f, 2.f) / 2.f; break;
        default:                 current = 1.f - (1.f - _step) * (1.f - _step); break;
        }
        return Lerp<T>(_start, _end, current);
      }

      template <typename T>
      LAMANCHA_INLINE T CubicInterp(T _start, T _end, f32 _step, EEasingType _type = EEasingType::Out) {
        // t^3 curve - more pronounced than quad, still smooth.
        f32 current = 0.f;
        switch (_type) {
        case EEasingType::In:    current = _step * _step * _step; break;
        case EEasingType::InOut: current = _step < 0.5f ? 4.f * _step * _step * _step : 1.f - fPow(-2.f * _step + 2.f, 3.f) / 2.f; break;
        default:                 current = 1.f - fPow(1.f - _step, 3.f); break;
        }
        return Lerp<T>(_start, _end, current);
      }

      template <typename T>
      LAMANCHA_INLINE T QuartInterp(T _start, T _end, f32 _step, EEasingType _type = EEasingType::Out) {
        // t^4 curve - sharper acceleration/deceleration than cubic.
        f32 current = 0.f;
        switch (_type) {
        case EEasingType::In:    current = _step * _step * _step * _step; break;
        case EEasingType::InOut: current = _step < 0.5f ? 8.f * _step * _step * _step * _step : 1.f - fPow(-2.f * _step + 2.f, 4.f) / 2.f; break;
        default:                 current = 1.f - fPow(1.f - _step, 4.f); break;
        }
        return Lerp<T>(_start, _end, current);
      }

      template <typename T>
      LAMANCHA_INLINE T QuintInterp(T _start, T _end, f32 _step, EEasingType _type = EEasingType::Out) {
        // t^5 curve - very sharp, noticeable snap at start or end.
        f32 current = 0.f;
        switch (_type) {
        case EEasingType::In:    current = _step * _step * _step * _step * _step; break;
        case EEasingType::InOut: current = _step < 0.5f ? 16.f * _step * _step * _step * _step * _step : 1.f - fPow(-2.f * _step + 2.f, 5.f) / 2.f; break;
        default:                 current = 1.f - fPow(1.f - _step, 5.f); break;
        }
        return Lerp<T>(_start, _end, current);
      }

      template <typename T>
      LAMANCHA_INLINE T ExpoInterp(T _start, T _end, f32 _step, EEasingType _type = EEasingType::Out) {
        // 2^(10t - 10) - exponential growth, extreme snap.
        // Special-cased at t=0 and t=1 because 2^-∞ and 2^∞ would produce 0/∞.
        f32 current = 0.f;
        switch (_type) {
        case EEasingType::In:
          current = _step == 0.f ? 0.f : fPow(2.f, 10.f * _step - 10.f);
          break;
        case EEasingType::InOut:
          current = _step == 0.f ? 0.f
            : _step == 1.f ? 1.f
            : _step < 0.5f ? fPow(2.f, 20.f * _step - 10.f) / 2.f
            : (2.f - fPow(2.f, -20.f * _step + 10.f)) / 2.f;
          break;
        default:
          current = _step == 1.f ? 1.f : 1.f - fPow(2.f, -10.f * _step);
          break;
        }
        return Lerp<T>(_start, _end, current);
      }

      template <typename T>
      LAMANCHA_INLINE T CircInterp(T _start, T _end, f32 _step, EEasingType _type = EEasingType::Out) {
        // 1 - sqrt(1 - t^2) traces a quarter circle. Produces a "mechanical" feel.
        f32 current = 0.f;
        switch (_type) {
        case EEasingType::In:    current = 1.f - fSqrt(1.f - fPow(_step, 2.f)); break;
        case EEasingType::InOut: current = _step < 0.5f
          ? (1.f - fSqrt(1.f - fPow(2.f * _step, 2.f))) / 2.f
          : (fSqrt(1.f - fPow(-2.f * _step + 2.f, 2.f)) + 1.f) / 2.f;
          break;
        default:                 current = fSqrt(1.f - fPow(_step - 1.f, 2.f)); break;
        }
        return Lerp<T>(_start, _end, current);
      }

      template <typename T>
      LAMANCHA_INLINE T BackInterp(T _start, T _end, f32 _step, EEasingType _type = EEasingType::Out) {
        // Overshoots slightly past the target before settling.
        // constant1 (overshoot amount), constant2 (InOut variant), constant3 (cubic term).
        const f32 constant1 = 1.70158f;
        const f32 constant2 = constant1 * 1.525f;
        const f32 constant3 = constant1 + 1.f;
        f32 current = 0.f;
        switch (_type) {
        case EEasingType::In:
          current = constant3 * _step * _step * _step - constant1 * _step * _step;
          break;
        case EEasingType::InOut:
          current = _step < 0.5f
            ? (fPow(2.f * _step, 2.f) * ((constant2 + 1.f) * 2.f * _step - constant2)) / 2.f
            : (fPow(2.f * _step - 2.f, 2.f) * ((constant2 + 1.f) * (_step * 2.f - 2.f) + constant2) + 2.f) / 2.f;
          break;
        default:
          current = 1.f + constant3 * fPow(_step - 1.f, 3.f) + constant1 * fPow(_step - 1.f, 2.f);
          break;
        }
        return Lerp<T>(_start, _end, current);
      }

      template <typename T>
      LAMANCHA_INLINE T ElasticInterp(T _start, T _end, f32 _step, EEasingType _type = EEasingType::Out) {
        // Oscillates past the target like a spring, then settles.
        // constant1 controls the period of the spring oscillation (2*pi/3 cycles).
        // constant2 controls the InOut variant period (2*pi/4.5 cycles).
        const f32 constant1 = 2.f * PI / 3.f;
        const f32 constant2 = 2.f * PI / 4.5f;
        f32 current = 0.f;
        switch (_type) {
        case EEasingType::In:
          current = _step == 0.f ? 0.f : _step == 1.f ? 1.f
            : -fPow(2.f, 10.f * _step - 10.f) * fSin((_step * 10.f - 10.75f) * constant1);
          break;
        case EEasingType::InOut:
          current = _step == 0.f ? 0.f : _step == 1.f ? 1.f
            : _step < 0.5f
            ? -(fPow(2.f, 20.f * _step - 10.f) * fSin((20.f * _step - 11.125f) * constant2)) / 2.f
            : (fPow(2.f, -20.f * _step + 10.f) * fSin((20.f * _step - 11.125f) * constant2)) / 2.f + 1.f;
          break;
        default:
          current = _step == 0.f ? 0.f : _step == 1.f ? 1.f
            : fPow(2.f, -10.f * _step) * fSin((_step * 10.f - 0.75f) * constant1) + 1.f;
          break;
        }
        return Lerp<T>(_start, _end, current);
      }

      //  BounceHelper
      //
      //  Computes the Out easing for a bounce effect. The function is a piecewise
      //  parabola: each segment is a t^2 curve (constant * t^2) offset to start where
      //  the previous segment ended. The offsets (1.5, 2.25, 2.625) and additions
      //  (0.75, 0.9375, 0.984375) are derived from the physics of a bouncing ball
      //  where each bounce is a fixed fraction of the previous height.
      //
      //  constantSQRT (2.75) is the denominator used to divide the [0,1] domain
      //  into 4 decreasing segments matching the visual bounce rhythm.

      LAMANCHA_INLINE f32 BounceHelper(f32 _step) {
        const f32 constant = 7.5625f;
        const f32 constantSQRT = 2.75f;
        if (_step < 1.f / constantSQRT) {
          return constant * _step * _step;
        }
        else if (_step < 2.f / constantSQRT) {
          _step -= 1.5f / constantSQRT;
          return constant * _step * _step + 0.75f;
        }
        else if (_step < 2.5f / constantSQRT) {
          _step -= 2.25f / constantSQRT;
          return constant * _step * _step + 0.9375f;
        }
        else {
          _step -= 2.625f / constantSQRT;
          return constant * _step * _step + 0.984375f;
        }
      }

      template <typename T>
      LAMANCHA_INLINE T BounceInterp(T _start, T _end, f32 _step, EEasingType _type = EEasingType::Out) {
        // In variant: reverse the step so the bounce plays backwards (start fast, land slow).
        // InOut: split at 0.5 - first half bounces in, second half bounces out.
        f32 current = 0.f;
        switch (_type) {
        case EEasingType::In:    current = 1.f - BounceHelper(1.f - _step); break;
        case EEasingType::InOut: current = _step < 0.5f
          ? (1.f - BounceHelper(1.f - 2.f * _step)) / 2.f
          : (1.f + BounceHelper(2.f * _step - 1.f)) / 2.f;
          break;
        default: current = BounceHelper(_step); break;
        }
        return Lerp<T>(_start, _end, current);
      }

      /** @brief CSS-stype cubic-bezier easing: bezier-curve(x1, y1, x2, y2).
       *
       *  The curve is defined by two control points P1=(x1,y1) and P2=(x2,y2).
       *  P0=(0,0) and P3=(1,1) are fixed, mapping input t=0 to output 0 and t=1 to 1.
       *
       *  Example:
       *  @code
       *  CubicBezierInterpolator<f32>::Interpolate(f32{start}, f32{end}, x1, y1, x2, y2, step);
       *  CubicBezierInterpolator<vec3>::Interpolate(vec3{start}, vec3{end}, x1, y1, x2, y2, step);
       *  @endcode
       */
      template <typename T>
      class CubicBezierInterpolator {
      public:
        // p1..p4 correspond to x1, y1, x2, y2 in CSS bezier notation.
        /** @brief Cubic-bezier easing algorithm.
         *
         *  The cubic bezier is parameterized by a curve parameter `u` in [0,1].
         *  We need to find `u` such that the X component of bezier(u) = input _step.
         *  This is solved with Newton's method (5 iterations, converges quickly
         *  because the curve is well-behaved). Then we evaluate the Y component
         *  at that `u` to get the eased output value.
         *
         *  @param p1, p2, p3, p4 correspond to x1, y1, x2, y2 in CSS bezier notation
         */
        static f32 GetEasedStep(f32 _step, f32 _x1, f32 _y1, f32 _x2, f32 _y2) {
          if (_step <= 0.0f) { return 0.0f; }
          if (_step >= 1.0f) { return 1.0f; }

          // Newton's method: find curve parameter u where X(u) = _step.
          // Starts with u = _step as initial guess (identity is a reasonable start).
          f32 curveStep = _step;
          i8  i = 5;   // 5 iterations is enough for convergence on well-formed curves
          while (i > 0) {
            i--;
            f32 slope = GetSlope(curveStep, _x1, _x2);
            if (slope == 0.0f) { break; }               // flat tangent - stop
            f32 x = GetBezierValue(curveStep, _x1, _x2) - _step;
            curveStep -= x / slope;                      // Newton step: u -= f(u)/f'(u)
          }
          return GetBezierValue(curveStep, _y1, _y2);      // Y output at solved u
        }

        /** @brief Cubic Bezier interpolation entry point
         *
         *  Unlike the polynomial easings, this gives full control over the curve shape by adjusting 4 floats
         *
         *  @param _start Value when t = 0
         *  @param _end Value when t = 1
         *  @param p1, p2, p3, p4 correspond to x1, y1, x2, y2 in CSS bezier notation
         *  @param _step Current t value to interpolate to.
         */
        static T Interpolate(const T& _start, const T& _end, f32 _p1, f32 _p2, f32 _p3, f32 _p4, f32 _step) {
          f32 easedStep = GetEasedStep(_step, _p1, _p2, _p3, _p4);
          return Lerp<T>(_start, _end, easedStep);
        }

      private:
        // Standard 1D cubic bezier: P0=0, P3=1 (fixed), P1=_p1, P2=_p2 (control points).
        // Formula: 3*(1-u)^2*u*p1 + 3*(1-u)*u^2*p2 + u^3
        static f32 GetBezierValue(f32 _step, f32 _p1, f32 _p2) {
          f32 inv = 1.0f - _step;
          return 3.f * inv * inv * _step * _p1 + 3.f * inv * _step * _step * _p2 + _step * _step * _step;
        }

        // Derivative of the cubic bezier (used by Newton's method as the slope).
        // Formula: 3*(1-u)^2*p1 + 6*(1-u)*u*(p2-p1) + 3*u^2*(1-p2)
        static f32 GetSlope(f32 _step, f32 _p1, f32 _p2) {
          f32 inv = 1.0f - _step;
          return 3.f * inv * inv * _p1 + 6.f * inv * _step * (_p2 - _p1) + 3.f * _step * _step * (1.0f - _p2);
        }
      };

      /** @brief Easing functions. */
      enum EInterpolationFunc { Linear, Sine, Quad, Cubic, Quart, Quint, Expo, Circ, Back, Elastic, Bounce };

      /** @brief Single entry point for all easing functions.
       *
       *  Each of the 4 SIMD lanes has its own start, end, and t value.
       *
       *  @param _start Value when _step = 0
       *  @param _end Value when _step = 1
       *  @param _function What curve to use
       *  @param _type What easing type (in, out, in-out)
       *  @param _bMirror Reflects the curve at t = 0.5 (ping-pong)
       *  @note Useful for blending 4 separate scalars (like RGBA color channels) at once.
       */
      template <typename T>
      LAMANCHA_INLINE T Ease(T _start, T _end, f32 _step, EInterpolationFunc _function,
        EEasingType _type = EEasingType::Out, bool _bMirror = false)
      {
        _step = fClamp(_step, 0.f, 1.f);
        if (_bMirror) {
          // Mirror: first half [0, 0.5) maps to [0, 1), second half [0.5, 1] maps back to [1, 0].
          _step = _step < 0.5f ? _step * 2.f : 1.f - (_step * 2.f - 1.f);
        }
        switch (_function) {
        case Sine:    return SineIterp<T>(_start, _end, _step, _type);
        case Quad:    return QuadIterp<T>(_start, _end, _step, _type);
        case Cubic:   return CubicInterp<T>(_start, _end, _step, _type);
        case Quart:   return QuartInterp<T>(_start, _end, _step, _type);
        case Quint:   return QuintInterp<T>(_start, _end, _step, _type);
        case Expo:    return ExpoInterp<T>(_start, _end, _step, _type);
        case Circ:    return CircInterp<T>(_start, _end, _step, _type);
        case Back:    return BackInterp<T>(_start, _end, _step, _type);
        case Elastic: return ElasticInterp<T>(_start, _end, _step, _type);
        case Bounce:  return BounceInterp<T>(_start, _end, _step, _type);
        default:      return Lerp<T>(_start, _end, _step);
        }
      }

      /** @} */ // end of easing group
    }
  }
}