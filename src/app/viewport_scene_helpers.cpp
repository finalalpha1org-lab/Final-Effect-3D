// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/viewport_scene_helpers.hpp"
#include "finaleffect/math/constants.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace finaleffect::app {

namespace {

math::Vec3 safeNormalize(const math::Vec3& v, const math::Vec3& fallback) noexcept {
    const float len_sq = v.lengthSquared();
    if (len_sq <= 1.0e-10f || !std::isfinite(len_sq)) {
        return fallback;
    }
    return v * (1.0f / std::sqrt(len_sq));
}

math::Vec3 clampColor01(const math::Vec3& c) noexcept {
    return {
        std::clamp(c.x, 0.0f, 1.0f),
        std::clamp(c.y, 0.0f, 1.0f),
        std::clamp(c.z, 0.0f, 1.0f),
    };
}

math::Vec3 mixColor(const math::Vec3& a, const math::Vec3& b, float t) noexcept {
    const float u = std::clamp(t, 0.0f, 1.0f);
    return a * (1.0f - u) + b * u;
}

void appendTriangleWorld(
    scene::MeshData& mesh,
    const math::Vec3& p0,
    const math::Vec3& p1,
    const math::Vec3& p2,
    const math::Vec3& color,
    bool double_sided = false) {
    const math::Vec3 n = safeNormalize((p1 - p0).cross(p2 - p0), {0.0f, 1.0f, 0.0f});
    const math::Vec3 col = clampColor01(color);
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({p0, n, col, {0.0f, 0.0f}});
    mesh.vertices.push_back({p1, n, col, {1.0f, 0.0f}});
    mesh.vertices.push_back({p2, n, col, {0.5f, 1.0f}});
    mesh.indices.insert(mesh.indices.end(), {base + 0, base + 1, base + 2});

    if (double_sided) {
        const auto back = static_cast<std::uint32_t>(mesh.vertices.size());
        const math::Vec3 nb = -n;
        mesh.vertices.push_back({p0, nb, col, {0.0f, 0.0f}});
        mesh.vertices.push_back({p2, nb, col, {0.5f, 1.0f}});
        mesh.vertices.push_back({p1, nb, col, {1.0f, 0.0f}});
        mesh.indices.insert(mesh.indices.end(), {back + 0, back + 1, back + 2});
    }
}

void appendQuadWorld(
    scene::MeshData& mesh,
    const math::Vec3& p0,
    const math::Vec3& p1,
    const math::Vec3& p2,
    const math::Vec3& p3,
    const math::Vec3& color,
    bool double_sided = false) {
    const math::Vec3 n = safeNormalize((p1 - p0).cross(p2 - p0), {0.0f, 1.0f, 0.0f});
    const math::Vec3 col = clampColor01(color);
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({p0, n, col, {0.0f, 0.0f}});
    mesh.vertices.push_back({p1, n, col, {1.0f, 0.0f}});
    mesh.vertices.push_back({p2, n, col, {1.0f, 1.0f}});
    mesh.vertices.push_back({p3, n, col, {0.0f, 1.0f}});
    mesh.indices.insert(mesh.indices.end(), {base + 0, base + 1, base + 2, base + 0, base + 2, base + 3});

    if (double_sided) {
        const auto back = static_cast<std::uint32_t>(mesh.vertices.size());
        const math::Vec3 nb = -n;
        mesh.vertices.push_back({p0, nb, col, {0.0f, 0.0f}});
        mesh.vertices.push_back({p3, nb, col, {0.0f, 1.0f}});
        mesh.vertices.push_back({p2, nb, col, {1.0f, 1.0f}});
        mesh.vertices.push_back({p1, nb, col, {1.0f, 0.0f}});
        mesh.indices.insert(mesh.indices.end(), {back + 0, back + 1, back + 2, back + 0, back + 2, back + 3});
    }
}

// Appends a 4-sided 3D strut between world points `a` and `b` so line work renders identically
// across GL_TRIANGLES GPU backends (OpenGL 3.3 / WebGL 2.0) and CPU software rasterization.
void appendStrutWorld(
    scene::MeshData& mesh,
    const math::Vec3& a,
    const math::Vec3& b,
    float radius,
    const math::Vec3& color) {
    const math::Vec3 delta = b - a;
    const float len = delta.length();
    if (len <= 1.0e-5f || radius <= 1.0e-6f) {
        return;
    }
    const math::Vec3 w = delta * (1.0f / len);
    const math::Vec3 ref = (std::fabs(w.y) < 0.9f) ? math::Vec3{0.0f, 1.0f, 0.0f} : math::Vec3{1.0f, 0.0f, 0.0f};
    const math::Vec3 u = safeNormalize(ref.cross(w), {1.0f, 0.0f, 0.0f}) * radius;
    const math::Vec3 v = safeNormalize(w.cross(u), {0.0f, 1.0f, 0.0f}) * radius;

    const std::array<math::Vec3, 4> a_ring{a + u, a + v, a - u, a - v};
    const std::array<math::Vec3, 4> b_ring{b + u, b + v, b - u, b - v};

    for (std::size_t i = 0; i < 4; ++i) {
        const std::size_t j = (i + 1) % 4;
        const float shade = (i % 2 == 0) ? 1.0f : 0.90f;
        appendQuadWorld(mesh, a_ring[i], a_ring[j], b_ring[j], b_ring[i], color * shade);
    }
}

void appendConeWorld(
    scene::MeshData& mesh,
    const math::Vec3& base_center,
    const math::Vec3& tip,
    float radius,
    std::uint32_t segments,
    const math::Vec3& color) {
    const math::Vec3 axis = tip - base_center;
    const float len = axis.length();
    if (len <= 1.0e-5f || segments < 3) {
        return;
    }
    const math::Vec3 w = axis * (1.0f / len);
    const math::Vec3 ref = (std::fabs(w.y) < 0.9f) ? math::Vec3{0.0f, 1.0f, 0.0f} : math::Vec3{1.0f, 0.0f, 0.0f};
    const math::Vec3 u = safeNormalize(ref.cross(w), {1.0f, 0.0f, 0.0f});
    const math::Vec3 v = safeNormalize(w.cross(u), {0.0f, 1.0f, 0.0f});

    for (std::uint32_t i = 0; i < segments; ++i) {
        const float a0 = (static_cast<float>(i) / static_cast<float>(segments)) * math::kTwoPi;
        const float a1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * math::kTwoPi;
        const math::Vec3 p0 = base_center + (u * std::cos(a0) + v * std::sin(a0)) * radius;
        const math::Vec3 p1 = base_center + (u * std::cos(a1) + v * std::sin(a1)) * radius;
        appendTriangleWorld(mesh, p0, p1, tip, color * ((i % 2 == 0) ? 1.04f : 0.92f));
        appendTriangleWorld(mesh, base_center, p1, p0, color * 0.82f);
    }
}

void appendLocalBox(
    scene::MeshData& mesh,
    const math::Mat4& world,
    const math::Vec3& local_min,
    const math::Vec3& local_max,
    const math::Vec3& color,
    float wire_radius,
    const math::Vec3& wire_color) {
    const math::Vec3 c000 = world.transformPoint({local_min.x, local_min.y, local_min.z});
    const math::Vec3 c100 = world.transformPoint({local_max.x, local_min.y, local_min.z});
    const math::Vec3 c110 = world.transformPoint({local_max.x, local_max.y, local_min.z});
    const math::Vec3 c010 = world.transformPoint({local_min.x, local_max.y, local_min.z});
    const math::Vec3 c001 = world.transformPoint({local_min.x, local_min.y, local_max.z});
    const math::Vec3 c101 = world.transformPoint({local_max.x, local_min.y, local_max.z});
    const math::Vec3 c111 = world.transformPoint({local_max.x, local_max.y, local_max.z});
    const math::Vec3 c011 = world.transformPoint({local_min.x, local_max.y, local_max.z});

    // Check mirrored determinant so negative scale still winds outward CCW!
    const math::Vec3 ex = c100 - c000;
    const math::Vec3 ey = c010 - c000;
    const math::Vec3 ez = c001 - c000;
    const bool flip = ex.cross(ey).dot(ez) < 0.0f;

    auto emitFace = [&](const math::Vec3& a, const math::Vec3& b, const math::Vec3& c, const math::Vec3& d, float s) {
        if (flip) {
            appendQuadWorld(mesh, a, d, c, b, color * s);
        } else {
            appendQuadWorld(mesh, a, b, c, d, color * s);
        }
    };

    emitFace(c001, c101, c111, c011, 1.00f); // +Z rear
    emitFace(c100, c000, c010, c110, 0.92f); // -Z front
    emitFace(c101, c100, c110, c111, 0.96f); // +X right
    emitFace(c000, c001, c011, c010, 0.88f); // -X left
    emitFace(c011, c111, c110, c010, 1.06f); // +Y top
    emitFace(c000, c100, c101, c001, 0.82f); // -Y bottom

    if (wire_radius > 0.0f) {
        appendStrutWorld(mesh, c000, c100, wire_radius, wire_color);
        appendStrutWorld(mesh, c100, c110, wire_radius, wire_color);
        appendStrutWorld(mesh, c110, c010, wire_radius, wire_color);
        appendStrutWorld(mesh, c010, c000, wire_radius, wire_color);

        appendStrutWorld(mesh, c001, c101, wire_radius, wire_color);
        appendStrutWorld(mesh, c101, c111, wire_radius, wire_color);
        appendStrutWorld(mesh, c111, c011, wire_radius, wire_color);
        appendStrutWorld(mesh, c011, c001, wire_radius, wire_color);

        appendStrutWorld(mesh, c000, c001, wire_radius, wire_color);
        appendStrutWorld(mesh, c100, c101, wire_radius, wire_color);
        appendStrutWorld(mesh, c110, c111, wire_radius, wire_color);
        appendStrutWorld(mesh, c010, c011, wire_radius, wire_color);
    }
}

struct OrthonormalCameraPose {
    math::Vec3 origin{};
    math::Vec3 right{1.0f, 0.0f, 0.0f};
    math::Vec3 up{0.0f, 1.0f, 0.0f};
    math::Vec3 back{0.0f, 0.0f, 1.0f};
    float avg_scale{1.0f};
};

OrthonormalCameraPose extractOrthonormalPose(const math::Mat4& world) noexcept {
    OrthonormalCameraPose pose;
    pose.origin = world.transformPoint(math::Vec3::zero());
    const math::Vec3 raw_x{world(0, 0), world(1, 0), world(2, 0)};
    const math::Vec3 raw_y{world(0, 1), world(1, 1), world(2, 1)};
    const math::Vec3 raw_z{world(0, 2), world(1, 2), world(2, 2)};

    const float sx = raw_x.length();
    const float sy = raw_y.length();
    const float sz = raw_z.length();
    pose.avg_scale = std::clamp((sx + sy + sz) / 3.0f, 0.15f, 8.0f);

    pose.back = safeNormalize(raw_z, {0.0f, 0.0f, 1.0f});
    pose.right = safeNormalize(raw_y.cross(pose.back), {1.0f, 0.0f, 0.0f});
    pose.up = safeNormalize(pose.back.cross(pose.right), {0.0f, 1.0f, 0.0f});
    return pose;
}

void buildCameraHelperInto(
    scene::MeshData& mesh,
    const math::Mat4& world,
    const scene::CameraComponent& cam,
    float viewport_aspect,
    bool is_active_camera,
    bool is_selected,
    bool include_frustum) {
    const math::Vec3 body_col = is_selected
                                    ? math::Vec3{0.44f, 0.31f, 0.14f}
                                    : (is_active_camera ? math::Vec3{0.19f, 0.25f, 0.33f}
                                                        : math::Vec3{0.22f, 0.24f, 0.29f});
    const math::Vec3 accent_col = is_selected
                                      ? math::Vec3{1.00f, 0.68f, 0.14f}
                                      : (is_active_camera ? math::Vec3{0.26f, 0.80f, 0.98f}
                                                          : math::Vec3{0.66f, 0.72f, 0.82f});
    const math::Vec3 frustum_col = is_selected
                                       ? math::Vec3{1.00f, 0.74f, 0.22f}
                                       : (is_active_camera ? math::Vec3{0.22f, 0.68f, 0.88f}
                                                           : math::Vec3{0.50f, 0.56f, 0.66f});

    const auto pose = extractOrthonormalPose(world);
    const float wire_r = (is_selected ? 0.0115f : 0.0085f) * pose.avg_scale;

    // 1. 3D Camera Housing Box (local +Z rear of nodal origin) + top cinema reel magazine.
    appendLocalBox(
        mesh,
        world,
        math::Vec3{-0.16f, -0.115f, 0.04f},
        math::Vec3{0.16f, 0.115f, 0.36f},
        body_col,
        wire_r,
        accent_col);
    appendLocalBox(
        mesh,
        world,
        math::Vec3{-0.065f, 0.115f, 0.08f},
        math::Vec3{0.065f, 0.195f, 0.31f},
        body_col * 1.12f,
        wire_r * 0.75f,
        accent_col);

    // 2. Tapered 3D Lens Hood opening along local -Z (viewing direction).
    const math::Vec3 b0 = world.transformPoint({-0.085f, -0.065f, 0.04f});
    const math::Vec3 b1 = world.transformPoint({0.085f, -0.065f, 0.04f});
    const math::Vec3 b2 = world.transformPoint({0.085f, 0.065f, 0.04f});
    const math::Vec3 b3 = world.transformPoint({-0.085f, 0.065f, 0.04f});

    const math::Vec3 f0 = world.transformPoint({-0.21f, -0.145f, -0.22f});
    const math::Vec3 f1 = world.transformPoint({0.21f, -0.145f, -0.22f});
    const math::Vec3 f2 = world.transformPoint({0.21f, 0.145f, -0.22f});
    const math::Vec3 f3 = world.transformPoint({-0.21f, 0.145f, -0.22f});

    appendQuadWorld(mesh, b0, b1, f1, f0, body_col * 0.86f, true);
    appendQuadWorld(mesh, b1, b2, f2, f1, body_col * 0.96f, true);
    appendQuadWorld(mesh, b2, b3, f3, f2, body_col * 1.08f, true);
    appendQuadWorld(mesh, b3, b0, f0, f3, body_col * 0.90f, true);
    // Dark recessed lens aperture plate at z = -0.14.
    const math::Vec3 l0 = world.transformPoint({-0.13f, -0.09f, -0.13f});
    const math::Vec3 l1 = world.transformPoint({0.13f, -0.09f, -0.13f});
    const math::Vec3 l2 = world.transformPoint({0.13f, 0.09f, -0.13f});
    const math::Vec3 l3 = world.transformPoint({-0.13f, 0.09f, -0.13f});
    appendQuadWorld(mesh, l1, l0, l3, l2, math::Vec3{0.08f, 0.11f, 0.16f}, true);

    appendStrutWorld(mesh, f0, f1, wire_r * 1.05f, accent_col);
    appendStrutWorld(mesh, f1, f2, wire_r * 1.05f, accent_col);
    appendStrutWorld(mesh, f2, f3, wire_r * 1.05f, accent_col);
    appendStrutWorld(mesh, f3, f0, wire_r * 1.05f, accent_col);
    appendStrutWorld(mesh, b0, f0, wire_r * 0.85f, accent_col);
    appendStrutWorld(mesh, b1, f1, wire_r * 0.85f, accent_col);
    appendStrutWorld(mesh, b2, f2, wire_r * 0.85f, accent_col);
    appendStrutWorld(mesh, b3, f3, wire_r * 0.85f, accent_col);

    // 3. Blender-style Up-Orientation Triangle (+Y above lens hood).
    // Solid filled when active scene camera; outlined frame when secondary camera.
    const math::Vec3 up_l = world.transformPoint({-0.135f, 0.175f, -0.22f});
    const math::Vec3 up_r = world.transformPoint({0.135f, 0.175f, -0.22f});
    const math::Vec3 up_tip = world.transformPoint({0.0f, 0.315f, -0.22f});
    if (is_active_camera) {
        appendTriangleWorld(mesh, up_l, up_r, up_tip, accent_col, true);
    }
    appendStrutWorld(mesh, up_l, up_r, wire_r, accent_col);
    appendStrutWorld(mesh, up_r, up_tip, wire_r, accent_col);
    appendStrutWorld(mesh, up_tip, up_l, wire_r, accent_col);

    if (!include_frustum) {
        return;
    }

    // 4. Optical -Z Viewing Direction Sightline + Arrowhead + Dynamic Perspective/Orthographic Frustum.
    // Uses orthonormalized camera world orientation so the frustum accurately reflects CameraComponent settings.
    const float safe_aspect = std::clamp(std::isfinite(viewport_aspect) ? viewport_aspect : (16.0f / 9.0f), 0.35f, 4.0f);
    const float d_near = std::clamp(0.24f + 0.18f * std::min(cam.near_plane, 6.0f), 0.24f, 1.35f);
    const float far_clamped = std::clamp(cam.far_plane, cam.near_plane + 0.1f, 1000.0f);
    const float d_far = std::clamp(
        0.58f + 0.20f * std::log2(1.0f + far_clamped),
        d_near + 0.38f,
        2.95f);

    auto camSpacePoint = [&](float x, float y, float neg_z_dist) -> math::Vec3 {
        return pose.origin + pose.right * x + pose.up * y - pose.back * neg_z_dist;
    };

    const math::Vec3 dir_start = camSpacePoint(0.0f, 0.0f, 0.14f);
    const math::Vec3 dir_shaft_end = camSpacePoint(0.0f, 0.0f, 0.56f);
    const math::Vec3 dir_tip = camSpacePoint(0.0f, 0.0f, 0.68f);
    appendStrutWorld(mesh, dir_start, dir_shaft_end, wire_r * 0.90f, accent_col);
    appendConeWorld(mesh, dir_shaft_end, dir_tip, wire_r * 2.8f, 8, accent_col);

    float near_hw = 0.0f;
    float near_hh = 0.0f;
    float far_hw = 0.0f;
    float far_hh = 0.0f;

    if (cam.projection == scene::ProjectionType::Perspective) {
        const float half_fov = std::clamp(0.5f * cam.fov_y_radians, math::degToRad(2.5f), math::degToRad(80.0f));
        const float tan_half = std::tan(half_fov);
        near_hh = d_near * tan_half;
        near_hw = near_hh * safe_aspect;
        far_hh = d_far * tan_half;
        far_hw = far_hh * safe_aspect;
    } else {
        // Orthographic parallel box scaled into clean editor helper proportions while responding live to ortho_height!
        const float oh = std::clamp(cam.orthographic_height * 0.16f, 0.12f, 2.4f);
        near_hh = oh;
        near_hw = oh * safe_aspect;
        far_hh = oh;
        far_hw = oh * safe_aspect;
    }

    const std::array<math::Vec3, 4> near_gate{
        camSpacePoint(-near_hw, -near_hh, d_near),
        camSpacePoint(near_hw, -near_hh, d_near),
        camSpacePoint(near_hw, near_hh, d_near),
        camSpacePoint(-near_hw, near_hh, d_near),
    };
    const std::array<math::Vec3, 4> far_gate{
        camSpacePoint(-far_hw, -far_hh, d_far),
        camSpacePoint(far_hw, -far_hh, d_far),
        camSpacePoint(far_hw, far_hh, d_far),
        camSpacePoint(-far_hw, far_hh, d_far),
    };

    const float frustum_r = is_selected ? 0.0085f : 0.0062f;
    for (std::size_t i = 0; i < 4; ++i) {
        const std::size_t j = (i + 1) % 4;
        appendStrutWorld(mesh, near_gate[i], near_gate[j], frustum_r, frustum_col * 0.92f);
        appendStrutWorld(mesh, far_gate[i], far_gate[j], frustum_r * 1.1f, frustum_col);
        if (cam.projection == scene::ProjectionType::Perspective) {
            appendStrutWorld(mesh, pose.origin, far_gate[i], frustum_r * 0.9f, frustum_col * 0.88f);
        } else {
            appendStrutWorld(mesh, near_gate[i], far_gate[i], frustum_r * 0.9f, frustum_col * 0.88f);
        }
    }
}

void buildDirectionalLightHelperInto(
    scene::MeshData& mesh,
    const math::Mat4& world,
    const scene::DirectionalLightComponent& light,
    bool is_selected,
    bool compact_pick_only) {
    // Guarantee strong visibility even when light.intensity == 0 or light.color is black!
    const math::Vec3 raw_tint = clampColor01(light.color);
    const float lum = std::max({raw_tint.x, raw_tint.y, raw_tint.z});
    const math::Vec3 visible_light_col =
        (lum > 0.25f) ? mixColor({1.0f, 0.88f, 0.42f}, raw_tint, 0.68f)
                      : math::Vec3{0.92f, 0.78f, 0.36f};

    const math::Vec3 core_col = is_selected
                                    ? math::Vec3{1.00f, 0.76f, 0.18f}
                                    : visible_light_col;
    const math::Vec3 ray_col = is_selected
                                   ? math::Vec3{1.00f, 0.68f, 0.14f}
                                   : mixColor(visible_light_col, {1.0f, 0.95f, 0.72f}, 0.30f);

    const auto pose = extractOrthonormalPose(world);
    const float wire_r = (is_selected ? 0.0115f : 0.0085f) * pose.avg_scale;

    // 1. 3D Octahedral Sun Core centered at local (0, 0, 0) transformed by `world` (supports Move/Rotate/Scale).
    const float cr = compact_pick_only ? 0.22f : 0.115f;
    const math::Vec3 px = world.transformPoint({cr, 0.0f, 0.0f});
    const math::Vec3 nx = world.transformPoint({-cr, 0.0f, 0.0f});
    const math::Vec3 py = world.transformPoint({0.0f, cr, 0.0f});
    const math::Vec3 ny = world.transformPoint({0.0f, -cr, 0.0f});
    const math::Vec3 pz = world.transformPoint({0.0f, 0.0f, cr});
    const math::Vec3 nz = world.transformPoint({0.0f, 0.0f, -cr});

    appendTriangleWorld(mesh, pz, px, py, core_col * 1.06f, true);
    appendTriangleWorld(mesh, pz, py, nx, core_col * 0.98f, true);
    appendTriangleWorld(mesh, pz, nx, ny, core_col * 0.88f, true);
    appendTriangleWorld(mesh, pz, ny, px, core_col * 0.94f, true);
    appendTriangleWorld(mesh, nz, py, px, core_col * 1.02f, true);
    appendTriangleWorld(mesh, nz, nx, py, core_col * 0.95f, true);
    appendTriangleWorld(mesh, nz, ny, nx, core_col * 0.85f, true);
    appendTriangleWorld(mesh, nz, px, ny, core_col * 0.90f, true);

    // 2. Emitter Halo Ring in local XY plane + orthogonal XZ ring + 8 Sunburst Corona Spikes.
    constexpr std::uint32_t kRingSegs = 16;
    const float halo_r = 0.21f;
    for (std::uint32_t i = 0; i < kRingSegs; ++i) {
        const float a0 = (static_cast<float>(i) / static_cast<float>(kRingSegs)) * math::kTwoPi;
        const float a1 = (static_cast<float>(i + 1) / static_cast<float>(kRingSegs)) * math::kTwoPi;
        const math::Vec3 xy0 = world.transformPoint({std::cos(a0) * halo_r, std::sin(a0) * halo_r, 0.0f});
        const math::Vec3 xy1 = world.transformPoint({std::cos(a1) * halo_r, std::sin(a1) * halo_r, 0.0f});
        appendStrutWorld(mesh, xy0, xy1, wire_r, ray_col);

        if (!compact_pick_only) {
            const math::Vec3 xz0 = world.transformPoint({std::cos(a0) * (halo_r * 0.78f), 0.0f, std::sin(a0) * (halo_r * 0.78f)});
            const math::Vec3 xz1 = world.transformPoint({std::cos(a1) * (halo_r * 0.78f), 0.0f, std::sin(a1) * (halo_r * 0.78f)});
            appendStrutWorld(mesh, xz0, xz1, wire_r * 0.75f, ray_col * 0.90f);
        }
    }

    if (!compact_pick_only) {
        for (int s = 0; s < 8; ++s) {
            const float ang = (static_cast<float>(s) / 8.0f) * math::kTwoPi;
            const float ca = std::cos(ang);
            const float sa = std::sin(ang);
            const math::Vec3 r_in = world.transformPoint({ca * 0.235f, sa * 0.235f, 0.0f});
            const math::Vec3 r_out = world.transformPoint({ca * 0.355f, sa * 0.355f, 0.0f});
            appendStrutWorld(mesh, r_in, r_out, wire_r * 0.90f, ray_col);
        }
    }

    // 3. Primary -Z Illumination Direction Shaft + Cone Arrowhead + 4 Parallel Directional Rays.
    const float clamped_int = std::clamp(std::isfinite(light.intensity) ? light.intensity : 1.0f, 0.0f, 8.0f);
    const float beam_len = 0.92f + 0.10f * std::sqrt(clamped_int);
    const math::Vec3 shaft_a = pose.origin - pose.back * (0.16f * pose.avg_scale);
    const math::Vec3 shaft_b = pose.origin - pose.back * (beam_len * pose.avg_scale);
    const math::Vec3 tip = pose.origin - pose.back * ((beam_len + 0.20f) * pose.avg_scale);

    appendStrutWorld(
        mesh,
        shaft_a,
        shaft_b,
        compact_pick_only ? (wire_r * 3.2f) : (wire_r * 1.25f),
        ray_col);
    appendConeWorld(
        mesh,
        shaft_b,
        tip,
        (compact_pick_only ? 0.075f : 0.048f) * pose.avg_scale,
        10,
        core_col);

    if (!compact_pick_only) {
        constexpr std::array<std::pair<float, float>, 4> kOffsets{{
            {0.15f, 0.15f},
            {-0.15f, 0.15f},
            {-0.15f, -0.15f},
            {0.15f, -0.15f},
        }};
        for (const auto& [ox, oy] : kOffsets) {
            const math::Vec3 p_start =
                pose.origin + (pose.right * ox + pose.up * oy - pose.back * 0.18f) * pose.avg_scale;
            const math::Vec3 p_end =
                pose.origin + (pose.right * ox + pose.up * oy - pose.back * (beam_len * 0.72f)) * pose.avg_scale;
            appendStrutWorld(mesh, p_start, p_end, wire_r * 0.75f, ray_col * 0.88f);
        }
    }
}

std::optional<scene::RaycastHit> raycastHelperMesh(
    scene::ObjectId obj_id,
    const scene::MeshData& helper_mesh,
    const math::Ray& world_ray,
    float max_distance) noexcept {
    if (helper_mesh.vertices.empty() || helper_mesh.indices.size() < 3) {
        return std::nullopt;
    }

    const math::Ray unit_ray(world_ray.origin, world_ray.direction);
    math::AABB bounds = math::AABB::empty();
    for (const auto& v : helper_mesh.vertices) {
        bounds.expand(v.position);
    }
    const auto box_hit = math::intersectRayAabb(unit_ray, bounds, 0.0f, max_distance);
    if (!box_hit.has_value()) {
        return std::nullopt;
    }

    float closest_t = max_distance;
    std::optional<scene::RaycastHit> best_hit;

    for (std::size_t ti = 0; ti + 2 < helper_mesh.indices.size(); ti += 3) {
        const auto i0 = helper_mesh.indices[ti + 0];
        const auto i1 = helper_mesh.indices[ti + 1];
        const auto i2 = helper_mesh.indices[ti + 2];
        const auto& v0 = helper_mesh.vertices[i0].position;
        const auto& v1 = helper_mesh.vertices[i1].position;
        const auto& v2 = helper_mesh.vertices[i2].position;

        const auto tri_t = math::intersectRayTriangle(unit_ray, v0, v1, v2, false, 1.0e-5f, closest_t);
        if (!tri_t.has_value()) {
            continue;
        }
        closest_t = tri_t.value();
        math::Vec3 nrm = (v1 - v0).cross(v2 - v0).normalized();
        if (nrm.dot(unit_ray.direction) > 0.0f) {
            nrm = -nrm;
        }
        best_hit = scene::RaycastHit{
            .object_id = obj_id,
            .distance = closest_t,
            .world_point = unit_ray.at(closest_t),
            .world_normal = nrm,
            .triangle_index = static_cast<std::uint32_t>(ti / 3),
        };
    }
    return best_hit;
}

} // namespace

