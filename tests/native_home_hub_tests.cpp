// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors
//
// Stage 1.1 Automated Verification Suite — Native Desktop Home Hub & Cross-Platform Completion

#include "finaleffect/app/app_preferences.hpp"
#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/app/project_hub_service.hpp"
#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/platform/window.hpp"
#include "finaleffect/project/project.hpp"
#include "finaleffect/rendering/software_render_device.hpp"
#include "finaleffect/ui/imgui_software_rasterizer.hpp"
#include "finaleffect/ui/native_file_dialog.hpp"
#include "finaleffect/ui/native_home_hub.hpp"
#include "finaleffect/ui/png_encoder.hpp"

#include <imgui.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

#define FE_CHECK(expr)                                                                             \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " -> " #expr << '\n';         \
            ++g_failures;                                                                          \
        }                                                                                          \
    } while (false)

class ScopedTempDir {
public:
    ScopedTempDir() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("fe3d_native_hub_test_" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }
    ~ScopedTempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

// Read-only / failing filesystem wrapper to verify honest Recovery Unavailable state.
class FailingWriteFileSystem final : public finaleffect::platform::IFileSystem {
public:
    [[nodiscard]] bool exists(const std::filesystem::path&) const override { return false; }
    [[nodiscard]] finaleffect::core::Status createDirectories(const std::filesystem::path&) override {
        return finaleffect::core::Status::fail(
            finaleffect::core::ErrorCode::IoError,
            "Simulated read-only recovery volume");
    }
    [[nodiscard]] finaleffect::core::Result<std::string> readTextFile(
        const std::filesystem::path&) const override {
        return finaleffect::core::Error(finaleffect::core::ErrorCode::NotFound, "Not found");
    }
    [[nodiscard]] finaleffect::core::Status writeTextFile(
        const std::filesystem::path&,
        std::string_view) override {
        return finaleffect::core::Status::fail(finaleffect::core::ErrorCode::IoError, "Read-only");
    }
    [[nodiscard]] finaleffect::core::Result<std::vector<std::uint8_t>> readBinaryFile(
        const std::filesystem::path&) const override {
        return finaleffect::core::Error(finaleffect::core::ErrorCode::NotFound, "Not found");
    }
    [[nodiscard]] finaleffect::core::Status writeBinaryFile(
        const std::filesystem::path&,
        const std::vector<std::uint8_t>&) override {
        return finaleffect::core::Status::fail(finaleffect::core::ErrorCode::IoError, "Read-only");
    }
};

struct NativeHubTestRig {
    ScopedTempDir temp_dir{};
    finaleffect::core::Engine engine{finaleffect::core::EngineConfig{
        .application_name = "NativeHubTest",
        .attach_console_log_sink = false,
    }};
    finaleffect::platform::HeadlessWindow window{};
    finaleffect::rendering::SoftwareRenderDevice* sw_device{nullptr};
    std::unique_ptr<finaleffect::app::StudioSession> session{};
    std::unique_ptr<finaleffect::app::EditorController> editor{};
    finaleffect::app::AppPreferences prefs{};
    finaleffect::app::RecentProjectsIndex recents{};
    finaleffect::app::RecoveryService recovery{};
    finaleffect::ui::MockNativeFileDialog* mock_dialog{nullptr};
    std::unique_ptr<finaleffect::ui::NativeHomeHub> hub{};

    explicit NativeHubTestRig(std::uint32_t w = 1920, std::uint32_t h = 1080) {
        FE_CHECK(window.initialize({.title = "NativeHubTest", .width = w, .height = h}).hasValue());
        auto sw = std::make_unique<finaleffect::rendering::SoftwareRenderDevice>();
        sw_device = sw.get();
        session = std::make_unique<finaleffect::app::StudioSession>(
            window,
            std::move(sw),
            finaleffect::app::StudioSessionConfig{
                .animate_showcase_rig = false,
                .poll_window_events_on_update = false,
                .consume_window_input_on_update = false,
                .swap_buffers_on_update = false,
            });
        editor = std::make_unique<finaleffect::app::EditorController>(*session);
        FE_CHECK(engine.initialize(session.get()).hasValue());

        const auto cfg_dir = temp_dir.path() / "config";
        const auto rec_dir = temp_dir.path() / "recovery";
        const auto proj_dir = temp_dir.path() / "projects";
        (void)engine.fileSystem().createDirectories(cfg_dir);
        (void)engine.fileSystem().createDirectories(proj_dir);
        FE_CHECK(recovery.initialize(rec_dir, engine.fileSystem()).hasValue());

        auto dlg = std::make_unique<finaleffect::ui::MockNativeFileDialog>(true);
        mock_dialog = dlg.get();

        hub = std::make_unique<finaleffect::ui::NativeHomeHub>(
            window,
            engine.fileSystem(),
            *session,
            *editor,
            prefs,
            recents,
            recovery,
            finaleffect::ui::NativeHomeHubPaths{
                .preferences_path = cfg_dir / "preferences.json",
                .recent_projects_path = cfg_dir / "recent_projects.json",
                .recovery_directory = rec_dir,
                .default_projects_directory = proj_dir,
            },
            std::move(dlg));
        FE_CHECK(hub->initialize(finaleffect::ui::DesktopMode::HomeHub).hasValue());
    }

