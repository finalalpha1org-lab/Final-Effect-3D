// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/workspace_manager.hpp"
#include "finaleffect/project/project.hpp"

#include <algorithm>
#include <utility>

namespace finaleffect::app {

using project::DockRegion;
using project::WorkspacePanelId;
using project::WorkspaceState;

WorkspaceManager::WorkspaceManager() {
    sanitizeFocusAndRatios();
}

WorkspaceManager::WorkspaceManager(WorkspaceState initial_state)
    : state_(std::move(initial_state)) {
    sanitizeFocusAndRatios();
}

void WorkspaceManager::setState(WorkspaceState new_state) {
    state_ = std::move(new_state);
    sanitizeFocusAndRatios();
}

bool WorkspaceManager::isPanelVisible(WorkspacePanelId id) const noexcept {
    return state_.panels[static_cast<std::size_t>(id)].visible;
}

void WorkspaceManager::setPanelVisible(WorkspacePanelId id, bool visible) {
    state_.panels[static_cast<std::size_t>(id)].visible = visible;
    sanitizeFocusAndRatios();
}

void WorkspaceManager::togglePanelVisible(WorkspacePanelId id) {
    const auto idx = static_cast<std::size_t>(id);
    state_.panels[idx].visible = !state_.panels[idx].visible;
    sanitizeFocusAndRatios();
}

void WorkspaceManager::setFocusedPanel(WorkspacePanelId id) noexcept {
    if (isPanelVisible(id)) {
        state_.focused_panel = id;
    }
}

void WorkspaceManager::setPanelDock(
    WorkspacePanelId id,
    DockRegion region,
    std::uint32_t tab_order) noexcept {
    auto& slot = state_.panels[static_cast<std::size_t>(id)];
    slot.dock_region = region;
    slot.tab_order = tab_order;
}

void WorkspaceManager::setDockRatios(float left_width, float right_width, float bottom_height) noexcept {
    state_.left_dock_width_ratio = left_width;
    state_.right_dock_width_ratio = right_width;
    state_.bottom_dock_height_ratio = bottom_height;
    sanitizeFocusAndRatios();
}

std::vector<WorkspacePanelId> WorkspaceManager::visiblePanelsInRegion(DockRegion region) const {
    struct OrderedPanel {
        WorkspacePanelId id;
        std::uint32_t order;
    };
    std::vector<OrderedPanel> matches;
    for (const auto& p : state_.panels) {
        if (p.visible && p.dock_region == region) {
            matches.push_back(OrderedPanel{p.id, p.tab_order});
        }
    }
    std::stable_sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) {
        return a.order < b.order;
    });

    std::vector<WorkspacePanelId> ids;
    ids.reserve(matches.size());
    for (const auto& m : matches) {
        ids.push_back(m.id);
    }
    return ids;
}

