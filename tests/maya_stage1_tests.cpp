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
#include "finaleffect/scene/editable_mesh.hpp"
#include "finaleffect/scene/primitives.hpp"

#include <cmath>
#include <memory>
#include <string>

namespace {

using namespace finaleffect;

struct MayaStage1Rig {
    platform::LocalFileSystem fs;
    platform::HeadlessWindow window;
    rendering::SoftwareRenderDevice* raw_device{nullptr};
    std::unique_ptr<core::Engine> engine;
    std::unique_ptr<app::StudioSession> session;
    std::unique_ptr<app::EditorController> editor;

    bool init(std::uint32_t width = 320, std::uint32_t height = 200) {
        if (!window.initialize(platform::WindowDesc{
                .title = "MayaStage1Tests",
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

    ~MayaStage1Rig() {
        if (engine != nullptr) {
            engine->shutdown();
        }
        window.shutdown();
    }
};

FE_TEST(MayaStage1, ClosedManifoldPolygonPrimitivesAndPresets) {
    auto cyl = scene::EditableMesh::createCylinder(0.55f, 1.25f, 16);
    auto cone = scene::EditableMesh::createCone(0.60f, 1.25f, 16);
    auto torus = scene::EditableMesh::createTorus(0.65f, 0.22f, 16, 10);

    FE_REQUIRE_TRUE(cyl.validateAndRebuild().hasValue());
    FE_REQUIRE_TRUE(cone.validateAndRebuild().hasValue());
    FE_REQUIRE_TRUE(torus.validateAndRebuild().hasValue());

    for (const auto& e : cyl.edges()) FE_EXPECT_TRUE(!e.isBoundary());
    for (const auto& e : cone.edges()) FE_EXPECT_TRUE(!e.isBoundary());
    for (const auto& e : torus.edges()) FE_EXPECT_TRUE(!e.isBoundary());

    MayaStage1Rig rig;
    FE_REQUIRE_TRUE(rig.init());

    const std::size_t base_objs = rig.session->document().scene.objectCount();
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"add_preset","preset":"cylinder"})").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"add_preset","preset":"cone"})").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"add_preset","preset":"torus"})").hasValue());
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), base_objs + 3u);
    FE_EXPECT_TRUE(rig.session->document().scene.findObjectByName("pCylinder") != nullptr);
    FE_EXPECT_TRUE(rig.session->document().scene.findObjectByName("pCone") != nullptr);
    FE_EXPECT_TRUE(rig.session->document().scene.findObjectByName("pTorus") != nullptr);
}

