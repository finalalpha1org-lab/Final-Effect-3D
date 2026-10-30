// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/math/mat4.hpp"
#include "finaleffect/math/quat.hpp"
#include "finaleffect/math/ray.hpp"
#include "finaleffect/math/transform.hpp"
#include "finaleffect/math/vec3.hpp"
#include "finaleffect/scene/camera.hpp"
#include "finaleffect/scene/mesh.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace finaleffect::app {

enum class GizmoMode : std::uint8_t {
    Translate = 0,
    Rotate = 1,
    Scale = 2,
};

enum class GizmoSpace : std::uint8_t {
    World = 0,
    Local = 1,
};

enum class GizmoHandle : std::uint8_t {
    None = 0,
    TranslateX,
    TranslateY,
    TranslateZ,
    TranslateXY,
    TranslateXZ,
    TranslateYZ,
    TranslateView,
    RotateX,
    RotateY,
    RotateZ,
    ScaleX,
    ScaleY,
    ScaleZ,
    ScaleUniform,
};

[[nodiscard]] std::string_view gizmoModeName(GizmoMode mode) noexcept;
[[nodiscard]] std::string_view gizmoSpaceName(GizmoSpace space) noexcept;
[[nodiscard]] std::string_view gizmoHandleName(GizmoHandle handle) noexcept;

struct GizmoFrame {
    math::Vec3 origin{0.0f, 0.0f, 0.0f};
    std::array<math::Vec3, 3> axes{
        math::Vec3{1.0f, 0.0f, 0.0f},
        math::Vec3{0.0f, 1.0f, 0.0f},
        math::Vec3{0.0f, 0.0f, 1.0f},
    };
    float scale{1.0f};
};

struct GizmoPickResult {
    GizmoHandle handle{GizmoHandle::None};
    float ray_t{0.0f};
    math::Vec3 hit_point{};
};

struct GizmoDragOutput {
    math::Transform local_transform{};
    math::Vec3 world_translation_delta{};
    math::Quat world_rotation_delta{math::Quat::identity()};
    math::Vec3 local_scale_factor{1.0f, 1.0f, 1.0f};
};

// Pure-C++20 3D viewport transform manipulator (Translate, Rotate, Scale).
// Supports perspective & orthographic cameras, World & Local coordinate spaces,
// exact ray-axis/plane/ring picking & solving, safe drag cancellation, and 3D mesh generation.
class TransformGizmo {
public:
    TransformGizmo() = default;

    [[nodiscard]] GizmoMode mode() const noexcept { return mode_; }
    void setMode(GizmoMode mode) noexcept { mode_ = mode; }

    [[nodiscard]] GizmoSpace space() const noexcept { return space_; }
    void setSpace(GizmoSpace space) noexcept { space_ = space; }

    [[nodiscard]] GizmoHandle hoveredHandle() const noexcept { return hovered_handle_; }
    void setHoveredHandle(GizmoHandle handle) noexcept { hovered_handle_ = handle; }

    [[nodiscard]] GizmoHandle activeHandle() const noexcept { return active_handle_; }
    [[nodiscard]] bool isDragging() const noexcept { return dragging_; }

    // Computes screen-space-aware gizmo frame for a target world matrix, camera, and viewport resolution/DPI.
    [[nodiscard]] static GizmoFrame computeFrame(
        const math::Mat4& target_world_matrix,
        const math::Mat4& camera_world_matrix,
        const scene::CameraComponent& camera,
        GizmoMode mode,
        GizmoSpace space,
        std::uint32_t viewport_width = 0,
        std::uint32_t viewport_height = 0,
        float pixel_scale = 1.0f) noexcept;

    // Raycasts individual 3D gizmo handles in world space.
    [[nodiscard]] static std::optional<GizmoPickResult> pickHandle(
        GizmoMode mode,
        const GizmoFrame& frame,
        const math::Ray& world_ray) noexcept;

    // Proximity halo guard used to prevent accidental background object reselection on near-handle clicks.
    [[nodiscard]] static bool isNearAnyHandle(
        GizmoMode mode,
        const GizmoFrame& frame,
        const math::Ray& world_ray) noexcept;

    // Begins an interactive drag operation.
    [[nodiscard]] core::Status beginDrag(
        GizmoHandle handle,
        const GizmoFrame& frame,
        const math::Ray& initial_world_ray,
        const math::Mat4& camera_world_matrix,
        const math::Transform& initial_local_transform,
        const math::Mat4& parent_world_matrix);

    // Evaluates current drag state from a live viewport ray without heap allocations.
    [[nodiscard]] core::Result<GizmoDragOutput> updateDrag(const math::Ray& current_world_ray) noexcept;

    // Completes drag session cleanly.
    void endDrag() noexcept;

    // Cancels active drag and returns the exact pre-drag local transform.
    [[nodiscard]] math::Transform cancelDrag() noexcept;

    // Builds actual 3D gizmo geometry in world space with hover & active handle highlighting.
    [[nodiscard]] static scene::MeshData buildRenderMesh(
        GizmoMode mode,
        const GizmoFrame& frame,
        GizmoHandle hovered_handle = GizmoHandle::None,
        GizmoHandle active_handle = GizmoHandle::None);

private:
    GizmoMode mode_{GizmoMode::Translate};
    GizmoSpace space_{GizmoSpace::World};
    GizmoHandle hovered_handle_{GizmoHandle::None};
    GizmoHandle active_handle_{GizmoHandle::None};
    bool dragging_{false};
    bool rotate_use_plane_intersection_{true};

    GizmoFrame start_frame_{};
    math::Transform initial_local_transform_{};
    math::Mat4 parent_world_matrix_{math::Mat4::identity()};
    math::Mat4 inv_parent_world_matrix_{math::Mat4::identity()};
    math::Quat parent_world_rotation_{math::Quat::identity()};

    math::Vec3 cam_forward_{0.0f, 0.0f, -1.0f};
    math::Vec3 cam_right_{1.0f, 0.0f, 0.0f};
    math::Vec3 cam_up_{0.0f, 1.0f, 0.0f};

    math::Vec3 constraint_plane_normal_{0.0f, 1.0f, 0.0f};
    math::Vec3 start_plane_hit_{};
    float start_axis_param_{0.0f};
    math::Vec3 start_ring_dir_{1.0f, 0.0f, 0.0f};
    math::Vec3 start_ring_tangent_{0.0f, 1.0f, 0.0f};
    GizmoDragOutput last_valid_output_{};
};

} // namespace finaleffect::app
