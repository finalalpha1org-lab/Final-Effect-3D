// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/ui/native_home_hub.hpp"
#include "finaleffect/core/version.hpp"
#include "finaleffect/project/project.hpp"

#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <system_error>

namespace finaleffect::ui {

namespace {

ImGuiKey mapPlatformKeyToImGui(platform::KeyCode key) noexcept {
    using platform::KeyCode;
    switch (key) {
        case KeyCode::Escape: return ImGuiKey_Escape;
        case KeyCode::Enter: return ImGuiKey_Enter;
        case KeyCode::Delete: return ImGuiKey_Delete;
        case KeyCode::Backspace: return ImGuiKey_Backspace;
        case KeyCode::Space: return ImGuiKey_Space;
        case KeyCode::Up: return ImGuiKey_UpArrow;
        case KeyCode::Down: return ImGuiKey_DownArrow;
        case KeyCode::Left: return ImGuiKey_LeftArrow;
        case KeyCode::Right: return ImGuiKey_RightArrow;
        case KeyCode::Home: return ImGuiKey_Home;
        case KeyCode::End: return ImGuiKey_End;
        case KeyCode::Tab: return ImGuiKey_Tab;
        case KeyCode::A: return ImGuiKey_A;
        case KeyCode::C: return ImGuiKey_C;
        case KeyCode::D: return ImGuiKey_D;
        case KeyCode::E: return ImGuiKey_E;
        case KeyCode::F: return ImGuiKey_F;
        case KeyCode::G: return ImGuiKey_G;
        case KeyCode::H: return ImGuiKey_H;
        case KeyCode::K: return ImGuiKey_K;
        case KeyCode::N: return ImGuiKey_N;
        case KeyCode::O: return ImGuiKey_O;
        case KeyCode::P: return ImGuiKey_P;
        case KeyCode::Q: return ImGuiKey_Q;
        case KeyCode::R: return ImGuiKey_R;
        case KeyCode::S: return ImGuiKey_S;
        case KeyCode::T: return ImGuiKey_T;
        case KeyCode::V: return ImGuiKey_V;
        case KeyCode::W: return ImGuiKey_W;
        case KeyCode::X: return ImGuiKey_X;
        case KeyCode::Y: return ImGuiKey_Y;
        case KeyCode::Z: return ImGuiKey_Z;
        case KeyCode::Digit1: return ImGuiKey_1;
        case KeyCode::Digit2: return ImGuiKey_2;
        case KeyCode::Digit3: return ImGuiKey_3;
        case KeyCode::Digit4: return ImGuiKey_4;
        case KeyCode::Digit5: return ImGuiKey_5;
        case KeyCode::Digit6: return ImGuiKey_6;
        case KeyCode::Digit7: return ImGuiKey_7;
        case KeyCode::F1: return ImGuiKey_F1;
        case KeyCode::F2: return ImGuiKey_F2;
        case KeyCode::F3: return ImGuiKey_F3;
        case KeyCode::F8: return ImGuiKey_F8;
        case KeyCode::F9: return ImGuiKey_F9;
        case KeyCode::F10: return ImGuiKey_F10;
        case KeyCode::F11: return ImGuiKey_F11;
        case KeyCode::F12: return ImGuiKey_F12;
        case KeyCode::LeftShift: return ImGuiKey_LeftShift;
        case KeyCode::LeftControl: return ImGuiKey_LeftCtrl;
        case KeyCode::LeftAlt: return ImGuiKey_LeftAlt;
        case KeyCode::Unknown: break;
    }
    return ImGuiKey_None;
}

std::uint64_t measureProjectFileBytes(
    const std::filesystem::path& path,
    const platform::IFileSystem& fs) {
    if (auto bin = fs.readBinaryFile(path); bin) {
        return static_cast<std::uint64_t>(bin.value().size());
    }
    if (auto txt = fs.readTextFile(path); txt) {
        return static_cast<std::uint64_t>(txt.value().size());
    }
    return 0ULL;
}

} // namespace

NativeHomeHub::NativeHomeHub(
    platform::IWindow& window,
    platform::IFileSystem& file_system,
    app::StudioSession& session,
    app::EditorController& editor,
    app::AppPreferences& preferences,
    app::RecentProjectsIndex& recent_index,
    app::RecoveryService& recovery_service,
    NativeHomeHubPaths paths,
    std::unique_ptr<INativeFileDialog> file_dialog)
    : window_(window),
      file_system_(file_system),
      session_(session),
      editor_(editor),
      preferences_(preferences),
      recent_index_(recent_index),
      recovery_service_(recovery_service),
      paths_(std::move(paths)),
      file_dialog_(file_dialog ? std::move(file_dialog) : std::make_unique<SystemNativeFileDialog>()) {}

NativeHomeHub::~NativeHomeHub() {
    shutdown();
}

std::uint64_t NativeHomeHub::nowEpochMs() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

core::Status NativeHomeHub::initialize(DesktopMode initial_mode) {
    if (initialized_) {
        return core::Status::ok();
    }

    if (ImGui::GetCurrentContext() == nullptr) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        owns_imgui_context_ = true;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // Never write stray imgui.ini files into user working directories!
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    const auto fb = window_.framebufferSize();
    io.DisplaySize = ImVec2(
        static_cast<float>(std::max(1u, fb.width)),
        static_cast<float>(std::max(1u, fb.height)));

    (void)theme_.syncScaleAndStyle(preferences_.ui_scale_percent, window_.contentScale());
    recent_index_.refreshDiskStatus(file_system_);

    project_creation_.initializeDefaults(preferences_, paths_.default_projects_directory);
    preferences_view_.syncFromPreferences(preferences_);

    mode_ = initial_mode;
    has_active_studio_session_ = (initial_mode == DesktopMode::Studio);
    last_autosave_epoch_ms_ = nowEpochMs();
    last_autosave_state_token_ = session_.commandStack().currentStateToken();
    initialized_ = true;

    updateWindowTitle();
    return core::Status::ok();
}

void NativeHomeHub::shutdown() noexcept {
    if (!initialized_) {
        return;
    }
    if (owns_imgui_context_ && ImGui::GetCurrentContext() != nullptr) {
        ImGui::DestroyContext();
        owns_imgui_context_ = false;
    }
    initialized_ = false;
}

void NativeHomeHub::navigateTo(HomeHubViewId view) noexcept {
    active_view_ = view;
    mode_ = DesktopMode::HomeHub;
    global_error_.clear();
    if (view == HomeHubViewId::ProjectsStart || view == HomeHubViewId::OpenProject) {
        recent_index_.refreshDiskStatus(file_system_);
    } else if (view == HomeHubViewId::Preferences) {
        preferences_view_.syncFromPreferences(preferences_);
    }
    updateWindowTitle();
}

void NativeHomeHub::markStudioSessionActive() noexcept {
    has_active_studio_session_ = true;
}

void NativeHomeHub::enterStudioMode() noexcept {
    has_active_studio_session_ = true;
    mode_ = DesktopMode::Studio;
    global_error_.clear();
    navigation_.closeCommandPalette();
    updateWindowTitle();
}

void NativeHomeHub::returnToHomeHub() noexcept {
    session_.syncDocumentState(editor_.selectedMaterial());
    recent_index_.refreshDiskStatus(file_system_);
    mode_ = DesktopMode::HomeHub;
    active_view_ = HomeHubViewId::ProjectsStart;
    updateWindowTitle();
}

void NativeHomeHub::updateWindowTitle() {
    if (mode_ == DesktopMode::HomeHub) {
        const std::string title =
            "Final Effect 3D — " + std::string(homeHubViewLabel(active_view_));
        window_.setTitle(title);
    } else {
        std::string proj_name = session_.document().metadata.project_name;
        if (proj_name.empty()) {
            proj_name = "Untitled Scene";
        }
        const std::string dirty_mark = editor_.isDocumentDirty() ? " *" : "";
        window_.setTitle("Final Effect 3D — " + proj_name + dirty_mark + " [3D Studio]");
    }
}

void NativeHomeHub::recordCurrentDocumentInRecents(const std::filesystem::path& file_path) {
    if (file_path.empty()) return;
    recent_index_.recordProject(
        session_.document(),
        file_path,
        measureProjectFileBytes(file_path, file_system_),
        nowEpochMs(),
        preferences_.recent_projects_limit);
    recent_index_.refreshDiskStatus(file_system_);
    if (!paths_.recent_projects_path.empty()) {
        (void)recent_index_.saveToFile(paths_.recent_projects_path, file_system_);
    }
}

core::Status NativeHomeHub::executeCreateProject(
    const NewProjectCreateSpec& spec,
    bool bypass_dirty_guard) {
    if (!bypass_dirty_guard && has_active_studio_session_ && editor_.isDocumentDirty()) {
        const NewProjectCreateSpec captured = spec;
        pending_dirty_continuation_ = [this, captured]() {
            (void)executeCreateProject(captured, true);
        };
        dirty_guard_modal_open_ = true;
        return core::Status::ok();
    }

    auto st = app::createNewProjectAtPath(
        editor_,
        spec.template_id,
        spec.project_name,
        spec.author,
        spec.target_path,
        spec.allow_overwrite,
        file_system_);
    if (!st) {
        project_creation_.setErrorMessage(st.error().message);
        global_error_ = st.error().message;
        return st;
    }

    if (!spec.description.empty() || std::fabs(spec.fps - 24.0f) > 0.01f) {
        session_.document().metadata.description = spec.description;
        auto tl = session_.document().scene.timeline();
        tl.fps = spec.fps;
        session_.document().scene.setTimeline(tl);
        session_.syncDocumentState(editor_.selectedMaterial());
        (void)session_.saveProjectToFile(spec.target_path, file_system_);
        editor_.markDocumentSaved();
    }

    recordCurrentDocumentInRecents(spec.target_path);
    last_autosave_state_token_ = session_.commandStack().currentStateToken();
    enterStudioMode();
    return core::Status::ok();
}

core::Status NativeHomeHub::executeQuickCreateTemplate(
    app::ProjectTemplate tmpl,
    bool bypass_dirty_guard) {
    if (!bypass_dirty_guard && has_active_studio_session_ && editor_.isDocumentDirty()) {
        pending_dirty_continuation_ = [this, tmpl]() {
            (void)executeQuickCreateTemplate(tmpl, true);
        };
        dirty_guard_modal_open_ = true;
        return core::Status::ok();
    }

    const auto& desc = app::TemplateCatalog::describe(tmpl);
    const std::filesystem::path base_dir = paths_.default_projects_directory.empty()
        ? std::filesystem::current_path()
        : paths_.default_projects_directory;
    (void)file_system_.createDirectories(base_dir);

    std::filesystem::path candidate;
    std::string proj_title;
    for (std::uint32_t attempt = 0; attempt < 200u; ++attempt) {
        const std::uint32_t n = quick_create_counter_++;
        const std::string suffix = (n == 1u) ? std::string("") : ("_" + std::to_string(n));
        candidate = (base_dir / (std::string(desc.slug) + suffix + ".fe3d")).lexically_normal();
        proj_title = std::string(desc.title) + (n == 1u ? "" : (" " + std::to_string(n)));
        if (!file_system_.exists(candidate)) {
            break;
        }
    }

    NewProjectCreateSpec spec{
        .project_name = proj_title,
        .author = preferences_.default_author.empty() ? std::string("Final Effect 3D") : preferences_.default_author,
        .description = std::string(desc.summary),
        .template_id = tmpl,
        .target_path = candidate,
        .fps = 24.0f,
        .allow_overwrite = true,
    };
    return executeCreateProject(spec, true);
}

core::Status NativeHomeHub::executeOpenProject(
    const std::filesystem::path& path,
    bool bypass_dirty_guard) {
    if (path.empty()) {
        const std::string msg = "Project file path cannot be empty.";
        project_open_.setErrorMessage(msg);
        return core::Status::fail(core::ErrorCode::InvalidArgument, msg);
    }

    if (!bypass_dirty_guard && has_active_studio_session_ && editor_.isDocumentDirty()) {
        const std::filesystem::path captured = path;
        pending_dirty_continuation_ = [this, captured]() {
            (void)executeOpenProject(captured, true);
        };
        dirty_guard_modal_open_ = true;
        return core::Status::ok();
    }

    auto load_st = editor_.loadProjectFile(path, file_system_);
    if (!load_st) {
        std::string diag = load_st.error().message;
        if (!load_st.error().context.empty()) {
            diag += " (" + load_st.error().context + ")";
        }
        project_open_.setErrorMessage(diag);
        global_error_ = diag;
        return load_st;
    }

    project_open_.clearError();
    global_error_.clear();
    recordCurrentDocumentInRecents(path);
    last_autosave_state_token_ = session_.commandStack().currentStateToken();
    enterStudioMode();
    return core::Status::ok();
}

core::Status NativeHomeHub::executeRestoreRecovery(
    std::string_view snapshot_id,
    bool bypass_dirty_guard) {
    if (!bypass_dirty_guard && has_active_studio_session_ && editor_.isDocumentDirty()) {
        const std::string id_copy(snapshot_id);
        pending_dirty_continuation_ = [this, id_copy]() {
            (void)executeRestoreRecovery(id_copy, true);
        };
        dirty_guard_modal_open_ = true;
        return core::Status::ok();
    }

    auto recovered = recovery_service_.recoverSnapshot(snapshot_id, file_system_);
    if (!recovered) {
        recovery_view_.setStatusBanner(recovered.error().message, TagTone::Danger);
        return recovered.error();
    }
    auto serialized = project::ProjectSerializer::serializeToString(recovered.value(), false);
    if (!serialized) {
        recovery_view_.setStatusBanner(serialized.error().message, TagTone::Danger);
        return serialized.error();
    }
    auto imp_st = editor_.importInlineProjectJson(serialized.value());
    if (!imp_st) {
        recovery_view_.setStatusBanner(imp_st.error().message, TagTone::Danger);
        return imp_st;
    }

    recovery_view_.clearBanner();
    last_autosave_state_token_ = session_.commandStack().currentStateToken();
    enterStudioMode();
    return core::Status::ok();
}

core::Status NativeHomeHub::saveActiveStudioProject() {
    std::filesystem::path target_path;
    if (!editor_.activeProjectPath().empty()) {
        target_path = std::filesystem::path(editor_.activeProjectPath());
    } else {
        const std::filesystem::path base_dir = paths_.default_projects_directory.empty()
            ? std::filesystem::current_path()
            : paths_.default_projects_directory;
        (void)file_system_.createDirectories(base_dir);
        target_path = (base_dir / "untitled_studio_scene.fe3d").lexically_normal();
    }

    session_.syncDocumentState(editor_.selectedMaterial());
    auto save_st = session_.saveProjectToFile(target_path, file_system_);
    if (!save_st) {
        global_error_ = save_st.error().message;
        return save_st;
    }
    editor_.setActiveProjectPath(target_path.generic_string());
    editor_.markDocumentSaved();
    recordCurrentDocumentInRecents(target_path);
    last_autosave_state_token_ = session_.commandStack().currentStateToken();
    updateWindowTitle();
    return core::Status::ok();
}

void NativeHomeHub::confirmDiscardDirtyAndContinue() {
    dirty_guard_modal_open_ = false;
    auto fn = std::move(pending_dirty_continuation_);
    pending_dirty_continuation_ = nullptr;
    if (fn) {
        fn();
    }
}

void NativeHomeHub::confirmSaveDirtyAndContinue() {
    if (!saveActiveStudioProject()) {
        dirty_guard_modal_open_ = false;
        return;
    }
    confirmDiscardDirtyAndContinue();
}

void NativeHomeHub::cancelDirtySessionGuard() noexcept {
    dirty_guard_modal_open_ = false;
    pending_dirty_continuation_ = nullptr;
}

bool NativeHomeHub::interceptWindowCloseIfDirty() {
    if (allow_close_with_unsaved_) {
        return false;
    }
    if (window_.shouldClose() && has_active_studio_session_ && editor_.isDocumentDirty()) {
        window_.cancelCloseRequest();
        exit_confirm_modal_open_ = true;
        return true;
    }
    return false;
}

void NativeHomeHub::confirmExitWithoutSaving() noexcept {
    exit_confirm_modal_open_ = false;
    allow_close_with_unsaved_ = true;
    window_.requestClose();
}

void NativeHomeHub::confirmSaveAndExit() {
    if (saveActiveStudioProject()) {
        exit_confirm_modal_open_ = false;
        allow_close_with_unsaved_ = true;
        window_.requestClose();
    }
}

void NativeHomeHub::feedHeadlessInputEvents(std::span<const platform::InputEvent> events) {
    ImGuiIO& io = ImGui::GetIO();
    for (const auto& ev : events) {
        if (const auto* mv = std::get_if<platform::CursorMoveEvent>(&ev)) {
            io.AddMousePosEvent(static_cast<float>(mv->x), static_cast<float>(mv->y));
        } else if (const auto* mb = std::get_if<platform::MouseButtonEvent>(&ev)) {
            io.AddMousePosEvent(static_cast<float>(mb->x), static_cast<float>(mb->y));
            int btn = 0;
            if (mb->button == platform::MouseButton::Right) btn = 1;
            else if (mb->button == platform::MouseButton::Middle) btn = 2;
            io.AddMouseButtonEvent(btn, mb->action != platform::KeyAction::Release);
        } else if (const auto* sc = std::get_if<platform::ScrollEvent>(&ev)) {
            io.AddMouseWheelEvent(static_cast<float>(sc->delta_x), static_cast<float>(sc->delta_y));
        } else if (const auto* txt = std::get_if<platform::TextInputEvent>(&ev)) {
            io.AddInputCharacter(txt->codepoint);
        } else if (const auto* ke = std::get_if<platform::KeyEvent>(&ev)) {
            const bool down = (ke->action != platform::KeyAction::Release);
            io.AddKeyEvent(ImGuiMod_Shift, ke->mods.shift);
            io.AddKeyEvent(ImGuiMod_Ctrl, ke->mods.ctrl);
            io.AddKeyEvent(ImGuiMod_Alt, ke->mods.alt);
            io.AddKeyEvent(ImGuiMod_Super, ke->mods.super);
            const ImGuiKey mapped = mapPlatformKeyToImGui(ke->key);
            if (mapped != ImGuiKey_None) {
                io.AddKeyEvent(mapped, down);
            }
        }
    }
}

bool NativeHomeHub::prepareFrameScale() {
    return theme_.syncScaleAndStyle(preferences_.ui_scale_percent, window_.contentScale());
}

void NativeHomeHub::renderCurrentFrameContent() {
    theme_.pushBodyFont();

    handleGlobalKeyboardShortcuts();

    if (mode_ == DesktopMode::HomeHub) {
        renderHomeHubCanvas();
    } else {
        renderStudioOverlay();
    }

    renderDirtySessionGuardModal();
    renderExitConfirmModal();
    renderInAppFileBrowserModal();

    theme_.popFont();
}

bool NativeHomeHub::buildUiFrame(float delta_seconds) {
    ImGuiIO& io = ImGui::GetIO();
    const auto fb = window_.framebufferSize();
    io.DisplaySize = ImVec2(
        static_cast<float>(std::max(1u, fb.width)),
        static_cast<float>(std::max(1u, fb.height)));
    io.DeltaTime = std::max(1.0e-4f, delta_seconds);

    const bool font_rebuilt = prepareFrameScale();

    ImGui::NewFrame();
    renderCurrentFrameContent();
    ImGui::Render();
    return font_rebuilt;
}

void NativeHomeHub::handleGlobalKeyboardShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (dirty_guard_modal_open_ || exit_confirm_modal_open_ || in_app_browser_open_) {
        return;
    }

