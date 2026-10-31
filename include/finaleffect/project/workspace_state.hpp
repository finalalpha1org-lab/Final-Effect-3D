// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/scene/editable_mesh.hpp"
#include "finaleffect/scene/object_id.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace finaleffect::project {

enum class WorkspacePanelId : std::uint8_t {
    Viewport3D = 0,
    SceneOutliner = 1,
    Properties = 2,
    MaterialEditor = 3,
    AssetBrowser = 4,
    Timeline = 5,
    Console = 6,
};

inline constexpr std::size_t kWorkspacePanelCount = 7;

[[nodiscard]] constexpr std::string_view workspacePanelSlug(WorkspacePanelId id) noexcept {
    switch (id) {
        case WorkspacePanelId::Viewport3D:     return "viewport_3d";
        case WorkspacePanelId::SceneOutliner:  return "scene_outliner";
        case WorkspacePanelId::Properties:     return "properties";
        case WorkspacePanelId::MaterialEditor: return "material_editor";
        case WorkspacePanelId::AssetBrowser:   return "asset_browser";
        case WorkspacePanelId::Timeline:       return "timeline";
        case WorkspacePanelId::Console:        return "console";
    }
    return "viewport_3d";
}

[[nodiscard]] constexpr std::string_view workspacePanelTitle(WorkspacePanelId id) noexcept {
    switch (id) {
        case WorkspacePanelId::Viewport3D:     return "3D Viewport";
        case WorkspacePanelId::SceneOutliner:  return "Scene Outliner";
        case WorkspacePanelId::Properties:     return "Properties";
        case WorkspacePanelId::MaterialEditor: return "Material Editor";
        case WorkspacePanelId::AssetBrowser:   return "Asset Browser";
        case WorkspacePanelId::Timeline:       return "Timeline";
        case WorkspacePanelId::Console:        return "Console";
    }
    return "3D Viewport";
}

[[nodiscard]] constexpr std::optional<WorkspacePanelId> parseWorkspacePanelSlug(std::string_view slug) noexcept {
    if (slug == "viewport_3d")     return WorkspacePanelId::Viewport3D;
    if (slug == "scene_outliner")  return WorkspacePanelId::SceneOutliner;
    if (slug == "properties")      return WorkspacePanelId::Properties;
    if (slug == "material_editor") return WorkspacePanelId::MaterialEditor;
    if (slug == "asset_browser")   return WorkspacePanelId::AssetBrowser;
    if (slug == "timeline")        return WorkspacePanelId::Timeline;
    if (slug == "console")         return WorkspacePanelId::Console;
    return std::nullopt;
}

enum class DockRegion : std::uint8_t {
    Center = 0,
    Left = 1,
    Right = 2,
    Bottom = 3,
    Floating = 4,
};

[[nodiscard]] constexpr std::string_view dockRegionSlug(DockRegion region) noexcept {
    switch (region) {
        case DockRegion::Center:   return "center";
        case DockRegion::Left:     return "left";
        case DockRegion::Right:    return "right";
        case DockRegion::Bottom:   return "bottom";
        case DockRegion::Floating: return "floating";
    }
    return "center";
}

[[nodiscard]] constexpr std::optional<DockRegion> parseDockRegionSlug(std::string_view slug) noexcept {
    if (slug == "center")   return DockRegion::Center;
    if (slug == "left")     return DockRegion::Left;
    if (slug == "right")    return DockRegion::Right;
    if (slug == "bottom")   return DockRegion::Bottom;
    if (slug == "floating") return DockRegion::Floating;
    return std::nullopt;
}

struct WorkspacePanelState {
    WorkspacePanelId id{WorkspacePanelId::Viewport3D};
    bool visible{true};
    DockRegion dock_region{DockRegion::Center};
    std::uint32_t tab_order{0};
};

struct WorkspaceState {
    std::string layout_name{"Default Studio"};
    WorkspacePanelId focused_panel{WorkspacePanelId::Viewport3D};
    float left_dock_width_ratio{0.19f};
    float right_dock_width_ratio{0.23f};
    float bottom_dock_height_ratio{0.22f};
    std::array<WorkspacePanelState, kWorkspacePanelCount> panels{{
        {WorkspacePanelId::Viewport3D, true, DockRegion::Center, 0},
        {WorkspacePanelId::SceneOutliner, true, DockRegion::Left, 0},
        {WorkspacePanelId::Properties, true, DockRegion::Right, 0},
        {WorkspacePanelId::MaterialEditor, true, DockRegion::Right, 1},
        {WorkspacePanelId::AssetBrowser, true, DockRegion::Bottom, 0},
        {WorkspacePanelId::Timeline, false, DockRegion::Bottom, 1},
        {WorkspacePanelId::Console, true, DockRegion::Bottom, 2},
    }};
};

struct ProjectEditorState {
    scene::EditMode edit_mode{scene::EditMode::Object};
    std::string gizmo_tool{"translate"};  // "translate" | "rotate" | "scale"
    std::string gizmo_space{"world"};     // "world" | "local"
    scene::ObjectId selected_object_id{scene::ObjectId::invalid()};
    scene::MaterialId active_material_id{scene::MaterialId::invalid()};
    scene::MeshElementSelection mesh_selection{};
};

} // namespace finaleffect::project
