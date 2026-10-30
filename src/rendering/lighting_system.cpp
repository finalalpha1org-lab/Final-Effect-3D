// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/rendering/lighting_system.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace finaleffect::rendering {

namespace {

bool isVisibleInHierarchy(const scene::Scene& scene, scene::ObjectId id) noexcept {
    scene::ObjectId cur = id;
    std::size_t guard = 0;
    const std::size_t max_depth = scene.objectCount() + 1u;
    while (cur.isValid() && guard++ < max_depth) {
        const auto* obj = scene.findObject(cur);
        if (obj == nullptr || !obj->visible) {
            return false;
        }
        cur = obj->parent_id;
    }
    return true;
}

math::Vec3 safeNormalize(const math::Vec3& v, const math::Vec3& fallback) noexcept {
    const float len_sq = v.lengthSquared();
    if (!std::isfinite(len_sq) || len_sq <= 1.0e-10f) {
        return fallback;
    }
    return v * (1.0f / std::sqrt(len_sq));
}

math::Vec3 lerpVec3(const math::Vec3& a, const math::Vec3& b, float t) noexcept {
    return a * (1.0f - t) + b * t;
}

std::uint64_t hashCombine64(std::uint64_t seed, std::uint64_t val) noexcept {
    seed ^= val + 0x9E3779B97F4A7C15ull + (seed << 6u) + (seed >> 2u);
    return seed;
}

std::uint64_t hashFloat(std::uint64_t seed, float f) noexcept {
    const float clean = (f == 0.0f) ? 0.0f : f;
    return hashCombine64(seed, static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(clean)));
}

std::uint64_t hashMat4(std::uint64_t seed, const math::Mat4& m) noexcept {
    const float* ptr = m.raw();
    for (int i = 0; i < 16; ++i) {
        seed = hashFloat(seed, ptr[i]);
    }
    return seed;
}

std::uint32_t roundUpPow2(std::uint32_t v) noexcept {
    if (v <= 512u) return 512u;
    if (v <= 1024u) return 1024u;
    if (v <= 2048u) return 2048u;
    return 4096u;
}

math::Vec3 rotateAroundY(const math::Vec3& v, float cos_a, float sin_a) noexcept {
    return math::Vec3{
        v.x * cos_a + v.z * sin_a,
        v.y,
        -v.x * sin_a + v.z * cos_a,
    };
}

math::Vec3 fresnelSchlick(float cos_theta, const math::Vec3& f0) noexcept {
    const float x = std::clamp(1.0f - cos_theta, 0.0f, 1.0f);
    const float x2 = x * x;
    return f0 + (math::Vec3{1.0f, 1.0f, 1.0f} - f0) * (x2 * x2 * x);
}

math::Vec3 fresnelSchlickRoughness(float cos_theta, const math::Vec3& f0, float roughness) noexcept {
    const float x = std::clamp(1.0f - cos_theta, 0.0f, 1.0f);
    const float x2 = x * x;
    const float gloss = 1.0f - roughness;
    const math::Vec3 max_f{
        std::max(gloss, f0.x),
        std::max(gloss, f0.y),
        std::max(gloss, f0.z),
    };
    return f0 + (max_f - f0) * (x2 * x2 * x);
}

// Karis analytic Split-Sum Environment BRDF approximation (scale, bias).
math::Vec2 envBrdfApprox(float roughness, float n_dot_v) noexcept {
    const math::Vec4 c0{-1.0f, -0.0275f, -0.572f, 0.022f};
    const math::Vec4 c1{1.0f, 0.0425f, 1.04f, -0.04f};
    const math::Vec4 r = c0 * roughness + c1;
    const float a004 = std::min(r.x * r.x, std::exp2(-9.28f * n_dot_v)) * r.x + r.y;
    return math::Vec2{
        std::clamp(-1.04f * a004 + r.z, 0.0f, 1.0f),
        std::clamp(a004 + r.w, 0.0f, 1.0f),
    };
}

float distributionGgx(float n_dot_h, float roughness) noexcept {
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float denom_term = n_dot_h * n_dot_h * (a2 - 1.0f) + 1.0f;
    const float denom = math::kPi * denom_term * denom_term;
    return a2 / std::max(denom, 1.0e-6f);
}

float geometrySchlickGgx(float n_dot_x, float k) noexcept {
    return n_dot_x / std::max(n_dot_x * (1.0f - k) + k, 1.0e-5f);
}

float evaluateDistanceAttenuation(
    float dist,
    float range,
    float exponent,
    float source_radius,
    scene::LightFalloffMode mode) noexcept {
    if (dist >= range || range <= 1.0e-4f) {
        return 0.0f;
    }
    const float u = std::clamp(dist / range, 0.0f, 1.0f);
    const float d_eff = std::max(dist, std::max(source_radius, 0.05f));

    switch (mode) {
        case scene::LightFalloffMode::InverseSquareWindowed: {
            const float u2 = u * u;
            const float win = std::max(0.0f, 1.0f - u2 * u2);
            const float window = win * win;
            const float denom = (std::fabs(exponent - 2.0f) <= 1.0e-4f)
                                    ? (d_eff * d_eff)
                                    : std::pow(d_eff, std::max(exponent, 0.0f));
            return window / std::max(denom, 1.0e-4f);
        }
        case scene::LightFalloffMode::SmoothQuadratic: {
            const float one_minus = 1.0f - u;
            return (one_minus * one_minus) / (1.0f + 0.18f * exponent * d_eff * d_eff);
        }
        case scene::LightFalloffMode::SmoothLinear: {
            return (1.0f - u) / (1.0f + 0.12f * exponent * d_eff);
        }
    }
    return 0.0f;
}

float evaluateSpotConeFactor(
    const math::Vec3& light_forward,
    const math::Vec3& dir_to_light,
    float cos_outer,
    float inv_cos_diff,
    float cone_falloff) noexcept {
    const float cos_theta = light_forward.dot(-dir_to_light);
    if (cos_theta <= cos_outer) {
        return 0.0f;
    }
    const float t = std::clamp((cos_theta - cos_outer) * inv_cos_diff, 0.0f, 1.0f);
    const float smooth = t * t * (3.0f - 2.0f * t);
    return (std::fabs(cone_falloff - 1.0f) <= 1.0e-4f)
               ? smooth
               : std::pow(smooth, std::max(cone_falloff, 0.1f));
}

// Shirley-Chiu concentric square [-1, 1]^2 -> unit disk mapping for uniform Disk AreaLight quadrature.
math::Vec2 concentricSampleDisk(float sx, float sy) noexcept {
    if (std::fabs(sx) <= 1.0e-6f && std::fabs(sy) <= 1.0e-6f) {
        return {0.0f, 0.0f};
    }
    float r = 0.0f;
    float theta = 0.0f;
    if (std::fabs(sx) > std::fabs(sy)) {
        r = sx;
        theta = (math::kPi * 0.25f) * (sy / sx);
    } else {
        r = sy;
        theta = (math::kPi * 0.5f) - (math::kPi * 0.25f) * (sx / sy);
    }
    return {r * std::cos(theta), r * std::sin(theta)};
}

float encodeShadowFilterSoftness(scene::ShadowFilterMode filter, float softness) noexcept {
    const float s = std::clamp(softness, 0.05f, 8.0f);
    switch (filter) {
        case scene::ShadowFilterMode::Hard: return -s;
        case scene::ShadowFilterMode::Pcf3x3: return s;
        case scene::ShadowFilterMode::Pcf5x5: return 100.0f + s;
    }
    return s;
}

