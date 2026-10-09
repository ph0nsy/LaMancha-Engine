/**
 * @file mathCore.h
 * @brief Foundational math functions for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * Constants (PI, conversions, epsilon)
 * Scalar math (abs, fmod, sin/cos, sqrt, log/exp, min/max)
 * SIMD utilities (abs, rsqrt, min, max, clamp)
 */

#pragma once

#include "core/pch.h"

 /**  @brief Use LUT (1) vs polynomial (0) for trig */
#define USE_SIN_LUT 1

/**  @brief Enable Mat division operators (1=yes, 0=no)
  *
  *  Division on matrices is semantically ambiguous (left-divide vs right-divide)and expensive
  */
#define DIV_OPD_USE 1

namespace LaMancha {
  namespace Math {
    /** @defgroup math_constants Mathematical Constants
     *
     *  All constants are constexpr f32 - they are substituted at compile time.
     *  @{
     */

    constexpr f32 PI = 3.14159265358f;            ///< pi
    constexpr f32 INV_PI = 1.0f / PI;             ///< 1/pi (faster multiply)
    constexpr f32 HALF_PI = PI / 2.f;             ///< pi/2 (90 deg)
    constexpr f32 TWO_PI = PI * 2.f;              ///< 2*pi (360 deg)
    constexpr f32 INV_TWO_PI = 1.0f / TWO_PI;     ///< 1/(2*pi)

    constexpr f32 RAD_2_DEG = 180.f / PI;         ///< Radians to degrees
    constexpr f32 DEG_2_RAD = PI / 180.f;         ///< Degrees to radians

    constexpr f32 LN_2 = 0.69314718f;             ///< ln(2)
    constexpr f32 L2_E = 1.44269504f;             ///< log2(e)

    constexpr f32 MACHINE_EPSILON = 1.19209e-7f;  ///< f32 epsilon (2^-23)
    /** @} */ // end of math_constants group

    /** @defgroup scalar_funcs Scalar Math Functions
     *  @{
     */

     /** @brief Branchless integer absolute value
      *  @param _x Input
      *  @return |_x|
      */
    LAMANCHA_INLINE i32 iAbs(i32 _x) { return (_x ^ (_x >> 31)) - (_x >> 31); }

    /** @brief Float absolute value via bit manipulation
     *  @param _x Input
     *  @return |_x|
     */
    LAMANCHA_INLINE f32 fAbs(f32 _x) {
      union { f32 f; u32 u; } res{ 0 };
      res.f = _x;
      res.u &= 0x7FFFFFFFu;  // clear bit 31 (sign bit), preserve exponent+mantissa
      return res.f;
    }

    /**
     * @brief 128-bit SIMD block viewed as either 4 floats or a register
     *
     * Union allows zero-cost reinterpretation between scalar array access
     * (arr[0..3]) and SIMD register (reg).
     *
     * @note Requires 16-byte alignment (enforced by LAMANCHA_SIMD_ALIGN)
     * @note On non-SIMD builds, only arr exists (reg is conditionally compiled)
     * @note Writing to arr and reading from reg (or vice versa) is safe, both 
     * are active members of the union simultaneously
     *
     * @code
     * SimdBlock4 v;
     * v.arr[0] = 1.f;
     * v.arr[1] = 2.f;
     * v.arr[2] = 3.f;
     * v.arr[3] = 4.f;
     * // v.reg now contains (1,2,3,4)
     * @endcode
     */
    union LAMANCHA_SIMD_ALIGN SimdBlock4
    {
      f32 arr[4];  ///< Scalar view: arr[i] = lane i (index 0 = lowest lane)

#ifndef LAMANCHA_SIMD_NONE
      lm_f32x4 reg;  ///< SIMD register view (128-bit hardware register)
#endif

      /**
       * @brief Load 4 floats from memory into a SimdBlock4
       * @param _ptr Pointer to 4 consecutive floats (need not be 16-byte aligned)
       * @return SimdBlock4 containing the loaded values
       *
       * Uses unaligned load instructions so source data doesn't require 16-byte alignment. 
       * Important when alignment isn't guaranteed.
       *
       * The result SimdBlock4 is always 16-byte aligned (union attribute).
       *
       * @note Prefer this over manual arr[i] = _ptr[i] loops (SIMD is faster).
       * @note Source pointer alignment doesn't matter, unaligned access is safe.
       */
      LAMANCHA_INLINE static SimdBlock4 load(const f32* _ptr) {
        SimdBlock4 res{ 0 };
#if defined(__ARM_NEON)
        res.reg = vld1q_f32(_ptr);  // Vector load 1 quadword of f32
#elif defined(__SSE2__)
        res.reg = _mm_loadu_ps(_ptr);  // Load unaligned packed singles
#else
        for (i32 idx = 0; idx < 4; ++idx) { res.arr[idx] = _ptr[idx]; }
#endif
        return res;
      }

