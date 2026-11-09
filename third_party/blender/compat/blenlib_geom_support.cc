/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2025 Blender Authors (function lifted verbatim from blenlib/intern/math_geom.cc) */
/* Copyright (C) 2026 Final Effect 3D Contributors (glue file) */

/*
 * normal_tri_v3(): taken verbatim from Blender's math_geom.cc. The whole geometry
 * translation unit is not vendored (it drags in further blenlib modules); only this
 * function is needed by the rotation code in the vendored subset.
 */

#include "BLI_math_inline.h"
#include "BLI_math_geom.h"

#include "BLI_math_vector.h"

namespace blender {

float normal_tri_v3(float n[3], const float v1[3], const float v2[3], const float v3[3])
{
  float n1[3], n2[3];

  n1[0] = v1[0] - v2[0];
  n2[0] = v2[0] - v3[0];
  n1[1] = v1[1] - v2[1];
  n2[1] = v2[1] - v3[1];
  n1[2] = v1[2] - v2[2];
  n2[2] = v2[2] - v3[2];
  n[0] = n1[1] * n2[2] - n1[2] * n2[1];
  n[1] = n1[2] * n2[0] - n1[0] * n2[2];
  n[2] = n1[0] * n2[1] - n1[1] * n2[0];

  return normalize_v3(n);
}

}  // namespace blender
