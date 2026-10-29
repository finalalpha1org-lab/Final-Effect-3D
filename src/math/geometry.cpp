// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/math/aabb.hpp"

#include <algorithm>
#include <cmath>

namespace finaleffect::math {

AABB AABB::transformed(const Mat4& matrix) const noexcept {
    if (!isValid()) {
        return AABB::empty();
    }

    // Arvo's affine AABB transform using center + abs(linear) * extents.
    const Vec3 world_center = matrix.transformPoint(center());
    const Vec3 ext = extents();

    const Vec3 world_extents{
        std::fabs(matrix(0, 0)) * ext.x + std::fabs(matrix(0, 1)) * ext.y + std::fabs(matrix(0, 2)) * ext.z,
        std::fabs(matrix(1, 0)) * ext.x + std::fabs(matrix(1, 1)) * ext.y + std::fabs(matrix(1, 2)) * ext.z,
        std::fabs(matrix(2, 0)) * ext.x + std::fabs(matrix(2, 1)) * ext.y + std::fabs(matrix(2, 2)) * ext.z,
    };

    return AABB::fromMinMax(world_center - world_extents, world_center + world_extents);
}

std::optional<float> intersectRayAabb(
    const Ray& ray,
    const AABB& box,
    float t_min,
    float t_max) noexcept {
    if (!ray.isValid() || !box.isValid() || t_max < t_min) {
        return std::nullopt;
    }

    float t_enter = t_min;
    float t_exit = t_max;

    const float origin_arr[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float dir_arr[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float min_arr[3] = {box.min.x, box.min.y, box.min.z};
    const float max_arr[3] = {box.max.x, box.max.y, box.max.z};

    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(dir_arr[axis]) <= kStrictEpsilon) {
            if (origin_arr[axis] < min_arr[axis] || origin_arr[axis] > max_arr[axis]) {
                return std::nullopt;
            }
            continue;
        }

        const float inv_d = 1.0f / dir_arr[axis];
        float t0 = (min_arr[axis] - origin_arr[axis]) * inv_d;
        float t1 = (max_arr[axis] - origin_arr[axis]) * inv_d;
        if (t0 > t1) {
            std::swap(t0, t1);
        }

        t_enter = std::max(t_enter, t0);
        t_exit = std::min(t_exit, t1);
        if (t_exit < t_enter) {
            return std::nullopt;
        }
    }

    return t_enter;
}

std::optional<float> intersectRayTriangle(
    const Ray& ray,
    const Vec3& v0,
    const Vec3& v1,
    const Vec3& v2,
    bool backface_cull,
    float t_min,
    float t_max) noexcept {
    if (!ray.isValid()) {
        return std::nullopt;
    }

    const Vec3 edge1 = v1 - v0;
    const Vec3 edge2 = v2 - v0;
    const Vec3 pvec = ray.direction.cross(edge2);
    const float det = edge1.dot(pvec);

    if (backface_cull) {
        if (det <= 1.0e-7f) {
            return std::nullopt;
        }
    } else {
        if (std::fabs(det) <= 1.0e-7f) {
            return std::nullopt;
        }
    }

    const float inv_det = 1.0f / det;
    const Vec3 tvec = ray.origin - v0;
    const float u = tvec.dot(pvec) * inv_det;
    if (u < 0.0f || u > 1.0f) {
        return std::nullopt;
    }

    const Vec3 qvec = tvec.cross(edge1);
    const float v = ray.direction.dot(qvec) * inv_det;
    if (v < 0.0f || (u + v) > 1.0f) {
        return std::nullopt;
    }

    const float t = edge2.dot(qvec) * inv_det;
    if (t < t_min || t > t_max) {
        return std::nullopt;
    }

    return t;
}

} // namespace finaleffect::math
