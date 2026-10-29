// Final Effect 3D - core scalar maths.
// SPDX-License-Identifier: GPL-3.0-only
//
// Coordinate system (documented once, used everywhere):
//   * Right handed.
//   * +Y is up, +X is right, -Z is forward (the direction the default camera looks at).
//   * Matrices are column-major and multiply column vectors: v' = M * v.
//   * Angles are radians unless a name says otherwise.
//   * Scalars are 32-bit floats (fe::math::Scalar), which matches GPU expectations.
#pragma once

#include <cmath>
#include <cstddef>

namespace fe::math {

using Scalar = float;

inline constexpr Scalar kPi = 3.14159265358979323846F;
inline constexpr Scalar kTwoPi = 2.0F * kPi;
inline constexpr Scalar kHalfPi = 0.5F * kPi;
inline constexpr Scalar kDegToRad = kPi / 180.0F;
inline constexpr Scalar kRadToDeg = 180.0F / kPi;

/// Rounding level of the scalar type: used to size tolerance defaults.
inline constexpr Scalar kScalarEpsilon = 1.1920929e-07F;
/// Default absolute tolerance for unit-test comparisons and approximate equality.
inline constexpr Scalar kDefaultTolerance = 1.0e-5F;

constexpr Scalar radians(Scalar degrees) noexcept { return degrees * kDegToRad; }
constexpr Scalar degrees(Scalar radiansValue) noexcept { return radiansValue * kRadToDeg; }

template <typename T>
constexpr T clamp(T value, T low, T high) noexcept {
    return value < low ? low : (value > high ? high : value);
}

template <typename T>
constexpr T lerp(T a, T b, Scalar t) noexcept {
    return static_cast<T>(a + (b - a) * t);
}

inline bool approxEqual(Scalar a, Scalar b, Scalar tolerance = kDefaultTolerance) noexcept {
    if (a == b) {
        return true;
    }
    const Scalar diff = std::fabs(a - b);
    if (diff <= tolerance) {
        return true;
    }
    const Scalar largest = std::fmax(std::fabs(a), std::fabs(b));
    return diff <= tolerance * largest;
}

inline bool approxZero(Scalar value, Scalar tolerance = kDefaultTolerance) noexcept {
    return std::fabs(value) <= tolerance;
}

inline bool isFinite(Scalar value) noexcept { return std::isfinite(value); }

}  // namespace fe::math
