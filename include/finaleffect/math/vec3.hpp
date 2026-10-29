// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/constants.hpp"

#include <algorithm>
#include <cmath>

namespace finaleffect::math {

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    constexpr Vec3() noexcept = default;
    constexpr Vec3(float x_val, float y_val, float z_val) noexcept : x(x_val), y(y_val), z(z_val) {}

    [[nodiscard]] static constexpr Vec3 zero() noexcept { return {0.0f, 0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec3 one() noexcept { return {1.0f, 1.0f, 1.0f}; }
    [[nodiscard]] static constexpr Vec3 unitX() noexcept { return {1.0f, 0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec3 unitY() noexcept { return {0.0f, 1.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec3 unitZ() noexcept { return {0.0f, 0.0f, 1.0f}; }

    [[nodiscard]] constexpr Vec3 operator+(const Vec3& rhs) const noexcept {
        return {x + rhs.x, y + rhs.y, z + rhs.z};
    }
    [[nodiscard]] constexpr Vec3 operator-(const Vec3& rhs) const noexcept {
        return {x - rhs.x, y - rhs.y, z - rhs.z};
    }
    [[nodiscard]] constexpr Vec3 operator-() const noexcept {
        return {-x, -y, -z};
    }
    [[nodiscard]] constexpr Vec3 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar};
    }
    [[nodiscard]] constexpr Vec3 cwiseProduct(const Vec3& rhs) const noexcept {
        return {x * rhs.x, y * rhs.y, z * rhs.z};
    }
    [[nodiscard]] constexpr Vec3 operator/(float scalar) const noexcept {
        const float inv = 1.0f / scalar;
        return {x * inv, y * inv, z * inv};
    }

    constexpr Vec3& operator+=(const Vec3& rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    constexpr Vec3& operator-=(const Vec3& rhs) noexcept {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    constexpr Vec3& operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const Vec3& rhs) const noexcept = default;

    [[nodiscard]] constexpr float dot(const Vec3& rhs) const noexcept {
        return x * rhs.x + y * rhs.y + z * rhs.z;
    }

    // Right-handed cross product: unitX().cross(unitY()) == unitZ().
    [[nodiscard]] constexpr Vec3 cross(const Vec3& rhs) const noexcept {
        return {
            y * rhs.z - z * rhs.y,
            z * rhs.x - x * rhs.z,
            x * rhs.y - y * rhs.x,
        };
    }

    [[nodiscard]] constexpr float lengthSquared() const noexcept {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(lengthSquared());
    }

    [[nodiscard]] Vec3 normalized(float epsilon = kStrictEpsilon) const noexcept {
        const float len_sq = lengthSquared();
        if (len_sq <= epsilon * epsilon) {
            return zero();
        }
        const float inv_len = 1.0f / std::sqrt(len_sq);
        return {x * inv_len, y * inv_len, z * inv_len};
    }

    [[nodiscard]] bool isFinite() const noexcept {
        return math::isFinite(x) && math::isFinite(y) && math::isFinite(z);
    }

    [[nodiscard]] bool approxEqual(const Vec3& rhs, float epsilon = kDefaultEpsilon) const noexcept {
        return math::approxEqual(x, rhs.x, epsilon) &&
               math::approxEqual(y, rhs.y, epsilon) &&
               math::approxEqual(z, rhs.z, epsilon);
    }

    [[nodiscard]] static constexpr Vec3 lerp(const Vec3& a, const Vec3& b, float t) noexcept {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
        };
    }

    [[nodiscard]] static constexpr Vec3 min(const Vec3& a, const Vec3& b) noexcept {
        return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
    }

    [[nodiscard]] static constexpr Vec3 max(const Vec3& a, const Vec3& b) noexcept {
        return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
    }
};

[[nodiscard]] constexpr Vec3 operator*(float scalar, const Vec3& v) noexcept {
    return v * scalar;
}

} // namespace finaleffect::math
