// Final Effect 3D - unit quaternion rotation.
// SPDX-License-Identifier: GPL-3.0-only
//
// Convention: quaternions rotate column vectors from local to parent space.
//   v_parent = q * v_local * q^-1
// Composition follows matrix order: (a * b) applies b first, then a.
// Quaternion multiplication is not commutative; q and -q describe the same rotation.
#pragma once

#include "finaleffect/math/Vec3.hpp"

namespace fe::math {

struct Mat4;

struct Quat {
    Scalar x = 0.0F;
    Scalar y = 0.0F;
    Scalar z = 0.0F;
    Scalar w = 1.0F;

    constexpr Quat() noexcept = default;
    constexpr Quat(Scalar xValue, Scalar yValue, Scalar zValue, Scalar wValue) noexcept
        : x(xValue), y(yValue), z(zValue), w(wValue) {}

    static constexpr Quat identity() noexcept { return Quat(0.0F, 0.0F, 0.0F, 1.0F); }

    /// Axis must be non-zero; a degenerate axis returns the identity rotation.
    static Quat fromAxisAngle(const Vec3& axis, Scalar angleRadians) noexcept;

    /// yaw around +Y, pitch around +X, roll around +Z, composed as Ry * Rx * Rz.
    static Quat fromEulerAngles(Scalar yawY, Scalar pitchX, Scalar rollZ) noexcept;

    /// Linear interpolation of two rotations, renormalized. Prefer slerp for constant speed.
    static Quat lerp(const Quat& a, const Quat& b, Scalar t) noexcept;

    /// Shortest-path spherical interpolation.
    static Quat slerp(const Quat& a, const Quat& b, Scalar t) noexcept;

    Scalar lengthSquared() const noexcept { return x * x + y * y + z * z + w * w; }
    Scalar length() const noexcept { return std::sqrt(lengthSquared()); }

    /// Returns the identity rotation when the quaternion is too short to normalize.
    Quat normalized(Scalar tolerance = kScalarEpsilon) const noexcept;

    constexpr Quat conjugate() const noexcept { return Quat(-x, -y, -z, w); }

    /// Inverse rotation. Returns the identity for degenerate input.
    Quat inverse(Scalar tolerance = kScalarEpsilon) const noexcept;

    constexpr Scalar dot(const Quat& rhs) const noexcept { return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w; }

    constexpr Quat operator+(const Quat& rhs) const noexcept {
        return Quat(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
    }
    constexpr Quat operator*(Scalar s) const noexcept { return Quat(x * s, y * s, z * s, w * s); }

    /// Hamilton product: (a * b) rotates by b, then by a.
    constexpr Quat operator*(const Quat& rhs) const noexcept {
        return Quat(w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
                    w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
                    w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
                    w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z);
    }

    /// Rotates a vector by this quaternion.
    Vec3 rotate(const Vec3& value) const noexcept;
    Vec3 operator*(const Vec3& value) const noexcept { return rotate(value); }

    /// Writes the equivalent axis (unit, or +Y for an identity rotation) and angle.
    void toAxisAngle(Vec3& axis, Scalar& angleRadians) const noexcept;

    /// Rotation matrix in column-major form.
    Mat4 toMatrix() const noexcept;

    bool isFinite() const noexcept;

    /// Sign-insensitive comparison: q and -q are the same rotation.
    bool isApprox(const Quat& rhs, Scalar tolerance = kDefaultTolerance) const noexcept;
};

}  // namespace fe::math