      /**
       * @brief Store 4 floats from this block to memory
       * @param _ptr Destination pointer (need not be 16-byte aligned)
       *
       * Uses unaligned store instructions for the same reason as load().
       * Destination may be an external buffer without alignment guarantees.
       *
       * @note Destination pointer alignment doesn't matter.
       * @note Writes exactly 16 bytes (4 * sizeof(f32))
       */
      LAMANCHA_INLINE void store(f32* _ptr) const {
#if defined(__ARM_NEON)
        vst1q_f32(_ptr, reg);       // Vector store 1 quadword of f32
#elif defined(__SSE2__)
        _mm_storeu_ps(_ptr, reg);   // Store unaligned packed singles
#else
        for (i32 idx = 0; idx < 4; ++idx) { _ptr[idx] = arr[idx]; }
#endif
      }
    };

    /**
     * @brief 128-bit SIMD block for u8x16 operations
     *
     * 16 meta bytes loaded and compared in one instruction to 
     * find key candidates without touching the heavier slots[] array.
     *
     * @note Distinct from SimdBlock4 (f32x4), shares register width
     * (128-bit) but uses integer comparison intrinsics throughout.
     */
    union LAMANCHA_SIMD_ALIGN SimdBlock16u8 {
      u8 arr[16];  ///< Scalar view: arr[i] = byte i

#ifndef LAMANCHA_SIMD_NONE
      lm_u8x16 reg;  ///< SIMD register view (128-bit integer register)
#endif

      /** @brief Load 16 bytes from ptr into a SimdBlock16u8 (unaligned) */
      LAMANCHA_INLINE static SimdBlock16u8 load(const u8* _ptr) {
        SimdBlock16u8 res{ 0 };
#if defined(__ARM_NEON)
        res.reg = vld1q_u8(_ptr);         // Vector load 1 quadword of u8
#elif defined(__SSE2__)
        res.reg = _mm_loadu_si128(        // Load unaligned 128-bit integer
          reinterpret_cast<const __m128i*>(_ptr));
#else
        for (i32 i = 0; i < 16; ++i) { res.arr[i] = _ptr[i]; }
#endif
        return res;
      }
    };

    /** 
     * @brief SIMD absolute value (4 lanes)
     * 
     * NEON: vabsq_f32 
     * SSE: _mm_set1_epi32, _mm_castsi128_ps, _mm_and_ps: 
     *
     * @param _val Input block
     * @return Absolute values
     */
    LAMANCHA_INLINE SimdBlock4 simd_abs(const SimdBlock4& _val)
    {
      SimdBlock4 result{ 0 };
#if defined(__ARM_NEON)
      result.reg = vabsq_f32(_val.reg);
#elif defined(__SSE2__)
      // Fill all 4 integer lanes with the same 32-bit value.
      // Reinterpret the integer register as a float register.
      lm_f32x4 sign_mask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF)); 
      result.reg = _mm_and_ps(_val.reg, sign_mask); // Bitwise AND all 4 float lanes with the mask
#else
      for (i32 i = 0; i < 4; ++i) { result.arr[i] = fAbs(_val.arr[i]); }
#endif
      return result;
    }

    /** @brief Bitwise modulo
     *  @param _x Dividend
     *  @param _y Divisor
     *  @return _x mod _y
     */
    LAMANCHA_INLINE i32 bwMod(i32 _x, i32 _y) { return _x & (_y - 1); }

    /** @brief Float modulo
     *  @param _x Dividend
     *  @param _y Divisor
     *  @return _x mod _y
     */
    LAMANCHA_INLINE f32 fMod(f32 _x, f32 _y) { return _x - f32(i32(_x / _y)) * _y; }

    /** @brief Wrap angle to (-pi, pi]
     *  @param _rad Input angle
     *  @return Wrapped angle
     */
    LAMANCHA_INLINE f32 fWrapPi(f32 _rad) {
      f32 res = fMod(_rad, TWO_PI);
      if (res > PI) { return res - TWO_PI; }
      if (res < -PI) { return res + TWO_PI; }
      return res;
    }
    /** @} */ // end of scalar_funcs group

    /** @defgroup sqrt_funcs Square Root Functions
     *  @{
     */

    /**
     * @brief Fast reciprocal square root (Quake III algorithm)
     *
     * Uses bit manipulation + Newton-Raphson refinement.
     * Magic constant 0x5f3759df from id Software, 1999.
     *
     * @param _x Input (must be > 0)
     * @return Approximate 1/sqrt(_x)
     */
    LAMANCHA_INLINE f32 fRSqrt(f32 _x) {
      f32 xhalf = 0.5f * _x;
      union { f32 f; i32 i; } val{ 0 };
      val.f = _x;
      val.i = 0x5f3759df - (val.i >> 1);  // bit-manipulation initial estimate
      _x = val.f;
      _x = _x * (1.5f - xhalf * _x * _x);  // one Newton-Raphson refinement
      // Add a second copy of the line above for higher accuracy if needed.
      return _x;
    }

    /**
     * @brief Fast square root via identity sqrt(x) = x * rsqrt(x)
     * @param _x Input
     * @return Approximate sqrt(_x)
     */
    LAMANCHA_INLINE f32 fSqrt(f32 _x) { return _x * fRSqrt(_x); }

    /** @} */ // end of sqrt_funcs gruop

    /** @defgroup minmax_funcs Min/Max/Clamp Functions
     *
     *  Scalar versions use the ternary operator. Modern compilers turn this into
     *  a conditional move (CMOV on x86, CSEL on ARM), a branchless instruction
     *  that avoids branch misprediction overhead.
     *
     *  @{
     */

    /** @brief Integer minimum */
    LAMANCHA_INLINE i32 iMin(i32 _a, i32 _b) { return (_a < _b) ? _a : _b; }

    /** @brief Integer maximum */
    LAMANCHA_INLINE i32 iMax(i32 _a, i32 _b) { return (_a > _b) ? _a : _b; }

    /** @brief Integer clamp to range [_lo, _hi] */
    LAMANCHA_INLINE i32 iClamp(i32 _val, i32 _lo, i32 _hi) { return iMin(iMax(_val, _lo), _hi); }

    /** @brief Float minimum */
    LAMANCHA_INLINE f32 fMin(f32 _a, f32 _b) { return (_a < _b) ? _a : _b; }

    /** @brief Float maximum */
    LAMANCHA_INLINE f32 fMax(f32 _a, f32 _b) { return (_a > _b) ? _a : _b; }

    /** @brief Float clamp to range [_lo, _hi] */
    LAMANCHA_INLINE f32 fClamp(f32 _val, f32 _lo, f32 _hi) { return fMin(fMax(_val, _lo), _hi); }

    /** @} */ // end of minmax_funcs group