    // Accept both Ctrl and Super (macOS Cmd; ImGui swaps Ctrl/Super when ConfigMacOSXBehaviors is active)
    const bool primary_mod = io.KeyCtrl || io.KeySuper;

    if (mode_ == DesktopMode::HomeHub) {
        if ((primary_mod && ImGui::IsKeyPressed(ImGuiKey_K, false)) ||
            ImGui::IsKeyPressed(ImGuiKey_F3, false)) {
            navigation_.openCommandPalette();
            return;
        }
        if (navigation_.isCommandPaletteOpen()) {
            return;
        }
        if (!io.WantTextInput) {
            if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_1, false)) navigateTo(HomeHubViewId::ProjectsStart);
            else if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_2, false)) navigateTo(HomeHubViewId::Templates);
            else if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_3, false)) navigateTo(HomeHubViewId::NewProject);
            else if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_4, false)) navigateTo(HomeHubViewId::OpenProject);
            else if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_5, false)) navigateTo(HomeHubViewId::Recovery);
            else if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_6, false)) navigateTo(HomeHubViewId::Preferences);
            else if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_7, false)) navigateTo(HomeHubViewId::About);
            else if (primary_mod && ImGui::IsKeyPressed(ImGuiKey_N, false)) navigateTo(HomeHubViewId::NewProject);
            else if (primary_mod && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
                ProjectOpenActions open_act{};
                project_open_.invokeNativeOpenDialog(*file_dialog_, paths_.default_projects_directory, open_act);
                if (open_act.open_project_path.has_value()) {
                    (void)executeOpenProject(*open_act.open_project_path);
                } else if (open_act.request_in_app_file_picker) {
                    in_app_browser_open_ = true;
                    in_app_browser_pick_dir_ = false;
                    in_app_browser_dir_ = paths_.default_projects_directory;
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_F2, false) && has_active_studio_session_) {
                enterStudioMode();
            }
        }
    } else {
        // Studio Mode Shortcuts: F2 or Ctrl/Cmd+Shift+H returns to Projects & Start; Ctrl/Cmd+S saves active .fe3d
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false) ||
            (primary_mod && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_H, false))) {
            returnToHomeHub();
        } else if (primary_mod && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
            (void)saveActiveStudioProject();
        }
    }
}

