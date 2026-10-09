/**
 * @file geometry.h
 * @brief Geometry primititves for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 */

#pragma once

#include "mat.h"

namespace LaMancha {
  namespace Math {
    namespace Geometry {
      /** @brief Axis-Aligned Bounding Box: defined by its four screen-space edges.
       *
       *  @note Used for broad-phase collision detection and UI layout
       */
      struct AABB {
        f32 left, right, top, bottom;
      };

      /** @brief Defined by a single radius. */
      struct Circle {
        f32 radius;

        /** @brief Area computed via pi*r^2. */
        LAMANCHA_INLINE f32 area() const { return PI * radius * radius; }
      };

      /** @brief Defined by width and height. */
      struct Rect {
        f32 width, height;

        /** @brief Area computed via w*h. */
        LAMANCHA_INLINE f32 area() const { return width * height; }
      };

      /** @brief Defined by 3 vertices stored as a 3x2 matrix (3 rows of 2D points). */
      struct Triangle {
        mat3_2 vtx;

        /** @brief Area computed via the signed cross-product (shoelace formula): area = 0.5 * |x0*(y1-y2) + x1*(y2-y0) + x2*(y0-y1)|.
         *
         *  @note The result is not always positive because we do not take the absolute value here.
         */
        LAMANCHA_INLINE f32 signedArea() const {
          return 0.5f * (vtx[0][0] * (vtx[1][1] - vtx[2][1]) +
            vtx[1][0] * (vtx[2][1] - vtx[0][1]) +
            vtx[2][0] * (vtx[0][1] - vtx[1][1]));
        }

        /** @brief Absolute area. */
        LAMANCHA_INLINE f32 area() const { return fAbs(signedArea()); }

        /**
         * @brief Check if polygon winding is counter-clockwise
         * @return true if CCW, false if CW or degenerate
         */
        LAMANCHA_INLINE bool isCounterClockwise() const { return signedArea() > 0.f; }
      };

      /** @brief Defined by N vertices as 2D points. */
      template <usize N>
      struct Polygon {
        static_assert(N >= 3, "Polygon must have at least 3 vertices");
        Mat<N, 2> vtx;

        /** @brief Signed area computed via the shoelace formula: sum(x_i*y_{i+1}) - sum(y_i*x_{i+1}) all divided by 2. */
        LAMANCHA_INLINE f32 signedArea() const {
          f32 x = 0.f, y = 0.f;
          for (usize idx = 0; idx < N - 1; ++idx) {
            x += vtx[idx][0] * vtx[idx + 1][1];
            y += vtx[idx][1] * vtx[idx + 1][0];
          }
          // Wrap around
          x += vtx[N - 1][0] * vtx[0][1];
          y += vtx[N - 1][1] * vtx[0][0];

          return (x - y) * 0.5f;
        }
        /** @brief Absolute area. */
        LAMANCHA_INLINE f32 area() const { return fAbs(signedArea()); }

        /**
         * @brief Check if polygon winding is counter-clockwise
         * @return true if CCW, false if CW or degenerate
         */
        LAMANCHA_INLINE bool isCounterClockwise() const { return signedArea() > 0.f; }
      };
    }
  }
}