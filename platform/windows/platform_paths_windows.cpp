// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors
// Windows-specific application path resolution (%APPDATA% / %LOCALAPPDATA%).

#include "finaleffect/platform/paths.hpp"

#include <cstdlib>
#include <system_error>

namespace finaleffect::platform {

namespace {
std::filesystem::path envPathOrFallback(const char* env_name, const std::filesystem::path& fallback) {
    if (const char* val = std::getenv(env_name); val != nullptr && val[0] != '\0') {
        return std::filesystem::path(val);
    }
    return fallback;
}
} // namespace

std::string_view hostPlatformName() noexcept {
    return "Windows";
}

core::Result<AppPaths> queryAppPaths(std::string_view app_folder_name) {
    if (app_folder_name.empty()) {
        return core::Result<AppPaths>::fail(
            core::ErrorCode::InvalidArgument,
            "Application folder name must not be empty");
    }

    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (ec) {
        return core::Result<AppPaths>::fail(
            core::ErrorCode::IoError,
            "Failed to determine working directory: " + ec.message());
    }

    auto temp = std::filesystem::temp_directory_path(ec);
    if (ec) {
        temp = cwd / "temp";
    }

    const std::filesystem::path user_profile = envPathOrFallback("USERPROFILE", temp);
    const std::filesystem::path roaming = envPathOrFallback("APPDATA", user_profile / "AppData" / "Roaming");
    const std::filesystem::path local = envPathOrFallback("LOCALAPPDATA", user_profile / "AppData" / "Local");
    const std::filesystem::path folder(app_folder_name);

    AppPaths paths;
    paths.working_directory = cwd;
    paths.user_config_directory = roaming / folder / "Config";
    paths.user_data_directory = roaming / folder / "Data";
    paths.user_cache_directory = local / folder / "Cache";
    paths.temp_directory = temp / folder;
    return core::Result<AppPaths>::ok(std::move(paths));
}

} // namespace finaleffect::platform
