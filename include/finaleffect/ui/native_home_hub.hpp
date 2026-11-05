// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/app/app_preferences.hpp"
#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/app/project_hub_service.hpp"
#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/platform/window.hpp"
#include "finaleffect/ui/native_about_view.hpp"
#include "finaleffect/ui/native_file_dialog.hpp"
#include "finaleffect/ui/native_home_navigation.hpp"
#include "finaleffect/ui/native_home_theme.hpp"
#include "finaleffect/ui/native_preferences_view.hpp"
#include "finaleffect/ui/native_project_browser.hpp"
#include "finaleffect/ui/native_project_creation.hpp"
#include "finaleffect/ui/native_project_open.hpp"
#include "finaleffect/ui/native_recovery_view.hpp"
#include "finaleffect/ui/native_template_browser.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace finaleffect::ui {

enum class DesktopMode : std::uint8_t {
    HomeHub = 0,
    Studio = 1,
};

struct NativeHomeHubPaths {
    std::filesystem::path preferences_path;
    std::filesystem::path recent_projects_path;
    std::filesystem::path recovery_directory;
    std::filesystem::path default_projects_directory;
};

// Main Stage 1.1 Native Desktop Home Hub & Studio Lifecycle Orchestrator.
// Reuses existing C++20 engine services (AppPreferencesStore, TemplateCatalog,
// RecentProjectsIndex, RecoveryService, SystemInfo, createNewProjectAtPath, EditorController)
// without duplicating business logic or creating a second window/GL context.
class NativeHomeHub {
public:
    NativeHomeHub(
        platform::IWindow& window,
        platform::IFileSystem& file_system,
        app::StudioSession& session,
        app::EditorController& editor,
        app::AppPreferences& preferences,
        app::RecentProjectsIndex& recent_index,
        app::RecoveryService& recovery_service,
        NativeHomeHubPaths paths,
        std::unique_ptr<INativeFileDialog> file_dialog = nullptr);

    ~NativeHomeHub();

    NativeHomeHub(const NativeHomeHub&) = delete;
    NativeHomeHub& operator=(const NativeHomeHub&) = delete;

    // Initializes ImGui context (if none active) and configures theme & default view state.
    [[nodiscard]] core::Status initialize(DesktopMode initial_mode = DesktopMode::HomeHub);
    void shutdown() noexcept;

    [[nodiscard]] DesktopMode mode() const noexcept { return mode_; }
    [[nodiscard]] HomeHubViewId activeView() const noexcept { return active_view_; }
    [[nodiscard]] bool hasActiveStudioSession() const noexcept { return has_active_studio_session_; }
    [[nodiscard]] const std::string& globalErrorMessage() const noexcept { return global_error_; }
    [[nodiscard]] bool isDirtySessionModalOpen() const noexcept { return dirty_guard_modal_open_; }
    [[nodiscard]] bool isExitConfirmModalOpen() const noexcept { return exit_confirm_modal_open_; }

    void navigateTo(HomeHubViewId view) noexcept;
    void markStudioSessionActive() noexcept;
    void enterStudioMode() noexcept;
    void returnToHomeHub() noexcept;

    // Direct programmatic & UI-shared actions (all subject to validation & unsaved-work protection).
    [[nodiscard]] core::Status executeCreateProject(
        const NewProjectCreateSpec& spec,
        bool bypass_dirty_guard = false);
    [[nodiscard]] core::Status executeQuickCreateTemplate(
        app::ProjectTemplate tmpl,
        bool bypass_dirty_guard = false);
    [[nodiscard]] core::Status executeOpenProject(
        const std::filesystem::path& path,
        bool bypass_dirty_guard = false);
    [[nodiscard]] core::Status executeRestoreRecovery(
        std::string_view snapshot_id,
        bool bypass_dirty_guard = false);
    [[nodiscard]] core::Status saveActiveStudioProject();

    void confirmDiscardDirtyAndContinue();
    void confirmSaveDirtyAndContinue();
    void cancelDirtySessionGuard() noexcept;

    // Intercepts window close requests when unsaved Studio work exists (returns true if close is blocked).
    bool interceptWindowCloseIfDirty();
    void confirmExitWithoutSaving() noexcept;
    void confirmSaveAndExit();
    void cancelExitConfirmation() noexcept { exit_confirm_modal_open_ = false; }

