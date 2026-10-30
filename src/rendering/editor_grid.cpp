// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/rendering/editor_grid.hpp"

#include <algorithm>
#include <cmath>

namespace finaleffect::rendering {

namespace {

constexpr float kGridDepthBias = 2.5e-5f;

float smoothstepScalar(float edge0, float edge1, float x) noexcept {
    if (edge1 <= edge0) {
        return (x >= edge1) ? 1.0f : 0.0f;
    }
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float fractFloor(float v) noexcept {
    return v - std::floor(v);
}

math::Vec3 mixColor(const math::Vec3& a, const math::Vec3& b, float t) noexcept {
    const float s = std::clamp(t, 0.0f, 1.0f);
    return a * (1.0f - s) + b * s;
}

float evalGridCascadeLine(
    float delta_coord,
    float phase_coord,
    float step,
    float footprint,
    float half_width_px) noexcept {
    if (step <= 1.0e-6f) {
        return 0.0f;
    }
    const float nyquist = 1.0f - smoothstepScalar(0.22f * step, 0.48f * step, footprint);
    if (nyquist <= 1.0e-4f) {
        return 0.0f;
    }
    const float u = (delta_coord + phase_coord) / step;
    const float dist_world = std::fabs(fractFloor(u + 0.5f) - 0.5f) * step;
    const float dist_px = dist_world / std::max(footprint, 1.0e-6f);
    const float cov = 1.0f - smoothstepScalar(half_width_px - 0.55f, half_width_px + 0.55f, dist_px);
    return cov * nyquist;
}

} // namespace

std::optional<EditorGridCommand> buildEditorGridCommand(
    const math::Mat4& view_matrix,
    const math::Mat4& proj_matrix,
    const math::Vec3& camera_pos,
    double camera_world_x_precise,
    double camera_world_z_precise,
    math::ClipSpaceDepth clip_space_depth,
    const ViewportGridOptions& options) noexcept {
    if (!options.enabled || options.opacity <= 1.0e-4f) {
        return std::nullopt;
    }

    const float p00 = std::fabs(proj_matrix(0, 0));
    const float p11 = std::fabs(proj_matrix(1, 1));
    if (p00 <= 1.0e-6f || p11 <= 1.0e-6f) {
        return std::nullopt;
    }

    EditorGridCommand cmd{};
    cmd.camera_position = camera_pos;
    cmd.camera_right = math::Vec3{view_matrix(0, 0), view_matrix(0, 1), view_matrix(0, 2)}.normalized();
    cmd.camera_up = math::Vec3{view_matrix(1, 0), view_matrix(1, 1), view_matrix(1, 2)}.normalized();
    cmd.camera_forward = math::Vec3{-view_matrix(2, 0), -view_matrix(2, 1), -view_matrix(2, 2)}.normalized();

    cmd.is_orthographic = std::fabs(proj_matrix(3, 3)) > 0.5f;
    cmd.zero_to_one_depth = (clip_space_depth == math::ClipSpaceDepth::ZeroToOne);
    cmd.frustum_half_x = 1.0f / p00;
    cmd.frustum_half_y = 1.0f / p11;
    cmd.proj_22 = proj_matrix(2, 2);
    cmd.proj_23 = proj_matrix(2, 3);
    cmd.opacity = std::clamp(options.opacity, 0.0f, 1.0f);

    const double base_scale = std::clamp(static_cast<double>(options.base_scale), 1.0e-3, 1.0e4);
    double scale_dist = 1.0;
    if (cmd.is_orthographic) {
        scale_dist = std::max(0.02, static_cast<double>(cmd.frustum_half_y) * 2.2);
    } else {
        const double cam_h = std::fabs(static_cast<double>(camera_pos.y));
        const double focus_d = (options.focus_distance > 0.0f)
                                   ? static_cast<double>(options.focus_distance)
                                   : std::max(cam_h * 2.1, 0.25);
        scale_dist = std::max(0.02, std::sqrt(cam_h * cam_h + (0.55 * focus_d) * (0.55 * focus_d)));
    }

    const double log_level = std::clamp(std::log10(std::max((scale_dist * 0.42) / base_scale, 1.0e-4)), -4.0, 6.0);
    const double level_floor = std::floor(log_level);
    const double blend = std::clamp(log_level - level_floor, 0.0, 1.0);

    const double s_minor = std::pow(10.0, level_floor) * base_scale;
    const double s_fine = s_minor * 0.1;
    const double s_major = s_minor * 10.0;

    cmd.step_fine = static_cast<float>(s_fine);
    cmd.step_minor = static_cast<float>(s_minor);
    cmd.step_major = static_cast<float>(s_major);
    cmd.level_blend = static_cast<float>(blend);
    cmd.fade_distance = static_cast<float>(std::clamp(scale_dist * 20.0, 20.0 * base_scale, 5.0e6));

    auto wrapPhase = [](double coord, double step) noexcept -> float {
        if (step <= 1.0e-9) {
            return 0.0f;
        }
        return static_cast<float>(std::fmod(coord, step));
    };

    cmd.phase_x = math::Vec3{
        wrapPhase(camera_world_x_precise, s_fine),
        wrapPhase(camera_world_x_precise, s_minor),
        wrapPhase(camera_world_x_precise, s_major),
    };
    cmd.phase_z = math::Vec3{
        wrapPhase(camera_world_z_precise, s_fine),
        wrapPhase(camera_world_z_precise, s_minor),
        wrapPhase(camera_world_z_precise, s_major),
    };

    // Clamp world-origin offset safely outside fade radius when camera is millions of units away.
    const double max_axis_offset = static_cast<double>(cmd.fade_distance) * 4.0 + 1024.0;
    cmd.axis_cam_x = static_cast<float>(std::clamp(camera_world_x_precise, -max_axis_offset, max_axis_offset));
    cmd.axis_cam_z = static_cast<float>(std::clamp(camera_world_z_precise, -max_axis_offset, max_axis_offset));

    return cmd;
}

EditorGridPixelSample evaluateEditorGridAtNdc(
    float ndc_x,
    float ndc_y,
    float dx_ndc,
    float dy_ndc,
    const EditorGridCommand& cmd) noexcept {
    EditorGridPixelSample out{};

    const math::Vec3& R = cmd.camera_right;
    const math::Vec3& U = cmd.camera_up;
    const math::Vec3& F = cmd.camera_forward;

    math::Vec3 o_rel{0.0f, 0.0f, 0.0f};
    math::Vec3 ray_d = F;
    if (cmd.is_orthographic) {
        o_rel = R * (ndc_x * cmd.frustum_half_x) + U * (ndc_y * cmd.frustum_half_y);
    } else {
        ray_d = F + R * (ndc_x * cmd.frustum_half_x) + U * (ndc_y * cmd.frustum_half_y);
    }

    const float origin_y = cmd.camera_position.y + o_rel.y;

    // Edge-on Orthographic Front/Back/Left/Right views (F.y == 0): draw crisp Y=0 ground-horizon axis line.
    if (cmd.is_orthographic && std::fabs(ray_d.y) <= 1.0e-4f) {
        const float px_world_y = std::max(
            std::fabs(dy_ndc * cmd.frustum_half_y * U.y) + std::fabs(dx_ndc * cmd.frustum_half_x * R.y),
            1.0e-5f);
        const float line_px = std::fabs(origin_y) / px_world_y;
        if (line_px > 1.85f) {
            return out;
        }
        const float t_mid = std::max(
            -(cmd.camera_position.x * F.x + cmd.camera_position.y * F.y + cmd.camera_position.z * F.z),
            0.25f);
        const float ndc_z = std::clamp(-cmd.proj_22 * t_mid + cmd.proj_23, -0.999f, 0.999f);
        const float win_depth = cmd.zero_to_one_depth ? std::clamp(ndc_z, 0.0f, 0.999f) : (ndc_z * 0.5f + 0.5f);
        const float cov = 1.0f - smoothstepScalar(0.65f, 1.80f, line_px);

        const math::Vec3 x_col{0.92f, 0.30f, 0.33f};
        const math::Vec3 z_col{0.26f, 0.58f, 0.96f};
        const float z_view_weight = std::fabs(F.z) / std::max(std::fabs(F.x) + std::fabs(F.z), 1.0e-4f);
        out.visible = (cov * cmd.opacity) > 0.004f;
        out.window_depth = std::min(win_depth + kGridDepthBias, 1.0f);
        out.color = mixColor(z_col, x_col, z_view_weight);
        out.alpha = std::clamp(cov * 0.84f * cmd.opacity, 0.0f, 1.0f);
        return out;
    }

    if (std::fabs(ray_d.y) <= 1.0e-6f) {
        return out;
    }

    const float t = -origin_y / ray_d.y;
    if (t <= 0.0f) {
        return out;
    }

    const float ndc_z = cmd.is_orthographic
                            ? (-cmd.proj_22 * t + cmd.proj_23)
                            : (-cmd.proj_22 + cmd.proj_23 / t);
    const float min_ndc_z = cmd.zero_to_one_depth ? 0.0f : -1.0f;
    if (ndc_z < min_ndc_z || ndc_z > 1.0f) {
        return out;
    }

    const float delta_x = o_rel.x + t * ray_d.x;
    const float delta_z = o_rel.z + t * ray_d.z;
    const float radial_dist = std::sqrt(delta_x * delta_x + delta_z * delta_z);
    if (radial_dist >= cmd.fade_distance) {
        return out;
    }

    const float radial_fade = 1.0f - smoothstepScalar(cmd.fade_distance * 0.36f, cmd.fade_distance, radial_dist);
    float angle_fade = 1.0f;
    if (!cmd.is_orthographic) {
        const float ray_len = ray_d.length();
        const float abs_sin_pitch = (ray_len > 1.0e-6f) ? (std::fabs(ray_d.y) / ray_len) : 0.0f;
        angle_fade = smoothstepScalar(0.012f, 0.13f, abs_sin_pitch);
    }
    const float global_fade = radial_fade * angle_fade * cmd.opacity;
    if (global_fade <= 0.004f) {
        return out;
    }

    // Analytic 1-pixel ground-plane differentials (zero 2x2 quad horizon artifacts, identical to GLSL).
    const float sx = cmd.is_orthographic ? (dx_ndc * cmd.frustum_half_x) : (dx_ndc * t * cmd.frustum_half_x);
    const float sy = cmd.is_orthographic ? (dy_ndc * cmd.frustum_half_y) : (dy_ndc * t * cmd.frustum_half_y);
    const float inv_dy = 1.0f / ray_d.y;

    const float jx_x = sx * (R.x - R.y * inv_dy * ray_d.x);
    const float jx_z = sx * (R.z - R.y * inv_dy * ray_d.z);
    const float jy_x = sy * (U.x - U.y * inv_dy * ray_d.x);
    const float jy_z = sy * (U.z - U.y * inv_dy * ray_d.z);

    const float fw_x = std::sqrt(jx_x * jx_x + jy_x * jy_x);
    const float fw_z = std::sqrt(jx_z * jx_z + jy_z * jy_z);

    // Continuous 3-cascade decade grid lines (C0-continuous across every 10x zoom boundary).
    const float cov_fine = std::max(
        evalGridCascadeLine(delta_x, cmd.phase_x.x, cmd.step_fine, fw_x, 0.65f),
        evalGridCascadeLine(delta_z, cmd.phase_z.x, cmd.step_fine, fw_z, 0.65f));
    const float cov_minor = std::max(
        evalGridCascadeLine(delta_x, cmd.phase_x.y, cmd.step_minor, fw_x, 0.78f),
        evalGridCascadeLine(delta_z, cmd.phase_z.y, cmd.step_minor, fw_z, 0.78f));
    const float cov_major = std::max(
        evalGridCascadeLine(delta_x, cmd.phase_x.z, cmd.step_major, fw_x, 1.06f),
        evalGridCascadeLine(delta_z, cmd.phase_z.z, cmd.step_major, fw_z, 1.06f));

    constexpr float kMinorAlpha = 0.28f;
    constexpr float kMajorAlpha = 0.56f;
    const float a_fine = cov_fine * kMinorAlpha * (1.0f - cmd.level_blend);
    const float a_minor = cov_minor * (kMajorAlpha * (1.0f - cmd.level_blend) + kMinorAlpha * cmd.level_blend);
    const float a_major = cov_major * kMajorAlpha;

    const math::Vec3 col_minor{0.35f, 0.40f, 0.48f};
    const math::Vec3 col_major{0.50f, 0.56f, 0.66f};

    float alpha = std::max(a_fine, std::max(a_minor, a_major));
    const float major_mix = (alpha > 1.0e-5f) ? std::clamp(a_major / alpha, 0.0f, 1.0f) : 0.0f;
    math::Vec3 color = mixColor(col_minor, col_major, major_mix);

    // World X Axis (z == 0, red/coral), World Z Axis (x == 0, blue/azure), and World Origin (0, 0, 0) marker.
    const float world_x = delta_x + cmd.axis_cam_x;
    const float world_z = delta_z + cmd.axis_cam_z;
    const float px_dist_x_axis = std::fabs(world_z) / std::max(fw_z, 1.0e-6f);
    const float px_dist_z_axis = std::fabs(world_x) / std::max(fw_x, 1.0e-6f);

    const float x_axis_nyq = 1.0f - smoothstepScalar(0.30f * cmd.step_major, 0.90f * cmd.step_major, fw_z);
    const float z_axis_nyq = 1.0f - smoothstepScalar(0.30f * cmd.step_major, 0.90f * cmd.step_major, fw_x);
    const float cov_x_axis = (1.0f - smoothstepScalar(0.75f, 1.85f, px_dist_x_axis)) * x_axis_nyq;
    const float cov_z_axis = (1.0f - smoothstepScalar(0.75f, 1.85f, px_dist_z_axis)) * z_axis_nyq;

    if (cov_x_axis > 1.0e-3f || cov_z_axis > 1.0e-3f) {
        const math::Vec3 x_axis_col{0.92f, 0.30f, 0.33f};
        const math::Vec3 z_axis_col{0.26f, 0.58f, 0.96f};
        const float axis_cov = std::max(cov_x_axis, cov_z_axis);
        const float axis_alpha = axis_cov * 0.88f;
        const float w_z = cov_z_axis / std::max(cov_x_axis + cov_z_axis, 1.0e-5f);
        const math::Vec3 axis_col = mixColor(x_axis_col, z_axis_col, w_z);

        const float blend_t = std::clamp(axis_cov, 0.0f, 1.0f);
        color = mixColor(color, axis_col, blend_t);
        alpha = std::max(alpha, axis_alpha);
    }

    // Crisp world-origin (0, 0, 0) intersection marker.
    if (px_dist_x_axis < 4.2f && px_dist_z_axis < 4.2f) {
        const float origin_r_px = std::sqrt(px_dist_x_axis * px_dist_x_axis + px_dist_z_axis * px_dist_z_axis);
        const float origin_cov = (1.0f - smoothstepScalar(1.65f, 3.25f, origin_r_px)) * std::min(x_axis_nyq, z_axis_nyq);
        if (origin_cov > 1.0e-3f) {
            const math::Vec3 origin_col{0.96f, 0.98f, 1.00f};
            color = mixColor(color, origin_col, origin_cov);
            alpha = std::max(alpha, origin_cov * 0.95f);
        }
    }

    const float final_alpha = std::clamp(alpha * global_fade, 0.0f, 1.0f);
    if (final_alpha <= 0.004f) {
        return out;
    }

    const float win_depth = cmd.zero_to_one_depth ? ndc_z : (ndc_z * 0.5f + 0.5f);
    out.visible = true;
    out.window_depth = std::clamp(win_depth + kGridDepthBias, 0.0f, 1.0f);
    out.color = color;
    out.alpha = final_alpha;
    return out;
}

} // namespace finaleffect::rendering
