// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/transform_gizmo.hpp"
#include "finaleffect/math/constants.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace finaleffect::app {

namespace {

constexpr float kRingRadiusRatio = 0.92f;
constexpr float kShaftStartRatio = 0.14f;
constexpr float kShaftEndRatio = 0.88f;
constexpr float kTipEndRatio = 1.08f;
constexpr float kPlaneMinRatio = 0.24f;
constexpr float kPlaneMaxRatio = 0.48f;

math::Vec3 safeNormalize(const math::Vec3& v, const math::Vec3& fallback) noexcept {
    const float len_sq = v.lengthSquared();
    if (len_sq > 1.0e-10f) {
        return v * (1.0f / std::sqrt(len_sq));
    }
    return fallback;
}

std::array<math::Vec3, 3> extractOrthonormalBasis(const math::Mat4& m) noexcept {
    math::Vec3 x = safeNormalize(m.transformDirection({1.0f, 0.0f, 0.0f}), {1.0f, 0.0f, 0.0f});
    math::Vec3 y_raw = m.transformDirection({0.0f, 1.0f, 0.0f});
    y_raw = y_raw - x * y_raw.dot(x);
    math::Vec3 y = safeNormalize(
        y_raw,
        std::fabs(x.y) < 0.9f ? safeNormalize(math::Vec3{0.0f, 1.0f, 0.0f} - x * x.y, {0.0f, 1.0f, 0.0f})
                              : safeNormalize(math::Vec3{0.0f, 0.0f, 1.0f} - x * x.z, {0.0f, 0.0f, 1.0f}));
    math::Vec3 z = safeNormalize(x.cross(y), {0.0f, 0.0f, 1.0f});
    return {x, y, z};
}

math::Quat quatFromOrthonormalBasis(const std::array<math::Vec3, 3>& b) noexcept {
    const float m00 = b[0].x, m01 = b[1].x, m02 = b[2].x;
    const float m10 = b[0].y, m11 = b[1].y, m12 = b[2].y;
    const float m20 = b[0].z, m21 = b[1].z, m22 = b[2].z;

    const float trace = m00 + m11 + m22;
    math::Quat q;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    } else {
        const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }
    return q.normalized();
}

std::optional<float> intersectRayPlane(
    const math::Ray& ray,
    const math::Vec3& plane_origin,
    const math::Vec3& plane_normal) noexcept {
    const float denom = ray.direction.dot(plane_normal);
    if (std::fabs(denom) < 1.0e-5f) {
        return std::nullopt;
    }
    const float t = (plane_origin - ray.origin).dot(plane_normal) / denom;
    if (t < 0.0f || !std::isfinite(t)) {
        return std::nullopt;
    }
    return t;
}

// Closest distance between a ray R(t) = O + t*D (t >= 0) and a finite 3D segment S(u) = A + u*(B-A) (u in [0,1]).
struct RaySegmentResult {
    float distance{std::numeric_limits<float>::infinity()};
    float ray_t{0.0f};
    float seg_u{0.0f};
};

RaySegmentResult closestRaySegment(
    const math::Ray& ray,
    const math::Vec3& seg_a,
    const math::Vec3& seg_b) noexcept {
    const math::Vec3 u = ray.direction;
    const math::Vec3 v = seg_b - seg_a;
    const math::Vec3 w = ray.origin - seg_a;

    const float a = u.dot(u);
    const float b = u.dot(v);
    const float c = v.dot(v);
    const float d = u.dot(w);
    const float e = v.dot(w);
    const float denom = a * c - b * b;

    float sc = 0.0f;
    float tc = 0.0f;

    if (c < 1.0e-9f) {
        // Degenerate zero-length segment (point): project point onto ray t >= 0.
        sc = (a > 1.0e-9f) ? std::max(0.0f, -d / a) : 0.0f;
        tc = 0.0f;
    } else if (denom < 1.0e-7f) {
        // Ray and segment are parallel: clamp segment endpoint and project onto ray.
        tc = std::clamp(e / c, 0.0f, 1.0f);
        sc = (a > 1.0e-9f) ? std::max(0.0f, (b * tc - d) / a) : 0.0f;
    } else {
        sc = (b * e - c * d) / denom;
        tc = (a * e - b * d) / denom;
        if (sc < 0.0f) {
            sc = 0.0f;
            tc = (c > 1.0e-7f) ? std::clamp(e / c, 0.0f, 1.0f) : 0.0f;
        } else if (tc < 0.0f) {
            tc = 0.0f;
            sc = std::max(0.0f, -d / a);
        } else if (tc > 1.0f) {
            tc = 1.0f;
            sc = std::max(0.0f, (b - d) / a);
        }
    }

    const math::Vec3 diff = w + u * sc - v * tc;
    return RaySegmentResult{
        .distance = diff.length(),
        .ray_t = sc,
        .seg_u = tc,
    };
}

