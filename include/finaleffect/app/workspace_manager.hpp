// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/platform/input.hpp"
#include "finaleffect/project/json.hpp"
#include "finaleffect/project/workspace_state.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace finaleffect::app {

enum class WorkspacePreset : std::uint8_t {
    DefaultStudio = 0,
    Modeling = 1,
    Shading = 2,
    AnimationPrep = 3,
};

struct PanelRect {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};

    [[nodiscard]] bool contains(float px, float py) const noexcept {
        return width > 0.0f && height > 0.0f &&
               px >= x && px < (x + width) &&
               py >= y && py < (y + height);
    }
};

struct ComputedDockLayout {
    PanelRect left_dock{};
    PanelRect right_dock{};
    PanelRect bottom_dock{};
    PanelRect center_viewport{};
};

enum class RoutedInputTarget : std::uint8_t {
    Ignored = 0,
    Viewport3D = 1,
    PanelUI = 2,
    TextInput = 3,
    GlobalEditorShortcut = 4,
};

struct RoutedInputDecision {
    RoutedInputTarget target{RoutedInputTarget::Ignored};
    std::optional<project::WorkspacePanelId> target_panel;
};

// Portable, UI-toolkit-independent desktop/web studio workspace & docking controller.
class WorkspaceManager {
public:
    WorkspaceManager();
    explicit WorkspaceManager(project::WorkspaceState initial_state);

    [[nodiscard]] const project::WorkspaceState& state() const noexcept { return state_; }
    void setState(project::WorkspaceState new_state);

    // Panel visibility, focus, and docking controls.
    [[nodiscard]] bool isPanelVisible(project::WorkspacePanelId id) const noexcept;
    void setPanelVisible(project::WorkspacePanelId id, bool visible);
    void togglePanelVisible(project::WorkspacePanelId id);

    [[nodiscard]] project::WorkspacePanelId focusedPanel() const noexcept { return state_.focused_panel; }
    void setFocusedPanel(project::WorkspacePanelId id) noexcept;

    void setPanelDock(
        project::WorkspacePanelId id,
        project::DockRegion region,
        std::uint32_t tab_order = 0) noexcept;

    void setDockRatios(float left_width, float right_width, float bottom_height) noexcept;

    [[nodiscard]] std::vector<project::WorkspacePanelId> visiblePanelsInRegion(
        project::DockRegion region) const;

    void applyPreset(WorkspacePreset preset);
    [[nodiscard]] core::Status applyPresetByName(std::string_view preset_name);

    // Computes non-overlapping dock zone rectangles for a given window size.
    [[nodiscard]] ComputedDockLayout computeDockLayout(
        std::uint32_t window_width,
        std::uint32_t window_height) const noexcept;

    // Deterministic, non-duplicating input routing across 3D Viewport, Panels, and Text Inputs.
    [[nodiscard]] RoutedInputDecision routePointerEvent(
        float x,
        float y,
        std::uint32_t window_width,
        std::uint32_t window_height,
        bool viewport_capture_active);

    [[nodiscard]] RoutedInputDecision routeKeyEvent(
        const platform::KeyEvent& event,
        bool text_input_focused) const noexcept;

    // Standalone JSON layout persistence (preserves unknown extension fields).
    [[nodiscard]] core::Result<std::string> serializeLayoutToJson(bool pretty = true) const;
    [[nodiscard]] core::Status deserializeLayoutFromJson(std::string_view json_text);

    [[nodiscard]] core::Status saveLayoutToFile(
        const std::filesystem::path& path,
        platform::IFileSystem& file_system) const;

    [[nodiscard]] core::Status loadLayoutFromFile(
        const std::filesystem::path& path,
        const platform::IFileSystem& file_system);

private:
    void sanitizeFocusAndRatios() noexcept;

    project::WorkspaceState state_{};
    project::JsonObject preserved_extra_{};
};

} // namespace finaleffect::app