HubShellMetrics NativeHomeHub::buildShellMetrics() const {
    const auto& caps = session_.renderer().device().capabilities();
    return HubShellMetrics{
        .engine_version = std::string(core::engineVersionString()),
        .graphics_backend = caps.backend_name.empty() ? std::string("OpenGL 3.3 Core") : caps.backend_name,
        .ui_scale_percent = preferences_.ui_scale_percent,
        .recent_count = recent_index_.entries().size(),
        .recovery_count = recovery_service_.snapshots().size(),
        .recovery_available = (recovery_service_.storageState() == app::RecoveryStorageState::Available),
        .recovery_unclean_warning = recovery_service_.hadUncleanPreviousSession(),
        .has_active_studio_session = has_active_studio_session_,
        .studio_session_dirty = has_active_studio_session_ && editor_.isDocumentDirty(),
        .active_project_name = session_.document().metadata.project_name,
    };
}

void NativeHomeHub::renderHomeHubCanvas() {
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const HubShellMetrics shell_metrics = buildShellMetrics();
    HubNavigationActions nav_actions{};

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(disp, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    constexpr ImGuiWindowFlags kRootFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("##NativeHomeHubRootWindow", nullptr, kRootFlags)) {
        const float topbar_h = std::round(theme_.px(54.0f));
        const float status_h = std::round(theme_.px(28.0f));
        const float body_h = std::max(120.0f, disp.y - topbar_h - status_h);
        const float rail_w = std::clamp(std::round(theme_.px(240.0f)), 195.0f, std::round(disp.x * 0.24f));
        const float content_w = std::max(240.0f, disp.x - rail_w);

        // 1. Top Bar
        ImGui::SetCursorScreenPos(ImVec2(0.0f, 0.0f));
        navigation_.drawTopBar(theme_, disp.x, topbar_h, shell_metrics, nav_actions);

        // 2. Left Navigation Rail
        ImGui::SetCursorScreenPos(ImVec2(0.0f, topbar_h));
        navigation_.drawLeftRail(theme_, rail_w, body_h, active_view_, shell_metrics, nav_actions);

        // 3. Main View Content Area (Scrollable, Centered Max-Width Container on Wide/QHD Monitors)
        ImGui::SetCursorScreenPos(ImVec2(rail_w, topbar_h));
        ImGui::PopStyleVar(3); // Restore standard panel/frame padding inside content scroll region!

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x0F, 0x11, 0x15));
        ImGui::BeginChild(
            "##HubActiveViewScrollCanvas",
            ImVec2(content_w, body_h),
            false,
            ImGuiWindowFlags_AlwaysUseWindowPadding);

        const float outer_pad = std::round(theme_.px(22.0f));
        const float max_inner_w = std::round(theme_.px(1240.0f));
        const float raw_avail = content_w - outer_pad * 2.0f;
        const float container_w = std::min(std::max(200.0f, raw_avail), max_inner_w);
        const float margin_left = std::max(outer_pad, std::round((content_w - container_w) * 0.5f));

        ImGui::SetCursorPos(ImVec2(margin_left, std::round(theme_.px(18.0f))));
        ImGui::BeginChild(
            "##HubCenteredContainer",
            ImVec2(container_w, 0.0f),
            ImGuiChildFlags_AutoResizeY,
            ImGuiWindowFlags_NoBackground);

        if (!global_error_.empty() && active_view_ != HomeHubViewId::NewProject &&
            active_view_ != HomeHubViewId::OpenProject) {
            theme_.drawBanner(global_error_, TagTone::Danger);
        }

        switch (active_view_) {
            case HomeHubViewId::ProjectsStart: {
                ProjectBrowserActions act{};
                project_browser_.render(theme_, recent_index_, recovery_service_, shell_metrics, act);
                if (act.recent_index_mutated && !paths_.recent_projects_path.empty()) {
                    (void)recent_index_.saveToFile(paths_.recent_projects_path, file_system_);
                }
                if (act.navigate_to.has_value()) nav_actions.navigate_to = act.navigate_to;
                if (act.quick_create_template.has_value()) nav_actions.quick_create_template = act.quick_create_template;
                if (act.trigger_open_dialog) nav_actions.trigger_open_dialog = true;
                if (act.resume_studio) nav_actions.resume_studio = true;
                if (act.reopen_project_path.has_value()) {
                    (void)executeOpenProject(*act.reopen_project_path);
                }
                break;
            }
            case HomeHubViewId::Templates: {
                TemplateBrowserActions act{};
                template_browser_.render(theme_, act);
                if (act.configure_new_with_template.has_value()) {
                    project_creation_.applyTemplateSelection(*act.configure_new_with_template);
                    nav_actions.navigate_to = HomeHubViewId::NewProject;
                }
                if (act.quick_create_template.has_value()) {
                    nav_actions.quick_create_template = act.quick_create_template;
                }
                break;
            }
            case HomeHubViewId::NewProject: {
                ProjectCreationActions act{};
                project_creation_.render(theme_, file_system_, *file_dialog_, act);
                if (act.navigate_to.has_value()) nav_actions.navigate_to = act.navigate_to;
                if (act.create_project.has_value()) {
                    (void)executeCreateProject(*act.create_project);
                }
                if (act.request_in_app_folder_picker) {
                    in_app_browser_open_ = true;
                    in_app_browser_pick_dir_ = true;
                    in_app_browser_dir_ = std::filesystem::path(project_creation_.destinationDirectory());
                }
                break;
            }
            case HomeHubViewId::OpenProject: {
                ProjectOpenActions act{};
                project_open_.render(
                    theme_,
                    file_system_,
                    recent_index_,
                    *file_dialog_,
                    paths_.default_projects_directory,
                    act);
                if (act.open_project_path.has_value()) {
                    (void)executeOpenProject(*act.open_project_path);
                }
                if (act.request_in_app_file_picker) {
                    in_app_browser_open_ = true;
                    in_app_browser_pick_dir_ = false;
                    in_app_browser_dir_ = paths_.default_projects_directory;
                }
                break;
            }
            case HomeHubViewId::Recovery: {
                RecoveryViewActions act{};
                recovery_view_.render(theme_, recovery_service_, preferences_, act);
                if (act.capture_manual_checkpoint) {
                    session_.syncDocumentState(editor_.selectedMaterial());
                    auto cap_res = recovery_service_.captureSnapshot(
                        session_.document(),
                        editor_.activeProjectPath(),
                        "Manual Hub Checkpoint",
                        nowEpochMs(),
                        file_system_);
                    if (cap_res) {
                        recovery_view_.setStatusBanner(
                            "Captured recovery checkpoint in slot #" + std::to_string(cap_res.value().slot_index) + ".",
                            TagTone::Emerald);
                    } else {
                        recovery_view_.setStatusBanner(cap_res.error().message, TagTone::Danger);
                    }
                }
                if (act.restore_snapshot_id.has_value()) {
                    (void)executeRestoreRecovery(*act.restore_snapshot_id);
                }
                if (act.discard_snapshot_id.has_value()) {
                    auto dst = recovery_service_.discardSnapshot(*act.discard_snapshot_id, file_system_);
                    if (dst) {
                        recovery_view_.setStatusBanner("Recovery checkpoint removed.", TagTone::Emerald);
                    } else {
                        recovery_view_.setStatusBanner(dst.error().message, TagTone::Danger);
                    }
                }
                if (act.clear_all_snapshots) {
                    auto cst = recovery_service_.discardAll(file_system_);
                    if (cst) {
                        recovery_view_.setStatusBanner("Cleared all recovery checkpoints.", TagTone::Emerald);
                    } else {
                        recovery_view_.setStatusBanner(cst.error().message, TagTone::Danger);
                    }
                }
                break;
            }
            case HomeHubViewId::Preferences: {
                PreferencesViewActions act{};
                preferences_view_.render(theme_, preferences_, paths_.preferences_path, act);
                if (act.reset_to_defaults) {
                    app::AppPreferences def{};
                    preferences_ = def;
                    preferences_view_.syncFromPreferences(preferences_);
                    project_creation_.setAuthor(preferences_.default_author);
                    if (!paths_.preferences_path.empty()) {
                        (void)app::AppPreferencesStore::saveToFile(preferences_, paths_.preferences_path, file_system_);
                    }
                    preferences_view_.setStatusFeedback(
                        "Preferences reset to factory defaults and saved.",
                        TagTone::Emerald);
                } else if (act.save_preferences.has_value()) {
                    preferences_ = *act.save_preferences;
                    recent_index_.enforceLimit(preferences_.recent_projects_limit);
                    project_creation_.setAuthor(preferences_.default_author);
                    if (!paths_.preferences_path.empty()) {
                        auto sv_st = app::AppPreferencesStore::saveToFile(
                            preferences_,
                            paths_.preferences_path,
                            file_system_);
                        if (sv_st) {
                            preferences_view_.setStatusFeedback(
                                "Preferences saved to " + paths_.preferences_path.generic_string(),
                                TagTone::Emerald);
                        } else {
                            preferences_view_.setStatusFeedback(sv_st.error().message, TagTone::Danger);
                        }
                    } else {
                        preferences_view_.setStatusFeedback("Preferences updated.", TagTone::Emerald);
                    }
                }
                break;
            }
            case HomeHubViewId::About: {
                const auto fb = window_.framebufferSize();
                AboutRuntimeContext about_ctx{
                    .system_info = app::querySystemInfo(session_, recovery_service_.storageState()),
                    .framebuffer_width = fb.width,
                    .framebuffer_height = fb.height,
                    .content_scale = window_.contentScale(),
                    .ui_scale_percent = preferences_.ui_scale_percent,
                    .file_dialog_backend = std::string(file_dialog_->backendLabel()),
                    .preferences_path = paths_.preferences_path,
                    .recovery_dir = paths_.recovery_directory,
                };
                about_view_.render(theme_, about_ctx);
                break;
            }
        }

        ImGui::Dummy(ImVec2(0.0f, theme_.px(20.0f)));
        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        // 4. Bottom Status Strip
        ImGui::SetCursorScreenPos(ImVec2(0.0f, topbar_h + body_h));
        navigation_.drawStatusStrip(theme_, disp.x, status_h, shell_metrics);
    }
    ImGui::End();
    ImGui::PopStyleVar(3);

    // Command Palette Modal
    navigation_.drawCommandPalette(theme_, shell_metrics, nav_actions);

    // Apply Navigation / Top-Level Actions
    if (nav_actions.navigate_to.has_value()) {
        navigateTo(*nav_actions.navigate_to);
    }
    if (nav_actions.quick_create_template.has_value()) {
        (void)executeQuickCreateTemplate(*nav_actions.quick_create_template);
    }
    if (nav_actions.trigger_open_dialog) {
        ProjectOpenActions open_act{};
        project_open_.invokeNativeOpenDialog(*file_dialog_, paths_.default_projects_directory, open_act);
        if (open_act.open_project_path.has_value()) {
            (void)executeOpenProject(*open_act.open_project_path);
        } else if (open_act.request_in_app_file_picker) {
            navigateTo(HomeHubViewId::OpenProject);
            in_app_browser_open_ = true;
            in_app_browser_pick_dir_ = false;
            in_app_browser_dir_ = paths_.default_projects_directory;
        }
    }
    if (nav_actions.resume_studio && has_active_studio_session_) {
        enterStudioMode();
    }
}