void WorkspaceManager::applyPreset(WorkspacePreset preset) {
    WorkspaceState next{};
    switch (preset) {
        case WorkspacePreset::DefaultStudio:
            next.layout_name = "Default Studio";
            next.focused_panel = WorkspacePanelId::Viewport3D;
            break;

        case WorkspacePreset::Modeling:
            next.layout_name = "Modeling";
            next.focused_panel = WorkspacePanelId::Viewport3D;
            next.left_dock_width_ratio = 0.16f;
            next.right_dock_width_ratio = 0.21f;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::MaterialEditor)].visible = false;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::AssetBrowser)].visible = false;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Timeline)].visible = false;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Console)].visible = false;
            break;

        case WorkspacePreset::Shading:
            next.layout_name = "Shading";
            next.focused_panel = WorkspacePanelId::MaterialEditor;
            next.left_dock_width_ratio = 0.17f;
            next.right_dock_width_ratio = 0.27f;
            next.bottom_dock_height_ratio = 0.20f;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::MaterialEditor)].visible = true;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Timeline)].visible = false;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Console)].visible = false;
            break;

        case WorkspacePreset::AnimationPrep:
            next.layout_name = "Animation";
            next.focused_panel = WorkspacePanelId::Timeline;
            next.bottom_dock_height_ratio = 0.26f;
            for (auto& p : next.panels) {
                p.visible = true;
            }
            break;

        case WorkspacePreset::Layout:
            next.layout_name = "Layout";
            next.focused_panel = WorkspacePanelId::Viewport3D;
            next.left_dock_width_ratio = 0.17f;
            next.right_dock_width_ratio = 0.23f;
            next.bottom_dock_height_ratio = 0.18f;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Timeline)].visible = true;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::AssetBrowser)].visible = false;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Console)].visible = false;
            break;

        case WorkspacePreset::Sculpting:
            next.layout_name = "Sculpting";
            next.focused_panel = WorkspacePanelId::Viewport3D;
            next.left_dock_width_ratio = 0.15f;
            next.right_dock_width_ratio = 0.20f;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::MaterialEditor)].visible = false;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::AssetBrowser)].visible = false;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Timeline)].visible = false;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Console)].visible = false;
            break;

        case WorkspacePreset::Rendering:
            next.layout_name = "Rendering";
            next.focused_panel = WorkspacePanelId::Properties;
            next.left_dock_width_ratio = 0.16f;
            next.right_dock_width_ratio = 0.26f;
            next.bottom_dock_height_ratio = 0.20f;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Timeline)].visible = false;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::AssetBrowser)].visible = false;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Console)].visible = true;
            break;

        case WorkspacePreset::Compositing:
            next.layout_name = "Compositing";
            next.focused_panel = WorkspacePanelId::Viewport3D;
            next.left_dock_width_ratio = 0.15f;
            next.right_dock_width_ratio = 0.23f;
            next.bottom_dock_height_ratio = 0.28f;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Console)].visible = true;
            break;

        case WorkspacePreset::VFX:
            next.layout_name = "VFX";
            next.focused_panel = WorkspacePanelId::Viewport3D;
            next.left_dock_width_ratio = 0.16f;
            next.right_dock_width_ratio = 0.24f;
            next.bottom_dock_height_ratio = 0.24f;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Timeline)].visible = true;
            break;

        case WorkspacePreset::GeometryNodes:
            next.layout_name = "Geometry Nodes";
            next.focused_panel = WorkspacePanelId::Viewport3D;
            next.left_dock_width_ratio = 0.16f;
            next.right_dock_width_ratio = 0.23f;
            next.bottom_dock_height_ratio = 0.30f;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::AssetBrowser)].visible = true;
            break;

        case WorkspacePreset::UvEditing:
            next.layout_name = "UV Editing";
            next.focused_panel = WorkspacePanelId::Viewport3D;
            next.left_dock_width_ratio = 0.16f;
            next.right_dock_width_ratio = 0.22f;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Timeline)].visible = false;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Console)].visible = false;
            break;

        case WorkspacePreset::Animation2D:
            next.layout_name = "2D Animation";
            next.focused_panel = WorkspacePanelId::Viewport3D;
            next.bottom_dock_height_ratio = 0.24f;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Timeline)].visible = true;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::AssetBrowser)].visible = false;
            break;

        case WorkspacePreset::VideoEditing:
            next.layout_name = "Video Editing";
            next.focused_panel = WorkspacePanelId::Timeline;
            next.bottom_dock_height_ratio = 0.34f;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::Timeline)].visible = true;
            next.panels[static_cast<std::size_t>(WorkspacePanelId::AssetBrowser)].visible = true;
            break;
    }
    state_ = std::move(next);
    sanitizeFocusAndRatios();
}

core::Status WorkspaceManager::applyPresetByName(std::string_view preset_name) {
    if (preset_name == "default" || preset_name == "Default Studio") {
        applyPreset(WorkspacePreset::DefaultStudio);
        return core::Status::ok();
    }
    if (preset_name == "layout" || preset_name == "Layout") {
        applyPreset(WorkspacePreset::Layout);
        return core::Status::ok();
    }
    if (preset_name == "modeling" || preset_name == "Modeling") {
        applyPreset(WorkspacePreset::Modeling);
        return core::Status::ok();
    }
    if (preset_name == "sculpting" || preset_name == "Sculpting") {
        applyPreset(WorkspacePreset::Sculpting);
        return core::Status::ok();
    }
    if (preset_name == "shading" || preset_name == "Shading") {
        applyPreset(WorkspacePreset::Shading);
        return core::Status::ok();
    }
    if (preset_name == "animation" || preset_name == "animation_prep" || preset_name == "Animation") {
        applyPreset(WorkspacePreset::AnimationPrep);
        return core::Status::ok();
    }
    if (preset_name == "rendering" || preset_name == "Rendering") {
        applyPreset(WorkspacePreset::Rendering);
        return core::Status::ok();
    }
    if (preset_name == "compositing" || preset_name == "Compositing") {
        applyPreset(WorkspacePreset::Compositing);
        return core::Status::ok();
    }
    if (preset_name == "vfx" || preset_name == "VFX") {
        applyPreset(WorkspacePreset::VFX);
        return core::Status::ok();
    }
    if (preset_name == "geometry_nodes" || preset_name == "Geometry Nodes") {
        applyPreset(WorkspacePreset::GeometryNodes);
        return core::Status::ok();
    }
    if (preset_name == "uv_editing" || preset_name == "UV Editing") {
        applyPreset(WorkspacePreset::UvEditing);
        return core::Status::ok();
    }
    if (preset_name == "animation_2d" || preset_name == "2d_animation" || preset_name == "2D Animation") {
        applyPreset(WorkspacePreset::Animation2D);
        return core::Status::ok();
    }
    if (preset_name == "video_editing" || preset_name == "Video Editing") {
        applyPreset(WorkspacePreset::VideoEditing);
        return core::Status::ok();
    }
    return core::Status::fail(
        core::ErrorCode::InvalidArgument,
        "Unknown workspace preset: " + std::string(preset_name));
}

