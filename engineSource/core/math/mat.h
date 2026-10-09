/**
 * @file mat.h
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 */

#pragma once

#include "vec.h"

namespace LaMancha {
  namespace Math {
    /**
     * @brief RxC matrix with SIMD support for 3x3 and 4x4
     * @tparam R Number of rows
     * @tparam C Number of columns
     *
     * Row-major storage (array of Vec<C> rows).
     * SIMD matrix multiply for mat3_3 and mat4_4.
     *
     * @note For column-vector convention (M * v, used by OpenGL/GLSL), transpose before uploading to GPU.
     *
     * Example:
     * @code
     * mat4_4 m = i4_4;  // Identity
     * m[0][0] = 2.f;    // Scale X by 2
     * vec4 v{1,2,3,1};
     * vec4 result = m * v;
     * @endcode
     */
    template <usize R, usize C>
    struct Mat {
      Vec<C> data[R]{ 0 };   ///< R rows, each a Vec<C>
      usize rows = R;
      usize cols = C;

      /** @brief Access row */
      LAMANCHA_INLINE Vec<C>& operator[](usize _idx) {
#if LAMANCHA_DEBUG
        LAMANCHA_ASSERT(_idx < R && _idx >= 0, "Mat<R,C> > Row index out of bounds.");
#endif
        return data[_idx];
      }

      LAMANCHA_INLINE const Vec<C>& operator[](usize _idx) const {
#if LAMANCHA_DEBUG
        LAMANCHA_ASSERT(_idx < R && _idx >= 0, "Mat<R,C> > Row index out of bounds.");
#endif
        return data[_idx];
      }

      //  operator+ / -
      //
      //  Matrix addition is element-wise: C[i][j] = A[i][j] + B[i][j].
      //  We delegate to Vec::operator+/- which is already SIMD for C=3/4.
      //  This means mat4_4 addition is 4 SIMD adds (one per row) - essentially free.

      LAMANCHA_INLINE Mat<R, C> operator+(const Mat<R, C>& _mat) const {
        Mat<R, C> m;
        for (usize r = 0; r < R; r++) { m.data[r] = data[r] + _mat.data[r]; }
        return m;
      }
      LAMANCHA_INLINE Mat<R, C>& operator+=(const Mat<R, C>& _mat) {
        for (usize r = 0; r < R; r++) { data[r] += _mat.data[r]; }
        return *this;
      }
      LAMANCHA_INLINE Mat<R, C> operator-(const Mat<R, C>& _mat) const {
        Mat<R, C> m;
        for (usize r = 0; r < R; r++) { m.data[r] = data[r] - _mat.data[r]; }
        return m;
      }
      LAMANCHA_INLINE Mat<R, C>& operator-=(const Mat<R, C>& _mat) {
        for (usize r = 0; r < R; r++) { data[r] -= _mat.data[r]; }
        return *this;
      }

      //  operator* (matrix x matrix)
      //
      //  The standard matrix multiply: C[i][j] = sum_k A[i][k] * B[k][j].
      //  The loop order i-j-k is the naive "inner product" order - simple but
      //  cache-friendly for row-major storage. The k-j inner loop walks B
      //  row by row, which is sequential in memory.
      //
      //  template <usize K>: allows multiplying non-square matrices as long as
      //  A has C columns and B has C rows (A is RxC, B is CxK, result is RxK).

      template <usize K>
      LAMANCHA_INLINE Mat<R, K> operator*(const Mat<C, K>& _mat) const {
        Mat<R, K> m{};
        for (usize i = 0; i < R; i++) {
          for (usize j = 0; j < C; j++) {
            for (usize k = 0; k < K; k++) {
              m.data[i][k] += data[i][j] * _mat.data[j][k];
            }
          }
        }
        return m;
      }
      LAMANCHA_INLINE Mat<R, C>& operator*=(const Mat<C, C>& _mat) {
        // Must go through a temporary because computing result[i] reads rows
        // of *this that are also being overwritten if we mutate in place.
        Mat<R, C> result = (*this) * _mat;
        *this = result;
        return *this;
      }

      //  operator* (matrix x scalar)
      //
      //  Scales every element. Each row is a Vec<C>, and Vec::operator*(f32)
      //  is SIMD for C=3/4. So scaling a mat4_4 is 4 SIMD broadcasts + 4 multiplies.

      LAMANCHA_INLINE Mat<R, C> operator*(f32 _scalar) const {
        Mat<R, C> m{};
        for (usize r = 0; r < R; r++) { m.data[r] = data[r] * _scalar; }
        return m;
      }
      LAMANCHA_INLINE Mat<R, C>& operator*=(f32 _scalar) {
        for (usize r = 0; r < R; r++) { data[r] *= _scalar; }
        return *this;
      }

#if DIV_OPD_USE
      //  operator/ (matrix / matrix)
      //
      //  A / B is defined as A * B^-1 (right-division).
      //  This is expensive: inverse is O(n^3) Gauss-Jordan elimination.
      //  Use explicit .inverse() calls on hot paths to make the cost obvious.
      //  This operator exists for completeness and editor tooling.

