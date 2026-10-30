// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/math/vec3.hpp"
#include "finaleffect/scene/object_id.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace finaleffect::scene {

enum class LightType : std::uint8_t {
    Directional = 0,
    Point = 1,
    Spot = 2,
    Area = 3,
};

enum class AreaLightShape : std::uint8_t {
    Rectangle = 0,
    Disk = 1,
};

enum class LightFalloffMode : std::uint8_t {
    InverseSquareWindowed = 0,
    SmoothQuadratic = 1,
    SmoothLinear = 2,
};

enum class ShadowQuality : std::uint8_t {
    Low = 0,    // 256
    Medium = 1, // 512
    High = 2,   // 1024
    Ultra = 3,  // 2048
};

enum class ShadowFilterMode : std::uint8_t {
    Hard = 0,
    Pcf3x3 = 1,
    Pcf5x5 = 2,
};

enum class LightLinkMode : std::uint8_t {
    AllObjects = 0,
    IncludeOnly = 1,
    ExcludeList = 2,
};

[[nodiscard]] constexpr std::uint32_t shadowMapResolution(ShadowQuality quality) noexcept {
    switch (quality) {
        case ShadowQuality::Low: return 256u;
        case ShadowQuality::Medium: return 512u;
        case ShadowQuality::High: return 1024u;
        case ShadowQuality::Ultra: return 2048u;
    }
    return 512u;
}

[[nodiscard]] constexpr std::string_view lightTypeSlug(LightType type) noexcept {
    switch (type) {
        case LightType::Directional: return "directional";
        case LightType::Point: return "point";
        case LightType::Spot: return "spot";
        case LightType::Area: return "area";
    }
    return "directional";
}

[[nodiscard]] inline std::optional<LightType> parseLightTypeSlug(std::string_view slug) noexcept {
    if (slug == "directional") return LightType::Directional;
    if (slug == "point") return LightType::Point;
    if (slug == "spot") return LightType::Spot;
    if (slug == "area") return LightType::Area;
    return std::nullopt;
}

[[nodiscard]] constexpr std::string_view areaLightShapeSlug(AreaLightShape shape) noexcept {
    switch (shape) {
        case AreaLightShape::Rectangle: return "rectangle";
        case AreaLightShape::Disk: return "disk";
    }
    return "rectangle";
}

[[nodiscard]] inline std::optional<AreaLightShape> parseAreaLightShapeSlug(std::string_view slug) noexcept {
    if (slug == "rectangle") return AreaLightShape::Rectangle;
    if (slug == "disk") return AreaLightShape::Disk;
    return std::nullopt;
}

[[nodiscard]] constexpr std::string_view lightFalloffModeSlug(LightFalloffMode mode) noexcept {
    switch (mode) {
        case LightFalloffMode::InverseSquareWindowed: return "inverse_square_windowed";
        case LightFalloffMode::SmoothQuadratic: return "smooth_quadratic";
        case LightFalloffMode::SmoothLinear: return "smooth_linear";
    }
    return "inverse_square_windowed";
}

[[nodiscard]] inline std::optional<LightFalloffMode> parseLightFalloffModeSlug(std::string_view slug) noexcept {
    if (slug == "inverse_square_windowed" || slug == "inverse_square") return LightFalloffMode::InverseSquareWindowed;
    if (slug == "smooth_quadratic" || slug == "quadratic") return LightFalloffMode::SmoothQuadratic;
    if (slug == "smooth_linear" || slug == "linear") return LightFalloffMode::SmoothLinear;
    return std::nullopt;
}

[[nodiscard]] constexpr std::string_view shadowQualitySlug(ShadowQuality quality) noexcept {
    switch (quality) {
        case ShadowQuality::Low: return "low";
        case ShadowQuality::Medium: return "medium";
        case ShadowQuality::High: return "high";
        case ShadowQuality::Ultra: return "ultra";
    }
    return "medium";
}