void NativeHomeHub::renderStudioOverlay() {
    // Non-intrusive Stage 1.1 Studio Return Pill (does NOT alter or redesign the existing 3D Studio).
    const float pad = std::round(theme_.px(12.0f));
    ImGui::SetNextWindowPos(ImVec2(pad, pad), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ThemeColors::vec4(0x16, 0x19, 0x20, 0xEC));
    ImGui::PushStyleColor(ImGuiCol_Border, ThemeColors::vec4(0x2C, 0x33, 0x40));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(theme_.px(10.0f), theme_.px(7.0f)));

    constexpr ImGuiWindowFlags kPillFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing;

    if (ImGui::Begin("##StudioReturnHubPill", nullptr, kPillFlags)) {
        if (theme_.styledButton(
                "Projects & Start (F2)##BtnStudioToHub",
                ButtonVariant::Secondary,
                ImVec2(0.0f, theme_.px(28.0f)),
                false,
                HubIcon::ArrowLeft)) {
            returnToHomeHub();
        }
        ImGui::SameLine();
        std::string proj_name = session_.document().metadata.project_name;
        if (proj_name.empty()) proj_name = "Untitled Scene";
        theme_.pushBoldFont();
        ImGui::TextUnformatted(proj_name.c_str());
        theme_.popFont();
        ImGui::SameLine();
        theme_.drawTagPill(
            editor_.isDocumentDirty() ? "Unsaved" : "Saved",
            editor_.isDocumentDirty() ? TagTone::Amber : TagTone::Emerald);
        ImGui::SameLine();
        if (theme_.styledButton(
                "Save .fe3d##BtnStudioSave",
                ButtonVariant::PrimaryCyan,
                ImVec2(0.0f, theme_.px(28.0f)),
                false,
                HubIcon::Save)) {
            (void)saveActiveStudioProject();
        }
    }
    ImGui::End();

    // Optional compact read-only Viewport HUD pill controlled by AppPreferences::show_viewport_hud
    if (preferences_.show_viewport_hud) {
        const ImVec2 disp = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(disp.x - pad, pad), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        if (ImGui::Begin(
                "##StudioViewportHudPill",
                nullptr,
                kPillFlags | ImGuiWindowFlags_NoInputs)) {
            theme_.pushMonoFont();
            const std::string hud_line =
                std::string(session_.viewportViewLabel()) + "  ·  " +
                std::to_string(session_.document().scene.objects().size()) + " Objects";
            ImGui::TextColored(ThemeColors::vec4(0xA8, 0xB2, 0xC3), "%s", hud_line.c_str());
            theme_.popFont();
        }
        ImGui::End();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void NativeHomeHub::renderDirtySessionGuardModal() {
    if (!dirty_guard_modal_open_) return;

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float mw = std::min(disp.x - theme_.px(40.0f), std::round(theme_.px(510.0f)));
    const float mh = std::round(theme_.px(218.0f));
    ImGui::SetNextWindowPos(ImVec2((disp.x - mw) * 0.5f, (disp.y - mh) * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(mw, mh), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ThemeColors::vec4(0x1B, 0x20, 0x29));
    ImGui::PushStyleColor(ImGuiCol_Border, ThemeColors::vec4(0xF2, 0x9D, 0x38));
    if (ImGui::Begin(
            "##DirtySessionGuardModal",
            nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings)) {
        theme_.drawTagPill("UNSAVED WORK PROTECTION", TagTone::Amber);
        ImGui::Spacing();
        theme_.pushSubheadingFont();
        ImGui::TextUnformatted("Active Studio Session Has Unsaved Changes");
        theme_.popFont();
        ImGui::TextWrapped(
            "Project '%s' contains unsaved Studio edits. Save your changes before opening or creating another project, or discard them.",
            session_.document().metadata.project_name.empty()
                ? "Untitled Scene"
                : session_.document().metadata.project_name.c_str());
        ImGui::Dummy(ImVec2(0.0f, theme_.px(12.0f)));

        if (theme_.styledButton("Cancel##CancelDirtyGuard", ButtonVariant::Secondary)) {
            cancelDirtySessionGuard();
        }
        ImGui::SameLine(0.0f, theme_.px(8.0f));
        if (theme_.styledButton("Discard Unsaved Changes##DiscardDirtyGuard", ButtonVariant::Danger)) {
            confirmDiscardDirtyAndContinue();
        }
        ImGui::SameLine(0.0f, theme_.px(8.0f));
        if (theme_.styledButton("Save & Continue##SaveDirtyGuard", ButtonVariant::PrimaryCyan)) {
            confirmSaveDirtyAndContinue();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}

void NativeHomeHub::renderExitConfirmModal() {
    if (!exit_confirm_modal_open_) return;

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float mw = std::min(disp.x - theme_.px(40.0f), std::round(theme_.px(490.0f)));
    const float mh = std::round(theme_.px(210.0f));
    ImGui::SetNextWindowPos(ImVec2((disp.x - mw) * 0.5f, (disp.y - mh) * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(mw, mh), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ThemeColors::vec4(0x1B, 0x20, 0x29));
    ImGui::PushStyleColor(ImGuiCol_Border, ThemeColors::vec4(0xF2, 0x9D, 0x38));
    if (ImGui::Begin(
            "##ExitUnsavedConfirmModal",
            nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings)) {
        theme_.drawTagPill("CONFIRM APPLICATION EXIT", TagTone::Amber);
        ImGui::Spacing();
        theme_.pushSubheadingFont();
        ImGui::TextUnformatted("Save Changes Before Closing Final Effect 3D?");
        theme_.popFont();
        ImGui::TextWrapped(
            "Your active 3D Studio session has unsaved modifications. Closing without saving will discard edits not yet written to a .fe3d file.");
        ImGui::Dummy(ImVec2(0.0f, theme_.px(12.0f)));

        if (theme_.styledButton("Cancel##CancelExitBtn", ButtonVariant::Secondary)) {
            cancelExitConfirmation();
        }
        ImGui::SameLine(0.0f, theme_.px(8.0f));
        if (theme_.styledButton("Exit Without Saving##DiscardExitBtn", ButtonVariant::Danger)) {
            confirmExitWithoutSaving();
        }
        ImGui::SameLine(0.0f, theme_.px(8.0f));
        if (theme_.styledButton("Save & Exit##SaveExitBtn", ButtonVariant::PrimaryCyan)) {
            confirmSaveAndExit();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}

void NativeHomeHub::renderInAppFileBrowserModal() {
    if (!in_app_browser_open_) return;

    std::error_code ec;
    if (in_app_browser_dir_.empty() || !std::filesystem::exists(in_app_browser_dir_, ec)) {
        in_app_browser_dir_ = std::filesystem::current_path(ec);
    }

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float mw = std::min(disp.x - theme_.px(48.0f), std::round(theme_.px(620.0f)));
    const float mh = std::min(disp.y - theme_.px(64.0f), std::round(theme_.px(430.0f)));
    ImGui::SetNextWindowPos(ImVec2((disp.x - mw) * 0.5f, (disp.y - mh) * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(mw, mh), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ThemeColors::vec4(0x18, 0x1C, 0x24));
    ImGui::PushStyleColor(ImGuiCol_Border, ThemeColors::vec4(0x27, 0xB6, 0xD9));
    if (ImGui::Begin(
            "##InAppNativeFileBrowser",
            nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings)) {
        theme_.pushSubheadingFont();
        ImGui::TextUnformatted(
            in_app_browser_pick_dir_ ? "Select Destination Folder" : "Select .fe3d Project File");
        theme_.popFont();

        theme_.pushMonoFont();
        ImGui::TextColored(
            ThemeColors::vec4(0x27, 0xB6, 0xD9),
            "%s",
            in_app_browser_dir_.generic_string().c_str());
        theme_.popFont();

        if (in_app_browser_dir_.has_parent_path() &&
            in_app_browser_dir_.parent_path() != in_app_browser_dir_) {
            if (theme_.styledButton("Up to Parent Folder##BtnBrowserUp", ButtonVariant::Secondary)) {
                in_app_browser_dir_ = in_app_browser_dir_.parent_path();
                in_app_browser_selected_file_.clear();
            }
        }

        ImGui::Spacing();
        ImGui::BeginChild("##BrowserEntriesList", ImVec2(0.0f, mh - theme_.px(150.0f)), true);

        std::vector<std::filesystem::directory_entry> dirs;
        std::vector<std::filesystem::directory_entry> files;
        for (const auto& entry : std::filesystem::directory_iterator(in_app_browser_dir_, ec)) {
            if (entry.is_directory(ec)) {
                dirs.push_back(entry);
            } else if (!in_app_browser_pick_dir_ && entry.is_regular_file(ec)) {
                const auto ext = entry.path().extension().string();
                if (ext == ".fe3d" || ext == ".json") {
                    files.push_back(entry);
                }
            }
        }
        auto byName = [](const auto& a, const auto& b) {
            return a.path().filename().string() < b.path().filename().string();
        };
        std::sort(dirs.begin(), dirs.end(), byName);
        std::sort(files.begin(), files.end(), byName);

        for (const auto& d : dirs) {
            const std::string label = "[DIR]  " + d.path().filename().string();
            if (ImGui::Selectable(label.c_str(), false)) {
                in_app_browser_dir_ = d.path();
                in_app_browser_selected_file_.clear();
            }
        }
        for (const auto& f : files) {
            const std::string fname = f.path().filename().string();
            const bool sel = (in_app_browser_selected_file_ == fname);
            if (ImGui::Selectable(fname.c_str(), sel, ImGuiSelectableFlags_AllowDoubleClick)) {
                in_app_browser_selected_file_ = fname;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    const auto chosen = in_app_browser_dir_ / fname;
                    in_app_browser_open_ = false;
                    project_open_.setPathInput(chosen.generic_string());
                    (void)executeOpenProject(chosen);
                }
            }
        }
        if (dirs.empty() && files.empty()) {
            ImGui::TextColored(ThemeColors::vec4(0x88, 0x93, 0xA7), "No matching entries in this directory.");
        }
        ImGui::EndChild();

        ImGui::Spacing();
        if (theme_.styledButton("Cancel##CancelInAppBrowser", ButtonVariant::Secondary)) {
            // Silent cancellation without error
            in_app_browser_open_ = false;
        }
        ImGui::SameLine(0.0f, theme_.px(8.0f));
        if (in_app_browser_pick_dir_) {
            if (theme_.styledButton("Use This Folder##ConfirmPickFolder", ButtonVariant::PrimaryCyan)) {
                project_creation_.setDestinationDirectory(in_app_browser_dir_);
                in_app_browser_open_ = false;
            }
        } else {
            if (theme_.styledButton(
                    "Open Selected .fe3d##ConfirmPickFile",
                    ButtonVariant::PrimaryCyan,
                    ImVec2(0, 0),
                    in_app_browser_selected_file_.empty())) {
                const auto chosen = in_app_browser_dir_ / in_app_browser_selected_file_;
                in_app_browser_open_ = false;
                project_open_.setPathInput(chosen.generic_string());
                (void)executeOpenProject(chosen);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}

void NativeHomeHub::tickPeriodicAutosave(std::uint64_t now_epoch_ms) {
    if (!preferences_.autosave_enabled ||
        recovery_service_.storageState() != app::RecoveryStorageState::Available ||
        !has_active_studio_session_ ||
        !editor_.isDocumentDirty()) {
        return;
    }
    const std::uint64_t interval_ms =
        static_cast<std::uint64_t>(preferences_.autosave_interval_seconds) * 1000ULL;
    const std::uint64_t cur_tok = session_.commandStack().currentStateToken();
    if (cur_tok != last_autosave_state_token_ && (now_epoch_ms - last_autosave_epoch_ms_) >= interval_ms) {
        session_.syncDocumentState(editor_.selectedMaterial());
        if (recovery_service_.captureSnapshot(
                session_.document(),
                editor_.activeProjectPath(),
                "Periodic Autosave",
                now_epoch_ms,
                file_system_)) {
            last_autosave_epoch_ms_ = now_epoch_ms;
            last_autosave_state_token_ = cur_tok;
        }
    }
}

} // namespace finaleffect::ui