/** @defgroup trig_funcs Trigonometric Functions
 *  @{
 */
#if USE_SIN_LUT

    constexpr i32 LUT_SIZE = 256;
    constexpr i32 LUT_SIZE_BITS = 256;
    constexpr i32 LUT_RANGE = LUT_SIZE - 1;
    constexpr i32 FULL_RANGE = (4 * LUT_SIZE) - 1;
    constexpr f32 PHASE_2_IDX = (4.f * LUT_SIZE) / TWO_PI;

    /** @brief Sine lookup table (256 entries, 0 to 2*pi)
     *
     *  @note Generated using Dr LUT - https://github.com/ppelikan/drlut
     *  @note Formula: sin(2*pi*t/T), T = LUT_SIZE * 4
     */
    constexpr f32 sinLUT[LUT_SIZE] = {
      0.0000000000f, 0.0061358846f, 0.0122715383f, 0.0184067299f, 0.0245412285f, 
      0.0306748032f, 0.0368072229f, 0.0429382569f, 0.0490676743f, 0.0551952443f, 
      0.0613207363f, 0.0674439196f, 0.0735645636f, 0.0796824380f, 0.0857973123f, 
      0.0919089565f, 0.0980171403f, 0.1041216339f, 0.1102222073f, 0.1163186309f, 
      0.1224106752f, 0.1284981108f, 0.1345807085f, 0.1406582393f, 0.1467304745f, 
      0.1527971853f, 0.1588581433f, 0.1649131205f, 0.1709618888f, 0.1770042204f, 
      0.1830398880f, 0.1890686641f, 0.1950903220f, 0.2011046348f, 0.2071113762f, 
      0.2131103199f, 0.2191012402f, 0.2250839114f, 0.2310581083f, 0.2370236060f,
      0.2429801799f, 0.2489276057f, 0.2548656596f, 0.2607941179f, 0.2667127575f, 
      0.2726213554f, 0.2785196894f, 0.2844075372f, 0.2902846773f, 0.2961508882f, 
      0.3020059493f, 0.3078496400f, 0.3136817404f, 0.3195020308f, 0.3253102922f, 
      0.3311063058f, 0.3368898534f, 0.3426607173f, 0.3484186802f, 0.3541635254f, 
      0.3598950365f, 0.3656129978f, 0.3713171940f, 0.3770074102f, 0.3826834324f, 
      0.3883450467f, 0.3939920401f, 0.3996241998f, 0.4052413140f, 0.4108431711f, 
      0.4164295601f, 0.4220002708f, 0.4275550934f, 0.4330938189f, 0.4386162385f, 
      0.4441221446f, 0.4496113297f, 0.4550835871f, 0.4605387110f, 0.4659764958f,
      0.4713967368f, 0.4767992301f, 0.4821837721f, 0.4875501601f, 0.4928981922f, 
      0.4982276670f, 0.5035383837f, 0.5088301425f, 0.5141027442f, 0.5193559902f, 
      0.5245896827f, 0.5298036247f, 0.5349976199f, 0.5401714727f, 0.5453249884f, 
      0.5504579729f, 0.5555702330f, 0.5606615762f, 0.5657318108f, 0.5707807459f,
      0.5758081914f, 0.5808139581f, 0.5857978575f, 0.5907597019f, 0.5956993045f, 
      0.6006164794f, 0.6055110414f, 0.6103828063f, 0.6152315906f, 0.6200572118f, 
      0.6248594881f, 0.6296382389f, 0.6343932842f, 0.6391244449f, 0.6438315429f, 
      0.6485144010f, 0.6531728430f, 0.6578066933f, 0.6624157776f, 0.6669999223f,
      0.6715589548f, 0.6760927036f, 0.6806009978f, 0.6850836678f, 0.6895405447f, 
      0.6939714609f, 0.6983762494f, 0.7027547445f, 0.7071067812f, 0.7114321957f, 
      0.7157308253f, 0.7200025080f, 0.7242470830f, 0.7284643904f, 0.7326542717f, 
      0.7368165689f, 0.7409511254f, 0.7450577854f, 0.7491363945f, 0.7531867990f, 
      0.7572088465f, 0.7612023855f, 0.7651672656f, 0.7691033376f, 0.7730104534f, 
      0.7768884657f, 0.7807372286f, 0.7845565972f, 0.7883464276f, 0.7921065773f, 
      0.7958369046f, 0.7995372691f, 0.8032075315f, 0.8068475535f, 0.8104571983f,
      0.8140363297f, 0.8175848132f, 0.8211025150f, 0.8245893028f, 0.8280450453f, 
      0.8314696123f, 0.8348628750f, 0.8382247056f, 0.8415549774f, 0.8448535652f, 
      0.8481203448f, 0.8513551931f, 0.8545579884f, 0.8577286100f, 0.8608669386f, 
      0.8639728561f, 0.8670462455f, 0.8700869911f, 0.8730949784f, 0.8760700942f, 
      0.8790122264f, 0.8819212643f, 0.8847970984f, 0.8876396204f, 0.8904487232f,
      0.8932243012f, 0.8959662498f, 0.8986744657f, 0.9013488470f, 0.9039892931f, 
      0.9065957045f, 0.9091679831f, 0.9117060320f, 0.9142097557f, 0.9166790599f, 
      0.9191138517f, 0.9215140393f, 0.9238795325f, 0.9262102421f, 0.9285060805f, 
      0.9307669611f, 0.9329927988f, 0.9351835099f, 0.9373390119f, 0.9394592236f, 
      0.9415440652f, 0.9435934582f, 0.9456073254f, 0.9475855910f, 0.9495281806f, 
      0.9514350210f, 0.9533060404f, 0.9551411683f, 0.9569403357f, 0.9587034749f, 
      0.9604305194f, 0.9621214043f, 0.9637760658f, 0.9653944417f, 0.9669764710f, 
      0.9685220943f, 0.9700312532f, 0.9715038910f, 0.9729399522f, 0.9743393828f, 
      0.9757021300f, 0.9770281427f, 0.9783173707f, 0.9795697657f, 0.9807852804f, 
      0.9819638691f, 0.9831054874f, 0.9842100924f, 0.9852776424f, 0.9863080972f, 
      0.9873014182f, 0.9882575677f, 0.9891765100f, 0.9900582103f, 0.9909026354f, 
      0.9917097537f, 0.9924795346f, 0.9932119492f, 0.9939069700f, 0.9945645707f, 
      0.9951847267f, 0.9957674145f, 0.9963126122f, 0.9968202993f, 0.9972904567f, 
      0.9977230666f, 0.9981181129f, 0.9984755806f, 0.9987954562f, 0.9990777278f, 
      0.9993223846f, 0.9995294175f, 0.9996988187f, 0.9998305818f, 0.9999247018f, 
      0.9999811753f };

    /**
     * @brief Helper indexing function for sine look-up table 
     */
    inline float getByIdxLUT(i32 _index)
    {
      i32 quadrant = _index >> LUT_SIZE_BITS;
      i32 offset = _index & LUT_RANGE;

      switch (quadrant)
      {
        case 1: return sinLUT[LUT_SIZE - offset];
        case 2: return -sinLUT[offset];
        case 3: return -sinLUT[LUT_SIZE - offset];
        default: return sinLUT[offset];
      }
    }

    /**
     * @brief Fast sine via lookup table with linear interpolation
     * @param _rad Angle in radians
     * @see https://zipcpu.com/dsp/2017/08/26/quarterwave.html
     */
    LAMANCHA_INLINE f32 fSin(f32 _rad) 
    {
      f32 norm = fMod(_rad, TWO_PI);       // wrap to (-2pi, 2pi)
      if (norm < 0.f) { norm += TWO_PI; }  // shift to [0, 2pi)

      f32 phase = norm * PHASE_2_IDX;

      i32 idx = static_cast<i32>(phase);
      f32 frac = phase - static_cast<f32>(idx);
      
      f32 sinLow = getByIdxLUT(idx & FULL_RANGE);
      f32 sinHigh = getByIdxLUT((idx + 1) & FULL_RANGE);
      
      return sinLow + frac * (sinHigh - sinLow);
    }

    /**
     * @brief Fast cosine via lookup table with linear interpolation
     * @param _rad Angle in radians
     * @see https://zipcpu.com/dsp/2017/08/26/quarterwave.html
     */
    LAMANCHA_INLINE f32 fCos(f32 _rad) { return fSin(_rad + HALF_PI); }

    /**
     * @brief Simple arc tangent via sin/cos
     * @param _rad Angle in radians
     * @see https://zipcpu.com/dsp/2017/08/26/quarterwave.html
     */
    LAMANCHA_INLINE f32 fTan(f32 _rad) 
    {
      f32 norm = fMod(_rad, TWO_PI); // wrap to (-2pi, 2pi)
      if (norm < 0.f) { norm += TWO_PI; }   // shift to [0, 2pi)

      f32 phase = norm * PHASE_2_IDX;

      i32 idx = static_cast<i32>(phase);
      f32 frac = phase - static_cast<f32>(idx);

      f32 sinLow = getByIdxLUT(idx & FULL_RANGE);
      f32 sinHigh = getByIdxLUT((idx + 1) & FULL_RANGE);

      i32 cosIdxDiff = static_cast<i32>(HALF_PI * PHASE_2_IDX);
      f32 cosLow = getByIdxLUT((idx + cosIdxDiff) & FULL_RANGE);
      f32 cosHigh = getByIdxLUT((idx + cosIdxDiff + 1) & FULL_RANGE);

      f32 sinInterp = sinLow + frac * (sinHigh - sinLow);
      f32 cosInterp = cosLow + frac * (cosHigh - cosLow);

      return sinInterp / cosInterp;
    }

