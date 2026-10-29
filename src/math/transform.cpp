// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/math/transform.hpp"

namespace finaleffect::math {

Mat4 Transform::toMatrix() const noexcept {
    return Mat4::trs(position, rotation, scale);
}

Vec3 Transform::right() const noexcept {
    return rotation.rotate(Vec3::unitX());
}

Vec3 Transform::up() const noexcept {
    return rotation.rotate(Vec3::unitY());
}

Vec3 Transform::forward() const noexcept {
    // Right-handed Y-up: local forward points along -Z.
    return rotation.rotate(Vec3{0.0f, 0.0f, -1.0f});
}

Vec3 Transform::transformPoint(const Vec3& local_point) const noexcept {
    const Vec3 scaled = local_point.cwiseProduct(scale);
    return position + rotation.rotate(scaled);
}

Vec3 Transform::transformDirection(const Vec3& local_direction) const noexcept {
    return rotation.rotate(local_direction.cwiseProduct(scale));
}

Transform Transform::combineRigidOrUniform(const Transform& parent, const Transform& local) noexcept {
    Transform result;
    result.scale = parent.scale.cwiseProduct(local.scale);
    result.rotation = (parent.rotation * local.rotation).normalized();
    result.position = parent.transformPoint(local.position);
    return result;
}

bool Transform::isValid() const noexcept {
    return position.isFinite() &&
           rotation.isFinite() &&
           scale.isFinite() &&
           rotation.lengthSquared() > (kStrictEpsilon * kStrictEpsilon);
}

bool Transform::approxEqual(const Transform& rhs, float epsilon) const noexcept {
    return position.approxEqual(rhs.position, epsilon) &&
           rotation.sameOrientation(rhs.rotation, epsilon) &&
           scale.approxEqual(rhs.scale, epsilon);
}

} // namespace finaleffect::math
