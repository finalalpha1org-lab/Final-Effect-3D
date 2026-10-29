// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/vec3.hpp"

namespace finaleffect::math {

struct Vec4 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{0.0f};

    constexpr Vec4() noexcept = default;
    constexpr Vec4(float x_val, float y_val, float z_val, float w_val) noexcept
        : x(x_val), y(y_val), z(z_val), w(w_val) {}
    constexpr Vec4(const Vec3& xyz_val, float w_val) noexcept
        : x(xyz_val.x), y(xyz_val.y), z(xyz_val.z), w(w_val) {}

    [[nodiscard]] constexpr Vec3 xyz() const noexcept { return {x, y, z}; }

    [[nodiscard]] constexpr Vec4 operator+(const Vec4& rhs) const noexcept {
        return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w};
    }
    [[nodiscard]] constexpr Vec4 operator-(const Vec4& rhs) const noexcept {
        return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w};
    }
    [[nodiscard]] constexpr Vec4 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar, w * scalar};
    }

    [[nodiscard]] constexpr bool operator==(const Vec4& rhs) const noexcept = default;

    [[nodiscard]] constexpr float dot(const Vec4& rhs) const noexcept {
        return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
    }

    [[nodiscard]] bool isFinite() const noexcept {
        return math::isFinite(x) && math::isFinite(y) && math::isFinite(z) && math::isFinite(w);
    }

    [[nodiscard]] bool approxEqual(const Vec4& rhs, float epsilon = kDefaultEpsilon) const noexcept {
        return math::approxEqual(x, rhs.x, epsilon) &&
               math::approxEqual(y, rhs.y, epsilon) &&
               math::approxEqual(z, rhs.z, epsilon) &&
               math::approxEqual(w, rhs.w, epsilon);
    }
};

} // namespace finaleffect::math
