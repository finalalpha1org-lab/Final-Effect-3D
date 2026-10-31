// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/rendering/render_types.hpp"

#include <cstdint>
#include <optional>

namespace finaleffect::rendering {

struct EditorGridPixelSample {
    bool visible{false};
    float window_depth{1.0f};
    math::Vec3 color{0.0f, 0.0f, 0.0f};
    float alpha{0.0f};
};

// Builds camera-relative procedural XZ ground grid draw parameters (double-precision modulo phases,
// continuous 3-cascade decade LOD, Perspective & Orthographic frustum differentials, and depth mapping).
[[nodiscard]] std::optional<EditorGridCommand> buildEditorGridCommand(
    const math::Mat4& view_matrix,
    const math::Mat4& proj_matrix,
    const math::Vec3& camera_pos,
    double camera_world_x_precise,
    double camera_world_z_precise,
    math::ClipSpaceDepth clip_space_depth,
    const ViewportGridOptions& options) noexcept;

// Convenience overload using camera_pos directly for world XZ coordinates.
[[nodiscard]] inline std::optional<EditorGridCommand> buildEditorGridCommand(
    const math::Mat4& view_matrix,
    const math::Mat4& proj_matrix,
    const math::Vec3& camera_pos,
    math::ClipSpaceDepth clip_space_depth,
    const ViewportGridOptions& options) noexcept {
    return buildEditorGridCommand(
        view_matrix,
        proj_matrix,
        camera_pos,
        static_cast<double>(camera_pos.x),
        static_cast<double>(camera_pos.z),
        clip_space_depth,
        options);
}

// Evaluates the procedural XZ ground grid at normalized device coordinates (ndc_x, ndc_y in [-1, 1])
// with per-pixel NDC step (dx_ndc = 2/width, dy_ndc = 2/height). Identical math to the GLSL 330 / ES 3.00 shaders.
[[nodiscard]] EditorGridPixelSample evaluateEditorGridAtNdc(
    float ndc_x,
    float ndc_y,
    float dx_ndc,
    float dy_ndc,
    const EditorGridCommand& cmd) noexcept;

} // namespace finaleffect::rendering
