// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/math/Mat4.hpp"

#include <cmath>
#include <cstddef>

namespace fe::math {
namespace {

bool isUsablePerspective(Scalar fovYRadians, Scalar aspect, Scalar nearZ, Scalar farZ) noexcept {
    return isFinite(fovYRadians) && isFinite(aspect) && isFinite(nearZ) && isFinite(farZ) &&
           fovYRadians > kScalarEpsilon && fovYRadians < kPi - kScalarEpsilon && aspect > kScalarEpsilon &&
           nearZ > kScalarEpsilon && farZ > nearZ;
}

Vec3 chooseUpFallback(const Vec3& forward) noexcept {
    // Pick an axis that is not parallel to forward to keep the basis well conditioned.
    const Vec3 candidate = std::fabs(forward.y) > 0.9F ? Vec3::unitZ() : Vec3::unitY();
    return candidate;
}

}  // namespace

Mat4 Mat4::identity() noexcept {
    return Mat4();
}

Mat4 Mat4::zero() noexcept {
    Mat4 result;
    for (std::size_t i = 0; i < kElementCount; ++i) {
        result.m[i] = 0.0F;
    }
    return result;
}

Mat4 Mat4::translation(const Vec3& offset) noexcept {
    Mat4 result;
    result.at(0, 3) = offset.x;
    result.at(1, 3) = offset.y;
    result.at(2, 3) = offset.z;
    return result;
}

Mat4 Mat4::scaling(const Vec3& scale) noexcept {
    Mat4 result;
    result.at(0, 0) = scale.x;
    result.at(1, 1) = scale.y;
    result.at(2, 2) = scale.z;
    return result;
}

Mat4 Mat4::scaling(Scalar uniformScale) noexcept {
    return scaling(Vec3(uniformScale, uniformScale, uniformScale));
}

Mat4 Mat4::rotation(const Quat& rotationValue) noexcept {
    const Quat q = rotationValue.normalized();
    const Scalar xx = q.x * q.x;
    const Scalar yy = q.y * q.y;
    const Scalar zz = q.z * q.z;
    const Scalar xy = q.x * q.y;
    const Scalar xz = q.x * q.z;
    const Scalar yz = q.y * q.z;
    const Scalar wx = q.w * q.x;
    const Scalar wy = q.w * q.y;
    const Scalar wz = q.w * q.z;

    Mat4 result;
    result.at(0, 0) = 1.0F - 2.0F * (yy + zz);
    result.at(0, 1) = 2.0F * (xy - wz);
    result.at(0, 2) = 2.0F * (xz + wy);
    result.at(1, 0) = 2.0F * (xy + wz);
    result.at(1, 1) = 1.0F - 2.0F * (xx + zz);
    result.at(1, 2) = 2.0F * (yz - wx);
    result.at(2, 0) = 2.0F * (xz - wy);
    result.at(2, 1) = 2.0F * (yz + wx);
    result.at(2, 2) = 1.0F - 2.0F * (xx + yy);
    return result;
}

Mat4 Mat4::rotationAxisAngle(const Vec3& axis, Scalar angleRadians) noexcept {
    return rotation(Quat::fromAxisAngle(axis, angleRadians));
}

Mat4 Mat4::perspective(Scalar fovYRadians, Scalar aspect, Scalar nearZ, Scalar farZ) noexcept {
    if (!isUsablePerspective(fovYRadians, aspect, nearZ, farZ)) {
        return Mat4::identity();
    }
    const Scalar focal = 1.0F / std::tan(fovYRadians * 0.5F);
    Mat4 result = Mat4::zero();
    result.at(0, 0) = focal / aspect;
    result.at(1, 1) = focal;
    result.at(2, 2) = (farZ + nearZ) / (nearZ - farZ);
    result.at(2, 3) = (2.0F * farZ * nearZ) / (nearZ - farZ);
    result.at(3, 2) = -1.0F;
    return result;
}

Mat4 Mat4::orthographic(Scalar left, Scalar right, Scalar bottom, Scalar top, Scalar nearZ, Scalar farZ) noexcept {
    const Scalar width = right - left;
    const Scalar height = top - bottom;
    const Scalar depth = farZ - nearZ;
    if (std::fabs(width) <= kScalarEpsilon || std::fabs(height) <= kScalarEpsilon) {
        return Mat4::identity();
    }
    Mat4 result;
    result.at(0, 0) = 2.0F / width;
    result.at(1, 1) = 2.0F / height;
    result.at(2, 2) = std::fabs(depth) <= kScalarEpsilon ? 1.0F : -2.0F / depth;
    result.at(0, 3) = -(right + left) / width;
    result.at(1, 3) = -(top + bottom) / height;
    result.at(2, 3) = std::fabs(depth) <= kScalarEpsilon ? 0.0F : -(farZ + nearZ) / depth;
    return result;
}

Mat4 Mat4::lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) noexcept {
    const Vec3 forward = (target - eye).normalized();
    if (forward.lengthSquared() <= kScalarEpsilon) {
        return Mat4::identity();
    }
    Vec3 upVector = up.normalized();
    if (upVector.lengthSquared() <= kScalarEpsilon || std::fabs(dot(upVector, forward)) > 0.999F) {
        upVector = chooseUpFallback(forward);
    }
    const Vec3 right = cross(forward, upVector).normalized();
    const Vec3 trueUp = cross(right, forward);

    Mat4 result;
    result.at(0, 0) = right.x;
    result.at(0, 1) = right.y;
    result.at(0, 2) = right.z;
    result.at(0, 3) = -dot(right, eye);
    result.at(1, 0) = trueUp.x;
    result.at(1, 1) = trueUp.y;
    result.at(1, 2) = trueUp.z;
    result.at(1, 3) = -dot(trueUp, eye);
    result.at(2, 0) = -forward.x;
    result.at(2, 1) = -forward.y;
    result.at(2, 2) = -forward.z;
    result.at(2, 3) = dot(forward, eye);
    result.at(3, 0) = 0.0F;
    result.at(3, 1) = 0.0F;
    result.at(3, 2) = 0.0F;
    result.at(3, 3) = 1.0F;
    return result;
}