bool isObjectVisibleInHierarchy(const scene::Scene& scene, scene::ObjectId id) noexcept {
    scene::ObjectId cur = id;
    std::size_t guard = 0;
    const std::size_t max_depth = scene.objectCount() + 1;
    while (cur.isValid() && guard++ < max_depth) {
        const auto* obj = scene.findObject(cur);
        if (obj == nullptr || !obj->visible) {
            return false;
        }
        cur = obj->parent_id;
    }
    return cur == scene::ObjectId::invalid() && id.isValid();
}

scene::MeshData buildViewportSceneHelpersMesh(
    const scene::Scene& scene,
    const ViewportHelperBuildOptions& options) {
    scene::MeshData combined;

    std::vector<const scene::SceneObject*> helper_objs;
    helper_objs.reserve(scene.objectCount());
    for (const auto& obj : scene.objects()) {
        if (!obj.camera.has_value() && !obj.directional_light.has_value()) {
            continue;
        }
        if (!isObjectVisibleInHierarchy(scene, obj.id)) {
            continue;
        }
        helper_objs.push_back(&obj);
    }
    std::sort(helper_objs.begin(), helper_objs.end(), [](const auto* a, const auto* b) {
        return a->id < b->id;
    });

    for (const auto* obj : helper_objs) {
        auto world_res = scene.computeWorldMatrix(obj->id);
        if (!world_res) {
            continue;
        }
        const bool is_sel = (options.selected_object.isValid() && obj->id == options.selected_object);

        if (obj->camera.has_value() && obj->id != options.hidden_camera_object) {
            buildCameraHelperInto(
                combined,
                world_res.value(),
                obj->camera.value(),
                options.viewport_aspect,
                obj->id == scene.activeCamera(),
                is_sel,
                true);
        }
        if (obj->directional_light.has_value()) {
            buildDirectionalLightHelperInto(
                combined,
                world_res.value(),
                obj->directional_light.value(),
                is_sel,
                false);
        }
    }

    return combined;
}

