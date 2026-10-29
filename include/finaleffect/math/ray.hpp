// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/vec3.hpp"

namespace finaleffect::math {

struct Ray {
    Vec3 origin{0.0f, 0.0f, 0.0f};
    Vec3 direction{0.0f, 0.0f, -1.0f};

    constexpr Ray() noexcept = default;
    Ray(const Vec3& ray_origin, const Vec3& ray_direction) noexcept
        : origin(ray_origin), direction(ray_direction.normalized()) {}

    [[nodiscard]] constexpr Vec3 at(float t) const noexcept {
        return origin + direction * t;
    }

    [[nodiscard]] bool isValid() const noexcept {
        return origin.isFinite() &&
               direction.isFinite() &&
               direction.lengthSquared() > (kStrictEpsilon * kStrictEpsilon);
    }
};

} // namespace finaleffect::math