    ~NativeHubTestRig() {
        if (hub) hub->shutdown();
        engine.shutdown();
        window.shutdown();
    }

    void stepUiFrame() {
        const auto evs = window.consumeInputEvents();
        hub->feedHeadlessInputEvents(evs);
        (void)hub->buildUiFrame(1.0f / 60.0f);
    }
};

void testLaunchAllSevenViewsAndNavigation() {
    NativeHubTestRig rig(1920, 1080);

    // 1. Default launch opens in Native Home Hub on Projects & Start with zero fake recents/recovery
    FE_CHECK(rig.hub->mode() == finaleffect::ui::DesktopMode::HomeHub);
    FE_CHECK(rig.hub->activeView() == finaleffect::ui::HomeHubViewId::ProjectsStart);
    FE_CHECK(rig.recents.entries().empty());
    FE_CHECK(rig.recovery.snapshots().empty());

    // 2. Render all 7 views at 1920x1080, verify non-empty draw lists, rasterize & verify valid PNG output
    std::vector<std::uint8_t> rgba_buf(1920u * 1080u * 4u, 15u);
    for (const auto& desc : finaleffect::ui::allHomeHubViews()) {
        rig.hub->navigateTo(desc.id);
        FE_CHECK(rig.hub->activeView() == desc.id);
        rig.stepUiFrame();

        const ImDrawData* dd = ImGui::GetDrawData();
        FE_CHECK(dd != nullptr);
        FE_CHECK(dd->CmdListsCount > 0);
        FE_CHECK(dd->TotalVtxCount > 40);
        FE_CHECK(dd->TotalIdxCount > 60);

        unsigned char* atlas_px = nullptr;
        int aw = 0;
        int ah = 0;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&atlas_px, &aw, &ah);
        FE_CHECK(atlas_px != nullptr && aw > 0 && ah > 0);

        std::fill(rgba_buf.begin(), rgba_buf.end(), 15u);
        finaleffect::ui::rasterizeImDrawDataToRgba8(
            dd,
            finaleffect::ui::FontAtlasRgbaView{atlas_px, aw, ah},
            1920u,
            1080u,
            rgba_buf);

        auto png = finaleffect::ui::encodeRgba8ToPng(1920u, 1080u, rgba_buf);
        FE_CHECK(png.hasValue());
        FE_CHECK(png.value().size() > 1024u);
        FE_CHECK(png.value()[0] == 0x89u && png.value()[1] == 'P' && png.value()[2] == 'N' && png.value()[3] == 'G');
    }

    // 3. Keyboard shortcut navigation (Alt+2 -> Templates, Ctrl+K -> Command Palette, Escape -> Close Palette)
    rig.window.pushInputEvent(finaleffect::platform::KeyEvent{
        .key = finaleffect::platform::KeyCode::Digit2,
        .action = finaleffect::platform::KeyAction::Press,
        .mods = {.alt = true},
    });
    rig.stepUiFrame();
    FE_CHECK(rig.hub->activeView() == finaleffect::ui::HomeHubViewId::Templates);
    rig.window.pushInputEvent(finaleffect::platform::KeyEvent{
        .key = finaleffect::platform::KeyCode::Digit2,
        .action = finaleffect::platform::KeyAction::Release,
    });
    rig.stepUiFrame();

