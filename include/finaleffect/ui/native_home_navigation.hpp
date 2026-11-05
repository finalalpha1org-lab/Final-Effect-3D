// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/ui/native_home_theme.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace finaleffect::ui {

enum class HomeHubViewId : std::uint8_t {
    ProjectsStart = 0,
    Templates = 1,
    NewProject = 2,
    OpenProject = 3,
    Recovery = 4,
    Preferences = 5,
    About = 6,
};

struct HomeHubViewDescriptor {
    HomeHubViewId id{HomeHubViewId::ProjectsStart};
    std::string_view slug;
    std::string_view label;
    std::string_view shortcut;
    HubIcon icon{HubIcon::Projects};
};

[[nodiscard]] const std::array<HomeHubViewDescriptor, 7>& allHomeHubViews() noexcept;
[[nodiscard]] std::string_view homeHubViewSlug(HomeHubViewId id) noexcept;
[[nodiscard]] std::string_view homeHubViewLabel(HomeHubViewId id) noexcept;
[[nodiscard]] std::optional<HomeHubViewId> parseHomeHubViewSlug(std::string_view slug) noexcept;

struct HubShellMetrics {
    std::string engine_version{"0.5.0"};
    std::string graphics_backend{"OpenGL 3.3 Core"};
    std::uint32_t ui_scale_percent{100};
    std::size_t recent_count{0};
    std::size_t recovery_count{0};
    bool recovery_available{true};
    bool recovery_unclean_warning{false};
    bool has_active_studio_session{false};
    bool studio_session_dirty{false};
    std::string active_project_name{};
};

struct HubNavigationActions {
    std::optional<HomeHubViewId> navigate_to{};
    std::optional<app::ProjectTemplate> quick_create_template{};
    bool trigger_open_dialog{false};
    bool resume_studio{false};
};

class NativeHomeNavigation {
public:
    NativeHomeNavigation() = default;

    void openCommandPalette() noexcept;
    void closeCommandPalette() noexcept { palette_open_ = false; }
    [[nodiscard]] bool isCommandPaletteOpen() const noexcept { return palette_open_; }

    void drawTopBar(
        const NativeHomeTheme& theme,
        float width,
        float height,
        const HubShellMetrics& metrics,
        HubNavigationActions& out_actions);

    void drawLeftRail(
        const NativeHomeTheme& theme,
        float width,
        float height,
        HomeHubViewId active_view,
        const HubShellMetrics& metrics,
        HubNavigationActions& out_actions) const;

    void drawStatusStrip(
        const NativeHomeTheme& theme,
        float width,
        float height,
        const HubShellMetrics& metrics) const;

    void drawCommandPalette(
        const NativeHomeTheme& theme,
        const HubShellMetrics& metrics,
        HubNavigationActions& out_actions);

private:
    bool palette_open_{false};
    bool focus_palette_input_{false};
    std::string palette_query_{};
    std::size_t palette_selected_idx_{0};
};

} // namespace finaleffect::ui