[[nodiscard]] inline std::optional<ShadowQuality> parseShadowQualitySlug(std::string_view slug) noexcept {
    if (slug == "low") return ShadowQuality::Low;
    if (slug == "medium") return ShadowQuality::Medium;
    if (slug == "high") return ShadowQuality::High;
    if (slug == "ultra") return ShadowQuality::Ultra;
    return std::nullopt;
}

[[nodiscard]] constexpr std::string_view shadowFilterModeSlug(ShadowFilterMode mode) noexcept {
    switch (mode) {
        case ShadowFilterMode::Hard: return "hard";
        case ShadowFilterMode::Pcf3x3: return "pcf3x3";
        case ShadowFilterMode::Pcf5x5: return "pcf5x5";
    }
    return "pcf3x3";
}

[[nodiscard]] inline std::optional<ShadowFilterMode> parseShadowFilterModeSlug(std::string_view slug) noexcept {
    if (slug == "hard") return ShadowFilterMode::Hard;
    if (slug == "pcf3x3" || slug == "pcf") return ShadowFilterMode::Pcf3x3;
    if (slug == "pcf5x5") return ShadowFilterMode::Pcf5x5;
    return std::nullopt;
}

[[nodiscard]] constexpr std::string_view lightLinkModeSlug(LightLinkMode mode) noexcept {
    switch (mode) {
        case LightLinkMode::AllObjects: return "all";
        case LightLinkMode::IncludeOnly: return "include_only";
        case LightLinkMode::ExcludeList: return "exclude";
    }
    return "all";
}

[[nodiscard]] inline std::optional<LightLinkMode> parseLightLinkModeSlug(std::string_view slug) noexcept {
    if (slug == "all" || slug == "all_objects") return LightLinkMode::AllObjects;
    if (slug == "include_only" || slug == "include") return LightLinkMode::IncludeOnly;
    if (slug == "exclude" || slug == "exclude_list") return LightLinkMode::ExcludeList;
    return std::nullopt;
}

struct LightShadowSettings {
    bool enabled{false};
    float strength{1.0f};
    ShadowQuality quality{ShadowQuality::Medium};
    ShadowFilterMode filter{ShadowFilterMode::Pcf3x3};
    float bias{0.0025f};
    float normal_bias{0.02f};
    float softness{1.0f};

    [[nodiscard]] core::Status validate() const {
        if (!math::isFinite(strength) || !math::isFinite(bias) ||
            !math::isFinite(normal_bias) || !math::isFinite(softness)) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "Light shadow settings must be finite");
        }
        if (strength < 0.0f || strength > 1.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "Light shadow strength must be in [0, 1]");
        }
        if (bias < 0.0f || bias > 0.25f || normal_bias < 0.0f || normal_bias > 1.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "Light shadow bias must be in [0, 0.25] and normal_bias in [0, 1]");
        }
        if (softness < 0.05f || softness > 8.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "Light shadow softness must be in [0.05, 8]");
        }
        return core::Status::ok();
    }
};

struct LightLinkingSettings {
    LightLinkMode mode{LightLinkMode::AllObjects};
    // Sorted, deduplicated target ObjectIds (individual objects or parent collection/pivot roots).
    std::vector<ObjectId> linked_objects;
    // When true, linking a parent/pivot node applies recursively to all child descendants.
    bool include_descendants{true};

    void normalize() {
        linked_objects.erase(
            std::remove_if(
                linked_objects.begin(),
                linked_objects.end(),
                [](const ObjectId& id) { return !id.isValid(); }),
            linked_objects.end());
        std::sort(linked_objects.begin(), linked_objects.end());
        linked_objects.erase(
            std::unique(linked_objects.begin(), linked_objects.end()),
            linked_objects.end());
    }

    [[nodiscard]] bool containsDirect(ObjectId id) const noexcept {
        return std::binary_search(linked_objects.begin(), linked_objects.end(), id);
    }

