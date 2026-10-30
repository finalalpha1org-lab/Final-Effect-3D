// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"

#include <filesystem>
#include <string_view>

namespace finaleffect::platform {

struct AppPaths {
    std::filesystem::path working_directory;
    std::filesystem::path user_config_directory;
    std::filesystem::path user_data_directory;
    std::filesystem::path user_cache_directory;
    std::filesystem::path temp_directory;
};

// Resolved by an OS-isolated translation unit selected in CMake (platform/linux,
// platform/windows, platform/macos, or generic fallback) with zero #ifdefs in engine code.
[[nodiscard]] core::Result<AppPaths> queryAppPaths(std::string_view app_folder_name = "finaleffect3d");
[[nodiscard]] std::string_view hostPlatformName() noexcept;

} // namespace finaleffect::platform
