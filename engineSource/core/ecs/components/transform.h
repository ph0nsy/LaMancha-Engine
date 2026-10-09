/**
 * @file transform.h
 * @brief Transform component definition
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
*
*/

#pragma once
#include "core/math/quat.h"
#include "core/ecs/component.h"

namespace LaMancha {
  namespace ECS {
    /**
     * @brief 3D transformation component defining the position, orientation, scale,
     * and pivot of an object in 3D space.
     *
     * The final model matrix is composed as:
     *
     * @code
     *   M_model = T_pos * (T_pvt * R_quat * T_pvt^(-1)) * S
     * @endcode
     *
     * Where:
     *
     *   - T_pos > world position translation
     *
     *   - T_pvt > pivot sandwich (rotate and scale around an off-center point)
     *
     *   - R_quat > rotation derived from the quaternion
     *
     *   - S > scale
     *
     * @note The output matrix is column-major, matching OpenGL/GPU conventions (out_[col][row]);
     * so out_[3] is the translation column.
     */
    struct Transform : CoreComponent {
      /**
       * @brief World position of the transform origin (x, y, z).
       *
       * Enters the matrix as a direct translation in the final column (col 3),
       * summed with the pivot residual:
       *
       * @code
       *   out_[3][i] = position[i] + pivot_residual[i]
       * @endcode
       */
      Math::vec3 position;

      /**
       * @brief Orientation of the transform, stored as a unit quaternion (w, x, y, z).
       *
       * A quaternion is used instead of Euler angles to avoid gimbal lock and to allow
       * smooth interpolation (slerp). It is converted to a 3x3 rotation matrix R at
       * matrix-build time via rotation.toMatrix().
       *
       * The expanded form of R from a unit quaternion q = (w, x, y, z) is:
       *
       * @code
       *   [ 1-2(y^2+z^2)   2(xy-wz)    2(xz+wy) ]
       *   [ 2(xy+wz)     1-2(x^2+z^2)  2(yz-wx) ]
       *   [ 2(xz-wy)     2(yz+wx)    1-2(x^2+y^2)]
       * @endcode
       *
       * @note Three construction paths are provided: directly from a quat, from Euler
       * angles (xyz radians), or from an axis-angle vec4 (x, y, z, angle).
       */
      Math::quat rotation;

      /**
       * @brief Per-axis scale factors (x, y, z).
       *
       * Applied as a diagonal matrix S. Because S is diagonal it folds directly into
       * the rotation matrix. Each column j of R is scaled by scale[j]:
       *
       * @code
       *   (R * S)[i][j] = R[i][j] * scale[j]
       * @endcode
       *
       * @note This means no separate matrix multiply is needed; the scale is baked into
       * the column writes of the final matrix.
       */
      Math::vec3 scale;

      /**
       * @brief Local offset from the transform origin that acts as the center of rotation and scaling (x, y, z).
       *
       * The pivot is a fixed anchor in parent space. Rotation and scale are applied around it
       * via the translate-rotate-untranslate sandwich:
       *
       * @code
       *   T_pvt * R * T_pvt⁻¹
       * @endcode
       *
       * Expanding the sandwich analytically, the pivot contributes a residual to the
       * translation column only. Because R·S just scales each column of R by scale[j],
       * the residual for row i is:
       *
       * @code
       *   tx[i] = pivot[i] - (R[i][0]*scale[0]*pivot[0]
       *                      + R[i][1]*scale[1]*pivot[1]
       *                      + R[i][2]*scale[2]*pivot[2])
       * @endcode
       *
       * Which reads as: "where the pivot ended up after rotation and scale" subtracted
       * from where it started, leaving only the displacement introduced by the transform.
       *
       * @note When pivot = (0, 0, 0) the residual is zero and the object rotates around its own origin.
       */
      Math::vec3 pivot;

      /** @brief Constructs an identity transform: no position, no rotation, unit scale, pivot at origin. */
      Transform()
        : position({ 0, 0, 0 }), rotation({ 0, 0, 0, 0 }), scale({ 1, 1, 1 }), pivot({ 0, 0, 0 }) {
      }