ComputedDockLayout WorkspaceManager::computeDockLayout(
    std::uint32_t window_width,
    std::uint32_t window_height) const noexcept {
    ComputedDockLayout out{};
    const float w = static_cast<float>(std::max<std::uint32_t>(window_width, 1u));
    const float h = static_cast<float>(std::max<std::uint32_t>(window_height, 1u));

    const bool has_left = !visiblePanelsInRegion(DockRegion::Left).empty();
    const bool has_right = !visiblePanelsInRegion(DockRegion::Right).empty();
    const bool has_bottom = !visiblePanelsInRegion(DockRegion::Bottom).empty();
    const bool has_center = !visiblePanelsInRegion(DockRegion::Center).empty();

    const float left_w = has_left ? (w * state_.left_dock_width_ratio) : 0.0f;
    const float right_w = has_right ? (w * state_.right_dock_width_ratio) : 0.0f;
    const float mid_w = std::max(0.0f, w - left_w - right_w);

    const float bottom_h = has_bottom ? (h * state_.bottom_dock_height_ratio) : 0.0f;
    const float top_h = std::max(0.0f, h - bottom_h);

    if (has_left) {
        out.left_dock = PanelRect{0.0f, 0.0f, left_w, h};
    }
    if (has_right) {
        out.right_dock = PanelRect{w - right_w, 0.0f, right_w, h};
    }
    if (has_bottom) {
        out.bottom_dock = PanelRect{left_w, top_h, mid_w, bottom_h};
    }
    if (has_center) {
        out.center_viewport = PanelRect{left_w, 0.0f, mid_w, top_h};
    }
    return out;
}

RoutedInputDecision WorkspaceManager::routePointerEvent(
    float x,
    float y,
    std::uint32_t window_width,
    std::uint32_t window_height,
    bool viewport_capture_active) {
    // Active 3D viewport drags retain exclusive pointer capture until release.
    if (viewport_capture_active && isPanelVisible(WorkspacePanelId::Viewport3D)) {
        state_.focused_panel = WorkspacePanelId::Viewport3D;
        return RoutedInputDecision{RoutedInputTarget::Viewport3D, WorkspacePanelId::Viewport3D};
    }

    const ComputedDockLayout rects = computeDockLayout(window_width, window_height);
    if (isPanelVisible(WorkspacePanelId::Viewport3D) && rects.center_viewport.contains(x, y)) {
        state_.focused_panel = WorkspacePanelId::Viewport3D;
        return RoutedInputDecision{RoutedInputTarget::Viewport3D, WorkspacePanelId::Viewport3D};
    }

    if (rects.left_dock.contains(x, y)) {
        const auto panels = visiblePanelsInRegion(DockRegion::Left);
        if (!panels.empty()) {
            state_.focused_panel = panels.front();
            return RoutedInputDecision{RoutedInputTarget::PanelUI, panels.front()};
        }
    }
    if (rects.right_dock.contains(x, y)) {
        const auto panels = visiblePanelsInRegion(DockRegion::Right);
        if (!panels.empty()) {
            state_.focused_panel = panels.front();
            return RoutedInputDecision{RoutedInputTarget::PanelUI, panels.front()};
        }
    }
    if (rects.bottom_dock.contains(x, y)) {
        const auto panels = visiblePanelsInRegion(DockRegion::Bottom);
        if (!panels.empty()) {
            state_.focused_panel = panels.front();
            return RoutedInputDecision{RoutedInputTarget::PanelUI, panels.front()};
        }
    }

    return RoutedInputDecision{RoutedInputTarget::Ignored, std::nullopt};
}

