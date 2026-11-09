/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_solvers.h"
#include "BLI_math_vector.h"
#include "BLI_simd.hh"

#ifndef MATH_STANDALONE
#  include "eigen_capi.h"
#endif

#include <algorithm>
#include <cstring>

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Init
 * \{ */

void zero_m3(float m[3][3])
{
  memset(m, 0, sizeof(float[3][3]));
}

void zero_m4(float m[4][4])
{
  memset(m, 0, sizeof(float[4][4]));
}

void unit_m2(float m[2][2])
{
  m[0][0] = m[1][1] = 1.0f;
  m[0][1] = 0.0f;
  m[1][0] = 0.0f;
}

void unit_m3(float m[3][3])
{
  m[0][0] = m[1][1] = m[2][2] = 1.0f;
  m[0][1] = m[0][2] = 0.0f;
  m[1][0] = m[1][2] = 0.0f;
  m[2][0] = m[2][1] = 0.0f;
}

void unit_m4(float m[4][4])
{
  m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
  m[0][1] = m[0][2] = m[0][3] = 0.0f;
  m[1][0] = m[1][2] = m[1][3] = 0.0f;
  m[2][0] = m[2][1] = m[2][3] = 0.0f;
  m[3][0] = m[3][1] = m[3][2] = 0.0f;
}

void unit_m4_db(double m[4][4])
{
  m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
  m[0][1] = m[0][2] = m[0][3] = 0.0f;
  m[1][0] = m[1][2] = m[1][3] = 0.0f;
  m[2][0] = m[2][1] = m[2][3] = 0.0f;
  m[3][0] = m[3][1] = m[3][2] = 0.0f;
}

void copy_m2_m2(float m1[2][2], const float m2[2][2])
{
  memcpy(m1, m2, sizeof(float[2][2]));
}

void copy_m3_m3(float m1[3][3], const float m2[3][3])
{
  /* destination comes first: */
  memcpy(m1, m2, sizeof(float[3][3]));
}

void copy_m4_m4(float m1[4][4], const float m2[4][4])
{
  memcpy(m1, m2, sizeof(float[4][4]));
}

void copy_m4_m4_db(double m1[4][4], const double m2[4][4])
{
  memcpy(m1, m2, sizeof(double[4][4]));
}

void copy_m3_m4(float m1[3][3], const float m2[4][4])
{
  m1[0][0] = m2[0][0];
  m1[0][1] = m2[0][1];
  m1[0][2] = m2[0][2];

  m1[1][0] = m2[1][0];
  m1[1][1] = m2[1][1];
  m1[1][2] = m2[1][2];

  m1[2][0] = m2[2][0];
  m1[2][1] = m2[2][1];
  m1[2][2] = m2[2][2];
}

void copy_m4_m3(float m1[4][4], const float m2[3][3]) /* no clear */
{
  m1[0][0] = m2[0][0];
  m1[0][1] = m2[0][1];
  m1[0][2] = m2[0][2];

  m1[1][0] = m2[1][0];
  m1[1][1] = m2[1][1];
  m1[1][2] = m2[1][2];

  m1[2][0] = m2[2][0];
  m1[2][1] = m2[2][1];
  m1[2][2] = m2[2][2];

  m1[0][3] = 0.0f;
  m1[1][3] = 0.0f;
  m1[2][3] = 0.0f;

  m1[3][0] = 0.0f;
  m1[3][1] = 0.0f;
  m1[3][2] = 0.0f;
  m1[3][3] = 1.0f;
}

void copy_m3d_m3(double m1[3][3], const float m2[3][3])
{
  m1[0][0] = m2[0][0];
  m1[0][1] = m2[0][1];
  m1[0][2] = m2[0][2];

  m1[1][0] = m2[1][0];
  m1[1][1] = m2[1][1];
  m1[1][2] = m2[1][2];

  m1[2][0] = m2[2][0];
  m1[2][1] = m2[2][1];
  m1[2][2] = m2[2][2];
}

void copy_m4d_m4(double m1[4][4], const float m2[4][4])
{
  m1[0][0] = m2[0][0];
  m1[0][1] = m2[0][1];
  m1[0][2] = m2[0][2];
  m1[0][3] = m2[0][3];

  m1[1][0] = m2[1][0];
  m1[1][1] = m2[1][1];
  m1[1][2] = m2[1][2];
  m1[1][3] = m2[1][3];

  m1[2][0] = m2[2][0];
  m1[2][1] = m2[2][1];
  m1[2][2] = m2[2][2];
  m1[2][3] = m2[2][3];

  m1[3][0] = m2[3][0];
  m1[3][1] = m2[3][1];
  m1[3][2] = m2[3][2];
  m1[3][3] = m2[3][3];
}

void copy_m3_m3d(float m1[3][3], const double m2[3][3])
{
  /* Keep it stupid simple for better data flow in CPU. */
  m1[0][0] = float(m2[0][0]);
  m1[0][1] = float(m2[0][1]);
  m1[0][2] = float(m2[0][2]);

  m1[1][0] = float(m2[1][0]);
  m1[1][1] = float(m2[1][1]);
  m1[1][2] = float(m2[1][2]);

  m1[2][0] = float(m2[2][0]);
  m1[2][1] = float(m2[2][1]);
  m1[2][2] = float(m2[2][2]);
}

void swap_m4m4(float m1[4][4], float m2[4][4])
{
  float t;
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      t = m1[i][j];
      m1[i][j] = m2[i][j];
      m2[i][j] = t;
    }
  }
}

