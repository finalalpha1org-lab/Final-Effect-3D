// Final Effect 3D - 4x4 matrix.
// SPDX-License-Identifier: GPL-3.0-only
//
// Storage is column-major: element (row, column) lives at m[column * 4 + row].
// This matches the memory layout expected by OpenGL, Vulkan, Metal and WebGPU,
// so no transposition is needed when uploading to a GPU.
#pragma once

#include "finaleffect/math/Quat.hpp"
#include "finaleffect/math/Vec3.hpp"

namespace fe::math {

struct Mat4 {
    static constexpr std::size_t kElementCount = 16;

    Scalar m[kElementCount] = {1.0F, 0.0F, 0.0F, 0.0F,
                               0.0F, 1.0F, 0.0F, 0.0F,
                               0.0F, 0.0F, 1.0F, 0.0F,
                               0.0F, 0.0F, 0.0F, 1.0F};

    constexpr Mat4() noexcept = default;

    static Mat4 identity() noexcept;
    static Mat4 zero() noexcept;
    static Mat4 translation(const Vec3& offset) noexcept;
    static Mat4 scaling(const Vec3& scale) noexcept;
    static Mat4 scaling(Scalar uniformScale) noexcept;
    static Mat4 rotation(const Quat& rotation) noexcept;
    static Mat4 rotationAxisAngle(const Vec3& axis, Scalar angleRadians) noexcept;

    /// Right-handed perspective projection producing OpenGL-style clip space (z in [-1, 1]).
    /// Returns the identity when the arguments are degenerate.
    static Mat4 perspective(Scalar fovYRadians, Scalar aspect, Scalar nearZ, Scalar farZ) noexcept;

    /// Right-handed orthographic projection.
    static Mat4 orthographic(Scalar left, Scalar right, Scalar bottom, Scalar top, Scalar nearZ, Scalar farZ) noexcept;

    /// Right-handed view matrix looking from eye towards target (-Z forward).
    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) noexcept;

    Scalar& at(std::size_t row, std::size_t column) noexcept { return m[column * 4 + row]; }
    const Scalar& at(std::size_t row, std::size_t column) const noexcept { return m[column * 4 + row]; }

    Scalar* data() noexcept { return m; }
    const Scalar* data() const noexcept { return m; }

    Mat4 operator*(const Mat4& rhs) const noexcept;

    /// Affine point transform. No perspective divide is performed; use a
    /// projection helper if you need clip-space coordinates.
    Vec3 transformPoint(const Vec3& point) const noexcept;
    /// Direction transform (ignores translation).
    Vec3 transformVector(const Vec3& direction) const noexcept;

    Vec3 translation() const noexcept { return Vec3(at(0, 3), at(1, 3), at(2, 3)); }

    Mat4 transposed() const noexcept;

    /// General inverse. Returns false (and leaves out untouched) for singular matrices.
    bool tryInverse(Mat4& out) const noexcept;

    bool isFinite() const noexcept;
    bool isApprox(const Mat4& rhs, Scalar tolerance = kDefaultTolerance) const noexcept;
};

}  // namespace fe::math
