// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "test_harness.hpp"

#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/platform/window.hpp"
#include "finaleffect/project/json.hpp"
#include "finaleffect/rendering/software_render_device.hpp"

#include <filesystem>
#include <memory>

namespace {

using namespace finaleffect;

struct EditorTestRig {
    platform::LocalFileSystem fs;
    platform::HeadlessWindow window;
    rendering::SoftwareRenderDevice* raw_device{nullptr};
    std::unique_ptr<core::Engine> engine;
    std::unique_ptr<app::StudioSession> session;
    std::unique_ptr<app::EditorController> editor;

    bool init(std::uint32_t width = 320, std::uint32_t height = 200) {
        if (!window.initialize(platform::WindowDesc{
                .title = "EditorTests",
                .width = width,
                .height = height,
            }).hasValue()) {
            return false;
        }

        core::EngineConfig cfg;
        cfg.minimum_log_level = core::LogLevel::Error;
        engine = std::make_unique<core::Engine>(cfg);

        auto sw = std::make_unique<rendering::SoftwareRenderDevice>();
        raw_device = sw.get();

        session = std::make_unique<app::StudioSession>(
            window,
            std::move(sw),
            app::StudioSessionConfig{.animate_showcase_rig = false});

        if (!engine->initialize(session.get()).hasValue()) {
            return false;
        }
        editor = std::make_unique<app::EditorController>(*session);
        return true;
    }

    ~EditorTestRig() {
        if (engine != nullptr) {
            engine->shutdown();
        }
        window.shutdown();
    }
};

FE_TEST(Editor, AuthoringWorkflowUndoRedoAndSelectionHighlight) {
    EditorTestRig rig;
    FE_REQUIRE_TRUE(rig.init(240, 160));

    // Initial state JSON must contain starter scene objects and empty inspector.
    const auto init_state_res = project::parseJson(rig.editor->serializeStateJson());
    FE_REQUIRE_TRUE(init_state_res.hasValue());
    const auto& init_state = init_state_res.value();
    FE_EXPECT_EQ(init_state.findKey("objects")->asArray().size(), static_cast<std::size_t>(6));
    FE_EXPECT_TRUE(init_state.findKey("inspector")->isNull());

    // Render baseline frame without selection, then pick center HeroCube and render with selection highlight.
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    const auto unselected_pixels = rig.raw_device->colorBuffer();

    const auto hit = rig.editor->pickViewportPixel(120.0f, 80.0f);
    FE_REQUIRE_TRUE(hit.has_value());
    const auto hero_id = hit->object_id;
    FE_EXPECT_EQ(rig.session->document().scene.findObject(hero_id)->name, std::string("HeroCube"));

    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    const auto selected_pixels = rig.raw_device->colorBuffer();
    FE_EXPECT_TRUE(unselected_pixels != selected_pixels);

    // Spawn preset Pyramid via JSON action, verify shared mesh reuse, rename, transform, undo & redo.
    const std::size_t initial_mesh_count = rig.session->document().scene.meshCount();
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"add_preset","preset":"pyramid"})").hasValue());
    FE_EXPECT_EQ(rig.session->document().scene.meshCount(), initial_mesh_count);
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), static_cast<std::size_t>(7));

    const auto spawned_id = rig.session->selectedObject();
    FE_REQUIRE_TRUE(spawned_id.isValid());
    FE_EXPECT_EQ(rig.session->document().scene.findObject(spawned_id)->name, std::string("Pyramid"));

    // Rename & set Euler transform via JSON action.
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        "{\"action\":\"rename_object\",\"id\":" + std::to_string(spawned_id.value) +
        ",\"name\":\"CrystalSpire\"}").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        "{\"action\":\"set_transform\",\"id\":" + std::to_string(spawned_id.value) +
        ",\"position\":[2.0,1.5,-1.0],\"rotation_deg\":[15.0,45.0,0.0],\"scale\":[1.2,1.5,1.2]}").hasValue());

    const auto* spire = rig.session->document().scene.findObject(spawned_id);
    FE_REQUIRE_TRUE(spire != nullptr);
    FE_EXPECT_EQ(spire->name, std::string("CrystalSpire"));
    FE_EXPECT_NEAR(spire->local_transform.position.x, 2.0f, 1e-5f);

    // Verify Inspector JSON reflects selected CrystalSpire.
    const auto state_after = project::parseJson(rig.editor->serializeStateJson()).value();
    const auto* insp = state_after.findKey("inspector");
    FE_REQUIRE_TRUE(insp != nullptr && insp->isObject());
    FE_EXPECT_EQ(insp->findKey("name")->asString(), std::string("CrystalSpire"));
    FE_EXPECT_EQ(insp->findKey("mesh")->findKey("name")->asString(), std::string("AccentPyramidMesh"));

    // Undo transform, undo rename, undo add_preset -> object removed and selection cleared cleanly!
    FE_REQUIRE_TRUE(rig.editor->undo().hasValue());
    FE_EXPECT_NEAR(rig.session->document().scene.findObject(spawned_id)->local_transform.position.y, 0.75f, 1e-5f);

    FE_REQUIRE_TRUE(rig.editor->undo().hasValue());
    FE_EXPECT_EQ(rig.session->document().scene.findObject(spawned_id)->name, std::string("Pyramid"));

    FE_REQUIRE_TRUE(rig.editor->undo().hasValue());
    FE_EXPECT_TRUE(rig.session->document().scene.findObject(spawned_id) == nullptr);
    FE_EXPECT_FALSE(rig.session->selectedObject().isValid());

    // Redo restores the exact same ObjectId!
    FE_REQUIRE_TRUE(rig.editor->redo().hasValue());
    FE_EXPECT_TRUE(rig.session->document().scene.findObject(spawned_id) != nullptr);

    // Protecting active camera from deletion must return InvalidState error cleanly.
    const auto cam_id = rig.session->document().scene.activeCamera();
    FE_REQUIRE_TRUE(rig.editor->selectObject(cam_id).hasValue());
    const auto del_cam_st = rig.editor->deleteSelectedObject();
    FE_EXPECT_TRUE(del_cam_st.isError());
    FE_EXPECT_EQ(del_cam_st.error().code, core::ErrorCode::InvalidState);
    FE_EXPECT_TRUE(rig.editor->lastStatusIsError());
}

