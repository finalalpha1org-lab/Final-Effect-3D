// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/app/project_hub_service.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/ui/native_file_dialog.hpp"
#include "finaleffect/ui/native_home_theme.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace finaleffect::ui {

struct ProjectOpenActions {
    std::optional<std::filesystem::path> open_project_path{};
    bool request_in_app_file_picker{false};
};

// Stage 1.1 View D: Native Open Project View (.fe3d + companion .fe3dbin bundle support)
class NativeProjectOpen {
public:
    NativeProjectOpen() = default;

    void setPathInput(std::string path_text) {
        path_input_ = std::move(path_text);
        error_message_.clear();
    }
    [[nodiscard]] const std::string& pathInput() const noexcept { return path_input_; }

    void setErrorMessage(std::string message) { error_message_ = std::move(message); }
    void clearError() noexcept { error_message_.clear(); }
    [[nodiscard]] const std::string& errorMessage() const noexcept { return error_message_; }

    // Invokes native OS file dialog safely; cancellation produces zero error.
    void invokeNativeOpenDialog(
        INativeFileDialog& file_dialog,
        const std::filesystem::path& initial_dir,
        ProjectOpenActions& out_actions);

    void render(
        const NativeHomeTheme& theme,
        const platform::IFileSystem& file_system,
        const app::RecentProjectsIndex& recent_index,
        INativeFileDialog& file_dialog,
        const std::filesystem::path& default_dir,
        ProjectOpenActions& out_actions);

private:
    std::string path_input_{};
    std::string error_message_{};
};

} // namespace finaleffect::ui
