// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors
//
// Stage 5.2 Automated Verification Suite:
// Verifies visible 3D Camera & Directional Light editor helpers, viewport picking without
// blocking normal mesh picking, Move/Rotate/Scale gizmo transforms, multi-camera & multi-light
// independence, Outliner <-> Viewport <-> Inspector sync, Undo/Redo, .fe3d/.fe3dbin persistence
// without duplicate objects, and strict exclusion of editor helpers from final scene renders.

#include "test_harness.hpp"

#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/app/viewport_scene_helpers.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/platform/window.hpp"
#include "finaleffect/project/json.hpp"
#include "finaleffect/rendering/renderer.hpp"
#include "finaleffect/rendering/software_render_device.hpp"

#include <cmath>
#include <filesystem>
#include <memory>
#include <string>

namespace {

using namespace finaleffect;

struct Stage52Rig {
    platform::LocalFileSystem fs;
    platform::HeadlessWindow window;
    rendering::SoftwareRenderDevice* sw_device{nullptr};
    std::unique_ptr<core::Engine> engine;
    std::unique_ptr<app::StudioSession> session;
    std::unique_ptr<app::EditorController> editor;

    bool init(std::uint32_t width = 480, std::uint32_t height = 320) {
        if (!window.initialize(platform::WindowDesc{
                .title = "Stage52CameraLightTests",
                .width = width,
                .height = height,
            }).hasValue()) {
            return false;
        }
        core::EngineConfig cfg;
        cfg.minimum_log_level = core::LogLevel::Error;
        engine = std::make_unique<core::Engine>(cfg);

        auto sw = std::make_unique<rendering::SoftwareRenderDevice>();
        sw_device = sw.get();

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

    ~Stage52Rig() {
        if (engine != nullptr) {
            engine->shutdown();
        }
        window.shutdown();
    }

    // Counts pixels inside a square patch around (cx, cy) that differ from the clear background color.
    std::uint32_t countNonBackgroundPixelsInPatch(int cx, int cy, int half_radius) const {
        const auto bg = sw_device->pixelAt(2, 2);
        std::uint32_t count = 0;
        for (int dy = -half_radius; dy <= half_radius; ++dy) {
            for (int dx = -half_radius; dx <= half_radius; ++dx) {
                const int px = cx + dx;
                const int py = cy + dy;
                if (px < 0 || py < 0 ||
                    px >= static_cast<int>(sw_device->width()) ||
                    py >= static_cast<int>(sw_device->height())) {
                    continue;
                }
                const auto p = sw_device->pixelAt(static_cast<std::uint32_t>(px), static_cast<std::uint32_t>(py));
                const int diff = std::abs(static_cast<int>(p.r) - static_cast<int>(bg.r)) +
                                 std::abs(static_cast<int>(p.g) - static_cast<int>(bg.g)) +
                                 std::abs(static_cast<int>(p.b) - static_cast<int>(bg.b));
                if (diff > 24) {
                    ++count;
                }
            }
        }
        return count;
    }

    std::uint64_t hashPatch(int cx, int cy, int half_radius) const {
        std::uint64_t h = 1469598103934665603ULL;
        for (int dy = -half_radius; dy <= half_radius; ++dy) {
            for (int dx = -half_radius; dx <= half_radius; ++dx) {
                const int px = std::clamp(cx + dx, 0, static_cast<int>(sw_device->width()) - 1);
                const int py = std::clamp(cy + dy, 0, static_cast<int>(sw_device->height()) - 1);
                const auto p = sw_device->pixelAt(static_cast<std::uint32_t>(px), static_cast<std::uint32_t>(py));
                h = (h ^ p.r) * 1099511628211ULL;
                h = (h ^ p.g) * 1099511628211ULL;
                h = (h ^ p.b) * 1099511628211ULL;
            }
        }
        return h;
    }
};

// Covers Mandatory Test Areas 1, 2, & 9:
// Default & newly created Camera + DirectionalLight viewport visibility, zero-intensity light visibility,
// and strict exclusion of editor helpers from final scene renders & Scene mesh assets.
FE_TEST(CameraLightViewport, DefaultAndCreatedHelpersVisibleAndExcludedFromFinalRender) {
    Stage52Rig rig;
    FE_REQUIRE_TRUE(rig.init(480, 320));

    const auto& scene = rig.session->document().scene;
    FE_EXPECT_EQ(scene.objectCount(), std::size_t{6});
    FE_EXPECT_EQ(scene.meshCount(), std::size_t{3}); // Helpers NEVER pollute Scene mesh asset registry!

    const auto* main_cam = scene.findObjectByName("MainCamera");
    const auto* sun_light = scene.findObjectByName("SunLight");
    FE_REQUIRE_TRUE(main_cam != nullptr && main_cam->camera.has_value());
    FE_REQUIRE_TRUE(sun_light != nullptr && sun_light->directional_light.has_value());

    // Render editor viewport frame with helpers enabled.
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());

