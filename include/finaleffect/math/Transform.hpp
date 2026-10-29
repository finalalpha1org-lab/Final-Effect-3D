// Final Effect 3D - position/rotation/scale transform.
// SPDX-License-Identifier: GPL-3.0-only
//
// A Transform is a value type (TRS). Its matrix applies scale first, then
// rotation, then translation:
//   M = T(position) * R(rotation) * S(scale)
// World matrices are built by walking the scene hierarchy and multiplying
// parent matrices on the left: world = parentWorld * local.
#pragma once

#include "finaleffect/math/Mat4.hpp"

namespace fe::math {

struct Transform {
    Vec3 position = Vec3::zero();
    Quat rotation = Quat::identity();
    Vec3 scale = Vec3::one();

    static Transform identity() noexcept { return Transform(); }

    /// T * R * S.
    Mat4 matrix() const noexcept;

    /// Inverse of the affine transform, returned as a TRS value.
    ///
    /// The result is exact whenever the scale is uniform, and whenever the
    /// rotation is identity, because then (T R S)^-1 factorises back into T R S.
    /// With a rotation combined with a non-uniform scale the linear part of the
    /// inverse is S^-1 R^-1, which is not a rotation times a diagonal scale, so no
    /// TRS value can represent it: the translation and the axis directions are
    /// still exact, but the linear part is the closest TRS approximation. Use
    /// Mat4::tryInverse when an exact inverse is required.
    /// Scale components close to zero produce a degenerate inverse (documented,
    /// not silently clamped).
    Transform inverse() const noexcept;

    /// Transforms a point from local space into this transform's parent space.
    Vec3 transformPoint(const Vec3& point) const noexcept;
    /// Transforms a direction (scale and rotation, no translation).
    Vec3 transformVector(const Vec3& direction) const noexcept;

    bool isFinite() const noexcept;

    bool isApprox(const Transform& rhs, Scalar tolerance = kDefaultTolerance) const noexcept;

    /// Decomposes an affine matrix without shear. Reflection (negative
    /// determinant) is folded into the Z scale component.
    static Transform fromMatrix(const Mat4& matrix) noexcept;
};

}  // namespace fe::math
