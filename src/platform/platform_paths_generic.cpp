// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors
// Generic portable path resolution for sandboxed/WebAssembly/embedded hosts.

#include "finaleffect/platform/paths.hpp"

#include <system_error>

namespace finaleffect::platform {

std::string_view hostPlatformName() noexcept {
    return "Generic";
}

core::Result<AppPaths> queryAppPaths(std::string_view app_folder_name) {
    if (app_folder_name.empty()) {
        return core::Result<AppPaths>::fail(
            core::ErrorCode::InvalidArgument,
            "Application folder name must not be empty");
    }

    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (ec) {
        cwd = "/";
    }

    auto temp = std::filesystem::temp_directory_path(ec);
    if (ec) {
        temp = cwd / "tmp";
    }

    const std::filesystem::path root = cwd / ("." + std::string(app_folder_name));
    AppPaths paths;
    paths.working_directory = cwd;
    paths.user_config_directory = root / "config";
    paths.user_data_directory = root / "data";
    paths.user_cache_directory = root / "cache";
    paths.temp_directory = temp / std::filesystem::path(app_folder_name);
    return core::Result<AppPaths>::ok(std::move(paths));
}

} // namespace finaleffect::platform