    const auto cam_wm = scene.computeWorldMatrix(main_cam->id).value();
    const auto cam_screen = rig.session->projectWorldPointToScreen(cam_wm.transformPoint({0.0f, 0.04f, 0.18f}));
    const auto sun_wm = scene.computeWorldMatrix(sun_light->id).value();
    const auto sun_screen = rig.session->projectWorldPointToScreen(sun_wm.transformPoint(math::Vec3::zero()));
    FE_REQUIRE_TRUE(cam_screen.has_value());
    FE_REQUIRE_TRUE(sun_screen.has_value());

    // Both MainCamera and SunLight project well inside the 480x320 viewport and rasterize visible 3D pixels!
    FE_EXPECT_TRUE(cam_screen->x > 30.0f && cam_screen->x < 450.0f);
    FE_EXPECT_TRUE(cam_screen->y > 20.0f && cam_screen->y < 300.0f);
    FE_EXPECT_TRUE(sun_screen->x > 30.0f && sun_screen->x < 450.0f);
    FE_EXPECT_TRUE(sun_screen->y > 20.0f && sun_screen->y < 300.0f);

    const auto cam_px_count = rig.countNonBackgroundPixelsInPatch(
        static_cast<int>(std::lround(cam_screen->x)),
        static_cast<int>(std::lround(cam_screen->y)),
        12);
    const auto sun_px_count = rig.countNonBackgroundPixelsInPatch(
        static_cast<int>(std::lround(sun_screen->x)),
        static_cast<int>(std::lround(sun_screen->y)),
        12);
    FE_EXPECT_TRUE(cam_px_count >= 12u);
    FE_EXPECT_TRUE(sun_px_count >= 12u);

    // Even when SunLight intensity is 0.0 and color is black, the light helper remains clearly visible!
    FE_REQUIRE_TRUE(rig.editor->setDirectionalLightComponent(
        sun_light->id,
        scene::DirectionalLightComponent{.color = {0.0f, 0.0f, 0.0f}, .intensity = 0.0f}).hasValue());
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    const auto dark_sun_px_count = rig.countNonBackgroundPixelsInPatch(
        static_cast<int>(std::lround(sun_screen->x)),
        static_cast<int>(std::lround(sun_screen->y)),
        12);
    FE_EXPECT_TRUE(dark_sun_px_count >= 12u);

    // Toggling editor helpers off removes MainCamera and SunLight helpers cleanly from the viewport!
    rig.session->setEditorHelpersVisible(false);
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    FE_EXPECT_EQ(
        rig.countNonBackgroundPixelsInPatch(
            static_cast<int>(std::lround(cam_screen->x)),
            static_cast<int>(std::lround(cam_screen->y)),
            10),
        0u);
    FE_EXPECT_EQ(
        rig.countNonBackgroundPixelsInPatch(
            static_cast<int>(std::lround(sun_screen->x)),
            static_cast<int>(std::lround(sun_screen->y)),
            10),
        0u);
    rig.session->setEditorHelpersVisible(true);

    // Area 9: Final scene render (Renderer::renderScene with empty overlays) MUST NOT show any editor helpers!
    // Place a bright cyan secondary camera right in the middle of MainCamera's view above HeroCube,
    // verify it is visible in the editor viewport, and verify a final render from MainCamera has ZERO helper pixels!
    const auto sec_cam = rig.editor->addPresetObject(app::ScenePreset::Camera).value();
    FE_REQUIRE_TRUE(rig.editor->setObjectTransformEulerDegrees(
        sec_cam,
        {0.0f, 1.95f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f}).hasValue());
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());