FE_TEST(Editor, InlineJsonAndBinaryBundleProjectRoundTrip) {
    EditorTestRig rig;
    FE_REQUIRE_TRUE(rig.init(160, 120));

    FE_REQUIRE_TRUE(rig.editor->updateProjectMetadata("Studio Stage 3 Test", "QA", "AuthoredScene").hasValue());
    const auto added_light = rig.editor->addPresetObject(app::ScenePreset::DirectionalLight);
    FE_REQUIRE_TRUE(added_light.hasValue());
    FE_REQUIRE_TRUE(rig.editor->setDirectionalLightComponent(
        added_light.value(),
        scene::DirectionalLightComponent{.color = {0.8f, 0.9f, 1.0f}, .intensity = 2.25f}).hasValue());

    // Export inline JSON, verify backward-compatible v1.0 schema load as well as v1.1 load.
    const auto inline_json_res = rig.editor->exportInlineProjectJson();
    FE_REQUIRE_TRUE(inline_json_res.hasValue());

    std::string legacy_v10_json = inline_json_res.value();
    const auto minor_pos = legacy_v10_json.find("\"minor\": 1");
    FE_REQUIRE_TRUE(minor_pos != std::string::npos);
    legacy_v10_json.replace(minor_pos, 10, "\"minor\": 0");

    FE_REQUIRE_TRUE(rig.editor->newStarterProject().hasValue());
    FE_EXPECT_EQ(rig.session->document().metadata.project_name, std::string("Final Effect 3D Starter Project"));
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), static_cast<std::size_t>(6));

    FE_REQUIRE_TRUE(rig.editor->importInlineProjectJson(legacy_v10_json).hasValue());
    FE_EXPECT_EQ(rig.session->document().metadata.project_name, std::string("Studio Stage 3 Test"));
    FE_EXPECT_EQ(rig.session->document().scene.name(), std::string("AuthoredScene"));
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), static_cast<std::size_t>(7));

    // Export v1.1 + .fe3dbin bundle to disk and reload via EditorController.
    const auto tmp_dir = std::filesystem::temp_directory_path() / "fe3d_editor_bundle_test";
    std::error_code ec;
    std::filesystem::remove_all(tmp_dir, ec);
    const auto bundle_fe3d = tmp_dir / "stage3_studio.fe3d";
    const auto bundle_bin = tmp_dir / "stage3_studio.fe3dbin";

    FE_REQUIRE_TRUE(rig.editor->saveBundleProject(bundle_fe3d, rig.fs).hasValue());
    FE_EXPECT_TRUE(rig.fs.exists(bundle_fe3d));
    FE_EXPECT_TRUE(rig.fs.exists(bundle_bin));

    FE_REQUIRE_TRUE(rig.editor->newStarterProject().hasValue());
    FE_REQUIRE_TRUE(rig.editor->loadProjectFile(bundle_fe3d, rig.fs).hasValue());
    FE_EXPECT_EQ(rig.session->document().metadata.project_name, std::string("Studio Stage 3 Test"));
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), static_cast<std::size_t>(7));

    std::filesystem::remove_all(tmp_dir, ec);
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