void shuffle_m4(float R[4][4], const int index[4])
{
  zero_m4(R);
  for (int k = 0; k < 4; k++) {
    if (index[k] >= 0) {
      R[index[k]][k] = 1.0f;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Arithmetic
 * \{ */

void mul_m4_m4m4(float R[4][4], const float A[4][4], const float B[4][4])
{
  if (ELEM(R, A, B)) {
    float T[4][4];
    mul_m4_m4m4(T, A, B);
    copy_m4_m4(R, T);
    return;
  }

  /* Matrix product: `R[j][k] = B[j][i] . A[i][k]`. */
#if BLI_HAVE_SSE2
  __m128 A0 = _mm_loadu_ps(A[0]);
  __m128 A1 = _mm_loadu_ps(A[1]);
  __m128 A2 = _mm_loadu_ps(A[2]);
  __m128 A3 = _mm_loadu_ps(A[3]);

  for (int i = 0; i < 4; i++) {
    __m128 B0 = _mm_set1_ps(B[i][0]);
    __m128 B1 = _mm_set1_ps(B[i][1]);
    __m128 B2 = _mm_set1_ps(B[i][2]);
    __m128 B3 = _mm_set1_ps(B[i][3]);

    __m128 sum = _mm_add_ps(_mm_add_ps(_mm_mul_ps(B0, A0), _mm_mul_ps(B1, A1)),
                            _mm_add_ps(_mm_mul_ps(B2, A2), _mm_mul_ps(B3, A3)));

    _mm_storeu_ps(R[i], sum);
  }
#else
  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0] + B[0][3] * A[3][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1] + B[0][3] * A[3][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2] + B[0][3] * A[3][2];
  R[0][3] = B[0][0] * A[0][3] + B[0][1] * A[1][3] + B[0][2] * A[2][3] + B[0][3] * A[3][3];

  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0] + B[1][3] * A[3][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1] + B[1][3] * A[3][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2] + B[1][3] * A[3][2];
  R[1][3] = B[1][0] * A[0][3] + B[1][1] * A[1][3] + B[1][2] * A[2][3] + B[1][3] * A[3][3];

  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0] + B[2][3] * A[3][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1] + B[2][3] * A[3][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2] + B[2][3] * A[3][2];
  R[2][3] = B[2][0] * A[0][3] + B[2][1] * A[1][3] + B[2][2] * A[2][3] + B[2][3] * A[3][3];

  R[3][0] = B[3][0] * A[0][0] + B[3][1] * A[1][0] + B[3][2] * A[2][0] + B[3][3] * A[3][0];
  R[3][1] = B[3][0] * A[0][1] + B[3][1] * A[1][1] + B[3][2] * A[2][1] + B[3][3] * A[3][1];
  R[3][2] = B[3][0] * A[0][2] + B[3][1] * A[1][2] + B[3][2] * A[2][2] + B[3][3] * A[3][2];
  R[3][3] = B[3][0] * A[0][3] + B[3][1] * A[1][3] + B[3][2] * A[2][3] + B[3][3] * A[3][3];
#endif
}

void mul_m4db_m4db_m4fl(double R[4][4], const double A[4][4], const float B[4][4])
{
  if (R == A) {
    double T[4][4];
    mul_m4db_m4db_m4fl(T, A, B);
    copy_m4_m4_db(R, T);
    return;
  }

  /* Matrix product: `R[j][k] = B[j][i] . A[i][k]`. */

  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0] + B[0][3] * A[3][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1] + B[0][3] * A[3][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2] + B[0][3] * A[3][2];
  R[0][3] = B[0][0] * A[0][3] + B[0][1] * A[1][3] + B[0][2] * A[2][3] + B[0][3] * A[3][3];

  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0] + B[1][3] * A[3][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1] + B[1][3] * A[3][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2] + B[1][3] * A[3][2];
  R[1][3] = B[1][0] * A[0][3] + B[1][1] * A[1][3] + B[1][2] * A[2][3] + B[1][3] * A[3][3];

  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0] + B[2][3] * A[3][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1] + B[2][3] * A[3][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2] + B[2][3] * A[3][2];
  R[2][3] = B[2][0] * A[0][3] + B[2][1] * A[1][3] + B[2][2] * A[2][3] + B[2][3] * A[3][3];

  R[3][0] = B[3][0] * A[0][0] + B[3][1] * A[1][0] + B[3][2] * A[2][0] + B[3][3] * A[3][0];
  R[3][1] = B[3][0] * A[0][1] + B[3][1] * A[1][1] + B[3][2] * A[2][1] + B[3][3] * A[3][1];
  R[3][2] = B[3][0] * A[0][2] + B[3][1] * A[1][2] + B[3][2] * A[2][2] + B[3][3] * A[3][2];
  R[3][3] = B[3][0] * A[0][3] + B[3][1] * A[1][3] + B[3][2] * A[2][3] + B[3][3] * A[3][3];
}

void mul_m4_m4_pre(float R[4][4], const float A[4][4])
{
  mul_m4_m4m4(R, A, R);
}

void mul_m4_m4_post(float R[4][4], const float B[4][4])
{
  mul_m4_m4m4(R, R, B);
}

void mul_m3_m3_pre(float R[3][3], const float A[3][3])
{
  mul_m3_m3m3(R, A, R);
}

void mul_m3_m3_post(float R[3][3], const float B[3][3])
{
  mul_m3_m3m3(R, R, B);
}

void mul_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3])
{
  if (ELEM(R, A, B)) {
    float T[3][3];
    mul_m3_m3m3(T, A, B);
    copy_m3_m3(R, T);
    return;
  }
  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2];

  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2];

  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2];
}

void mul_m4_m4m3(float R[4][4], const float A[4][4], const float B[3][3])
{
  if (R == A) {
    float T[4][4];
    /* The mul_m4_m4m3 only writes to the upper-left 3x3 block, so make it so the rest of the
     * matrix is copied from the input to the output.
     *
     * TODO(sergey): It does sound a bit redundant from the number of copy operations, so there is
     * a potential for optimization. */
    copy_m4_m4(T, A);
    mul_m4_m4m3(T, A, B);
    copy_m4_m4(R, T);
    return;
  }

  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2];
  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2];
  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2];
}

void mul_m3_m3m4(float R[3][3], const float A[3][3], const float B[4][4])
{
  if (R == A) {
    float T[3][3];
    mul_m3_m3m4(T, A, B);
    copy_m3_m3(R, T);
    return;
  }

  /* Matrix product: `R[j][k] = B[j][i] . A[i][k]`. */

  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2];

  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2];

  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2];
}

void mul_m3_m4m3(float R[3][3], const float A[4][4], const float B[3][3])
{
  if (R == B) {
    float T[3][3];
    mul_m3_m4m3(T, A, B);
    copy_m3_m3(R, T);
    return;
  }

  /* Matrix product: `R[j][k] = B[j][i] . A[i][k]`. */

  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2];

  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2];

  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2];
}

void mul_m4_m3m4(float R[4][4], const float A[3][3], const float B[4][4])
{
  if (R == B) {
    float T[4][4];
    /* The mul_m4_m4m3 only writes to the upper-left 3x3 block, so make it so the rest of the
     * matrix is copied from the input to the output.
     *
     * TODO(sergey): It does sound a bit redundant from the number of copy operations, so there is
     * a potential for optimization. */
    copy_m4_m4(T, B);
    mul_m4_m3m4(T, A, B);
    copy_m4_m4(R, T);
    return;
  }

  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2];
  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2];
  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2];
}