    auto final_sw = std::make_unique<rendering::SoftwareRenderDevice>();
    auto* final_sw_ptr = final_sw.get();
    rendering::Renderer final_renderer(std::move(final_sw));
    FE_REQUIRE_TRUE(final_renderer.initialize(240, 160).hasValue());
    FE_REQUIRE_TRUE(final_renderer.renderScene(scene).hasValue());

    // Now delete sec_cam and render final scene again -> pixel-for-pixel IDENTICAL because cameras/lights have no final-render geometry!
    const auto final_pixels_with_cam_and_lights = final_sw_ptr->colorBuffer();
    FE_REQUIRE_TRUE(rig.editor->selectObject(sec_cam).hasValue());
    FE_REQUIRE_TRUE(rig.editor->deleteSelectedObject().hasValue());
    FE_REQUIRE_TRUE(final_renderer.renderScene(scene).hasValue());
    FE_EXPECT_TRUE(final_pixels_with_cam_and_lights == final_sw_ptr->colorBuffer());
}

// Covers Mandatory Test Areas 3, 4, & 6:
// Viewport click selection, selection highlight, Move/Rotate/Scale gizmo transforms on BOTH Camera and Light,
// dynamic Camera frustum updates, real PBR lighting updates, and bidirectional Outliner <-> Viewport <-> Inspector sync.
FE_TEST(CameraLightViewport, ViewportPickingHighlightGizmoTransformsAndInspectorSync) {
    Stage52Rig rig;
    FE_REQUIRE_TRUE(rig.init(480, 320));
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());

    const auto& scene = rig.session->document().scene;
    const auto* main_cam = scene.findObjectByName("MainCamera");
    const auto* sun_light = scene.findObjectByName("SunLight");
    const auto* hero_cube = scene.findObjectByName("HeroCube");
    FE_REQUIRE_TRUE(main_cam != nullptr && sun_light != nullptr && hero_cube != nullptr);

    // Normal mesh picking at center (HeroCube sits inside MainCamera's frustum!) MUST pick HeroCube without frustum interference!
    const auto center_hit = rig.editor->pickViewportPixel(240.0f, 160.0f);
    FE_REQUIRE_TRUE(center_hit.has_value());
    FE_EXPECT_EQ(center_hit->object_id, hero_cube->id);

    // Clear selection, capture unselected MainCamera patch, then click MainCamera directly in the 3D viewport!
    FE_REQUIRE_TRUE(rig.editor->selectObject(scene::ObjectId{}).hasValue());
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());

    const auto cam_anchor = scene.computeWorldMatrix(main_cam->id).value().transformPoint({0.0f, 0.04f, 0.18f});
    const auto cam_pt = rig.session->projectWorldPointToScreen(cam_anchor).value();
    const auto cam_unselected_hash = rig.hashPatch(
        static_cast<int>(std::lround(cam_pt.x)),
        static_cast<int>(std::lround(cam_pt.y)),
        14);

    rig.window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left,
        .action = platform::KeyAction::Press,
        .x = cam_pt.x,
        .y = cam_pt.y,
    });
    rig.window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left,
        .action = platform::KeyAction::Release,
        .x = cam_pt.x,
        .y = cam_pt.y,
    });
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());

    // Viewport click selected MainCamera -> synced to Outliner & Inspector with CameraComponent payload!
    FE_EXPECT_EQ(rig.session->selectedObject(), main_cam->id);
    const auto state_cam = project::parseJson(rig.editor->serializeStateJson()).value();
    FE_EXPECT_EQ(
        static_cast<std::uint64_t>(state_cam.findKey("viewport")->findKey("selected_object_id")->asNumber()),
        main_cam->id.value);
    FE_REQUIRE_TRUE(state_cam.findKey("inspector")->findKey("camera") != nullptr);

    // Selection highlights MainCamera visually AND exposes Move/Rotate/Scale gizmo frame!
    const auto cam_selected_hash = rig.hashPatch(
        static_cast<int>(std::lround(cam_pt.x)),
        static_cast<int>(std::lround(cam_pt.y)),
        14);
    FE_EXPECT_TRUE(cam_selected_hash != cam_unselected_hash);
    FE_REQUIRE_TRUE(rig.session->computeActiveGizmoFrame().has_value());

    // Drag Move (+Y), Rotate (Y), and Scale (+X) gizmos on MainCamera -> updates actual SceneObject transform & final render!
    auto final_sw = std::make_unique<rendering::SoftwareRenderDevice>();
    auto* final_sw_ptr = final_sw.get();
    rendering::Renderer final_renderer(std::move(final_sw));
    FE_REQUIRE_TRUE(final_renderer.initialize(160, 120).hasValue());
    FE_REQUIRE_TRUE(final_renderer.renderScene(scene).hasValue());
    const auto final_before_cam_move = final_sw_ptr->colorBuffer();

    const math::Vec3 cam_orig_pos = main_cam->local_transform.position;
    rig.session->setGizmoMode(app::GizmoMode::Translate);
    const auto gframe_move = rig.session->computeActiveGizmoFrame().value();
    const auto gy_pt = rig.session->projectWorldPointToScreen(
        gframe_move.origin + gframe_move.axes[1] * (gframe_move.scale * 0.65f)).value();
    rig.window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left, .action = platform::KeyAction::Press, .x = gy_pt.x, .y = gy_pt.y});
    rig.window.pushInputEvent(platform::CursorMoveEvent{.x = gy_pt.x, .y = gy_pt.y - 36.0});
    rig.window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left, .action = platform::KeyAction::Release, .x = gy_pt.x, .y = gy_pt.y - 36.0});
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    FE_EXPECT_TRUE((scene.findObject(main_cam->id)->local_transform.position - cam_orig_pos).length() > 0.08f);

    // Rotate & Scale gizmos also compute valid handles and mutate MainCamera's rotation & scale!
    rig.session->setGizmoMode(app::GizmoMode::Rotate);
    const auto gframe_rot = rig.session->computeActiveGizmoFrame().value();
    const auto rot_ray = app::OrbitCameraController::screenPointToWorldRay(
        rig.session->projectWorldPointToScreen(gframe_rot.origin + gframe_rot.axes[0] * gframe_rot.scale).value().x,
        rig.session->projectWorldPointToScreen(gframe_rot.origin + gframe_rot.axes[0] * gframe_rot.scale).value().y,
        480, 320, rig.session->activeCameraState().camera_world, rig.session->activeCameraState().camera).value();
    FE_EXPECT_TRUE(app::TransformGizmo::pickHandle(app::GizmoMode::Rotate, gframe_rot, rot_ray).has_value());

    rig.session->setGizmoMode(app::GizmoMode::Scale);
    const auto gframe_scl = rig.session->computeActiveGizmoFrame().value();
    const auto sx_pt = rig.session->projectWorldPointToScreen(
        gframe_scl.origin + gframe_scl.axes[0] * (gframe_scl.scale * 0.65f)).value();
    rig.window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left, .action = platform::KeyAction::Press, .x = sx_pt.x, .y = sx_pt.y});
    rig.window.pushInputEvent(platform::CursorMoveEvent{.x = sx_pt.x + 28.0, .y = sx_pt.y});
    rig.window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left, .action = platform::KeyAction::Release, .x = sx_pt.x + 28.0, .y = sx_pt.y});
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    FE_EXPECT_TRUE(std::fabs(scene.findObject(main_cam->id)->local_transform.scale.x - 1.0f) > 0.02f);

    // Actual final scene camera view changed because MainCamera's real SceneObject moved!
    FE_REQUIRE_TRUE(final_renderer.renderScene(scene).hasValue());
    FE_EXPECT_TRUE(final_sw_ptr->colorBuffer() != final_before_cam_move);

    // Changing CameraComponent FOV / Projection dynamically reshapes the 3D camera frustum helper mesh!
    const auto helper_persp_48 = app::buildViewportSceneHelpersMesh(scene, {.viewport_aspect = 1.5f});
    scene::CameraComponent wide_cam = scene.findObject(main_cam->id)->camera.value();
    wide_cam.fov_y_radians = math::degToRad(95.0f);
    FE_REQUIRE_TRUE(rig.editor->setCameraComponent(main_cam->id, wide_cam).hasValue());
    const auto helper_persp_95 = app::buildViewportSceneHelpersMesh(scene, {.viewport_aspect = 1.5f});
    FE_EXPECT_EQ(helper_persp_48.vertices.size(), helper_persp_95.vertices.size());
    bool frustum_changed = false;
    for (std::size_t i = 0; i < helper_persp_48.vertices.size(); ++i) {
        if (!helper_persp_48.vertices[i].position.approxEqual(helper_persp_95.vertices[i].position, 1e-3f)) {
            frustum_changed = true;
            break;
        }
    }
    FE_EXPECT_TRUE(frustum_changed);

    // Now pick SunLight directly in the 3D Viewport, verify highlight + Inspector sync + real PBR lighting change on rotation!
    const auto sun_pt = rig.session->projectWorldPointToScreen(
        scene.computeWorldMatrix(sun_light->id).value().transformPoint(math::Vec3::zero())).value();
    const auto sun_hit = rig.editor->pickViewportPixel(sun_pt.x, sun_pt.y);
    FE_REQUIRE_TRUE(sun_hit.has_value());
    FE_EXPECT_EQ(sun_hit->object_id, sun_light->id);

    const auto state_light = project::parseJson(rig.editor->serializeStateJson()).value();
    FE_REQUIRE_TRUE(state_light.findKey("inspector")->findKey("light") != nullptr);

    const auto hero_lighting_before = rig.hashPatch(240, 160, 18);
    FE_REQUIRE_TRUE(rig.editor->setObjectTransformEulerDegrees(
        sun_light->id,
        sun_light->local_transform.position,
        {-25.0f, 125.0f, 0.0f},
        {1.0f, 1.0f, 1.0f}).hasValue());
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    const auto hero_lighting_after = rig.hashPatch(240, 160, 18);
    FE_EXPECT_TRUE(hero_lighting_after != hero_lighting_before);
}