Mat4 Mat4::operator*(const Mat4& rhs) const noexcept {
    Mat4 result = Mat4::zero();
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            Scalar sum = 0.0F;
            for (std::size_t k = 0; k < 4; ++k) {
                sum += at(row, k) * rhs.at(k, column);
            }
            result.at(row, column) = sum;
        }
    }
    return result;
}

Vec3 Mat4::transformPoint(const Vec3& point) const noexcept {
    return Vec3(at(0, 0) * point.x + at(0, 1) * point.y + at(0, 2) * point.z + at(0, 3),
                at(1, 0) * point.x + at(1, 1) * point.y + at(1, 2) * point.z + at(1, 3),
                at(2, 0) * point.x + at(2, 1) * point.y + at(2, 2) * point.z + at(2, 3));
}

Vec3 Mat4::transformVector(const Vec3& direction) const noexcept {
    return Vec3(at(0, 0) * direction.x + at(0, 1) * direction.y + at(0, 2) * direction.z,
                at(1, 0) * direction.x + at(1, 1) * direction.y + at(1, 2) * direction.z,
                at(2, 0) * direction.x + at(2, 1) * direction.y + at(2, 2) * direction.z);
}

Mat4 Mat4::transposed() const noexcept {
    Mat4 result;
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            result.at(row, column) = at(column, row);
        }
    }
    return result;
}

bool Mat4::tryInverse(Mat4& out) const noexcept {
    // Gauss-Jordan elimination with partial pivoting on the augmented matrix [A | I].
    Scalar work[4][8] = {};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            work[row][column] = at(row, column);
        }
        work[row][4 + row] = 1.0F;
    }

    for (std::size_t column = 0; column < 4; ++column) {
        std::size_t pivotRow = column;
        Scalar best = std::fabs(work[column][column]);
        for (std::size_t row = column + 1; row < 4; ++row) {
            const Scalar candidate = std::fabs(work[row][column]);
            if (candidate > best) {
                best = candidate;
                pivotRow = row;
            }
        }
        if (best <= 1.0e-12F) {
            return false;
        }
        if (pivotRow != column) {
            for (std::size_t k = 0; k < 8; ++k) {
                const Scalar swap = work[column][k];
                work[column][k] = work[pivotRow][k];
                work[pivotRow][k] = swap;
            }
        }
        const Scalar pivot = work[column][column];
        for (std::size_t k = 0; k < 8; ++k) {
            work[column][k] /= pivot;
        }
        for (std::size_t row = 0; row < 4; ++row) {
            if (row == column) {
                continue;
            }
            const Scalar factor = work[row][column];
            if (factor == 0.0F) {
                continue;
            }
            for (std::size_t k = 0; k < 8; ++k) {
                work[row][k] -= factor * work[column][k];
            }
        }
    }

    Mat4 result;
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            result.at(row, column) = work[row][4 + column];
        }
    }
    out = result;
    return true;
}

bool Mat4::isFinite() const noexcept {
    for (std::size_t i = 0; i < kElementCount; ++i) {
        if (!fe::math::isFinite(m[i])) {
            return false;
        }
    }
    return true;
}

bool Mat4::isApprox(const Mat4& rhs, Scalar tolerance) const noexcept {
    for (std::size_t i = 0; i < kElementCount; ++i) {
        if (!approxEqual(m[i], rhs.m[i], tolerance)) {
            return false;
        }
    }
    return true;
}

}  // namespace fe::math