RoutedInputDecision WorkspaceManager::routeKeyEvent(
    const platform::KeyEvent& event,
    bool text_input_focused) const noexcept {
    // Active text inputs own all keystrokes exclusively — never duplicate into scene tools!
    if (text_input_focused) {
        return RoutedInputDecision{RoutedInputTarget::TextInput, state_.focused_panel};
    }

    const bool cmd_or_ctrl = event.mods.ctrl || event.mods.super;
    if (cmd_or_ctrl && (event.key == platform::KeyCode::Z || event.key == platform::KeyCode::Y)) {
        return RoutedInputDecision{RoutedInputTarget::GlobalEditorShortcut, std::nullopt};
    }

    if (!isPanelVisible(WorkspacePanelId::Viewport3D)) {
        return RoutedInputDecision{RoutedInputTarget::PanelUI, state_.focused_panel};
    }

    switch (event.key) {
        case platform::KeyCode::W:
        case platform::KeyCode::E:
        case platform::KeyCode::R:
        case platform::KeyCode::F:
        case platform::KeyCode::Tab:
        case platform::KeyCode::Digit1:
        case platform::KeyCode::Digit2:
        case platform::KeyCode::Digit3:
        case platform::KeyCode::Digit4:
        case platform::KeyCode::Delete:
        case platform::KeyCode::Backspace:
        case platform::KeyCode::Escape:
            return RoutedInputDecision{RoutedInputTarget::Viewport3D, WorkspacePanelId::Viewport3D};
        default:
            break;
    }

    return RoutedInputDecision{RoutedInputTarget::PanelUI, state_.focused_panel};
}

core::Result<std::string> WorkspaceManager::serializeLayoutToJson(bool pretty) const {
    project::JsonValue root{project::JsonObject{}};
    root.setKey("format", "finaleffect.workspace");
    root.setKey("version", static_cast<std::uint64_t>(1));
    root.setKey("workspace", project::ProjectSerializer::encodeWorkspaceJson(state_, preserved_extra_));
    return project::writeJson(root, pretty);
}

core::Status WorkspaceManager::deserializeLayoutFromJson(std::string_view json_text) {
    auto parsed = project::parseJson(json_text);
    if (!parsed) {
        return parsed.error();
    }
    const auto& root = parsed.value();
    if (!root.isObject()) {
        return core::Status::fail(core::ErrorCode::ValidationError, "Workspace layout root must be a JSON object");
    }

    const project::JsonValue* ws_node = root.findKey("workspace");
    if (ws_node == nullptr) {
        ws_node = &root;
    }

    project::JsonObject extra;
    auto decoded = project::ProjectSerializer::decodeWorkspaceJson(*ws_node, &extra, nullptr);
    if (!decoded) {
        return decoded.error();
    }
    state_ = std::move(decoded.value());
    preserved_extra_ = std::move(extra);
    sanitizeFocusAndRatios();
    return core::Status::ok();
}

core::Status WorkspaceManager::saveLayoutToFile(
    const std::filesystem::path& path,
    platform::IFileSystem& file_system) const {
    auto json_res = serializeLayoutToJson(true);
    if (!json_res) {
        return json_res.error();
    }
    return file_system.writeTextFile(path, json_res.value());
}

core::Status WorkspaceManager::loadLayoutFromFile(
    const std::filesystem::path& path,
    const platform::IFileSystem& file_system) {
    auto text_res = file_system.readTextFile(path);
    if (!text_res) {
        return text_res.error();
    }
    return deserializeLayoutFromJson(text_res.value());
}

void WorkspaceManager::sanitizeFocusAndRatios() noexcept {
    state_.left_dock_width_ratio = std::clamp(state_.left_dock_width_ratio, 0.08f, 0.35f);
    state_.right_dock_width_ratio = std::clamp(state_.right_dock_width_ratio, 0.08f, 0.38f);
    state_.bottom_dock_height_ratio = std::clamp(state_.bottom_dock_height_ratio, 0.08f, 0.45f);

    if (!isPanelVisible(state_.focused_panel)) {
        if (isPanelVisible(WorkspacePanelId::Viewport3D)) {
            state_.focused_panel = WorkspacePanelId::Viewport3D;
        } else {
            for (const auto& p : state_.panels) {
                if (p.visible) {
                    state_.focused_panel = p.id;
                    break;
                }
            }
        }
    }
}

} // namespace finaleffect::app
