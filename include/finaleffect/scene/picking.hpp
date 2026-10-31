// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/ray.hpp"
#include "finaleffect/math/vec3.hpp"
#include "finaleffect/scene/object_id.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace finaleffect::scene {

class Scene;

struct RaycastOptions {
    bool ignore_invisible{true};
    bool backface_cull{false};
    float max_distance{std::numeric_limits<float>::max()};
    bool respect_display_layers{true};
    bool allow_meshes{true};
    bool allow_lights{true};
    bool allow_cameras{true};
    const std::vector<ObjectId>* isolated_objects{nullptr};
};

struct RaycastHit {
    ObjectId object_id{ObjectId::invalid()};
    float distance{0.0f};
    math::Vec3 world_point{};
    math::Vec3 world_normal{0.0f, 1.0f, 0.0f};
    std::uint32_t triangle_index{0};
};

// Broad-phase AABB + narrow-phase local-space Moller-Trumbore mesh raycast against a Scene.
[[nodiscard]] std::optional<RaycastHit> raycastScene(
    const Scene& scene,
    const math::Ray& world_ray,
    const RaycastOptions& options = {});

} // namespace finaleffect::scene