math::Vec3 computeAxisConstraintPlaneNormal(
    const math::Vec3& axis,
    const math::Vec3& view_dir) noexcept {
    // Plane containing `axis` and facing the camera as squarely as possible.
    const math::Vec3 tangent = view_dir.cross(axis);
    if (tangent.lengthSquared() > 1.0e-6f) {
        return safeNormalize(axis.cross(tangent), {0.0f, 1.0f, 0.0f});
    }
    // View direction is nearly collinear with `axis`: choose any perpendicular normal.
    const math::Vec3 ref = (std::fabs(axis.y) < 0.9f) ? math::Vec3{0.0f, 1.0f, 0.0f} : math::Vec3{1.0f, 0.0f, 0.0f};
    return safeNormalize(axis.cross(ref), {0.0f, 0.0f, 1.0f});
}

int axisIndexForHandle(GizmoHandle h) noexcept {
    switch (h) {
        case GizmoHandle::TranslateX:
        case GizmoHandle::RotateX:
        case GizmoHandle::ScaleX:
            return 0;
        case GizmoHandle::TranslateY:
        case GizmoHandle::RotateY:
        case GizmoHandle::ScaleY:
            return 1;
        case GizmoHandle::TranslateZ:
        case GizmoHandle::RotateZ:
        case GizmoHandle::ScaleZ:
            return 2;
        default:
            return -1;
    }
}

math::Vec3 handleColor(GizmoHandle handle, GizmoHandle hovered, GizmoHandle active) noexcept {
    if (handle != GizmoHandle::None && handle == active) {
        return {1.0f, 0.96f, 0.55f};
    }
    if (handle != GizmoHandle::None && handle == hovered) {
        return {1.0f, 0.85f, 0.30f};
    }
    switch (handle) {
        case GizmoHandle::TranslateX:
        case GizmoHandle::RotateX:
        case GizmoHandle::ScaleX:
        case GizmoHandle::TranslateYZ:
            return {0.94f, 0.28f, 0.30f};
        case GizmoHandle::TranslateY:
        case GizmoHandle::RotateY:
        case GizmoHandle::ScaleY:
        case GizmoHandle::TranslateXZ:
            return {0.28f, 0.84f, 0.42f};
        case GizmoHandle::TranslateZ:
        case GizmoHandle::RotateZ:
        case GizmoHandle::ScaleZ:
        case GizmoHandle::TranslateXY:
            return {0.29f, 0.60f, 0.98f};
        case GizmoHandle::ScaleUniform:
            return {0.90f, 0.92f, 0.96f};
        default:
            return {0.8f, 0.8f, 0.8f};
    }
}

void appendQuad(
    scene::MeshData& mesh,
    const math::Vec3& p0,
    const math::Vec3& p1,
    const math::Vec3& p2,
    const math::Vec3& p3,
    const math::Vec3& color,
    bool double_sided = false) {
    const math::Vec3 n = safeNormalize((p1 - p0).cross(p2 - p0), {0.0f, 1.0f, 0.0f});
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({p0, n, color, {0.0f, 0.0f}});
    mesh.vertices.push_back({p1, n, color, {1.0f, 0.0f}});
    mesh.vertices.push_back({p2, n, color, {1.0f, 1.0f}});
    mesh.vertices.push_back({p3, n, color, {0.0f, 1.0f}});
    mesh.indices.insert(mesh.indices.end(), {base + 0, base + 1, base + 2, base + 0, base + 2, base + 3});

    if (double_sided) {
        const auto back = static_cast<std::uint32_t>(mesh.vertices.size());
        const math::Vec3 nb = -n;
        mesh.vertices.push_back({p0, nb, color, {0.0f, 0.0f}});
        mesh.vertices.push_back({p3, nb, color, {0.0f, 1.0f}});
        mesh.vertices.push_back({p2, nb, color, {1.0f, 1.0f}});
        mesh.vertices.push_back({p1, nb, color, {1.0f, 0.0f}});
        mesh.indices.insert(mesh.indices.end(), {back + 0, back + 1, back + 2, back + 0, back + 2, back + 3});
    }
}

void appendBoxAlongBasis(
    scene::MeshData& mesh,
    const math::Vec3& center,
    const math::Vec3& u,
    const math::Vec3& v,
    const math::Vec3& w,
    const math::Vec3& half_extents,
    const math::Vec3& color) {
    const math::Vec3 du = u * half_extents.x;
    const math::Vec3 dv = v * half_extents.y;
    const math::Vec3 dw = w * half_extents.z;

    const math::Vec3 c000 = center - du - dv - dw;
    const math::Vec3 c100 = center + du - dv - dw;
    const math::Vec3 c110 = center + du + dv - dw;
    const math::Vec3 c010 = center - du + dv - dw;
    const math::Vec3 c001 = center - du - dv + dw;
    const math::Vec3 c101 = center + du - dv + dw;
    const math::Vec3 c111 = center + du + dv + dw;
    const math::Vec3 c011 = center - du + dv + dw;

    appendQuad(mesh, c001, c101, c111, c011, color);
    appendQuad(mesh, c100, c000, c010, c110, color * 0.92f);
    appendQuad(mesh, c101, c100, c110, c111, color * 0.96f);
    appendQuad(mesh, c000, c001, c011, c010, color * 0.88f);
    appendQuad(mesh, c011, c111, c110, c010, color * 1.04f);
    appendQuad(mesh, c000, c100, c101, c001, color * 0.84f);
}

