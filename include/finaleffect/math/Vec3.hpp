// Final Effect 3D - 3D vector.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "finaleffect/math/Math.hpp"

namespace fe::math {

struct Vec3 {
    Scalar x = 0.0F;
    Scalar y = 0.0F;
    Scalar z = 0.0F;

    constexpr Vec3() noexcept = default;
    constexpr Vec3(Scalar xValue, Scalar yValue, Scalar zValue) noexcept
        : x(xValue), y(yValue), z(zValue) {}

    static constexpr Vec3 zero() noexcept { return Vec3(0.0F, 0.0F, 0.0F); }
    static constexpr Vec3 one() noexcept { return Vec3(1.0F, 1.0F, 1.0F); }
    static constexpr Vec3 unitX() noexcept { return Vec3(1.0F, 0.0F, 0.0F); }
    static constexpr Vec3 unitY() noexcept { return Vec3(0.0F, 1.0F, 0.0F); }
    static constexpr Vec3 unitZ() noexcept { return Vec3(0.0F, 0.0F, 1.0F); }

    constexpr Scalar& operator[](std::size_t index) noexcept { return (&x)[index]; }
    constexpr const Scalar& operator[](std::size_t index) const noexcept { return (&x)[index]; }

    constexpr Vec3 operator-() const noexcept { return Vec3(-x, -y, -z); }
    constexpr Vec3 operator+(const Vec3& rhs) const noexcept { return Vec3(x + rhs.x, y + rhs.y, z + rhs.z); }
    constexpr Vec3 operator-(const Vec3& rhs) const noexcept { return Vec3(x - rhs.x, y - rhs.y, z - rhs.z); }
    constexpr Vec3 operator*(Scalar s) const noexcept { return Vec3(x * s, y * s, z * s); }
    constexpr Vec3 operator/(Scalar s) const noexcept { return Vec3(x / s, y / s, z / s); }
    constexpr Vec3 operator*(const Vec3& rhs) const noexcept { return Vec3(x * rhs.x, y * rhs.y, z * rhs.z); }

    constexpr Vec3& operator+=(const Vec3& rhs) noexcept { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
    constexpr Vec3& operator-=(const Vec3& rhs) noexcept { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
    constexpr Vec3& operator*=(Scalar s) noexcept { x *= s; y *= s; z *= s; return *this; }

    constexpr bool operator==(const Vec3& rhs) const noexcept { return x == rhs.x && y == rhs.y && z == rhs.z; }
    constexpr bool operator!=(const Vec3& rhs) const noexcept { return !(*this == rhs); }

    constexpr Scalar lengthSquared() const noexcept { return x * x + y * y + z * z; }
    Scalar length() const noexcept { return std::sqrt(lengthSquared()); }

    /// Returns the zero vector when this vector is too short to normalize.
    Vec3 normalized(Scalar tolerance = kScalarEpsilon) const noexcept {
        const Scalar len = length();
        if (len <= tolerance) {
            return Vec3::zero();
        }
        return *this / len;
    }

    bool isFinite() const noexcept { return fe::math::isFinite(x) && fe::math::isFinite(y) && fe::math::isFinite(z); }

    bool isApprox(const Vec3& rhs, Scalar tolerance = kDefaultTolerance) const noexcept {
        return approxEqual(x, rhs.x, tolerance) && approxEqual(y, rhs.y, tolerance) &&
               approxEqual(z, rhs.z, tolerance);
    }
};

constexpr Vec3 operator*(Scalar s, const Vec3& v) noexcept { return v * s; }

constexpr Scalar dot(const Vec3& a, const Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

constexpr Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

constexpr Vec3 lerp(const Vec3& a, const Vec3& b, Scalar t) noexcept {
    return Vec3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}

constexpr Vec3 min(const Vec3& a, const Vec3& b) noexcept {
    return Vec3(a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z);
}

constexpr Vec3 max(const Vec3& a, const Vec3& b) noexcept {
    return Vec3(a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z);
}

/// Reflects an incident vector around a (unit) normal.
constexpr Vec3 reflect(const Vec3& incident, const Vec3& normal) noexcept {
    return incident - normal * (2.0F * dot(incident, normal));
}

}  // namespace fe::math
