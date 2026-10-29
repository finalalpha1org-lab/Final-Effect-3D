// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/mat4.hpp"
#include "finaleffect/math/ray.hpp"
#include "finaleffect/math/vec3.hpp"

#include <limits>
#include <optional>

namespace finaleffect::math {

struct AABB {
    Vec3 min{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    Vec3 max{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()};

    [[nodiscard]] static constexpr AABB empty() noexcept {
        return AABB{};
    }

    [[nodiscard]] static constexpr AABB fromMinMax(const Vec3& min_pt, const Vec3& max_pt) noexcept {
        AABB box;
        box.min = Vec3::min(min_pt, max_pt);
        box.max = Vec3::max(min_pt, max_pt);
        return box;
    }

    [[nodiscard]] bool isValid() const noexcept {
        return min.isFinite() && max.isFinite() &&
               min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    void expand(const Vec3& point) noexcept {
        min = Vec3::min(min, point);
        max = Vec3::max(max, point);
    }

    void expand(const AABB& other) noexcept {
        if (!other.isValid()) {
            return;
        }
        min = Vec3::min(min, other.min);
        max = Vec3::max(max, other.max);
    }

    [[nodiscard]] Vec3 center() const noexcept {
        return (min + max) * 0.5f;
    }

    [[nodiscard]] Vec3 extents() const noexcept {
        return (max - min) * 0.5f;
    }

    [[nodiscard]] Vec3 size() const noexcept {
        return max - min;
    }

    [[nodiscard]] float boundingSphereRadius() const noexcept {
        return isValid() ? extents().length() : 0.0f;
    }

    // Transforms an AABB by an affine 4x4 matrix without allocating corner arrays.
    [[nodiscard]] AABB transformed(const Mat4& matrix) const noexcept;
};

// Slab ray-AABB intersection; returns entry distance t >= t_min (0 if ray starts inside box).
[[nodiscard]] std::optional<float> intersectRayAabb(
    const Ray& ray,
    const AABB& box,
    float t_min = 0.0f,
    float t_max = std::numeric_limits<float>::max()) noexcept;

// Moller-Trumbore ray-triangle intersection; returns hit distance t >= t_min if hit.
[[nodiscard]] std::optional<float> intersectRayTriangle(
    const Ray& ray,
    const Vec3& v0,
    const Vec3& v1,
    const Vec3& v2,
    bool backface_cull = false,
    float t_min = 1.0e-5f,
    float t_max = std::numeric_limits<float>::max()) noexcept;

} // namespace finaleffect::math