float sampleShadowAtlasTexel(
    std::span<const float> depth,
    std::uint32_t atlas_size,
    float u_clamped,
    float v_clamped) noexcept {
    if (depth.empty() || atlas_size == 0u) {
        return 1.0f;
    }
    const auto px = std::min<std::uint32_t>(
        static_cast<std::uint32_t>(std::max(0.0f, u_clamped * static_cast<float>(atlas_size))),
        atlas_size - 1u);
    const auto py = std::min<std::uint32_t>(
        static_cast<std::uint32_t>(std::max(0.0f, v_clamped * static_cast<float>(atlas_size))),
        atlas_size - 1u);
    return depth[static_cast<std::size_t>(py) * atlas_size + px];
}

float filterShadowTile(
    std::span<const float> atlas_depth,
    std::uint32_t atlas_size,
    const math::Vec4& uv_rect,
    float local_u,
    float local_v,
    float receiver_depth,
    float depth_bias,
    scene::ShadowFilterMode filter,
    float softness_scale) noexcept {
    if (atlas_depth.empty() || atlas_size == 0u || uv_rect.z <= 0.0f || uv_rect.w <= 0.0f) {
        return 1.0f;
    }
    if (local_u < 0.0f || local_u > 1.0f || local_v < 0.0f || local_v > 1.0f || receiver_depth > 1.0f) {
        return 1.0f;
    }
    if (receiver_depth <= 0.0f) {
        return 1.0f;
    }

    const float inv_atlas = 1.0f / static_cast<float>(atlas_size);
    const float min_u = uv_rect.x + 0.5f * inv_atlas;
    const float max_u = uv_rect.x + uv_rect.z - 0.5f * inv_atlas;
    const float min_v = uv_rect.y + 0.5f * inv_atlas;
    const float max_v = uv_rect.y + uv_rect.w - 0.5f * inv_atlas;

    const float base_u = uv_rect.x + local_u * uv_rect.z;
    const float base_v = uv_rect.y + local_v * uv_rect.w;
    const float compare_z = receiver_depth - depth_bias;

    if (filter == scene::ShadowFilterMode::Hard) {
        const float d = sampleShadowAtlasTexel(
            atlas_depth,
            atlas_size,
            std::clamp(base_u, min_u, max_u),
            std::clamp(base_v, min_v, max_v));
        return (compare_z <= d) ? 1.0f : 0.0f;
    }

    const float step = inv_atlas * softness_scale;
    const int radius = (filter == scene::ShadowFilterMode::Pcf5x5) ? 2 : 1;
    float lit_sum = 0.0f;
    int tap_count = 0;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const float su = std::clamp(base_u + static_cast<float>(dx) * step, min_u, max_u);
            const float sv = std::clamp(base_v + static_cast<float>(dy) * step, min_v, max_v);
            const float stored = sampleShadowAtlasTexel(atlas_depth, atlas_size, su, sv);
            lit_sum += (compare_z <= stored) ? 1.0f : 0.0f;
            ++tap_count;
        }
    }
    return lit_sum / static_cast<float>(tap_count);
}

float evaluateLightShadowCpu(
    const PreparedSceneLighting& scene_lights,
    const PreparedRealtimeLight& light,
    const math::Vec3& world_pos,
    const math::Vec3& unit_normal,
    const math::Vec3& dir_to_light,
    std::span<const float> atlas_depth,
    std::uint32_t atlas_size) noexcept {
    if (light.shadow_slot < 0 || !light.shadow.enabled || light.shadow.strength <= 1.0e-4f ||
        atlas_depth.empty() || atlas_size == 0u) {
        return 1.0f;
    }

    const float n_dot_l = std::clamp(unit_normal.dot(dir_to_light), 0.0f, 1.0f);
    const float slope = 1.0f - n_dot_l;
    const math::Vec3 biased_pos =
        world_pos + unit_normal * (light.shadow.normal_bias * light.shadow_world_texel_size * (0.35f + 0.65f * slope));
    const float depth_bias = light.shadow.bias * (1.0f + 1.5f * slope);

    float raw_vis = 1.0f;
    if (light.shadow_slot >= 0 && light.shadow_slot < static_cast<std::int32_t>(LightingLimits::kMaxShadowCasters2D)) {
        const auto slot = static_cast<std::size_t>(light.shadow_slot);
        const math::Vec4 clip = scene_lights.shadow_2d_view_proj[slot] * math::Vec4(biased_pos, 1.0f);
        if (clip.w <= 1.0e-5f) {
            return 1.0f;
        }
        const float inv_w = 1.0f / clip.w;
        const float u = clip.x * inv_w * 0.5f + 0.5f;
        const float v = clip.y * inv_w * 0.5f + 0.5f;
        const float z_win = clip.z * inv_w * 0.5f + 0.5f;

        float softness_scale = light.shadow.softness;
        if (light.type == scene::LightType::Area) {
            // Area-light emitter size widens soft penumbra proportionally to physical emitter extent.
            softness_scale *= (1.0f + 1.6f * std::max(light.half_width, light.half_height));
        }

        raw_vis = filterShadowTile(
            atlas_depth,
            atlas_size,
            scene_lights.shadow_2d_uv_rects[slot],
            u,
            v,
            z_win,
            depth_bias,
            light.shadow.filter,
            softness_scale);
    } else if (light.shadow_slot >= 100 &&
               light.shadow_slot < static_cast<std::int32_t>(100 + LightingLimits::kMaxPointShadowCasters)) {
        const auto p_idx = static_cast<std::size_t>(light.shadow_slot - 100);
        const math::Vec4 params = scene_lights.point_shadow_params[p_idx];
        const float z_near = params.w;
        const float z_far = scene_lights.point_shadow_far[p_idx];

        const math::Vec3 d = biased_pos - light.position;
        const float ax = std::fabs(d.x);
        const float ay = std::fabs(d.y);
        const float az = std::fabs(d.z);

        std::uint32_t face = 0;
        float xv = 0.0f;
        float yv = 0.0f;
        float z_eye = 1.0f;
        if (ax >= ay && ax >= az) {
            z_eye = ax;
            if (d.x >= 0.0f) { face = 0; xv = +d.z; yv = +d.y; }
            else             { face = 1; xv = -d.z; yv = +d.y; }
        } else if (ay >= ax && ay >= az) {
            z_eye = ay;
            if (d.y >= 0.0f) { face = 2; xv = +d.x; yv = +d.z; }
            else             { face = 3; xv = +d.x; yv = -d.z; }
        } else {
            z_eye = az;
            if (d.z >= 0.0f) { face = 4; xv = -d.x; yv = +d.y; }
            else             { face = 5; xv = +d.x; yv = +d.y; }
        }

        if (z_eye <= z_near || z_eye >= z_far) {
            return 1.0f;
        }
        const float inv_guard = 1.0f / (z_eye * LightingLimits::kPointFaceGuardTan);
        const float u = 0.5f * (xv * inv_guard) + 0.5f;
        const float v = 0.5f * (yv * inv_guard) + 0.5f;
        const float z_win = (z_far * (z_eye - z_near)) / ((z_far - z_near) * z_eye);

        const float tile_sz = params.z;
        const math::Vec4 face_rect{
            params.x + static_cast<float>(face % 3u) * tile_sz,
            params.y + static_cast<float>(face / 3u) * tile_sz,
            tile_sz,
            tile_sz,
        };
        raw_vis = filterShadowTile(
            atlas_depth,
            atlas_size,
            face_rect,
            u,
            v,
            z_win,
            depth_bias,
            light.shadow.filter,
            light.shadow.softness);
    }

    return 1.0f - light.shadow.strength * (1.0f - raw_vis);
}

} // namespace

