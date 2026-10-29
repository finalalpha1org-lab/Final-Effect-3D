// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/vec3.hpp"

namespace finaleffect::math {

struct Quat {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{1.0f};

    constexpr Quat() noexcept = default;
    constexpr Quat(float x_val, float y_val, float z_val, float w_val) noexcept
        : x(x_val), y(y_val), z(z_val), w(w_val) {}

    [[nodiscard]] static constexpr Quat identity() noexcept {
        return {0.0f, 0.0f, 0.0f, 1.0f};
    }

    // Creates a unit quaternion from an arbitrary axis and angle in radians.
    // Returns identity if the axis length is near zero.
    [[nodiscard]] static Quat fromAxisAngle(const Vec3& axis, float angle_radians) noexcept;

    // Intrinsic XYZ / extrinsic ZYX Euler angles (rx, ry, rz in radians): Rz * Ry * Rx.
    [[nodiscard]] static Quat fromEulerXYZ(const Vec3& euler_radians) noexcept;

    // Shortest-path spherical linear interpolation.
    [[nodiscard]] static Quat slerp(const Quat& a, const Quat& b, float t) noexcept;

    [[nodiscard]] constexpr bool operator==(const Quat& rhs) const noexcept = default;

    // Hamilton product: (q1 * q2).rotate(v) == q1.rotate(q2.rotate(v)).
    [[nodiscard]] constexpr Quat operator*(const Quat& rhs) const noexcept {
        return {
            w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
            w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
            w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
        };
    }

    [[nodiscard]] constexpr Quat conjugate() const noexcept {
        return {-x, -y, -z, w};
    }

    [[nodiscard]] constexpr float dot(const Quat& rhs) const noexcept {
        return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
    }

    [[nodiscard]] constexpr float lengthSquared() const noexcept {
        return x * x + y * y + z * z + w * w;
    }

    [[nodiscard]] float length() const noexcept;
    [[nodiscard]] Quat normalized(float epsilon = kStrictEpsilon) const noexcept;
    [[nodiscard]] Quat inverse(float epsilon = kStrictEpsilon) const noexcept;
    [[nodiscard]] Vec3 rotate(const Vec3& v) const noexcept;

    [[nodiscard]] bool isFinite() const noexcept {
        return math::isFinite(x) && math::isFinite(y) && math::isFinite(z) && math::isFinite(w);
    }

    [[nodiscard]] bool approxEqual(const Quat& rhs, float epsilon = kDefaultEpsilon) const noexcept {
        return math::approxEqual(x, rhs.x, epsilon) &&
               math::approxEqual(y, rhs.y, epsilon) &&
               math::approxEqual(z, rhs.z, epsilon) &&
               math::approxEqual(w, rhs.w, epsilon);
    }

    // Accounts for q and -q representing the same 3D orientation.
    [[nodiscard]] bool sameOrientation(const Quat& rhs, float epsilon = kDefaultEpsilon) const noexcept;
};

} // namespace finaleffect::math