    rig.window.pushInputEvent(finaleffect::platform::KeyEvent{
        .key = finaleffect::platform::KeyCode::K,
        .action = finaleffect::platform::KeyAction::Press,
        .mods = {.ctrl = true},
    });
    rig.stepUiFrame();
    FE_CHECK(rig.hub->navigation().isCommandPaletteOpen());

    // Exercise mouse move, left-click, scroll wheel, and Unicode text input events into Command Palette
    rig.window.pushInputEvent(finaleffect::platform::TextInputEvent{.codepoint = static_cast<std::uint32_t>('A')});
    rig.window.pushInputEvent(finaleffect::platform::TextInputEvent{.codepoint = static_cast<std::uint32_t>('b')});
    rig.window.pushInputEvent(finaleffect::platform::CursorMoveEvent{.x = 120.0, .y = 210.0});
    rig.window.pushInputEvent(finaleffect::platform::ScrollEvent{.delta_x = 0.0, .delta_y = -1.0});
    rig.stepUiFrame();

    // Close Command Palette with Escape, then left-click sidebar item #1 (Projects & Start)
    rig.window.pushInputEvent(finaleffect::platform::KeyEvent{
        .key = finaleffect::platform::KeyCode::Escape,
        .action = finaleffect::platform::KeyAction::Press,
    });
    rig.stepUiFrame();
    FE_CHECK(!rig.hub->navigation().isCommandPaletteOpen());

    rig.window.pushInputEvent(finaleffect::platform::CursorMoveEvent{.x = 110.0, .y = 118.0});
    rig.window.pushInputEvent(finaleffect::platform::MouseButtonEvent{
        .button = finaleffect::platform::MouseButton::Left,
        .action = finaleffect::platform::KeyAction::Press,
        .x = 110.0,
        .y = 118.0,
    });
    rig.stepUiFrame();
    rig.window.pushInputEvent(finaleffect::platform::MouseButtonEvent{
        .button = finaleffect::platform::MouseButton::Left,
        .action = finaleffect::platform::KeyAction::Release,
        .x = 110.0,
        .y = 118.0,
    });
    rig.stepUiFrame();
    FE_CHECK(rig.hub->activeView() == finaleffect::ui::HomeHubViewId::ProjectsStart);
}

void testAllFourTemplatesCreationAndRecentProjectsManagement() {
    NativeHubTestRig rig(1920, 1080);

    // Create projects from all 4 verified C++ templates
    for (const auto& tmpl : finaleffect::app::TemplateCatalog::all()) {
        rig.hub->templateBrowser().setSelectedTemplate(tmpl.id);
        FE_CHECK(rig.hub->templateBrowser().selectedTemplate() == tmpl.id);
        FE_CHECK(rig.hub->executeQuickCreateTemplate(tmpl.id, true).hasValue());
        FE_CHECK(rig.hub->mode() == finaleffect::ui::DesktopMode::Studio);
        FE_CHECK(rig.session->document().scene.objects().size() == tmpl.expected_objects);
        FE_CHECK(rig.session->document().scene.meshes().size() == tmpl.expected_meshes);
        rig.hub->returnToHomeHub();
        FE_CHECK(rig.hub->mode() == finaleffect::ui::DesktopMode::HomeHub);
        FE_CHECK(rig.hub->activeView() == finaleffect::ui::HomeHubViewId::ProjectsStart);
    }

    FE_CHECK(rig.recents.entries().size() == 4u);

    // Verify search, sorting, Grid/List rendering, Pin/Unpin, and Remove from History without deleting disk file
    rig.hub->projectBrowser().setSearchQuery("Shader");
    rig.hub->projectBrowser().setLayoutMode(finaleffect::ui::ProjectLayoutMode::Grid);
    rig.stepUiFrame();
    const auto shader_matches = rig.recents.query("Shader");
    FE_CHECK(shader_matches.size() == 1u);

    const std::string target_id = shader_matches.front().id;
    const std::filesystem::path disk_file(shader_matches.front().file_path);
    FE_CHECK(rig.engine.fileSystem().exists(disk_file));

    FE_CHECK(rig.recents.setPinned(target_id, true));
    FE_CHECK(rig.recents.query({}, finaleffect::app::RecentSortMode::LastOpenedDesc, true).size() == 1u);

    // Removing from history MUST NOT delete the project file on disk!
    FE_CHECK(rig.recents.removeById(target_id));
    FE_CHECK(rig.recents.entries().size() == 3u);
    FE_CHECK(rig.engine.fileSystem().exists(disk_file));
}