void mul_m3_m4m4(float R[3][3], const float A[4][4], const float B[4][4])
{
  R[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0] + B[0][2] * A[2][0];
  R[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1] + B[0][2] * A[2][1];
  R[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2] * A[2][2];
  R[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0] + B[1][2] * A[2][0];
  R[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1] + B[1][2] * A[2][1];
  R[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2] * A[2][2];
  R[2][0] = B[2][0] * A[0][0] + B[2][1] * A[1][0] + B[2][2] * A[2][0];
  R[2][1] = B[2][0] * A[0][1] + B[2][1] * A[1][1] + B[2][2] * A[2][1];
  R[2][2] = B[2][0] * A[0][2] + B[2][1] * A[1][2] + B[2][2] * A[2][2];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Macro helpers for: mul_m3_series
 * \{ */

void _va_mul_m3_series_3(float r[3][3], const float m1[3][3], const float m2[3][3])
{
  mul_m3_m3m3(r, m1, m2);
}
void _va_mul_m3_series_4(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3])
{
  float s[3][3];
  mul_m3_m3m3(s, m1, m2);
  mul_m3_m3m3(r, s, m3);
}
void _va_mul_m3_series_5(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3])
{
  float s[3][3];
  float t[3][3];
  mul_m3_m3m3(s, m1, m2);
  mul_m3_m3m3(t, s, m3);
  mul_m3_m3m3(r, t, m4);
}
void _va_mul_m3_series_6(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3],
                         const float m5[3][3])
{
  float s[3][3];
  float t[3][3];
  mul_m3_m3m3(s, m1, m2);
  mul_m3_m3m3(t, s, m3);
  mul_m3_m3m3(s, t, m4);
  mul_m3_m3m3(r, s, m5);
}
void _va_mul_m3_series_7(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3],
                         const float m5[3][3],
                         const float m6[3][3])
{
  float s[3][3];
  float t[3][3];
  mul_m3_m3m3(s, m1, m2);
  mul_m3_m3m3(t, s, m3);
  mul_m3_m3m3(s, t, m4);
  mul_m3_m3m3(t, s, m5);
  mul_m3_m3m3(r, t, m6);
}
void _va_mul_m3_series_8(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3],
                         const float m5[3][3],
                         const float m6[3][3],
                         const float m7[3][3])
{
  float s[3][3];
  float t[3][3];
  mul_m3_m3m3(s, m1, m2);
  mul_m3_m3m3(t, s, m3);
  mul_m3_m3m3(s, t, m4);
  mul_m3_m3m3(t, s, m5);
  mul_m3_m3m3(s, t, m6);
  mul_m3_m3m3(r, s, m7);
}
void _va_mul_m3_series_9(float r[3][3],
                         const float m1[3][3],
                         const float m2[3][3],
                         const float m3[3][3],
                         const float m4[3][3],
                         const float m5[3][3],
                         const float m6[3][3],
                         const float m7[3][3],
                         const float m8[3][3])
{
  float s[3][3];
  float t[3][3];
  mul_m3_m3m3(s, m1, m2);
  mul_m3_m3m3(t, s, m3);
  mul_m3_m3m3(s, t, m4);
  mul_m3_m3m3(t, s, m5);
  mul_m3_m3m3(s, t, m6);
  mul_m3_m3m3(t, s, m7);
  mul_m3_m3m3(r, t, m8);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Macro helpers for: mul_m4_series
 * \{ */

void _va_mul_m4_series_3(float r[4][4], const float m1[4][4], const float m2[4][4])
{
  mul_m4_m4m4(r, m1, m2);
}
void _va_mul_m4_series_4(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4])
{
  float s[4][4];
  mul_m4_m4m4(s, m1, m2);
  mul_m4_m4m4(r, s, m3);
}
void _va_mul_m4_series_5(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4])
{
  float s[4][4];
  float t[4][4];
  mul_m4_m4m4(s, m1, m2);
  mul_m4_m4m4(t, s, m3);
  mul_m4_m4m4(r, t, m4);
}
void _va_mul_m4_series_6(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4],
                         const float m5[4][4])
{
  float s[4][4];
  float t[4][4];
  mul_m4_m4m4(s, m1, m2);
  mul_m4_m4m4(t, s, m3);
  mul_m4_m4m4(s, t, m4);
  mul_m4_m4m4(r, s, m5);
}
void _va_mul_m4_series_7(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4],
                         const float m5[4][4],
                         const float m6[4][4])
{
  float s[4][4];
  float t[4][4];
  mul_m4_m4m4(s, m1, m2);
  mul_m4_m4m4(t, s, m3);
  mul_m4_m4m4(s, t, m4);
  mul_m4_m4m4(t, s, m5);
  mul_m4_m4m4(r, t, m6);
}
void _va_mul_m4_series_8(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4],
                         const float m5[4][4],
                         const float m6[4][4],
                         const float m7[4][4])
{
  float s[4][4];
  float t[4][4];
  mul_m4_m4m4(s, m1, m2);
  mul_m4_m4m4(t, s, m3);
  mul_m4_m4m4(s, t, m4);
  mul_m4_m4m4(t, s, m5);
  mul_m4_m4m4(s, t, m6);
  mul_m4_m4m4(r, s, m7);
}
void _va_mul_m4_series_9(float r[4][4],
                         const float m1[4][4],
                         const float m2[4][4],
                         const float m3[4][4],
                         const float m4[4][4],
                         const float m5[4][4],
                         const float m6[4][4],
                         const float m7[4][4],
                         const float m8[4][4])
{
  float s[4][4];
  float t[4][4];
  mul_m4_m4m4(s, m1, m2);
  mul_m4_m4m4(t, s, m3);
  mul_m4_m4m4(s, t, m4);
  mul_m4_m4m4(t, s, m5);
  mul_m4_m4m4(s, t, m6);
  mul_m4_m4m4(t, s, m7);
  mul_m4_m4m4(r, t, m8);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Arithmetic
 * \{ */

void mul_v2_m3v2(float r[2], const float m[3][3], const float v[2])
{
  float temp[3], warped[3];

  copy_v2_v2(temp, v);
  temp[2] = 1.0f;

  mul_v3_m3v3(warped, m, temp);

  r[0] = warped[0] / warped[2];
  r[1] = warped[1] / warped[2];
}

void mul_m3_v2(const float m[3][3], float r[2])
{
  mul_v2_m3v2(r, m, r);
}

void mul_m4_v3(const float M[4][4], float r[3])
{
  const float x = r[0];
  const float y = r[1];

  r[0] = x * M[0][0] + y * M[1][0] + M[2][0] * r[2] + M[3][0];
  r[1] = x * M[0][1] + y * M[1][1] + M[2][1] * r[2] + M[3][1];
  r[2] = x * M[0][2] + y * M[1][2] + M[2][2] * r[2] + M[3][2];
}

void mul_v3_m4v3(float r[3], const float mat[4][4], const float vec[3])
{
  const float x = vec[0];
  const float y = vec[1];

  r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2] + mat[3][0];
  r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2] + mat[3][1];
  r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2] + mat[3][2];
}

void mul_v3_m4v3_db(double r[3], const double mat[4][4], const double vec[3])
{
  const double x = vec[0];
  const double y = vec[1];

  r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2] + mat[3][0];
  r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2] + mat[3][1];
  r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2] + mat[3][2];
}
void mul_v4_m4v3_db(double r[4], const double mat[4][4], const double vec[3])
{
  const double x = vec[0];
  const double y = vec[1];

  r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2] + mat[3][0];
  r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2] + mat[3][1];
  r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2] + mat[3][2];
  r[3] = x * mat[0][3] + y * mat[1][3] + mat[2][3] * vec[2] + mat[3][3];
}

void mul_v2_m4v3(float r[2], const float mat[4][4], const float vec[3])
{
  const float x = vec[0];

  r[0] = x * mat[0][0] + vec[1] * mat[1][0] + mat[2][0] * vec[2] + mat[3][0];
  r[1] = x * mat[0][1] + vec[1] * mat[1][1] + mat[2][1] * vec[2] + mat[3][1];
}

void mul_v2_m2v2(float r[2], const float mat[2][2], const float vec[2])
{
  const float x = vec[0];

  r[0] = mat[0][0] * x + mat[1][0] * vec[1];
  r[1] = mat[0][1] * x + mat[1][1] * vec[1];
}

void mul_m2_v2(const float mat[2][2], float vec[2])
{
  mul_v2_m2v2(vec, mat, vec);
}

void mul_mat3_m4_v3(const float mat[4][4], float r[3])
{
  const float x = r[0];
  const float y = r[1];

  r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * r[2];
  r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * r[2];
  r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * r[2];
}

