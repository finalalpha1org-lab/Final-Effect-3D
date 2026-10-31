// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/scene/picking.hpp"
#include "finaleffect/scene/scene.hpp"

#include <cmath>

namespace finaleffect::scene {

std::optional<RaycastHit> raycastScene(
    const Scene& scene,
    const math::Ray& world_ray,
    const RaycastOptions& options) {
    if (!world_ray.isValid() || options.max_distance <= 0.0f) {
        return std::nullopt;
    }

    const math::Ray unit_world_ray(world_ray.origin, world_ray.direction);
    float closest_t = options.max_distance;
    std::optional<RaycastHit> best_hit;

    for (const auto& obj : scene.objects()) {
        if (options.ignore_invisible) {
            ObjectId cur = obj.id;
            std::size_t guard = 0;
            bool vis = true;
            while (cur.isValid() && guard++ <= scene.objectCount()) {
                const auto* node = scene.findObject(cur);
                if (node == nullptr || !node->visible) {
                    vis = false;
                    break;
                }
                cur = node->parent_id;
            }
            if (!vis) {
                continue;
            }
        }
        if (!obj.mesh_id.has_value()) {
            continue;
        }

        const auto* mesh_asset = scene.findMesh(obj.mesh_id.value());
        if (mesh_asset == nullptr || !mesh_asset->local_bounds.isValid()) {
            continue;
        }

        const auto world_mat_res = scene.computeWorldMatrix(obj.id);
        if (!world_mat_res) {
            continue;
        }
        const math::Mat4& world_mat = world_mat_res.value();

        // 1. Broad-phase conservative world-AABB test.
        const math::AABB world_aabb = mesh_asset->local_bounds.transformed(world_mat);
        const auto aabb_t = math::intersectRayAabb(unit_world_ray, world_aabb, 0.0f, closest_t);
        if (!aabb_t.has_value()) {
            continue;
        }

        const auto inv_world_opt = world_mat.inverted();
        if (!inv_world_opt.has_value()) {
            continue;
        }
        const math::Mat4& inv_world = inv_world_opt.value();

        // 2. Transform ray into object's local mesh space to avoid CPU vertex buffer copies.
        const math::Vec3 local_origin = inv_world.transformPoint(unit_world_ray.origin);
        const math::Vec3 local_dir_unnorm = inv_world.transformDirection(unit_world_ray.direction);
        const float dir_scale = local_dir_unnorm.length();
        if (dir_scale <= math::kStrictEpsilon) {
            continue;
        }

        const math::Ray local_ray(local_origin, local_dir_unnorm * (1.0f / dir_scale));
        const float local_max_t = closest_t * dir_scale;

        // Refine against exact local AABB as well.
        if (!math::intersectRayAabb(local_ray, mesh_asset->local_bounds, 0.0f, local_max_t).has_value()) {
            continue;
        }

        const auto& verts = mesh_asset->data.vertices;
        const auto& indices = mesh_asset->data.indices;
        const math::Mat4 normal_matrix = inv_world.transposed();

        for (std::size_t ti = 0; ti + 2 < indices.size(); ti += 3) {
            const math::Vec3& p0 = verts[indices[ti + 0]].position;
            const math::Vec3& p1 = verts[indices[ti + 1]].position;
            const math::Vec3& p2 = verts[indices[ti + 2]].position;

            const auto hit_local_t = math::intersectRayTriangle(
                local_ray,
                p0,
                p1,
                p2,
                options.backface_cull,
                1.0e-5f,
                closest_t * dir_scale);

            if (!hit_local_t.has_value()) {
                continue;
            }

            const float hit_world_t = hit_local_t.value() / dir_scale;
            if (hit_world_t < closest_t) {
                closest_t = hit_world_t;
                const math::Vec3 local_normal = (p1 - p0).cross(p2 - p0).normalized();
                const math::Vec3 world_normal = normal_matrix.transformDirection(local_normal).normalized();

                best_hit = RaycastHit{
                    .object_id = obj.id,
                    .distance = hit_world_t,
                    .world_point = unit_world_ray.at(hit_world_t),
                    .world_normal = world_normal,
                    .triangle_index = static_cast<std::uint32_t>(ti / 3),
                };
            }
        }
    }

    return best_hit;
}

} // namespace finaleffect::scene
