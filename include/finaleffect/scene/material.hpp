// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/math/vec3.hpp"
#include "finaleffect/scene/object_id.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace finaleffect::scene {

// Metallic-roughness Physically Based Rendering (PBR) material definition.
struct PbrMaterial {
    MaterialId id{MaterialId::invalid()};
    std::string name{"Default Material"};

    // Linear-space albedo [0, 1]^3.
    math::Vec3 base_color{0.82f, 0.84f, 0.88f};
    float metallic{0.0f};
    float roughness{0.45f};
    float ambient_occlusion{1.0f};

    // Linear-space emissive color [0, 1]^3 scaled by emissive_strength >= 0.
    math::Vec3 emissive_color{0.0f, 0.0f, 0.0f};
    float emissive_strength{0.0f};

    // Alpha / opacity [0, 1].
    float opacity{1.0f};

    std::uint64_t revision{1};

    [[nodiscard]] core::Status validate() const {
        if (name.empty()) {
            return core::Status::fail(core::ErrorCode::ValidationError, "Material name must be non-empty");
        }
        if (!base_color.isFinite() || !emissive_color.isFinite() ||
            !std::isfinite(metallic) || !std::isfinite(roughness) ||
            !std::isfinite(ambient_occlusion) || !std::isfinite(emissive_strength) ||
            !std::isfinite(opacity)) {
            return core::Status::fail(core::ErrorCode::ValidationError, "Material properties must be finite");
        }
        if (base_color.x < 0.0f || base_color.x > 1.0f ||
            base_color.y < 0.0f || base_color.y > 1.0f ||
            base_color.z < 0.0f || base_color.z > 1.0f) {
            return core::Status::fail(core::ErrorCode::ValidationError, "Material base_color components must be in [0, 1]");
        }
        if (metallic < 0.0f || metallic > 1.0f) {
            return core::Status::fail(core::ErrorCode::ValidationError, "Material metallic must be in [0, 1]");
        }
        if (roughness < 0.0f || roughness > 1.0f) {
            return core::Status::fail(core::ErrorCode::ValidationError, "Material roughness must be in [0, 1]");
        }
        if (ambient_occlusion < 0.0f || ambient_occlusion > 1.0f) {
            return core::Status::fail(core::ErrorCode::ValidationError, "Material ambient_occlusion must be in [0, 1]");
        }
        if (emissive_color.x < 0.0f || emissive_color.x > 1.0f ||
            emissive_color.y < 0.0f || emissive_color.y > 1.0f ||
            emissive_color.z < 0.0f || emissive_color.z > 1.0f ||
            emissive_strength < 0.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "Material emissive_color must be in [0, 1] and emissive_strength >= 0");
        }
        if (opacity < 0.0f || opacity > 1.0f) {
            return core::Status::fail(core::ErrorCode::ValidationError, "Material opacity must be in [0, 1]");
        }
        return core::Status::ok();
    }

    [[nodiscard]] PbrMaterial sanitized() const {
        PbrMaterial out = *this;
        if (out.name.empty()) {
            out.name = "Untitled Material";
        }
        out.base_color.x = std::clamp(out.base_color.x, 0.0f, 1.0f);
        out.base_color.y = std::clamp(out.base_color.y, 0.0f, 1.0f);
        out.base_color.z = std::clamp(out.base_color.z, 0.0f, 1.0f);
        out.metallic = std::clamp(out.metallic, 0.0f, 1.0f);
        out.roughness = std::clamp(out.roughness, 0.04f, 1.0f);
        out.ambient_occlusion = std::clamp(out.ambient_occlusion, 0.0f, 1.0f);
        out.emissive_color.x = std::clamp(out.emissive_color.x, 0.0f, 1.0f);
        out.emissive_color.y = std::clamp(out.emissive_color.y, 0.0f, 1.0f);
        out.emissive_color.z = std::clamp(out.emissive_color.z, 0.0f, 1.0f);
        out.emissive_strength = std::max(0.0f, out.emissive_strength);
        out.opacity = std::clamp(out.opacity, 0.0f, 1.0f);
        return out;
    }

    [[nodiscard]] bool approxEqualProperties(const PbrMaterial& rhs, float eps = 1.0e-5f) const noexcept {
        return base_color.approxEqual(rhs.base_color, eps) &&
               std::fabs(metallic - rhs.metallic) <= eps &&
               std::fabs(roughness - rhs.roughness) <= eps &&
               std::fabs(ambient_occlusion - rhs.ambient_occlusion) <= eps &&
               emissive_color.approxEqual(rhs.emissive_color, eps) &&
               std::fabs(emissive_strength - rhs.emissive_strength) <= eps &&
               std::fabs(opacity - rhs.opacity) <= eps;
    }
};

} // namespace finaleffect::scene
