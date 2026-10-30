// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/math/aabb.hpp"
#include "finaleffect/math/mat4.hpp"
#include "finaleffect/math/ray.hpp"
#include "finaleffect/math/transform.hpp"
#include "finaleffect/scene/camera.hpp"

#include <cstdint>

namespace finaleffect::app {

// Right-handed Y-up spherical orbit / pan / dolly viewport camera controller.
class OrbitCameraController {
public:
    void syncFromCameraTransform(const math::Transform& cam_tf, const math::Vec3& focus_target) noexcept;

    void setPose(
        const math::Vec3& target,
        float yaw_radians,
        float pitch_radians,
        float distance) noexcept;

    void orbit(float delta_yaw_radians, float delta_pitch_radians) noexcept;

    // Pans target in the camera view plane so grabbed points track cursor pixel motion.
    void pan(
        float delta_screen_x,
        float delta_screen_y,
        std::uint32_t viewport_height,
        float fov_y_radians) noexcept;

    // Exponential dolly zoom (positive steps zoom closer to target).
    void zoom(float wheel_steps) noexcept;

    // Centers target on world_bounds and adjusts distance to fit within vertical FOV.
    void frameBounds(const math::AABB& world_bounds, float fov_y_radians) noexcept;

    [[nodiscard]] math::Transform computeCameraTransform() const noexcept;

    [[nodiscard]] const math::Vec3& target() const noexcept { return target_; }
    [[nodiscard]] float yawRadians() const noexcept { return yaw_radians_; }
    [[nodiscard]] float pitchRadians() const noexcept { return pitch_radians_; }
    [[nodiscard]] float distance() const noexcept { return distance_; }

    void setDistanceLimits(float min_dist, float max_dist) noexcept;

    // Unprojects a window/framebuffer pixel coordinate (top-left origin) into a world-space ray.
    [[nodiscard]] static core::Result<math::Ray> screenPointToWorldRay(
        float screen_x,
        float screen_y,
        std::uint32_t viewport_width,
        std::uint32_t viewport_height,
        const math::Mat4& camera_world_matrix,
        const scene::CameraComponent& camera) noexcept;

private:
    math::Vec3 target_{0.0f, 0.65f, 0.0f};
    float yaw_radians_{0.0f};
    float pitch_radians_{-0.35f};
    float distance_{6.0f};
    float min_distance_{0.25f};
    float max_distance_{500.0f};
};

} // namespace finaleffect::app