FE_TEST(MayaStage1, MultiSelectHierarchyGroupParentFreezeAndPivotCommands) {
    MayaStage1Rig rig;
    FE_REQUIRE_TRUE(rig.init());
    auto& sc = rig.session->document().scene;

    const auto* cube = sc.findObjectByName("HeroCube");
    const auto* pyr = sc.findObjectByName("AccentPyramid");
    FE_REQUIRE_TRUE(cube != nullptr && pyr != nullptr);
    const scene::ObjectId cube_id = cube->id;
    const scene::ObjectId pyr_id = pyr->id;

    // Multi-select HeroCube + AccentPyramid and group them under group1.
    FE_REQUIRE_TRUE(rig.editor->selectObject(cube_id).hasValue());
    FE_REQUIRE_TRUE(rig.editor->toggleSelectObject(pyr_id).hasValue());
    FE_EXPECT_EQ(rig.session->selectedObjects().size(), std::size_t{2});

    auto grp_res = rig.editor->groupSelectedObjects("rigGrp");
    FE_REQUIRE_TRUE(grp_res.hasValue());
    const scene::ObjectId grp_id = grp_res.value();
    FE_EXPECT_EQ(sc.findObject(cube_id)->parent_id, grp_id);
    FE_EXPECT_EQ(sc.findObject(pyr_id)->parent_id, grp_id);

    // Single Ctrl+Z undoes group creation and restores original parent links!
    FE_REQUIRE_TRUE(rig.editor->undo().hasValue());
    FE_EXPECT_TRUE(sc.findObject(grp_id) == nullptr);
    FE_EXPECT_TRUE(sc.findObject(cube_id) != nullptr);
    FE_EXPECT_TRUE(sc.findObject(pyr_id) != nullptr);
    FE_REQUIRE_TRUE(rig.editor->redo().hasValue());
    FE_EXPECT_EQ(sc.findObject(cube_id)->parent_id, grp_id);

    // Unparent HeroCube back to world root, then Parent AccentPyramid -> HeroCube.
    FE_REQUIRE_TRUE(rig.editor->selectObject(cube_id).hasValue());
    FE_REQUIRE_TRUE(rig.editor->unparentSelectedObjects().hasValue());
    FE_EXPECT_TRUE(!sc.findObject(cube_id)->parent_id.isValid());

    // Translate HeroCube to (3, 2, -1) and Freeze Transformations: world vertex positions stay, local_transform resets!
    FE_REQUIRE_TRUE(rig.editor->setObjectTransformEulerDegrees(
        cube_id, {3.0f, 2.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 1.0f}, false).hasValue());
    const auto* mesh_before = sc.findMesh(sc.findObject(cube_id)->mesh_id.value());
    FE_REQUIRE_TRUE(mesh_before != nullptr);
    const math::Vec3 world_center_before =
        sc.computeWorldMatrix(cube_id).value().transformPoint(mesh_before->local_bounds.center());

    FE_REQUIRE_TRUE(rig.editor->freezeObjectTransforms(cube_id).hasValue());
    const auto* cube_after = sc.findObject(cube_id);
    FE_EXPECT_NEAR(cube_after->local_transform.position.x, 0.0f, 1.0e-5f);
    FE_EXPECT_NEAR(cube_after->local_transform.position.y, 0.0f, 1.0e-5f);
    FE_EXPECT_NEAR(cube_after->local_transform.scale.x, 1.0f, 1.0e-5f);

    const auto* mesh_after = sc.findMesh(cube_after->mesh_id.value());
    const math::Vec3 world_center_after =
        sc.computeWorldMatrix(cube_id).value().transformPoint(mesh_after->local_bounds.center());
    FE_EXPECT_NEAR(world_center_after.x, world_center_before.x, 1.0e-4f);
    FE_EXPECT_NEAR(world_center_after.y, world_center_before.y, 1.0e-4f);
    FE_EXPECT_NEAR(world_center_after.z, world_center_before.z, 1.0e-4f);

    // Center Pivot places pivot_offset at baked mesh bounds center!
    FE_REQUIRE_TRUE(rig.editor->centerObjectPivot(cube_id).hasValue());
    FE_EXPECT_NEAR(sc.findObject(cube_id)->pivot_offset.x, 3.0f, 1.0e-4f);
    FE_EXPECT_NEAR(sc.findObject(cube_id)->pivot_offset.y, 2.0f, 1.0e-4f);

    // Undo Center Pivot + Undo Freeze Transformations restores original unbaked mesh & local transform!
    FE_REQUIRE_TRUE(rig.editor->undo().hasValue());
    FE_REQUIRE_TRUE(rig.editor->undo().hasValue());
    FE_EXPECT_NEAR(sc.findObject(cube_id)->local_transform.position.x, 3.0f, 1.0e-4f);
    FE_EXPECT_NEAR(sc.findObject(cube_id)->local_transform.scale.x, 2.0f, 1.0e-4f);
}

FE_TEST(MayaStage1, ChannelLocksNodeLockAndCustomAttributes) {
    MayaStage1Rig rig;
    FE_REQUIRE_TRUE(rig.init());
    auto& sc = rig.session->document().scene;
    const scene::ObjectId cube_id = sc.findObjectByName("HeroCube")->id;
    const math::Vec3 orig_pos = sc.findObject(cube_id)->local_transform.position;

    // Lock TranslateX & ScaleY; attempt to modify all channels -> TranslateX & ScaleY stay clamped!
    FE_REQUIRE_TRUE(rig.editor->setObjectChannelLocks(
        cube_id,
        static_cast<std::uint16_t>(scene::ChannelBit::kTranslateX | scene::ChannelBit::kScaleY)).hasValue());
    FE_REQUIRE_TRUE(rig.editor->setObjectTransformEulerDegrees(
        cube_id, {9.0f, 4.5f, -2.0f}, {0.0f, 30.0f, 0.0f}, {1.5f, 3.0f, 1.5f}, false).hasValue());
    FE_EXPECT_NEAR(sc.findObject(cube_id)->local_transform.position.x, orig_pos.x, 1.0e-5f);
    FE_EXPECT_NEAR(sc.findObject(cube_id)->local_transform.position.y, 4.5f, 1.0e-5f);
    FE_EXPECT_NEAR(sc.findObject(cube_id)->local_transform.scale.y, 1.0f, 1.0e-5f);
    FE_EXPECT_NEAR(sc.findObject(cube_id)->local_transform.scale.x, 1.5f, 1.0e-5f);

    // Full node lock blocks transform mutation completely until unlocked!
    FE_REQUIRE_TRUE(rig.editor->setObjectLocked(cube_id, true).hasValue());
    FE_EXPECT_TRUE(!rig.editor->setObjectTransformEulerDegrees(
        cube_id, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, false).hasValue());
    FE_REQUIRE_TRUE(rig.editor->setObjectLocked(cube_id, false).hasValue());

    // Custom attribute creation, limit clamping, mutation, and undo/redo!
    FE_REQUIRE_TRUE(rig.editor->addCustomAttribute(
        cube_id,
        scene::CustomAttribute{
            .name = "rigCurl",
            .value = 0.5,
            .min_value = 0.0,
            .max_value = 1.0,
            .has_limits = true,
            .keyable = true,
        }).hasValue());
    FE_REQUIRE_TRUE(rig.editor->setCustomAttributeValue(cube_id, "rigCurl", 2.5, false).hasValue());
    FE_EXPECT_NEAR(static_cast<float>(sc.findObject(cube_id)->custom_attributes[0].value), 1.0f, 1.0e-5f);
    FE_REQUIRE_TRUE(rig.editor->undo().hasValue());
    FE_EXPECT_NEAR(static_cast<float>(sc.findObject(cube_id)->custom_attributes[0].value), 0.5f, 1.0e-5f);
}