math::Mat4 computePointShadowFaceViewProj(
    const math::Vec3& light_pos,
    std::uint32_t face_index,
    float z_near,
    float z_far) noexcept {
    math::Vec3 fwd{0.0f, 0.0f, -1.0f};
    math::Vec3 up{0.0f, 1.0f, 0.0f};
    switch (face_index) {
        case 0: fwd = {+1.0f, 0.0f, 0.0f}; up = {0.0f, +1.0f, 0.0f}; break; // +X
        case 1: fwd = {-1.0f, 0.0f, 0.0f}; up = {0.0f, +1.0f, 0.0f}; break; // -X
        case 2: fwd = {0.0f, +1.0f, 0.0f}; up = {0.0f, 0.0f, +1.0f}; break; // +Y
        case 3: fwd = {0.0f, -1.0f, 0.0f}; up = {0.0f, 0.0f, -1.0f}; break; // -Y
        case 4: fwd = {0.0f, 0.0f, +1.0f}; up = {0.0f, +1.0f, 0.0f}; break; // +Z
        default: fwd = {0.0f, 0.0f, -1.0f}; up = {0.0f, +1.0f, 0.0f}; break; // -Z
    }
    const float fov = 2.0f * std::atan(LightingLimits::kPointFaceGuardTan);
    const math::Mat4 view = math::Mat4::lookAtRH(light_pos, light_pos + fwd, up);
    const math::Mat4 proj = math::Mat4::perspectiveRH_NO(fov, 1.0f, z_near, z_far);
    return proj * view;
}

std::uint32_t PreparedSceneLighting::computeObjectLightMask(
    const scene::Scene& scene,
    scene::ObjectId object_id) const noexcept {
    std::uint32_t mask = 0u;
    for (std::uint32_t i = 0; i < light_count; ++i) {
        if (scene.doesLightAffectObject(lights[i].linking, object_id)) {
            mask |= (1u << i);
        }
    }
    return mask;
}

