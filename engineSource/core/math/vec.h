/**
 * @file vec.h
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 */

#pragma once

#include "mathCore.h"

namespace LaMancha {
  namespace Math {
    /**
     * @brief N-component vector with SIMD support for @c N=3 and @c N=4
     *
     * SIMD-accelerated for @a vec3 / @a vec4 (maps to 128-bit register).
     * @a vec3 allocates 4 floats with padding for alignment.
     * All arithmetic operations are vectorized for @c N=3 and @c N=4.
     *
     * @tparam N Number of components
     *
     * Example:
     * @code
     *   vec3 a{1,2,3};
     *   vec3 b{4,5,6};
     *   vec3 c = (a + b) * 2.f;
     *   f32 len = c.length();
     *   vec3 unit = c.normalized();
     * @endcode
     */
    template <usize N>
    struct Vec {
      union {
        f32 data[N == 3 ? 4 : N];   ///< Scalar access (vec3 pads to 4)
#ifndef LAMANCHA_SIMD_NONE
        lm_f32x4 simd;              ///< SIMD register (N=3,4 only)
#endif
      };

      /** @brief Zero-initialize all components */
      //LAMANCHA_INLINE Vec() { for (usize i = 0; i < N; i++) { data[i] = 0; } }

      LAMANCHA_INLINE f32& operator[](usize _idx) 
      {
#if LAMANCHA_DEBUG
        LAMANCHA_ASSERT(_idx < N && _idx >= 0, "Vec > Index out of bounds.");
#endif
        return data[_idx];
      }
      LAMANCHA_INLINE const f32& operator[](usize _idx) const 
      {
#if LAMANCHA_DEBUG
        LAMANCHA_ASSERT(_idx < N && _idx >= 0, "Vec > Index out of bounds.");
#endif
        return data[_idx];
      }

      // Arithmetic operators
      //
      // For  N=3 and N=4, each operator uses a single SIMD instruction that
      // operates on all 4 lanes simultaneously. For vec3, lane 3 is padding (0),
      // so it is operated on harmlessly (0+0=0, 0-0=0, etc.) and ignored.
      //
      // For other N, a scalar loop is used. The compiler may still auto-vectorized
      // these with -O2/-O3.
      //
      // All returning operators return by value (a new Vec<N>) rather than
      // mutating. The compound assignment operators (+=, -=, etc.) mutate in place.
      //
      // Component-wise operators work directly on lm_f32x4 simd (the union member)
      // and must keep the NEON/SSE branch because there is no SimdBlock4 operator+/-.
      // Scalar operators (v * f, v / f) delegate entirely to simd_fma and
      // broadcast; no raw intrinsic branches needed there.
      //
      // Intrinsics used in component-wise ops only:
      // - vaddq_f32 / _mm_add_ps > add all 4 lanes
      // - vsubq_f32 / _mm_sub_ps > subtract all 4 lanes
      // - vmulq_f32 / _mm_mul_ps > multiply all 4 lanes
      // - vdivq_f32 / _mm_div_ps > divide all 4 lanes

      Vec<N> operator+(const Vec<N>& _vec) const {
        Vec<N> temp{ 0 };
        if constexpr (N == 4 || N == 3) {
#if defined(__ARM_NEON)
          temp.simd = vaddq_f32(simd, _vec.simd);   // add all 4 lanes
#elif defined(__SSE2__)
          temp.simd = _mm_add_ps(simd, _vec.simd);  // add all 4 lanes
#else
          for (i32 i = 0; i < 4; ++i) { temp.data[i] = data[i] + _vec.data[i]; }
#endif
        }
        else {
          for (i32 idx = 0; idx < (i32)N; idx++) { temp.data[idx] = data[idx] + _vec.data[idx]; }
        }
        return temp;
      }
      Vec<N>& operator+=(const Vec<N>& _vec) {
        if constexpr (N == 4 || N == 3) {
#if defined(__ARM_NEON)
          simd = vaddq_f32(simd, _vec.simd);
#elif defined(__SSE2__)
          simd = _mm_add_ps(simd, _vec.simd);
#else
          for (i32 i = 0; i < 4; ++i) { data[i] += _vec.data[i]; }
#endif
        }
        else {
          for (i32 idx = 0; idx < (i32)N; idx++) { data[idx] += _vec.data[idx]; }
        }
        return *this;
      }