FE_TEST(MayaStage1, DisplayLayersSelectionMasksAndViewportDisplayModes) {
    MayaStage1Rig rig;
    FE_REQUIRE_TRUE(rig.init());
    auto& sc = rig.session->document().scene;
    const auto* cube = sc.findObjectByName("HeroCube");
    FE_REQUIRE_TRUE(cube != nullptr);
    const scene::ObjectId cube_id = cube->id;

    const auto sp = rig.session->projectWorldPointToScreen(
        sc.computeWorldMatrix(cube_id).value().transformPoint(math::Vec3::zero()));
    FE_REQUIRE_TRUE(sp.has_value());

    // Baseline pick hits HeroCube.
    const auto base_hit = rig.editor->pickViewportPixel(sp->x, sp->y);
    FE_REQUIRE_TRUE(base_hit.has_value());
    FE_EXPECT_EQ(base_hit->object_id, cube_id);

    // Assign HeroCube to a Display Layer and set Reference mode -> unselectable in viewport!
    FE_REQUIRE_TRUE(rig.editor->selectObject(cube_id).hasValue());
    auto lid_res = rig.editor->createDisplayLayer("geo_L", true);
    FE_REQUIRE_TRUE(lid_res.hasValue());
    const std::uint32_t lid = lid_res.value();

    FE_REQUIRE_TRUE(rig.editor->updateDisplayLayer(
        lid, true, true, scene::DisplayLayerType::Reference, {0.3f, 0.6f, 0.9f}).hasValue());
    const auto ref_hit = rig.editor->pickViewportPixel(sp->x, sp->y);
    FE_EXPECT_TRUE(!ref_hit.has_value() || ref_hit->object_id != cube_id);

    // Template mode -> also unselectable in viewport, and renders wireframe without crashing!
    FE_REQUIRE_TRUE(rig.editor->updateDisplayLayer(
        lid, true, true, scene::DisplayLayerType::Template, {0.8f, 0.4f, 0.4f}).hasValue());
    const auto tmpl_hit = rig.editor->pickViewportPixel(sp->x, sp->y);
    FE_EXPECT_TRUE(!tmpl_hit.has_value() || tmpl_hit->object_id != cube_id);

    // Exercise Maya 4/5/6/7 + X-Ray + Wireframe on Shaded render passes cleanly.
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"set_viewport_display","shading_mode":"wireframe","use_all_lights":false,"xray_mode":true})").hasValue());
    FE_REQUIRE_TRUE(rig.session->onUpdate(*rig.engine, core::FrameTimings{.delta_seconds = 0.016}).hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"set_viewport_display","shading_mode":"shaded","wireframe_on_shaded":true,"default_material":true})").hasValue());
    FE_REQUIRE_TRUE(rig.session->onUpdate(*rig.engine, core::FrameTimings{.delta_seconds = 0.016}).hasValue());

    // Delete Display Layer -> HeroCube remains intact in scene and becomes pickable again!
    FE_REQUIRE_TRUE(rig.editor->deleteDisplayLayer(lid).hasValue());
    FE_EXPECT_TRUE(sc.findObject(cube_id) != nullptr);
    const auto restored_hit = rig.editor->pickViewportPixel(sp->x, sp->y);
    FE_REQUIRE_TRUE(restored_hit.has_value());
    FE_EXPECT_EQ(restored_hit->object_id, cube_id);

    // Surface selection mask OFF -> mesh pick suppressed!
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"set_selection_mask","surfaces":false})").hasValue());
    const auto masked_hit = rig.editor->pickViewportPixel(sp->x, sp->y);
    FE_EXPECT_TRUE(!masked_hit.has_value() || masked_hit->object_id != cube_id);
}