PreparedSceneLighting prepareSceneLighting(const scene::Scene& scene) {
    PreparedSceneLighting out{};

    // 1. Prepare Environment & IBL state.
    const auto& env = scene.environment();
    out.environment.enabled = env.enabled;
    out.environment.intensity = env.enabled ? std::max(0.0f, env.intensity) : 0.0f;
    out.environment.cos_rotation = std::cos(env.rotation_radians);
    out.environment.sin_rotation = std::sin(env.rotation_radians);
    out.environment.diffuse_ibl_scale = std::max(0.0f, env.diffuse_ibl_scale);
    out.environment.specular_ibl_scale = std::max(0.0f, env.specular_ibl_scale);
    out.environment.has_hdri = env.enabled && env.hasHdri();
    out.environment.hdri_revision = out.environment.has_hdri ? env.revision : 0u;
    out.environment.hdri_asset = out.environment.has_hdri ? env.hdri_asset.get() : nullptr;

    if (out.environment.has_hdri && out.environment.hdri_asset != nullptr) {
        const auto& sh = out.environment.hdri_asset->irradiance_sh.coeffs;
        for (std::size_t k = 0; k < 9u; ++k) {
            out.gpu_ibl_sh_rgb[k * 3u + 0u] = sh[k].x;
            out.gpu_ibl_sh_rgb[k * 3u + 1u] = sh[k].y;
            out.gpu_ibl_sh_rgb[k * 3u + 2u] = sh[k].z;
        }
    }

    // 2. Collect visible mesh objects (potential shadow casters + scene bounds).
    struct VisibleMeshEntry {
        scene::ObjectId object_id;
        scene::MeshId mesh_id;
        std::uint64_t mesh_revision{0};
        math::Mat4 world;
        math::AABB world_bounds;
    };
    std::vector<VisibleMeshEntry> visible_meshes;
    visible_meshes.reserve(scene.objectCount());

    for (const auto& obj : scene.objects()) {
        if (!obj.mesh_id.has_value() || !isVisibleInHierarchy(scene, obj.id)) {
            continue;
        }
        const auto* asset = scene.findMesh(*obj.mesh_id);
        if (asset == nullptr || asset->data.vertices.empty() || asset->data.indices.empty()) {
            continue;
        }
        auto wm = scene.computeWorldMatrix(obj.id);
        if (!wm) {
            continue;
        }
        visible_meshes.push_back(VisibleMeshEntry{
            .object_id = obj.id,
            .mesh_id = *obj.mesh_id,
            .mesh_revision = asset->revision,
            .world = wm.value(),
            .world_bounds = asset->local_bounds.transformed(wm.value()),
        });
    }

    // 3. Collect all candidate lights in Scene.
    struct CandidateLight {
        PreparedRealtimeLight prepared;
        float sort_power{0.0f};
    };
    std::vector<CandidateLight> candidates;
    bool scene_has_any_light_component = false;

    for (const auto& obj : scene.objects()) {
        if (!obj.hasAnyLight()) {
            continue;
        }
        scene_has_any_light_component = true;
        if (!isVisibleInHierarchy(scene, obj.id)) {
            continue;
        }
        auto wm_res = scene.computeWorldMatrix(obj.id);
        if (!wm_res) {
            continue;
        }
        const math::Mat4& wm = wm_res.value();
        const math::Vec3 pos = wm.transformPoint(math::Vec3::zero());
        const math::Vec3 raw_x{wm(0, 0), wm(1, 0), wm(2, 0)};
        const math::Vec3 raw_y{wm(0, 1), wm(1, 1), wm(2, 1)};
        const math::Vec3 raw_z{wm(0, 2), wm(1, 2), wm(2, 2)};
        const math::Vec3 forward = safeNormalize(-raw_z, math::Vec3{-0.45f, -0.80f, -0.40f}.normalized());
        const math::Vec3 normal_l = forward; // Area/Spot emit along local -Z = `forward`!
        const math::Vec3 u_axis = safeNormalize(
            raw_x - normal_l * raw_x.dot(normal_l),
            (std::fabs(normal_l.y) < 0.9f) ? math::Vec3{0.0f, 1.0f, 0.0f}.cross(normal_l).normalized()
                                           : math::Vec3{1.0f, 0.0f, 0.0f}.cross(normal_l).normalized());
        const math::Vec3 v_axis = safeNormalize(normal_l.cross(u_axis), raw_y.normalized());

        PreparedRealtimeLight pl{};
        pl.object_id = obj.id;
        pl.position = pos;
        pl.direction = forward;
        pl.tangent_u = u_axis;
        pl.tangent_v = v_axis;

        bool is_enabled = false;
        if (obj.directional_light.has_value()) {
            const auto& d = *obj.directional_light;
            is_enabled = d.enabled;
            pl.type = scene::LightType::Directional;
            pl.radiance = d.color * d.intensity;
            pl.shadow = d.shadow;
            pl.linking = d.linking;
        } else if (obj.point_light.has_value()) {
            const auto& p = *obj.point_light;
            is_enabled = p.enabled;
            pl.type = scene::LightType::Point;
            pl.radiance = p.color * p.intensity;
            pl.range = p.range;
            pl.attenuation = p.attenuation;
            pl.falloff = p.falloff;
            pl.source_radius = p.source_radius;
            pl.shadow = p.shadow;
            pl.linking = p.linking;
        } else if (obj.spot_light.has_value()) {
            const auto& s = *obj.spot_light;
            is_enabled = s.enabled;
            pl.type = scene::LightType::Spot;
            pl.radiance = s.color * s.intensity;
            pl.range = s.range;
            pl.attenuation = s.attenuation;
            pl.falloff = s.falloff;
            pl.cos_inner = std::cos(s.inner_cone_radians);
            pl.cos_outer = std::cos(s.outer_cone_radians);
            pl.cone_falloff = s.cone_falloff;
            pl.shadow = s.shadow;
            pl.linking = s.linking;
        } else if (obj.area_light.has_value()) {
            const auto& a = *obj.area_light;
            is_enabled = a.enabled;
            pl.type = scene::LightType::Area;
            pl.radiance = a.color * a.intensity;
            pl.area_shape = a.shape;
            const float sx = std::max(raw_x.length(), 0.05f);
            const float sy = std::max(raw_y.length(), 0.05f);
            pl.half_width = std::max(0.01f, 0.5f * a.width * sx);
            pl.half_height = (a.shape == scene::AreaLightShape::Disk)
                                 ? pl.half_width
                                 : std::max(0.01f, 0.5f * a.height * sy);
            pl.range = a.range;
            pl.two_sided = a.two_sided;
            pl.area_samples = (a.sample_quality <= 1u) ? 4u : ((a.sample_quality == 2u) ? 9u : 16u);
            const float area_val = (a.shape == scene::AreaLightShape::Disk)
                                       ? (math::kPi * pl.half_width * pl.half_width)
                                       : (4.0f * pl.half_width * pl.half_height);
            // Energy-conscious area flux factor: larger emitters deliver greater total luminous flux
            // without singular blowup for huge softboxes.
            pl.area_flux_scale = std::clamp(std::sqrt(std::max(area_val, 0.0004f)), 0.05f, 16.0f);
            pl.shadow = a.shadow;
            pl.linking = a.linking;
        }

        if (!is_enabled) {
            continue;
        }
        ++out.diagnostics.total_enabled_lights;
        if (pl.shadow.enabled) {
            ++out.diagnostics.requested_shadow_lights;
        }
        candidates.push_back(CandidateLight{
            .prepared = std::move(pl),
            .sort_power = pl.radiance.length(),
        });
    }

    // Fallback default studio key light ONLY when the scene defines zero light components at all.
    if (!scene_has_any_light_component) {
        PreparedRealtimeLight fallback{};
        fallback.type = scene::LightType::Directional;
        fallback.direction = -math::Vec3{0.45f, 0.80f, 0.40f}.normalized();
        fallback.radiance = {1.05f, 1.02f, 0.96f};
        out.lights[0] = fallback;
        out.light_count = 1u;
        out.apply_primary_studio_fill = true;
    } else if (!candidates.empty()) {
        // Preserve creation/ObjectId order when <= kMaxRealtimeLights; prioritize shadow + power on overflow.
        if (candidates.size() > LightingLimits::kMaxRealtimeLights) {
            std::stable_sort(
                candidates.begin(),
                candidates.end(),
                [](const CandidateLight& a, const CandidateLight& b) {
                    if (a.prepared.shadow.enabled != b.prepared.shadow.enabled) {
                        return a.prepared.shadow.enabled > b.prepared.shadow.enabled;
                    }
                    if (std::fabs(a.sort_power - b.sort_power) > 1.0e-4f) {
                        return a.sort_power > b.sort_power;
                    }
                    return a.prepared.object_id < b.prepared.object_id;
                });
        }

        out.light_count = static_cast<std::uint32_t>(
            std::min(candidates.size(), LightingLimits::kMaxRealtimeLights));
        out.diagnostics.active_realtime_lights = out.light_count;

        for (std::uint32_t i = 0; i < out.light_count; ++i) {
            out.lights[i] = std::move(candidates[i].prepared);
        }

        // Deterministic overflow handling: never silently discard excess lights!
        if (candidates.size() > LightingLimits::kMaxRealtimeLights) {
            out.diagnostics.overflow_lights_folded =
                static_cast<std::uint32_t>(candidates.size() - LightingLimits::kMaxRealtimeLights);
            for (std::size_t i = LightingLimits::kMaxRealtimeLights; i < candidates.size(); ++i) {
                const auto& ov = candidates[i].prepared;
                if (ov.type == scene::LightType::Directional) {
                    out.environment.overflow_irradiance += ov.radiance * 0.35f;
                } else {
                    out.environment.overflow_irradiance += ov.radiance * 0.04f;
                }
            }
        }

        if (out.light_count > 0u && out.lights[0].type == scene::LightType::Directional && !out.environment.has_hdri) {
            out.apply_primary_studio_fill = true;
        }
    }

    // 4. Allocate Shadow Atlas tiles & compute caster hashes for shadow-enabled lights.
    std::uint32_t used_2d_slots = 0u;
    std::uint32_t used_point_slots = 0u;

    // Shelf packer state inside [0..4096] x [0..4096].
    std::uint32_t shelf_x = 0u;
    std::uint32_t shelf_y = 0u;
    std::uint32_t shelf_h = 0u;
    std::uint32_t max_used_x = 0u;
    std::uint32_t max_used_y = 0u;

    auto packBlock = [&](std::uint32_t bw, std::uint32_t bh, std::uint32_t& out_x, std::uint32_t& out_y) -> bool {
        if (bw > LightingLimits::kMaxShadowAtlasSize || bh > LightingLimits::kMaxShadowAtlasSize) {
            return false;
        }
        if (shelf_x + bw > LightingLimits::kMaxShadowAtlasSize) {
            shelf_x = 0u;
            shelf_y += shelf_h;
            shelf_h = 0u;
        }
        if (shelf_y + bh > LightingLimits::kMaxShadowAtlasSize) {
            return false;
        }
        out_x = shelf_x;
        out_y = shelf_y;
        shelf_x += bw;
        shelf_h = std::max(shelf_h, bh);
        max_used_x = std::max(max_used_x, out_x + bw);
        max_used_y = std::max(max_used_y, out_y + bh);
        return true;
    };

    auto buildCasterListAndSig = [&](const PreparedRealtimeLight& lgt, const math::Mat4& vp) {
        std::vector<ShadowCasterDrawRef> casters;
        casters.reserve(visible_meshes.size());
        std::uint64_t sig = 14695981039346656037ull;
        sig = hashMat4(sig, vp);
        sig = hashCombine64(sig, static_cast<std::uint64_t>(lgt.shadow.quality));

        for (const auto& vm : visible_meshes) {
            if (!scene.doesLightAffectObject(lgt.linking, vm.object_id)) {
                continue;
            }
            casters.push_back(ShadowCasterDrawRef{
                .object_id = vm.object_id,
                .mesh_id = vm.mesh_id,
                .world_matrix = vm.world,
            });
            sig = hashCombine64(sig, vm.object_id.value);
            sig = hashCombine64(sig, vm.mesh_id.value);
            sig = hashCombine64(sig, vm.mesh_revision);
            sig = hashMat4(sig, vm.world);
        }
        return std::pair{std::move(casters), sig};
    };

    for (std::uint32_t i = 0; i < out.light_count; ++i) {
        auto& lgt = out.lights[i];
        if (!lgt.shadow.enabled) {
            continue;
        }

        if (lgt.type == scene::LightType::Point) {
            if (used_point_slots >= LightingLimits::kMaxPointShadowCasters) {
                ++out.diagnostics.shadow_limit_clamped_lights;
                continue;
            }
            const std::uint32_t face_res = std::min<std::uint32_t>(
                512u,
                (lgt.shadow.quality == scene::ShadowQuality::Low) ? 128u
                : (lgt.shadow.quality == scene::ShadowQuality::Medium) ? 256u : 512u);
            std::uint32_t bx = 0u;
            std::uint32_t by = 0u;
            if (!packBlock(face_res * 3u, face_res * 2u, bx, by)) {
                ++out.diagnostics.shadow_limit_clamped_lights;
                continue;
            }

            const std::uint32_t p_slot = used_point_slots++;
            const float z_near = std::clamp(lgt.range * 0.008f, 0.04f, 0.20f);
            const float z_far = std::max(lgt.range, z_near + 0.5f);
            out.point_shadow_far[p_slot] = z_far;
            // Temporarily store pixel coordinates in point_shadow_params; normalized after atlas_size is finalized.
            out.point_shadow_params[p_slot] = math::Vec4{
                static_cast<float>(bx),
                static_cast<float>(by),
                static_cast<float>(face_res),
                z_near,
            };

            lgt.shadow_slot = static_cast<std::int32_t>(100u + p_slot);
            lgt.shadow_world_texel_size = (2.0f * lgt.range) / static_cast<float>(face_res);
            ++out.diagnostics.active_shadow_lights;

            for (std::uint32_t face = 0; face < 6u; ++face) {
                const math::Mat4 vp = computePointShadowFaceViewProj(lgt.position, face, z_near, z_far);
                auto [casters, sig] = buildCasterListAndSig(lgt, vp);
                sig = hashCombine64(sig, face);
                out.shadow_tiles.push_back(ShadowTileDescriptor{
                    .tile_id = 100u + p_slot * 6u + face,
                    .x = bx + (face % 3u) * face_res,
                    .y = by + (face / 3u) * face_res,
                    .width = face_res,
                    .height = face_res,
                    .light_view_proj = vp,
                    .caster_signature = sig,
                    .casters = std::move(casters),
                });
            }
        } else {
            if (used_2d_slots >= LightingLimits::kMaxShadowCasters2D) {
                ++out.diagnostics.shadow_limit_clamped_lights;
                continue;
            }
            std::uint32_t res = scene::shadowMapResolution(lgt.shadow.quality);
            if (used_2d_slots >= 2u && res > 1024u) {
                res = 1024u;
            }
            std::uint32_t tx = 0u;
            std::uint32_t ty = 0u;
            if (!packBlock(res, res, tx, ty)) {
                ++out.diagnostics.shadow_limit_clamped_lights;
                continue;
            }

            const std::uint32_t s_slot = used_2d_slots++;
            math::Mat4 vp = math::Mat4::identity();

            if (lgt.type == scene::LightType::Directional) {
                math::AABB bounds = math::AABB::empty();
                for (const auto& vm : visible_meshes) {
                    if (scene.doesLightAffectObject(lgt.linking, vm.object_id)) {
                        bounds.expand(vm.world_bounds);
                    }
                }
                const math::Vec3 center = bounds.isValid() ? bounds.center() : math::Vec3::zero();
                const float radius = bounds.isValid()
                                         ? std::clamp(bounds.extents().length() + 0.75f, 2.5f, 120.0f)
                                         : 8.0f;

                const math::Vec3 up_ref = (std::fabs(lgt.direction.y) < 0.95f)
                                              ? math::Vec3::unitY()
                                              : math::Vec3::unitZ();
                const math::Mat4 rot_only = math::Mat4::lookAtRH(math::Vec3::zero(), lgt.direction, up_ref);
                math::Vec3 ls_center = rot_only.transformPoint(center);

                // Texel-snap light-space X/Y center so camera orbit/pan/zoom causes ZERO shadow swimming!
                const float world_per_texel = (2.0f * radius) / static_cast<float>(res);
                ls_center.x = std::floor(ls_center.x / world_per_texel) * world_per_texel;
                ls_center.y = std::floor(ls_center.y / world_per_texel) * world_per_texel;

                const math::Mat4 inv_rot = rot_only.transposed();
                const math::Vec3 snapped_world_center = inv_rot.transformPoint(ls_center);
                const float pull_back = radius + 4.0f;
                const math::Vec3 light_eye = snapped_world_center - lgt.direction * pull_back;
                const math::Mat4 view = math::Mat4::lookAtRH(light_eye, snapped_world_center, up_ref);
                const math::Mat4 proj = math::Mat4::orthographicRH_NO(
                    -radius, +radius, -radius, +radius,
                    0.1f, 2.0f * radius + 8.0f);
                vp = proj * view;
                lgt.shadow_world_texel_size = world_per_texel;
            } else if (lgt.type == scene::LightType::Spot) {
                const float outer_angle = std::acos(std::clamp(lgt.cos_outer, -1.0f, 1.0f));
                const float fov = std::clamp(2.0f * outer_angle * 1.04f, math::degToRad(4.0f), math::degToRad(175.0f));
                const float z_near = std::clamp(lgt.range * 0.008f, 0.04f, 0.25f);
                const float z_far = std::max(lgt.range, z_near + 0.5f);
                const math::Vec3 up_ref = (std::fabs(lgt.direction.y) < 0.95f)
                                              ? math::Vec3::unitY()
                                              : math::Vec3::unitZ();
                const math::Mat4 view = math::Mat4::lookAtRH(lgt.position, lgt.position + lgt.direction, up_ref);
                const math::Mat4 proj = math::Mat4::perspectiveRH_NO(fov, 1.0f, z_near, z_far);
                vp = proj * view;
                lgt.shadow_world_texel_size = (2.0f * std::tan(0.5f * fov) * (0.5f * z_far)) / static_cast<float>(res);
            } else {
                // Area Light: pulled-back wide-angle emitter frustum covering the finite panel + forward range.
                const float max_half = std::max(lgt.half_width, lgt.half_height);
                const float back_offset = std::max(0.25f, 1.15f * max_half);
                const math::Vec3 apex = lgt.position - lgt.direction * back_offset;
                const float fov = math::degToRad(118.0f);
                const float z_near = std::max(0.05f, back_offset * 0.45f);
                const float z_far = lgt.range + back_offset + 0.5f;
                const math::Mat4 view = math::Mat4::lookAtRH(apex, apex + lgt.direction, lgt.tangent_v);
                const math::Mat4 proj = math::Mat4::perspectiveRH_NO(fov, 1.0f, z_near, z_far);
                vp = proj * view;
                lgt.shadow_world_texel_size = (2.0f * lgt.range) / static_cast<float>(res);
            }

            lgt.shadow_slot = static_cast<std::int32_t>(s_slot);
            out.shadow_2d_view_proj[s_slot] = vp;
            out.shadow_2d_uv_rects[s_slot] = math::Vec4{
                static_cast<float>(tx),
                static_cast<float>(ty),
                static_cast<float>(res),
                static_cast<float>(res),
            };
            ++out.diagnostics.active_shadow_lights;

            auto [casters, sig] = buildCasterListAndSig(lgt, vp);
            out.shadow_tiles.push_back(ShadowTileDescriptor{
                .tile_id = s_slot,
                .x = tx,
                .y = ty,
                .width = res,
                .height = res,
                .light_view_proj = vp,
                .caster_signature = sig,
                .casters = std::move(casters),
            });
        }
    }

    if (!out.shadow_tiles.empty()) {
        out.shadow_atlas_size = roundUpPow2(std::max(max_used_x, max_used_y));
        out.diagnostics.shadow_atlas_dimension = out.shadow_atlas_size;
        const float inv_atlas = 1.0f / static_cast<float>(out.shadow_atlas_size);

        for (std::uint32_t s = 0; s < used_2d_slots; ++s) {
            out.shadow_2d_uv_rects[s].x *= inv_atlas;
            out.shadow_2d_uv_rects[s].y *= inv_atlas;
            out.shadow_2d_uv_rects[s].z *= inv_atlas;
            out.shadow_2d_uv_rects[s].w *= inv_atlas;
        }
        for (std::uint32_t p = 0; p < used_point_slots; ++p) {
            out.point_shadow_params[p].x *= inv_atlas;
            out.point_shadow_params[p].y *= inv_atlas;
            out.point_shadow_params[p].z *= inv_atlas;
        }
    }

    if (out.diagnostics.overflow_lights_folded > 0u || out.diagnostics.shadow_limit_clamped_lights > 0u) {
        std::string msg;
        if (out.diagnostics.overflow_lights_folded > 0u) {
            msg += "Active light limit (" + std::to_string(LightingLimits::kMaxRealtimeLights) +
                   ") exceeded: folded " + std::to_string(out.diagnostics.overflow_lights_folded) +
                   " excess light(s) into residual scene irradiance.";
        }
        if (out.diagnostics.shadow_limit_clamped_lights > 0u) {
            if (!msg.empty()) msg += " ";
            msg += "Shadow-caster budget exceeded: " +
                   std::to_string(out.diagnostics.shadow_limit_clamped_lights) +
                   " light(s) rendered unshadowed.";
        }
        out.diagnostics.warning_message = std::move(msg);
    }

    // 5. Pack contiguous float4 GPU uniform buffers.
    for (std::uint32_t i = 0; i < out.light_count; ++i) {
        const auto& l = out.lights[i];
        const std::size_t b = static_cast<std::size_t>(i) * 4u;

        out.gpu_light_pos_type[b + 0u] = l.position.x;
        out.gpu_light_pos_type[b + 1u] = l.position.y;
        out.gpu_light_pos_type[b + 2u] = l.position.z;
        out.gpu_light_pos_type[b + 3u] = static_cast<float>(static_cast<int>(l.type));

        out.gpu_light_dir_range[b + 0u] = l.direction.x;
        out.gpu_light_dir_range[b + 1u] = l.direction.y;
        out.gpu_light_dir_range[b + 2u] = l.direction.z;
        out.gpu_light_dir_range[b + 3u] = l.range;

        out.gpu_light_radiance_atten[b + 0u] = l.radiance.x;
        out.gpu_light_radiance_atten[b + 1u] = l.radiance.y;
        out.gpu_light_radiance_atten[b + 2u] = l.radiance.z;
        out.gpu_light_radiance_atten[b + 3u] = l.attenuation;

        if (l.type == scene::LightType::Point) {
            out.gpu_light_shape[b + 0u] = l.source_radius;
            out.gpu_light_shape[b + 1u] = static_cast<float>(static_cast<int>(l.falloff));
            out.gpu_light_shape[b + 2u] = 0.0f;
            out.gpu_light_shape[b + 3u] = 0.0f;
        } else if (l.type == scene::LightType::Spot) {
            out.gpu_light_shape[b + 0u] = l.cos_outer;
            out.gpu_light_shape[b + 1u] = 1.0f / std::max(l.cos_inner - l.cos_outer, 1.0e-4f);
            out.gpu_light_shape[b + 2u] = l.cone_falloff;
            out.gpu_light_shape[b + 3u] = static_cast<float>(static_cast<int>(l.falloff));
        } else if (l.type == scene::LightType::Area) {
            const int flags = (l.area_shape == scene::AreaLightShape::Disk ? 1 : 0) |
                              (l.two_sided ? 2 : 0) |
                              (static_cast<int>(l.area_samples) << 2);
            out.gpu_light_shape[b + 0u] = l.half_width;
            out.gpu_light_shape[b + 1u] = l.half_height;
            out.gpu_light_shape[b + 2u] = static_cast<float>(flags);
            out.gpu_light_shape[b + 3u] = l.area_flux_scale;
        }

        out.gpu_light_tangent_shadow[b + 0u] = l.tangent_u.x;
        out.gpu_light_tangent_shadow[b + 1u] = l.tangent_u.y;
        out.gpu_light_tangent_shadow[b + 2u] = l.tangent_u.z;
        out.gpu_light_tangent_shadow[b + 3u] = static_cast<float>(l.shadow_slot);

        out.gpu_light_shadow_params[b + 0u] = l.shadow.strength;
        out.gpu_light_shadow_params[b + 1u] = l.shadow.bias;
        out.gpu_light_shadow_params[b + 2u] = l.shadow.normal_bias * l.shadow_world_texel_size;
        out.gpu_light_shadow_params[b + 3u] = encodeShadowFilterSoftness(l.shadow.filter, l.shadow.softness);
    }

    return out;
}