#else

   /**
    * @brief Fast sine via 5th-order polynomial
    * @param _rad Angle in radians
    * @return Angle in radians (-pi to pi)
    */
    LAMANCHA_INLINE f32 fSin(f32 _rad) {
      const f32 B = 4.0f / PI;          // slope coefficient
      const f32 C = -4.0f / (PI * PI);  // curvature coefficient
      f32 y = B * _rad + C * _rad * fAbs(_rad);
      const f32 P = 0.225f;             // precision correction factor
      // P*(y*|y| - y) + y refines the parabolic approximation into a better sinusoid.
      return P * (y * fAbs(y) - y) + y;
    }

    /**
     * @brief Fast cosine via phase-shifted sine
     * @param _rad Angle in radians
     * @return cos(x) = sin(x + pi/2), angle in radians (-pi to pi)
     */
    LAMANCHA_INLINE f32 fCos(f32 _rad) { return fSin(_rad + HALF_PI); }
    
    /**
     * @brief Fast cosine via phase-shifted sine
     * @param _rad Angle in radians
     * @return tan(x) = sin(x) / sin(x + pi/2), angle in radians (-pi to pi)
     */
    LAMANCHA_INLINE f32 fTan(f32 _rad) { return fSin(_rad) / fSin(_rad + HALF_PI); }
