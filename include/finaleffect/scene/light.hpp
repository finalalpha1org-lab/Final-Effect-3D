// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/math/vec3.hpp"

namespace finaleffect::scene {

struct DirectionalLightComponent {
    math::Vec3 color{1.0f, 0.98f, 0.94f};
    float intensity{1.1f};

    [[nodiscard]] core::Status validate() const {
        if (!color.isFinite() || !math::isFinite(intensity)) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "DirectionalLight parameters must be finite");
        }
        if (color.x < 0.0f || color.y < 0.0f || color.z < 0.0f || intensity < 0.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "DirectionalLight color channels and intensity must be non-negative");
        }
        return core::Status::ok();
    }
};

} // namespace finaleffect::scene