void appendCone(
    scene::MeshData& mesh,
    const math::Vec3& base_center,
    const math::Vec3& axis,
    const math::Vec3& perp_u,
    const math::Vec3& perp_v,
    float radius,
    float length,
    const math::Vec3& color) {
    constexpr int kSegs = 10;
    const math::Vec3 apex = base_center + axis * length;
    for (int i = 0; i < kSegs; ++i) {
        const float a0 = (static_cast<float>(i) / static_cast<float>(kSegs)) * math::kTwoPi;
        const float a1 = (static_cast<float>(i + 1) / static_cast<float>(kSegs)) * math::kTwoPi;
        const math::Vec3 r0 = perp_u * std::cos(a0) + perp_v * std::sin(a0);
        const math::Vec3 r1 = perp_u * std::cos(a1) + perp_v * std::sin(a1);
        const math::Vec3 p0 = base_center + r0 * radius;
        const math::Vec3 p1 = base_center + r1 * radius;

        const math::Vec3 n = safeNormalize((p1 - p0).cross(apex - p0), axis);
        const auto start = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({p0, n, color, {0.0f, 0.0f}});
        mesh.vertices.push_back({p1, n, color, {1.0f, 0.0f}});
        mesh.vertices.push_back({apex, n, color, {0.5f, 1.0f}});
        mesh.indices.insert(mesh.indices.end(), {start + 0, start + 1, start + 2});

        // Base cap triangle
        const auto cap = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({base_center, -axis, color * 0.85f, {0.5f, 0.5f}});
        mesh.vertices.push_back({p1, -axis, color * 0.85f, {1.0f, 0.0f}});
        mesh.vertices.push_back({p0, -axis, color * 0.85f, {0.0f, 0.0f}});
        mesh.indices.insert(mesh.indices.end(), {cap + 0, cap + 1, cap + 2});
    }
}

void appendTorusRing(
    scene::MeshData& mesh,
    const math::Vec3& center,
    const math::Vec3& normal_axis,
    const math::Vec3& u,
    const math::Vec3& v,
    float major_radius,
    float minor_radius,
    const math::Vec3& color) {
    constexpr int kMajorSegs = 36;
    constexpr int kMinorSegs = 6;
    for (int i = 0; i < kMajorSegs; ++i) {
        const float t0 = (static_cast<float>(i) / static_cast<float>(kMajorSegs)) * math::kTwoPi;
        const float t1 = (static_cast<float>(i + 1) / static_cast<float>(kMajorSegs)) * math::kTwoPi;
        const math::Vec3 radial0 = u * std::cos(t0) + v * std::sin(t0);
        const math::Vec3 radial1 = u * std::cos(t1) + v * std::sin(t1);
        const math::Vec3 c0 = center + radial0 * major_radius;
        const math::Vec3 c1 = center + radial1 * major_radius;

        for (int j = 0; j < kMinorSegs; ++j) {
            const float p0 = (static_cast<float>(j) / static_cast<float>(kMinorSegs)) * math::kTwoPi;
            const float p1 = (static_cast<float>(j + 1) / static_cast<float>(kMinorSegs)) * math::kTwoPi;

            const math::Vec3 n00 = radial0 * std::cos(p0) + normal_axis * std::sin(p0);
            const math::Vec3 n10 = radial1 * std::cos(p0) + normal_axis * std::sin(p0);
            const math::Vec3 n11 = radial1 * std::cos(p1) + normal_axis * std::sin(p1);
            const math::Vec3 n01 = radial0 * std::cos(p1) + normal_axis * std::sin(p1);

            appendQuad(
                mesh,
                c0 + n00 * minor_radius,
                c1 + n10 * minor_radius,
                c1 + n11 * minor_radius,
                c0 + n01 * minor_radius,
                color);
        }
    }
}

} // namespace

std::string_view gizmoModeName(GizmoMode mode) noexcept {
    switch (mode) {
        case GizmoMode::Translate: return "translate";
        case GizmoMode::Rotate: return "rotate";
        case GizmoMode::Scale: return "scale";
    }
    return "translate";
}

std::string_view gizmoSpaceName(GizmoSpace space) noexcept {
    return (space == GizmoSpace::Local) ? "local" : "world";
}

std::string_view gizmoHandleName(GizmoHandle handle) noexcept {
    switch (handle) {
        case GizmoHandle::None: return "none";
        case GizmoHandle::TranslateX: return "translate_x";
        case GizmoHandle::TranslateY: return "translate_y";
        case GizmoHandle::TranslateZ: return "translate_z";
        case GizmoHandle::TranslateXY: return "translate_xy";
        case GizmoHandle::TranslateXZ: return "translate_xz";
        case GizmoHandle::TranslateYZ: return "translate_yz";
        case GizmoHandle::RotateX: return "rotate_x";
        case GizmoHandle::RotateY: return "rotate_y";
        case GizmoHandle::RotateZ: return "rotate_z";
        case GizmoHandle::ScaleX: return "scale_x";
        case GizmoHandle::ScaleY: return "scale_y";
        case GizmoHandle::ScaleZ: return "scale_z";
        case GizmoHandle::ScaleUniform: return "scale_uniform";
    }
    return "none";
}

