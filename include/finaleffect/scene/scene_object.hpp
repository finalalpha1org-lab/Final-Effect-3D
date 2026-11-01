// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/transform.hpp"
#include "finaleffect/scene/camera.hpp"
#include "finaleffect/scene/light.hpp"
#include "finaleffect/scene/object_id.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace finaleffect::scene {

// Bitmask indices for per-channel locking in the Maya Channel Box.
namespace ChannelBit {
constexpr std::uint16_t kTranslateX = 1u << 0u;
constexpr std::uint16_t kTranslateY = 1u << 1u;
constexpr std::uint16_t kTranslateZ = 1u << 2u;
constexpr std::uint16_t kRotateX    = 1u << 3u;
constexpr std::uint16_t kRotateY    = 1u << 4u;
constexpr std::uint16_t kRotateZ    = 1u << 5u;
constexpr std::uint16_t kScaleX     = 1u << 6u;
constexpr std::uint16_t kScaleY     = 1u << 7u;
constexpr std::uint16_t kScaleZ     = 1u << 8u;
constexpr std::uint16_t kVisibility = 1u << 9u;
} // namespace ChannelBit

struct CustomAttribute {
    std::string name;
    double value{0.0};
    double min_value{-1000.0};
    double max_value{1000.0};
    bool has_limits{false};
    bool keyable{true};
    bool locked{false};

    bool operator==(const CustomAttribute&) const = default;
};

enum class DisplayLayerType : std::uint8_t {
    Normal = 0,
    Template = 1,
    Reference = 2,
};

[[nodiscard]] constexpr std::string_view displayLayerTypeSlug(DisplayLayerType t) noexcept {
    switch (t) {
        case DisplayLayerType::Template:  return "template";
        case DisplayLayerType::Reference: return "reference";
        case DisplayLayerType::Normal:
        default:                          return "normal";
    }
}

[[nodiscard]] constexpr std::optional<DisplayLayerType> parseDisplayLayerTypeSlug(std::string_view s) noexcept {
    if (s == "normal" || s.empty()) return DisplayLayerType::Normal;
    if (s == "template" || s == "T") return DisplayLayerType::Template;
    if (s == "reference" || s == "R") return DisplayLayerType::Reference;
    return std::nullopt;
}

struct DisplayLayer {
    std::uint32_t id{1};
    std::string name{"layer1"};
    bool visible{true};
    bool playback_visible{true};
    DisplayLayerType display_type{DisplayLayerType::Normal};
    math::Vec3 color{0.34f, 0.56f, 0.86f};
    std::vector<ObjectId> members{};
};

struct TransformKeyframe {
    float frame{1.0f};
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    math::Vec3 rotation_deg{0.0f, 0.0f, 0.0f};
    math::Vec3 scale{1.0f, 1.0f, 1.0f};
    bool visible{true};
};

struct ObjectAnimationTrack {
    ObjectId object_id{ObjectId::invalid()};
    std::vector<TransformKeyframe> keyframes{};
};

enum class TimelineLoopMode : std::uint8_t {
    Continuous = 0,
    Once = 1,
    Oscillate = 2,
};

[[nodiscard]] constexpr std::string_view timelineLoopModeSlug(TimelineLoopMode m) noexcept {
    switch (m) {
        case TimelineLoopMode::Once:      return "once";
        case TimelineLoopMode::Oscillate: return "oscillate";
        case TimelineLoopMode::Continuous:
        default:                          return "continuous";
    }
}

[[nodiscard]] constexpr std::optional<TimelineLoopMode> parseTimelineLoopModeSlug(std::string_view s) noexcept {
    if (s == "continuous") return TimelineLoopMode::Continuous;
    if (s == "once")       return TimelineLoopMode::Once;
    if (s == "oscillate")  return TimelineLoopMode::Oscillate;
    return std::nullopt;
}

struct TimelineSettings {
    float current_frame{1.0f};
    float start_frame{1.0f};
    float playback_start{1.0f};
    float playback_end{120.0f};
    float end_frame{240.0f};
    float fps{24.0f};
    TimelineLoopMode loop_mode{TimelineLoopMode::Continuous};
    bool auto_key{false};
};

struct SceneObject {
    ObjectId id{ObjectId::invalid()};
    std::string name;
    bool visible{true};
    math::Transform local_transform{};
    ObjectId parent_id{ObjectId::invalid()};
    std::vector<ObjectId> children;

    std::optional<MeshId> mesh_id;
    std::optional<MaterialId> material_id;
    std::optional<CameraComponent> camera;
    std::optional<DirectionalLightComponent> directional_light;
    std::optional<PointLightComponent> point_light;
    std::optional<SpotLightComponent> spot_light;
    std::optional<AreaLightComponent> area_light;

    // Maya Stage 1 node attributes: node lock, per-channel lock mask, pivot offset, and custom channels.
    bool locked{false};
    std::uint16_t locked_channels_mask{0};
    math::Vec3 pivot_offset{0.0f, 0.0f, 0.0f};
    std::vector<CustomAttribute> custom_attributes{};

    [[nodiscard]] bool hasAnyLight() const noexcept {
        return directional_light.has_value() ||
               point_light.has_value() ||
               spot_light.has_value() ||
               area_light.has_value();
    }

    [[nodiscard]] std::optional<LightType> lightType() const noexcept {
        if (directional_light.has_value()) return LightType::Directional;
        if (point_light.has_value()) return LightType::Point;
        if (spot_light.has_value()) return LightType::Spot;
        if (area_light.has_value()) return LightType::Area;
        return std::nullopt;
    }

    [[nodiscard]] AnyLightComponent anyLightComponent() const {
        if (directional_light.has_value()) return *directional_light;
        if (point_light.has_value()) return *point_light;
        if (spot_light.has_value()) return *spot_light;
        if (area_light.has_value()) return *area_light;
        return std::monostate{};
    }

    [[nodiscard]] const LightLinkingSettings* lightLinking() const noexcept {
        if (directional_light.has_value()) return &directional_light->linking;
        if (point_light.has_value()) return &point_light->linking;
        if (spot_light.has_value()) return &spot_light->linking;
        if (area_light.has_value()) return &area_light->linking;
        return nullptr;
    }

    [[nodiscard]] LightLinkingSettings* lightLinkingMutable() noexcept {
        if (directional_light.has_value()) return &directional_light->linking;
        if (point_light.has_value()) return &point_light->linking;
        if (spot_light.has_value()) return &spot_light->linking;
        if (area_light.has_value()) return &area_light->linking;
        return nullptr;
    }
};

} // namespace finaleffect::scene
