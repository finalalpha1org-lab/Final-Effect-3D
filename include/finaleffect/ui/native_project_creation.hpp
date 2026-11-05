// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/app/app_preferences.hpp"
#include "finaleffect/app/project_hub_service.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/ui/native_file_dialog.hpp"
#include "finaleffect/ui/native_home_navigation.hpp"
#include "finaleffect/ui/native_home_theme.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace finaleffect::ui {

struct NewProjectCreateSpec {
    std::string project_name;
    std::string author;
    std::string description;
    app::ProjectTemplate template_id{app::ProjectTemplate::StarterShowcase};
    std::filesystem::path target_path;
    float fps{24.0f};
    bool allow_overwrite{false};
};

struct ProjectCreationActions {
    std::optional<HomeHubViewId> navigate_to{};
    std::optional<NewProjectCreateSpec> create_project{};
    bool request_in_app_folder_picker{false};
};

// Stage 1.1 View C: Validated New Project Creation Flow
class NativeProjectCreation {
public:
    NativeProjectCreation() = default;

    void initializeDefaults(
        const app::AppPreferences& prefs,
        const std::filesystem::path& default_projects_dir);

    void applyTemplateSelection(app::ProjectTemplate tmpl) noexcept { template_id_ = tmpl; }
    [[nodiscard]] app::ProjectTemplate selectedTemplate() const noexcept { return template_id_; }

    void setProjectName(std::string name) {
        project_name_ = std::move(name);
        error_message_.clear();
    }
    [[nodiscard]] const std::string& projectName() const noexcept { return project_name_; }

    void setDestinationDirectory(const std::filesystem::path& dir) {
        destination_dir_ = dir.generic_string();
        error_message_.clear();
    }
    [[nodiscard]] const std::string& destinationDirectory() const noexcept { return destination_dir_; }

    void setExplicitFileName(std::string file_name) {
        explicit_file_name_ = std::move(file_name);
        error_message_.clear();
    }
    [[nodiscard]] const std::string& explicitFileName() const noexcept { return explicit_file_name_; }

    void setAuthor(std::string author) { author_ = std::move(author); }
    [[nodiscard]] const std::string& author() const noexcept { return author_; }

    void setErrorMessage(std::string message) { error_message_ = std::move(message); }
    [[nodiscard]] const std::string& errorMessage() const noexcept { return error_message_; }

    [[nodiscard]] bool isOverwriteConfirmModalOpen() const noexcept { return overwrite_modal_open_; }
    void confirmPendingOverwrite(ProjectCreationActions& out_actions);
    void cancelPendingOverwrite() noexcept { overwrite_modal_open_ = false; }

    [[nodiscard]] std::filesystem::path resolvedTargetPath() const;

    // Validates inputs and either emits create_project (when safe) or opens overwrite confirmation modal.
    bool requestSubmit(
        const platform::IFileSystem& file_system,
        ProjectCreationActions& out_actions);

    void render(
        const NativeHomeTheme& theme,
        const platform::IFileSystem& file_system,
        INativeFileDialog& file_dialog,
        ProjectCreationActions& out_actions);

private:
    std::string project_name_{"Untitled Studio Project"};
    std::string destination_dir_{};
    std::string explicit_file_name_{};
    std::string author_{"Final Effect 3D"};
    std::string description_{};
    app::ProjectTemplate template_id_{app::ProjectTemplate::StarterShowcase};
    int units_index_{0}; // 0 = Meters, 1 = Centimeters, 2 = Millimeters
    int fps_index_{0};   // 0 = 24, 1 = 25, 2 = 30, 3 = 60
    std::string error_message_{};
    bool overwrite_modal_open_{false};
};

} // namespace finaleffect::ui
