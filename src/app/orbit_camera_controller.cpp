// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/orbit_camera_controller.hpp"
#include "finaleffect/math/constants.hpp"

#include <algorithm>
#include <cmath>

namespace finaleffect::app {

namespace {

constexpr float kMaxPitchRadians = math::degToRad(89.0f);

} // namespace

void OrbitCameraController::syncFromCameraTransform(
    const math::Transform& cam_tf,
    const math::Vec3& focus_target) noexcept {
    target_ = focus_target;
    const math::Vec3 offset = cam_tf.position - focus_target;
    const float len = offset.length();
    distance_ = std::clamp(len > math::kStrictEpsilon ? len : 6.0f, min_distance_, max_distance_);

    const float horiz = std::sqrt(offset.x * offset.x + offset.z * offset.z);
    if (horiz > math::kStrictEpsilon || std::fabs(offset.y) > math::kStrictEpsilon) {
        yaw_radians_ = std::atan2(offset.x, offset.z);
        pitch_radians_ = std::clamp(-std::atan2(offset.y, horiz), -kMaxPitchRadians, kMaxPitchRadians);
    }
}

void OrbitCameraController::orbit(float delta_yaw_radians, float delta_pitch_radians) noexcept {
    yaw_radians_ = std::remainder(yaw_radians_ + delta_yaw_radians, math::kTwoPi);
    pitch_radians_ = std::clamp(
        pitch_radians_ + delta_pitch_radians,
        -kMaxPitchRadians,
        kMaxPitchRadians);
}

void OrbitCameraController::pan(
    float delta_screen_x,
    float delta_screen_y,
    std::uint32_t viewport_height,
    float fov_y_radians) noexcept {
    if (viewport_height == 0) {
        return;
    }
    const float world_per_pixel =
        (2.0f * distance_ * std::tan(0.5f * std::max(0.1f, fov_y_radians))) /
        static_cast<float>(viewport_height);

    const math::Quat orient =
        math::Quat::fromAxisAngle(math::Vec3::unitY(), yaw_radians_) *
        math::Quat::fromAxisAngle(math::Vec3::unitX(), pitch_radians_);
    const math::Vec3 cam_right = orient.rotate(math::Vec3::unitX());
    const math::Vec3 cam_up = orient.rotate(math::Vec3::unitY());

    // Dragging mouse right (+dx) moves camera left relative to scene (shifts target -right);
    // dragging mouse down (+dy) shifts target +up so scene tracks the cursor.
    target_ += (-delta_screen_x * world_per_pixel) * cam_right +
               (delta_screen_y * world_per_pixel) * cam_up;
}

void OrbitCameraController::zoom(float wheel_steps) noexcept {
    if (!std::isfinite(wheel_steps)) {
        return;
    }
    const float factor = std::pow(0.88f, wheel_steps);
    distance_ = std::clamp(distance_ * factor, min_distance_, max_distance_);
}

void OrbitCameraController::frameBounds(const math::AABB& world_bounds, float fov_y_radians) noexcept {
    if (!world_bounds.isValid()) {
        return;
    }
    target_ = world_bounds.center();
    const float radius = std::max(0.35f, world_bounds.boundingSphereRadius());
    const float half_fov = std::clamp(0.5f * fov_y_radians, math::degToRad(5.0f), math::degToRad(75.0f));
    const float fit_dist = (radius / std::sin(half_fov)) * 1.15f;
    distance_ = std::clamp(fit_dist, min_distance_, max_distance_);
}

math::Transform OrbitCameraController::computeCameraTransform() const noexcept {
    const math::Quat orient =
        math::Quat::fromAxisAngle(math::Vec3::unitY(), yaw_radians_) *
        math::Quat::fromAxisAngle(math::Vec3::unitX(), pitch_radians_);
    const math::Vec3 eye = target_ + orient.rotate(math::Vec3{0.0f, 0.0f, distance_});
    return math::Transform{
        .position = eye,
        .rotation = orient,
        .scale = math::Vec3::one(),
    };
}

void OrbitCameraController::setDistanceLimits(float min_dist, float max_dist) noexcept {
    min_distance_ = std::max(0.05f, min_dist);
    max_distance_ = std::max(min_distance_, max_dist);
    distance_ = std::clamp(distance_, min_distance_, max_distance_);
}

core::Result<math::Ray> OrbitCameraController::screenPointToWorldRay(
    float screen_x,
    float screen_y,
    std::uint32_t viewport_width,
    std::uint32_t viewport_height,
    const math::Mat4& camera_world_matrix,
    const scene::CameraComponent& camera) noexcept {
    if (viewport_width == 0 || viewport_height == 0) {
        return core::Result<math::Ray>::fail(
            core::ErrorCode::InvalidArgument,
            "Cannot unproject screen ray for zero-area viewport");
    }

    const float f_width = static_cast<float>(viewport_width);
    const float f_height = static_cast<float>(viewport_height);
    const float aspect = f_width / f_height;

    const float ndc_x = (2.0f * screen_x / f_width) - 1.0f;
    const float ndc_y = 1.0f - (2.0f * screen_y / f_height);

    if (camera.projection == scene::ProjectionType::Orthographic) {
        const float half_h = 0.5f * camera.orthographic_height;
        const float half_w = half_h * aspect;
        const math::Vec3 local_origin{ndc_x * half_w, ndc_y * half_h, 0.0f};
        const math::Vec3 world_origin = camera_world_matrix.transformPoint(local_origin);
        const math::Vec3 world_dir =
            camera_world_matrix.transformDirection(math::Vec3{0.0f, 0.0f, -1.0f}).normalized();
        if (world_dir.lengthSquared() <= math::kStrictEpsilon) {
            return core::Result<math::Ray>::fail(
                core::ErrorCode::InvalidState,
                "Degenerate camera world matrix produced zero ray direction");
        }
        return core::Result<math::Ray>::ok(math::Ray(world_origin, world_dir));
    }

    const float tan_half_fov = std::tan(0.5f * camera.fov_y_radians);
    const math::Vec3 local_dir{
        ndc_x * aspect * tan_half_fov,
        ndc_y * tan_half_fov,
        -1.0f,
    };
    const math::Vec3 world_origin = camera_world_matrix.transformPoint(math::Vec3::zero());
    const math::Vec3 world_dir = camera_world_matrix.transformDirection(local_dir).normalized();
    if (world_dir.lengthSquared() <= math::kStrictEpsilon) {
        return core::Result<math::Ray>::fail(
            core::ErrorCode::InvalidState,
            "Degenerate camera world matrix produced zero ray direction");
    }
    return core::Result<math::Ray>::ok(math::Ray(world_origin, world_dir));
}

} // namespace finaleffect::app