      /**
       * @brief Constructs a transform from a position, a pre-built quaternion, and a scale.
       * @param _pos  World position (x, y, z).
       * @param _rot  Rotation as a unit quaternion.
       * @param _scl  Scale factors (x, y, z).
       */
      Transform(Math::vec3 _pos, Math::quat _rot, Math::vec3 _scl)
        : position(_pos), rotation(_rot), scale(_scl), pivot({ 0, 0, 0 }) {
      }

      /**
       * @brief Constructs a transform from a position, Euler angles, and a scale.
       *
       * Euler angles are in radians and applied in XYZ order. Internally converted
       * to a quaternion via @c Math::quat::fromEuler to avoid gimbal lock.
       *
       * @param _pos  World position (x, y, z).
       * @param _rot  Euler angles in radians (pitch, yaw, roll).
       * @param _scl  Scale factors (x, y, z).
       */
      Transform(Math::vec3 _pos, Math::vec3 _rot, Math::vec3 _scl)
        : position(_pos), rotation(Math::quat::fromEuler(_rot[0], _rot[1], _rot[2])), scale(_scl), pivot({ 0, 0, 0 }) {
      }

      /**
       * @brief Constructs a transform from a position, an axis-angle vec4, and a scale.
       *
       * The vec4 is interpreted as (x, y, z, angle) where (x, y, z) is the rotation axis
       * and angle is in radians.
       *
       * @param _pos  World position (x, y, z).
       * @param _rot  Axis-angle as vec4 (axis_x, axis_y, axis_z, angle_radians).
       * @param _scl  Scale factors (x, y, z).
       */
      Transform(Math::vec3 _pos, Math::vec4 _rot, Math::vec3 _scl)
        : position(_pos), rotation(_rot[0], _rot[1], _rot[2], _rot[3]), scale(_scl), pivot({ 0, 0, 0 }) {
      }

      void toMatrix(Math::mat4_4& out_) const;
    };

    /**
     * @brief 2D transformation component defining the position, orientation, scale,
     * and projection of a plane in 2D space.
     *
     * The final transform matrix is composed as:
     *
     * @code
     *   M = T_pos * (T_pvt * Rz * T_pvt^(-1)) * S * A
     * @endcode
     *
     * Where:
     *
     *   - T_pos > world position translation
     *
     *   - T_pvt > pivot sandwich (rotate around an off-center point)
     *
     *   - Rz > in-plane rotation (pivotRotation)
     *
     *   - S > scale
     *
     *   - A > axis tilt (Rx and Ry projected into 2D)
     */
    struct Transform2D : CoreComponent {
      /**
       * @brief World position of the transform origin, in 2D space (x, y).
       *
       * Enters the matrix as a direct translation in the final column:
       *
       * @code
       *   [ 1  0  pos_x ]
       *   [ 0  1  pos_y ]
       *   [ 0  0  1     ]
       * @endcode
       */
      Math::vec2 position;

      /**
       * @brief Per-axis tilt angles (radians), encoding a projection of 3D rotations onto the 2D plane.
       *
       * Since this transform describes a 2D plane in 3D space, tilting the plane toward or away from
       * the camera along each axis is expressed as a projected rotation. Only the cosine of each angle
       * survives the projection, the sine terms drop out because they would map onto the missing Z axis.
       *
       * - axisRotation[0] -> angle of rotation around the world X axis (Rx). Tilts the plane up/down
       * (toward/away from camera vertically). Projects to: cos(axisRotation[0]) scaling the Y column.
       *
       * Z row/col dropped (no depth axis in 2D)
       * @code
       *   [ 1   0               0  ]
       *   [ 0   cos(axRot_x)    0  ]
       *   [ 0   0               1  ]
       * @endcode
       *
       * - axisRotation[1] -> angle of rotation around the world Y axis (Ry). Tilts the plane left/right
       * (toward/away from camera horizontally). Projects to: cos(axisRotation[1]) scaling the X column.
       *
       * Z row/col dropped (no depth axis in 2D)
       * @code
       *   [ cos(axRot_y)   0   0  ]
       *   [ 0              1   0  ]
       *   [ 0              0   1  ]
       * @endcode
       *
       * Combined axis tilt matrix A = Ry * Rx projected:
       * @code
       *   [ cos(axRot_y)   0             0 ]
       *   [ 0              cos(axRot_x)  0 ]
       *   [ 0              0             1 ]
       * @endcode
       *
       * @note At 0 radians on both axes the matrix is identity (no tilt, full size).
       * @note At +- pi/2 on either axis the plane is edge-on and collapses to a line in that direction.
       */
      Math::vec2 axisRotation;

