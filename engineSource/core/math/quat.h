/**
 * @file quat.h
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 */

#pragma once

#include "mat.h"

namespace LaMancha {
  namespace Math {
    /** @brief Quaternion for 3D rotations
      *
      *  Quaternions represent rotations as q = w + xi + yj + zk where w^2 + x^2 + y^2 + z^2 = 1.
      *  They avoid gimbal lock and interpolate smoothly (via slerp).
      *
      *  @param The scalar part is w
      *  @param The vector part is (x, y, z)
      *
      *  @note This class maintains the invariant that quaternions are normalized after construction and operations. Non-unit quaternions represent scaling + rotation.
      */
    struct Quat {
      f32 w, x, y, z;  ///< w=scalar, (x,y,z)=vector components

      /** @brief Default constructor: identity rotation (no rotation) */
      LAMANCHA_INLINE Quat() : w(1.f), x(0.f), y(0.f), z(0.f) {}

      /** @brief Construct from explicit components (not normalized automatically) */
      LAMANCHA_INLINE Quat(f32 _w, f32 _x, f32 _y, f32 _z)
        : w(_w), x(_x), y(_y), z(_z) {
      }

      /** @brief Construct from scalar + vector parts */
      LAMANCHA_INLINE Quat(f32 _w, const vec3& v)
        : w(_w), x(v[0]), y(v[1]), z(v[2]) {
      }

      /** @brief Construct from axis-angle representation
       *  @param axis Unit vector representing rotation axis
       *  @param angle Rotation angle in radians (right-hand rule: thumb=axis, fingers=rotation)
       *  @return Normalized quaternion representing the rotation
       *
       *  @note Axis must be pre-normalized. No normalization check for performance.
       *  @note For 180° rotations, any perpendicular axis works (non-unique)
       *  @note Formula: q = [cos(θ/2), sin(θ/2)·axis]
       */
      static LAMANCHA_INLINE Quat fromAxisAngle(const vec3& axis, f32 angle) {
        f32 halfAngle = angle * 0.5f;
        f32 s = fSin(halfAngle);
        return Quat(fCos(halfAngle), axis[0] * s, axis[1] * s, axis[2] * s);
      }

      /** @brief Construct from Euler angles (yaw-pitch-roll, ZYX order)
       *
       *  @param yaw Rotation around Z-axis (heading) in radians
       *  @param pitch Rotation around Y-axis (elevation) in radians
       *  @param roll Rotation around X-axis (bank) in radians
       *  @return Normalized quaternion
       *
       *  @warning Euler angles suffer from gimbal lock at pitch = ±90°
       *  @note Use fromAxisAngle() or matrix conversion for animation interpolation
       *  @note Formulas (from aerospace/robotics convention):
       *  - First rotate by roll around X
       *  - Then rotate by pitch around Y
       *  - Finally rotate by yaw around Z
       */
      static LAMANCHA_INLINE Quat fromEuler(f32 yaw, f32 pitch, f32 roll) {
        f32 cy = fCos(yaw * 0.5f);
        f32 sy = fSin(yaw * 0.5f);
        f32 cp = fCos(pitch * 0.5f);
        f32 sp = fSin(pitch * 0.5f);
        f32 cr = fCos(roll * 0.5f);
        f32 sr = fSin(roll * 0.5f);

        return Quat(
          cr * cp * cy + sr * sp * sy,  // w
          sr * cp * cy - cr * sp * sy,  // x
          cr * sp * cy + sr * cp * sy,  // y
          cr * cp * sy - sr * sp * cy   // z
        );
      }

