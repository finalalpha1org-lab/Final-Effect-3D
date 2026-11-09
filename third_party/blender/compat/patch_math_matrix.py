#!/usr/bin/env python3
"""Derive the vendored math_matrix.cc from Blender's upstream file.

Upstream math_matrix.cc pulls in Eigen (for determinant/invert/SVD/polar decomposition)
and TBB threading + the profiling framework (for the bulk transform helpers). This script
removes exactly those parts and leaves the rest of Blender's file untouched, so the
vendored subset stays free of external dependencies. The removed functionality is provided
by blenlib_matrix_support.cc (determinant/invert/is_negative) and left out entirely where
the project does not use it.

Usage: python3 patch_math_matrix.py <upstream.cc> <output.cc>
"""
import pathlib
import sys

VENDOR_NOTE = "final effect vendor patch"

def main() -> int:
    src = pathlib.Path(sys.argv[1])
    dst = pathlib.Path(sys.argv[2])
    lines = src.read_text().split("\n")

    drop = ('#include "BLI_task.hh"', '#include "PRF_profile.hh"', '#include <Eigen/Core>',
            '#include <Eigen/Dense>', '#include <Eigen/Eigenvalues>')
    lines = [l for l in lines if l.strip() not in drop]

    def find(sub, start=0):
        for i in range(start, len(lines)):
            if sub in lines[i]:
                return i
        return -1

    def section(name):
        return find("/** \\name " + name)

    cuts = []
    for first, nxt in (("Determinant", "Adjoint"),
                       ("Inverse", "Polar Decomposition"),
                       ("Polar Decomposition", "Interpolate")):
        a, b = section(first), section(nxt)
        if a != -1 and b != -1 and b > a:
            cuts.append((a, b))
    for a, b in reversed(cuts):
        lines[a:b] = ["/* --- %s: sections needing Eigen removed; see the note at the top --- */" % VENDOR_NOTE, ""]

    def block_start(i):
        j = i
        while j > 0 and (lines[j - 1].strip() == "" or lines[j - 1].strip().startswith(("template", "/**", "*", "*/"))):
            j -= 1
        return j

    i_a = find("interpolate(const MatBase<T, 3, 3> &A")
    i_b = find("interpolate_fast(const MatBase<T, 3, 3> &a")
    if i_a != -1 and i_b != -1 and i_b > i_a:
        a, b = block_start(i_a), block_start(i_b)
        lines[a:b] = ["/* --- %s: matrix interpolate() removed (needs Eigen polar decomposition) --- */" % VENDOR_NOTE, ""]
    lines = [l for l in lines if not l.strip().startswith("template ") or " interpolate(" not in l]

    i0 = find("void transform_normals(const float3x3 &transform, MutableSpan<float3>")
    i1 = find("}  // namespace math")
    if i0 != -1 and i1 != -1:
        lines[i0:i1] = ["/* --- %s: bulk transform helpers removed (need TBB threading + the profiler) --- */" % VENDOR_NOTE,
                        "#if 0"] + lines[i0:i1] + ["#endif", ""]

    dst.write_text("\n".join(lines))
    print("wrote", dst, len(lines), "lines; remaining Eigen/threading/PRF uses:",
          sum(1 for l in lines if "Eigen::" in l or "threading::" in l or "PRF_" in l))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
