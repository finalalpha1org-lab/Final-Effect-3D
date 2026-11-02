// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "test_harness.hpp"

#include "finaleffect/app/app_preferences.hpp"
#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/app/project_hub_service.hpp"
#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/platform/window.hpp"
#include "finaleffect/project/json.hpp"
#include "finaleffect/rendering/software_render_device.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {

using namespace finaleffect;

// Deterministic in-memory IFileSystem with toggleable write failures to test
// both Available and Unavailable RecoveryStorageState honestly without disk flakiness.
class MemoryFileSystem final : public platform::IFileSystem {
public:
    bool fail_writes{false};

    [[nodiscard]] bool exists(const std::filesystem::path& path) const override {
        const std::string key = normalize(path);
        return files_.find(key) != files_.end() || dirs_.find(key) != dirs_.end();
    }

    [[nodiscard]] core::Status createDirectories(const std::filesystem::path& directory) override {
        if (fail_writes) {
            return core::Status::fail(core::ErrorCode::IoError, "Simulated read-only storage");
        }
        dirs_.insert(normalize(directory));
        return core::Status::ok();
    }

    [[nodiscard]] core::Result<std::string> readTextFile(const std::filesystem::path& path) const override {
        const auto it = files_.find(normalize(path));
        if (it == files_.end()) {
            return core::Result<std::string>::fail(core::ErrorCode::NotFound, "File not found", path.string());
        }
        return core::Result<std::string>::ok(it->second);
    }

    [[nodiscard]] core::Status writeTextFile(
        const std::filesystem::path& path,
        std::string_view contents) override {
        if (fail_writes) {
            return core::Status::fail(core::ErrorCode::IoError, "Simulated write failure");
        }
        files_[normalize(path)] = std::string(contents);
        return core::Status::ok();
    }

    [[nodiscard]] core::Result<std::vector<std::uint8_t>> readBinaryFile(
        const std::filesystem::path& path) const override {
        const auto it = bin_files_.find(normalize(path));
        if (it == bin_files_.end()) {
            return core::Result<std::vector<std::uint8_t>>::fail(
                core::ErrorCode::NotFound, "Binary file not found", path.string());
        }
        return core::Result<std::vector<std::uint8_t>>::ok(it->second);
    }

    [[nodiscard]] core::Status writeBinaryFile(
        const std::filesystem::path& path,
        const std::vector<std::uint8_t>& bytes) override {
        if (fail_writes) {
            return core::Status::fail(core::ErrorCode::IoError, "Simulated binary write failure");
        }
        bin_files_[normalize(path)] = bytes;
        return core::Status::ok();
    }

    void eraseFile(const std::filesystem::path& path) {
        files_.erase(normalize(path));
    }

private:
    static std::string normalize(const std::filesystem::path& p) {
        return p.lexically_normal().generic_string();
    }

    std::unordered_map<std::string, std::string> files_;
    std::unordered_map<std::string, std::vector<std::uint8_t>> bin_files_;
    std::unordered_set<std::string> dirs_;
};

struct HeadlessHubRig {
    MemoryFileSystem mem_fs;
    platform::HeadlessWindow window;
    std::unique_ptr<core::Engine> engine;
    std::unique_ptr<app::StudioSession> session;
    std::unique_ptr<app::EditorController> editor;

    bool init() {
        if (!window.initialize(platform::WindowDesc{
                .title = "Stage1HubTests",
                .width = 320,
                .height = 200,
            }).hasValue()) {
            return false;
        }
        core::EngineConfig cfg;
        cfg.minimum_log_level = core::LogLevel::Error;
        engine = std::make_unique<core::Engine>(cfg);

        auto sw = std::make_unique<rendering::SoftwareRenderDevice>();
        session = std::make_unique<app::StudioSession>(window, std::move(sw));
        if (!engine->initialize(session.get())) {
            return false;
        }
        editor = std::make_unique<app::EditorController>(*session);
        return static_cast<bool>(engine->stepFrame());
    }

    ~HeadlessHubRig() {
        editor.reset();
        if (engine) {
            engine->shutdown();
        }
        window.shutdown();
    }
};

} // namespace