      Vec<N> operator-(const Vec<N>& _vec) const {
        Vec<N> temp{ 0 };
        if constexpr (N == 4 || N == 3) {
#if defined(__ARM_NEON)
          temp.simd = vsubq_f32(simd, _vec.simd);
#elif defined(__SSE2__)
          temp.simd = _mm_sub_ps(simd, _vec.simd);
#else
          for (i32 i = 0; i < 4; ++i) { temp.data[i] = data[i] - _vec.data[i]; }
#endif
        }
        else {
          for (i32 idx = 0; idx < (i32)N; idx++) { temp.data[idx] = data[idx] - _vec.data[idx]; }
        }
        return temp;
      }
      Vec<N>& operator-=(const Vec<N>& _vec) {
        if constexpr (N == 4 || N == 3) {
#if defined(__ARM_NEON)
          simd = vsubq_f32(simd, _vec.simd);
#elif defined(__SSE2__)
          simd = _mm_sub_ps(simd, _vec.simd);
#else
          for (i32 i = 0; i < 4; ++i) { data[i] -= _vec.data[i]; }
#endif
        }
        else {
          for (i32 idx = 0; idx < (i32)N; idx++) { data[idx] -= _vec.data[idx]; }
        }
        return *this;
      }

      /** 
       * @brief Component-wise (Hadamard) product, NOT dot product.
       * @code result[i] = a[i] * b[i] @endcode
       */
      Vec<N> operator*(const Vec<N>& _vec) const {
        Vec<N> temp{ 0 };
        if constexpr (N == 4 || N == 3) {
#if defined(__ARM_NEON)
          temp.simd = vmulq_f32(simd, _vec.simd);
#elif defined(__SSE2__)
          temp.simd = _mm_mul_ps(simd, _vec.simd);
#else
          for (i32 i = 0; i < 4; ++i) { temp.data[i] = data[i] * _vec.data[i]; }
#endif
        }
        else {
          for (i32 idx = 0; idx < (i32)N; idx++) { temp.data[idx] = data[idx] * _vec.data[idx]; }
        }
        return temp;
      }
      Vec<N>& operator*=(const Vec<N>& _vec) {
        if constexpr (N == 4 || N == 3) {
#if defined(__ARM_NEON)
          simd = vmulq_f32(simd, _vec.simd);
#elif defined(__SSE2__)
          simd = _mm_mul_ps(simd, _vec.simd);
#else
          for (i32 i = 0; i < 4; ++i) { data[i] *= _vec.data[i]; }
#endif
        }
        else {
          for (i32 idx = 0; idx < (i32)N; idx++) { data[idx] *= _vec.data[idx]; }
        }
        return *this;
      }

      /**
       * @brief Scalar multiply.
       * 
       * @details
       * Load this vector into a SimdBlock4, broadcast the scalar to all 4 lanes via @c broadcast .
       * Multiply using simd_fma(v, scalar, 0) which computes (v * scalar) + 0.
       * 
       * @note
       * Using SimdBlock4 ops entirely avoids writing a naked vmulq_f32 / _mm_mul_ps branch.
       */
      Vec<N> operator*(f32 _scalar) const {
        Vec<N> temp{ 0 };
        if constexpr (N == 4 || N == 3) {
#ifndef LAMANCHA_SIMD_NONE
          SimdBlock4 s = SimdBlock4::load(data);
          SimdBlock4 bcast = broadcast(_scalar);
          SimdBlock4 zero{ 0 };
          temp.simd = simd_fma(s, bcast, zero).reg;
#else
          for (i32 i = 0; i < 4; ++i) { temp.data[i] = data[i] * _scalar; }
#endif
        }
        else {
          for (i32 idx = 0; idx < (i32)N; idx++) { temp.data[idx] = data[idx] * _scalar; }
        }
        return temp;
      }
      Vec<N>& operator*=(f32 _scalar) {
        if constexpr (N == 4 || N == 3) {
#ifndef LAMANCHA_SIMD_NONE
          SimdBlock4 s = SimdBlock4::load(data);
          SimdBlock4 bcast = broadcast(_scalar);
          SimdBlock4 zero{ 0 };
          simd = simd_fma(s, bcast, zero).reg;
#else
          for (i32 i = 0; i < 4; ++i) { data[i] *= _scalar; }
#endif
        }
        else {
          for (i32 idx = 0; idx < (i32)N; idx++) { data[idx] *= _scalar; }
        }
        return *this;
      }

