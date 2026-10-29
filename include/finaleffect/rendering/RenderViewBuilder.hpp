// Final Effect 3D - scene to RenderView translation.
// SPDX-License-Identifier: GPL-3.0-only
//
// This is the one place that knows both the scene and the renderer interface. It
// produces a backend-agnostic frame description; it uploads nothing and owns no
// GPU resource, which is what keeps scene data and renderer state separate.
#pragma once

#include "finaleffect/core/Error.hpp"
#include "finaleffect/rendering/MeshLibrary.hpp"
#include "finaleffect/rendering/Renderer.hpp"
#include "finaleffect/scene/Scene.hpp"

namespace fe::rendering {

/// Builds one frame from the scene. Objects with an empty mesh name are ignored.
/// Returns NotFound when an object references a mesh the library does not know.
core::Result<RenderView> buildRenderView(const scene::Scene& scene,
                                         const Camera& camera,
                                         const MeshLibrary& meshes);

struct Bounds {
    math::Vec3 min{0.0F, 0.0F, 0.0F};
    math::Vec3 max{0.0F, 0.0F, 0.0F};
    bool valid = false;

    math::Vec3 center() const noexcept;
    math::Scalar halfDiagonal() const noexcept;
};

/// Axis-aligned world-space bounds of every visible mesh instance. Unknown mesh
/// names are skipped here so that diagnostics still work on partial scenes.
Bounds computeSceneBounds(const scene::Scene& scene, const MeshLibrary& meshes);

/// Camera framing the whole scene from a three-quarter view. Empty scenes get a
/// sensible default framing instead of a degenerate one.
Camera frameScene(const scene::Scene& scene, const MeshLibrary& meshes);

}  // namespace fe::rendering