FE_TEST(Stage1Hub, PreferencesPersistOutsideProjectDocuments) {
    HeadlessHubRig rig;
    FE_REQUIRE_TRUE(rig.init());

    const std::filesystem::path prefs_path = "/virtual/config/preferences.json";
    auto default_loaded = app::AppPreferencesStore::loadOrDefault(prefs_path, rig.mem_fs);
    FE_REQUIRE_TRUE(default_loaded.hasValue());
    FE_EXPECT_EQ(default_loaded.value().ui_scale_percent, 100u);
    FE_EXPECT_EQ(default_loaded.value().navigation_profile, app::NavigationProfile::Fe3dHybrid);

    app::AppPreferences custom{};
    custom.ui_scale_percent = 116u; // Snaps to 115
    custom.startup_environment = app::StartupEnvironment::DefaultScene;
    custom.default_author = "  Studio Pipeline Lead  ";
    custom.navigation_profile = app::NavigationProfile::Maya;
    custom.recent_projects_limit = 35u;
    custom.autosave_enabled = true;
    custom.autosave_interval_seconds = 120u;
    custom.show_viewport_hud = false;
    custom.keymap_overrides = {{"palette.open", "Ctrl+Shift+P"}};

    FE_REQUIRE_TRUE(static_cast<bool>(app::AppPreferencesStore::saveToFile(custom, prefs_path, rig.mem_fs)));

    // Open a completely different .fe3d project and verify preferences remain untouched.
    FE_REQUIRE_TRUE(static_cast<bool>(rig.editor->createNewProject(
        app::ProjectTemplate::ShaderBall, "External Asset", "External Author")));

    auto reloaded = app::AppPreferencesStore::loadOrDefault(prefs_path, rig.mem_fs);
    FE_REQUIRE_TRUE(reloaded.hasValue());
    FE_EXPECT_EQ(reloaded.value().ui_scale_percent, 115u);
    FE_EXPECT_EQ(reloaded.value().startup_environment, app::StartupEnvironment::DefaultScene);
    FE_EXPECT_EQ(reloaded.value().default_author, std::string("Studio Pipeline Lead"));
    FE_EXPECT_EQ(reloaded.value().navigation_profile, app::NavigationProfile::Maya);
    FE_EXPECT_EQ(reloaded.value().recent_projects_limit, 35u);
    FE_EXPECT_EQ(reloaded.value().autosave_interval_seconds, 120u);
    FE_EXPECT_FALSE(reloaded.value().show_viewport_hud);
    FE_REQUIRE_TRUE(reloaded.value().keymap_overrides.size() == 1u);
    FE_EXPECT_EQ(reloaded.value().keymap_overrides[0].first, std::string("palette.open"));
    FE_EXPECT_EQ(reloaded.value().keymap_overrides[0].second, std::string("Ctrl+Shift+P"));
}

FE_TEST(Stage1Hub, AllFourProjectTemplatesInitializeValidProjects) {
    HeadlessHubRig rig;
    FE_REQUIRE_TRUE(rig.init());

    const auto& templates = app::TemplateCatalog::all();
    FE_REQUIRE_TRUE(templates.size() == 4u);

    for (const auto& desc : templates) {
        const auto parsed = app::TemplateCatalog::parseSlug(desc.slug);
        FE_REQUIRE_TRUE(parsed.has_value());
        FE_EXPECT_EQ(parsed.value(), desc.id);

        const std::string title = std::string("Test ") + std::string(desc.title);
        const std::filesystem::path save_path =
            std::filesystem::path("/virtual/projects") / (std::string(desc.slug) + ".fe3d");

        auto create_st = app::createNewProjectAtPath(
            *rig.editor,
            desc.id,
            title,
            "QA Artist",
            save_path,
            false,
            rig.mem_fs);
        FE_REQUIRE_TRUE(static_cast<bool>(create_st));
        FE_EXPECT_FALSE(rig.editor->isDocumentDirty());
        FE_EXPECT_EQ(rig.editor->activeProjectPath(), save_path.generic_string());
        FE_EXPECT_EQ(rig.session->document().metadata.project_name, title);
        FE_EXPECT_EQ(rig.session->document().scene.objectCount(), static_cast<std::size_t>(desc.expected_objects));
        FE_EXPECT_EQ(rig.session->document().scene.meshCount(), static_cast<std::size_t>(desc.expected_meshes));

        // Verify saved .fe3d deserializes cleanly without corruption.
        auto disk_doc = project::ProjectSerializer::loadFromFile(save_path, rig.mem_fs);
        FE_REQUIRE_TRUE(disk_doc.hasValue());
        FE_EXPECT_EQ(disk_doc.value().metadata.project_name, title);
    }
}

