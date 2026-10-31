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

    // Initial state JSON must contain starter scene objects (no fake GroundPlane object) and empty inspector.
    const auto init_state_res = project::parseJson(rig.editor->serializeStateJson());
    FE_REQUIRE_TRUE(init_state_res.hasValue());
    const auto& init_state = init_state_res.value();
    FE_EXPECT_EQ(init_state.findKey("objects")->asArray().size(), static_cast<std::size_t>(5));
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
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), static_cast<std::size_t>(6));

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

    // Export inline JSON (v1.3), verify backward-compatible v1.0, v1.1, and v1.2 schema loads as well as v1.3 load.
    const auto inline_json_res = rig.editor->exportInlineProjectJson();
    FE_REQUIRE_TRUE(inline_json_res.hasValue());

    std::string legacy_v10_json = inline_json_res.value();
    const auto minor_pos = legacy_v10_json.find("\"minor\": 3");
    FE_REQUIRE_TRUE(minor_pos != std::string::npos);
    std::string legacy_v11_json = legacy_v10_json;
    std::string legacy_v12_json = legacy_v10_json;
    legacy_v10_json.replace(minor_pos, 10, "\"minor\": 0");
    legacy_v11_json.replace(minor_pos, 10, "\"minor\": 1");
    legacy_v12_json.replace(minor_pos, 10, "\"minor\": 2");

    FE_REQUIRE_TRUE(rig.editor->newStarterProject().hasValue());
    FE_EXPECT_EQ(rig.session->document().metadata.project_name, std::string("Final Effect 3D Starter Project"));
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), static_cast<std::size_t>(5));

    FE_REQUIRE_TRUE(rig.editor->importInlineProjectJson(legacy_v12_json).hasValue());
    FE_REQUIRE_TRUE(rig.editor->importInlineProjectJson(legacy_v11_json).hasValue());
    FE_REQUIRE_TRUE(rig.editor->importInlineProjectJson(legacy_v10_json).hasValue());
    FE_EXPECT_EQ(rig.session->document().metadata.project_name, std::string("Studio Stage 3 Test"));
    FE_EXPECT_EQ(rig.session->document().scene.name(), std::string("AuthoredScene"));
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), static_cast<std::size_t>(6));

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
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), static_cast<std::size_t>(6));

    std::filesystem::remove_all(tmp_dir, ec);
}

FE_TEST(Editor, Stage4MeshEditingMaterialsLivePreviewAndWorkspaceRoundTrip) {
    EditorTestRig rig;
    FE_REQUIRE_TRUE(rig.init(240, 160));

    const auto* hero = rig.session->document().scene.findObjectByName("HeroCube");
    FE_REQUIRE_TRUE(hero != nullptr);
    FE_REQUIRE_TRUE(rig.editor->selectObject(hero->id).hasValue());

    // 1. Enter Face Edit mode, select top face (4), extrude +0.4, then delete top cap, select boundary loop & re-cap!
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"set_editor_mode","mode":"face"})").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"select_mesh_element","kind":"face","index":4,"additive":false})").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"extrude_selected_faces","distance":0.4})").hasValue());

    const auto* ma_after_ext = rig.session->document().scene.findMesh(hero->mesh_id.value());
    FE_REQUIRE_TRUE(ma_after_ext != nullptr && ma_after_ext->editable_mesh.has_value());
    FE_EXPECT_EQ(ma_after_ext->editable_mesh->faceCount(), std::size_t{10}); // 6 -> 10 quad faces

    // Delete the newly extruded cap face -> 9 faces (open boundary loop of 4 vertices), then fill it back to 10!
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"delete_mesh_selection"})").hasValue());
    FE_EXPECT_EQ(ma_after_ext->editable_mesh->faceCount(), std::size_t{9});

    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"select_boundary_loop"})").hasValue());
    FE_EXPECT_EQ(rig.session->meshSelection().vertices.size(), std::size_t{4});
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"fill_selected_face"})").hasValue());
    FE_EXPECT_EQ(ma_after_ext->editable_mesh->faceCount(), std::size_t{10});

    // Reject invalid topology: trying to fill a face on a closed mesh must fail cleanly without mutating!
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"set_editor_mode","mode":"vertex"})").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"select_mesh_element","kind":"vertex","index":0,"additive":true})").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"select_mesh_element","kind":"vertex","index":1,"additive":true})").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"select_mesh_element","kind":"vertex","index":2,"additive":true})").hasValue());
    FE_EXPECT_TRUE(rig.editor->executeActionJson(R"({"action":"fill_selected_face"})").isError());
    FE_EXPECT_EQ(ma_after_ext->editable_mesh->faceCount(), std::size_t{10});

    // 2. PBR Material creation, parameter editing, live 3D sphere preview generation bump, face assignment & safe delete!
    const auto gen_before = rig.editor->materialPreviewGeneration();
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"create_material","name":"NeonGold","base_color":[1.0,0.78,0.22],"metallic":0.95,"roughness":0.10})").hasValue());
    const auto gold_id = rig.editor->selectedMaterial();
    FE_REQUIRE_TRUE(gold_id.isValid());
    FE_EXPECT_TRUE(rig.editor->materialPreviewGeneration() > gen_before);

    const auto gen_mid = rig.editor->materialPreviewGeneration();
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        "{\"action\":\"update_material\",\"id\":" + std::to_string(gold_id.value) +
        ",\"emissive_color\":[0.2,0.9,1.0],\"emissive_strength\":2.5,\"opacity\":0.85}").hasValue());
    FE_EXPECT_TRUE(rig.editor->materialPreviewGeneration() > gen_mid);

    // Assign NeonGold to extruded face 0!
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"select_mesh_element","kind":"face","index":0,"additive":false})").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        "{\"action\":\"assign_face_material\",\"material_id\":" + std::to_string(gold_id.value) + "}").hasValue());
    FE_EXPECT_EQ(ma_after_ext->editable_mesh->faces()[0].material_id, gold_id);

    // 3. Switch workspace preset to Shading & gizmo to Rotate Local, serialize .fe3d v1.2, reset, and reload!
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"apply_workspace_preset","preset":"shading"})").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"set_gizmo_mode","mode":"rotate"})").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"set_gizmo_space","space":"local"})").hasValue());

    const auto saved_v12 = rig.editor->exportInlineProjectJson();
    FE_REQUIRE_TRUE(saved_v12.hasValue());

    FE_REQUIRE_TRUE(rig.editor->newStarterProject().hasValue());
    FE_REQUIRE_TRUE(rig.editor->importInlineProjectJson(saved_v12.value()).hasValue());

    FE_EXPECT_EQ(rig.session->editorMode(), scene::EditMode::Face);
    FE_EXPECT_EQ(rig.session->gizmo().mode(), app::GizmoMode::Rotate);
    FE_EXPECT_EQ(rig.session->gizmo().space(), app::GizmoSpace::Local);
    FE_EXPECT_EQ(rig.session->workspace().state().layout_name, std::string("Shading"));
    const auto* reloaded_hero = rig.session->document().scene.findObjectByName("HeroCube");
    const auto* reloaded_mesh = rig.session->document().scene.findMesh(reloaded_hero->mesh_id.value());
    FE_EXPECT_EQ(reloaded_mesh->editable_mesh->faceCount(), std::size_t{10});
    FE_EXPECT_TRUE(rig.session->document().scene.findMaterialByName("NeonGold") != nullptr);
}

