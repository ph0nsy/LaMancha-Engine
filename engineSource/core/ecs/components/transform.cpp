#include "core/pch.h"
#include "transform.h"

 /**
  * @brief Computes the 4x4 column-major model matrix and writes it to out_.
  *
  * Collapses the full composition M_model = T_pos * (T_pvt * R_quat * T_pvt^(-1)) * S
  * into direct slot assignment with no intermediate matrix allocations.
  *
  * The quaternion is first expanded to a 3x3 rotation matrix R. Scale is folded into R
  * by multiplying each column j by scale[j]. The pivot sandwich contributes a residual
  * to the translation column. World position is added on top of that residual.
  *
  * The resulting layout (column-major, so out_[col][row]) is:
  *
  * @code
  *   col 0        col 1         col 2         col 3
  *   R[0][0]*sx   R[0][1]*sy    R[0][2]*sz    pos_x + tx
  *   R[1][0]*sx   R[1][1]*sy    R[1][2]*sz    pos_y + ty
  *   R[2][0]*sx   R[2][1]*sy    R[2][2]*sz    pos_z + tz
  *   0            0             0             1
  *
  *   tx[i] = pivot[i] - (R[i][0]*sx*pivot[0] + R[i][1]*sy*pivot[1] + R[i][2]*sz*pivot[2])
  * @endcode
  *
  * @param[out] out_  The 4x4 column-major matrix to write into. All 16 entries are overwritten.
  */
void LaMancha::ECS::Transform::toMatrix(Math::mat4_4& out_) const
{
  Math::mat3_3 rot = rotation.toMatrix();
  out_[0][0] = rot[0][0] * scale[0];
  out_[1][0] = rot[0][1] * scale[1];
  out_[2][0] = rot[0][2] * scale[2];
  out_[3][0] = position[0] + (pivot[0] - (rot[0][0] * scale[0] * pivot[0] + rot[0][1] * scale[1] * pivot[1] + rot[0][2] * scale[2] * pivot[2]));

  out_[0][1] = rot[1][0] * scale[0];
  out_[1][1] = rot[1][1] * scale[1];
  out_[2][1] = rot[1][2] * scale[2];
  out_[3][1] = position[1] + (pivot[1] - (rot[1][0] * scale[0] * pivot[0] + rot[1][1] * scale[1] * pivot[1] + rot[1][2] * scale[2] * pivot[2]));

  out_[0][2] = rot[2][0] * scale[0];
  out_[1][2] = rot[2][1] * scale[1];
  out_[2][2] = rot[2][2] * scale[2];
  out_[3][2] = position[2] + (pivot[2] - (rot[2][0] * scale[0] * pivot[0] + rot[2][1] * scale[1] * pivot[1] + rot[2][2] * scale[2] * pivot[2]));

  out_[0][3] = 0.f;
  out_[1][3] = 0.f;
  out_[2][3] = 0.f;
  out_[3][3] = 1.f;
}

/**
 * @brief Computes the final 3x3 homogeneous transform matrix and writes it to @p out_.
 *
 * All transform components are collapsed into a single matrix with no intermediate
 * allocations. The composition order is:
 *
 * @code
 *   M = T_pos * (T_pvt * Rz * T_pvt^(-1)) * S * A
 * @endcode
 *
 * Because T_pos and the pivot sandwich only affect the translation column, and because
 * S and A are both diagonal (so they simply scale the rotation columns), the entire
 * matrix reduces to direct slot assignment:
 *
 * @code
 *   [ cos(pvtRot)*scale_x*cos(axRot_y)   -sin(pvtRot)*scale_y*cos(axRot_x)   pos_x + tx ]
 *   [ sin(pvtRot)*scale_x*cos(axRot_y)   cos(pvtRot)*scale_y*cos(axRot_x)    pos_y + ty ]
 *   [ 0                                  0                                   1          ]
 *
 *   tx = pivot_x*(1 - cos(pvtRot)) + pivot_y*sin(pvtRot)
 *   ty = pivot_y*(1 - cos(pvtRot)) - pivot_x*sin(pvtRot)
 * @endcode
 *
 * @param[out] out_  The 3x3 matrix (column major) to write into. All 9 entries are overwritten.
 */
void LaMancha::ECS::Transform2D::toMatrix(Math::mat3_3& out_) const
{
  f32 cosPvt = Math::fCos(pivotRotation);
  f32 sinPvt = Math::fSin(pivotRotation);

  f32 cosOX = Math::fCos(axisRotation[0]);
  f32 cosOY = Math::fCos(axisRotation[1]);

  out_[0][0] = cosPvt * scale[0] * cosOY;
  out_[1][0] = -sinPvt * scale[1] * cosOX;
  out_[2][0] = position[0] + (pivot[0] * (1 - cosPvt) + pivot[1] * sinPvt);

  out_[0][1] = sinPvt * scale[0] * cosOY;
  out_[1][1] = cosPvt * scale[1] * cosOX;
  out_[2][1] = position[1] + (pivot[1] * (1 - cosPvt) - pivot[0] * sinPvt);

  out_[0][2] = 0;
  out_[1][2] = 0;
  out_[2][2] = 1.f;
}

/**
 * @brief Lifting Transform2D's 3x3 into a 4x4 for a 3D pipeline
 *
 * @code
 *   [ mat3D[0,0]   mat3D[0,1]    0  mat3D[0,2] ]
 *   [ mat3D[1,0]   mat3D[1,1]    0  mat3D[1,2] ]
 *   [ 0            0             1  zOrder     ]
 *   [ 0            0             0  1          ]
 * @endcode
 *
 * @param[out] out_  The 4x4 matrix (column major) to write into. All 16 entries are overwritten.
 */
void LaMancha::ECS::Transform2D::toMatrix3D(Math::mat4_4& out_) const
{
  Math::mat3_3 mat3;
  toMatrix(mat3);

  out_[0][0] = mat3[0][0];
  out_[1][0] = mat3[0][1];
  out_[2][0] = 0.f;
  out_[3][0] = mat3[0][2];

  out_[0][1] = mat3[1][0];
  out_[1][1] = mat3[1][1];
  out_[2][1] = 0.f;
  out_[3][1] = mat3[1][2];

  out_[0][2] = 0.f;
  out_[1][2] = 0.f;
  out_[2][2] = 1.f;
  out_[3][2] = zOrder;

  out_[0][3] = 0.f;
  out_[1][3] = 0.f;
  out_[2][3] = 0.f;
  out_[3][3] = 1.f;
}