void testNewProjectValidationAndOverwriteProtection() {
    NativeHubTestRig rig(1920, 1080);
    rig.hub->navigateTo(finaleffect::ui::HomeHubViewId::NewProject);

    auto& creation = rig.hub->projectCreation();
    finaleffect::ui::ProjectCreationActions actions{};

    // Empty name rejected
    creation.setProjectName("   ");
    FE_CHECK(!creation.requestSubmit(rig.engine.fileSystem(), actions));
    FE_CHECK(!creation.errorMessage().empty());
    FE_CHECK(!actions.create_project.has_value());

    // Illegal explicit filename rejected
    creation.setProjectName("Valid LookDev");
    creation.setExplicitFileName("bad*name?.fe3d");
    FE_CHECK(!creation.requestSubmit(rig.engine.fileSystem(), actions));
    FE_CHECK(!creation.errorMessage().empty());

    // Valid creation succeeds and transitions to Studio
    creation.setExplicitFileName("hero_lookdev.fe3d");
    FE_CHECK(creation.requestSubmit(rig.engine.fileSystem(), actions));
    FE_CHECK(actions.create_project.has_value());
    FE_CHECK(rig.hub->executeCreateProject(*actions.create_project, true).hasValue());
    FE_CHECK(rig.hub->mode() == finaleffect::ui::DesktopMode::Studio);
    const auto saved_path = actions.create_project->target_path;
    FE_CHECK(rig.engine.fileSystem().exists(saved_path));

    // Overwrite protection: submitting again at the same path opens overwrite modal instead of overwriting
    rig.hub->returnToHomeHub();
    rig.hub->navigateTo(finaleffect::ui::HomeHubViewId::NewProject);
    finaleffect::ui::ProjectCreationActions conflict_actions{};
    FE_CHECK(!creation.requestSubmit(rig.engine.fileSystem(), conflict_actions));
    FE_CHECK(creation.isOverwriteConfirmModalOpen());
    FE_CHECK(!conflict_actions.create_project.has_value());
    rig.stepUiFrame();

    // Cancel keeps modal closed without overwriting
    creation.cancelPendingOverwrite();
    FE_CHECK(!creation.isOverwriteConfirmModalOpen());

    // Confirming overwrite emits allow_overwrite = true and succeeds
    FE_CHECK(!creation.requestSubmit(rig.engine.fileSystem(), conflict_actions));
    creation.confirmPendingOverwrite(conflict_actions);
    FE_CHECK(conflict_actions.create_project.has_value());
    FE_CHECK(conflict_actions.create_project->allow_overwrite);
    FE_CHECK(rig.hub->executeCreateProject(*conflict_actions.create_project, true).hasValue());
}

