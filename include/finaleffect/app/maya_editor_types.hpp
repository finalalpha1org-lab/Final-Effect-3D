// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/transform.hpp"
#include "finaleffect/math/vec3.hpp"
#include "finaleffect/scene/object_id.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace finaleffect::app {

enum class MayaMenuSet : std::uint8_t {
    Modeling = 0,
    Rigging = 1,
    Animation = 2,
    FX = 3,
    Rendering = 4,
};

[[nodiscard]] constexpr std::string_view mayaMenuSetSlug(MayaMenuSet set) noexcept {
    switch (set) {
        case MayaMenuSet::Rigging:   return "rigging";
        case MayaMenuSet::Animation: return "animation";
        case MayaMenuSet::FX:        return "fx";
        case MayaMenuSet::Rendering: return "rendering";
        case MayaMenuSet::Modeling:
        default:                     return "modeling";
    }
}

[[nodiscard]] constexpr std::string_view mayaMenuSetLabel(MayaMenuSet set) noexcept {
    switch (set) {
        case MayaMenuSet::Rigging:   return "Rigging";
        case MayaMenuSet::Animation: return "Animation";
        case MayaMenuSet::FX:        return "FX";
        case MayaMenuSet::Rendering: return "Rendering";
        case MayaMenuSet::Modeling:
        default:                     return "Modeling";
    }
}

[[nodiscard]] constexpr std::optional<MayaMenuSet> parseMayaMenuSetSlug(std::string_view slug) noexcept {
    if (slug == "modeling")  return MayaMenuSet::Modeling;
    if (slug == "rigging")   return MayaMenuSet::Rigging;
    if (slug == "animation") return MayaMenuSet::Animation;
    if (slug == "fx")        return MayaMenuSet::FX;
    if (slug == "rendering") return MayaMenuSet::Rendering;
    return std::nullopt;
}

enum class MayaTool : std::uint8_t {
    Select = 0,
    Move = 1,
    Rotate = 2,
    Scale = 3,
    ShowManipulator = 4,
    Universal = 5,
    PivotEdit = 6,
};

[[nodiscard]] constexpr std::string_view mayaToolSlug(MayaTool tool) noexcept {
    switch (tool) {
        case MayaTool::Select:          return "select";
        case MayaTool::Move:            return "move";
        case MayaTool::Rotate:          return "rotate";
        case MayaTool::Scale:           return "scale";
        case MayaTool::ShowManipulator: return "manipulator";
        case MayaTool::Universal:       return "universal";
        case MayaTool::PivotEdit:       return "pivot";
    }
    return "move";
}

[[nodiscard]] constexpr std::optional<MayaTool> parseMayaToolSlug(std::string_view slug) noexcept {
    if (slug == "select")                            return MayaTool::Select;
    if (slug == "move" || slug == "translate")       return MayaTool::Move;
    if (slug == "rotate")                            return MayaTool::Rotate;
    if (slug == "scale")                             return MayaTool::Scale;
    if (slug == "manipulator" || slug == "show_manipulator") return MayaTool::ShowManipulator;
    if (slug == "universal")                         return MayaTool::Universal;
    if (slug == "pivot" || slug == "edit_pivot")     return MayaTool::PivotEdit;
    return std::nullopt;
}

struct SelectionTypeMasks {
    bool meshes{true};
    bool cameras{true};
    bool lights{true};
    bool helpers{true};
    bool uv_mode{false};
    bool multi_component_mode{false};
};

struct SnapSettings {
    bool grid{false};
    bool point{false};
    bool curve{false};
    bool surface{false};
    float grid_step{0.5f};
    float rotate_step_deg{15.0f};
    float scale_step{0.25f};
};

enum class DisplayLayerMode : std::uint8_t {
    Normal = 0,
    Template = 1,
    Reference = 2,
};

[[nodiscard]] constexpr std::string_view displayLayerModeSlug(DisplayLayerMode mode) noexcept {
    switch (mode) {
        case DisplayLayerMode::Template:  return "template";
        case DisplayLayerMode::Reference: return "reference";
        case DisplayLayerMode::Normal:
        default:                          return "normal";
    }
}

[[nodiscard]] constexpr std::optional<DisplayLayerMode> parseDisplayLayerModeSlug(std::string_view slug) noexcept {
    if (slug == "normal" || slug == "") return DisplayLayerMode::Normal;
    if (slug == "template" || slug == "t") return DisplayLayerMode::Template;
    if (slug == "reference" || slug == "r") return DisplayLayerMode::Reference;
    return std::nullopt;
}

struct DisplayLayer {
    std::uint32_t id{0};
    std::string name{"defaultLayer"};
    bool visible{true};
    DisplayLayerMode mode{DisplayLayerMode::Normal};
    math::Vec3 color{0.34f, 0.56f, 0.84f};
    std::vector<scene::ObjectId> members;
};

struct CustomChannelAttribute {
    std::string name;
    float value{0.0f};
    float min_value{-1000.0f};
    float max_value{1000.0f};
    bool keyable{true};
    bool locked{false};
};

struct ObjectEditorMeta {
    bool locked{false};
    // Bitmask: 0=tx, 1=ty, 2=tz, 3=rx, 4=ry, 5=rz, 6=sx, 7=sy, 8=sz, 9=v
    std::uint16_t locked_channels{0};
    math::Vec3 pivot_offset{0.0f, 0.0f, 0.0f};
    std::vector<CustomChannelAttribute> custom_attributes;

    [[nodiscard]] constexpr bool isChannelLocked(std::uint8_t idx) const noexcept {
        return locked || ((locked_channels & static_cast<std::uint16_t>(1u << idx)) != 0u);
    }
};

enum class PlaybackLoopMode : std::uint8_t {
    Loop = 0,
    Once = 1,
    Oscillate = 2,
};

[[nodiscard]] constexpr std::string_view playbackLoopModeSlug(PlaybackLoopMode mode) noexcept {
    switch (mode) {
        case PlaybackLoopMode::Once:      return "once";
        case PlaybackLoopMode::Oscillate: return "oscillate";
        case PlaybackLoopMode::Loop:
        default:                          return "loop";
    }
}

[[nodiscard]] constexpr std::optional<PlaybackLoopMode> parsePlaybackLoopModeSlug(std::string_view slug) noexcept {
    if (slug == "loop") return PlaybackLoopMode::Loop;
    if (slug == "once" || slug == "single") return PlaybackLoopMode::Once;
    if (slug == "oscillate" || slug == "pingpong") return PlaybackLoopMode::Oscillate;
    return std::nullopt;
}

struct TransformKeyframe {
    int frame{1};
    math::Transform transform{};
};

struct ObjectAnimationTrack {
    scene::ObjectId object_id{};
    std::vector<TransformKeyframe> keys;
};

struct TimelineState {
    double current_frame{1.0};
    int scene_start{1};
    int scene_end{240};
    int playback_start{1};
    int playback_end{120};
    float fps{24.0f};
    bool playing{false};
    int playback_direction{1};
    bool auto_key{false};
    PlaybackLoopMode loop_mode{PlaybackLoopMode::Loop};
    std::vector<ObjectAnimationTrack> tracks;
};

} // namespace finaleffect::app