#endif

    /**
     * @brief Arctangent of y/x with correct quadrant
     * @param _y Numerator
     * @param _x Denominator
     * @return Angle in radians (-pi to pi)
     */
    LAMANCHA_INLINE f32 fAtan2(f32 _y, f32 _x) {
      if (_x == 0.f) {
        if (_y > 0.f) { return HALF_PI; }
        if (_y < 0.f) { return -HALF_PI; }
        return 0.f; // undefined
      }
      f32 t = _y / _x;
      bool flipped = fAbs(t) > 1.f;
      if (flipped) { t = 1.f / t; }

      f32 t2 = t * t;
      f32 atan = t * (1.f - t2 * (1.f / 3.f - t2 * (1.f / 5.f - t2 * 1.f / 7.f)));

      if (flipped) { atan = (t > 0.f ? HALF_PI : -HALF_PI) - atan; }
      if (_x < 0.f) { atan += (_y >= 0.f ? PI : -PI); }
      return atan;
    }

    /**
     * @brief Arcsine
     * @param _x Input in [-1, 1]
     * @return Angle in radians (-pi/2 to pi/2)
     */
    LAMANCHA_INLINE f32 fAsin(f32 _x)
    {
      _x = fClamp(_x, -1.f, 1.f);
      return fAtan2(_x, fSqrt(1.f - _x * _x));
    }

    /**
     * @brief Arccosine
     * @param _x Input in [-1, 1]
     * @return Angle in radians (0 to pi)
     */
    LAMANCHA_INLINE f32 fAcos(f32 _x)
    {
      _x = fClamp(_x, -1.f, 1.f);
      return fAtan2(fSqrt(1.f - _x * _x), _x);
    }

    /** @} */ // end of trig_funcs group

    /**
     * @brief Broadcast a scalar to all 4 lanes of a SimdBlock4
     * @param _val Scalar value to replicate
     * @return SimdBlock4 with all lanes set to _val
     *
     * @code
     * SimdBlock4 vec = SimdBlock4::load(data);
     * SimdBlock4 scalar = broadcast(2.5f);
     * SimdBlock4 result = simd_fma(vec, scalar, zero);  // vec * 2.5
     * @endcode
     *
     * @note Result: res.arr[0] == res.arr[1] == res.arr[2] == res.arr[3] == _val
     */
    LAMANCHA_INLINE SimdBlock4 broadcast(f32 _val) {
      SimdBlock4 result{ 0 };
#if defined(__ARM_NEON)
      result.reg = vdupq_n_f32(_val);  // Duplicate to quad (n = immediate source)
#elif defined(__SSE2__)
      result.reg = _mm_set1_ps(_val);  // Set all lanes to 1 value
#else
      result.arr[0] = result.arr[1] = result.arr[2] = result.arr[3] = _val;
#endif
      return result;
    }

    /**
     * @brief Sum all 4 lanes of a SimdBlock4 to a scalar
     * @param _val Input block with 4 float lanes
     * @return Scalar sum: arr[0] + arr[1] + arr[2] + arr[3]
     *
     * Performs horizontal reduction (lanes -> scalar). Essential for dot products
     * and lengthSq computation. After multiplying two vectors component-wise,
     * the result [x1*x2, y1*y2, z1*z2, w1*w2] needs to be summed to get the
     * final dot product scalar.
     *
     * @note For vec3, lane 3 is padding (zero), contributing zero to sum
     */
    LAMANCHA_INLINE f32 horizontal_sum(const SimdBlock4& _val) {
#if defined(__ARM_NEON)
      float32x2_t lo = vget_low_f32(_val.reg);    // [a, b]
      float32x2_t hi = vget_high_f32(_val.reg);   // [c, d]
      float32x2_t sum_pair = vpadd_f32(lo, hi);   // [a+b, c+d]
      sum_pair = vpadd_f32(sum_pair, sum_pair);   // [a+b+c+d, a+b+c+d]
      return vget_lane_f32(sum_pair, 0);          // Extract lane 0

#elif defined(__SSE2__)
      lm_f32x4 shuf = _mm_shuffle_ps(_val.reg, _val.reg, _MM_SHUFFLE(2, 3, 0, 1));
      lm_f32x4 sums = _mm_add_ps(_val.reg, shuf);    // [a+b, a+b, c+d, c+d]
      shuf = _mm_movehl_ps(shuf, sums);              // Move high 64 bits: [c+d, c+d, ?, ?]
      sums = _mm_add_ss(sums, shuf);                 // Scalar add lane 0: a+b+c+d
      return _mm_cvtss_f32(sums);                    // Extract lane 0 as f32

#else
      return _val.arr[0] + _val.arr[1] + _val.arr[2] + _val.arr[3];
#endif
    }

    /**
     * @brief Fused Multiply-Add: (a * b) + c for all 4 lanes
     * @param _a First multiplicand (4 floats)
     * @param _b Second multiplicand (4 floats)
     * @param _c Addend (4 floats)
     * @return Result: [a0*b0+c0, a1*b1+c1, a2*b2+c2, a3*b3+c3]
     *
     * On hardware with native FMA support (ARM NEON vmlaq), this is a single
     * instruction with only one rounding step. 
     * On SSE2 (no native FMA), it decomposes to multiply + add (two rounds).
     *
     * @code
     * SimdBlock4 vec = SimdBlock4::load(data);
     * SimdBlock4 scalar = broadcast(2.5f);
     * SimdBlock4 zero{0, 0, 0, 0};
     * SimdBlock4 result = simd_fma(vec, scalar, zero);  // (vec * 2.5) + 0
     * @endcode
     * 
     * @note Order of arguments matches hardware: c + (a * b)
     * @note Numerically superior to separate operations (one rounding vs two)
     */
    LAMANCHA_INLINE SimdBlock4 simd_fma(const SimdBlock4& _a, const SimdBlock4& _b, const SimdBlock4& _c) {
      SimdBlock4 result{ 0 };
#if defined(__ARM_NEON)
      result.reg = vmlaq_f32(_c.reg, _a.reg, _b.reg);  // c + (a * b), native FMA
#elif defined(__SSE2__)
      lm_f32x4 prod = _mm_mul_ps(_a.reg, _b.reg);      // a * b
      result.reg = _mm_add_ps(prod, _c.reg);           // + c
#else
      for (i32 idx = 0; idx < 4; ++idx)
      {
        result.arr[idx] = _a.arr[idx] * _b.arr[idx] + _c.arr[idx];
      }
#endif
      return result;
    }

    /**
     * @brief Fast reciprocal estimate: 1/x for all 4 lanes
     * @param _val Input values (4 floats)
     * @return Approximate reciprocals (4 floats)
     *
     * Computes 1/x using a hardware lookup table followed by a Newton-Raphson
     * seed, avoiding the division instruction. This trades precision for speed.
     *
     * Full float precision (about 23 bits) when applying one Newton-Raphson refinement:
     * `refined = approx * (2 - x * approx)`
     *
     * @code
     * SimdBlock4 denominators = SimdBlock4::load(data);
     * SimdBlock4 reciprocals = simd_recip_approx(denominators);
     * // Now multiply instead of divide (much faster)
     * SimdBlock4 quotients = simd_fma(numerators, reciprocals, zero);
     * @endcode
     *
     * @note For graphics/animation, the estimate alone is usually sufficient
     * @note Division by zero produces infinity (IEEE 754 behavior)
     */
    LAMANCHA_INLINE SimdBlock4 simd_recip_approx(const SimdBlock4& _val) {
      SimdBlock4 result{ 0 };
#if defined(__ARM_NEON)
      result.reg = vrecpeq_f32(_val.reg);  // Reciprocal estimate, quad, f32
#elif defined(__SSE2__)
      result.reg = _mm_rcp_ps(_val.reg);   // Reciprocal, packed singles
#else
      for (i32 idx = 0; idx < 4; ++idx)
      {
        result.arr[idx] = 1.0f / _val.arr[idx];  // Exact division (no approximation)
      }
#endif
      return result;
    }

    /**
     * @brief Broadcast a single byte to all 16 lanes
     * @param _val Byte to replicate
     *
     * Used to build the comparison target: @c broadcast(H2) produces [H2, H2, H2, H2] 
     * which is then compared against the meta group to find all candidate slots 
     * in one instruction.
     */
    LAMANCHA_INLINE static SimdBlock16u8 broadcast(u8 _val) {
      SimdBlock16u8 res{ 0 };
#if defined(__ARM_NEON)
      res.reg = vdupq_n_u8(_val);       // Duplicate byte to all 16 lanes
#elif defined(__SSE2__)
      res.reg = _mm_set1_epi8(          // Set all 16 byte lanes to _val
        static_cast<char>(_val));
#else
      for (i32 i = 0; i < 16; ++i) { res.arr[i] = _val; }
#endif
      return res;
    }

    /**
     * @brief Compare two SimdBlock16u8 for equality, lane by lane
     * @return SimdBlock16u8 where each lane is 0xFF (equal) or 0x00 (not equal)
     *
     * Result feeds directly into @c extractMask() to get a bitmask of matching slots.
     */
    LAMANCHA_INLINE static SimdBlock16u8 cmpeq(
      const SimdBlock16u8& _a, const SimdBlock16u8& _b)
    {
      SimdBlock16u8 res{ 0 };
#if defined(__ARM_NEON)
      res.reg = vceqq_u8(_a.reg, _b.reg);  // Compare equal, u8, quadword
#elif defined(__SSE2__)
      res.reg = _mm_cmpeq_epi8(            // Compare packed bytes for equality
        _a.reg, _b.reg);
#else
      for (i32 i = 0; i < 16; ++i)
      {
        res.arr[i] = (_a.arr[i] == _b.arr[i]) ? 0xFF : 0x00;
      }
#endif
      return res;
    }

    /**
     * @brief Collapse 16 comparison lanes into a u16 bitmask
     * @param _cmp Result of @c cmpeq() (each lane is 0xFF or 0x00)
     * @return u16 where bit i is set if lane i was 0xFF
     *
     * SSE2: _mm_movemask_epi8
     * NEON: shift-and-narrow trick
     *
     * @note Caller iterates set bits with __builtin_ctzll / scalar loop
     */
    LAMANCHA_INLINE static u16 extractMask(const SimdBlock16u8& _cmp) {
#if defined(__ARM_NEON)
      // Shift each 0xFF lane to 0x0F, 0x00 stays 0x00
      // This compresses the match signal into the low nibble of each byte
      uint8x16_t shifted = vshrq_n_u8(_cmp.reg, 4);

      // Reinterpret 16 bytes as 8 u16s, then narrow each u16
      // down to u8 by taking the low byte, effectively packing pairs
      // of nibbles: 
      // byte[i] = (lane[2i] nibble) | (lane[2i+1] nibble << 4)
      uint8x8_t narrowed = vshrn_n_u16(vreinterpretq_u16_u8(shifted), 0);  // narrow without extra shift

      // Reinterpret 8-byte result as u64 for compact bit scanning
      // Each set nibble in this u64 corresponds to one matched group slot.
      // Bit layout: nibble 0 = slot 0 & 1, nibble 1 = slot 2 & 3, etc.
      u64 bits = vget_lane_u64(vreinterpret_u64_u8(narrowed), 0);

      // Expand nibble bitmask back to a u16 slot bitmask
      // Each nibble encodes two slots; unpack into individual bits
      u16 mask = 0;
      for (i32 i = 0; i < 8; i++)
      {
        u8 nibble = (bits >> (i * 8)) & 0xFF;
        if (nibble & 0x0F) { mask |= (1u << (i * 2)); }      // even slot
        if (nibble & 0xF0) { mask |= (1u << (i * 2 + 1)); }  // odd slot
      }
      return mask;

#elif defined(__SSE2__)
      // Collapses high bit of each byte lane into a u16
      // bit i = high bit of byte lane i from _cmp.reg
      // Since cmpeq produces 0xFF (all bits set) or 0x00, high bit = match
      return static_cast<u16>(_mm_movemask_epi8(_cmp.reg));

#else
      // Scalar fallback: manually build bitmask from arr[]
      u16 mask = 0;
      for (i32 i = 0; i < 16; ++i)
      {
        if (_cmp.arr[i] == 0xFF) { mask |= (1u << i); }
      }
      return mask;
#endif
    }

    /**
     * @brief SIMD reciprocal square root (4 lanes)
     * @param _val Input block
     * @return Approximate 1/sqrt for each lane, ~12-bit accuracy
     * @note Refine with Newton-Raphson if higher precision is needed
     */
    LAMANCHA_INLINE SimdBlock4 simd_rsqrt_approx(const SimdBlock4& _val)
    {
      SimdBlock4 result{ 0 };
#if defined(__ARM_NEON)
      result.reg = vrsqrteq_f32(_val.reg);
#elif defined(__SSE2__)
      result.reg = _mm_rsqrt_ps(_val.reg);
#else
      for (i32 i = 0; i < 4; ++i) { result.arr[i] = fRSqrt(_val.arr[i]); }
#endif
      return result;
    }

    /** @defgroup log_exp_funcs Logarithm and Exponential Functions
     *
     *  These exploit the same IEEE 754 bit-layout insight as fRSqrt. For a float x:
     *  raw_bits(x) ~= 2^23 * (log2(x) + 127)
     *  Therefore: log2(x) ~= raw_bits(x) * 2^-23 - 127
     *  @{
     */

     /**
      * @brief Fast usize-based log base-2 via bit
      * @param _x Input (must be > 0)
      */
    LAMANCHA_INLINE usize uLog2(usize _x) 
    {
      usize r = 0;
      while (_x >>= 1) { r++; }
      return r;
    }

    /**
     * @brief Fast log base-2 via bit manipulation by treating float bits as a scaled integer
     * @param _x Input (must be > 0)
     * @return Approximate log2(_x)
     */
    LAMANCHA_INLINE f32 fLog2(f32 _x) 
    {
      union { f32 f; i32 i; } val{ 0 };
      val.f = _x;
      return f32(val.i) * MACHINE_EPSILON - 127.0f;
    }

    /**
     * @brief Fast natural logarithm
     * @param _x Input (must be > 0)
     * @return Approximate ln(_x)
     * @note log(x) = log2(x) * ln(2).  One multiply converts base-2 to natural log
     */
    LAMANCHA_INLINE f32 fLog(f32 _x) { return fLog2(_x) * LN_2; }

    /**
     * @brief Fast 2^x via bit manipulation
     * @param _x Exponent
     * @return Approximate 2^_x
     * @note The clamp at -126 prevents underflow into denormal numbers
     */
    LAMANCHA_INLINE f32 fExp2(f32 _x) 
    {
      f32 clipp = (_x < -126.f) ? -126.f : _x;
      union { f32 f; i32 i; } val{ 0 };
      val.i = i32((1 << 23) * (clipp + 127.f));  // reconstruct bits from exponent
      return val.f;
    }

    /**
     * @brief Fast e^x
     * @param _x Exponent
     * @return Approximate e^_x
     * @note e^x = 2^(x * log2(e)). Converts natural exponent to base-2
     */
    LAMANCHA_INLINE f32 fExp(f32 _x) { return fExp2(_x * L2_E); }

    /**
     * @brief Fast power: a^b
     * @param _a Base
     * @param _b Exponent
     * @return Approximate _a^_b
     * @note a^b = 2^(b * log2(a)).  General power via log/exp identity
     */
    LAMANCHA_INLINE f32 fPow(f32 _a, f32 _b) { return fExp2(_b * fLog2(_a)); }

    /** @} */ // end of log_exp_funcs group

    /** @brief SIMD minimum (component-wise, 4 lanes) */
    LAMANCHA_INLINE SimdBlock4 simd_min(const SimdBlock4& _a, const SimdBlock4& _b)
    {
      SimdBlock4 result{ 0 };
#if defined(__ARM_NEON)
      result.reg = vminq_f32(_a.reg, _b.reg);
#elif defined(__SSE2__)
      result.reg = _mm_min_ps(_a.reg, _b.reg);
#else
      for (i32 i = 0; i < 4; ++i) { result.arr[i] = fMin(_a.arr[i], _b.arr[i]); }
#endif
      return result;
    }

    /** @brief SIMD maximum (component-wise, 4 lanes) */
    LAMANCHA_INLINE SimdBlock4 simd_max(const SimdBlock4& _a, const SimdBlock4& _b)
    {
      SimdBlock4 result{ 0 };
#if defined(__ARM_NEON)
      result.reg = vmaxq_f32(_a.reg, _b.reg);
#elif defined(__SSE2__)
      result.reg = _mm_max_ps(_a.reg, _b.reg);
#else
      for (i32 i = 0; i < 4; ++i) { result.arr[i] = fMax(_a.arr[i], _b.arr[i]); }
#endif
      return result;
    }

    /** @brief SIMD clamp (component-wise, 4 lanes) */
    LAMANCHA_INLINE SimdBlock4 simd_clamp(const SimdBlock4& _val, const SimdBlock4& _min, const SimdBlock4& _max)
    {
      // max(val, min_bound) followed by min(result, max_bound).
      // Two SIMD instructions replace 4 scalar comparisons + 4 branches.
      return simd_min(simd_max(_val, _min), _max);
    }
  }
} 