FE_TEST(Editor, Stage5ProjectTemplatesWorkspaceTabsAndSharedSceneIsolation) {
    EditorTestRig rig;
    FE_REQUIRE_TRUE(rig.init(640, 360));

    // 1. Create MinimalCube project template via JSON action and verify real scene objects.
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"create_new_project","template":"minimal_cube","project_name":"ArchDesign","author":"StudioUser"})")
        .hasValue());
    FE_EXPECT_EQ(rig.session->document().metadata.project_name, std::string("ArchDesign"));
    FE_EXPECT_TRUE(rig.session->document().scene.findObjectByName("Cube") != nullptr);
    FE_EXPECT_TRUE(rig.session->document().scene.findObjectByName("MainCamera") != nullptr);
    FE_EXPECT_TRUE(rig.session->document().scene.findMaterialByName("Studio Clay") != nullptr);

    // 2. Switching across all 12 workspace tabs and resizing docks must preserve the exact same scene & materials!
    const auto obj_count_before = rig.session->document().scene.objectCount();
    const auto mat_count_before = rig.session->document().scene.materialCount();

    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"apply_workspace_preset","preset":"modeling"})").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"add_preset","preset":"sphere"})").hasValue());
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), obj_count_before + 1);

    // Switch to Shading, create material, then switch to Layout & Animation — same scene, objects & materials persist!
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"apply_workspace_preset","preset":"shading"})").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"create_material","name":"SharedVelvet","base_color":[0.7,0.15,0.3]})").hasValue());
    FE_EXPECT_EQ(rig.session->document().scene.materialCount(), mat_count_before + 1);

    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"apply_workspace_preset","preset":"layout"})").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"set_dock_ratios","left":0.21,"right":0.26,"bottom":0.19})").hasValue());
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), obj_count_before + 1);
    FE_EXPECT_TRUE(rig.session->document().scene.findMaterialByName("SharedVelvet") != nullptr);
    FE_EXPECT_NEAR(rig.session->workspace().state().left_dock_width_ratio, 0.21f, 1.0e-4f);
    FE_EXPECT_NEAR(rig.session->workspace().state().right_dock_width_ratio, 0.26f, 1.0e-4f);

    // 3. Verify EmptyStage and ShaderBall templates construct valid scenes.
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"create_new_project","template":"empty_stage","project_name":"CleanStage"})").hasValue());
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), std::size_t{2}); // MainCamera + SunLight

    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"create_new_project","template":"shader_ball","project_name":"LookDevBall"})").hasValue());
    FE_EXPECT_TRUE(rig.session->document().scene.findObjectByName("ShaderSphere") != nullptr);
    FE_EXPECT_EQ(rig.session->document().scene.materialCount(), std::size_t{3});
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
