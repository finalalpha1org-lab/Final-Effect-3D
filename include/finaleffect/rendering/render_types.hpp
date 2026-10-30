// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/constants.hpp"
#include "finaleffect/math/mat4.hpp"
#include "finaleffect/math/vec3.hpp"
#include "finaleffect/math/vec4.hpp"

#include <compare>
#include <cstdint>
#include <string>

namespace finaleffect::rendering {

struct GpuMeshHandle {
    std::uint64_t value{0};

    [[nodiscard]] static constexpr GpuMeshHandle invalid() noexcept { return GpuMeshHandle{0}; }
    [[nodiscard]] constexpr bool isValid() const noexcept { return value != 0; }
    constexpr auto operator<=>(const GpuMeshHandle&) const noexcept = default;
};

// Only backends actually implemented in the codebase are enumerated here.
enum class BackendType : std::uint8_t {
    Null = 0,
    Software = 1,
    OpenGL33 = 2,
    WebGL2 = 3
};

struct DeviceCapabilities {
    BackendType backend{BackendType::Null};
    std::string backend_name{"Null"};
    std::string device_name{"Headless Validation Device"};
    math::ClipSpaceDepth clip_space_depth{math::ClipSpaceDepth::NegativeOneToOne};
    std::uint32_t max_viewport_dimension{8192};
};

struct FrameClearDesc {
    math::Vec4 clear_color{0.09f, 0.11f, 0.14f, 1.0f};
    float clear_depth{1.0f};
};

struct DirectionalLightLobe {
    math::Vec3 direction_to_light{0.0f, 1.0f, 0.0f};
    math::Vec3 radiance{0.0f, 0.0f, 0.0f};
};

struct DirectionalLightUniforms {
    static constexpr std::size_t kMaxExtraLights = 3;

    // Primary normalized world-space vector pointing FROM surface TOWARD light source.
    math::Vec3 direction_to_light{0.45f, 0.80f, 0.40f};
    math::Vec3 radiance{1.05f, 1.02f, 0.96f};
    math::Vec3 ambient{0.18f, 0.20f, 0.24f};

    // Additional independent directional lights active in the scene (up to kMaxExtraLights).
    std::uint32_t extra_light_count{0};
    DirectionalLightLobe extra_lights[kMaxExtraLights]{};
};

// Metallic-roughness PBR surface & overlay shading parameters.
struct PbrSurfaceParams {
    math::Vec3 base_color{1.0f, 1.0f, 1.0f};
    float metallic{0.0f};
    float roughness{0.48f};
    float ambient_occlusion{1.0f};
    math::Vec3 emissive_radiance{0.0f, 0.0f, 0.0f};
    float opacity{1.0f};
    // 1.0 modulates base_color by mesh vertex colors; 0.0 uses pure material base_color.
    float vertex_color_weight{1.0f};
    // True renders unlit flat color (used for 3D transform gizmos and mesh-edit overlays).
    bool unlit_overlay{false};
};

struct DrawMeshCommand {
    GpuMeshHandle mesh_handle{GpuMeshHandle::invalid()};
    math::Mat4 model_matrix{math::Mat4::identity()};
    math::Mat4 view_projection_matrix{math::Mat4::identity()};
    math::Vec3 camera_position{0.0f, 0.0f, 5.0f};
    DirectionalLightUniforms lighting{};
    bool selected{false};
    PbrSurfaceParams surface{};
    std::uint32_t index_offset{0};
    std::uint32_t index_count{0}; // 0 draws entire index buffer from index_offset
};

// Viewport-only procedural ground grid options (never part of Scene or serialized geometry).
struct ViewportGridOptions {
    bool enabled{true};
    float base_scale{1.0f};
    float opacity{1.0f};
    // Optional camera-to-pivot focus distance hint for horizon-stable Perspective zoom scaling.
    float focus_distance{0.0f};
};

// Camera-relative procedural XZ ground grid draw parameters shared across CPU, OpenGL 3.3, and WebGL 2.0.
struct EditorGridCommand {
    math::Vec3 camera_position{0.0f, 2.95f, 6.10f};
    math::Vec3 camera_right{1.0f, 0.0f, 0.0f};
    math::Vec3 camera_up{0.0f, 1.0f, 0.0f};
    math::Vec3 camera_forward{0.0f, 0.0f, -1.0f};

    bool is_orthographic{false};
    bool zero_to_one_depth{false};

    // Frustum ray extents: tan(fov_x/2), tan(fov_y/2) for Perspective; half_width, half_height for Orthographic.
    float frustum_half_x{0.6f};
    float frustum_half_y{0.4f};

    // Projection depth row coefficients: clip_z = proj_22 * (-t) + proj_23.
    float proj_22{-1.001f};
    float proj_23{-0.2001f};

    // Continuous 3-cascade decade LOD steps: 10^(L0-1), 10^L0, 10^(L0+1) and fractional blend f in [0, 1).
    float step_fine{0.1f};
    float step_minor{1.0f};
    float step_major{10.0f};
    float level_blend{0.0f};
    float fade_distance{80.0f};

    // Double-precision pre-wrapped camera ground phases fmod(camera_xz, step_k) for large-coordinate stability.
    math::Vec3 phase_x{0.0f, 0.0f, 0.0f};
    math::Vec3 phase_z{0.0f, 0.0f, 0.0f};

    // Clamped world-origin camera XZ offset for X/Z principal axes and (0,0,0) origin marker.
    float axis_cam_x{0.0f};
    float axis_cam_z{0.0f};
    float opacity{1.0f};
};

} // namespace finaleffect::rendering