      /** @brief Construct from rotation matrix (orthonormal 3x3 or 4x4)
       *
       *  Uses the Shepperd method; chooses the numerically stable branch based
       *  on which diagonal element is largest, avoiding division by small numbers.
       *
       *  @param m Rotation matrix (must be orthonormal, no scaling/shearing)
       *  @return Normalized quaternion
       *
       *  @note If matrix contains scaling, extract scale first: scale = length(m[0])
       *  @note For 4x4 matrices, only the upper-left 3x3 is used
       */
      static LAMANCHA_INLINE Quat fromMatrix(const mat3_3& m) {
        f32 trace = m[0][0] + m[1][1] + m[2][2];
        Quat q;

        if (trace > 0.f) {
          // w is largest component
          f32 s = fSqrt(trace + 1.f) * 2.f;  // s = 4*w
          q.w = 0.25f * s;
          q.x = (m[2][1] - m[1][2]) / s;
          q.y = (m[0][2] - m[2][0]) / s;
          q.z = (m[1][0] - m[0][1]) / s;
        }
        else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
          // x is largest component
          f32 s = fSqrt(1.f + m[0][0] - m[1][1] - m[2][2]) * 2.f;  // s = 4*x
          q.w = (m[2][1] - m[1][2]) / s;
          q.x = 0.25f * s;
          q.y = (m[0][1] + m[1][0]) / s;
          q.z = (m[0][2] + m[2][0]) / s;
        }
        else if (m[1][1] > m[2][2]) {
          // y is largest component
          f32 s = fSqrt(1.f + m[1][1] - m[0][0] - m[2][2]) * 2.f;  // s = 4*y
          q.w = (m[0][2] - m[2][0]) / s;
          q.x = (m[0][1] + m[1][0]) / s;
          q.y = 0.25f * s;
          q.z = (m[1][2] + m[2][1]) / s;
        }
        else {
          // z is largest component
          f32 s = fSqrt(1.f + m[2][2] - m[0][0] - m[1][1]) * 2.f;  // s = 4*z
          q.w = (m[1][0] - m[0][1]) / s;
          q.x = (m[0][2] + m[2][0]) / s;
          q.y = (m[1][2] + m[2][1]) / s;
          q.z = 0.25f * s;
        }

        return q.normalized();
      }

      /** @brief Squared magnitude: w^2 + x^2 + y^2 + z^2 */
      LAMANCHA_INLINE f32 lengthSq() const {
        return w * w + x * x + y * y + z * z;
      }

      /** @brief Magnitude: sqrt(w^2 + x^2 + y^2 + z^2) */
      LAMANCHA_INLINE f32 length() const {
        return fSqrt(lengthSq());
      }

      /** @brief Return normalized quaternion (magnitude = 1) */
      LAMANCHA_INLINE Quat normalized() const {
        f32 len = length();
        if (len < MACHINE_EPSILON) {
          return Quat();  // Return identity if degenerate
        }
        f32 invLen = 1.f / len;
        return Quat(w * invLen, x * invLen, y * invLen, z * invLen);
      }

      /** @brief Normalize in place */
      LAMANCHA_INLINE void normalize() {
        f32 len = length();
        if (len > MACHINE_EPSILON) {
          f32 invLen = 1.f / len;
          w *= invLen;
          x *= invLen;
          y *= invLen;
          z *= invLen;
        }
        else {
          // Degenerate: reset to identity
          w = 1.f;
          x = y = z = 0.f;
        }
      }

      /**
       * @brief Conjugate: [w, -x, -y, -z]
       *
       * For unit quaternions, conjugate equals inverse (q* * q = 1).
       * Represents the opposite rotation: if q rotates by θ around axis,
       * q* rotates by -θ around the same axis.
       */
      LAMANCHA_INLINE Quat conjugate() const {
        return Quat(w, -x, -y, -z);
      }

      /**
       * @brief Inverse: q^(-1) = q* / |q|^2
       *
       * For unit quaternions (|q| = 1), inverse = conjugate.
       * For non-unit quaternions (scaling), this is the true inverse.
       *
       * @note q * q^(-1) = [1, 0, 0, 0] (identity)
       */
      LAMANCHA_INLINE Quat inverse() const {
        f32 lenSq = lengthSq();
        if (lenSq < MACHINE_EPSILON) {
          return Quat();  // Degenerate -> return identity
        }
        f32 invLenSq = 1.f / lenSq;
        return Quat(w * invLenSq, -x * invLenSq, -y * invLenSq, -z * invLenSq);
      }

      /** @brief Quaternion multiplication: q1 * q2 (rotation composition)
       *
       *  Rotate first by q2, then by q1 (right-to-left)
       *
       *  @note Non-commutative: q₁*q₂ ≠ q₂*q₁ (order matters!)
       *  @note (q₁*q₂)*v = q₁*(q₂*v) for rotating vectors
       *  @note Formula (Grassmann product):
       *    w = w₁w₂ - x₁x₂ - y₁y₂ - z₁z₂
       *    x = w₁x₂ + x₁w₂ + y₁z₂ - z₁y₂
       *    y = w₁y₂ - x₁z₂ + y₁w₂ + z₁x₂
       *    z = w₁z₂ + x₁y₂ - y₁x₂ + z₁w₂
       */
      LAMANCHA_INLINE Quat operator*(const Quat& _q) const {
        return Quat(
          w * _q.w - x * _q.x - y * _q.y - z * _q.z,  // scalar part
          w * _q.x + x * _q.w + y * _q.z - z * _q.y,  // i part
          w * _q.y - x * _q.z + y * _q.w + z * _q.x,  // j part
          w * _q.z + x * _q.y - y * _q.x + z * _q.w   // k part
        );
      }

