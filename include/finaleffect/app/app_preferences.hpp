// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/platform/file_system.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace finaleffect::app {

enum class NavigationProfile : std::uint8_t {
    Fe3dHybrid = 0,
    Blender = 1,
    Maya = 2,
};

[[nodiscard]] std::string_view navigationProfileSlug(NavigationProfile profile) noexcept;
[[nodiscard]] std::optional<NavigationProfile> parseNavigationProfileSlug(std::string_view slug) noexcept;

enum class StartupEnvironment : std::uint8_t {
    HomeHub = 0,
    DefaultScene = 1,
};

[[nodiscard]] std::string_view startupEnvironmentSlug(StartupEnvironment env) noexcept;
[[nodiscard]] std::optional<StartupEnvironment> parseStartupEnvironmentSlug(std::string_view slug) noexcept;

// Stage 1 Application Preferences model.
// Persisted strictly outside .fe3d scene/project documents so opening external projects
// never overwrites user interface scale, startup behavior, keymaps, or navigation settings.
struct AppPreferences {
    std::uint32_t ui_scale_percent{100}; // Validated to 85, 100, 115, or 125
    StartupEnvironment startup_environment{StartupEnvironment::HomeHub};
    std::string default_author{"Final Effect 3D"};
    NavigationProfile navigation_profile{NavigationProfile::Fe3dHybrid};
    std::uint32_t recent_projects_limit{20}; // Validated to 10..50
    bool autosave_enabled{true};
    std::uint32_t autosave_interval_seconds{180}; // Validated to 60..900
    bool show_viewport_hud{true};
    std::vector<std::pair<std::string, std::string>> keymap_overrides{};

    void sanitize() noexcept;
};

class AppPreferencesStore {
public:
    static constexpr std::string_view kFormatIdentifier = "finaleffect.preferences";
    static constexpr std::uint32_t kSchemaVersion = 1;

    [[nodiscard]] static core::Result<std::string> serializeToString(
        const AppPreferences& prefs,
        bool pretty = true);

    [[nodiscard]] static core::Result<AppPreferences> deserializeFromString(std::string_view json_text);

    // Returns clean default AppPreferences when path does not exist yet.
    [[nodiscard]] static core::Result<AppPreferences> loadOrDefault(
        const std::filesystem::path& path,
        const platform::IFileSystem& file_system);

    [[nodiscard]] static core::Status saveToFile(
        const AppPreferences& prefs,
        const std::filesystem::path& path,
        platform::IFileSystem& file_system);
};

} // namespace finaleffect::app