FE_TEST(Stage1Hub, NewProjectValidationAndOverwriteProtection) {
    HeadlessHubRig rig;
    FE_REQUIRE_TRUE(rig.init());

    // Empty name is rejected.
    auto empty_name_st = app::createNewProjectAtPath(
        *rig.editor,
        app::ProjectTemplate::MinimalCube,
        "   ",
        "Author",
        "/virtual/projects/cube.fe3d",
        false,
        rig.mem_fs);
    FE_EXPECT_FALSE(static_cast<bool>(empty_name_st));
    FE_EXPECT_EQ(empty_name_st.error().code, core::ErrorCode::InvalidArgument);

    // First creation succeeds.
    auto first_st = app::createNewProjectAtPath(
        *rig.editor,
        app::ProjectTemplate::MinimalCube,
        "First Cube",
        "Author",
        "/virtual/projects/cube.fe3d",
        false,
        rig.mem_fs);
    FE_REQUIRE_TRUE(static_cast<bool>(first_st));

    // Second creation at the same path without confirmation is refused with AlreadyExists.
    auto conflict_st = app::createNewProjectAtPath(
        *rig.editor,
        app::ProjectTemplate::ShaderBall,
        "Overwritten Project",
        "Author",
        "/virtual/projects/cube.fe3d",
        false,
        rig.mem_fs);
    FE_EXPECT_FALSE(static_cast<bool>(conflict_st));
    FE_EXPECT_EQ(conflict_st.error().code, core::ErrorCode::AlreadyExists);

    // Original file on disk remains untouched until explicit overwrite confirmation.
    auto preserved_doc = project::ProjectSerializer::loadFromFile("/virtual/projects/cube.fe3d", rig.mem_fs);
    FE_REQUIRE_TRUE(preserved_doc.hasValue());
    FE_EXPECT_EQ(preserved_doc.value().metadata.project_name, std::string("First Cube"));

    // Explicit allow_overwrite == true succeeds.
    auto confirmed_st = app::createNewProjectAtPath(
        *rig.editor,
        app::ProjectTemplate::ShaderBall,
        "Confirmed Overwrite",
        "Author",
        "/virtual/projects/cube.fe3d",
        true,
        rig.mem_fs);
    FE_REQUIRE_TRUE(static_cast<bool>(confirmed_st));
    auto updated_doc = project::ProjectSerializer::loadFromFile("/virtual/projects/cube.fe3d", rig.mem_fs);
    FE_REQUIRE_TRUE(updated_doc.hasValue());
    FE_EXPECT_EQ(updated_doc.value().metadata.project_name, std::string("Confirmed Overwrite"));
}

