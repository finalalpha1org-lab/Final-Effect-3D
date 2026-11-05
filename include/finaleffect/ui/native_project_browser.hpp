// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/app/project_hub_service.hpp"
#include "finaleffect/ui/native_home_navigation.hpp"
#include "finaleffect/ui/native_home_theme.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace finaleffect::ui {

enum class ProjectLayoutMode : std::uint8_t {
    List = 0,
    Grid = 1,
};

enum class RecentDiskFilter : std::uint8_t {
    All = 0,
    PinnedOnly = 1,
    OnDiskOnly = 2,
};

struct ProjectBrowserActions {
    std::optional<HomeHubViewId> navigate_to{};
    std::optional<app::ProjectTemplate> quick_create_template{};
    std::optional<std::filesystem::path> reopen_project_path{};
    bool trigger_open_dialog{false};
    bool resume_studio{false};
    bool recent_index_mutated{false};
};

// Stage 1.1 View A: Projects & Start
class NativeProjectBrowser {
public:
    NativeProjectBrowser() = default;

    void setSearchQuery(std::string query) { search_query_ = std::move(query); }
    [[nodiscard]] const std::string& searchQuery() const noexcept { return search_query_; }

    void setSortMode(app::RecentSortMode mode) noexcept { sort_mode_ = mode; }
    [[nodiscard]] app::RecentSortMode sortMode() const noexcept { return sort_mode_; }

    void setDiskFilter(RecentDiskFilter filter) noexcept { disk_filter_ = filter; }
    [[nodiscard]] RecentDiskFilter diskFilter() const noexcept { return disk_filter_; }

    void setLayoutMode(ProjectLayoutMode mode) noexcept { layout_mode_ = mode; }
    [[nodiscard]] ProjectLayoutMode layoutMode() const noexcept { return layout_mode_; }

    void render(
        const NativeHomeTheme& theme,
        app::RecentProjectsIndex& recent_index,
        const app::RecoveryService& recovery_service,
        const HubShellMetrics& shell_metrics,
        ProjectBrowserActions& out_actions);

private:
    std::string search_query_{};
    app::RecentSortMode sort_mode_{app::RecentSortMode::LastOpenedDesc};
    RecentDiskFilter disk_filter_{RecentDiskFilter::All};
    ProjectLayoutMode layout_mode_{ProjectLayoutMode::List};
};

} // namespace finaleffect::ui