math::Vec3 evaluateUnifiedPbrFragmentCpu(
    const math::Vec3& world_pos,
    const math::Vec3& world_normal_raw,
    const math::Vec3& vertex_color,
    const DrawMeshCommand& cmd,
    std::span<const float> shadow_atlas_depth,
    std::uint32_t shadow_atlas_size) noexcept {
    const PbrSurfaceParams& s = cmd.surface;
    const math::Vec3 weighted_vcol = lerpVec3(math::Vec3{1.0f, 1.0f, 1.0f}, vertex_color, s.vertex_color_weight);
    const math::Vec3 albedo = s.base_color.cwiseProduct(weighted_vcol);

    if (s.unlit_overlay) {
        const math::Vec3 tint = cmd.selected ? math::Vec3{0.25f, 0.16f, 0.04f} : math::Vec3::zero();
        return albedo + tint;
    }

    const math::Vec3 n = safeNormalize(world_normal_raw, math::Vec3::unitY());
    const math::Vec3 v = safeNormalize(cmd.camera_position - world_pos, math::Vec3::unitZ());
    const float n_dot_v = std::max(n.dot(v), 1.0e-4f);

    const float metallic = std::clamp(s.metallic, 0.0f, 1.0f);
    const float roughness = std::clamp(s.roughness, 0.045f, 1.0f);
    const float ao = std::clamp(s.ambient_occlusion, 0.0f, 1.0f);
    const float k_direct = ((roughness + 1.0f) * (roughness + 1.0f)) * 0.125f;
    const math::Vec3 f0 = lerpVec3(math::Vec3{0.04f, 0.04f, 0.04f}, albedo, metallic);

    auto evalCookTorrance = [&](const math::Vec3& l, const math::Vec3& radiance, float spec_roughness, float spec_norm) {
        const float n_dot_l = std::max(n.dot(l), 0.0f);
        if (n_dot_l <= 0.0f) {
            return math::Vec3::zero();
        }
        const math::Vec3 h = safeNormalize(v + l, n);
        const float n_dot_h = std::max(n.dot(h), 0.0f);
        const float h_dot_v = std::max(h.dot(v), 0.0f);

        const float ndf = distributionGgx(n_dot_h, spec_roughness) * spec_norm;
        const float geom = geometrySchlickGgx(n_dot_v, k_direct) * geometrySchlickGgx(n_dot_l, k_direct);
        const math::Vec3 fresnel = fresnelSchlick(h_dot_v, f0);

        const math::Vec3 kd = (math::Vec3{1.0f, 1.0f, 1.0f} - fresnel) * (1.0f - metallic);
        const math::Vec3 diffuse = kd.cwiseProduct(albedo) * (1.0f / math::kPi);
        const math::Vec3 specular = fresnel * ((ndf * geom) / std::max(4.0f * n_dot_v * n_dot_l, 1.0e-4f));
        return (diffuse + specular).cwiseProduct(radiance) * n_dot_l;
    };

    math::Vec3 lo = math::Vec3::zero();
    math::Vec3 ambient_term = math::Vec3::zero();

    if (cmd.scene_lighting != nullptr) {
        const PreparedSceneLighting& sl = *cmd.scene_lighting;
        for (std::uint32_t i = 0; i < sl.light_count; ++i) {
            if ((cmd.light_mask & (1u << i)) == 0u) {
                continue;
            }
            const PreparedRealtimeLight& lgt = sl.lights[i];
            if (lgt.radiance.lengthSquared() <= math::kStrictEpsilon) {
                continue;
            }

            if (lgt.type == scene::LightType::Directional) {
                const math::Vec3 l = safeNormalize(-lgt.direction, math::Vec3::unitY());
                const float vis = evaluateLightShadowCpu(
                    sl, lgt, world_pos, n, l, shadow_atlas_depth, shadow_atlas_size);
                lo += evalCookTorrance(l, lgt.radiance * vis, roughness, 1.0f);
                if (i == 0u && sl.apply_primary_studio_fill) {
                    const math::Vec3 fill_l = math::Vec3{-0.55f, 0.35f, -0.65f}.normalized();
                    lo += evalCookTorrance(fill_l, lgt.radiance * 0.22f, roughness, 1.0f);
                }
            } else if (lgt.type == scene::LightType::Point) {
                const math::Vec3 to_light = lgt.position - world_pos;
                const float dist = to_light.length();
                const float atten = evaluateDistanceAttenuation(
                    dist, lgt.range, lgt.attenuation, lgt.source_radius, lgt.falloff);
                if (atten <= 1.0e-6f) {
                    continue;
                }
                const math::Vec3 l = to_light * (1.0f / std::max(dist, 1.0e-5f));
                const float vis = evaluateLightShadowCpu(
                    sl, lgt, world_pos, n, l, shadow_atlas_depth, shadow_atlas_size);
                lo += evalCookTorrance(l, lgt.radiance * (atten * vis), roughness, 1.0f);
            } else if (lgt.type == scene::LightType::Spot) {
                const math::Vec3 to_light = lgt.position - world_pos;
                const float dist = to_light.length();
                const float atten = evaluateDistanceAttenuation(
                    dist, lgt.range, lgt.attenuation, 0.08f, lgt.falloff);
                if (atten <= 1.0e-6f) {
                    continue;
                }
                const math::Vec3 l = to_light * (1.0f / std::max(dist, 1.0e-5f));
                const float inv_diff = 1.0f / std::max(lgt.cos_inner - lgt.cos_outer, 1.0e-4f);
                const float cone = evaluateSpotConeFactor(
                    lgt.direction, l, lgt.cos_outer, inv_diff, lgt.cone_falloff);
                if (cone <= 1.0e-6f) {
                    continue;
                }
                const float vis = evaluateLightShadowCpu(
                    sl, lgt, world_pos, n, l, shadow_atlas_depth, shadow_atlas_size);
                lo += evalCookTorrance(l, lgt.radiance * (atten * cone * vis), roughness, 1.0f);
            } else if (lgt.type == scene::LightType::Area) {
                // 1. Stratified 2D Emitter Quadrature for Diffuse Area Illumination:
                const math::Vec3 n_emit = lgt.direction; // Emitter normal points along forward (-Z_local)
                const math::Vec3 center_delta = world_pos - lgt.position;
                const float signed_plane_dist = center_delta.dot(n_emit);
                if (!lgt.two_sided && signed_plane_dist <= 1.0e-4f) {
                    continue;
                }
                const float center_dist = center_delta.length();
                if (center_dist >= lgt.range + std::max(lgt.half_width, lgt.half_height)) {
                    continue;
                }

                const std::uint32_t grid_n = (lgt.area_samples <= 4u) ? 2u : ((lgt.area_samples <= 9u) ? 3u : 4u);
                const float inv_grid = 1.0f / static_cast<float>(grid_n);
                math::Vec3 diff_irradiance = math::Vec3::zero();

                for (std::uint32_t sy = 0; sy < grid_n; ++sy) {
                    for (std::uint32_t sx = 0; sx < grid_n; ++sx) {
                        const float su = 2.0f * ((static_cast<float>(sx) + 0.5f) * inv_grid) - 1.0f;
                        const float sv = 2.0f * ((static_cast<float>(sy) + 0.5f) * inv_grid) - 1.0f;
                        math::Vec2 offset{su * lgt.half_width, sv * lgt.half_height};
                        if (lgt.area_shape == scene::AreaLightShape::Disk) {
                            const math::Vec2 d2 = concentricSampleDisk(su, sv);
                            offset = {d2.x * lgt.half_width, d2.y * lgt.half_width};
                        }
                        const math::Vec3 sample_pos =
                            lgt.position + lgt.tangent_u * offset.x + lgt.tangent_v * offset.y;
                        const math::Vec3 to_sample = sample_pos - world_pos;
                        const float dist = to_sample.length();
                        if (dist <= 1.0e-5f || dist >= lgt.range) {
                            continue;
                        }
                        const math::Vec3 wi = to_sample * (1.0f / dist);
                        const float emit_cos_raw = n_emit.dot(-wi);
                        const float emit_cos = lgt.two_sided ? std::fabs(emit_cos_raw) : std::max(emit_cos_raw, 0.0f);
                        const float recv_cos = std::max(n.dot(wi), 0.0f);
                        if (emit_cos <= 0.0f || recv_cos <= 0.0f) {
                            continue;
                        }
                        const float atten = evaluateDistanceAttenuation(
                            dist, lgt.range, 2.0f, 0.12f, scene::LightFalloffMode::InverseSquareWindowed);
                        diff_irradiance += lgt.radiance * (emit_cos * recv_cos * atten);
                    }
                }
                diff_irradiance = diff_irradiance * (lgt.area_flux_scale / static_cast<float>(grid_n * grid_n));

                // 2. Karis Representative-Point Planar GGX Specular for Rectangle & Disk Emitters:
                const math::Vec3 r_vec = (n * (2.0f * n.dot(v)) - v).normalized();
                const float r_dot_plane = r_vec.dot(n_emit);
                math::Vec3 plane_hit = world_pos;
                if (std::fabs(r_dot_plane) > 1.0e-4f) {
                    const float t_ray = -signed_plane_dist / r_dot_plane;
                    plane_hit = world_pos + r_vec * std::max(t_ray, 0.0f);
                } else {
                    plane_hit = world_pos - n_emit * signed_plane_dist;
                }
                const math::Vec3 rel = plane_hit - lgt.position;
                float du = rel.dot(lgt.tangent_u);
                float dv = rel.dot(lgt.tangent_v);
                if (lgt.area_shape == scene::AreaLightShape::Disk) {
                    const float radial = std::sqrt(du * du + dv * dv);
                    if (radial > lgt.half_width && radial > 1.0e-6f) {
                        const float scale = lgt.half_width / radial;
                        du *= scale;
                        dv *= scale;
                    }
                } else {
                    du = std::clamp(du, -lgt.half_width, +lgt.half_width);
                    dv = std::clamp(dv, -lgt.half_height, +lgt.half_height);
                }

                const math::Vec3 rep_point = lgt.position + lgt.tangent_u * du + lgt.tangent_v * dv;
                const math::Vec3 to_rep = rep_point - world_pos;
                const float rep_dist = std::max(to_rep.length(), 1.0e-4f);
                const math::Vec3 l_spec = to_rep * (1.0f / rep_dist);
                const float emit_spec_raw = n_emit.dot(-l_spec);
                const float emit_spec = lgt.two_sided ? std::fabs(emit_spec_raw) : std::max(emit_spec_raw, 0.0f);

                const math::Vec3 l_center = safeNormalize(lgt.position - world_pos, -n_emit);
                const float vis = evaluateLightShadowCpu(
                    sl, lgt, world_pos, n, l_center, shadow_atlas_depth, shadow_atlas_size);

                const math::Vec3 f_view = fresnelSchlick(n_dot_v, f0);
                const math::Vec3 kd = (math::Vec3{1.0f, 1.0f, 1.0f} - f_view) * (1.0f - metallic);
                const math::Vec3 area_diff = kd.cwiseProduct(albedo).cwiseProduct(diff_irradiance) * (1.0f / math::kPi);

                math::Vec3 area_spec = math::Vec3::zero();
                const float n_dot_ls = std::max(n.dot(l_spec), 0.0f);
                if (n_dot_ls > 0.0f && emit_spec > 0.0f) {
                    const float eff_radius = 0.5f * (lgt.half_width + lgt.half_height);
                    const float alpha = roughness * roughness;
                    const float alpha_prime = std::clamp(alpha + eff_radius / (2.0f * rep_dist), alpha, 1.0f);
                    const float spec_rough = std::sqrt(alpha_prime);
                    const float energy_norm = (alpha * alpha) / std::max(alpha_prime * alpha_prime, 1.0e-5f);
                    const float spec_atten = evaluateDistanceAttenuation(
                        rep_dist, lgt.range, 2.0f, 0.12f, scene::LightFalloffMode::InverseSquareWindowed);

                    const math::Vec3 h = safeNormalize(v + l_spec, n);
                    const float ndf = distributionGgx(std::max(n.dot(h), 0.0f), spec_rough) * energy_norm;
                    const float geom = geometrySchlickGgx(n_dot_v, k_direct) * geometrySchlickGgx(n_dot_ls, k_direct);
                    const math::Vec3 f_spec = fresnelSchlick(std::max(h.dot(v), 0.0f), f0);
                    area_spec = f_spec
                                    .cwiseProduct(lgt.radiance) *
                                (((ndf * geom) / std::max(4.0f * n_dot_v * n_dot_ls, 1.0e-4f)) *
                                 n_dot_ls * emit_spec * spec_atten * lgt.area_flux_scale);
                }

                lo += (area_diff + area_spec) * vis;
            }
        }

        // Environment & Split-Sum Image-Based Lighting (IBL).
        const auto& env = sl.environment;
        if (env.enabled && env.intensity > 1.0e-5f) {
            if (env.has_hdri && env.hdri_asset != nullptr) {
                const math::Vec3 n_rot = rotateAroundY(n, env.cos_rotation, env.sin_rotation);
                const math::Vec3 r_world = (n * (2.0f * n.dot(v)) - v).normalized();
                const math::Vec3 r_rot = rotateAroundY(r_world, env.cos_rotation, env.sin_rotation);

                const math::Vec3 f_ibl = fresnelSchlickRoughness(n_dot_v, f0, roughness);
                const math::Vec3 kd_ibl = (math::Vec3{1.0f, 1.0f, 1.0f} - f_ibl) * (1.0f - metallic);

                const math::Vec3 sh_irr =
                    env.hdri_asset->irradiance_sh.evaluateIrradiance(n_rot) *
                    (env.intensity * env.diffuse_ibl_scale);
                const math::Vec3 diffuse_ibl = kd_ibl.cwiseProduct(albedo).cwiseProduct(sh_irr);

                const float lod = roughness * static_cast<float>(scene::PrefilteredEnvCubemap::kMipCount - 1u);
                const math::Vec3 prefiltered =
                    env.hdri_asset->prefiltered_cubemap.sampleLod(r_rot, lod) *
                    (env.intensity * env.specular_ibl_scale);
                const math::Vec2 brdf = envBrdfApprox(roughness, n_dot_v);
                const math::Vec3 specular_ibl = prefiltered.cwiseProduct(
                    f0 * brdf.x + math::Vec3{brdf.y, brdf.y, brdf.y});

                ambient_term = (diffuse_ibl + specular_ibl) * ao;
            } else {
                const float sky_factor = n.y * 0.5f + 0.5f;
                const math::Vec3 env_irradiance =
                    env.fallback_ambient * ((0.65f + 0.70f * sky_factor) * env.intensity);
                const math::Vec3 f_env = fresnelSchlick(n_dot_v, f0);
                const math::Vec3 kd_env = (math::Vec3{1.0f, 1.0f, 1.0f} - f_env) * (1.0f - metallic);
                const math::Vec3 env_spec =
                    f_env.cwiseProduct(env_irradiance) * ((1.0f - 0.75f * roughness) * env.specular_ibl_scale);
                ambient_term =
                    (kd_env.cwiseProduct(albedo).cwiseProduct(env_irradiance) * env.diffuse_ibl_scale + env_spec) * ao;
            }
        }

        if (env.overflow_irradiance.lengthSquared() > math::kStrictEpsilon) {
            const math::Vec3 kd_ov = (math::Vec3{1.0f, 1.0f, 1.0f} - f0) * (1.0f - metallic);
            ambient_term += kd_ov.cwiseProduct(albedo).cwiseProduct(env.overflow_irradiance) * ao;
        }
    } else {
        // Direct standalone DrawMeshCommand compatibility path (used by low-level device tests).
        const math::Vec3 key_l = safeNormalize(cmd.lighting.direction_to_light, math::Vec3::unitY());
        const math::Vec3 fill_l = math::Vec3{-0.55f, 0.35f, -0.65f}.normalized();
        lo = evalCookTorrance(key_l, cmd.lighting.radiance, roughness, 1.0f) +
             evalCookTorrance(fill_l, cmd.lighting.radiance * 0.22f, roughness, 1.0f);

        const std::uint32_t extra_count = std::min<std::uint32_t>(
            cmd.lighting.extra_light_count,
            static_cast<std::uint32_t>(DirectionalLightUniforms::kMaxExtraLights));
        for (std::uint32_t li = 0; li < extra_count; ++li) {
            const auto& extra = cmd.lighting.extra_lights[li];
            if (extra.radiance.lengthSquared() > math::kStrictEpsilon &&
                extra.direction_to_light.lengthSquared() > math::kStrictEpsilon) {
                lo += evalCookTorrance(extra.direction_to_light.normalized(), extra.radiance, roughness, 1.0f);
            }
        }
        const float sky_factor = n.y * 0.5f + 0.5f;
        const math::Vec3 env_irradiance = cmd.lighting.ambient * (0.65f + 0.70f * sky_factor);
        const math::Vec3 f_env = fresnelSchlick(n_dot_v, f0);
        const math::Vec3 kd_env = (math::Vec3{1.0f, 1.0f, 1.0f} - f_env) * (1.0f - metallic);
        const math::Vec3 env_spec = f_env.cwiseProduct(env_irradiance) * (1.0f - 0.75f * roughness);
        ambient_term = (kd_env.cwiseProduct(albedo).cwiseProduct(env_irradiance) + env_spec) * ao;
    }

    math::Vec3 linear_hdr = ambient_term + lo + s.emissive_radiance;
    if (cmd.selected) {
        linear_hdr = linear_hdr * 0.85f + math::Vec3{0.22f, 0.14f, 0.03f};
    }

    // Reinhard tone mapping + linear-to-sRGB (gamma 2.2) conversion.
    const math::Vec3 mapped{
        linear_hdr.x / (1.0f + std::max(0.0f, linear_hdr.x)),
        linear_hdr.y / (1.0f + std::max(0.0f, linear_hdr.y)),
        linear_hdr.z / (1.0f + std::max(0.0f, linear_hdr.z)),
    };
    constexpr float kInvGamma = 1.0f / 2.2f;
    return math::Vec3{
        std::pow(std::clamp(mapped.x, 0.0f, 1.0f), kInvGamma),
        std::pow(std::clamp(mapped.y, 0.0f, 1.0f), kInvGamma),
        std::pow(std::clamp(mapped.z, 0.0f, 1.0f), kInvGamma),
    };
}

} // namespace finaleffect::rendering