    // Feeds platform::InputEvent queue into ImGuiIO when running without ImGui_ImplGlfw (headless/CTest).
    void feedHeadlessInputEvents(std::span<const platform::InputEvent> events);

    // Pre-NewFrame scale & TrueType font atlas synchronization (safe to call before ImGui_ImplOpenGL3_NewFrame).
    // Returns true when the font atlas was rebuilt and GPU font textures should be refreshed.
    [[nodiscard]] bool prepareFrameScale();

    // Renders active Native Home Hub or Studio return overlay inside an open ImGui::NewFrame()..ImGui::Render() scope.
    void renderCurrentFrameContent();

    // Convenience helper for headless/software hosts: runs prepareFrameScale + NewFrame + renderCurrentFrameContent + Render.
    bool buildUiFrame(float delta_seconds);

    // Periodic dirty-session autosave tick.
    void tickPeriodicAutosave(std::uint64_t now_epoch_ms);

    // Sub-module accessors for inspection & automated tests
    [[nodiscard]] const NativeHomeTheme& theme() const noexcept { return theme_; }
    [[nodiscard]] NativeHomeNavigation& navigation() noexcept { return navigation_; }
    [[nodiscard]] NativeProjectBrowser& projectBrowser() noexcept { return project_browser_; }
    [[nodiscard]] NativeTemplateBrowser& templateBrowser() noexcept { return template_browser_; }
    [[nodiscard]] NativeProjectCreation& projectCreation() noexcept { return project_creation_; }
    [[nodiscard]] NativeProjectOpen& projectOpen() noexcept { return project_open_; }
    [[nodiscard]] NativeRecoveryView& recoveryView() noexcept { return recovery_view_; }
    [[nodiscard]] NativePreferencesView& preferencesView() noexcept { return preferences_view_; }
    [[nodiscard]] NativeAboutView& aboutView() noexcept { return about_view_; }
    [[nodiscard]] INativeFileDialog& fileDialog() noexcept { return *file_dialog_; }

private:
    void renderHomeHubCanvas();
    void renderStudioOverlay();
    void renderDirtySessionGuardModal();
    void renderExitConfirmModal();
    void renderInAppFileBrowserModal();
    void handleGlobalKeyboardShortcuts();
    void updateWindowTitle();
    void recordCurrentDocumentInRecents(const std::filesystem::path& file_path);
    [[nodiscard]] HubShellMetrics buildShellMetrics() const;
    [[nodiscard]] static std::uint64_t nowEpochMs() noexcept;

    platform::IWindow& window_;
    platform::IFileSystem& file_system_;
    app::StudioSession& session_;
    app::EditorController& editor_;
    app::AppPreferences& preferences_;
    app::RecentProjectsIndex& recent_index_;
    app::RecoveryService& recovery_service_;
    NativeHomeHubPaths paths_;
    std::unique_ptr<INativeFileDialog> file_dialog_;

    bool owns_imgui_context_{false};
    bool initialized_{false};
    DesktopMode mode_{DesktopMode::HomeHub};
    HomeHubViewId active_view_{HomeHubViewId::ProjectsStart};
    bool has_active_studio_session_{false};
    bool allow_close_with_unsaved_{false};
    std::string global_error_{};

    // Dirty-session replacement guard state
    bool dirty_guard_modal_open_{false};
    std::function<void()> pending_dirty_continuation_{};

    // Window close confirmation modal state
    bool exit_confirm_modal_open_{false};

    // Built-in fallback file/folder navigator modal state
    bool in_app_browser_open_{false};
    bool in_app_browser_pick_dir_{false};
    std::filesystem::path in_app_browser_dir_{};
    std::string in_app_browser_selected_file_{};

    // Autosave tracking
    std::uint64_t last_autosave_epoch_ms_{0};
    std::uint64_t last_autosave_state_token_{0};
    std::uint32_t quick_create_counter_{1};

    // Modular Native UI Subsystems
    NativeHomeTheme theme_{};
    NativeHomeNavigation navigation_{};
    NativeProjectBrowser project_browser_{};
    NativeTemplateBrowser template_browser_{};
    NativeProjectCreation project_creation_{};
    NativeProjectOpen project_open_{};
    NativeRecoveryView recovery_view_{};
    NativePreferencesView preferences_view_{};
    NativeAboutView about_view_{};
};

} // namespace finaleffect::ui