GizmoFrame TransformGizmo::computeFrame(
    const math::Mat4& target_world_matrix,
    const math::Mat4& camera_world_matrix,
    const scene::CameraComponent& camera,
    GizmoMode mode,
    GizmoSpace space) noexcept {
    GizmoFrame frame;
    frame.origin = target_world_matrix.transformPoint(math::Vec3::zero());

    // Scale handles always align with local object axes because Transform::scale is local TRS.
    if (space == GizmoSpace::Local || mode == GizmoMode::Scale) {
        frame.axes = extractOrthonormalBasis(target_world_matrix);
    } else {
        frame.axes = {
            math::Vec3{1.0f, 0.0f, 0.0f},
            math::Vec3{0.0f, 1.0f, 0.0f},
            math::Vec3{0.0f, 0.0f, 1.0f},
        };
    }

    const math::Vec3 cam_pos = camera_world_matrix.transformPoint(math::Vec3::zero());
    if (camera.projection == scene::ProjectionType::Orthographic) {
        frame.scale = std::max(0.05f, camera.orthographic_height * 0.16f);
    } else {
        const float dist = std::max(0.25f, (frame.origin - cam_pos).length());
        const float half_tan = std::tan(std::clamp(camera.fov_y_radians, 0.1f, 2.6f) * 0.5f);
        frame.scale = std::max(0.05f, dist * half_tan * 0.30f);
    }
    return frame;
}

std::optional<GizmoPickResult> TransformGizmo::pickHandle(
    GizmoMode mode,
    const GizmoFrame& frame,
    const math::Ray& world_ray) noexcept {
    if (!world_ray.isValid() || frame.scale <= 0.0f) {
        return std::nullopt;
    }

    const float s = frame.scale;

    if (mode == GizmoMode::Translate) {
        // 1. Check XY, XZ, YZ planar handles first.
        struct PlaneCandidate {
            GizmoHandle handle;
            std::size_t u_idx;
            std::size_t v_idx;
            std::size_t n_idx;
        };
        constexpr std::array<PlaneCandidate, 3> planes{{
            {GizmoHandle::TranslateXY, 0, 1, 2},
            {GizmoHandle::TranslateXZ, 0, 2, 1},
            {GizmoHandle::TranslateYZ, 1, 2, 0},
        }};

        std::optional<GizmoPickResult> best_plane;
        for (const auto& pc : planes) {
            const auto t_opt = intersectRayPlane(world_ray, frame.origin, frame.axes[pc.n_idx]);
            if (!t_opt.has_value()) {
                continue;
            }
            const math::Vec3 rel = world_ray.at(t_opt.value()) - frame.origin;
            const float u = rel.dot(frame.axes[pc.u_idx]);
            const float v = rel.dot(frame.axes[pc.v_idx]);
            if (u >= kPlaneMinRatio * s && u <= kPlaneMaxRatio * s &&
                v >= kPlaneMinRatio * s && v <= kPlaneMaxRatio * s) {
                if (!best_plane.has_value() || t_opt.value() < best_plane->ray_t) {
                    best_plane = GizmoPickResult{
                        .handle = pc.handle,
                        .ray_t = t_opt.value(),
                        .hit_point = world_ray.at(t_opt.value()),
                    };
                }
            }
        }
        if (best_plane.has_value()) {
            return best_plane;
        }

        // 2. Check X, Y, Z axis shafts + cones.
        constexpr std::array<GizmoHandle, 3> axis_handles{
            GizmoHandle::TranslateX,
            GizmoHandle::TranslateY,
            GizmoHandle::TranslateZ,
        };
        const float pick_radius = 0.095f * s;
        float best_dist = pick_radius;
        std::optional<GizmoPickResult> best_axis;

        for (std::size_t i = 0; i < 3; ++i) {
            const math::Vec3 seg_a = frame.origin + frame.axes[i] * (kShaftStartRatio * s);
            const math::Vec3 seg_b = frame.origin + frame.axes[i] * (kTipEndRatio * s);
            const auto res = closestRaySegment(world_ray, seg_a, seg_b);
            if (res.distance <= best_dist) {
                best_dist = res.distance;
                best_axis = GizmoPickResult{
                    .handle = axis_handles[i],
                    .ray_t = res.ray_t,
                    .hit_point = world_ray.at(res.ray_t),
                };
            }
        }
        return best_axis;
    }

    if (mode == GizmoMode::Rotate) {
        constexpr std::array<GizmoHandle, 3> ring_handles{
            GizmoHandle::RotateX,
            GizmoHandle::RotateY,
            GizmoHandle::RotateZ,
        };
        const float ring_r = kRingRadiusRatio * s;
        const float tube_tol = 0.105f * s;
        float best_err = tube_tol;
        std::optional<GizmoPickResult> best_ring;

        for (std::size_t i = 0; i < 3; ++i) {
            const math::Vec3& n = frame.axes[i];
            const math::Vec3& u = frame.axes[(i + 1) % 3];
            const math::Vec3& v = frame.axes[(i + 2) % 3];

            // Sample 36 chord segments on the 3D circle so both face-on and grazing views pick accurately.
            constexpr int kSegs = 36;
            for (int k = 0; k < kSegs; ++k) {
                const float a0 = (static_cast<float>(k) / static_cast<float>(kSegs)) * math::kTwoPi;
                const float a1 = (static_cast<float>(k + 1) / static_cast<float>(kSegs)) * math::kTwoPi;
                const math::Vec3 p0 = frame.origin + (u * std::cos(a0) + v * std::sin(a0)) * ring_r;
                const math::Vec3 p1 = frame.origin + (u * std::cos(a1) + v * std::sin(a1)) * ring_r;
                const auto seg = closestRaySegment(world_ray, p0, p1);
                if (seg.distance <= best_err) {
                    best_err = seg.distance;
                    best_ring = GizmoPickResult{
                        .handle = ring_handles[i],
                        .ray_t = seg.ray_t,
                        .hit_point = world_ray.at(seg.ray_t),
                    };
                }
            }
            (void)n;
        }
        return best_ring;
    }

    // Scale mode: center uniform cube first, then X/Y/Z shafts.
    {
        const auto center_res = closestRaySegment(world_ray, frame.origin, frame.origin);
        if (center_res.distance <= 0.16f * s) {
            return GizmoPickResult{
                .handle = GizmoHandle::ScaleUniform,
                .ray_t = center_res.ray_t,
                .hit_point = world_ray.at(center_res.ray_t),
            };
        }

        constexpr std::array<GizmoHandle, 3> scale_handles{
            GizmoHandle::ScaleX,
            GizmoHandle::ScaleY,
            GizmoHandle::ScaleZ,
        };
        const float pick_radius = 0.095f * s;
        float best_dist = pick_radius;
        std::optional<GizmoPickResult> best_scale;

        for (std::size_t i = 0; i < 3; ++i) {
            const math::Vec3 seg_a = frame.origin + frame.axes[i] * (0.17f * s);
            const math::Vec3 seg_b = frame.origin + frame.axes[i] * (1.04f * s);
            const auto res = closestRaySegment(world_ray, seg_a, seg_b);
            if (res.distance <= best_dist) {
                best_dist = res.distance;
                best_scale = GizmoPickResult{
                    .handle = scale_handles[i],
                    .ray_t = res.ray_t,
                    .hit_point = world_ray.at(res.ray_t),
                };
            }
        }
        return best_scale;
    }
}