void testOpenDialogIsolationBundlesLegacySchemaAndInvalidFiles() {
    NativeHubTestRig rig(1920, 1080);

    // 1. Cancelled native dialog produces zero errors
    rig.mock_dialog->queueOpenResult({.status = finaleffect::ui::FileDialogStatus::Cancelled});
    finaleffect::ui::ProjectOpenActions cancel_act{};
    rig.hub->projectOpen().invokeNativeOpenDialog(
        *rig.mock_dialog,
        rig.temp_dir.path(),
        cancel_act);
    FE_CHECK(rig.mock_dialog->openCallCount() == 1u);
    FE_CHECK(!cancel_act.open_project_path.has_value());
    FE_CHECK(rig.hub->projectOpen().errorMessage().empty());

    // 2. Save a .fe3d + companion .fe3dbin bundle and reopen through MockNativeFileDialog
    const auto bundle_path = rig.temp_dir.path() / "projects" / "bundled_rig.fe3d";
    FE_CHECK(rig.editor->createNewProject(
        finaleffect::app::ProjectTemplate::ShaderBall,
        "Bundled Shader Ball",
        "Native Bundle Tester").hasValue());
    FE_CHECK(rig.editor->saveBundleProject(bundle_path, rig.engine.fileSystem()).hasValue());
    FE_CHECK(rig.engine.fileSystem().exists(rig.temp_dir.path() / "projects" / "bundled_rig.fe3dbin"));

    // Reset to MinimalCube first so reopening proves the bundle genuinely loaded
    FE_CHECK(rig.editor->createNewProject(
        finaleffect::app::ProjectTemplate::MinimalCube,
        "Temp Cube",
        "Tester").hasValue());
    rig.editor->markDocumentSaved();

    rig.mock_dialog->queueOpenResult({
        .status = finaleffect::ui::FileDialogStatus::Selected,
        .selected_path = bundle_path,
    });
    finaleffect::ui::ProjectOpenActions open_bundle_act{};
    rig.hub->projectOpen().invokeNativeOpenDialog(
        *rig.mock_dialog,
        rig.temp_dir.path(),
        open_bundle_act);
    FE_CHECK(open_bundle_act.open_project_path.has_value());
    FE_CHECK(rig.hub->executeOpenProject(*open_bundle_act.open_project_path, true).hasValue());
    FE_CHECK(rig.session->document().metadata.project_name == "Bundled Shader Ball");
    FE_CHECK(rig.hub->mode() == finaleffect::ui::DesktopMode::Studio);

    // 3. Invalid / corrupt .fe3d file reports clear error without corrupting active session
    const auto corrupt_path = rig.temp_dir.path() / "projects" / "corrupt.fe3d";
    FE_CHECK(rig.engine.fileSystem().writeTextFile(corrupt_path, "{ not valid json !!!").hasValue());
    auto bad_st = rig.hub->executeOpenProject(corrupt_path, true);
    FE_CHECK(bad_st.isError());
    FE_CHECK(!rig.hub->projectOpen().errorMessage().empty());
    FE_CHECK(rig.session->document().metadata.project_name == "Bundled Shader Ball");

    // 4. Backward compatibility: .fe3d schema v1.2 loads cleanly
    auto v13_json = finaleffect::project::ProjectSerializer::serializeToString(rig.session->document(), true);
    FE_CHECK(v13_json.hasValue());
    std::string v12_json = v13_json.value();
    const auto ver_pos = v12_json.find("\"minor\": 3");
    if (ver_pos != std::string::npos) {
        v12_json.replace(ver_pos, 10u, "\"minor\": 2");
    }
    const auto v12_path = rig.temp_dir.path() / "projects" / "legacy_v12.fe3d";
    FE_CHECK(rig.engine.fileSystem().writeTextFile(v12_path, v12_json).hasValue());
    FE_CHECK(rig.hub->executeOpenProject(v12_path, true).hasValue());
}

