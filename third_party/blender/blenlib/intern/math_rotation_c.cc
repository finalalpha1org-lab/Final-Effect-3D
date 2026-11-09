/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_math_rotation.h"

#include "BLI_math_base_safe.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Quaternions
 * \{ */

/* used to test is a quat is not normalized (only used for debug prints) */
#ifndef NDEBUG
#  define QUAT_EPSILON 0.0001
#endif

/**
 * The threshold for using a zeroed 3rd (typically Z) value when calculating the euler.
 * NOTE(@ideasman42): A reasonable range for this value is (0.0002 .. 0.00002).
 * This was previously `16 * FLT_EPSILON` however it caused imprecision at times,
 * see examples from: #116880.
 */
#define EULER_HYPOT_EPSILON 0.0000375

void unit_axis_angle(float axis[3], float *angle)
{
  axis[0] = 0.0f;
  axis[1] = 1.0f;
  axis[2] = 0.0f;
  *angle = 0.0f;
}

void unit_qt(float q[4])
{
  q[0] = 1.0f;
  q[1] = q[2] = q[3] = 0.0f;
}

void copy_qt_qt(float q[4], const float a[4])
{
  q[0] = a[0];
  q[1] = a[1];
  q[2] = a[2];
  q[3] = a[3];
}

bool is_zero_qt(const float q[4])
{
  return (q[0] == 0 && q[1] == 0 && q[2] == 0 && q[3] == 0);
}

void mul_qt_qtqt(float q[4], const float a[4], const float b[4])
{
  float t0, t1, t2;

  t0 = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
  t1 = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
  t2 = a[0] * b[2] + a[2] * b[0] + a[3] * b[1] - a[1] * b[3];
  q[3] = a[0] * b[3] + a[3] * b[0] + a[1] * b[2] - a[2] * b[1];
  q[0] = t0;
  q[1] = t1;
  q[2] = t2;
}

void mul_qt_v3(const float q[4], float r[3])
{
  float t0, t1, t2;

  t0 = -q[1] * r[0] - q[2] * r[1] - q[3] * r[2];
  t1 = q[0] * r[0] + q[2] * r[2] - q[3] * r[1];
  t2 = q[0] * r[1] + q[3] * r[0] - q[1] * r[2];
  r[2] = q[0] * r[2] + q[1] * r[1] - q[2] * r[0];
  r[0] = t1;
  r[1] = t2;

  t1 = t0 * -q[1] + r[0] * q[0] - r[1] * q[3] + r[2] * q[2];
  t2 = t0 * -q[2] + r[1] * q[0] - r[2] * q[1] + r[0] * q[3];
  r[2] = t0 * -q[3] + r[2] * q[0] - r[0] * q[2] + r[1] * q[1];
  r[0] = t1;
  r[1] = t2;
}

void conjugate_qt_qt(float q1[4], const float q2[4])
{
  q1[0] = q2[0];
  q1[1] = -q2[1];
  q1[2] = -q2[2];
  q1[3] = -q2[3];
}

void conjugate_qt(float q[4])
{
  q[1] = -q[1];
  q[2] = -q[2];
  q[3] = -q[3];
}

float dot_qtqt(const float a[4], const float b[4])
{
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

void invert_qt(float q[4])
{
  const float f = dot_qtqt(q, q);

  if (f == 0.0f) {
    return;
  }

  conjugate_qt(q);
  mul_qt_fl(q, 1.0f / f);
}

void invert_qt_qt(float q1[4], const float q2[4])
{
  copy_qt_qt(q1, q2);
  invert_qt(q1);
}

void invert_qt_normalized(float q[4])
{
  BLI_ASSERT_UNIT_QUAT(q);
  conjugate_qt(q);
}

void invert_qt_qt_normalized(float q1[4], const float q2[4])
{
  copy_qt_qt(q1, q2);
  invert_qt_normalized(q1);
}

void mul_qt_fl(float q[4], const float f)
{
  q[0] *= f;
  q[1] *= f;
  q[2] *= f;
  q[3] *= f;
}

void sub_qt_qtqt(float q[4], const float a[4], const float b[4])
{
  float n_b[4];

  n_b[0] = -b[0];
  n_b[1] = b[1];
  n_b[2] = b[2];
  n_b[3] = b[3];

  mul_qt_qtqt(q, a, n_b);
}

void pow_qt_fl_normalized(float q[4], const float fac)
{
  BLI_ASSERT_UNIT_QUAT(q);
  const float angle = fac * safe_acosf(q[0]); /* quat[0] = cos(0.5 * angle),
                                               * but now the 0.5 and 2.0 rule out */
  const float co = cosf(angle);
  const float si = sinf(angle);
  q[0] = co;
  normalize_v3_length(q + 1, si);
}

void quat_to_compatible_quat(float q[4], const float a[4], const float old[4])
{
  const float eps = 1e-4f;
  BLI_ASSERT_UNIT_QUAT(a);
  float old_unit[4];
  /* Skips `!finite_v4(old)` case too. */
  if (normalize_qt_qt(old_unit, old) > eps) {
    float q_negate[4];
    float delta[4];
    rotation_between_quats_to_quat(delta, old_unit, a);
    mul_qt_qtqt(q, old, delta);
    negate_v4_v4(q_negate, q);
    if (len_squared_v4v4(q_negate, old) < len_squared_v4v4(q, old)) {
      copy_qt_qt(q, q_negate);
    }
  }
  else {
    copy_qt_qt(q, a);
  }
}

/* Skip error check, currently only needed by #mat3_to_quat_legacy. */
static void quat_to_mat3_no_error(float m[3][3], const float q[4])
{
  double q0, q1, q2, q3, qda, qdb, qdc, qaa, qab, qac, qbb, qbc, qcc;

  q0 = M_SQRT2 * double(q[0]);
  q1 = M_SQRT2 * double(q[1]);
  q2 = M_SQRT2 * double(q[2]);
  q3 = M_SQRT2 * double(q[3]);

  qda = q0 * q1;
  qdb = q0 * q2;
  qdc = q0 * q3;
  qaa = q1 * q1;
  qab = q1 * q2;
  qac = q1 * q3;
  qbb = q2 * q2;
  qbc = q2 * q3;
  qcc = q3 * q3;

  m[0][0] = float(1.0 - qbb - qcc);
  m[0][1] = float(qdc + qab);
  m[0][2] = float(-qdb + qac);

  m[1][0] = float(-qdc + qab);
  m[1][1] = float(1.0 - qaa - qcc);
  m[1][2] = float(qda + qbc);

  m[2][0] = float(qdb + qac);
  m[2][1] = float(-qda + qbc);
  m[2][2] = float(1.0 - qaa - qbb);
}

void quat_to_mat3(float m[3][3], const float q[4])
{
#ifndef NDEBUG
  float f;
  if (!((f = dot_qtqt(q, q)) == 0.0f || (fabsf(f - 1.0f) < float(QUAT_EPSILON)))) {
    fprintf(stderr,
            "Warning! quat_to_mat3() called with non-normalized: size %.8f *** report a bug ***\n",
            f);
  }
#endif

  quat_to_mat3_no_error(m, q);
}

void quat_to_mat4(float m[4][4], const float q[4])
{
  double q0, q1, q2, q3, qda, qdb, qdc, qaa, qab, qac, qbb, qbc, qcc;

#ifndef NDEBUG
  if (!((q0 = dot_qtqt(q, q)) == 0.0 || (fabs(q0 - 1.0) < QUAT_EPSILON))) {
    fprintf(stderr,
            "Warning! quat_to_mat4() called with non-normalized: size %.8f *** report a bug ***\n",
            float(q0));
  }
#endif

  q0 = M_SQRT2 * double(q[0]);
  q1 = M_SQRT2 * double(q[1]);
  q2 = M_SQRT2 * double(q[2]);
  q3 = M_SQRT2 * double(q[3]);

  qda = q0 * q1;
  qdb = q0 * q2;
  qdc = q0 * q3;
  qaa = q1 * q1;
  qab = q1 * q2;
  qac = q1 * q3;
  qbb = q2 * q2;
  qbc = q2 * q3;
  qcc = q3 * q3;

  m[0][0] = float(1.0 - qbb - qcc);
  m[0][1] = float(qdc + qab);
  m[0][2] = float(-qdb + qac);
  m[0][3] = 0.0f;

  m[1][0] = float(-qdc + qab);
  m[1][1] = float(1.0 - qaa - qcc);
  m[1][2] = float(qda + qbc);
  m[1][3] = 0.0f;

  m[2][0] = float(qdb + qac);
  m[2][1] = float(-qda + qbc);
  m[2][2] = float(1.0 - qaa - qbb);
  m[2][3] = 0.0f;

  m[3][0] = m[3][1] = m[3][2] = 0.0f;
  m[3][3] = 1.0f;
}

void mat3_normalized_to_quat_fast(float q[4], const float mat[3][3])
{
  BLI_ASSERT_UNIT_M3(mat);
  /* Caller must ensure matrices aren't negative for valid results, see: #24291, #94231. */
  BLI_assert(!is_negative_m3(mat));

  /* Method outlined by Mike Day, ref: https://math.stackexchange.com/a/3183435/220949
   * with an additional `sqrtf(..)` for higher precision result.
   * Removing the `sqrt` causes tests to fail unless the precision is set to 1e-6 or larger. */

  if (mat[2][2] < 0.0f) {
    if (mat[0][0] > mat[1][1]) {
      const float trace = 1.0f + mat[0][0] - mat[1][1] - mat[2][2];
      float s = 2.0f * sqrtf(trace);
      if (mat[1][2] < mat[2][1]) {
        /* Ensure W is non-negative for a canonical result. */
        s = -s;
      }
      q[1] = 0.25f * s;
      s = 1.0f / s;
      q[0] = (mat[1][2] - mat[2][1]) * s;
      q[2] = (mat[0][1] + mat[1][0]) * s;
      q[3] = (mat[2][0] + mat[0][2]) * s;
      if (UNLIKELY((trace == 1.0f) && (q[0] == 0.0f && q[2] == 0.0f && q[3] == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q[1] = 1.0f;
      }
    }
    else {
      const float trace = 1.0f - mat[0][0] + mat[1][1] - mat[2][2];
      float s = 2.0f * sqrtf(trace);
      if (mat[2][0] < mat[0][2]) {
        /* Ensure W is non-negative for a canonical result. */
        s = -s;
      }
      q[2] = 0.25f * s;
      s = 1.0f / s;
      q[0] = (mat[2][0] - mat[0][2]) * s;
      q[1] = (mat[0][1] + mat[1][0]) * s;
      q[3] = (mat[1][2] + mat[2][1]) * s;
      if (UNLIKELY((trace == 1.0f) && (q[0] == 0.0f && q[1] == 0.0f && q[3] == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q[2] = 1.0f;
      }
    }
  }
  else {
    if (mat[0][0] < -mat[1][1]) {
      const float trace = 1.0f - mat[0][0] - mat[1][1] + mat[2][2];
      float s = 2.0f * sqrtf(trace);
      if (mat[0][1] < mat[1][0]) {
        /* Ensure W is non-negative for a canonical result. */
        s = -s;
      }
      q[3] = 0.25f * s;
      s = 1.0f / s;
      q[0] = (mat[0][1] - mat[1][0]) * s;
      q[1] = (mat[2][0] + mat[0][2]) * s;
      q[2] = (mat[1][2] + mat[2][1]) * s;
      if (UNLIKELY((trace == 1.0f) && (q[0] == 0.0f && q[1] == 0.0f && q[2] == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q[3] = 1.0f;
      }
    }
    else {
      /* NOTE(@ideasman42): A zero matrix will fall through to this block,
       * needed so a zero scaled matrices to return a quaternion without rotation, see: #101848. */
      const float trace = 1.0f + mat[0][0] + mat[1][1] + mat[2][2];
      float s = 2.0f * sqrtf(trace);
      q[0] = 0.25f * s;
      s = 1.0f / s;
      q[1] = (mat[1][2] - mat[2][1]) * s;
      q[2] = (mat[2][0] - mat[0][2]) * s;
      q[3] = (mat[0][1] - mat[1][0]) * s;
      if (UNLIKELY((trace == 1.0f) && (q[1] == 0.0f && q[2] == 0.0f && q[3] == 0.0f))) {
        /* Avoids the need to normalize the degenerate case. */
        q[0] = 1.0f;
      }
    }
  }

  BLI_assert(!(q[0] < 0.0f));

  /* Sometimes normalization is necessary due to round-off errors in the above
   * calculations. The comparison here uses tighter tolerances than
   * BLI_ASSERT_UNIT_QUAT(), so it's likely that even after a few more
   * transformations the quaternion will still be considered unit-ish. */
  const float q_len_squared = dot_qtqt(q, q);
  const float threshold = 0.0002f /* #BLI_ASSERT_UNIT_EPSILON */ * 3;
  if (fabs(q_len_squared - 1.0f) >= threshold) {
    normalize_qt(q);
  }
}

static void mat3_normalized_to_quat_with_checks(float q[4], float mat[3][3])
{
  const float det = determinant_m3_array(mat);
  if (UNLIKELY(!isfinite(det))) {
    unit_m3(mat);
  }
  else if (UNLIKELY(det < 0.0f)) {
    negate_m3(mat);
  }
  mat3_normalized_to_quat_fast(q, mat);
}

void mat3_normalized_to_quat(float q[4], const float mat[3][3])
{
  float unit_mat_abs[3][3];
  copy_m3_m3(unit_mat_abs, mat);
  mat3_normalized_to_quat_with_checks(q, unit_mat_abs);
}

void mat3_to_quat(float q[4], const float mat[3][3])
{
  float unit_mat_abs[3][3];
  normalize_m3_m3(unit_mat_abs, mat);
  mat3_normalized_to_quat_with_checks(q, unit_mat_abs);
}

void mat4_normalized_to_quat(float q[4], const float mat[4][4])
{
  float unit_mat_abs[3][3];
  copy_m3_m4(unit_mat_abs, mat);
  mat3_normalized_to_quat_with_checks(q, unit_mat_abs);
}

void mat4_to_quat(float q[4], const float mat[4][4])
{
  float unit_mat_abs[3][3];
  copy_m3_m4(unit_mat_abs, mat);
  normalize_m3(unit_mat_abs);
  mat3_normalized_to_quat_with_checks(q, unit_mat_abs);
}

void mat3_to_quat_legacy(float q[4], const float wmat[3][3])
{
  /* Legacy version of #mat3_to_quat which has slightly different behavior.
   * Keep for particle-system & boids since replacing this will make subtle changes
   * that impact hair in existing files. See: D15772. */

  float mat[3][3], matr[3][3], matn[3][3], q1[4], q2[4], angle, si, co, nor[3];

  /* work on a copy */
  copy_m3_m3(mat, wmat);
  normalize_m3(mat);

  /* rotate z-axis of matrix to z-axis */

  nor[0] = mat[2][1]; /* cross product with (0,0,1) */
  nor[1] = -mat[2][0];
  nor[2] = 0.0;
  normalize_v3(nor);

  co = mat[2][2];
  angle = 0.5f * safe_acosf(co);

  co = cosf(angle);
  si = sinf(angle);
  q1[0] = co;
  q1[1] = -nor[0] * si; /* negative here, but why? */
  q1[2] = -nor[1] * si;
  q1[3] = -nor[2] * si;

  /* rotate back x-axis from mat, using inverse q1 */
  quat_to_mat3_no_error(matr, q1);
  invert_m3_m3(matn, matr);
  mul_m3_v3(matn, mat[0]);

  /* and align x-axes */
  angle = 0.5f * atan2f(mat[0][1], mat[0][0]);

  co = cosf(angle);
  si = sinf(angle);
  q2[0] = co;
  q2[1] = 0.0f;
  q2[2] = 0.0f;
  q2[3] = si;

  mul_qt_qtqt(q, q1, q2);
}

float normalize_qt(float q[4])
{
  const float len = sqrtf(dot_qtqt(q, q));

  if (len != 0.0f) {
    mul_qt_fl(q, 1.0f / len);
  }
  else {
    q[1] = 1.0f;
    q[0] = q[2] = q[3] = 0.0f;
  }

  return len;
}

float normalize_qt_qt(float r[4], const float q[4])
{
  copy_qt_qt(r, q);
  return normalize_qt(r);
}

void rotation_between_vecs_to_mat3(float m[3][3], const float v1[3], const float v2[3])
{
  float axis[3];
  /* avoid calculating the angle */
  float angle_sin;
  float angle_cos;

  BLI_ASSERT_UNIT_V3(v1);
  BLI_ASSERT_UNIT_V3(v2);

  cross_v3_v3v3(axis, v1, v2);

  angle_sin = normalize_v3(axis);
  angle_cos = dot_v3v3(v1, v2);

  if (angle_sin > FLT_EPSILON) {
  axis_calc:
    BLI_ASSERT_UNIT_V3(axis);
    axis_angle_normalized_to_mat3_ex(m, axis, angle_sin, angle_cos);
    BLI_ASSERT_UNIT_M3(m);
  }
  else {
    if (angle_cos > 0.0f) {
      /* Same vectors, zero rotation... */
      unit_m3(m);
    }
    else {
      /* Colinear but opposed vectors, 180 rotation... */
      ortho_v3_v3(axis, v1);
      normalize_v3(axis);
      angle_sin = 0.0f;  /* sin(M_PI) */
      angle_cos = -1.0f; /* cos(M_PI) */
      goto axis_calc;
    }
  }
}

void rotation_between_vecs_to_quat(float q[4], const float v1[3], const float v2[3])
{
  float axis[3];

  cross_v3_v3v3(axis, v1, v2);

  if (normalize_v3(axis) > FLT_EPSILON) {
    float angle;

    angle = angle_normalized_v3v3(v1, v2);

    axis_angle_normalized_to_quat(q, axis, angle);
  }
  else {
    /* degenerate case */

    if (dot_v3v3(v1, v2) > 0.0f) {
      /* Same vectors, zero rotation... */
      unit_qt(q);
    }
    else {
      /* Colinear but opposed vectors, 180 rotation... */
      ortho_v3_v3(axis, v1);
      axis_angle_to_quat(q, axis, float(M_PI));
    }
  }
}

void rotation_between_quats_to_quat(float q[4], const float q1[4], const float q2[4])
{
  float tquat[4];

  conjugate_qt_qt(tquat, q1);

  mul_qt_fl(tquat, 1.0f / dot_qtqt(tquat, tquat));

  mul_qt_qtqt(q, tquat, q2);
}

float quat_split_swing_and_twist(const float q_in[4],
                                 const int axis,
                                 float r_swing[4],
                                 float r_twist[4])
{
  BLI_assert(axis >= 0 && axis <= 2);

  /* The calculation requires a canonical quaternion. */
  float q[4];

  if (q_in[0] < 0) {
    negate_v4_v4(q, q_in);
  }
  else {
    copy_v4_v4(q, q_in);
  }

  /* Half-twist angle can be computed directly. */
  float t = atan2f(q[axis + 1], q[0]);

  if (r_swing || r_twist) {
    float sin_t = sinf(t), cos_t = cosf(t);

    /* Compute swing by multiplying the original quaternion by inverted twist. */
    if (r_swing) {
      float twist_inv[4];

      twist_inv[0] = cos_t;
      zero_v3(twist_inv + 1);
      twist_inv[axis + 1] = -sin_t;

      mul_qt_qtqt(r_swing, q, twist_inv);

      BLI_assert(fabsf(r_swing[axis + 1]) < BLI_ASSERT_UNIT_EPSILON);
    }

    /* Output twist last just in case q overlaps r_twist. */
    if (r_twist) {
      r_twist[0] = cos_t;
      zero_v3(r_twist + 1);
      r_twist[axis + 1] = sin_t;
    }
  }

  return 2.0f * t;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Angle
 *
 * Unlike the angle between vectors, this does NOT return the shortest angle.
 * See signed functions below for this.
 *
 * \{ */

float angle_normalized_qt(const float q[4])
{
  BLI_ASSERT_UNIT_QUAT(q);
  return 2.0f * safe_acosf(q[0]);
}

float angle_qt(const float q[4])
{
  float tquat[4];

  normalize_qt_qt(tquat, q);

  return angle_normalized_qt(tquat);
}

float angle_normalized_qtqt(const float q1[4], const float q2[4])
{
  float qdelta[4];

  BLI_ASSERT_UNIT_QUAT(q1);
  BLI_ASSERT_UNIT_QUAT(q2);

  rotation_between_quats_to_quat(qdelta, q1, q2);

  return angle_normalized_qt(qdelta);
}

float angle_qtqt(const float q1[4], const float q2[4])
{
  float quat1[4], quat2[4];

  normalize_qt_qt(quat1, q1);
  normalize_qt_qt(quat2, q2);

  return angle_normalized_qtqt(quat1, quat2);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Angle (Signed)
 *
 * Angles with quaternion calculation can exceed 180d,
 * Having signed versions of these functions allows 'fabsf(angle_signed_qtqt(...))'
 * to give us the shortest angle between quaternions.
 * With higher precision than subtracting pi afterwards.
 *
 * \{ */

float angle_signed_normalized_qt(const float q[4])
{
  BLI_ASSERT_UNIT_QUAT(q);
  if (q[0] >= 0.0f) {
    return 2.0f * safe_acosf(q[0]);
  }

  return -2.0f * safe_acosf(-q[0]);
}

float angle_signed_normalized_qtqt(const float q1[4], const float q2[4])
{
  if (dot_qtqt(q1, q2) >= 0.0f) {
    return angle_normalized_qtqt(q1, q2);
  }

  float q2_copy[4];
  negate_v4_v4(q2_copy, q2);
  return -angle_normalized_qtqt(q1, q2_copy);
}

float angle_signed_qt(const float q[4])
{
  float tquat[4];

  normalize_qt_qt(tquat, q);

  return angle_signed_normalized_qt(tquat);
}

float angle_signed_qtqt(const float q1[4], const float q2[4])
{
  if (dot_qtqt(q1, q2) >= 0.0f) {
    return angle_qtqt(q1, q2);
  }

  float q2_copy[4];
  negate_v4_v4(q2_copy, q2);
  return -angle_qtqt(q1, q2_copy);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quaternion Vector
 * \{ */

void vec_to_quat(float q[4], const float vec[3], short axis, const short upflag)
{
  const float eps = 1e-4f;
  float nor[3], tvec[3];
  float angle, si, co, len;

  BLI_assert(axis >= 0 && axis <= 5);
  BLI_assert(upflag >= 0 && upflag <= 2);

  /* first set the quat to unit */
  unit_qt(q);

  len = len_v3(vec);

  if (UNLIKELY(len == 0.0f)) {
    return;
  }

  /* rotate to axis */
  if (axis > 2) {
    copy_v3_v3(tvec, vec);
    axis = short(axis - 3);
  }
  else {
    negate_v3_v3(tvec, vec);
  }

  /* nasty! I need a good routine for this...
   * problem is a rotation of an Y axis to the negative Y-axis for example.
   */

  if (axis == 0) { /* x-axis */
    nor[0] = 0.0;
    nor[1] = -tvec[2];
    nor[2] = tvec[1];

    if (fabsf(tvec[1]) + fabsf(tvec[2]) < eps) {
      nor[1] = 1.0f;
    }

    co = tvec[0];
  }
  else if (axis == 1) { /* y-axis */
    nor[0] = tvec[2];
    nor[1] = 0.0;
    nor[2] = -tvec[0];

    if (fabsf(tvec[0]) + fabsf(tvec[2]) < eps) {
      nor[2] = 1.0f;
    }

    co = tvec[1];
  }
  else { /* z-axis */
    nor[0] = -tvec[1];
    nor[1] = tvec[0];
    nor[2] = 0.0;

    if (fabsf(tvec[0]) + fabsf(tvec[1]) < eps) {
      nor[0] = 1.0f;
    }

    co = tvec[2];
  }
  co /= len;

  normalize_v3(nor);

  axis_angle_normalized_to_quat(q, nor, safe_acosf(co));

  if (axis != upflag) {
    float mat[3][3];
    float q2[4];
    const float *fp = mat[2];
    quat_to_mat3(mat, q);

    if (axis == 0) {
      if (upflag == 1) {
        angle = 0.5f * atan2f(fp[2], fp[1]);
      }
      else {
        angle = -0.5f * atan2f(fp[1], fp[2]);
      }
    }
    else if (axis == 1) {
      if (upflag == 0) {
        angle = -0.5f * atan2f(fp[2], fp[0]);
      }
      else {
        angle = 0.5f * atan2f(fp[0], fp[2]);
      }
    }
    else {
      if (upflag == 0) {
        angle = 0.5f * atan2f(-fp[1], -fp[0]);
      }
      else {
        angle = -0.5f * atan2f(-fp[0], -fp[1]);
      }
    }

    co = cosf(angle);
    si = sinf(angle) / len;
    q2[0] = co;
    q2[1] = tvec[0] * si;
    q2[2] = tvec[1] * si;
    q2[3] = tvec[2] * si;

    mul_qt_qtqt(q, q2, q);
  }
}

#if 0

/* A & M Watt, Advanced animation and rendering techniques, 1992 ACM press */
void QuatInterpolW(float *result, float quat1[4], float quat2[4], float t)
{
  float omega, cosom, sinom, sc1, sc2;

  cosom = quat1[0] * quat2[0] + quat1[1] * quat2[1] + quat1[2] * quat2[2] + quat1[3] * quat2[3];

  /* rotate around shortest angle */
  if ((1.0f + cosom) > 0.0001f) {

    if ((1.0f - cosom) > 0.0001f) {
      omega = float(acos(cosom));
      sinom = sinf(omega);
      sc1 = sinf((1.0 - t) * omega) / sinom;
      sc2 = sinf(t * omega) / sinom;
    }
    else {
      sc1 = 1.0f - t;
      sc2 = t;
    }
    result[0] = sc1 * quat1[0] + sc2 * quat2[0];
    result[1] = sc1 * quat1[1] + sc2 * quat2[1];
    result[2] = sc1 * quat1[2] + sc2 * quat2[2];
    result[3] = sc1 * quat1[3] + sc2 * quat2[3];
  }
  else {
    result[0] = quat2[3];
    result[1] = -quat2[2];
    result[2] = quat2[1];
    result[3] = -quat2[0];

    sc1 = sinf((1.0 - t) * M_PI_2);
    sc2 = sinf(t * M_PI_2);

    result[0] = sc1 * quat1[0] + sc2 * result[0];
    result[1] = sc1 * quat1[1] + sc2 * result[1];
    result[2] = sc1 * quat1[2] + sc2 * result[2];
    result[3] = sc1 * quat1[3] + sc2 * result[3];
  }
}
#endif

void interp_dot_slerp(const float t, const float cosom, float r_w[2])
{
  const float eps = 1e-4f;

  BLI_assert(IN_RANGE_INCL(cosom, -1.0001f, 1.0001f));

  /* within [-1..1] range, avoid aligned axis */
  if (LIKELY(fabsf(cosom) < (1.0f - eps))) {
    float omega, sinom;

    omega = acosf(cosom);
    sinom = sinf(omega);
    r_w[0] = sinf((1.0f - t) * omega) / sinom;
    r_w[1] = sinf(t * omega) / sinom;
  }
  else {
    /* fall back to lerp */
    r_w[0] = 1.0f - t;
    r_w[1] = t;
  }
}

void interp_qt_qtqt(float q[4], const float a[4], const float b[4], const float t)
{
  float quat[4], cosom, w[2];

  BLI_ASSERT_UNIT_QUAT(a);
  BLI_ASSERT_UNIT_QUAT(b);

  cosom = dot_qtqt(a, b);

  /* rotate around shortest angle */
  if (cosom < 0.0f) {
    cosom = -cosom;
    negate_v4_v4(quat, a);
  }
  else {
    copy_qt_qt(quat, a);
  }

  interp_dot_slerp(t, cosom, w);

  q[0] = w[0] * quat[0] + w[1] * b[0];
  q[1] = w[0] * quat[1] + w[1] * b[1];
  q[2] = w[0] * quat[2] + w[1] * b[2];
  q[3] = w[0] * quat[3] + w[1] * b[3];
}

void add_qt_qtqt(float q[4], const float a[4], const float b[4], const float t)
{
  q[0] = a[0] + t * b[0];
  q[1] = a[1] + t * b[1];
  q[2] = a[2] + t * b[2];
  q[3] = a[3] + t * b[3];
}

void tri_to_quat_ex(
    float quat[4], const float v1[3], const float v2[3], const float v3[3], const float no_orig[3])
{
  /* imaginary x-axis, y-axis triangle is being rotated */
  float vec[3], q1[4], q2[4], n[3], si, co, angle, mat[3][3], imat[3][3];

  /* move z-axis to face-normal */
#if 0
  normal_tri_v3(vec, v1, v2, v3);
#else
  copy_v3_v3(vec, no_orig);
  (void)v3;
#endif

  n[0] = vec[1];
  n[1] = -vec[0];
  n[2] = 0.0f;
  normalize_v3(n);

  if (n[0] == 0.0f && n[1] == 0.0f) {
    n[0] = 1.0f;
  }

  angle = -0.5f * safe_acosf(vec[2]);
  co = cosf(angle);
  si = sinf(angle);
  q1[0] = co;
  q1[1] = n[0] * si;
  q1[2] = n[1] * si;
  q1[3] = 0.0f;

  /* rotate back line v1-v2 */
  quat_to_mat3(mat, q1);
  invert_m3_m3(imat, mat);
  sub_v3_v3v3(vec, v2, v1);
  mul_m3_v3(imat, vec);

  /* what angle has this line with x-axis? */
  vec[2] = 0.0f;
  normalize_v3(vec);

  angle = 0.5f * atan2f(vec[1], vec[0]);
  co = cosf(angle);
  si = sinf(angle);
  q2[0] = co;
  q2[1] = 0.0f;
  q2[2] = 0.0f;
  q2[3] = si;

  mul_qt_qtqt(quat, q1, q2);
}

float tri_to_quat(float q[4], const float a[3], const float b[3], const float c[3])
{
  float vec[3];
  const float len = normal_tri_v3(vec, a, b, c);

  tri_to_quat_ex(q, a, b, c, vec);
  return len;
}

void sin_cos_from_fraction(int numerator, int denominator, float *r_sin, float *r_cos)
{
  /* By default, creating a circle from an integer: calling #sinf & #cosf on the fraction doesn't
   * create symmetrical values (because floats can't represent Pi exactly).
   * Resolve this when the rotation is calculated from a fraction by mapping the `numerator`
   * to lower values so X/Y values for points around a circle are exactly symmetrical, see #87779.
   *
   * Multiply both the `numerator` and `denominator` by eight to ensure we can divide the circle
   * into 8 octants. For each octant, we then use symmetry and negation to bring the `numerator`
   * closer to the origin where precision is highest.
   *
   * Cases 2, 4, 5 and 7, use the trigonometric identity sin(-x) == -sin(x).
   * Cases 1, 2, 5 and 6, swap the pointers `r_sin` and `r_cos`.
   */
  BLI_assert(0 <= numerator);
  BLI_assert(numerator <= denominator);
  BLI_assert(denominator > 0);

  numerator *= 8;                             /* Multiply numerator the same as denominator. */
  const int octant = numerator / denominator; /* Determine the octant. */
  denominator *= 8;                           /* Ensure denominator is a multiple of eight. */
  float cos_sign = 1.0f;                      /* Either 1.0f or -1.0f. */

  switch (octant) {
    case 0:
      /* Primary octant, nothing to do. */
      break;
    case 1:
    case 2:
      numerator = (denominator / 4) - numerator;
      SWAP(float *, r_sin, r_cos);
      break;
    case 3:
    case 4:
      numerator = (denominator / 2) - numerator;
      cos_sign = -1.0f;
      break;
    case 5:
    case 6:
      numerator = numerator - (denominator * 3 / 4);
      SWAP(float *, r_sin, r_cos);
      cos_sign = -1.0f;
      break;
    case 7:
      numerator = numerator - denominator;
      break;
    default:
      BLI_assert_unreachable();
  }

  BLI_assert(-denominator / 4 <= numerator); /* Numerator may be negative. */
  BLI_assert(numerator <= denominator / 4);
  BLI_assert(ELEM(cos_sign, -1.0f, 1.0f));

  const float angle = float(2.0 * M_PI) * (float(numerator) / float(denominator));
  *r_sin = sinf(angle);
  *r_cos = cosf(angle) * cos_sign;
}

void print_qt(const char *str, const float q[4])
{
  printf("%s: %.3f %.3f %.3f %.3f\n", str, q[0], q[1], q[2], q[3]);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Axis Angle
 * \{ */

void axis_angle_normalized_to_quat(float r[4], const float axis[3], const float angle)
{
  const float phi = 0.5f * angle;
  const float si = sinf(phi);
  const float co = cosf(phi);
  BLI_ASSERT_UNIT_V3(axis);
  r[0] = co;
  mul_v3_v3fl(r + 1, axis, si);
}

void axis_angle_to_quat(float r[4], const float axis[3], const float angle)
{
  float nor[3];

  if (LIKELY(normalize_v3_v3(nor, axis) != 0.0f)) {
    axis_angle_normalized_to_quat(r, nor, angle);
  }
  else {
    unit_qt(r);
  }
}

void quat_to_axis_angle(float axis[3], float *angle, const float q[4])
{
  float ha, si;

#ifndef NDEBUG
  if (!((ha = dot_qtqt(q, q)) == 0.0f || (fabsf(ha - 1.0f) < float(QUAT_EPSILON)))) {
    fprintf(stderr,
            "Warning! quat_to_axis_angle() called with non-normalized: size %.8f *** report a bug "
            "***\n",
            ha);
  }
#endif

  /* calculate angle/2, and sin(angle/2) */
  ha = acosf(q[0]);
  si = sinf(ha);

  /* from half-angle to angle */
  *angle = ha * 2;

  /* prevent division by zero for axis conversion */
  if (fabsf(si) < 0.0005f) {
    si = 1.0f;
  }

  axis[0] = q[1] / si;
  axis[1] = q[2] / si;
  axis[2] = q[3] / si;
  if (is_zero_v3(axis)) {
    axis[1] = 1.0f;
  }
}

void axis_angle_to_eulO(float eul[3], const short order, const float axis[3], const float angle)
{
  float q[4];

  /* use quaternions as intermediate representation for now... */
  axis_angle_to_quat(q, axis, angle);
  quat_to_eulO(eul, order, q);
}

void eulO_to_axis_angle(float axis[3], float *angle, const float eul[3], const short order)
{
  float q[4];

  /* use quaternions as intermediate representation for now... */
  eulO_to_quat(q, eul, order);
  quat_to_axis_angle(axis, angle, q);
}

void axis_angle_normalized_to_mat3_ex(float mat[3][3],
                                      const float axis[3],
                                      const float angle_sin,
                                      const float angle_cos)
{
  float nsi[3], ico;
  float n_00, n_01, n_11, n_02, n_12, n_22;

  BLI_ASSERT_UNIT_V3(axis);

  /* now convert this to a 3x3 matrix */

  ico = (1.0f - angle_cos);
  nsi[0] = axis[0] * angle_sin;
  nsi[1] = axis[1] * angle_sin;
  nsi[2] = axis[2] * angle_sin;

  n_00 = (axis[0] * axis[0]) * ico;
  n_01 = (axis[0] * axis[1]) * ico;
  n_11 = (axis[1] * axis[1]) * ico;
  n_02 = (axis[0] * axis[2]) * ico;
  n_12 = (axis[1] * axis[2]) * ico;
  n_22 = (axis[2] * axis[2]) * ico;

  mat[0][0] = n_00 + angle_cos;
  mat[0][1] = n_01 + nsi[2];
  mat[0][2] = n_02 - nsi[1];
  mat[1][0] = n_01 - nsi[2];
  mat[1][1] = n_11 + angle_cos;
  mat[1][2] = n_12 + nsi[0];
  mat[2][0] = n_02 + nsi[1];
  mat[2][1] = n_12 - nsi[0];
  mat[2][2] = n_22 + angle_cos;
}

void axis_angle_normalized_to_mat3(float R[3][3], const float axis[3], const float angle)
{
  axis_angle_normalized_to_mat3_ex(R, axis, sinf(angle), cosf(angle));
}

void axis_angle_to_mat3(float R[3][3], const float axis[3], const float angle)
{
  float nor[3];

  /* normalize the axis first (to remove unwanted scaling) */
  if (normalize_v3_v3(nor, axis) == 0.0f) {
    unit_m3(R);
    return;
  }

  axis_angle_normalized_to_mat3(R, nor, angle);
}

void axis_angle_to_mat4(float R[4][4], const float axis[3], const float angle)
{
  float tmat[3][3];

  axis_angle_to_mat3(tmat, axis, angle);
  unit_m4(R);
  copy_m4_m3(R, tmat);
}

void mat3_normalized_to_axis_angle(float axis[3], float *angle, const float mat[3][3])
{
  float q[4];

  /* use quaternions as intermediate representation */
  /* TODO: it would be nicer to go straight there... */
  mat3_normalized_to_quat(q, mat);
  quat_to_axis_angle(axis, angle, q);
}
void mat3_to_axis_angle(float axis[3], float *angle, const float mat[3][3])
{
  float q[4];

  /* use quaternions as intermediate representation */
  /* TODO: it would be nicer to go straight there... */
  mat3_to_quat(q, mat);
  quat_to_axis_angle(axis, angle, q);
}

void mat4_normalized_to_axis_angle(float axis[3], float *angle, const float mat[4][4])
{
  float q[4];

  /* use quaternions as intermediate representation */
  /* TODO: it would be nicer to go straight there... */
  mat4_normalized_to_quat(q, mat);
  quat_to_axis_angle(axis, angle, q);
}

void mat4_to_axis_angle(float axis[3], float *angle, const float mat[4][4])
{
  float q[4];

  /* use quaternions as intermediate representation */
  /* TODO: it would be nicer to go straight there... */
  mat4_to_quat(q, mat);
  quat_to_axis_angle(axis, angle, q);
}

void axis_angle_to_mat4_single(float R[4][4], const char axis, const float angle)
{
  float mat3[3][3];
  axis_angle_to_mat3_single(mat3, axis, angle);
  copy_m4_m3(R, mat3);
}

void axis_angle_to_mat3_single(float R[3][3], const char axis, const float angle)
{
  const float angle_cos = cosf(angle);
  const float angle_sin = sinf(angle);

  switch (axis) {
    case 'X': /* rotation around X */
      R[0][0] = 1.0f;
      R[0][1] = 0.0f;
      R[0][2] = 0.0f;
      R[1][0] = 0.0f;
      R[1][1] = angle_cos;
      R[1][2] = angle_sin;
      R[2][0] = 0.0f;
      R[2][1] = -angle_sin;
      R[2][2] = angle_cos;
      break;
    case 'Y': /* rotation around Y */
      R[0][0] = angle_cos;
      R[0][1] = 0.0f;
      R[0][2] = -angle_sin;
      R[1][0] = 0.0f;
      R[1][1] = 1.0f;
      R[1][2] = 0.0f;
      R[2][0] = angle_sin;
      R[2][1] = 0.0f;
      R[2][2] = angle_cos;
      break;
    case 'Z': /* rotation around Z */
      R[0][0] = angle_cos;
      R[0][1] = angle_sin;
      R[0][2] = 0.0f;
      R[1][0] = -angle_sin;
      R[1][1] = angle_cos;
      R[1][2] = 0.0f;
      R[2][0] = 0.0f;
      R[2][1] = 0.0f;
      R[2][2] = 1.0f;
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

void angle_to_mat2(float R[2][2], const float angle)
{
  const float angle_cos = cosf(angle);
  const float angle_sin = sinf(angle);

  /* 2D rotation matrix */
  R[0][0] = angle_cos;
  R[0][1] = angle_sin;
  R[1][0] = -angle_sin;
  R[1][1] = angle_cos;
}

void axis_angle_to_quat_single(float q[4], const char axis, const float angle)
{
  const float angle_half = angle * 0.5f;
  const float angle_cos = cosf(angle_half);
  const float angle_sin = sinf(angle_half);
  const int axis_index = (axis - 'X');

  BLI_assert(axis >= 'X' && axis <= 'Z');

  q[0] = angle_cos;
  zero_v3(q + 1);
  q[axis_index + 1] = angle_sin;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Exponential Map
 * \{ */

void quat_normalized_to_expmap(float expmap[3], const float q[4])
{
  float angle;
  BLI_ASSERT_UNIT_QUAT(q);

  /* Obtain axis/angle representation. */
  quat_to_axis_angle(expmap, &angle, q);

  /* Convert to exponential map. */
  mul_v3_fl(expmap, angle);
}

void quat_to_expmap(float expmap[3], const float q[4])
{
  float q_no[4];
  normalize_qt_qt(q_no, q);
  quat_normalized_to_expmap(expmap, q_no);
}

void expmap_to_quat(float r[4], const float expmap[3])
{
  float axis[3];
  float angle;

  /* Obtain axis/angle representation. */
  if (LIKELY((angle = normalize_v3_v3(axis, expmap)) != 0.0f)) {
    axis_angle_normalized_to_quat(r, axis, angle_wrap_rad(angle));
  }
  else {
    unit_qt(r);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XYZ Eulers
 * \{ */

void eul_to_mat3(float mat[3][3], const float eul[3])
{
  double ci, cj, ch, si, sj, sh, cc, cs, sc, ss;

  ci = cos(eul[0]);
  cj = cos(eul[1]);
  ch = cos(eul[2]);
  si = sin(eul[0]);
  sj = sin(eul[1]);
  sh = sin(eul[2]);
  cc = ci * ch;
  cs = ci * sh;
  sc = si * ch;
  ss = si * sh;

  mat[0][0] = float(cj * ch);
  mat[1][0] = float(sj * sc - cs);
  mat[2][0] = float(sj * cc + ss);
  mat[0][1] = float(cj * sh);
  mat[1][1] = float(sj * ss + cc);
  mat[2][1] = float(sj * cs - sc);
  mat[0][2] = float(-sj);
  mat[1][2] = float(cj * si);
  mat[2][2] = float(cj * ci);
}

void eul_to_mat4(float mat[4][4], const float eul[3])
{
  double ci, cj, ch, si, sj, sh, cc, cs, sc, ss;

  ci = cos(eul[0]);
  cj = cos(eul[1]);
  ch = cos(eul[2]);
  si = sin(eul[0]);
  sj = sin(eul[1]);
  sh = sin(eul[2]);
  cc = ci * ch;
  cs = ci * sh;
  sc = si * ch;
  ss = si * sh;

  mat[0][0] = float(cj * ch);
  mat[1][0] = float(sj * sc - cs);
  mat[2][0] = float(sj * cc + ss);
  mat[0][1] = float(cj * sh);
  mat[1][1] = float(sj * ss + cc);
  mat[2][1] = float(sj * cs - sc);
  mat[0][2] = float(-sj);
  mat[1][2] = float(cj * si);
  mat[2][2] = float(cj * ci);

  mat[3][0] = mat[3][1] = mat[3][2] = mat[0][3] = mat[1][3] = mat[2][3] = 0.0f;
  mat[3][3] = 1.0f;
}

/* returns two euler calculation methods, so we can pick the best */

/* XYZ order */
static void mat3_normalized_to_eul2(const float mat[3][3], float eul1[3], float eul2[3])
{
  const float cy = hypotf(mat[0][0], mat[0][1]);

  BLI_ASSERT_UNIT_M3(mat);

  if (cy > float(EULER_HYPOT_EPSILON)) {
    eul1[0] = atan2f(mat[1][2], mat[2][2]);
    eul1[1] = atan2f(-mat[0][2], cy);
    eul1[2] = atan2f(mat[0][1], mat[0][0]);

    eul2[0] = atan2f(-mat[1][2], -mat[2][2]);
    eul2[1] = atan2f(-mat[0][2], -cy);
    eul2[2] = atan2f(-mat[0][1], -mat[0][0]);
  }
  else {
    eul1[0] = atan2f(-mat[2][1], mat[1][1]);
    eul1[1] = atan2f(-mat[0][2], cy);
    eul1[2] = 0.0f;

    copy_v3_v3(eul2, eul1);
  }
}

void mat3_normalized_to_eul(float eul[3], const float mat[3][3])
{
  float eul1[3], eul2[3];

  mat3_normalized_to_eul2(mat, eul1, eul2);

  /* return best, which is just the one with lowest values it in */
  if (fabsf(eul1[0]) + fabsf(eul1[1]) + fabsf(eul1[2]) >
      fabsf(eul2[0]) + fabsf(eul2[1]) + fabsf(eul2[2]))
  {
    copy_v3_v3(eul, eul2);
  }
  else {
    copy_v3_v3(eul, eul1);
  }
}
void mat3_to_eul(float eul[3], const float mat[3][3])
{
  float unit_mat[3][3];
  normalize_m3_m3(unit_mat, mat);
  mat3_normalized_to_eul(eul, unit_mat);
}

void mat4_normalized_to_eul(float eul[3], const float m[4][4])
{
  float mat3[3][3];
  copy_m3_m4(mat3, m);
  mat3_normalized_to_eul(eul, mat3);
}
void mat4_to_eul(float eul[3], const float mat[4][4])
{
  float mat3[3][3];
  copy_m3_m4(mat3, mat);
  mat3_to_eul(eul, mat3);
}

void quat_to_eul(float eul[3], const float quat[4])
{
  float unit_mat[3][3];
  quat_to_mat3(unit_mat, quat);
  mat3_normalized_to_eul(eul, unit_mat);
}

void eul_to_quat(float quat[4], const float eul[3])
{
  float ti, tj, th, ci, cj, ch, si, sj, sh, cc, cs, sc, ss;

  ti = eul[0] * 0.5f;
  tj = eul[1] * 0.5f;
  th = eul[2] * 0.5f;
  ci = cosf(ti);
  cj = cosf(tj);
  ch = cosf(th);
  si = sinf(ti);
  sj = sinf(tj);
  sh = sinf(th);
  cc = ci * ch;
  cs = ci * sh;
  sc = si * ch;
  ss = si * sh;

  quat[0] = cj * cc + sj * ss;
  quat[1] = cj * sc - sj * cs;
  quat[2] = cj * ss + sj * cc;
  quat[3] = cj * cs - sj * sc;
}

void rotate_eul(float beul[3], const char axis, const float angle)
{
  float eul[3], mat1[3][3], mat2[3][3], totmat[3][3];

  BLI_assert(axis >= 'X' && axis <= 'Z');

  eul[0] = eul[1] = eul[2] = 0.0f;
  if (axis == 'X') {
    eul[0] = angle;
  }
  else if (axis == 'Y') {
    eul[1] = angle;
  }
  else {
    eul[2] = angle;
  }

  eul_to_mat3(mat1, eul);
  eul_to_mat3(mat2, beul);

  mul_m3_m3m3(totmat, mat2, mat1);

  mat3_to_eul(beul, totmat);
}

void compatible_eul(float eul[3], const float oldrot[3])
{
  /* When the rotation exceeds 180 degrees, it can be wrapped by 360 degrees
   * to produce a closer match.
   * NOTE: Values between `pi` & `2 * pi` work, where `pi` has the lowest number of
   * discontinuities and values approaching `2 * pi` center the resulting rotation around zero,
   * at the expense of the result being less compatible, see !104856. */
  const float pi_thresh = float(M_PI);
  const float pi_x2 = (2.0f * float(M_PI));

  float deul[3];
  uint i;

  /* Correct differences around 360 degrees first. */
  for (i = 0; i < 3; i++) {
    deul[i] = eul[i] - oldrot[i];
    if (deul[i] > pi_thresh) {
      eul[i] -= floorf((deul[i] / pi_x2) + 0.5f) * pi_x2;
      deul[i] = eul[i] - oldrot[i];
    }
    else if (deul[i] < -pi_thresh) {
      eul[i] += floorf((-deul[i] / pi_x2) + 0.5f) * pi_x2;
      deul[i] = eul[i] - oldrot[i];
    }
  }

  uint j = 1, k = 2;
  for (i = 0; i < 3; j = k, k = i++) {
    /* Check if this axis of rotations larger than 180 degrees and
     * the others are smaller than 90 degrees. */
    if (fabsf(deul[i]) > M_PI && fabsf(deul[j]) < M_PI_2 && fabsf(deul[k]) < M_PI_2) {
      if (deul[i] > 0.0f) {
        eul[i] -= pi_x2;
      }
      else {
        eul[i] += pi_x2;
      }
    }
  }
}

/* uses 2 methods to retrieve eulers, and picks the closest */

void mat3_normalized_to_compatible_eul(float eul[3], const float oldrot[3], float mat[3][3])
{
  float eul1[3], eul2[3];
  float d1, d2;

  mat3_normalized_to_eul2(mat, eul1, eul2);

  compatible_eul(eul1, oldrot);
  compatible_eul(eul2, oldrot);

  d1 = fabsf(eul1[0] - oldrot[0]) + fabsf(eul1[1] - oldrot[1]) + fabsf(eul1[2] - oldrot[2]);
  d2 = fabsf(eul2[0] - oldrot[0]) + fabsf(eul2[1] - oldrot[1]) + fabsf(eul2[2] - oldrot[2]);

  /* return best, which is just the one with lowest difference */
  if (d1 > d2) {
    copy_v3_v3(eul, eul2);
  }
  else {
    copy_v3_v3(eul, eul1);
  }
}
void mat3_to_compatible_eul(float eul[3], const float oldrot[3], float mat[3][3])
{
  float unit_mat[3][3];
  normalize_m3_m3(unit_mat, mat);
  mat3_normalized_to_compatible_eul(eul, oldrot, unit_mat);
}

void quat_to_compatible_eul(float eul[3], const float oldrot[3], const float quat[4])
{
  float unit_mat[3][3];
  quat_to_mat3(unit_mat, quat);
  mat3_normalized_to_compatible_eul(eul, oldrot, unit_mat);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Arbitrary Order Eulers
 * \{ */

/* Euler Rotation Order Code:
 * was adapted from
 *      ANSI C code from the article
 *      "Euler Angle Conversion"
 *      by Ken Shoemake <shoemake@graphics.cis.upenn.edu>
 *      in "Graphics Gems IV", Academic Press, 1994
 * for use in Blender
 */

/* Type for rotation order info - see wiki for derivation details */
struct RotOrderInfo {
  short axis[3];
  short parity; /* parity of axis permutation (even=0, odd=1) - 'n' in original code */
};

/* Array of info for Rotation Order calculations
 * WARNING: must be kept in same order as eEulerRotationOrders
 */
static const RotOrderInfo rotOrders[] = {
    /* i, j, k, n */
    {{0, 1, 2}, 0}, /* XYZ */
    {{0, 2, 1}, 1}, /* XZY */
    {{1, 0, 2}, 1}, /* YXZ */
    {{1, 2, 0}, 0}, /* YZX */
    {{2, 0, 1}, 0}, /* ZXY */
    {{2, 1, 0}, 1}  /* ZYX */
};

/* Get relevant pointer to rotation order set from the array
 * NOTE: since we start at 1 for the values, but arrays index from 0,
 *       there is -1 factor involved in this process...
 */
static const RotOrderInfo *get_rotation_order_info(const short order)
{
  BLI_assert(order >= 0 && order <= 6);
  if (order < 1) {
    return &rotOrders[0];
  }
  if (order < 6) {
    return &rotOrders[order - 1];
  }

  return &rotOrders[5];
}

void eulO_to_quat(float q[4], const float e[3], const short order)
{
  const RotOrderInfo *R = get_rotation_order_info(order);
  short i = R->axis[0], j = R->axis[1], k = R->axis[2];
  double ti, tj, th, ci, cj, ch, si, sj, sh, cc, cs, sc, ss;
  double a[3];

  ti = double(e[i]) * 0.5;
  tj = double(e[j]) * (R->parity ? -0.5 : 0.5);
  th = double(e[k]) * 0.5;

  ci = cos(ti);
  cj = cos(tj);
  ch = cos(th);
  si = sin(ti);
  sj = sin(tj);
  sh = sin(th);

  cc = ci * ch;
  cs = ci * sh;
  sc = si * ch;
  ss = si * sh;

  a[i] = cj * sc - sj * cs;
  a[j] = cj * ss + sj * cc;
  a[k] = cj * cs - sj * sc;

  q[0] = float(cj * cc + sj * ss);
  q[1] = float(a[0]);
  q[2] = float(a[1]);
  q[3] = float(a[2]);

  if (R->parity) {
    q[j + 1] = -q[j + 1];
  }
}

void quat_to_eulO(float e[3], short const order, const float q[4])
{
  float unit_mat[3][3];

  quat_to_mat3(unit_mat, q);
  mat3_normalized_to_eulO(e, order, unit_mat);
}

void eulO_to_mat3(float M[3][3], const float e[3], const short order)
{
  const RotOrderInfo *R = get_rotation_order_info(order);
  short i = R->axis[0], j = R->axis[1], k = R->axis[2];
  double ti, tj, th, ci, cj, ch, si, sj, sh, cc, cs, sc, ss;

  if (R->parity) {
    ti = -e[i];
    tj = -e[j];
    th = -e[k];
  }
  else {
    ti = e[i];
    tj = e[j];
    th = e[k];
  }

  ci = cos(ti);
  cj = cos(tj);
  ch = cos(th);
  si = sin(ti);
  sj = sin(tj);
  sh = sin(th);

  cc = ci * ch;
  cs = ci * sh;
  sc = si * ch;
  ss = si * sh;

  M[i][i] = float(cj * ch);
  M[j][i] = float(sj * sc - cs);
  M[k][i] = float(sj * cc + ss);
  M[i][j] = float(cj * sh);
  M[j][j] = float(sj * ss + cc);
  M[k][j] = float(sj * cs - sc);
  M[i][k] = float(-sj);
  M[j][k] = float(cj * si);
  M[k][k] = float(cj * ci);
}

/* returns two euler calculation methods, so we can pick the best */
static void mat3_normalized_to_eulo2(const float mat[3][3],
                                     float eul1[3],
                                     float eul2[3],
                                     const short order)
{
  const RotOrderInfo *R = get_rotation_order_info(order);
  short i = R->axis[0], j = R->axis[1], k = R->axis[2];
  float cy;

  BLI_ASSERT_UNIT_M3(mat);

  cy = hypotf(mat[i][i], mat[i][j]);

  if (cy > float(EULER_HYPOT_EPSILON)) {
    eul1[i] = atan2f(mat[j][k], mat[k][k]);
    eul1[j] = atan2f(-mat[i][k], cy);
    eul1[k] = atan2f(mat[i][j], mat[i][i]);

    eul2[i] = atan2f(-mat[j][k], -mat[k][k]);
    eul2[j] = atan2f(-mat[i][k], -cy);
    eul2[k] = atan2f(-mat[i][j], -mat[i][i]);
  }
  else {
    eul1[i] = atan2f(-mat[k][j], mat[j][j]);
    eul1[j] = atan2f(-mat[i][k], cy);
    eul1[k] = 0;

    copy_v3_v3(eul2, eul1);
  }

  if (R->parity) {
    negate_v3(eul1);
    negate_v3(eul2);
  }
}

void eulO_to_mat4(float mat[4][4], const float e[3], const short order)
{
  float unit_mat[3][3];

  /* for now, we'll just do this the slow way (i.e. copying matrices) */
  eulO_to_mat3(unit_mat, e, order);
  copy_m4_m3(mat, unit_mat);
}

void mat3_normalized_to_eulO(float eul[3], const short order, const float m[3][3])
{
  float eul1[3], eul2[3];
  float d1, d2;

  mat3_normalized_to_eulo2(m, eul1, eul2, order);

  d1 = fabsf(eul1[0]) + fabsf(eul1[1]) + fabsf(eul1[2]);
  d2 = fabsf(eul2[0]) + fabsf(eul2[1]) + fabsf(eul2[2]);

  /* return best, which is just the one with lowest values it in */
  if (d1 > d2) {
    copy_v3_v3(eul, eul2);
  }
  else {
    copy_v3_v3(eul, eul1);
  }
}
void mat3_to_eulO(float eul[3], const short order, const float m[3][3])
{
  float unit_mat[3][3];
  normalize_m3_m3(unit_mat, m);
  mat3_normalized_to_eulO(eul, order, unit_mat);
}

void mat4_normalized_to_eulO(float eul[3], const short order, const float m[4][4])
{
  float mat3[3][3];

  /* for now, we'll just do this the slow way (i.e. copying matrices) */
  copy_m3_m4(mat3, m);
  mat3_normalized_to_eulO(eul, order, mat3);
}

void mat4_to_eulO(float eul[3], const short order, const float m[4][4])
{
  float mat3[3][3];
  copy_m3_m4(mat3, m);
  normalize_m3(mat3);
  mat3_normalized_to_eulO(eul, order, mat3);
}

void mat3_normalized_to_compatible_eulO(float eul[3],
                                        const float oldrot[3],
                                        const short order,
                                        const float mat[3][3])
{
  float eul1[3], eul2[3];
  float d1, d2;

  mat3_normalized_to_eulo2(mat, eul1, eul2, order);

  compatible_eul(eul1, oldrot);
  compatible_eul(eul2, oldrot);

  d1 = fabsf(eul1[0] - oldrot[0]) + fabsf(eul1[1] - oldrot[1]) + fabsf(eul1[2] - oldrot[2]);
  d2 = fabsf(eul2[0] - oldrot[0]) + fabsf(eul2[1] - oldrot[1]) + fabsf(eul2[2] - oldrot[2]);

  /* return best, which is just the one with lowest difference */
  if (d1 > d2) {
    copy_v3_v3(eul, eul2);
  }
  else {
    copy_v3_v3(eul, eul1);
  }
}
void mat3_to_compatible_eulO(float eul[3],
                             const float oldrot[3],
                             const short order,
                             const float mat[3][3])
{
  float unit_mat[3][3];

  normalize_m3_m3(unit_mat, mat);
  mat3_normalized_to_compatible_eulO(eul, oldrot, order, unit_mat);
}

void mat4_normalized_to_compatible_eulO(float eul[3],
                                        const float oldrot[3],
                                        const short order,
                                        const float mat[4][4])
{
  float mat3[3][3];

  /* for now, we'll just do this the slow way (i.e. copying matrices) */
  copy_m3_m4(mat3, mat);
  mat3_normalized_to_compatible_eulO(eul, oldrot, order, mat3);
}
void mat4_to_compatible_eulO(float eul[3],
                             const float oldrot[3],
                             const short order,
                             const float mat[4][4])
{
  float mat3[3][3];

  /* for now, we'll just do this the slow way (i.e. copying matrices) */
  copy_m3_m4(mat3, mat);
  normalize_m3(mat3);
  mat3_normalized_to_compatible_eulO(eul, oldrot, order, mat3);
}

void quat_to_compatible_eulO(float eul[3],
                             const float oldrot[3],
                             const short order,
                             const float quat[4])
{
  float unit_mat[3][3];

  quat_to_mat3(unit_mat, quat);
  mat3_normalized_to_compatible_eulO(eul, oldrot, order, unit_mat);
}

/* rotate the given euler by the given angle on the specified axis */
/* NOTE: is this safe to do with different axis orders? */

void rotate_eulO(float beul[3], const short order, const char axis, const float angle)
{
  float eul[3], mat1[3][3], mat2[3][3], totmat[3][3];

  BLI_assert(axis >= 'X' && axis <= 'Z');

  zero_v3(eul);

  if (axis == 'X') {
    eul[0] = angle;
  }
  else if (axis == 'Y') {
    eul[1] = angle;
  }
  else {
    eul[2] = angle;
  }

  eulO_to_mat3(mat1, eul, order);
  eulO_to_mat3(mat2, beul, order);

  mul_m3_m3m3(totmat, mat2, mat1);

  mat3_to_eulO(beul, order, totmat);
}

void eulO_to_gimbal_axis(float gmat[3][3], const float eul[3], const short order)
{
  const RotOrderInfo *R = get_rotation_order_info(order);

  float mat[3][3];
  float teul[3];

  /* first axis is local */
  eulO_to_mat3(mat, eul, order);
  copy_v3_v3(gmat[R->axis[0]], mat[R->axis[0]]);

  /* second axis is local minus first rotation */
  copy_v3_v3(teul, eul);
  teul[R->axis[0]] = 0;
  eulO_to_mat3(mat, teul, order);
  copy_v3_v3(gmat[R->axis[1]], mat[R->axis[1]]);

  /* Last axis is global */
  zero_v3(gmat[R->axis[2]]);
  gmat[R->axis[2]][R->axis[2]] = 1;
}

void add_eul_euleul(float r_eul[3], float a[3], float b[3], const short order)
{
  float quat[4], quat_b[4];

  eulO_to_quat(quat, a, order);
  eulO_to_quat(quat_b, b, order);

  mul_qt_qtqt(quat, quat_b, quat);

  quat_to_eulO(r_eul, order, quat);
}

void sub_eul_euleul(float r_eul[3], float a[3], float b[3], const short order)
{
  float quat[4], quat_b[4];

  eulO_to_quat(quat, a, order);
  eulO_to_quat(quat_b, b, order);

  invert_qt_normalized(quat_b);
  mul_qt_qtqt(quat, quat_b, quat);

  quat_to_eulO(r_eul, order, quat);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dual Quaternions
 * \{ */

/* Conversion routines between (regular quaternion, translation) and dual quaternion.
 *
 * Version 1.0.0, February 7th, 2007
 *
 * SPDX-License-Identifier: Zlib
 * Copyright 2006-2007 University of Dublin, Trinity College, All Rights Reserved.
 *
 * Changes for Blender:
 * - renaming, style changes and optimization's
 * - added support for scaling
 */

[505 more lines in file. Use offset=2001 to continue.]