    void removeObjectReference(ObjectId id) noexcept {
        linked_objects.erase(
            std::remove(linked_objects.begin(), linked_objects.end(), id),
            linked_objects.end());
    }
};

struct DirectionalLightComponent {
    math::Vec3 color{1.0f, 0.98f, 0.94f};
    float intensity{1.1f};
    bool enabled{true};
    LightShadowSettings shadow{};
    LightLinkingSettings linking{};

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
        return shadow.validate();
    }
};

struct PointLightComponent {
    math::Vec3 color{1.0f, 0.96f, 0.90f};
    float intensity{8.0f};
    bool enabled{true};
    float range{12.0f};
    float attenuation{2.0f};
    LightFalloffMode falloff{LightFalloffMode::InverseSquareWindowed};
    float source_radius{0.08f};
    LightShadowSettings shadow{};
    LightLinkingSettings linking{};

    [[nodiscard]] core::Status validate() const {
        if (!color.isFinite() || !math::isFinite(intensity) || !math::isFinite(range) ||
            !math::isFinite(attenuation) || !math::isFinite(source_radius)) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "PointLight parameters must be finite");
        }
        if (color.x < 0.0f || color.y < 0.0f || color.z < 0.0f || intensity < 0.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "PointLight color channels and intensity must be non-negative");
        }
        if (range <= 0.05f || range > 10000.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "PointLight range must be in (0.05, 10000]");
        }
        if (attenuation < 0.0f || attenuation > 4.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "PointLight attenuation exponent must be in [0, 4]");
        }
        if (source_radius < 0.01f || source_radius > 25.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "PointLight source_radius must be in [0.01, 25]");
        }
        return shadow.validate();
    }
};

struct SpotLightComponent {
    math::Vec3 color{1.0f, 0.97f, 0.92f};
    float intensity{14.0f};
    bool enabled{true};
    float range{16.0f};
    float attenuation{2.0f};
    LightFalloffMode falloff{LightFalloffMode::InverseSquareWindowed};
    float inner_cone_radians{math::degToRad(20.0f)};
    float outer_cone_radians{math::degToRad(35.0f)};
    float cone_falloff{1.0f};
    LightShadowSettings shadow{};
    LightLinkingSettings linking{};

    [[nodiscard]] core::Status validate() const {
        if (!color.isFinite() || !math::isFinite(intensity) || !math::isFinite(range) ||
            !math::isFinite(attenuation) || !math::isFinite(inner_cone_radians) ||
            !math::isFinite(outer_cone_radians) || !math::isFinite(cone_falloff)) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "SpotLight parameters must be finite");
        }
        if (color.x < 0.0f || color.y < 0.0f || color.z < 0.0f || intensity < 0.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "SpotLight color channels and intensity must be non-negative");
        }
        if (range <= 0.05f || range > 10000.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "SpotLight range must be in (0.05, 10000]");
        }
        if (attenuation < 0.0f || attenuation > 4.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "SpotLight attenuation exponent must be in [0, 4]");
        }
        if (outer_cone_radians <= math::degToRad(0.5f) || outer_cone_radians > math::degToRad(89.0f)) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "SpotLight outer_cone_radians must be in (0.5 deg, 89 deg]");
        }
        if (inner_cone_radians < 0.0f || inner_cone_radians > outer_cone_radians) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "SpotLight inner_cone_radians must be in [0, outer_cone_radians]");
        }
        if (cone_falloff < 0.1f || cone_falloff > 16.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "SpotLight cone_falloff must be in [0.1, 16]");
        }
        return shadow.validate();
    }
};

