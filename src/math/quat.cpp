// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/math/quat.hpp"

#include <algorithm>
#include <cmath>

namespace finaleffect::math {

Quat Quat::fromAxisAngle(const Vec3& axis, float angle_radians) noexcept {
    const Vec3 unit_axis = axis.normalized();
    if (unit_axis.lengthSquared() <= kStrictEpsilon) {
        return identity();
    }
    const float half_angle = 0.5f * angle_radians;
    const float s = std::sin(half_angle);
    const float c = std::cos(half_angle);
    return Quat(unit_axis.x * s, unit_axis.y * s, unit_axis.z * s, c).normalized();
}

Quat Quat::fromEulerXYZ(const Vec3& euler_radians) noexcept {
    const Quat qx = fromAxisAngle(Vec3::unitX(), euler_radians.x);
    const Quat qy = fromAxisAngle(Vec3::unitY(), euler_radians.y);
    const Quat qz = fromAxisAngle(Vec3::unitZ(), euler_radians.z);
    return (qz * qy * qx).normalized();
}

float Quat::length() const noexcept {
    return std::sqrt(lengthSquared());
}

Quat Quat::normalized(float epsilon) const noexcept {
    const float len_sq = lengthSquared();
    if (len_sq <= epsilon * epsilon) {
        return identity();
    }
    const float inv_len = 1.0f / std::sqrt(len_sq);
    return {x * inv_len, y * inv_len, z * inv_len, w * inv_len};
}

Quat Quat::inverse(float epsilon) const noexcept {
    const float len_sq = lengthSquared();
    if (len_sq <= epsilon * epsilon) {
        return identity();
    }
    const float inv_len_sq = 1.0f / len_sq;
    return {-x * inv_len_sq, -y * inv_len_sq, -z * inv_len_sq, w * inv_len_sq};
}

Vec3 Quat::rotate(const Vec3& v) const noexcept {
    const Quat q = normalized();
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 uv = u.cross(v);
    const Vec3 uuv = u.cross(uv);
    return v + (uv * (2.0f * q.w)) + (uuv * 2.0f);
}

Quat Quat::slerp(const Quat& a, const Quat& b, float t) noexcept {
    const Quat qa = a.normalized();
    Quat qb = b.normalized();

    float cos_theta = qa.dot(qb);
    if (cos_theta < 0.0f) {
        qb = Quat{-qb.x, -qb.y, -qb.z, -qb.w};
        cos_theta = -cos_theta;
    }

    // Use normalized linear interpolation when orientations are nearly identical.
    if (cos_theta > 0.9995f) {
        const Quat blended{
            qa.x + (qb.x - qa.x) * t,
            qa.y + (qb.y - qa.y) * t,
            qa.z + (qb.z - qa.z) * t,
            qa.w + (qb.w - qa.w) * t,
        };
        return blended.normalized();
    }

    const float clamped_cos = std::clamp(cos_theta, -1.0f, 1.0f);
    const float theta = std::acos(clamped_cos);
    const float sin_theta = std::sin(theta);
    const float weight_a = std::sin((1.0f - t) * theta) / sin_theta;
    const float weight_b = std::sin(t * theta) / sin_theta;

    return Quat{
        qa.x * weight_a + qb.x * weight_b,
        qa.y * weight_a + qb.y * weight_b,
        qa.z * weight_a + qb.z * weight_b,
        qa.w * weight_a + qb.w * weight_b,
    }.normalized();
}

bool Quat::sameOrientation(const Quat& rhs, float epsilon) const noexcept {
    const Quat qa = normalized();
    const Quat qb = rhs.normalized();
    const Quat neg_qb{-qb.x, -qb.y, -qb.z, -qb.w};
    return qa.approxEqual(qb, epsilon) || qa.approxEqual(neg_qb, epsilon);
}

} // namespace finaleffect::math