      LAMANCHA_INLINE Quat& operator*=(const Quat& _q) {
        *this = (*this) * _q;
        return *this;
      }

      /** @brief Rotate a 3D vector by this quaternion
       *
       *  Optimized version avoids full quaternion multiplies:
       *    v' = v + 2w(vec × v) + 2(vec × (vec × v))
       *    Where vec = (x, y, z) is the quaternion's vector part
       *
       *  @param v Vector to rotate
       *  @return Rotated vector
       *
       *  @note Cost: 15 multiplies, 15 adds vs 32 multiplies, 24 adds for naive method
       *  @note This quaternion should be normalized (unit quaternion)
       *  @note For repeated rotations, convert to matrix first
       *  @note Formula:
       *    v' = q * v * q*
       *    Where v is treated as pure quaternion [0, v]
       */
      LAMANCHA_INLINE vec3 rotate(const vec3& _vec) const {
        // Extract vector part of quaternion
        vec3 qvec{ x, y, z };

        // Compute cross products
        vec3 cross1 = qvec.Cross(_vec);
        vec3 cross2 = qvec.Cross(cross1);

        // v' = v + 2w(qvec × v) + 2(qvec × (qvec × v))
        return _vec + cross1 * (2.f * w) + cross2 * 2.f;
      }

      /** @defgroup formatting Transform into other formats
       *  @{
       */

       /** @brief Convert to 3x3 rotation matrix
        *  @return Orthonormal rotation matrix
        *
        *  @note Result is orthonormal if this quaternion is normalized
        *  @note Formula (from quaternion algebra):
        *    R = I + 2s·[v]× + 2[v]×²
        *    Where s = w, v = (x,y,z), [v]× is the cross-product matrix
        */
      LAMANCHA_INLINE mat3_3 toMatrix() const {
        f32 xx = x * x, yy = y * y, zz = z * z;
        f32 xy = x * y, xz = x * z, yz = y * z;
        f32 wx = w * x, wy = w * y, wz = w * z;

        mat3_3 m;
        m[0][0] = 1.f - 2.f * (yy + zz);
        m[0][1] = 2.f * (xy - wz);
        m[0][2] = 2.f * (xz + wy);

        m[1][0] = 2.f * (xy + wz);
        m[1][1] = 1.f - 2.f * (xx + zz);
        m[1][2] = 2.f * (yz - wx);

        m[2][0] = 2.f * (xz - wy);
        m[2][1] = 2.f * (yz + wx);
        m[2][2] = 1.f - 2.f * (xx + yy);

        return m;
      }

      /** @brief Convert to axis-angle representation
       *  @param outAxis Output: normalized rotation axis
       *  @param outAngle Output: rotation angle in radians [0, π]
       *
       *  @note For identity quaternion (w=1), axis is undefined (returns +Z arbitrarily)
       *  @note For 180° rotations (w=0), axis is (x,y,z) / ||(x,y,z)||
       *  @note Formula:
       *    axis = (x, y, z) / ||(x, y, z)||
       *    angle = 2·arccos(w)
       */
      LAMANCHA_INLINE void toAxisAngle(vec3& outAxis_, f32& outAngle_) const {
        // Ensure normalized
        Quat q = normalized();

        // Clamp w to [-1, 1] to handle numerical errors in arccos
        f32 wClamped = fClamp(q.w, -1.f, 1.f);
        outAngle_ = 2.f * fAcos(wClamped);

        f32 sinHalfAngle = fSqrt(1.f - wClamped * wClamped);

        if (sinHalfAngle < MACHINE_EPSILON) {
          // Angle ≈ 0 or 2π - axis is arbitrary (rotation is nearly identity)
          outAxis_ = vec3{ 0.f, 0.f, 1.f };  // Choose +Z by convention
        }
        else {
          // axis = (x, y, z) / sin(angle/2)
          f32 invSin = 1.f / sinHalfAngle;
          outAxis_ = vec3{ q.x * invSin, q.y * invSin, q.z * invSin };
        }
      }