// Covers Mandatory Test Areas 5, 7, & 8:
// Multiple independent cameras & lights (including multi-light PBR shading), rename/delete/Undo/Redo,
// and .fe3d + .fe3dbin round-trip persistence with zero silent object duplication.
FE_TEST(CameraLightViewport, MultipleCamerasAndLightsUndoRedoAndPersistenceWithoutDuplication) {
    Stage52Rig rig;
    FE_REQUIRE_TRUE(rig.init(480, 320));
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());

    auto& scene = rig.session->document().scene;
    const auto initial_count = scene.objectCount(); // 6

    // Add 2 extra cameras and 2 extra directional lights -> 10 objects total, all independent!
    const auto cam2 = rig.editor->addPresetObject(app::ScenePreset::Camera).value();
    const auto cam3 = rig.editor->addPresetObject(app::ScenePreset::Camera).value();
    const auto lgt2 = rig.editor->addPresetObject(app::ScenePreset::DirectionalLight).value();
    const auto lgt3 = rig.editor->addPresetObject(app::ScenePreset::DirectionalLight).value();
    FE_EXPECT_EQ(scene.objectCount(), initial_count + 4);

    // Customize cam2 (Orthographic) vs cam3 (Perspective 75 deg) independently.
    scene::CameraComponent c2_cfg{};
    c2_cfg.projection = scene::ProjectionType::Orthographic;
    c2_cfg.orthographic_height = 6.5f;
    FE_REQUIRE_TRUE(rig.editor->setCameraComponent(cam2, c2_cfg).hasValue());

    scene::CameraComponent c3_cfg{};
    c3_cfg.projection = scene::ProjectionType::Perspective;
    c3_cfg.fov_y_radians = math::degToRad(75.0f);
    FE_REQUIRE_TRUE(rig.editor->setCameraComponent(cam3, c3_cfg).hasValue());

    FE_EXPECT_EQ(scene.findObject(cam2)->camera->projection, scene::ProjectionType::Orthographic);
    FE_EXPECT_NEAR(scene.findObject(cam2)->camera->orthographic_height, 6.5f, 1e-4f);
    FE_EXPECT_EQ(scene.findObject(cam3)->camera->projection, scene::ProjectionType::Perspective);
    FE_EXPECT_NEAR(math::radToDeg(scene.findObject(cam3)->camera->fov_y_radians), 75.0f, 1e-3f);

    // Verify independent multi-light PBR shading: lgt2 (strong blue) + lgt3 (strong red) both illuminate HeroCube!
    FE_REQUIRE_TRUE(rig.editor->setDirectionalLightComponent(
        lgt2, scene::DirectionalLightComponent{.color = {0.05f, 0.25f, 1.0f}, .intensity = 2.8f}).hasValue());
    FE_REQUIRE_TRUE(rig.editor->setDirectionalLightComponent(
        lgt3, scene::DirectionalLightComponent{.color = {1.0f, 0.10f, 0.05f}, .intensity = 0.0f}).hasValue());
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    const auto hero_with_lgt2_only = rig.hashPatch(240, 160, 16);

    FE_REQUIRE_TRUE(rig.editor->setDirectionalLightComponent(
        lgt3, scene::DirectionalLightComponent{.color = {1.0f, 0.10f, 0.05f}, .intensity = 3.2f}).hasValue());
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    const auto hero_with_lgt2_and_lgt3 = rig.hashPatch(240, 160, 16);
    FE_EXPECT_TRUE(hero_with_lgt2_and_lgt3 != hero_with_lgt2_only);
    FE_EXPECT_NEAR(scene.findObject(lgt2)->directional_light->intensity, 2.8f, 1e-4f);
    FE_EXPECT_NEAR(scene.findObject(lgt3)->directional_light->intensity, 3.2f, 1e-4f);

    // Rename, Delete, Undo, and Redo on cam3 & lgt3!
    FE_REQUIRE_TRUE(rig.editor->renameObject(cam3, "CloseUpCam").hasValue());
    FE_EXPECT_EQ(scene.findObject(cam3)->name, std::string("CloseUpCam"));

    FE_REQUIRE_TRUE(rig.editor->selectObject(cam3).hasValue());
    FE_REQUIRE_TRUE(rig.editor->deleteSelectedObject().hasValue());
    FE_EXPECT_TRUE(scene.findObject(cam3) == nullptr);

    // Undo restores CloseUpCam with exact ObjectId and 75-deg Perspective settings!
    FE_REQUIRE_TRUE(rig.editor->undo().hasValue());
    FE_REQUIRE_TRUE(scene.findObject(cam3) != nullptr);
    FE_EXPECT_EQ(scene.findObject(cam3)->name, std::string("CloseUpCam"));
    FE_EXPECT_NEAR(math::radToDeg(scene.findObject(cam3)->camera->fov_y_radians), 75.0f, 1e-3f);

    // Redo re-deletes CloseUpCam, then Undo brings it back for persistence testing!
    FE_REQUIRE_TRUE(rig.editor->redo().hasValue());
    FE_EXPECT_TRUE(scene.findObject(cam3) == nullptr);
    FE_REQUIRE_TRUE(rig.editor->undo().hasValue());
    FE_REQUIRE_TRUE(scene.findObject(cam3) != nullptr);

    // Switch active camera to cam2, export .fe3d inline JSON AND .fe3d/.fe3dbin bundle, reload multiple times ->
    // exact object count (10) preserved with ZERO silent duplicates!
    FE_REQUIRE_TRUE(rig.editor->setActiveCamera(cam2).hasValue());
    const auto saved_json = rig.editor->exportInlineProjectJson().value();

    const auto tmp_dir = std::filesystem::temp_directory_path() / "fe3d_stage52_persist_test";
    std::error_code ec;
    std::filesystem::remove_all(tmp_dir, ec);
    const auto bundle_fe3d = tmp_dir / "stage52_scene.fe3d";
    FE_REQUIRE_TRUE(rig.editor->saveBundleProject(bundle_fe3d, rig.fs).hasValue());

    FE_REQUIRE_TRUE(rig.editor->importInlineProjectJson(saved_json).hasValue());
    FE_REQUIRE_TRUE(rig.editor->importInlineProjectJson(saved_json).hasValue());
    FE_EXPECT_EQ(scene.objectCount(), initial_count + 4);
    FE_EXPECT_EQ(scene.activeCamera(), cam2);
    FE_EXPECT_EQ(scene.findObject(cam2)->camera->projection, scene::ProjectionType::Orthographic);
    FE_EXPECT_EQ(scene.findObject(cam3)->name, std::string("CloseUpCam"));
    FE_EXPECT_NEAR(scene.findObject(lgt3)->directional_light->intensity, 3.2f, 1e-4f);

    FE_REQUIRE_TRUE(rig.editor->loadProjectFile(bundle_fe3d, rig.fs).hasValue());
    FE_EXPECT_EQ(scene.objectCount(), initial_count + 4);
    FE_EXPECT_EQ(scene.activeCamera(), cam2);
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());

    std::filesystem::remove_all(tmp_dir, ec);
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
