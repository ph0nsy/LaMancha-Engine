#pragma once
#include "core/pch.h"
#include "core/math/vec.h"

namespace LaMancha {
  struct Color {
    u8 r, g, b, a;

    Color() : r(255), g(255), b(255), a(255) {}
    Color(u8 _r, u8 _g, u8 _b, u8 _a = 255) : r(_r), g(_g), b(_b), a(_a) {}

    static Color fromFloat(f32 _r, f32 _g, f32 _b, f32 _a = 1.0f) {
      return Color(static_cast<u8>(Math::fClamp(_r, 0.f, 1.f) * 255.f),
        static_cast<u8>(Math::fClamp(_g, 0.f, 1.f) * 255.f),
        static_cast<u8>(Math::fClamp(_b, 0.f, 1.f) * 255.f),
        static_cast<u8>(Math::fClamp(_a, 0.f, 1.f) * 255.f)
      );
    }

    static Color fromU32(u32 _color) {
      return Color(
        (_color >> 24) & U8_MAX,  // R
        (_color >> 16) & U8_MAX,  // G
        (_color >> 8) & U8_MAX,   // B
        _color & U8_MAX           // A
      );
    }

    u32 toU32() const { return (r << 24) | (g << 16) | (b << 8) | a; }
  };

  struct ColorHDR {
    Math::vec4 rgba;  ///< r=x, g=y, b=z, a=w (no upper bound on channel values)

    ColorHDR() : rgba{ 0 } {}
    ColorHDR(f32 _r, f32 _g, f32 _b, f32 _a = 1.f) : rgba{ _r, _g, _b, _a } {}

    // Construct from LDR Color - unpacks u8 channels to [0,1] float range
    static ColorHDR fromColor(const Color& _c) {
      constexpr f32 inv255 = 1.f / 255.f;
      return ColorHDR(_c.r * inv255, _c.g * inv255, _c.b * inv255, _c.a * inv255);
    }

    // Clamp to [0,1] and pack to LDR Color - loses HDR data above 1.0
    Color toColor() const {
      return Color::fromFloat(rgba[0], rgba[1], rgba[2], rgba[3]);
    }

    // Tonemap via reinhard: v / (v + 1) per channel - maps [0,inf) to [0,1)
    ColorHDR tonemapReinhard() const {
      ColorHDR out;
      for (i32 i = 0; i < 4; i++) { out.rgba[i] = rgba[i] / (rgba[i] + 1.f); }
      return out;
    }

    // SIMD lerp via fma: result = a + t*(b-a)
    ColorHDR lerp(const ColorHDR& _other, f32 _t) const {
      ColorHDR out;
      Math::SimdBlock4 a = Math::SimdBlock4::load(rgba.data);
      Math::SimdBlock4 b = Math::SimdBlock4::load(_other.rgba.data);
      Math::SimdBlock4 t = Math::broadcast(_t);
      Math::SimdBlock4 d{ 0 };                         // diff = b - a
      for (i32 i = 0; i < 4; i++) { d.arr[i] = b.arr[i] - a.arr[i]; }
      Math::SimdBlock4 result = simd_fma(t, d, a); // a + t*d
      result.store(out.rgba.data);
      return out;
    }

    ColorHDR operator+(const ColorHDR& _o) const { return { rgba[0] + _o.rgba[0], rgba[1] + _o.rgba[1], rgba[2] + _o.rgba[2], rgba[3] + _o.rgba[3] }; }
    ColorHDR operator*(f32 _s)             const { return { rgba[0] * _s, rgba[1] * _s, rgba[2] * _s, rgba[3] * _s }; }
  };
}