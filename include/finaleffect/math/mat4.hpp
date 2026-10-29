// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/constants.hpp"
#include "finaleffect/math/quat.hpp"
#include "finaleffect/math/vec3.hpp"
#include "finaleffect/math/vec4.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace finaleffect::math {

// Column-major 4x4 matrix: element at (row, col) is data[col * 4 + row].
struct Mat4 {
    std::array<float, 16> data{};

    [[nodiscard]] static constexpr Mat4 zero() noexcept {
        return Mat4{};
    }

    [[nodiscard]] static constexpr Mat4 identity() noexcept {
        Mat4 m{};
        m.data[0] = 1.0f;
        m.data[5] = 1.0f;
        m.data[10] = 1.0f;
        m.data[15] = 1.0f;
        return m;
    }

    [[nodiscard]] constexpr float& operator()(std::size_t row, std::size_t col) noexcept {
        return data[col * 4 + row];
    }

    [[nodiscard]] constexpr float operator()(std::size_t row, std::size_t col) const noexcept {
        return data[col * 4 + row];
    }

    [[nodiscard]] constexpr const float* raw() const noexcept { return data.data(); }
    [[nodiscard]] constexpr float* raw() noexcept { return data.data(); }

    [[nodiscard]] static Mat4 translation(const Vec3& t) noexcept;
    [[nodiscard]] static Mat4 scale(const Vec3& s) noexcept;
    [[nodiscard]] static Mat4 fromQuat(const Quat& q) noexcept;
    [[nodiscard]] static Mat4 trs(const Vec3& t, const Quat& r, const Vec3& s) noexcept;

    [[nodiscard]] static Mat4 lookAtRH(const Vec3& eye, const Vec3& target, const Vec3& up) noexcept;
    [[nodiscard]] static Mat4 perspectiveRH_NO(float fov_y_radians, float aspect, float z_near, float z_far) noexcept;
    [[nodiscard]] static Mat4 perspectiveRH_ZO(float fov_y_radians, float aspect, float z_near, float z_far) noexcept;
    [[nodiscard]] static Mat4 orthographicRH_NO(
        float left, float right, float bottom, float top, float z_near, float z_far) noexcept;
    [[nodiscard]] static Mat4 orthographicRH_ZO(
        float left, float right, float bottom, float top, float z_near, float z_far) noexcept;

    [[nodiscard]] Mat4 operator*(const Mat4& rhs) const noexcept;
    [[nodiscard]] Vec4 operator*(const Vec4& v) const noexcept;

    [[nodiscard]] Vec3 transformPoint(const Vec3& p) const noexcept;
    [[nodiscard]] Vec3 transformDirection(const Vec3& d) const noexcept;

    [[nodiscard]] Mat4 transposed() const noexcept;
    [[nodiscard]] float determinant() const noexcept;
    [[nodiscard]] std::optional<Mat4> inverted(float min_determinant = 1.0e-8f) const noexcept;

    [[nodiscard]] bool isFinite() const noexcept;
    [[nodiscard]] bool approxEqual(const Mat4& rhs, float epsilon = kDefaultEpsilon) const noexcept;
};

} // namespace finaleffect::math
