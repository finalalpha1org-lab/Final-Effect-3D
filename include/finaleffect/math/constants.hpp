// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors
//
// Coordinate System Convention (Final Effect 3D):
// - Handedness: Right-handed 3D coordinate system
// - World axes: +X = Right, +Y = Up, +Z = Toward viewer (-Z = Forward)
// - Matrix storage: Column-major (m[col * 4 + row]), column-vector multiplication (v' = M * v)
// - Angles: Radians across all math APIs

#pragma once

#include <cmath>
#include <cstdint>
#include <numbers>

namespace finaleffect::math {

inline constexpr float kPi = std::numbers::pi_v<float>;
inline constexpr float kTwoPi = 2.0f * kPi;
inline constexpr float kHalfPi = 0.5f * kPi;
inline constexpr float kDefaultEpsilon = 1.0e-5f;
inline constexpr float kStrictEpsilon = 1.0e-6f;

enum class ClipSpaceDepth : std::uint8_t {
    NegativeOneToOne = 0, // OpenGL 3.3 Core, WebGL2
    ZeroToOne = 1         // Future Vulkan, Metal, WebGPU
};

[[nodiscard]] constexpr float degToRad(float degrees) noexcept {
    return degrees * (kPi / 180.0f);
}

[[nodiscard]] constexpr float radToDeg(float radians) noexcept {
    return radians * (180.0f / kPi);
}

[[nodiscard]] inline bool isFinite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] inline bool approxEqual(float a, float b, float epsilon = kDefaultEpsilon) noexcept {
    return std::fabs(a - b) <= epsilon;
}

} // namespace finaleffect::math
