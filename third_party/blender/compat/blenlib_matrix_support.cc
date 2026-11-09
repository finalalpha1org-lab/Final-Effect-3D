/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Final Effect 3D Contributors (adapter code, not upstream Blender code) */

/*
 * Adapter for the blenlib matrix functions whose upstream implementation (Blender >= 3.6)
 * is built on the Eigen library: determinant(), invert() and is_negative().
 *
 * Everything else in the vendored blenlib subset is Blender's code verbatim. These three
 * functions use the same signatures, the same blender::MatBase types and the same semantics,
 * implemented with plain Gaussian elimination with partial pivoting so the vendored subset
 * stays free of external dependencies. If exact numerical parity with Blender's Eigen-backed
 * path is required, vendor Eigen and drop this file.
 */

#include "BLI_math_matrix.hh"

#include <algorithm>
#include <cmath>

namespace blender::math {

template<typename T, int Size> T determinant(const MatBase<T, Size, Size> &mat)
{
  MatBase<T, Size, Size> a = mat;
  T det = T(1);
  for (int col = 0; col < Size; col++) {
    int pivot_row = col;
    T best = std::abs(a[col][col]);
    for (int row = col + 1; row < Size; row++) {
      const T candidate = std::abs(a[col][row]);
      if (candidate > best) {
        best = candidate;
        pivot_row = row;
      }
    }
    if (best <= T(0)) {
      return T(0);
    }
    if (pivot_row != col) {
      for (int c = 0; c < Size; c++) {
        std::swap(a[c][col], a[c][pivot_row]);
      }
      det = -det;
    }
    det *= a[col][col];
    for (int row = col + 1; row < Size; row++) {
      const T factor = a[col][row] / a[col][col];
      if (factor == T(0)) {
        continue;
      }
      for (int c = col; c < Size; c++) {
        a[c][row] -= factor * a[c][col];
      }
    }
  }
  return det;
}

template<typename T, int Size>
MatBase<T, Size, Size> invert(const MatBase<T, Size, Size> &mat, bool &r_success)
{
  MatBase<T, Size, Size> a = mat;
  MatBase<T, Size, Size> result = MatBase<T, Size, Size>::identity();
  for (int col = 0; col < Size; col++) {
    int pivot_row = col;
    T best = std::abs(a[col][col]);
    for (int row = col + 1; row < Size; row++) {
      const T candidate = std::abs(a[col][row]);
      if (candidate > best) {
        best = candidate;
        pivot_row = row;
      }
    }
    if (best <= T(0)) {
      r_success = false;
      return MatBase<T, Size, Size>::identity();
    }
    if (pivot_row != col) {
      for (int c = 0; c < Size; c++) {
        std::swap(a[c][col], a[c][pivot_row]);
        std::swap(result[c][col], result[c][pivot_row]);
      }
    }
    const T diagonal = a[col][col];
    for (int c = 0; c < Size; c++) {
      a[c][col] /= diagonal;
      result[c][col] /= diagonal;
    }
    for (int row = 0; row < Size; row++) {
      if (row == col) {
        continue;
      }
      const T factor = a[col][row];
      if (factor == T(0)) {
        continue;
      }
      for (int c = 0; c < Size; c++) {
        a[c][row] -= factor * a[c][col];
        result[c][row] -= factor * result[c][col];
      }
    }
  }
  r_success = true;
  return result;
}

template<typename T> bool is_negative(const MatBase<T, 3, 3> &mat)
{
  return determinant(mat) < T(0);
}

template<typename T> bool is_negative(const MatBase<T, 4, 4> &mat)
{
  return determinant(mat) < T(0);
}

/* Explicit instantiations matching the declarations in BLI_math_matrix.hh. */
template float determinant(const MatBase<float, 1, 1> &mat);
template double determinant(const MatBase<double, 1, 1> &mat);
template MatBase<float, 1, 1> invert(const MatBase<float, 1, 1> &mat, bool &r_success);
template MatBase<double, 1, 1> invert(const MatBase<double, 1, 1> &mat, bool &r_success);


template float determinant(const float2x2 &mat);
template float determinant(const float3x3 &mat);
template float determinant(const float4x4 &mat);
template double determinant(const double2x2 &mat);
template double determinant(const double3x3 &mat);
template double determinant(const double4x4 &mat);

template float2x2 invert(const float2x2 &mat, bool &r_success);
template float3x3 invert(const float3x3 &mat, bool &r_success);
template float4x4 invert(const float4x4 &mat, bool &r_success);
template double2x2 invert(const double2x2 &mat, bool &r_success);
template double3x3 invert(const double3x3 &mat, bool &r_success);
template double4x4 invert(const double4x4 &mat, bool &r_success);

template bool is_negative(const float3x3 &mat);
template bool is_negative(const float4x4 &mat);
template bool is_negative(const double3x3 &mat);
template bool is_negative(const double4x4 &mat);

}  // namespace blender::math