FE_TEST(Stage1Hub, RecentProjectsSearchSortPinAndDiskStatus) {
    HeadlessHubRig rig;
    FE_REQUIRE_TRUE(rig.init());

    app::RecentProjectsIndex index{};
    FE_EXPECT_TRUE(index.entries().empty());

    FE_REQUIRE_TRUE(static_cast<bool>(app::createNewProjectAtPath(
        *rig.editor, app::ProjectTemplate::MinimalCube, "Alpha Robot", "Ada",
        "/virtual/projects/alpha.fe3d", true, rig.mem_fs)));
    const auto rec_a = index.recordProject(
        rig.session->document(), "/virtual/projects/alpha.fe3d", 2048u, 1000u, 10u);

    FE_REQUIRE_TRUE(static_cast<bool>(app::createNewProjectAtPath(
        *rig.editor, app::ProjectTemplate::ShaderBall, "Zeta LookDev", "Linus",
        "/virtual/projects/zeta.fe3d", true, rig.mem_fs)));
    const auto rec_z = index.recordProject(
        rig.session->document(), "/virtual/projects/zeta.fe3d", 8192u, 2000u, 10u);

    // Search filter
    const auto filtered = index.query("lookdev");
    FE_REQUIRE_TRUE(filtered.size() == 1u);
    FE_EXPECT_EQ(filtered[0].id, rec_z.id);

    // Pinning keeps Alpha Robot first even though Zeta LookDev was opened later
    FE_EXPECT_TRUE(index.setPinned(rec_a.id, true));
    const auto ordered = index.query("", app::RecentSortMode::LastOpenedDesc, false);
    FE_REQUIRE_TRUE(ordered.size() == 2u);
    FE_EXPECT_EQ(ordered[0].id, rec_a.id);
    FE_EXPECT_EQ(ordered[1].id, rec_z.id);

    // Persist and reload across session restart; remove zeta.fe3d on disk to verify exists_on_disk honesty
    const std::filesystem::path idx_path = "/virtual/config/recent_projects.json";
    FE_REQUIRE_TRUE(static_cast<bool>(index.saveToFile(idx_path, rig.mem_fs)));
    rig.mem_fs.eraseFile("/virtual/projects/zeta.fe3d");

    app::RecentProjectsIndex reloaded{};
    FE_REQUIRE_TRUE(static_cast<bool>(reloaded.loadFromFile(idx_path, rig.mem_fs)));
    const auto all_reloaded = reloaded.query();
    FE_REQUIRE_TRUE(all_reloaded.size() == 2u);
    FE_EXPECT_TRUE(all_reloaded[0].exists_on_disk);
    FE_EXPECT_FALSE(all_reloaded[1].exists_on_disk);
}

FE_TEST(Stage1Hub, RecoveryServiceHonestStatesRingBufferAndUncleanDetection) {
    HeadlessHubRig rig;
    FE_REQUIRE_TRUE(rig.init());

    // 1. Unavailable storage state when filesystem rejects directory creation
    rig.mem_fs.fail_writes = true;
    app::RecoveryService unavailable_svc{};
    FE_EXPECT_FALSE(static_cast<bool>(unavailable_svc.initialize("/virtual/recovery_ro", rig.mem_fs)));
    FE_EXPECT_EQ(unavailable_svc.storageState(), app::RecoveryStorageState::Unavailable);
    FE_EXPECT_TRUE(unavailable_svc.snapshots().empty());
    rig.mem_fs.fail_writes = false;

    // 2. Available storage state with empty recovery list (zero fabricated records)
    app::RecoveryService svc{};
    FE_REQUIRE_TRUE(static_cast<bool>(svc.initialize("/virtual/recovery", rig.mem_fs)));
    FE_EXPECT_EQ(svc.storageState(), app::RecoveryStorageState::Available);
    FE_EXPECT_FALSE(svc.hadUncleanPreviousSession());
    FE_EXPECT_TRUE(svc.snapshots().empty());

    // 3. Capture 4 snapshots to verify 3-slot ring buffer rotation
    for (int i = 1; i <= 4; ++i) {
        const std::string name = "Autosaved Scene " + std::to_string(i);
        FE_REQUIRE_TRUE(static_cast<bool>(rig.editor->createNewProject(
            app::ProjectTemplate::MinimalCube, name, "Artist")));
        rig.session->syncDocumentState(rig.editor->selectedMaterial());
        auto snap_res = svc.captureSnapshot(
            rig.session->document(),
            "/virtual/projects/work.fe3d",
            "Periodic Autosave",
            static_cast<std::uint64_t>(1000 * i),
            rig.mem_fs);
        FE_REQUIRE_TRUE(snap_res.hasValue());
    }
    FE_EXPECT_EQ(svc.snapshots().size(), static_cast<std::size_t>(app::RecoveryService::kMaxRingSlots));

    // 4. Simulate unclean crash (do NOT call markCleanShutdown) and restart RecoveryService
    app::RecoveryService restarted_after_crash{};
    FE_REQUIRE_TRUE(static_cast<bool>(restarted_after_crash.initialize("/virtual/recovery", rig.mem_fs)));
    FE_EXPECT_TRUE(restarted_after_crash.hadUncleanPreviousSession());
    FE_REQUIRE_TRUE(restarted_after_crash.snapshots().size() == 3u);
    FE_EXPECT_TRUE(restarted_after_crash.snapshots().front().unclean_session);

    auto recovered_doc = restarted_after_crash.recoverSnapshot("latest", rig.mem_fs);
    FE_REQUIRE_TRUE(recovered_doc.hasValue());
    FE_EXPECT_EQ(recovered_doc.value().metadata.project_name, std::string("Autosaved Scene 4"));

    // 5. Clean shutdown clears active session marker
    FE_REQUIRE_TRUE(static_cast<bool>(restarted_after_crash.markCleanShutdown(rig.mem_fs)));
    app::RecoveryService clean_restart{};
    FE_REQUIRE_TRUE(static_cast<bool>(clean_restart.initialize("/virtual/recovery", rig.mem_fs)));
    FE_EXPECT_FALSE(clean_restart.hadUncleanPreviousSession());
}

