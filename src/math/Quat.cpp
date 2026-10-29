// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/math/Quat.hpp"

#include <cmath>

#include "finaleffect/math/Mat4.hpp"

namespace fe::math {

Quat Quat::fromAxisAngle(const Vec3& axis, Scalar angleRadians) noexcept {
    const Vec3 unit = axis.normalized();
    if (unit.lengthSquared() <= kScalarEpsilon) {
        return Quat::identity();
    }
    const Scalar half = angleRadians * 0.5F;
    const Scalar s = std::sin(half);
    return Quat(unit.x * s, unit.y * s, unit.z * s, std::cos(half));
}

Quat Quat::fromEulerAngles(Scalar yawY, Scalar pitchX, Scalar rollZ) noexcept {
    const Quat yaw = Quat::fromAxisAngle(Vec3::unitY(), yawY);
    const Quat pitch = Quat::fromAxisAngle(Vec3::unitX(), pitchX);
    const Quat roll = Quat::fromAxisAngle(Vec3::unitZ(), rollZ);
    return yaw * pitch * roll;
}

Quat Quat::normalized(Scalar tolerance) const noexcept {
    const Scalar len = length();
    if (len <= tolerance) {
        return Quat::identity();
    }
    return Quat(x / len, y / len, z / len, w / len);
}

Quat Quat::inverse(Scalar tolerance) const noexcept {
    const Scalar normSquared = lengthSquared();
    if (normSquared <= tolerance) {
        return Quat::identity();
    }
    const Quat conjugateValue = conjugate();
    return Quat(conjugateValue.x / normSquared,
                conjugateValue.y / normSquared,
                conjugateValue.z / normSquared,
                conjugateValue.w / normSquared);
}

Vec3 Quat::rotate(const Vec3& value) const noexcept {
    // v' = v + 2 * q_vec x (q_vec x v + w * v)
    const Vec3 q(x, y, z);
    const Vec3 t = cross(q, value) + value * w;
    return value + cross(q, t) * 2.0F;
}

void Quat::toAxisAngle(Vec3& axis, Scalar& angleRadians) const noexcept {
    const Quat unit = normalized();
    const Scalar clamped = clamp(unit.w, -1.0F, 1.0F);
    angleRadians = 2.0F * std::acos(clamped);
    const Scalar sinHalf = std::sqrt(std::fmax(0.0F, 1.0F - clamped * clamped));
    if (sinHalf <= kScalarEpsilon) {
        axis = Vec3::unitY();
        angleRadians = 0.0F;
        return;
    }
    axis = Vec3(unit.x / sinHalf, unit.y / sinHalf, unit.z / sinHalf);
}

Mat4 Quat::toMatrix() const noexcept {
    return Mat4::rotation(*this);
}

bool Quat::isFinite() const noexcept {
    return fe::math::isFinite(x) && fe::math::isFinite(y) && fe::math::isFinite(z) && fe::math::isFinite(w);
}

bool Quat::isApprox(const Quat& rhs, Scalar tolerance) const noexcept {
    const Scalar d = std::fabs(dot(rhs));
    return approxEqual(d, 1.0F, tolerance * 10.0F);
}

Quat Quat::lerp(const Quat& a, const Quat& b, Scalar t) noexcept {
    const Scalar d = a.dot(b);
    const Quat target = d < 0.0F ? Quat(-b.x, -b.y, -b.z, -b.w) : b;
    const Quat blended(a.x + (target.x - a.x) * t,
                       a.y + (target.y - a.y) * t,
                       a.z + (target.z - a.z) * t,
                       a.w + (target.w - a.w) * t);
    return blended.normalized();
}

Quat Quat::slerp(const Quat& a, const Quat& b, Scalar t) noexcept {
    const Quat from = a.normalized();
    Quat to = b.normalized();
    Scalar d = from.dot(to);
    if (d < 0.0F) {
        to = Quat(-to.x, -to.y, -to.z, -to.w);
        d = -d;
    }
    d = clamp(d, -1.0F, 1.0F);
    if (d > 0.9995F) {
        return lerp(from, to, t);
    }
    const Scalar theta = std::acos(d);
    const Scalar sinTheta = std::sin(theta);
    const Scalar wa = std::sin((1.0F - t) * theta) / sinTheta;
    const Scalar wb = std::sin(t * theta) / sinTheta;
    const Quat result(from.x * wa + to.x * wb,
                      from.y * wa + to.y * wb,
                      from.z * wa + to.z * wb,
                      from.w * wa + to.w * wb);
    return result.normalized();
}

}  // namespace fe::math