std::optional<scene::RaycastHit> pickSceneOrViewportHelper(
    const scene::Scene& scene,
    const math::Ray& world_ray,
    bool include_editor_helpers,
    scene::ObjectId hidden_camera_object,
    const scene::RaycastOptions& options) noexcept {
    if (!world_ray.isValid() || options.max_distance <= 0.0f) {
        return std::nullopt;
    }

    // 1. Standard mesh raycast across visible scene geometry.
    auto best_hit = scene::raycastScene(scene, world_ray, options);
    float closest_t = best_hit.has_value() ? best_hit->distance : options.max_distance;

    if (!include_editor_helpers) {
        return best_hit;
    }

    // 2. Compact 3D Camera body/lens/up-triangle & DirectionalLight sun-core/arrow raycast.
    // Frustum rays are intentionally excluded from picking so normal mesh picking is NEVER blocked.
    scene::MeshData pick_proxy;
    for (const auto& obj : scene.objects()) {
        if (!obj.camera.has_value() && !obj.directional_light.has_value()) {
            continue;
        }
        if (options.ignore_invisible && !isObjectVisibleInHierarchy(scene, obj.id)) {
            continue;
        }
        auto world_res = scene.computeWorldMatrix(obj.id);
        if (!world_res) {
            continue;
        }

        pick_proxy.vertices.clear();
        pick_proxy.indices.clear();

        if (obj.camera.has_value() && obj.id != hidden_camera_object) {
            buildCameraHelperInto(
                pick_proxy,
                world_res.value(),
                obj.camera.value(),
                16.0f / 9.0f,
                obj.id == scene.activeCamera(),
                false,
                false /* exclude frustum from pick proxy */);
        }
        if (obj.directional_light.has_value()) {
            buildDirectionalLightHelperInto(
                pick_proxy,
                world_res.value(),
                obj.directional_light.value(),
                false,
                true /* compact sun core + shaft pick proxy */);
        }

        if (const auto h = raycastHelperMesh(obj.id, pick_proxy, world_ray, closest_t);
            h.has_value()) {
            if (!best_hit.has_value() ||
                h->distance < closest_t - math::kDefaultEpsilon ||
                (std::fabs(h->distance - closest_t) <= math::kDefaultEpsilon && h->object_id < best_hit->object_id)) {
                closest_t = h->distance;
                best_hit = h;
            }
        }
    }

    return best_hit;
}

} // namespace finaleffect::app