      Vec<N> operator/(const Vec<N>& _vec) const {
        Vec<N> temp{ 0 };
        if constexpr (N == 4 || N == 3) {
#if defined(__ARM_NEON)
          temp.simd = vdivq_f32(simd, _vec.simd);
#elif defined(__SSE2__)
          temp.simd = _mm_div_ps(simd, _vec.simd);
#else
          for (i32 i = 0; i < 4; ++i) { temp.data[i] = data[i] / _vec.data[i]; }
#endif
        }
        else {
          for (i32 idx = 0; idx < (i32)N; idx++) { temp.data[idx] = data[idx] / _vec.data[idx]; }
        }
        return temp;
      }
      Vec<N>& operator/=(const Vec<N>& _vec) {
        if constexpr (N == 4 || N == 3) {
#if defined(__ARM_NEON)
          simd = vdivq_f32(simd, _vec.simd);
#elif defined(__SSE2__)
          simd = _mm_div_ps(simd, _vec.simd);
#else
          for (i32 i = 0; i < 4; ++i) { data[i] /= _vec.data[i]; }
#endif
        }
        else {
          for (i32 idx = 0; idx < (i32)N; idx++) { data[idx] /= _vec.data[idx]; }
        }
        return *this;
      }

      /**
       * @brief Scalar divide.
       *
       * @details
       * Compute the reciprocal once.
       * Then multiply all components.
       * One division + N multiplies is faster than N divisions (~20 cycles each)
       *
       * @note
       * @c broadcast and @c simd_fma handle the platform branch.
       */
      Vec<N> operator/(f32 _scalar) const {
        Vec<N> temp{ 0 };
        if constexpr (N == 4 || N == 3) {
          f32 inv = 1.0f / _scalar;
#ifndef LAMANCHA_SIMD_NONE
          SimdBlock4 s = SimdBlock4::load(data);
          SimdBlock4 bcast = broadcast(inv);
          SimdBlock4 zero{ 0 };
          temp.simd = simd_fma(s, bcast, zero).reg;
#else
          for (i32 i = 0; i < 4; ++i) { temp.data[i] = data[i] * inv; }
#endif
        }
        else {
          for (i32 idx = 0; idx < (i32)N; idx++) { temp.data[idx] = data[idx] / _scalar; }
        }
        return temp;
      }
      Vec<N>& operator/=(f32 _scalar) {
        if constexpr (N == 4 || N == 3) {
          f32 inv = 1.0f / _scalar;
#ifndef LAMANCHA_SIMD_NONE
          SimdBlock4 s = SimdBlock4::load(data);
          SimdBlock4 bcast = broadcast(inv);
          SimdBlock4 zero{ 0 };
          simd = simd_fma(s, bcast, zero).reg;
#else
          for (i32 i = 0; i < 4; ++i) { data[i] *= inv; }
#endif
        }
        else {
          for (i32 idx = 0; idx < (i32)N; idx++) { data[idx] /= _scalar; }
        }
        return *this;
      }

      /**
       * @brief Squared length (cheaper than length).
       * 
       * @note Prefer lengthSq over length whenever only distance comparisons are needed, saving an fSqrt call
       * @note Delegates to Dot(*this) which handles the full SIMD pipeline
       */
      LAMANCHA_INLINE f32 lengthSq() const {
        if constexpr (N == 3 || N == 4) {
          return Dot(*this);  // Dot already has the SIMD/scalar branch
        }
        else {
          f32 sum = 0.f;
          for (i32 idx = 0; idx < (i32)N; idx++) { sum += data[idx] * data[idx]; }
          return sum;
        }
      }

      /** @brief Euclidean length. */
      LAMANCHA_INLINE f32 length() const { return fSqrt(lengthSq()); }

      /** 
       * @brief Return normalized copy (length=1).
       * 
       * @note v / |v| = v * (1 / |v|) = v * rsqrt(dot(v, v))
       */
      LAMANCHA_INLINE Vec<N> normalized() const {
        Vec<N> norm{ 0 };
        if constexpr (N == 3 || N == 4) {
          f32 lenSq = lengthSq();  // delegates to Dot(*this), already SIMD
          if (lenSq == 0.f) { return norm; }
#ifndef LAMANCHA_SIMD_NONE
          // Broadcast lenSq to all 4 lanes, compute rsqrt, multiply each component.
          // simd_fma(vec, rsqrt, zero) = (vec * rsqrt) + 0.
          SimdBlock4 vec = SimdBlock4::load(data);
          SimdBlock4 rsqrtBlk = simd_rsqrt_approx(broadcast(lenSq));
          SimdBlock4 zero{ 0 };
          norm.simd = simd_fma(vec, rsqrtBlk, zero).reg;
#else
          f32 inv = fRSqrt(lenSq);
          for (i32 i = 0; i < (i32)N; ++i) { norm.data[i] = data[i] * inv; }
#endif
        }
        else {
          f32 len = length();
          if (len == 0.f) { return norm; }
          for (i32 idx = 0; idx < (i32)N; idx++) { norm.data[idx] = data[idx] / len; }
        }
        return norm;
      }

