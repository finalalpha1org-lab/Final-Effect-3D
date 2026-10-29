// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors
// macOS-specific application path resolution (~/Library/Application Support & Caches).

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
    return "macOS";
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
        temp = "/tmp";
    }

    const std::filesystem::path home = envPathOrFallback("HOME", temp);
    const std::filesystem::path app_support = home / "Library" / "Application Support" / std::filesystem::path(app_folder_name);

    AppPaths paths;
    paths.working_directory = cwd;
    paths.user_config_directory = app_support / "Config";
    paths.user_data_directory = app_support / "Data";
    paths.user_cache_directory = home / "Library" / "Caches" / std::filesystem::path(app_folder_name);
    paths.temp_directory = temp / std::filesystem::path(app_folder_name);
    return core::Result<AppPaths>::ok(std::move(paths));
}

} // namespace finaleffect::platform