void testRecoveryPreferencesPersistenceAndUnavailableSystemInfo() {
    NativeHubTestRig rig(1920, 1080);

    // 1. Unavailable Recovery state check on read-only filesystem
    FailingWriteFileSystem ro_fs{};
    finaleffect::app::RecoveryService unavail_recovery{};
    FE_CHECK(unavail_recovery.initialize("/readonly/recovery", ro_fs).isError());
    FE_CHECK(unavail_recovery.storageState() == finaleffect::app::RecoveryStorageState::Unavailable);

    // 2. Live Recovery capture, restore, and confirmed clear
    FE_CHECK(rig.hub->executeQuickCreateTemplate(finaleffect::app::ProjectTemplate::MinimalCube, true).hasValue());
    rig.session->document().metadata.project_name = "Checkpoint Scene";
    FE_CHECK(rig.recovery.captureSnapshot(
        rig.session->document(),
        rig.editor->activeProjectPath(),
        "Unit Test Checkpoint",
        1700000000000ULL,
        rig.engine.fileSystem()).hasValue());
    FE_CHECK(rig.recovery.snapshots().size() == 1u);

    rig.hub->navigateTo(finaleffect::ui::HomeHubViewId::Recovery);
    rig.stepUiFrame();

    // Mutate title, then restore checkpoint and verify restoration
    rig.session->document().metadata.project_name = "Discarded Temp Title";
    FE_CHECK(rig.hub->executeRestoreRecovery("latest", true).hasValue());
    FE_CHECK(rig.session->document().metadata.project_name == "Checkpoint Scene");

    // Confirmed Clear All
    rig.hub->navigateTo(finaleffect::ui::HomeHubViewId::Recovery);
    rig.hub->recoveryView().requestClearAllConfirmation();
    FE_CHECK(rig.hub->recoveryView().isClearAllConfirmOpen());
    finaleffect::ui::RecoveryViewActions rec_act{};
    rig.hub->recoveryView().confirmClearAll(rec_act);
    FE_CHECK(rec_act.clear_all_snapshots);
    FE_CHECK(rig.recovery.discardAll(rig.engine.fileSystem()).hasValue());
    FE_CHECK(rig.recovery.snapshots().empty());

    // 3. Preferences validation, persistence across restart, and reset
    rig.hub->navigateTo(finaleffect::ui::HomeHubViewId::Preferences);
    auto& pref_view = rig.hub->preferencesView();
    pref_view.setDraftAutosaveIntervalSeconds(15); // Invalid (< 60)
    finaleffect::ui::PreferencesViewActions pref_act{};
    FE_CHECK(!pref_view.validateAndBuildSave(pref_act));

    pref_view.setDraftAutosaveIntervalSeconds(300);
    pref_view.setDraftUiScalePercent(125u);
    pref_view.setDraftNavigationProfile(finaleffect::app::NavigationProfile::Blender);
    pref_view.setDraftDefaultAuthor("Pipeline TD");
    FE_CHECK(pref_view.validateAndBuildSave(pref_act));
    FE_CHECK(pref_act.save_preferences.has_value());

    const auto prefs_path = rig.temp_dir.path() / "config" / "preferences.json";
    FE_CHECK(finaleffect::app::AppPreferencesStore::saveToFile(
        *pref_act.save_preferences,
        prefs_path,
        rig.engine.fileSystem()).hasValue());

    auto reloaded = finaleffect::app::AppPreferencesStore::loadOrDefault(prefs_path, rig.engine.fileSystem());
    FE_CHECK(reloaded.hasValue());
    FE_CHECK(reloaded.value().ui_scale_percent == 125u);
    FE_CHECK(reloaded.value().navigation_profile == finaleffect::app::NavigationProfile::Blender);
    FE_CHECK(reloaded.value().default_author == "Pipeline TD");
    FE_CHECK(reloaded.value().autosave_interval_seconds == 300u);

    // 4. About / System Info gracefully handles unavailable GPU strings on SoftwareRenderDevice
    const auto sys_info = finaleffect::app::querySystemInfo(*rig.session, rig.recovery.storageState());
    FE_CHECK(sys_info.graphics_vendor == "Unavailable");
    FE_CHECK(sys_info.graphics_api_version == "Unavailable");
    FE_CHECK(sys_info.shading_language_version == "Unavailable");
    FE_CHECK(sys_info.max_texture_dimension == 0u);
    rig.hub->navigateTo(finaleffect::ui::HomeHubViewId::About);
    rig.stepUiFrame();
}