      LAMANCHA_INLINE Mat<R, C> operator/(const Mat<C, C>& _mat) const {
        Mat<C, C> inv = _mat.inverse();
        Mat<R, C> m{};
        for (usize i = 0; i < R; i++) {
          for (usize j = 0; j < C; j++) {
            for (usize k = 0; k < C; k++) {
              m.data[i][k] += data[i][j] * inv.data[j][k];
            }
          }
        }
        return m;
      }
      LAMANCHA_INLINE Mat<R, C>& operator/=(const Mat<C, C>& _mat) {
        Mat<C, C> inv = _mat.inverse();
        Mat<R, C> m{};
        for (usize i = 0; i < R; i++) {
          for (usize j = 0; j < C; j++) {
            for (usize k = 0; k < C; k++) {
              m.data[i][k] += data[i][j] * inv.data[j][k];
            }
          }
        }
        *this = m;
        return *this;
      }

      //  operator/ (matrix / scalar)
      //
      //  Compute reciprocal once, then multiply each row (via Vec::operator*(f32)).
      //  One division + R SIMD multiplies is cheaper than R * C scalar divisions.

      LAMANCHA_INLINE Mat<R, C> operator/(f32 _scalar) const {
        f32 inv = 1.0f / _scalar;
        Mat<R, C> m{};
        for (usize r = 0; r < R; r++) { m.data[r] = data[r] * inv; }
        return m;
      }
      LAMANCHA_INLINE Mat<R, C>& operator/=(f32 _scalar) {
        f32 inv = 1.0f / _scalar;
        for (usize r = 0; r < R; r++) { data[r] *= inv; }
        return *this;
      }
#endif

      /** @brief Swap two rows */
      LAMANCHA_INLINE void swapRows(usize _rowA, usize _rowB) {
#if LAMANCHA_DEBUG
        LAMANCHA_ASSERT(_rowA < R && _rowB < R, "Mat<R,C>::swapRows > Row index out of bounds.");
#endif
        if (_rowA == _rowB) { return; }
        Vec<C> temp = data[_rowA];
        data[_rowA] = data[_rowB];
        data[_rowB] = temp;
      }

      /** @brief Swap two rows
       *  @note Not SIMD-optimized
       */
      LAMANCHA_INLINE void swapCols(usize _colA, usize _colB) {
#if LAMANCHA_DEBUG
        LAMANCHA_ASSERT(_colA < C && _colB < C, "Mat<R,C>::swapCols > Column index out of bounds.");
#endif
        if (_colA == _colB) { return; }
        for (usize r = 0; r < R; ++r) {
          f32 tmp = data[r][_colB];
          data[r][_colB] = data[r][_colA];
          data[r][_colA] = tmp;
        }
      }

      /** @brief Extracts a contiguous rectangular sub-region [SR,ER)
       *  @tparam SR start row (inclusive)
       *  @tparam SC start column (inclusive)
       *  @tparam ER end row (exclusive)
       *  @tparam EC end column (exclusive)
       */
      template <usize SR, usize SC, usize ER, usize EC>
      constexpr Mat<ER - SR, EC - SC> submatrix() const {
        static_assert(SR < ER && SC < EC, "Invalid submatrix range");
        static_assert(ER <= R && EC <= C, "Sub-matrix out of bounds");
        Mat<ER - SR, EC - SC> out{};
        for (usize i = 0; i < ER - SR; ++i) {
          for (usize j = 0; j < EC - SC; ++j) {
            out.data[i][j] = data[SR + i][SC + j];
          }
        }
        return out;
      }

      /** @brief Computes the scalar determinant using Gaussian elimination with partial pivoting
       *
       *  Partial pivoting: at each step, swap the current row with the row that has the largest absolute
       *  value in the current column. This improves numerical stability by avoiding division by small numbers.
       *
       *  Each row swap flips the sign of the determinant (property of alternating multilinear forms).
       *  The product of the diagonal entries of the upper triangular matrix after elimination equals the
       *  determinant's magnitude.
       *
       *  @tparam SR start row (inclusive)
       *  @tparam SC start column (inclusive)
       *  @tparam ER end row (exclusive)
       *  @tparam EC end column (exclusive)
       *  @note Not SIMD-optimized
       */
      f32 determinant() const {
        static_assert(R == C, "determinant() requires a square matrix");
        Mat<R, C> tmp = *this;      // working copy - do not mutate the original
        f32 det = 1.0f;
        constexpr usize n = R;

        for (usize i = 0; i < n; i++) {
          usize pivot = i;
          for (usize r = i + 1; r < n; r++) {
            if (fAbs(tmp.data[r][i]) > fAbs(tmp.data[pivot][i])) {
              pivot = r;
            }
          }
          if (fAbs(tmp.data[pivot][i]) < 1e-12f) { return 0.0f; }

          if (pivot != i) {
            tmp.swapRows(pivot, i);
            det = -det;    // each row swap negates the determinant
          }

          det *= tmp.data[i][i];  // accumulate diagonal product

          for (usize r = i + 1; r < n; r++) {
            f32 factor = tmp.data[r][i] / tmp.data[i][i];
            for (usize c = i; c < n; c++) {
              tmp.data[r][c] -= factor * tmp.data[i][c];
            }
          }
        }
        return det;
      }

