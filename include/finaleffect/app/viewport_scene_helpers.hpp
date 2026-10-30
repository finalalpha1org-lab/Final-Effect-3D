// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/aabb.hpp"
#include "finaleffect/math/ray.hpp"
#include "finaleffect/scene/mesh.hpp"
#include "finaleffect/scene/object_id.hpp"
#include "finaleffect/scene/picking.hpp"
#include "finaleffect/scene/scene.hpp"

#include <optional>

namespace finaleffect::app {

struct ViewportHelperBuildOptions {
    float viewport_aspect{16.0f / 9.0f};
    scene::ObjectId selected_object{scene::ObjectId::invalid()};
    // When looking directly through the active scene camera, omit its own body/frustum so it does not block the lens.
    scene::ObjectId hidden_camera_object{scene::ObjectId::invalid()};
};

// Returns true iff `id` exists and `visible == true` on `id` and every ancestor in `scene`.
[[nodiscard]] bool isObjectVisibleInHierarchy(const scene::Scene& scene, scene::ObjectId id) noexcept;

// Builds a combined world-space MeshData of 3D editor helpers for all visible CameraComponent
// and DirectionalLightComponent objects in `scene`. Never mutates `scene` or registers MeshAssets,
// so helpers are strictly excluded from final scene renders and project files.
[[nodiscard]] scene::MeshData buildViewportSceneHelpersMesh(
    const scene::Scene& scene,
    const ViewportHelperBuildOptions& options = {});

// Performs unified depth-ordered picking across visible scene meshes and (when enabled) compact
// 3D Camera / DirectionalLight helper proxies without allowing frustum lines to block mesh picking.
[[nodiscard]] std::optional<scene::RaycastHit> pickSceneOrViewportHelper(
    const scene::Scene& scene,
    const math::Ray& world_ray,
    bool include_editor_helpers = true,
    scene::ObjectId hidden_camera_object = scene::ObjectId::invalid(),
    const scene::RaycastOptions& options = {}) noexcept;

} // namespace finaleffect::app
