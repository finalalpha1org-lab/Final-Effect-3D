// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/math/mat4.hpp"

#include <cstdint>

namespace finaleffect::scene {

enum class ProjectionType : std::uint8_t {
    Perspective = 0,
    Orthographic = 1
};

struct CameraComponent {
    ProjectionType projection{ProjectionType::Perspective};
    float fov_y_radians{math::degToRad(50.0f)};
    float orthographic_height{6.0f};
    float near_plane{0.1f};
    float far_plane{1000.0f};

    [[nodiscard]] core::Status validate() const {
        if (!math::isFinite(fov_y_radians) || !math::isFinite(orthographic_height) ||
            !math::isFinite(near_plane) || !math::isFinite(far_plane)) {
            return core::Status::fail(core::ErrorCode::ValidationError, "Camera parameters must be finite");
        }
        if (near_plane <= 0.0f || far_plane <= near_plane) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "Camera requires 0 < near_plane < far_plane");
        }
        if (projection == ProjectionType::Perspective) {
            if (fov_y_radians <= math::degToRad(1.0f) || fov_y_radians >= math::degToRad(179.0f)) {
                return core::Status::fail(
                    core::ErrorCode::ValidationError,
                    "Perspective camera vertical FOV must lie in (1 deg, 179 deg)");
            }
        } else {
            if (orthographic_height <= 1.0e-4f) {
                return core::Status::fail(
                    core::ErrorCode::ValidationError,
                    "Orthographic camera height must be positive");
            }
        }
        return core::Status::ok();
    }

    [[nodiscard]] math::Mat4 computeProjectionMatrix(
        float aspect_ratio,
        math::ClipSpaceDepth depth_range = math::ClipSpaceDepth::NegativeOneToOne) const noexcept {
        const float safe_aspect = (aspect_ratio > 1.0e-4f) ? aspect_ratio : 1.0f;
        if (projection == ProjectionType::Perspective) {
            return (depth_range == math::ClipSpaceDepth::NegativeOneToOne)
                       ? math::Mat4::perspectiveRH_NO(fov_y_radians, safe_aspect, near_plane, far_plane)
                       : math::Mat4::perspectiveRH_ZO(fov_y_radians, safe_aspect, near_plane, far_plane);
        }

        const float half_h = 0.5f * orthographic_height;
        const float half_w = half_h * safe_aspect;
        return (depth_range == math::ClipSpaceDepth::NegativeOneToOne)
                   ? math::Mat4::orthographicRH_NO(-half_w, half_w, -half_h, half_h, near_plane, far_plane)
                   : math::Mat4::orthographicRH_ZO(-half_w, half_w, -half_h, half_h, near_plane, far_plane);
    }
};

} // namespace finaleffect::scene