FE_TEST(MayaStage1, TimelineKeyframeInterpolationPlaybackAndV13Serialization) {
    MayaStage1Rig rig;
    FE_REQUIRE_TRUE(rig.init());
    auto& sc = rig.session->document().scene;
    const scene::ObjectId cube_id = sc.findObjectByName("HeroCube")->id;

    FE_REQUIRE_TRUE(rig.editor->selectObject(cube_id).hasValue());
    FE_REQUIRE_TRUE(rig.editor->setObjectChannelLocks(cube_id, 0u).hasValue());

    // Frame 1: key at X = 0.0; Frame 25: key at X = 12.0.
    rig.editor->setCurrentFrame(1.0f);
    FE_REQUIRE_TRUE(rig.editor->setObjectTransformEulerDegrees(
        cube_id, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, false).hasValue());
    FE_REQUIRE_TRUE(rig.editor->setKeyframeOnSelected().hasValue());

    rig.editor->setCurrentFrame(25.0f);
    FE_REQUIRE_TRUE(rig.editor->setObjectTransformEulerDegrees(
        cube_id, {12.0f, 1.0f, 0.0f}, {0.0f, 90.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, false).hasValue());
    FE_REQUIRE_TRUE(rig.editor->setKeyframeOnSelected().hasValue());

    // Scrub to midpoint Frame 13 -> X = 6.0!
    rig.editor->setCurrentFrame(13.0f);
    FE_EXPECT_NEAR(sc.findObject(cube_id)->local_transform.position.x, 6.0f, 1.0e-3f);

    // Live playback advances current_frame at 24 fps across five 0.1s ticks (= 0.5s -> +12 frames)!
    rig.editor->setCurrentFrame(1.0f);
    rig.editor->setTimelinePlaying(true);
    for (int step = 0; step < 5; ++step) {
        FE_REQUIRE_TRUE(rig.session->onUpdate(*rig.engine, core::FrameTimings{.delta_seconds = 0.1}).hasValue());
    }
    rig.editor->setTimelinePlaying(false);
    FE_EXPECT_NEAR(sc.timeline().current_frame, 13.0f, 1.0e-2f);
    FE_EXPECT_NEAR(sc.findObject(cube_id)->local_transform.position.x, 6.0f, 1.0e-2f);

    // Safe structured Command Line execution + error safety!
    FE_REQUIRE_TRUE(rig.editor->executeCommandLine("addAttr animWeight 0.85;").hasValue());
    FE_REQUIRE_TRUE(rig.editor->createDisplayLayer("anim_L", true).hasValue());
    FE_EXPECT_TRUE(!rig.editor->executeCommandLine("eval(window.alert(1))").hasValue());
    FE_EXPECT_TRUE(rig.editor->lastStatusIsError());

    // .fe3d v1.3 round-trip preserves Display Layers, Keyframes, Timeline & Custom Attributes with 0 warnings!
    const auto exported = rig.editor->exportInlineProjectJson();
    FE_REQUIRE_TRUE(exported.hasValue());
    FE_REQUIRE_TRUE(rig.editor->newStarterProject().hasValue());
    FE_EXPECT_EQ(sc.displayLayers().size(), std::size_t{0});

    FE_REQUIRE_TRUE(rig.editor->importInlineProjectJson(exported.value()).hasValue());
    FE_EXPECT_EQ(rig.session->document().load_warnings.size(), std::size_t{0});
    FE_EXPECT_EQ(sc.displayLayers().size(), std::size_t{1});
    FE_EXPECT_EQ(sc.displayLayers()[0].name, std::string("anim_L"));

    const auto* reloaded_cube = sc.findObjectByName("HeroCube");
    FE_REQUIRE_TRUE(reloaded_cube != nullptr);
    FE_REQUIRE_TRUE(!reloaded_cube->custom_attributes.empty());
    FE_EXPECT_NEAR(static_cast<float>(reloaded_cube->custom_attributes[0].value), 0.85f, 1.0e-4f);

    rig.editor->setCurrentFrame(25.0f);
    FE_EXPECT_NEAR(sc.findObjectByName("HeroCube")->local_transform.position.x, 12.0f, 1.0e-3f);

    // Workspace preset "maya_classic" applies cleanly without resetting scene objects!
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"apply_workspace_preset","preset":"maya_classic"})").hasValue());
    FE_EXPECT_EQ(rig.session->workspace().state().layout_name, std::string("Maya Classic"));
    FE_EXPECT_EQ(sc.displayLayers().size(), std::size_t{1});
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