      /** @brief Computes the inverse using Gauss-Jordan elimination
       *
       *  Augments [A | I] and row-reduces both halves simultaneously.
       *  When the left half becomes I, the right half is A^-1.
       *
       *  @note Do not call in tight loops
       */
      LAMANCHA_INLINE Mat<R, R> inverse() const {
        static_assert(R == C, "inverse() requires a square matrix");

        Mat<R, R> A = *this;
        Mat<R, R> inv = customIdentity<R>();
        constexpr f32 EPS = 1e-8f;

        for (usize i = 0; i < R; ++i) {
          // Find pivot: row with largest |value| in column i (partial pivoting).
          usize pivot = i;
          f32 maxAbs = fAbs(A.data[i][i]);
          for (usize r = i + 1; r < R; ++r) {
            f32 v = fAbs(A.data[r][i]);
            if (v > maxAbs) { maxAbs = v; pivot = r; }
          }

          // If pivot is too small (near-singular), return zero matrix
          if (maxAbs < EPS) { return Mat<R, R>{}; }

          // Swap pivot row to position i
          if (pivot != i) {
            A.swapRows(i, pivot);
            inv.swapRows(i, pivot);
          }

          // Divide the entire row by the pivot to make A[i][i] = 1
          f32 invPivot = 1.0f / A.data[i][i];
          for (usize c = 0; c < R; ++c) {
            A.data[i][c] *= invPivot;
            inv.data[i][c] *= invPivot;
          }

          // Eliminate column i from all other rows
          for (usize r = 0; r < R; ++r) {
            if (r == i) { continue; }
            f32 factor = A.data[r][i];
            if (factor == 0.0f) { continue; }
            for (usize c = 0; c < R; ++c) {
              A.data[r][c] -= factor * A.data[i][c];
              inv.data[r][c] -= factor * inv.data[i][c];
            }
          }
        }
        return inv;
      }

      /** @brief Swaps rows and columns: result[j][i] = this[i][j].
       *
       *  The return type is Mat<C, R> (dimensions flip) - correct even for non-square matrices.
       *
       *  @note Do not call in tight loops
       */
      LAMANCHA_INLINE Mat<C, R> transpose() const {
        Mat<C, R> out{};
        for (i32 i = 0; i < (i32)R; i++) 
        {
          for (i32 j = 0; j < (i32)C; j++) { out.data[j][i] = data[i][j]; }
        }
        return out;
      }
    };

    using mat2_2 = Mat<2, 2>;
    using mat2_3 = Mat<2, 3>;
    using mat2_4 = Mat<2, 4>;
    using mat3_2 = Mat<3, 2>;
    using mat3_3 = Mat<3, 3>;
    using mat3_4 = Mat<3, 4>;
    using mat4_2 = Mat<4, 2>;
    using mat4_3 = Mat<4, 3>;
    using mat4_4 = Mat<4, 4>;

    /** @defgroup identity Identity matrices
     *  @{
     */

    constexpr Mat<2, 2> i2_2{ {{1,0}, {0, 1}} };
    constexpr Mat<3, 3> i3_3{ { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} } };
    constexpr Mat<4, 4> i4_4{ { {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1} } };

    /** @brief Runtime identity for any square size.
     *
     *  Sets diagonal elements to 1 and leaves everything else zero-initialized.
     *
     *  @tparam Side of the square matrix
     */
    template <usize N>
    LAMANCHA_INLINE Mat<N, N> customIdentity() {
      Mat<N, N> m{};
      for (usize i = 0; i < N; i++) { m.data[i][i] = 1.f; }
      return m;
    }

    /** @} */ // end of identity gruop

    /** @brief Multiplies A (RxM) by B (MxP) and writes the result into a pre-allocated C (RxP).
     *
     *  The loop order i-j-k walks the rows of A and columns of B in the pattern that is most
     *  cache-friendly for row-major storage: for each element A[i][j], the inner loop walks across B_row[j],
     *  which is sequential in memory.
     *
     *  @tparam Side of the square matrix
     *  @note Useful when you want to avoid a temporary or when the destination buffer is already allocated (like updating an existing transform in place)
     */
    template <usize R, usize M, usize P>
    LAMANCHA_INLINE void MatrixMult(const Mat<R, M>& A, const Mat<M, P>& B, Mat<R, P>& C) {
      for (usize i = 0; i < R; i++) {
        for (usize j = 0; j < M; j++) {
          for (usize k = 0; k < P; k++) {
            C.data[i][k] += A.data[i][j] * B.data[j][k];
          }
        }
      }
    }
  }
}