core::Status TransformGizmo::beginDrag(
    GizmoHandle handle,
    const GizmoFrame& frame,
    const math::Ray& initial_world_ray,
    const math::Mat4& camera_world_matrix,
    const math::Transform& initial_local_transform,
    const math::Mat4& parent_world_matrix) {
    if (handle == GizmoHandle::None) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Cannot begin gizmo drag with GizmoHandle::None");
    }
    if (!initial_world_ray.isValid() || !initial_local_transform.isValid()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid initial ray or transform for gizmo drag");
    }

    const auto inv_parent = parent_world_matrix.inverted();
    if (!inv_parent.has_value()) {
        return core::Status::fail(core::ErrorCode::ValidationError, "Singular parent world matrix in gizmo drag");
    }

    active_handle_ = handle;
    start_frame_ = frame;
    initial_local_transform_ = initial_local_transform;
    parent_world_matrix_ = parent_world_matrix;
    inv_parent_world_matrix_ = inv_parent.value();
    parent_world_rotation_ = quatFromOrthonormalBasis(extractOrthonormalBasis(parent_world_matrix));

    cam_right_ = safeNormalize(camera_world_matrix.transformDirection({1.0f, 0.0f, 0.0f}), {1.0f, 0.0f, 0.0f});
    cam_up_ = safeNormalize(camera_world_matrix.transformDirection({0.0f, 1.0f, 0.0f}), {0.0f, 1.0f, 0.0f});
    cam_forward_ = safeNormalize(camera_world_matrix.transformDirection({0.0f, 0.0f, -1.0f}), {0.0f, 0.0f, -1.0f});

    const int axis_idx = axisIndexForHandle(handle);

    if (handle == GizmoHandle::TranslateX || handle == GizmoHandle::TranslateY || handle == GizmoHandle::TranslateZ ||
        handle == GizmoHandle::ScaleX || handle == GizmoHandle::ScaleY || handle == GizmoHandle::ScaleZ) {
        const math::Vec3& axis = frame.axes[static_cast<std::size_t>(axis_idx)];
        constraint_plane_normal_ = computeAxisConstraintPlaneNormal(axis, initial_world_ray.direction);
        const auto t_opt = intersectRayPlane(initial_world_ray, frame.origin, constraint_plane_normal_);
        if (!t_opt.has_value()) {
            return core::Status::fail(core::ErrorCode::InvalidState, "Initial ray parallel to axis constraint plane");
        }
        start_plane_hit_ = initial_world_ray.at(t_opt.value());
        start_axis_param_ = (start_plane_hit_ - frame.origin).dot(axis);
    } else if (handle == GizmoHandle::TranslateXY || handle == GizmoHandle::TranslateXZ || handle == GizmoHandle::TranslateYZ) {
        const int n_idx = (handle == GizmoHandle::TranslateXY) ? 2 : ((handle == GizmoHandle::TranslateXZ) ? 1 : 0);
        constraint_plane_normal_ = frame.axes[static_cast<std::size_t>(n_idx)];
        const auto t_opt = intersectRayPlane(initial_world_ray, frame.origin, constraint_plane_normal_);
        if (!t_opt.has_value()) {
            return core::Status::fail(core::ErrorCode::InvalidState, "Initial ray parallel to planar translation handle");
        }
        start_plane_hit_ = initial_world_ray.at(t_opt.value());
    } else if (handle == GizmoHandle::RotateX || handle == GizmoHandle::RotateY || handle == GizmoHandle::RotateZ) {
        const math::Vec3& axis = frame.axes[static_cast<std::size_t>(axis_idx)];
        constraint_plane_normal_ = axis;
        if (std::fabs(initial_world_ray.direction.dot(axis)) >= 0.08f) {
            const auto t_opt = intersectRayPlane(initial_world_ray, frame.origin, axis);
            if (!t_opt.has_value()) {
                return core::Status::fail(core::ErrorCode::InvalidState, "Failed to intersect rotation plane");
            }
            const math::Vec3 radial = initial_world_ray.at(t_opt.value()) - frame.origin;
            const math::Vec3 in_plane = radial - axis * radial.dot(axis);
            start_ring_dir_ = safeNormalize(in_plane, frame.axes[static_cast<std::size_t>((axis_idx + 1) % 3)]);
        } else {
            // Grazing view fallback: construct tangent plane facing camera.
            constraint_plane_normal_ = -cam_forward_;
            const auto t_opt = intersectRayPlane(initial_world_ray, frame.origin, constraint_plane_normal_);
            start_plane_hit_ = t_opt.has_value() ? initial_world_ray.at(t_opt.value()) : frame.origin;
            start_ring_dir_ = frame.axes[static_cast<std::size_t>((axis_idx + 1) % 3)];
        }
        start_ring_tangent_ = safeNormalize(axis.cross(start_ring_dir_), frame.axes[static_cast<std::size_t>((axis_idx + 2) % 3)]);
    } else if (handle == GizmoHandle::ScaleUniform) {
        constraint_plane_normal_ = -cam_forward_;
        const auto t_opt = intersectRayPlane(initial_world_ray, frame.origin, constraint_plane_normal_);
        if (!t_opt.has_value()) {
            return core::Status::fail(core::ErrorCode::InvalidState, "Failed to intersect uniform scale plane");
        }
        start_plane_hit_ = initial_world_ray.at(t_opt.value());
    }

    last_valid_output_ = GizmoDragOutput{
        .local_transform = initial_local_transform_,
        .world_translation_delta = math::Vec3::zero(),
        .world_rotation_delta = math::Quat::identity(),
        .local_scale_factor = {1.0f, 1.0f, 1.0f},
    };
    dragging_ = true;
    return core::Status::ok();
}