// Planar finite-surface area emitter lying in local XY plane and emitting along local -Z
// (and +Z when two_sided == true). Evaluated via stratified 2D emitter solid-angle quadrature
// for diffuse illumination and Brian Karis's Representative-Point planar GGX reflection for specular.
struct AreaLightComponent {
    AreaLightShape shape{AreaLightShape::Rectangle};
    math::Vec3 color{1.0f, 0.98f, 0.95f};
    float intensity{10.0f};
    bool enabled{true};
    float width{1.6f};  // Rectangle width, or Disk diameter when shape == Disk
    float height{1.0f}; // Rectangle height
    float range{16.0f};
    bool two_sided{false};
    std::uint32_t sample_quality{2}; // 1 = Low (4x), 2 = Medium (9x), 3 = High (16x)
    LightShadowSettings shadow{};
    LightLinkingSettings linking{};

    [[nodiscard]] float emittingArea() const noexcept {
        if (shape == AreaLightShape::Disk) {
            const float r = 0.5f * std::max(width, 0.02f);
            return math::kPi * r * r;
        }
        return std::max(width, 0.02f) * std::max(height, 0.02f);
    }

    [[nodiscard]] core::Status validate() const {
        if (!color.isFinite() || !math::isFinite(intensity) || !math::isFinite(width) ||
            !math::isFinite(height) || !math::isFinite(range)) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "AreaLight parameters must be finite");
        }
        if (color.x < 0.0f || color.y < 0.0f || color.z < 0.0f || intensity < 0.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "AreaLight color channels and intensity must be non-negative");
        }
        if (width < 0.02f || width > 500.0f || height < 0.02f || height > 500.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "AreaLight width and height must be in [0.02, 500]");
        }
        if (range <= 0.05f || range > 10000.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "AreaLight range must be in (0.05, 10000]");
        }
        if (sample_quality < 1u || sample_quality > 3u) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "AreaLight sample_quality must be 1 (4x), 2 (9x), or 3 (16x)");
        }
        return shadow.validate();
    }
};

using AnyLightComponent = std::variant<
    std::monostate,
    DirectionalLightComponent,
    PointLightComponent,
    SpotLightComponent,
    AreaLightComponent>;

// Converts between light types while preserving common properties (color, intensity, enabled,
// shadow configuration, and light linking).
[[nodiscard]] inline AnyLightComponent convertLightComponentType(
    const AnyLightComponent& current,
    LightType target_type) {
    math::Vec3 col{1.0f, 0.98f, 0.94f};
    float intensity = 1.5f;
    bool enabled = true;
    float range = 14.0f;
    LightShadowSettings shadow{};
    LightLinkingSettings linking{};

    std::visit(
        [&](const auto& l) {
            using T = std::decay_t<decltype(l)>;
            if constexpr (!std::is_same_v<T, std::monostate>) {
                col = l.color;
                intensity = l.intensity;
                enabled = l.enabled;
                shadow = l.shadow;
                linking = l.linking;
                if constexpr (std::is_same_v<T, PointLightComponent> ||
                              std::is_same_v<T, SpotLightComponent> ||
                              std::is_same_v<T, AreaLightComponent>) {
                    range = l.range;
                }
            }
        },
        current);

    switch (target_type) {
        case LightType::Directional:
            return DirectionalLightComponent{
                .color = col,
                .intensity = intensity,
                .enabled = enabled,
                .shadow = shadow,
                .linking = std::move(linking),
            };
        case LightType::Point:
            return PointLightComponent{
                .color = col,
                .intensity = std::max(intensity, 0.0f),
                .enabled = enabled,
                .range = range,
                .shadow = shadow,
                .linking = std::move(linking),
            };
        case LightType::Spot:
            return SpotLightComponent{
                .color = col,
                .intensity = std::max(intensity, 0.0f),
                .enabled = enabled,
                .range = range,
                .shadow = shadow,
                .linking = std::move(linking),
            };
        case LightType::Area:
            return AreaLightComponent{
                .color = col,
                .intensity = std::max(intensity, 0.0f),
                .enabled = enabled,
                .range = range,
                .shadow = shadow,
                .linking = std::move(linking),
            };
    }
    return std::monostate{};
}

} // namespace finaleffect::scene
