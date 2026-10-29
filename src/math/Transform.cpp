// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/math/Transform.hpp"

#include <cmath>

namespace fe::math {
namespace {

Quat quatFromOrthonormalBasis(const Vec3& xAxis, const Vec3& yAxis, const Vec3& zAxis) noexcept {
    const Scalar trace = xAxis.x + yAxis.y + zAxis.z;
    if (trace > 0.0F) {
        const Scalar s = std::sqrt(trace + 1.0F) * 2.0F;
        return Quat((yAxis.z - zAxis.y) / s, (zAxis.x - xAxis.z) / s, (xAxis.y - yAxis.x) / s, 0.25F * s)
            .normalized();
    }
    if (xAxis.x > yAxis.y && xAxis.x > zAxis.z) {
        const Scalar s = std::sqrt(1.0F + xAxis.x - yAxis.y - zAxis.z) * 2.0F;
        return Quat(0.25F * s, (xAxis.y + yAxis.x) / s, (zAxis.x + xAxis.z) / s, (yAxis.z - zAxis.y) / s)
            .normalized();
    }
    if (yAxis.y > zAxis.z) {
        const Scalar s = std::sqrt(1.0F + yAxis.y - xAxis.x - zAxis.z) * 2.0F;
        return Quat((xAxis.y + yAxis.x) / s, 0.25F * s, (yAxis.z + zAxis.y) / s, (zAxis.x - xAxis.z) / s)
            .normalized();
    }
    const Scalar s = std::sqrt(1.0F + zAxis.z - xAxis.x - yAxis.y) * 2.0F;
    return Quat((zAxis.x + xAxis.z) / s, (yAxis.z + zAxis.y) / s, 0.25F * s, (xAxis.y - yAxis.x) / s)
        .normalized();
}

Vec3 safeInverseScale(const Vec3& scale) noexcept {
    return Vec3(approxZero(scale.x) ? 0.0F : 1.0F / scale.x,
                approxZero(scale.y) ? 0.0F : 1.0F / scale.y,
                approxZero(scale.z) ? 0.0F : 1.0F / scale.z);
}

}  // namespace

Mat4 Transform::matrix() const noexcept {
    return Mat4::translation(position) * Mat4::rotation(rotation) * Mat4::scaling(scale);
}

Transform Transform::inverse() const noexcept {
    Transform result;
    result.rotation = rotation.inverse();
    result.scale = safeInverseScale(scale);
    const Vec3 rotated = result.rotation.rotate(position);
    result.position = Vec3(-rotated.x * result.scale.x, -rotated.y * result.scale.y, -rotated.z * result.scale.z);
    return result;
}

Vec3 Transform::transformPoint(const Vec3& point) const noexcept {
    const Vec3 scaled(point.x * scale.x, point.y * scale.y, point.z * scale.z);
    return position + rotation.rotate(scaled);
}

Vec3 Transform::transformVector(const Vec3& direction) const noexcept {
    const Vec3 scaled(direction.x * scale.x, direction.y * scale.y, direction.z * scale.z);
    return rotation.rotate(scaled);
}

bool Transform::isFinite() const noexcept {
    return position.isFinite() && rotation.isFinite() && scale.isFinite();
}

bool Transform::isApprox(const Transform& rhs, Scalar tolerance) const noexcept {
    return position.isApprox(rhs.position, tolerance) && scale.isApprox(rhs.scale, tolerance) &&
           rotation.isApprox(rhs.rotation, tolerance);
}

Transform Transform::fromMatrix(const Mat4& matrix) noexcept {
    Transform result;
    result.position = matrix.translation();

    Vec3 xAxis(matrix.at(0, 0), matrix.at(1, 0), matrix.at(2, 0));
    Vec3 yAxis(matrix.at(0, 1), matrix.at(1, 1), matrix.at(2, 1));
    Vec3 zAxis(matrix.at(0, 2), matrix.at(1, 2), matrix.at(2, 2));

    // Column lengths are the absolute scale factors; a mirrored basis (negative
    // determinant) is folded into the z scale, so the extracted rotation stays a
    // proper rotation.
    const Scalar scaleX = xAxis.length();
    const Scalar scaleY = yAxis.length();
    const Scalar scaleZ = zAxis.length();
    result.scale = Vec3(scaleX, scaleY, scaleZ);

    if (dot(cross(xAxis, yAxis), zAxis) < 0.0F) {
        zAxis = -zAxis;
        result.scale.z = -scaleZ;
    }

    if (approxZero(scaleX) || approxZero(scaleY) || approxZero(scaleZ)) {
        result.rotation = Quat::identity();
        return result;
    }

    // Normalise by the magnitudes: the signs belong to the scale, and the basis
    // built from the (possibly flipped) axes must be right-handed.
    const Vec3 normalizedX = xAxis / scaleX;
    const Vec3 normalizedY = yAxis / scaleY;
    const Vec3 normalizedZ = zAxis / scaleZ;
    result.rotation = quatFromOrthonormalBasis(normalizedX, normalizedY, normalizedZ);
    return result;
}

}  // namespace fe::math