FE_TEST(Stage1Hub, ExactDirtyStateAcrossEditsUndoRedoBranchAndJsonBridge) {
    HeadlessHubRig rig;
    FE_REQUIRE_TRUE(rig.init());

    FE_REQUIRE_TRUE(static_cast<bool>(rig.editor->executeActionJson(
        R"({"action":"create_new_project","template":"minimal_cube","project_name":"Precision Rig","author":"Eng","fps":30,"units":"centimeters","file_path":"/virtual/precision.fe3d"})")));
    FE_EXPECT_FALSE(rig.editor->isDocumentDirty());
    FE_EXPECT_NEAR(rig.session->document().scene.timeline().fps, 30.0f, 1e-4f);

    // Mutate scene -> becomes dirty
    FE_REQUIRE_TRUE(static_cast<bool>(rig.editor->executeActionJson(
        R"({"action":"add_preset","preset":"sphere"})")));
    FE_EXPECT_TRUE(rig.editor->isDocumentDirty());

    // Undo back to initial state -> becomes clean again!
    FE_REQUIRE_TRUE(static_cast<bool>(rig.editor->undo()));
    FE_EXPECT_FALSE(rig.editor->isDocumentDirty());

    // Redo -> dirty again, then save -> clean at depth 1
    FE_REQUIRE_TRUE(static_cast<bool>(rig.editor->redo()));
    FE_EXPECT_TRUE(rig.editor->isDocumentDirty());
    FE_REQUIRE_TRUE(static_cast<bool>(rig.editor->executeActionJson(
        R"({"action":"mark_document_saved","file_path":"/virtual/precision.fe3d"})")));
    FE_EXPECT_FALSE(rig.editor->isDocumentDirty());

    // Undo once (depth 0 != saved token) -> dirty; then execute a DIFFERENT command (depth 1 again, new token!) -> MUST remain dirty!
    FE_REQUIRE_TRUE(static_cast<bool>(rig.editor->undo()));
    FE_EXPECT_TRUE(rig.editor->isDocumentDirty());
    FE_REQUIRE_TRUE(static_cast<bool>(rig.editor->executeActionJson(
        R"({"action":"add_preset","preset":"cylinder"})")));
    FE_EXPECT_TRUE(rig.editor->isDocumentDirty());

    // Verify JSON state exposes is_dirty, file_path, and units
    const auto state_parsed = project::parseJson(rig.editor->serializeStateJson());
    FE_REQUIRE_TRUE(state_parsed.hasValue());
    const auto* proj = state_parsed.value().findKey("project");
    FE_REQUIRE_TRUE(proj != nullptr && proj->isObject());
    FE_EXPECT_TRUE(proj->findKey("is_dirty")->asBool());
    FE_EXPECT_EQ(proj->findKey("file_path")->asString(), std::string("/virtual/precision.fe3d"));
    FE_EXPECT_EQ(proj->findKey("units")->asString(), std::string("centimeters"));

    // Verify querySystemInfo reports genuine device and version
    const auto sys_info = app::querySystemInfo(*rig.session, app::RecoveryStorageState::Available);
    FE_EXPECT_FALSE(sys_info.engine_version.empty());
    FE_EXPECT_FALSE(sys_info.platform_name.empty());
    FE_EXPECT_FALSE(sys_info.graphics_backend.empty());
}

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