void mul_v3_mat3_m4v3(float r[3], const float mat[4][4], const float vec[3])
{
  const float x = vec[0];
  const float y = vec[1];

  r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2];
  r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2];
  r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2];
}

void mul_v3_mat3_m4v3_db(double r[3], const double mat[4][4], const double vec[3])
{
  const double x = vec[0];
  const double y = vec[1];

  r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2];
  r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2];
  r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2];
}

void mul_project_m4_v3(const float mat[4][4], float vec[3])
{
  /* absolute value to not flip the frustum upside down behind the camera */
  const float w = fabsf(mul_project_m4_v3_zfac(mat, vec));
  mul_m4_v3(mat, vec);

  vec[0] /= w;
  vec[1] /= w;
  vec[2] /= w;
}

void mul_v3_project_m4_v3(float r[3], const float mat[4][4], const float vec[3])
{
  const float w = fabsf(mul_project_m4_v3_zfac(mat, vec));
  mul_v3_m4v3(r, mat, vec);

  r[0] /= w;
  r[1] /= w;
  r[2] /= w;
}

void mul_v2_project_m4_v3(float r[2], const float mat[4][4], const float vec[3])
{
  const float w = fabsf(mul_project_m4_v3_zfac(mat, vec));
  mul_v2_m4v3(r, mat, vec);

  r[0] /= w;
  r[1] /= w;
}

void mul_v4_m4v4(float r[4], const float mat[4][4], const float v[4])
{
  const float x = v[0];
  const float y = v[1];
  const float z = v[2];

  r[0] = x * mat[0][0] + y * mat[1][0] + z * mat[2][0] + mat[3][0] * v[3];
  r[1] = x * mat[0][1] + y * mat[1][1] + z * mat[2][1] + mat[3][1] * v[3];
  r[2] = x * mat[0][2] + y * mat[1][2] + z * mat[2][2] + mat[3][2] * v[3];
  r[3] = x * mat[0][3] + y * mat[1][3] + z * mat[2][3] + mat[3][3] * v[3];
}

void mul_m4_v4(const float mat[4][4], float r[4])
{
  mul_v4_m4v4(r, mat, r);
}

void mul_v4_m4v3(float r[4], const float M[4][4], const float v[3])
{
  /* v has implicit w = 1.0f */
  r[0] = v[0] * M[0][0] + v[1] * M[1][0] + M[2][0] * v[2] + M[3][0];
  r[1] = v[0] * M[0][1] + v[1] * M[1][1] + M[2][1] * v[2] + M[3][1];
  r[2] = v[0] * M[0][2] + v[1] * M[1][2] + M[2][2] * v[2] + M[3][2];
  r[3] = v[0] * M[0][3] + v[1] * M[1][3] + M[2][3] * v[2] + M[3][3];
}

void mul_v3_m3v3(float r[3], const float M[3][3], const float a[3])
{
  float t[3];
  copy_v3_v3(t, a);

  r[0] = M[0][0] * t[0] + M[1][0] * t[1] + M[2][0] * t[2];
  r[1] = M[0][1] * t[0] + M[1][1] * t[1] + M[2][1] * t[2];
  r[2] = M[0][2] * t[0] + M[1][2] * t[1] + M[2][2] * t[2];
}

void mul_v3_m3v3_db(double r[3], const double M[3][3], const double a[3])
{
  double t[3];
  copy_v3_v3_db(t, a);

  r[0] = M[0][0] * t[0] + M[1][0] * t[1] + M[2][0] * t[2];
  r[1] = M[0][1] * t[0] + M[1][1] * t[1] + M[2][1] * t[2];
  r[2] = M[0][2] * t[0] + M[1][2] * t[1] + M[2][2] * t[2];
}

void mul_v2_m3v3(float r[2], const float M[3][3], const float a[3])
{
  float t[3];
  copy_v3_v3(t, a);

  r[0] = M[0][0] * t[0] + M[1][0] * t[1] + M[2][0] * t[2];
  r[1] = M[0][1] * t[0] + M[1][1] * t[1] + M[2][1] * t[2];
}

void mul_m3_v3(const float M[3][3], float r[3])
{
  mul_v3_m3v3(r, M, r);
}

void mul_m3_v3_db(const double M[3][3], double r[3])
{
  mul_v3_m3v3_db(r, M, r);
}

void mul_transposed_m3_v3(const float M[3][3], float r[3])
{
  const float x = r[0];
  const float y = r[1];

  r[0] = x * M[0][0] + y * M[0][1] + M[0][2] * r[2];
  r[1] = x * M[1][0] + y * M[1][1] + M[1][2] * r[2];
  r[2] = x * M[2][0] + y * M[2][1] + M[2][2] * r[2];
}

void mul_transposed_mat3_m4_v3(const float M[4][4], float r[3])
{
  const float x = r[0];
  const float y = r[1];

  r[0] = x * M[0][0] + y * M[0][1] + M[0][2] * r[2];
  r[1] = x * M[1][0] + y * M[1][1] + M[1][2] * r[2];
  r[2] = x * M[2][0] + y * M[2][1] + M[2][2] * r[2];
}

void mul_m3_fl(float R[3][3], float f)
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      R[i][j] *= f;
    }
  }
}

void mul_m4_fl(float R[4][4], float f)
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      R[i][j] *= f;
    }
  }
}

void mul_mat3_m4_fl(float R[4][4], float f)
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      R[i][j] *= f;
    }
  }
}

void negate_m3(float R[3][3])
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      R[i][j] *= -1.0f;
    }
  }
}

void negate_mat3_m4(float R[4][4])
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      R[i][j] *= -1.0f;
    }
  }
}

void negate_m4(float R[4][4])
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      R[i][j] *= -1.0f;
    }
  }
}

void add_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3])
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      R[i][j] = A[i][j] + B[i][j];
    }
  }
}

void add_m4_m4m4(float R[4][4], const float A[4][4], const float B[4][4])
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      R[i][j] = A[i][j] + B[i][j];
    }
  }
}

void madd_m3_m3m3fl(float R[3][3], const float A[3][3], const float B[3][3], const float f)
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      R[i][j] = A[i][j] + B[i][j] * f;
    }
  }
}

void madd_m4_m4m4fl(float R[4][4], const float A[4][4], const float B[4][4], const float f)
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      R[i][j] = A[i][j] + B[i][j] * f;
    }
  }
}

void sub_m3_m3m3(float R[3][3], const float A[3][3], const float B[3][3])
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      R[i][j] = A[i][j] - B[i][j];
    }
  }
}

float determinant_m3_array(const float m[3][3])
{
  return (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
          m[1][0] * (m[0][1] * m[2][2] - m[0][2] * m[2][1]) +
          m[2][0] * (m[0][1] * m[1][2] - m[0][2] * m[1][1]));
}

float determinant_m4_mat3_array(const float m[4][4])
{
  return (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
          m[1][0] * (m[0][1] * m[2][2] - m[0][2] * m[2][1]) +
          m[2][0] * (m[0][1] * m[1][2] - m[0][2] * m[1][1]));
}

bool invert_m2_m2(float inverse[2][2], const float mat[2][2])
{
  const float det = determinant_m2(mat[0][0], mat[1][0], mat[0][1], mat[1][1]);
  adjoint_m2_m2(inverse, mat);

  bool success = (det != 0.0f);
  if (success) {
    inverse[0][0] /= det;
    inverse[1][0] /= det;
    inverse[0][1] /= det;
    inverse[1][1] /= det;
  }

  return success;
}

