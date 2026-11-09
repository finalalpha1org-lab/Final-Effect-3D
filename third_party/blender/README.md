# Vendored Blender blenlib math subset

This directory contains source files taken from **Blender** (https://www.blender.org,
project repository https://projects.blender.org/blender/blender), used here as a library by
Final Effect 3D. Final Effect 3D is GPL-3.0-or-later; the vendored files are
GPL-2.0-or-later, which is compatible with that (GPLv2-or-later code may be combined into
a GPLv3 work). Every vendored file keeps its original SPDX copyright and license header.

## Provenance

- Upstream project: Blender
- Upstream version: **v5.2.3** (git tag `v5.2.3`), file set fetched from the official mirror
  `https://github.com/blender/blender` at that tag, path prefix `source/blender/blenlib/`,
  plus `source/blender/makesdna/DNA_vec_types.h`.
- Build configuration: Blender's own `MATH_STANDALONE` switch plus the patch described below.

## What is vendored

`blenlib/` holds Blender's C++ and C math headers (vector, matrix, quaternion, Euler,
axis-angle, rotation, basis, interpolation, half float, SIMD helpers, and the small utility
headers they need), together with the geometry header used by the rotation code.

`blenlib/intern/` holds the implementation translation units that this project builds:
`math_base.cc`, `math_basis_types.cc`, `math_half.cc`, `math_interp.cc`, `math_matrix.cc`,
`math_matrix_c.cc`, `math_rotation.cc`, `math_rotation_c.cc`, `math_vector.cc`.

`compat/` holds three files that are **not** upstream Blender code. They exist so the
vendored subset can build stand-alone:

- `MEM_guardedalloc.h` - implements the guarded-allocator entry points used by
  `BLI_allocator.hh` over aligned C++ allocation. Upstream keeps those in
  `source/blender/memutils/`. Replace with the real module to get Blender's
  leak/overflow tracking.
- `blenlib_assert.cc` - implements the assert-reporting hooks declared by `BLI_assert.h`
  (report to stderr). Upstream's versions live with the backtrace machinery.
- `blenlib_matrix_support.cc` - implements `determinant()`, `invert()` and `is_negative()`,
  whose upstream implementations (Blender >= 3.6) are built on the Eigen library, using
  Gaussian elimination with partial pivoting instead. Same signatures, same semantics, no
  external dependency.
- `blenlib_geom_support.cc` - `normal_tri_v3()`, lifted verbatim from Blender's
  `math_geom.cc`, because the whole geometry translation unit drags in further modules that
  this project does not need yet.

## Patches applied to upstream files

1. `blenlib/BLI_string_ref.hh`: the `fmt` include and the trailing fmtlib formatter
   integration were removed. `fmt` is an external library Blender downloads separately; only
   the formatting hook needs it, the `StringRef` type itself does not.
2. `blenlib/intern/math_matrix.cc`: derived from upstream by
   `compat/patch_math_matrix.py`. The sections that need Eigen (`Determinant`, `Inverse`,
   `Polar Decomposition`), the matrix `interpolate()` overloads that depend on polar
   decomposition, the `BLI_task.hh`/`PRF_profile.hh` includes and the TBB-parallel bulk
   transform helpers were removed; everything else is untouched. Re-run the script against a
   fresh upstream copy to reproduce the file.
3. `blenlib/intern/math_matrix.cc.upstream` is not vendored: keep only the derived file.

## Not vendored (deliberately)

Eigen (used upstream for matrix determinant/invert/SVD), TBB (bulk transform helpers), the
profiling framework `PRF_profile.hh`, `fmt`, and the blenlib modules outside math that are
not needed here (`math_geom.cc` as a whole, `math_color*`, `math_statistics.cc`,
`math_filter.cc`, `math_solvers.cc`).

## Building

The target `finaleffect::blenlib` is created by `CMakeLists.txt` in this directory and is
linked into `finaleffect::math`. It is compiled with `-DMATH_STANDALONE` and only
`-Wall -Wextra`; the project-wide strict warnings are applied to first-party code only.

## Using it from engine code

Blender's math types live in `namespace blender` (`blender::float3`, `blender::float4x4`,
`blender::math::Quaternion`, ...). Convert at the boundary, for example:

```cpp
#include "BLI_math_matrix.hh"
#include "BLI_math_quaternion.hh"

blender::float4x4 rotation = blender::math::from_rotation<blender::float4x4>(quaternion);
blender::float3 moved = blender::math::transform_point(rotation, blender::float3{x, y, z});
```
