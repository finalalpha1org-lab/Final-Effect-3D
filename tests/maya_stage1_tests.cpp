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
#include <cstdint>
#include <memory>
#include <span>

namespace {

using namespace finaleffect;

std::uint64_t fnvHashPixels(std::span<const rendering::Rgba8> pixels) noexcept {
    std::uint64_t h = 14695981039346656037ull;
    for (const auto& px : pixels) {
        h = (h ^ px.r) * 1099511628211ull;
        h = (h ^ px.g) * 1099511628211ull;
        h = (h ^ px.b) * 1099511628211ull;
        h = (h ^ px.a) * 1099511628211ull;
    }
    return h;
}

struct MayaStage1Rig {
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

FE_TEST(MayaStage1, ClosedManifoldPrimitivesAndNormalEditing) {
    const auto cyl = scene::EditableMesh::createCylinder(0.55f, 1.4f, 16);
    FE_EXPECT_TRUE(cyl.vertexCount() >= 32u);
    for (const auto& e : cyl.edges()) {
        FE_EXPECT_FALSE(e.isBoundary());
    }

    const auto cone = scene::EditableMesh::createCone(0.65f, 1.45f, 16);
    FE_EXPECT_TRUE(cone.vertexCount() >= 17u);
    for (const auto& e : cone.edges()) {
        FE_EXPECT_FALSE(e.isBoundary());
    }

    auto torus = scene::EditableMesh::createTorus(0.7f, 0.25f, 16, 12);
    FE_EXPECT_TRUE(torus.faceCount() == 16u * 12u);
    for (const auto& e : torus.edges()) {
        FE_EXPECT_FALSE(e.isBoundary());
    }

    const math::Vec3 orig_normal = torus.faces().front().normal;
    FE_REQUIRE_TRUE(torus.reverseNormals().hasValue());
    FE_EXPECT_TRUE(orig_normal.dot(torus.faces().front().normal) < -0.95f);
}

FE_TEST(MayaStage1, MenuSetsAndMayaToolboxPreserveSceneAndUndoState) {
    MayaStage1Rig rig;
    FE_REQUIRE_TRUE(rig.init());

    auto cyl_res = rig.editor->addPresetObject(app::ScenePreset::Cylinder);
    FE_REQUIRE_TRUE(cyl_res.hasValue());
    const scene::ObjectId cyl_id = cyl_res.value();
    const std::size_t obj_count_before = rig.session->document().scene.objectCount();
    const std::size_t undo_before = rig.session->commandStack().undoCount();

    // Switching across all 5 Maya Menu Sets never resets scene, selection, camera, or undo history.
    for (const auto ms : {
             app::MayaMenuSet::Rigging,
             app::MayaMenuSet::Animation,
             app::MayaMenuSet::FX,
             app::MayaMenuSet::Rendering,
             app::MayaMenuSet::Modeling}) {
        rig.session->setMenuSet(ms);
        FE_EXPECT_EQ(rig.session->menuSet(), ms);
        FE_EXPECT_EQ(rig.session->selectedObject(), cyl_id);
        FE_EXPECT_EQ(rig.session->document().scene.objectCount(), obj_count_before);
        FE_EXPECT_EQ(rig.session->commandStack().undoCount(), undo_before);
    }

    // Q = Select tool hides 3D manipulator; W/E/R/T/Y restore active & last tools.
    rig.window.pushInputEvent(platform::KeyEvent{.key = platform::KeyCode::Q, .action = platform::KeyAction::Press});
    FE_REQUIRE_TRUE(rig.engine->runFrames(1).hasValue());
    FE_EXPECT_EQ(rig.session->activeTool(), app::MayaTool::Select);
    FE_EXPECT_FALSE(rig.session->computeActiveGizmoFrame().has_value());

    rig.window.pushInputEvent(platform::KeyEvent{.key = platform::KeyCode::E, .action = platform::KeyAction::Press});
    FE_REQUIRE_TRUE(rig.engine->runFrames(1).hasValue());
    FE_EXPECT_EQ(rig.session->activeTool(), app::MayaTool::Rotate);
    FE_EXPECT_TRUE(rig.session->computeActiveGizmoFrame().has_value());

    rig.window.pushInputEvent(platform::KeyEvent{.key = platform::KeyCode::Q, .action = platform::KeyAction::Press});
    rig.window.pushInputEvent(platform::KeyEvent{.key = platform::KeyCode::Y, .action = platform::KeyAction::Press});
    FE_REQUIRE_TRUE(rig.engine->runFrames(1).hasValue());
    FE_EXPECT_EQ(rig.session->activeTool(), app::MayaTool::Rotate);
}

FE_TEST(MayaStage1, ViewportDisplayModes4567XRayAndIsolateUpdateRenderedPixels) {
    MayaStage1Rig rig;
    FE_REQUIRE_TRUE(rig.init());
    const auto* hero = rig.session->document().scene.findObjectByName("HeroCube");
    FE_REQUIRE_TRUE(hero != nullptr);
    FE_REQUIRE_TRUE(rig.editor->selectObject(hero->id).hasValue());

    FE_REQUIRE_TRUE(rig.engine->runFrames(1).hasValue());
    // Startup GPU buffer count must stay compact (no eager wireframe buffer upload).
    FE_EXPECT_TRUE(rig.session->collectResourceStats().gpu_mesh_buffer_count <= 4u);
    const auto hash_default = fnvHashPixels(rig.raw_device->colorBuffer());

    // Key 4: Wireframe
    rig.window.pushInputEvent(platform::KeyEvent{.key = platform::KeyCode::Digit4, .action = platform::KeyAction::Press});
    FE_REQUIRE_TRUE(rig.engine->runFrames(1).hasValue());
    const auto hash_wire = fnvHashPixels(rig.raw_device->colorBuffer());
    FE_EXPECT_TRUE(hash_wire != hash_default);

    // Key 5: Smooth Shaded (Headlamp)
    rig.window.pushInputEvent(platform::KeyEvent{.key = platform::KeyCode::Digit5, .action = platform::KeyAction::Press});
    FE_REQUIRE_TRUE(rig.engine->runFrames(1).hasValue());
    const auto hash_shaded = fnvHashPixels(rig.raw_device->colorBuffer());
    FE_EXPECT_TRUE(hash_shaded != hash_wire);
    FE_EXPECT_TRUE(hash_shaded != hash_default);

    // Key 6: Material/Textured (Headlamp)
    rig.window.pushInputEvent(platform::KeyEvent{.key = platform::KeyCode::Digit6, .action = platform::KeyAction::Press});
    FE_REQUIRE_TRUE(rig.engine->runFrames(1).hasValue());
    const auto hash_textured = fnvHashPixels(rig.raw_device->colorBuffer());
    FE_EXPECT_TRUE(hash_textured != hash_shaded);

    // Key 7: Use All Scene Lights
    rig.window.pushInputEvent(platform::KeyEvent{.key = platform::KeyCode::Digit7, .action = platform::KeyAction::Press});
    FE_REQUIRE_TRUE(rig.engine->runFrames(1).hasValue());
    const auto hash_lit = fnvHashPixels(rig.raw_device->colorBuffer());
    FE_EXPECT_TRUE(hash_lit != hash_textured);

    // X-Ray & Isolate Select (Ctrl+1)
    rig.session->setXRayEnabled(true);
    FE_REQUIRE_TRUE(rig.engine->runFrames(1).hasValue());
    const auto hash_xray = fnvHashPixels(rig.raw_device->colorBuffer());
    FE_EXPECT_TRUE(hash_xray != hash_lit);

    rig.session->setXRayEnabled(false);
    rig.window.pushInputEvent(platform::KeyEvent{
        .key = platform::KeyCode::Digit1,
        .action = platform::KeyAction::Press,
        .mods = {.ctrl = true},
    });
    FE_REQUIRE_TRUE(rig.engine->runFrames(1).hasValue());
    FE_EXPECT_TRUE(rig.session->isIsolateSelectActive());
    const auto hash_iso = fnvHashPixels(rig.raw_device->colorBuffer());
    FE_EXPECT_TRUE(hash_iso != hash_lit);
}

FE_TEST(MayaStage1, DisplayLayersHierarchyFreezeTransformsAndChannelLocks) {
    MayaStage1Rig rig;
    FE_REQUIRE_TRUE(rig.init());
    const auto* hero = rig.session->document().scene.findObjectByName("HeroCube");
    const auto* accent = rig.session->document().scene.findObjectByName("AccentPyramid");
    FE_REQUIRE_TRUE(hero != nullptr && accent != nullptr);

    // Display Layer visibility / template / reference without deleting object.
    FE_REQUIRE_TRUE(rig.editor->selectObject(hero->id).hasValue());
    const std::uint32_t lid = rig.session->createDisplayLayer("PropsLayer", true);
    FE_EXPECT_TRUE(rig.session->isObjectSelectableInViewport(hero->id));

    FE_REQUIRE_TRUE(rig.session->setDisplayLayerState(lid, true, app::DisplayLayerMode::Reference).hasValue());
    FE_EXPECT_FALSE(rig.session->isObjectSelectableInViewport(hero->id));

    FE_REQUIRE_TRUE(rig.session->setDisplayLayerState(lid, false, app::DisplayLayerMode::Normal).hasValue());
    FE_EXPECT_FALSE(rig.session->isObjectSelectableInViewport(hero->id));
    FE_EXPECT_TRUE(rig.session->document().scene.findObject(hero->id) != nullptr);

    FE_REQUIRE_TRUE(rig.session->deleteDisplayLayer(lid).hasValue());
    FE_EXPECT_TRUE(rig.session->isObjectSelectableInViewport(hero->id));

    // Channel lock bitmask prevents Translate X mutation while allowing Translate Y.
    rig.session->setObjectChannelMask(hero->id, 0x0001u); // Lock TX
    const float initial_tx = hero->local_transform.position.x;
    FE_REQUIRE_TRUE(rig.editor->setObjectTransformEulerDegrees(
        hero->id, {9.5f, 3.25f, -1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, false).hasValue());
    FE_EXPECT_NEAR(rig.session->document().scene.findObject(hero->id)->local_transform.position.x, initial_tx, 1.0e-4f);
    FE_EXPECT_NEAR(rig.session->document().scene.findObject(hero->id)->local_transform.position.y, 3.25f, 1.0e-4f);
    rig.session->setObjectChannelMask(hero->id, 0u);

    // Freeze Transformations resets local transform to identity while preserving world-space bounds!
    const auto bounds_before = rig.session->document().scene.computeObjectWorldBounds(hero->id).value();
    FE_REQUIRE_TRUE(rig.editor->freezeSelectedTransformations().hasValue());
    const auto* frozen = rig.session->document().scene.findObject(hero->id);
    FE_EXPECT_NEAR(frozen->local_transform.position.length(), 0.0f, 1.0e-4f);
    FE_EXPECT_NEAR((frozen->local_transform.scale - math::Vec3{1.0f, 1.0f, 1.0f}).length(), 0.0f, 1.0e-4f);
    const auto bounds_after = rig.session->document().scene.computeObjectWorldBounds(hero->id).value();
    FE_EXPECT_NEAR((bounds_before.center() - bounds_after.center()).length(), 0.0f, 1.0e-3f);

    // Group & Unparent
    FE_REQUIRE_TRUE(rig.editor->selectObject(accent->id).hasValue());
    auto grp_res = rig.editor->groupSelectedObjects("HeroGroup");
    FE_REQUIRE_TRUE(grp_res.hasValue());
    FE_EXPECT_EQ(rig.session->document().scene.findObject(accent->id)->parent_id, grp_res.value());
    FE_REQUIRE_TRUE(rig.editor->selectObject(accent->id).hasValue());
    FE_REQUIRE_TRUE(rig.editor->unparentSelectedObjects().hasValue());
    FE_EXPECT_FALSE(rig.session->document().scene.findObject(accent->id)->parent_id.isValid());
}

FE_TEST(MayaStage1, TimelineKeyframesPlaybackAndSafeCommandLine) {
    MayaStage1Rig rig;
    FE_REQUIRE_TRUE(rig.init());

    // Safe structured Command Line creates Torus, moves it, keys frame 1 & frame 25, and scrubs!
    FE_REQUIRE_TRUE(rig.editor->executeCommandLine("polyTorus").hasValue());
    const scene::ObjectId torus_id = rig.session->selectedObject();
    FE_REQUIRE_TRUE(torus_id.isValid());

    FE_REQUIRE_TRUE(rig.editor->executeCommandLine("currentTime 1").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeCommandLine("move 0 1 0").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeCommandLine("setKey").hasValue());

    FE_REQUIRE_TRUE(rig.editor->executeCommandLine("currentTime 25").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeCommandLine("move 4 3 -2").hasValue());
    FE_REQUIRE_TRUE(rig.editor->executeCommandLine("setKey").hasValue());

    // Scrubbing to midpoint frame 13 interpolates C++ SceneObject transform directly!
    FE_REQUIRE_TRUE(rig.editor->executeCommandLine("currentTime 13").hasValue());
    const math::Vec3 mid_pos = rig.session->document().scene.findObject(torus_id)->local_transform.position;
    FE_EXPECT_NEAR(mid_pos.x, 2.0f, 1.0e-3f);
    FE_EXPECT_NEAR(mid_pos.y, 2.0f, 1.0e-3f);
    FE_EXPECT_NEAR(mid_pos.z, -1.0f, 1.0e-3f);

    // Real C++ playback advances frame during StudioSession::onUpdate
    rig.session->setTimelineFps(24.0f);
    FE_REQUIRE_TRUE(rig.editor->executeCommandLine("play").hasValue());
    for (int i = 0; i < 6; ++i) {
        FE_REQUIRE_TRUE(
            rig.session->onUpdate(*rig.engine, core::FrameTimings{.delta_seconds = 1.0 / 24.0}).hasValue());
    }
    FE_EXPECT_NEAR(static_cast<float>(rig.session->timeline().current_frame), 19.0f, 0.25f);
    FE_REQUIRE_TRUE(rig.editor->executeCommandLine("stop").hasValue());

    // Unknown or unsafe input is safely rejected with structured error status.
    FE_EXPECT_FALSE(rig.editor->executeCommandLine("alert(1)").hasValue());
    FE_EXPECT_TRUE(rig.editor->lastStatusIsError());
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
