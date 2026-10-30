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

struct DirectionalLightUniforms {
    // Normalized world-space vector pointing FROM surface TOWARD light source.
    math::Vec3 direction_to_light{0.45f, 0.80f, 0.40f};
    math::Vec3 radiance{1.05f, 1.02f, 0.96f};
    math::Vec3 ambient{0.18f, 0.20f, 0.24f};
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

} // namespace finaleffect::rendering