bool invert_m3(float mat[3][3])
{
  float mat_tmp[3][3];
  const bool success = invert_m3_m3(mat_tmp, mat);

  copy_m3_m3(mat, mat_tmp);
  return success;
}

bool invert_m3_m3(float inverse[3][3], const float mat[3][3])
{
  float det;
  int a, b;
  bool success;

  /* calc adjoint */
  adjoint_m3_m3(inverse, mat);

  /* then determinant old matrix! */
  det = determinant_m3_array(mat);

  success = (det != 0.0f);

  if (LIKELY(det != 0.0f)) {
    det = 1.0f / det;
    for (a = 0; a < 3; a++) {
      for (b = 0; b < 3; b++) {
        inverse[a][b] *= det;
      }
    }
  }

  return success;
}

bool invert_m4(float mat[4][4])
{
  float mat_tmp[4][4];
  const bool success = invert_m4_m4(mat_tmp, mat);

  copy_m4_m4(mat, mat_tmp);
  return success;
}

bool invert_m4_m4_fallback(float inverse[4][4], const float mat[4][4])
{
#ifndef MATH_STANDALONE
  if (EIG_invert_m4_m4(inverse, mat)) {
    return true;
  }
#endif

  int i, j, k;
  double temp;
  float tempmat[4][4];
  float max;
  int maxj;

  BLI_assert(inverse != mat);

  /* Set inverse to identity */
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      inverse[i][j] = 0;
    }
  }
  for (i = 0; i < 4; i++) {
    inverse[i][i] = 1;
  }

  /* Copy original matrix so we don't mess it up */
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      tempmat[i][j] = mat[i][j];
    }
  }

  for (i = 0; i < 4; i++) {
    /* Look for row with max pivot */
    max = fabsf(tempmat[i][i]);
    maxj = i;
    for (j = i + 1; j < 4; j++) {
      if (fabsf(tempmat[j][i]) > max) {
        max = fabsf(tempmat[j][i]);
        maxj = j;
      }
    }
    /* Swap rows if necessary */
    if (maxj != i) {
      for (k = 0; k < 4; k++) {
        SWAP(float, tempmat[i][k], tempmat[maxj][k]);
        SWAP(float, inverse[i][k], inverse[maxj][k]);
      }
    }

    if (UNLIKELY(tempmat[i][i] == 0.0f)) {
      return false; /* No non-zero pivot */
    }
    temp = double(tempmat[i][i]);
    for (k = 0; k < 4; k++) {
      tempmat[i][k] = float(double(tempmat[i][k]) / temp);
      inverse[i][k] = float(double(inverse[i][k]) / temp);
    }
    for (j = 0; j < 4; j++) {
      if (j != i) {
        temp = tempmat[j][i];
        for (k = 0; k < 4; k++) {
          tempmat[j][k] -= float(double(tempmat[i][k]) * temp);
          inverse[j][k] -= float(double(inverse[i][k]) * temp);
        }
      }
    }
  }
  return true;
}

bool invert_m4_m4(float inverse[4][4], const float mat[4][4])
{
#ifndef MATH_STANDALONE
  /* Use optimized matrix inverse from Eigen, since performance
   * impact of this function is significant in complex rigs. */
  return EIG_invert_m4_m4(inverse, mat);
#else
  return invert_m4_m4_fallback(inverse, mat);
#endif
}

void mul_m4_m4m4_aligned_scale(float R[4][4], const float A[4][4], const float B[4][4])
{
  float loc_a[3], rot_a[3][3], size_a[3];
  float loc_b[3], rot_b[3][3], size_b[3];
  float loc_r[3], rot_r[3][3], size_r[3];

  mat4_to_loc_rot_size(loc_a, rot_a, size_a, A);
  mat4_to_loc_rot_size(loc_b, rot_b, size_b, B);

  mul_v3_m4v3(loc_r, A, loc_b);
  mul_m3_m3m3(rot_r, rot_a, rot_b);
  mul_v3_v3v3(size_r, size_a, size_b);

  loc_rot_size_to_mat4(R, loc_r, rot_r, size_r);
}