      /**
       * @brief Per-axis scale factors (x, y).
       *
       * Applied as a diagonal matrix after the pivot rotation and before axis tilt:
       *
       * @code
       *   [ scale_x  0        0 ]
       *   [ 0        scale_y  0 ]
       *   [ 0        0        1 ]
       * @endcode
       *
       * @note Because S and A are both diagonal, they collapse into a single diagonal when multiplied.
       * Each rotation column is simply scaled by scale_x * cos(axRot_y) or scale_y * cos(axRot_x).
       */
      Math::vec2 scale;

      /**
       * @brief Local offset from the transform origin that acts as the center of rotation and scaling (x, y).
       *
       * The pivot is not a transformed point; it is a fixed anchor in parent space around which
       * pivotRotation is applied. This is achieved via the translate-rotate-untranslate sandwich:
       * @code
       *   T_pvt * Rz * T_pvt^(-1)
       * @endcode
       *
       * Expanding the sandwich analytically, the pivot contributes only to the translation column
       * of the final matrix; the rotation sub-matrix (top-left 2x2) is unaffected.
       *
       * @code
       *   tx = pivot_x * (1 - cos(pvtRot)) + pivot_y * sin(pvtRot)
       *   ty = pivot_y * (1 - cos(pvtRot)) - pivot_x * sin(pvtRot)
       * @endcode
       *
       * @note When pivot = (0, 0) the sandwich collapses to identity and pivotRotation spins the object around its own local origin.
       */
      Math::vec2 pivot;

      /**
       * @brief In-plane rotation angle (radians) applied around the pivot point.
       *
       * This is a standard 2D (Rz) rotation (it spins the object within the XY plane).
       * It is applied after scale and axis tilt, wrapped in the pivot sandwich so that
       * the center of rotation is @ref pivot rather than the local origin:
       *
       * @code
       *   [ cos(pvtRot)  -sin(pvtRot)  tx ]
       *   [ sin(pvtRot)   cos(pvtRot)  ty ]
       *   [ 0             0             1 ]
       * @endcode
       *
       * Where tx, ty are the pivot-derived translation residuals (see @ref pivot).
       */
      f32 pivotRotation;

      /**
       * @brief Depth ordering value used to sort draw calls.
       *
       * Does not contribute to the 2D transform matrix. Higher values draw on top.
       */
      f32 zOrder;

      Transform2D()
        : position({ 0, 0 }), zOrder(0), axisRotation({ 0, 0 }), scale({ 1, 1 }), pivot({ 0, 0 }), pivotRotation(0) {
      }

      Transform2D(Math::vec2 _pos, Math::vec2 _axRot = { 0, 0 }, f32 _zO = 0, f32 _pvtRot = 0, Math::vec2 _pvt = { 1, 1 }, Math::vec2 _scl = { 1, 1 })
        : position(_pos), axisRotation(_axRot* Math::RAD_2_DEG), zOrder(_zO), pivotRotation(_pvtRot), pivot(_pvt), scale(_scl) {
      }

      void toMatrix(Math::mat3_3& out_) const;
      void toMatrix3D(Math::mat4_4& out_) const;
    };
  }
}