core::Result<GizmoDragOutput> TransformGizmo::updateDrag(const math::Ray& current_world_ray) noexcept {
    if (!dragging_ || active_handle_ == GizmoHandle::None) {
        return core::Result<GizmoDragOutput>::fail(core::ErrorCode::InvalidState, "TransformGizmo is not currently dragging");
    }
    if (!current_world_ray.isValid()) {
        return core::Result<GizmoDragOutput>::fail(core::ErrorCode::InvalidArgument, "Current drag ray is invalid");
    }

    GizmoDragOutput out = last_valid_output_;
    out.local_transform = initial_local_transform_;
    out.world_translation_delta = math::Vec3::zero();
    out.world_rotation_delta = math::Quat::identity();
    out.local_scale_factor = {1.0f, 1.0f, 1.0f};

    const int axis_idx = axisIndexForHandle(active_handle_);
    const float safe_scale = std::max(0.05f, start_frame_.scale);

    if (active_handle_ == GizmoHandle::TranslateX ||
        active_handle_ == GizmoHandle::TranslateY ||
        active_handle_ == GizmoHandle::TranslateZ) {
        const auto t_opt = intersectRayPlane(current_world_ray, start_frame_.origin, constraint_plane_normal_);
        if (!t_opt.has_value()) {
            return core::Result<GizmoDragOutput>::ok(last_valid_output_);
        }
        const math::Vec3& axis = start_frame_.axes[static_cast<std::size_t>(axis_idx)];
        const float cur_param = (current_world_ray.at(t_opt.value()) - start_frame_.origin).dot(axis);
        const math::Vec3 world_delta = axis * (cur_param - start_axis_param_);
        const math::Vec3 new_world_pos = start_frame_.origin + world_delta;

        out.local_transform.position = inv_parent_world_matrix_.transformPoint(new_world_pos);
        out.world_translation_delta = world_delta;
    } else if (active_handle_ == GizmoHandle::TranslateXY ||
               active_handle_ == GizmoHandle::TranslateXZ ||
               active_handle_ == GizmoHandle::TranslateYZ) {
        const auto t_opt = intersectRayPlane(current_world_ray, start_frame_.origin, constraint_plane_normal_);
        if (!t_opt.has_value()) {
            return core::Result<GizmoDragOutput>::ok(last_valid_output_);
        }
        const math::Vec3 cur_hit = current_world_ray.at(t_opt.value());
        const math::Vec3 raw_delta = cur_hit - start_plane_hit_;
        // Project onto plane strictly so numerical drift along normal is zero.
        const math::Vec3 world_delta = raw_delta - constraint_plane_normal_ * raw_delta.dot(constraint_plane_normal_);
        const math::Vec3 new_world_pos = start_frame_.origin + world_delta;

        out.local_transform.position = inv_parent_world_matrix_.transformPoint(new_world_pos);
        out.world_translation_delta = world_delta;
    } else if (active_handle_ == GizmoHandle::RotateX ||
               active_handle_ == GizmoHandle::RotateY ||
               active_handle_ == GizmoHandle::RotateZ) {
        const math::Vec3& world_axis = start_frame_.axes[static_cast<std::size_t>(axis_idx)];
        float angle_rad = 0.0f;

        if (std::fabs(current_world_ray.direction.dot(world_axis)) >= 0.08f &&
            constraint_plane_normal_.approxEqual(world_axis, 1.0e-3f)) {
            const auto t_opt = intersectRayPlane(current_world_ray, start_frame_.origin, world_axis);
            if (!t_opt.has_value()) {
                return core::Result<GizmoDragOutput>::ok(last_valid_output_);
            }
            const math::Vec3 radial = current_world_ray.at(t_opt.value()) - start_frame_.origin;
            const math::Vec3 in_plane = radial - world_axis * radial.dot(world_axis);
            if (in_plane.lengthSquared() < 1.0e-8f) {
                return core::Result<GizmoDragOutput>::ok(last_valid_output_);
            }
            const math::Vec3 cur_dir = in_plane.normalized();
            const float sin_a = world_axis.dot(start_ring_dir_.cross(cur_dir));
            const float cos_a = std::clamp(start_ring_dir_.dot(cur_dir), -1.0f, 1.0f);
            angle_rad = std::atan2(sin_a, cos_a);
        } else {
            // Edge-on grazing tangent projection.
            const auto t_opt = intersectRayPlane(current_world_ray, start_frame_.origin, -cam_forward_);
            if (!t_opt.has_value()) {
                return core::Result<GizmoDragOutput>::ok(last_valid_output_);
            }
            const math::Vec3 delta = current_world_ray.at(t_opt.value()) - start_plane_hit_;
            angle_rad = delta.dot(start_ring_tangent_) / (kRingRadiusRatio * safe_scale);
        }

        const math::Quat dq_world = math::Quat::fromAxisAngle(world_axis, angle_rad);
        const math::Quat init_world_rot = (parent_world_rotation_ * initial_local_transform_.rotation).normalized();
        const math::Quat new_world_rot = (dq_world * init_world_rot).normalized();
        out.local_transform.rotation = (parent_world_rotation_.conjugate() * new_world_rot).normalized();
        out.world_rotation_delta = dq_world;
    } else if (active_handle_ == GizmoHandle::ScaleX ||
               active_handle_ == GizmoHandle::ScaleY ||
               active_handle_ == GizmoHandle::ScaleZ) {
        const auto t_opt = intersectRayPlane(current_world_ray, start_frame_.origin, constraint_plane_normal_);
        if (!t_opt.has_value()) {
            return core::Result<GizmoDragOutput>::ok(last_valid_output_);
        }
        const math::Vec3& axis = start_frame_.axes[static_cast<std::size_t>(axis_idx)];
        const float cur_param = (current_world_ray.at(t_opt.value()) - start_frame_.origin).dot(axis);
        const float factor = std::clamp(1.0f + (cur_param - start_axis_param_) / safe_scale, 0.05f, 100.0f);

        math::Vec3 factors{1.0f, 1.0f, 1.0f};
        if (axis_idx == 0) factors.x = factor;
        else if (axis_idx == 1) factors.y = factor;
        else factors.z = factor;

        out.local_transform.scale = initial_local_transform_.scale.cwiseProduct(factors);
        out.local_scale_factor = factors;
    } else if (active_handle_ == GizmoHandle::ScaleUniform) {
        const auto t_opt = intersectRayPlane(current_world_ray, start_frame_.origin, constraint_plane_normal_);
        if (!t_opt.has_value()) {
            return core::Result<GizmoDragOutput>::ok(last_valid_output_);
        }
        const math::Vec3 delta = current_world_ray.at(t_opt.value()) - start_plane_hit_;
        const math::Vec3 diag = safeNormalize(cam_right_ + cam_up_, cam_right_);
        const float factor = std::clamp(1.0f + delta.dot(diag) / safe_scale, 0.05f, 100.0f);

        out.local_transform.scale = initial_local_transform_.scale * factor;
        out.local_scale_factor = {factor, factor, factor};
    }

    if (!out.local_transform.isValid()) {
        return core::Result<GizmoDragOutput>::ok(last_valid_output_);
    }

    last_valid_output_ = out;
    return core::Result<GizmoDragOutput>::ok(out);
}