void mul_m4_m4m4_split_channels(float R[4][4], const float A[4][4], const float B[4][4])
{
  float loc_a[3], rot_a[3][3], size_a[3];
  float loc_b[3], rot_b[3][3], size_b[3];
  float loc_r[3], rot_r[3][3], size_r[3];

  mat4_to_loc_rot_size(loc_a, rot_a, size_a, A);
  mat4_to_loc_rot_size(loc_b, rot_b, size_b, B);

  add_v3_v3v3(loc_r, loc_a, loc_b);
  mul_m3_m3m3(rot_r, rot_a, rot_b);
  mul_v3_v3v3(size_r, size_a, size_b);

  loc_rot_size_to_mat4(R, loc_r, rot_r, size_r);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Linear Algebra
 * \{ */

void transpose_m3(float R[3][3])
{
  float t;

  t = R[0][1];
  R[0][1] = R[1][0];
  R[1][0] = t;
  t = R[0][2];
  R[0][2] = R[2][0];
  R[2][0] = t;
  t = R[1][2];
  R[1][2] = R[2][1];
  R[2][1] = t;
}

void transpose_m3_m3(float R[3][3], const float M[3][3])
{
  BLI_assert(R != M);

  R[0][0] = M[0][0];
  R[0][1] = M[1][0];
  R[0][2] = M[2][0];
  R[1][0] = M[0][1];
  R[1][1] = M[1][1];
  R[1][2] = M[2][1];
  R[2][0] = M[0][2];
  R[2][1] = M[1][2];
  R[2][2] = M[2][2];
}

void transpose_m3_m4(float R[3][3], const float M[4][4])
{
  BLI_assert(&R[0][0] != &M[0][0]);

  R[0][0] = M[0][0];
  R[0][1] = M[1][0];
  R[0][2] = M[2][0];
  R[1][0] = M[0][1];
  R[1][1] = M[1][1];
  R[1][2] = M[2][1];
  R[2][0] = M[0][2];
  R[2][1] = M[1][2];
  R[2][2] = M[2][2];
}

void transpose_m4(float R[4][4])
{
  float t;

  t = R[0][1];
  R[0][1] = R[1][0];
  R[1][0] = t;
  t = R[0][2];
  R[0][2] = R[2][0];
  R[2][0] = t;
  t = R[0][3];
  R[0][3] = R[3][0];
  R[3][0] = t;

  t = R[1][2];
  R[1][2] = R[2][1];
  R[2][1] = t;
  t = R[1][3];
  R[1][3] = R[3][1];
  R[3][1] = t;

  t = R[2][3];
  R[2][3] = R[3][2];
  R[3][2] = t;
}

void transpose_m4_m4(float R[4][4], const float M[4][4])
{
  BLI_assert(R != M);

  R[0][0] = M[0][0];
  R[0][1] = M[1][0];
  R[0][2] = M[2][0];
  R[0][3] = M[3][0];
  R[1][0] = M[0][1];
  R[1][1] = M[1][1];
  R[1][2] = M[2][1];
  R[1][3] = M[3][1];
  R[2][0] = M[0][2];
  R[2][1] = M[1][2];
  R[2][2] = M[2][2];
  R[2][3] = M[3][2];
  R[3][0] = M[0][3];
  R[3][1] = M[1][3];
  R[3][2] = M[2][3];
  R[3][3] = M[3][3];
}

bool compare_m4m4(const float mat1[4][4], const float mat2[4][4], float limit)
{
  if (compare_v4v4(mat1[0], mat2[0], limit)) {
    if (compare_v4v4(mat1[1], mat2[1], limit)) {
      if (compare_v4v4(mat1[2], mat2[2], limit)) {
        if (compare_v4v4(mat1[3], mat2[3], limit)) {
          return true;
        }
      }
    }
  }
  return false;
}

void orthogonalize_m3(float R[3][3], int axis)
{
  float size[3];
  mat3_to_size(size, R);
  normalize_v3(R[axis]);
  switch (axis) {
    case 0:
      if (dot_v3v3(R[0], R[1]) < 1) {
        cross_v3_v3v3(R[2], R[0], R[1]);
        normalize_v3(R[2]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      else if (dot_v3v3(R[0], R[2]) < 1) {
        cross_v3_v3v3(R[1], R[2], R[0]);
        normalize_v3(R[1]);
        cross_v3_v3v3(R[2], R[0], R[1]);
      }
      else {
        float vec[3];

        vec[0] = R[0][1];
        vec[1] = R[0][2];
        vec[2] = R[0][0];

        cross_v3_v3v3(R[2], R[0], vec);
        normalize_v3(R[2]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      break;
    case 1:
      if (dot_v3v3(R[1], R[0]) < 1) {
        cross_v3_v3v3(R[2], R[0], R[1]);
        normalize_v3(R[2]);
        cross_v3_v3v3(R[0], R[1], R[2]);
      }
      else if (dot_v3v3(R[0], R[2]) < 1) {
        cross_v3_v3v3(R[0], R[1], R[2]);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[2], R[0], R[1]);
      }
      else {
        float vec[3];

        vec[0] = R[1][1];
        vec[1] = R[1][2];
        vec[2] = R[1][0];

        cross_v3_v3v3(R[0], R[1], vec);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[2], R[0], R[1]);
      }
      break;
    case 2:
      if (dot_v3v3(R[2], R[0]) < 1) {
        cross_v3_v3v3(R[1], R[2], R[0]);
        normalize_v3(R[1]);
        cross_v3_v3v3(R[0], R[1], R[2]);
      }
      else if (dot_v3v3(R[2], R[1]) < 1) {
        cross_v3_v3v3(R[0], R[1], R[2]);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      else {
        float vec[3];

        vec[0] = R[2][1];
        vec[1] = R[2][2];
        vec[2] = R[2][0];

        cross_v3_v3v3(R[0], vec, R[2]);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
  mul_v3_fl(R[0], size[0]);
  mul_v3_fl(R[1], size[1]);
  mul_v3_fl(R[2], size[2]);
}

void orthogonalize_m4(float R[4][4], int axis)
{
  float size[3];
  mat4_to_size(size, R);
  normalize_v3(R[axis]);
  switch (axis) {
    case 0:
      if (dot_v3v3(R[0], R[1]) < 1) {
        cross_v3_v3v3(R[2], R[0], R[1]);
        normalize_v3(R[2]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      else if (dot_v3v3(R[0], R[2]) < 1) {
        cross_v3_v3v3(R[1], R[2], R[0]);
        normalize_v3(R[1]);
        cross_v3_v3v3(R[2], R[0], R[1]);
      }
      else {
        float vec[3];

        vec[0] = R[0][1];
        vec[1] = R[0][2];
        vec[2] = R[0][0];

        cross_v3_v3v3(R[2], R[0], vec);
        normalize_v3(R[2]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      break;
    case 1:
      if (dot_v3v3(R[1], R[0]) < 1) {
        cross_v3_v3v3(R[2], R[0], R[1]);
        normalize_v3(R[2]);
        cross_v3_v3v3(R[0], R[1], R[2]);
      }
      else if (dot_v3v3(R[0], R[2]) < 1) {
        cross_v3_v3v3(R[0], R[1], R[2]);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[2], R[0], R[1]);
      }
      else {
        float vec[3];

        vec[0] = R[1][1];
        vec[1] = R[1][2];
        vec[2] = R[1][0];

        cross_v3_v3v3(R[0], R[1], vec);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[2], R[0], R[1]);
      }
      break;
    case 2:
      if (dot_v3v3(R[2], R[0]) < 1) {
        cross_v3_v3v3(R[1], R[2], R[0]);
        normalize_v3(R[1]);
        cross_v3_v3v3(R[0], R[1], R[2]);
      }
      else if (dot_v3v3(R[2], R[1]) < 1) {
        cross_v3_v3v3(R[0], R[1], R[2]);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      else {
        float vec[3];

        vec[0] = R[2][1];
        vec[1] = R[2][2];
        vec[2] = R[2][0];

        cross_v3_v3v3(R[0], vec, R[2]);
        normalize_v3(R[0]);
        cross_v3_v3v3(R[1], R[2], R[0]);
      }
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
  mul_v3_fl(R[0], size[0]);
  mul_v3_fl(R[1], size[1]);
  mul_v3_fl(R[2], size[2]);
}

/** Make an orthonormal basis around v1 in a way that is stable and symmetric. */
static void orthogonalize_stable(float v1[3], float v2[3], float v3[3], bool normalize)
{
  /* Make secondary axis vectors orthogonal to the primary via
   * plane projection, which preserves the determinant. */
  float len_sq_v1 = len_squared_v3(v1);

  if (len_sq_v1 > 0.0f) {
    madd_v3_v3fl(v2, v1, -dot_v3v3(v2, v1) / len_sq_v1);
    madd_v3_v3fl(v3, v1, -dot_v3v3(v3, v1) / len_sq_v1);

    if (normalize) {
      mul_v3_fl(v1, 1.0f / sqrtf(len_sq_v1));
    }
  }

  /* Make secondary axis vectors orthogonal relative to each other. */
  float norm_v2[3], norm_v3[3], tmp[3];
  float length_v2 = normalize_v3_v3(norm_v2, v2);
  float length_v3 = normalize_v3_v3(norm_v3, v3);
  float cos_angle = dot_v3v3(norm_v2, norm_v3);
  float abs_cos_angle = fabsf(cos_angle);

  /* Apply correction if the shear angle is significant, and not degenerate. */
  if (abs_cos_angle > 1e-4f && abs_cos_angle < 1.0f - FLT_EPSILON) {
    /* Adjust v2 by half of the necessary angle correction.
     * Thus the angle change is the same for both axis directions. */
    float angle = acosf(cos_angle);
    float target_angle = angle + (float(M_PI_2) - angle) / 2;

    madd_v3_v3fl(norm_v2, norm_v3, -cos_angle);
    mul_v3_fl(norm_v2, sinf(target_angle) / len_v3(norm_v2));
    madd_v3_v3fl(norm_v2, norm_v3, cosf(target_angle));

    /* Make v3 orthogonal. */
    cross_v3_v3v3(tmp, norm_v2, norm_v3);
    cross_v3_v3v3(norm_v3, tmp, norm_v2);
    normalize_v3(norm_v3);

    /* Re-apply scale, preserving area and proportion. */
    if (!normalize) {
      float scale_fac = sqrtf(sinf(angle));
      mul_v3_v3fl(v2, norm_v2, length_v2 * scale_fac);
      mul_v3_v3fl(v3, norm_v3, length_v3 * scale_fac);
    }
  }

  if (normalize) {
    copy_v3_v3(v2, norm_v2);
    copy_v3_v3(v3, norm_v3);
  }
}

void orthogonalize_m4_stable(float R[4][4], int axis, bool normalize)
{
  switch (axis) {
    case 0:
      orthogonalize_stable(R[0], R[1], R[2], normalize);
      break;
    case 1:
      orthogonalize_stable(R[1], R[0], R[2], normalize);
      break;
    case 2:
      orthogonalize_stable(R[2], R[0], R[1], normalize);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Orthogonalize Matrix Zeroed Axes
 *
 * Set any zeroed axes to an orthogonal vector in relation to the other axes.
 *
 * Typically used so matrix inversion can be performed.
 *
 * \note If an object has a zero scaled axis, this function can be used to "clean" the matrix
 * to behave as if the scale on that axis was `unit_length`. So it can be inverted
 * or used in matrix multiply without creating degenerate matrices, see: #50103
 * \{ */

/**
 * \return true if any axis needed to be modified.
 */
static bool orthogonalize_m3_zero_axes_impl(float *mat[3], const float unit_length)
{
  enum { X = 1 << 0, Y = 1 << 1, Z = 1 << 2 };
  int flag = 0;
  for (int i = 0; i < 3; i++) {
    flag |= (len_squared_v3(mat[i]) == 0.0f) ? (1 << i) : 0;
  }

  /* Either all or none are zero, either way we can't properly resolve this
   * since we need to fill invalid axes from valid ones. */
  if (ELEM(flag, 0, X | Y | Z)) {
    return false;
  }

  switch (flag) {
    case X | Y: {
      ortho_v3_v3(mat[1], mat[2]);
      ATTR_FALLTHROUGH;
    }
    case X: {
      cross_v3_v3v3(mat[0], mat[1], mat[2]);
      break;
    }

    case Y | Z: {
      ortho_v3_v3(mat[2], mat[0]);
      ATTR_FALLTHROUGH;
    }
    case Y: {
      cross_v3_v3v3(mat[1], mat[0], mat[2]);
      break;
    }

    case Z | X: {
      ortho_v3_v3(mat[0], mat[1]);
      ATTR_FALLTHROUGH;
    }
    case Z: {
      cross_v3_v3v3(mat[2], mat[0], mat[1]);
      break;
    }
    default: {
      BLI_assert_unreachable();
    }
  }

  for (int i = 0; i < 3; i++) {
    if (flag & (1 << i)) {
      if (UNLIKELY(normalize_v3_length(mat[i], unit_length) == 0.0f)) {
        mat[i][i] = unit_length;
      }
    }
  }

  return true;
}

bool orthogonalize_m3_zero_axes(float m[3][3], const float unit_length)
{
  float *unpacked[3] = {m[0], m[1], m[2]};
  return orthogonalize_m3_zero_axes_impl(unpacked, unit_length);
}
bool orthogonalize_m4_zero_axes(float m[4][4], const float unit_length)
{
  float *unpacked[3] = {m[0], m[1], m[2]};
  return orthogonalize_m3_zero_axes_impl(unpacked, unit_length);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Linear Algebra
 * \{ */

bool is_orthogonal_m3(const float m[3][3])
{
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < i; j++) {
      if (fabsf(dot_v3v3(m[i], m[j])) > 1e-5f) {
        return false;
      }
    }
  }

  return true;
}

bool is_orthogonal_m4(const float m[4][4])
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < i; j++) {
      if (fabsf(dot_v4v4(m[i], m[j])) > 1e-5f) {
        return false;
      }
    }
  }

  return true;
}

bool is_orthonormal_m3(const float m[3][3])
{
  if (is_orthogonal_m3(m)) {
    int i;

    for (i = 0; i < 3; i++) {
      if (fabsf(dot_v3v3(m[i], m[i]) - 1) > 1e-5f) {
        return false;
      }
    }

    return true;
  }

  return false;
}

bool is_orthonormal_m4(const float m[4][4])
{
  if (is_orthogonal_m4(m)) {
    int i;

    for (i = 0; i < 4; i++) {
      if (fabsf(dot_v4v4(m[i], m[i]) - 1) > 1e-5f) {
        return false;
      }
    }

    return true;
  }

  return false;
}

bool is_identity_m4(const float m[4][4])
{
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      if (m[row][col] != (row == col ? 1.0f : 0.0f)) {
        return false;
      }
    }
  }

  return true;
}

bool is_uniform_scaled_m3(const float m[3][3])
{
  const float eps = 1e-7f;
  float t[3][3];
  float l1, l2, l3, l4, l5, l6;

  transpose_m3_m3(t, m);

  l1 = len_squared_v3(m[0]);
  l2 = len_squared_v3(m[1]);
  l3 = len_squared_v3(m[2]);

  l4 = len_squared_v3(t[0]);
  l5 = len_squared_v3(t[1]);
  l6 = len_squared_v3(t[2]);

  if (fabsf(l2 - l1) <= eps && fabsf(l3 - l1) <= eps && fabsf(l4 - l1) <= eps &&
      fabsf(l5 - l1) <= eps && fabsf(l6 - l1) <= eps)
  {
    return true;
  }

  return false;
}

bool is_uniform_scaled_m4(const float m[4][4])
{
  float t[3][3];
  copy_m3_m4(t, m);
  return is_uniform_scaled_m3(t);
}

void normalize_m2_m2(float R[2][2], const float M[2][2])
{
  int i;
  for (i = 0; i < 2; i++) {
    normalize_v2_v2(R[i], M[i]);
  }
}

void normalize_m3(float R[3][3])
{
  int i;
  for (i = 0; i < 3; i++) {
    normalize_v3(R[i]);
  }
}

void normalize_m3_m3(float R[3][3], const float M[3][3])
{
  int i;
  for (i = 0; i < 3; i++) {
    normalize_v3_v3(R[i], M[i]);
  }
}

void normalize_m4_ex(float R[4][4], float r_scale[3])
{
  int i;
  for (i = 0; i < 3; i++) {
    r_scale[i] = normalize_v3(R[i]);
    if (r_scale[i] != 0.0f) {
      R[i][3] /= r_scale[i];
    }
  }
}
void normalize_m4(float R[4][4])
{
  int i;
  for (i = 0; i < 3; i++) {
    float len = normalize_v3(R[i]);
    if (len != 0.0f) {
      R[i][3] /= len;
    }
  }
}

void normalize_m4_m4(float rmat[4][4], const float mat[4][4])
{
  int i;
  for (i = 0; i < 3; i++) {
    float len = normalize_v3_v3(rmat[i], mat[i]);
    rmat[i][3] = (len != 0.0f) ? (mat[i][3] / len) : mat[i][3];
  }
  copy_v4_v4(rmat[3], mat[3]);
}

void adjoint_m2_m2(float R[2][2], const float M[2][2])
{
  const float r00 = M[1][1];
  const float r01 = -M[0][1];
  const float r10 = -M[1][0];
  const float r11 = M[0][0];

  R[0][0] = r00;
  R[0][1] = r01;
  R[1][0] = r10;
  R[1][1] = r11;
}

void adjoint_m3_m3(float R[3][3], const float M[3][3])
{
  const float m00 = M[0][0], m01 = M[0][1], m02 = M[0][2];
  const float m10 = M[1][0], m11 = M[1][1], m12 = M[1][2];
  const float m20 = M[2][0], m21 = M[2][1], m22 = M[2][2];

  R[0][0] = m11 * m22 - m12 * m21;
  R[0][1] = -m01 * m22 + m02 * m21;
  R[0][2] = m01 * m12 - m02 * m11;

  R[1][0] = -m10 * m22 + m12 * m20;
  R[1][1] = m00 * m22 - m02 * m20;
  R[1][2] = -m00 * m12 + m02 * m10;

  R[2][0] = m10 * m21 - m11 * m20;
  R[2][1] = -m00 * m21 + m01 * m20;
  R[2][2] = m00 * m11 - m01 * m10;
}

void adjoint_m4_m4(float R[4][4], const float M[4][4]) /* out = ADJ(in) */
{
  float a1, a2, a3, a4, b1, b2, b3, b4;
  float c1, c2, c3, c4, d1, d2, d3, d4;

  a1 = M[0][0];
  b1 = M[0][1];
  c1 = M[0][2];
  d1 = M[0][3];

  a2 = M[1][0];
  b2 = M[1][1];
  c2 = M[1][2];
  d2 = M[1][3];

  a3 = M[2][0];
  b3 = M[2][1];
  c3 = M[2][2];
  d3 = M[2][3];

  a4 = M[3][0];
  b4 = M[3][1];
  c4 = M[3][2];
  d4 = M[3][3];

  R[0][0] = determinant_m3(b2, b3, b4, c2, c3, c4, d2, d3, d4);
  R[1][0] = -determinant_m3(a2, a3, a4, c2, c3, c4, d2, d3, d4);
  R[2][0] = determinant_m3(a2, a3, a4, b2, b3, b4, d2, d3, d4);
  R[3][0] = -determinant_m3(a2, a3, a4, b2, b3, b4, c2, c3, c4);

  R[0][1] = -determinant_m3(b1, b3, b4, c1, c3, c4, d1, d3, d4);
  R[1][1] = determinant_m3(a1, a3, a4, c1, c3, c4, d1, d3, d4);
  R[2][1] = -determinant_m3(a1, a3, a4, b1, b3, b4, d1, d3, d4);
  R[3][1] = determinant_m3(a1, a3, a4, b1, b3, b4, c1, c3, c4);

  R[0][2] = determinant_m3(b1, b2, b4, c1, c2, c4, d1, d2, d4);
  R[1][2] = -determinant_m3(a1, a2, a4, c1, c2, c4, d1, d2, d4);
  R[2][2] = determinant_m3(a1, a2, a4, b1, b2, b4, d1, d2, d4);
  R[3][2] = -determinant_m3(a1, a2, a4, b1, b2, b4, c1, c2, c4);

  R[0][3] = -determinant_m3(b1, b2, b3, c1, c2, c3, d1, d2, d3);
  R[1][3] = determinant_m3(a1, a2, a3, c1, c2, c3, d1, d2, d3);
  R[2][3] = -determinant_m3(a1, a2, a3, b1, b2, b3, d1, d2, d3);
  R[3][3] = determinant_m3(a1, a2, a3, b1, b2, b3, c1, c2, c3);
}

float determinant_m2(const float a, const float b, const float c, const float d)
{
  return a * d - b * c;
}

float determinant_m3(
    float a1, float a2, float a3, float b1, float b2, float b3, float c1, float c2, float c3)
{
  float ans;

  ans = (a1 * determinant_m2(b2, b3, c2, c3) - b1 * determinant_m2(a2, a3, c2, c3) +
         c1 * determinant_m2(a2, a3, b2, b3));

  return ans;
}

float determinant_m4(const float m[4][4])
{
  float ans;
  float a1, a2, a3, a4, b1, b2, b3, b4, c1, c2, c3, c4, d1, d2, d3, d4;

  a1 = m[0][0];
  b1 = m[0][1];
  c1 = m[0][2];
  d1 = m[0][3];

  a2 = m[1][0];
  b2 = m[1][1];
  c2 = m[1][2];
  d2 = m[1][3];

  a3 = m[2][0];
  b3 = m[2][1];
  c3 = m[2][2];
  d3 = m[2][3];

  a4 = m[3][0];
  b4 = m[3][1];
  c4 = m[3][2];
  d4 = m[3][3];

  ans = (a1 * determinant_m3(b2, b3, b4, c2, c3, c4, d2, d3, d4) -
         b1 * determinant_m3(a2, a3, a4, c2, c3, c4, d2, d3, d4) +
         c1 * determinant_m3(a2, a3, a4, b2, b3, b4, d2, d3, d4) -
         d1 * determinant_m3(a2, a3, a4, b2, b3, b4, c2, c3, c4));

  return ans;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transformations
 * \{ */

void size_to_mat3(float R[3][3], const float size[3])
{
  R[0][0] = size[0];
  R[0][1] = 0.0f;
  R[0][2] = 0.0f;
  R[1][1] = size[1];
  R[1][0] = 0.0f;
  R[1][2] = 0.0f;
  R[2][2] = size[2];
  R[2][1] = 0.0f;
  R[2][0] = 0.0f;
}

void size_to_mat4(float R[4][4], const float size[3])
{
  R[0][0] = size[0];
  R[0][1] = 0.0f;
  R[0][2] = 0.0f;
  R[0][3] = 0.0f;
  R[1][0] = 0.0f;
  R[1][1] = size[1];
  R[1][2] = 0.0f;
  R[1][3] = 0.0f;
  R[2][0] = 0.0f;
  R[2][1] = 0.0f;
  R[2][2] = size[2];
  R[2][3] = 0.0f;
  R[3][0] = 0.0f;
  R[3][1] = 0.0f;
  R[3][2] = 0.0f;
  R[3][3] = 1.0f;
}

void mat3_to_size(float size[3], const float M[3][3])
{
  size[0] = len_v3(M[0]);
  size[1] = len_v3(M[1]);
  size[2] = len_v3(M[2]);
}

void mat4_to_size(float size[3], const float M[4][4])
{
  size[0] = len_v3(M[0]);
  size[1] = len_v3(M[1]);
  size[2] = len_v3(M[2]);
}

float mat4_to_size_max_axis(const float M[4][4])
{
  return sqrtf(std::max({len_squared_v3(M[0]), len_squared_v3(M[1]), len_squared_v3(M[2])}));
}

void mat4_to_size_fix_shear(float size[3], const float M[4][4])
{
  mat4_to_size(size, M);

  float volume = size[0] * size[1] * size[2];

  if (volume != 0.0f) {
    mul_v3_fl(size, cbrtf(fabsf(mat4_to_volume_scale(M) / volume)));
  }
}

float mat4_to_volume_scale(const float mat[4][4])
{
  return determinant_m4_mat3_array(mat);
}


[1012 more lines in file. Use offset=2001 to continue.]