      /**
       * @brief Dot product.
       * 
       * @details
       * |a| * |b| * cos(theta), where theta is the angle between the two vectors.
       * 
       * @returns 
       * - Dot > 0 means angle < 90 degrees
       * - Dot < 0 means > 90 degrees
       * - Dot == 0 means perpendicular.
       * 
       * @note Dot(a, b) = a.x*b.x + a.y*b.y + a.z*b.z
       */
      LAMANCHA_INLINE f32 Dot(const Vec<N>& _other) const {
        f32 res = 0.f;
        if constexpr (N == 3 || N == 4) {
          SimdBlock4 a{ 0 }, b{ 0 }, prod{ 0 };
          a.reg = simd;
          b.reg = _other.simd;
#ifndef LAMANCHA_SIMD_NONE
          // simd_fma(a, b, zero) = (a * b) + 0. The platform branch is inside simd_fma.
          SimdBlock4 zero{ 0 };
          prod = simd_fma(a, b, zero);
#else
          for (i32 i = 0; i < 4; ++i) { prod.arr[i] = a.arr[i] * b.arr[i]; }
#endif
          res = horizontal_sum(prod);
        }
        else {
          for (i32 idx = 0; idx < N; idx++) { res += data[idx] * _other.data[idx]; }
        }
        return res;
      }

      /** 
       * @brief Cross product (vec3 only).
       * 
       * @details
       * Defined only for 3-component vectors. The result is a vector perpendicular
       * to both inputs, with magnitude |a|*|b|*sin(theta).
       * @note 
       * _mm_shuffle(3,0,2,1) means: 
       * - output lane 3 = input lane 3 (w), 
       * - output lane 2 = input lane 0 (x), 
       * - output lane 1 = input lane 2 (z), 
       * - output lane 0 = input lane 1 (y). 
       * So: [y, z, x, w].
       */
      LAMANCHA_INLINE Vec<3> Cross(const Vec<3>& _other) const {
#if LAMANCHA_DEBUG
        LAMANCHA_ASSERT(N == 3, "Vec<N>::Cross > Cross function can only be use for N equals 3");
#endif
        Vec<3> result{};
#if defined(__ARM_NEON)
        // vextq_f32(a, a, n) rotates the register left by n lanes.
        // Rotation by 1: [x, y, z, w] -> [y, z, w, x]  (w = 0 padding)
        // Rotation by 2: [x, y, z, w] -> [z, w, x, y]
        lm_f32x4 a_yzx = vextq_f32(simd, simd, 1);
        lm_f32x4 a_zxy = vextq_f32(simd, simd, 2);
        lm_f32x4 b_yzx = vextq_f32(_other.simd, _other.simd, 1);
        lm_f32x4 b_zxy = vextq_f32(_other.simd, _other.simd, 2);
        result.simd = vsubq_f32(vmulq_f32(a_yzx, b_zxy), vmulq_f32(a_zxy, b_yzx));
#elif defined(__SSE2__)
        lm_f32x4 a_yzxw = _mm_shuffle_ps(simd, simd, _MM_SHUFFLE(3, 0, 2, 1));
        lm_f32x4 a_zxyw = _mm_shuffle_ps(simd, simd, _MM_SHUFFLE(3, 1, 0, 2));
        lm_f32x4 b_yzxw = _mm_shuffle_ps(_other.simd, _other.simd, _MM_SHUFFLE(3, 0, 2, 1));
        lm_f32x4 b_zxyw = _mm_shuffle_ps(_other.simd, _other.simd, _MM_SHUFFLE(3, 1, 0, 2));
        result.simd = _mm_sub_ps(_mm_mul_ps(a_yzxw, b_zxyw), _mm_mul_ps(a_zxyw, b_yzxw));
#else
        result.data[0] = data[1] * _other.data[2] - data[2] * _other.data[1];
        result.data[1] = data[2] * _other.data[0] - data[0] * _other.data[2];
        result.data[2] = data[0] * _other.data[1] - data[1] * _other.data[0];
#endif
        return result;
      }
    };

    using vec2 = Vec<2>;
    using vec3 = Vec<3>;      // 16 bytes, SIMD-backed (padded to 4 floats)
    using vec4 = Vec<4>;      // 16 bytes, SIMD-backed (maps 1:1 to one register)
  }
}