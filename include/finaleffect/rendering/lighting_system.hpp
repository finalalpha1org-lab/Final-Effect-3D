// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/aabb.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/math/mat4.hpp"
#include "finaleffect/math/vec2.hpp"
#include "finaleffect/math/vec3.hpp"
#include "finaleffect/math/vec4.hpp"
#include "finaleffect/rendering/render_types.hpp"
#include "finaleffect/scene/environment.hpp"
#include "finaleffect/scene/light.hpp"
#include "finaleffect/scene/scene.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace finaleffect::rendering {

struct LightingLimits {
    static constexpr std::size_t kMaxRealtimeLights = 12;
    static constexpr std::size_t kMaxShadowCasters2D = 4;
    static constexpr std::size_t kMaxPointShadowCasters = 2;
    static constexpr std::uint32_t kMaxShadowAtlasSize = 4096;
    static constexpr float kPointFaceGuardTan = 1.03f;
};

struct LightingDiagnostics {
    std::uint32_t total_enabled_lights{0};
    std::uint32_t active_realtime_lights{0};
    std::uint32_t overflow_lights_folded{0};
    std::uint32_t requested_shadow_lights{0};
    std::uint32_t active_shadow_lights{0};
    std::uint32_t shadow_limit_clamped_lights{0};
    std::uint32_t shadow_tiles_rendered_last_frame{0};
    std::uint32_t shadow_tiles_cached_last_frame{0};
    std::uint32_t shadow_atlas_dimension{0};
    std::size_t shadow_gpu_bytes{0};
    std::size_t environment_gpu_bytes{0};
    double last_frame_render_ms{0.0};
    std::string warning_message;
};

struct PreparedRealtimeLight {
    scene::ObjectId object_id{scene::ObjectId::invalid()};
    scene::LightType type{scene::LightType::Directional};

    math::Vec3 position{0.0f, 0.0f, 0.0f};
    // Normalized forward emission direction (local -Z in world space).
    // For Directional lights, vector FROM surface TOWARD light is `-direction`.
    math::Vec3 direction{0.0f, -1.0f, 0.0f};
    math::Vec3 tangent_u{1.0f, 0.0f, 0.0f};
    math::Vec3 tangent_v{0.0f, 1.0f, 0.0f};

    math::Vec3 radiance{0.0f, 0.0f, 0.0f};
    float range{12.0f};
    float attenuation{2.0f};
    scene::LightFalloffMode falloff{scene::LightFalloffMode::InverseSquareWindowed};
    float source_radius{0.08f};

    float cos_inner{0.9396926f};
    float cos_outer{0.8191520f};
    float cone_falloff{1.0f};

    scene::AreaLightShape area_shape{scene::AreaLightShape::Rectangle};
    float half_width{0.8f};
    float half_height{0.5f};
    bool two_sided{false};
    std::uint32_t area_samples{9}; // 4, 9, or 16
    float area_flux_scale{1.0f};

    // -1 = no shadow; 0..3 = 2D shadow slot; 100..101 = 6-face omnidirectional Point shadow caster 0..1.
    std::int32_t shadow_slot{-1};
    float shadow_world_texel_size{0.02f};
    scene::LightShadowSettings shadow{};
    scene::LightLinkingSettings linking{};
};

struct ShadowCasterDrawRef {
    scene::ObjectId object_id{scene::ObjectId::invalid()};
    scene::MeshId mesh_id{scene::MeshId::invalid()};
    math::Mat4 world_matrix{math::Mat4::identity()};
};

struct ShadowTileDescriptor {
    // Unique stable slot key within the atlas (0..3 for 2D slots; 100 + caster*6 + face for Point faces).
    std::uint32_t tile_id{0};
    std::uint32_t x{0};
    std::uint32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    math::Mat4 light_view_proj{math::Mat4::identity()};
    std::uint64_t caster_signature{0};
    std::vector<ShadowCasterDrawRef> casters;
};

struct PreparedEnvironmentState {
    bool enabled{true};
    bool has_hdri{false};
    float intensity{1.0f};
    float cos_rotation{1.0f};
    float sin_rotation{0.0f};
    float diffuse_ibl_scale{1.0f};
    float specular_ibl_scale{1.0f};
    math::Vec3 fallback_ambient{0.18f, 0.20f, 0.24f};
    math::Vec3 overflow_irradiance{0.0f, 0.0f, 0.0f};
    std::uint64_t hdri_revision{0};
    const scene::HdriEnvironmentAsset* hdri_asset{nullptr};
};

struct PreparedSceneLighting {
    std::uint32_t light_count{0};
    bool apply_primary_studio_fill{false};
    std::array<PreparedRealtimeLight, LightingLimits::kMaxRealtimeLights> lights{};

    std::uint32_t shadow_atlas_size{0};
    std::vector<ShadowTileDescriptor> shadow_tiles;
    std::array<math::Mat4, LightingLimits::kMaxShadowCasters2D> shadow_2d_view_proj{};
    // xy = tile UV origin, zw = tile UV size in [0, 1]^2.
    std::array<math::Vec4, LightingLimits::kMaxShadowCasters2D> shadow_2d_uv_rects{};
    // xy = base UV origin of 3x2 face block, z = single-face UV size, w = near_plane.
    std::array<math::Vec4, LightingLimits::kMaxPointShadowCasters> point_shadow_params{};
    std::array<float, LightingLimits::kMaxPointShadowCasters> point_shadow_far{};

    PreparedEnvironmentState environment{};
    LightingDiagnostics diagnostics{};

    // Packed GPU uniform arrays ready for single-call glUniform4fv / glUniformMatrix4fv upload.
    std::array<float, LightingLimits::kMaxRealtimeLights * 4> gpu_light_pos_type{};
    std::array<float, LightingLimits::kMaxRealtimeLights * 4> gpu_light_dir_range{};
    std::array<float, LightingLimits::kMaxRealtimeLights * 4> gpu_light_radiance_atten{};
    std::array<float, LightingLimits::kMaxRealtimeLights * 4> gpu_light_shape{};
    std::array<float, LightingLimits::kMaxRealtimeLights * 4> gpu_light_tangent_shadow{};
    std::array<float, LightingLimits::kMaxRealtimeLights * 4> gpu_light_shadow_params{};
    std::array<float, 9 * 3> gpu_ibl_sh_rgb{};

    [[nodiscard]] std::uint32_t computeObjectLightMask(
        const scene::Scene& scene,
        scene::ObjectId object_id) const noexcept;
};

// Builds the unified frame lighting, shadow-atlas tile plan, caster hashes, GPU uniform buffers,
// and deterministic limit diagnostics from `scene`.
[[nodiscard]] PreparedSceneLighting prepareSceneLighting(const scene::Scene& scene);

// Evaluates exact physically based Cook-Torrance GGX direct + area + shadow + SH9/Cubemap IBL
// fragment color on CPU (used by SoftwareRenderDevice and lighting unit verification).
[[nodiscard]] math::Vec3 evaluateUnifiedPbrFragmentCpu(
    const math::Vec3& world_pos,
    const math::Vec3& world_normal_raw,
    const math::Vec3& vertex_color,
    const DrawMeshCommand& cmd,
    std::span<const float> shadow_atlas_depth = {},
    std::uint32_t shadow_atlas_size = 0) noexcept;

// Analytic helper for Point Light 6-face omnidirectional shadow projection.
[[nodiscard]] math::Mat4 computePointShadowFaceViewProj(
    const math::Vec3& light_pos,
    std::uint32_t face_index,
    float z_near,
    float z_far) noexcept;

} // namespace finaleffect::rendering