      /** @brief Convert to Euler angles (yaw-pitch-roll, ZYX order)
       *  @return vec3(yaw, pitch, roll) in radians
       *
       *
       *  @warning Gimbal lock occurs at pitch = ±90° (returns yaw+roll combined)
       *  @note Returned angles are in range: yaw[-π,π], pitch[-π/2,π/2], roll[-π,π]
       *  @note Formulas (aerospace convention):
       *    yaw   = atan2(2(wx + yz), 1 - 2(x² + y²))
       *    pitch = asin(2(wy - xz))
       *    roll  = atan2(2(wz + xy), 1 - 2(y² + z²))
       */
      LAMANCHA_INLINE vec3 toEuler() const {
        vec3 euler = { 0 };

        // Yaw (Z-axis rotation)
        euler[0] = fAtan2(2.f * (w * z + x * y), 1.f - 2.f * (y * y + z * z));

        // Pitch (Y-axis rotation)
        f32 sinPitch = 2.f * (w * y - z * x);
        sinPitch = fClamp(sinPitch, -1.f, 1.f);  // Clamp for numerical stability
        euler[1] = fAsin(sinPitch);

        // Roll (X-axis rotation)
        euler[2] = fAtan2(2.f * (w * x + y * z), 1.f - 2.f * (x * x + y * y));

        return euler;
      }

      /** @} */ // end of formatting gruop

      /** @defgroup interp Interpolation
       *  @{
       */

       /** @brief Dot product of two quaternions.
        *
        *  cos(θ) where θ is the angle between rotations. Used to determine interpolation direction in slerp.
        *
        *  Range [-1, 1]:
        *  - Near 1: rotations are similar
        *  - Near 0: rotations are perpendicular (90° apart)
        *  - Near -1: rotations are opposite (180° apart)
        */
      LAMANCHA_INLINE f32 dot(const Quat& _q) const {
        return w * _q.w + x * _q.x + y * _q.y + z * _q.z;
      }

      /** @brief Linear interpolation (fast but not constant velocity).
       *  @param _start Starting quaternion
       *  @param _end Target quaternion
       *  @param _step Interpolation parameter [0, 1]
       *  @return Interpolated quaternion (normalized)
       *
       *  @note Faster than slerp but animation speed varies (faster near endpoints)
       *  @note Good enough for small rotations or when performance is critical
       *  @note Always normalizes result to maintain unit quaternion
       *  @note Formula: lerp(q1, q2, t) = (1-t)q1 + t·q2, then normalize
       */
      static LAMANCHA_INLINE Quat lerp(const Quat& _start, const Quat& _end, f32 _step) {
        // Handle quaternion double-cover: q and -q represent same rotation
        f32 dotProduct = _start.dot(_end);
        f32 sign = (dotProduct < 0.f) ? -1.f : 1.f;

        Quat result(
          _start.w + _step * (sign * _end.w - _start.w),
          _start.x + _step * (sign * _end.x - _start.x),
          _start.y + _step * (sign * _end.y - _start.y),
          _start.z + _step * (sign * _end.z - _start.z)
        );

        return result.normalized();
      }

      /** @brief Spherical linear interpolation (constant velocity, smooth)
       *  Properties:
       *  - Constant angular velocity (smooth animation)
       *  - Shortest path on the 4D unit hypersphere
       *  - Torque-free (no sudden jerks)
       *
       *  @param q Target quaternion
       *  @param t Interpolation parameter [0, 1]
       *  @return Interpolated quaternion (normalized)
       *
       *  @note Falls back to lerp when quaternions are very close (dotProduct > 0.9995)
       *  @note Handles quaternion double-cover (q ≡ -q) automatically
       *  @note Standard for high-quality animation and camera movement
       *  @note Formula:
       *    slerp(q1, q2, t) = (sin((1-t)θ)/sin(θ))q1 + (sin(tθ)/sin(θ))q2
       *    Where θ = arccos(q₁·q₂) is the angle between quaternions
       */
      static LAMANCHA_INLINE Quat slerp(const Quat& _start, const Quat& _end, f32 _step) {
        f32 dotProduct = _start.dot(_end);

        // Handle quaternion double-cover: take shortest path
        Quat q2 = _end;
        if (dotProduct < 0.f) {
          q2 = Quat(-_end.w, -_end.x, -_end.y, -_end.z);
          dotProduct = -dotProduct;
        }

        // Clamp for numerical stability
        dotProduct = fClamp(dotProduct, -1.f, 1.f);

        // If quaternions are very close, use lerp to avoid division by ~0
        if (dotProduct > 0.9995f) {
          return lerp(_start, q2, _step);
        }

        // Compute angle and interpolation weights
        f32 theta = fAcos(dotProduct);
        f32 sinTheta = fSin(theta);
        f32 w1 = fSin((1.f - _step) * theta) / sinTheta;
        f32 w2 = fSin(_step * theta) / sinTheta;

        return Quat(
          _start.w * w1 + q2.w * w2,
          _start.x * w1 + q2.x * w2,
          _start.y * w1 + q2.y * w2,
          _start.z * w1 + q2.z * w2
        );
      }

      /** @} */ // end of interp gruop
    };

    using quat = Quat;  // Alias for consistency with vector and matrix naming
  }
}