void TransformGizmo::endDrag() noexcept {
    dragging_ = false;
    active_handle_ = GizmoHandle::None;
}

math::Transform TransformGizmo::cancelDrag() noexcept {
    dragging_ = false;
    active_handle_ = GizmoHandle::None;
    return initial_local_transform_;
}

scene::MeshData TransformGizmo::buildRenderMesh(
    GizmoMode mode,
    const GizmoFrame& frame,
    GizmoHandle hovered_handle,
    GizmoHandle active_handle) {
    scene::MeshData mesh;
    const float s = std::max(0.01f, frame.scale);

    if (mode == GizmoMode::Translate) {
        constexpr std::array<GizmoHandle, 3> axis_h{
            GizmoHandle::TranslateX,
            GizmoHandle::TranslateY,
            GizmoHandle::TranslateZ,
        };
        for (std::size_t i = 0; i < 3; ++i) {
            const math::Vec3& a = frame.axes[i];
            const math::Vec3& u = frame.axes[(i + 1) % 3];
            const math::Vec3& v = frame.axes[(i + 2) % 3];
            const math::Vec3 col = handleColor(axis_h[i], hovered_handle, active_handle);

            const float shaft_half = 0.5f * (kShaftEndRatio - kShaftStartRatio) * s;
            const math::Vec3 shaft_center = frame.origin + a * ((kShaftStartRatio + kShaftEndRatio) * 0.5f * s);
            appendBoxAlongBasis(mesh, shaft_center, a, u, v, {shaft_half, 0.022f * s, 0.022f * s}, col);
            appendCone(
                mesh,
                frame.origin + a * (kShaftEndRatio * s),
                a, u, v,
                0.068f * s,
                (kTipEndRatio - kShaftEndRatio) * s,
                col);
        }

        // XY, XZ, YZ plane quads
        struct PlaneDraw {
            GizmoHandle h;
            std::size_t u_i;
            std::size_t v_i;
        };
        constexpr std::array<PlaneDraw, 3> planes{{
            {GizmoHandle::TranslateXY, 0, 1},
            {GizmoHandle::TranslateXZ, 0, 2},
            {GizmoHandle::TranslateYZ, 1, 2},
        }};
        for (const auto& pd : planes) {
            const math::Vec3 col = handleColor(pd.h, hovered_handle, active_handle);
            const math::Vec3& u = frame.axes[pd.u_i];
            const math::Vec3& v = frame.axes[pd.v_i];
            const math::Vec3 p0 = frame.origin + u * (kPlaneMinRatio * s) + v * (kPlaneMinRatio * s);
            const math::Vec3 p1 = frame.origin + u * (kPlaneMaxRatio * s) + v * (kPlaneMinRatio * s);
            const math::Vec3 p2 = frame.origin + u * (kPlaneMaxRatio * s) + v * (kPlaneMaxRatio * s);
            const math::Vec3 p3 = frame.origin + u * (kPlaneMinRatio * s) + v * (kPlaneMaxRatio * s);
            appendQuad(mesh, p0, p1, p2, p3, col, true);
        }
    } else if (mode == GizmoMode::Rotate) {
        constexpr std::array<GizmoHandle, 3> ring_h{
            GizmoHandle::RotateX,
            GizmoHandle::RotateY,
            GizmoHandle::RotateZ,
        };
        for (std::size_t i = 0; i < 3; ++i) {
            const math::Vec3 col = handleColor(ring_h[i], hovered_handle, active_handle);
            appendTorusRing(
                mesh,
                frame.origin,
                frame.axes[i],
                frame.axes[(i + 1) % 3],
                frame.axes[(i + 2) % 3],
                kRingRadiusRatio * s,
                0.024f * s,
                col);
        }
    } else {
        // Scale: center uniform cube + 3 axis shafts + 3 tip cubes
        const math::Vec3 center_col = handleColor(GizmoHandle::ScaleUniform, hovered_handle, active_handle);
        appendBoxAlongBasis(
            mesh,
            frame.origin,
            frame.axes[0],
            frame.axes[1],
            frame.axes[2],
            {0.095f * s, 0.095f * s, 0.095f * s},
            center_col);

        constexpr std::array<GizmoHandle, 3> scl_h{
            GizmoHandle::ScaleX,
            GizmoHandle::ScaleY,
            GizmoHandle::ScaleZ,
        };
        for (std::size_t i = 0; i < 3; ++i) {
            const math::Vec3& a = frame.axes[i];
            const math::Vec3& u = frame.axes[(i + 1) % 3];
            const math::Vec3& v = frame.axes[(i + 2) % 3];
            const math::Vec3 col = handleColor(scl_h[i], hovered_handle, active_handle);

            const float shaft_half = 0.5f * (0.90f - 0.16f) * s;
            const math::Vec3 shaft_center = frame.origin + a * ((0.16f + 0.90f) * 0.5f * s);
            appendBoxAlongBasis(mesh, shaft_center, a, u, v, {shaft_half, 0.022f * s, 0.022f * s}, col);
            appendBoxAlongBasis(
                mesh,
                frame.origin + a * (0.96f * s),
                a, u, v,
                {0.065f * s, 0.065f * s, 0.065f * s},
                col);
        }
    }

    return mesh;
}

} // namespace finaleffect::app
