// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/constants.hpp"

#include <algorithm>
#include <cmath>

namespace finaleffect::math {

struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    constexpr Vec2() noexcept = default;
    constexpr Vec2(float x_val, float y_val) noexcept : x(x_val), y(y_val) {}

    [[nodiscard]] static constexpr Vec2 zero() noexcept { return {0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec2 one() noexcept { return {1.0f, 1.0f}; }
    [[nodiscard]] static constexpr Vec2 unitX() noexcept { return {1.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec2 unitY() noexcept { return {0.0f, 1.0f}; }

    [[nodiscard]] constexpr Vec2 operator+(const Vec2& rhs) const noexcept { return {x + rhs.x, y + rhs.y}; }
    [[nodiscard]] constexpr Vec2 operator-(const Vec2& rhs) const noexcept { return {x - rhs.x, y - rhs.y}; }
    [[nodiscard]] constexpr Vec2 operator-() const noexcept { return {-x, -y}; }
    [[nodiscard]] constexpr Vec2 operator*(float scalar) const noexcept { return {x * scalar, y * scalar}; }
    [[nodiscard]] constexpr Vec2 operator/(float scalar) const noexcept {
        const float inv = 1.0f / scalar;
        return {x * inv, y * inv};
    }

    constexpr Vec2& operator+=(const Vec2& rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    constexpr Vec2& operator-=(const Vec2& rhs) noexcept {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    constexpr Vec2& operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const Vec2& rhs) const noexcept = default;

    [[nodiscard]] constexpr float dot(const Vec2& rhs) const noexcept {
        return x * rhs.x + y * rhs.y;
    }

    [[nodiscard]] constexpr float lengthSquared() const noexcept {
        return x * x + y * y;
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(lengthSquared());
    }

    // Safe normalization: returns {0, 0} if length is below threshold rather than NaN.
    [[nodiscard]] Vec2 normalized(float epsilon = kStrictEpsilon) const noexcept {
        const float len_sq = lengthSquared();
        if (len_sq <= epsilon * epsilon) {
            return zero();
        }
        const float inv_len = 1.0f / std::sqrt(len_sq);
        return {x * inv_len, y * inv_len};
    }

    [[nodiscard]] bool isFinite() const noexcept {
        return math::isFinite(x) && math::isFinite(y);
    }

    [[nodiscard]] bool approxEqual(const Vec2& rhs, float epsilon = kDefaultEpsilon) const noexcept {
        return math::approxEqual(x, rhs.x, epsilon) && math::approxEqual(y, rhs.y, epsilon);
    }

    [[nodiscard]] static constexpr Vec2 lerp(const Vec2& a, const Vec2& b, float t) noexcept {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
    }
};

[[nodiscard]] constexpr Vec2 operator*(float scalar, const Vec2& v) noexcept {
    return v * scalar;
}

} // namespace finaleffect::math