void testResizeScalingUnsavedProtectionAndWebRoundtrip() {
    NativeHubTestRig rig(1920, 1080);

    // 1. Verify window resize & UI scale behavior across 1920x1080, 2560x1440, and 1366x768 (100% & 125%)
    struct ResCase {
        std::uint32_t w;
        std::uint32_t h;
        std::uint32_t scale_pct;
        float dpi;
    };
    constexpr ResCase kCases[] = {
        {1920u, 1080u, 100u, 1.0f},
        {2560u, 1440u, 100u, 1.0f},
        {1366u, 768u, 100u, 1.0f},
        {1366u, 768u, 125u, 1.0f},
        {1920u, 1080u, 100u, 1.5f},
    };
    for (const auto& rc : kCases) {
        rig.window.setSize(rc.w, rc.h);
        rig.window.setContentScale(rc.dpi);
        rig.prefs.ui_scale_percent = rc.scale_pct;
        rig.stepUiFrame();
        const ImDrawData* dd = ImGui::GetDrawData();
        FE_CHECK(dd != nullptr);
        FE_CHECK(static_cast<std::uint32_t>(dd->DisplaySize.x) == rc.w);
        FE_CHECK(static_cast<std::uint32_t>(dd->DisplaySize.y) == rc.h);
        FE_CHECK(dd->TotalVtxCount > 40);
    }

    // 2. Lifecycle Home -> Studio -> Dirty Edit -> Return Home -> Unsaved Guard & Window Close Guard
    FE_CHECK(rig.hub->executeQuickCreateTemplate(finaleffect::app::ProjectTemplate::StarterShowcase, true).hasValue());
    FE_CHECK(rig.hub->mode() == finaleffect::ui::DesktopMode::Studio);
    const std::filesystem::path first_project_path(rig.editor->activeProjectPath());
    const std::size_t initial_obj_count = rig.session->document().scene.objects().size();

    // Perform real Studio modeling operation (proving zero Studio regression + dirty flag)
    auto added_sphere = rig.editor->addPresetObject(finaleffect::app::ScenePreset::Sphere);
    FE_CHECK(added_sphere.hasValue());
    FE_CHECK(rig.editor->renameObject(added_sphere.value(), "AddedStudioSphere").hasValue());
    FE_CHECK(rig.editor->isDocumentDirty());
    FE_CHECK(rig.session->document().scene.objects().size() == initial_obj_count + 1u);

    // Return to Projects & Start
    rig.hub->returnToHomeHub();
    FE_CHECK(rig.hub->mode() == finaleffect::ui::DesktopMode::HomeHub);
    FE_CHECK(rig.hub->activeView() == finaleffect::ui::HomeHubViewId::ProjectsStart);

    // Attempting to create/open another project triggers the Dirty Session Guard modal!
    FE_CHECK(rig.hub->executeQuickCreateTemplate(finaleffect::app::ProjectTemplate::EmptyStage, false).hasValue());
    FE_CHECK(rig.hub->isDirtySessionModalOpen());
    FE_CHECK(rig.session->document().scene.objects().size() == initial_obj_count + 1u);

    // Cancelling preserves unsaved Studio work
    rig.hub->cancelDirtySessionGuard();
    FE_CHECK(!rig.hub->isDirtySessionModalOpen());
    FE_CHECK(rig.session->document().scene.objects().size() == initial_obj_count + 1u);

    // Window close request with unsaved work is intercepted safely
    rig.window.requestClose();
    FE_CHECK(rig.hub->interceptWindowCloseIfDirty());
    FE_CHECK(!rig.window.shouldClose());
    FE_CHECK(rig.hub->isExitConfirmModalOpen());
    rig.hub->cancelExitConfirmation();

    // Save & Continue saves AddedStudioSphere to first_project_path and switches cleanly
    FE_CHECK(rig.hub->executeQuickCreateTemplate(finaleffect::app::ProjectTemplate::EmptyStage, false).hasValue());
    FE_CHECK(rig.hub->isDirtySessionModalOpen());
    rig.hub->confirmSaveDirtyAndContinue();
    FE_CHECK(!rig.hub->isDirtySessionModalOpen());
    FE_CHECK(rig.hub->mode() == finaleffect::ui::DesktopMode::Studio);

    // 3. Cross-Platform Native <-> Web .fe3d Roundtrip Verification
    auto saved_native_json = rig.engine.fileSystem().readTextFile(first_project_path);
    FE_CHECK(saved_native_json.hasValue());

    // Load through EditorController::importInlineProjectJson (the exact Desktop Web WASM bridge!)
    FE_CHECK(rig.editor->importInlineProjectJson(saved_native_json.value()).hasValue());
    FE_CHECK(rig.session->document().scene.objects().size() == initial_obj_count + 1u);
    FE_CHECK(rig.session->document().scene.findObjectByName("AddedStudioSphere") != nullptr);

    // Export back from Web bridge and load through Native loadProjectFile
    auto web_exported_json = rig.editor->exportInlineProjectJson();
    FE_CHECK(web_exported_json.hasValue());
    const auto web_roundtrip_path = rig.temp_dir.path() / "projects" / "from_web_roundtrip.fe3d";
    FE_CHECK(rig.engine.fileSystem().writeTextFile(web_roundtrip_path, web_exported_json.value()).hasValue());
    FE_CHECK(rig.hub->executeOpenProject(web_roundtrip_path, true).hasValue());
    FE_CHECK(rig.session->document().scene.objects().size() == initial_obj_count + 1u);
    FE_CHECK(rig.session->document().scene.findObjectByName("AddedStudioSphere") != nullptr);
}

} // namespace

int main() {
    testLaunchAllSevenViewsAndNavigation();
    testAllFourTemplatesCreationAndRecentProjectsManagement();
    testNewProjectValidationAndOverwriteProtection();
    testOpenDialogIsolationBundlesLegacySchemaAndInvalidFiles();
    testRecoveryPreferencesPersistenceAndUnavailableSystemInfo();
    testResizeScalingUnsavedProtectionAndWebRoundtrip();

    if (g_failures > 0) {
        std::cerr << "[SUMMARY] " << g_failures << " assertion(s) failed in finaleffect_native_home_hub_tests.\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] All Stage 1.1 Native Desktop Home Hub & cross-platform compatibility tests passed.\n";
    return EXIT_SUCCESS;
}
