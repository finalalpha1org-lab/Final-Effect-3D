// Final Effect 3D - 2D vector (screen space, UV and texture coordinates).
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "finaleffect/math/Math.hpp"

namespace fe::math {

struct Vec2 {
    Scalar x = 0.0F;
    Scalar y = 0.0F;

    constexpr Vec2() noexcept = default;
    constexpr Vec2(Scalar xValue, Scalar yValue) noexcept : x(xValue), y(yValue) {}

    static constexpr Vec2 zero() noexcept { return Vec2(0.0F, 0.0F); }
    static constexpr Vec2 one() noexcept { return Vec2(1.0F, 1.0F); }

    constexpr Scalar& operator[](std::size_t index) noexcept { return (&x)[index]; }
    constexpr const Scalar& operator[](std::size_t index) const noexcept { return (&x)[index]; }

    constexpr Vec2 operator-() const noexcept { return Vec2(-x, -y); }
    constexpr Vec2 operator+(const Vec2& rhs) const noexcept { return Vec2(x + rhs.x, y + rhs.y); }
    constexpr Vec2 operator-(const Vec2& rhs) const noexcept { return Vec2(x - rhs.x, y - rhs.y); }
    constexpr Vec2 operator*(Scalar s) const noexcept { return Vec2(x * s, y * s); }
    constexpr Vec2 operator/(Scalar s) const noexcept { return Vec2(x / s, y / s); }

    constexpr bool operator==(const Vec2& rhs) const noexcept { return x == rhs.x && y == rhs.y; }
    constexpr bool operator!=(const Vec2& rhs) const noexcept { return !(*this == rhs); }

    constexpr Scalar lengthSquared() const noexcept { return x * x + y * y; }
    Scalar length() const noexcept { return std::sqrt(lengthSquared()); }

    Vec2 normalized(Scalar tolerance = kScalarEpsilon) const noexcept {
        const Scalar len = length();
        if (len <= tolerance) {
            return Vec2::zero();
        }
        return *this / len;
    }

    bool isFinite() const noexcept { return fe::math::isFinite(x) && fe::math::isFinite(y); }

    bool isApprox(const Vec2& rhs, Scalar tolerance = kDefaultTolerance) const noexcept {
        return approxEqual(x, rhs.x, tolerance) && approxEqual(y, rhs.y, tolerance);
    }
};

constexpr Vec2 operator*(Scalar s, const Vec2& v) noexcept { return v * s; }

constexpr Scalar dot(const Vec2& a, const Vec2& b) noexcept { return a.x * b.x + a.y * b.y; }

/// 2D analogue of the cross product: the z component of cross(a, 0) x (b, 0).
constexpr Scalar cross(const Vec2& a, const Vec2& b) noexcept { return a.x * b.y - a.y * b.x; }

constexpr Vec2 lerp(const Vec2& a, const Vec2& b, Scalar t) noexcept {
    return Vec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}

}  // namespace fe::math
