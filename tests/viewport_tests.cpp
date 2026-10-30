// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "test_harness.hpp"

#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/app/orbit_camera_controller.hpp"
#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/platform/window.hpp"
#include "finaleffect/rendering/software_render_device.hpp"

#include <cmath>
#include <memory>

namespace {

using namespace finaleffect;

FE_TEST(Viewport, OrbitPanZoomFrameBoundsAndScreenRayUnprojection) {
    app::OrbitCameraController ctrl;
    math::Transform init_cam;
    init_cam.position = {0.0f, 3.0f, 6.0f};
    ctrl.syncFromCameraTransform(init_cam, {0.0f, 1.0f, 0.0f});

    const math::Transform recomputed = ctrl.computeCameraTransform();
    FE_EXPECT_TRUE(recomputed.position.approxEqual(init_cam.position, 1e-4f));

    // Extreme pitch orbits clamp cleanly below +-90 degrees without NaN or gimbal flip.
    ctrl.orbit(math::degToRad(45.0f), math::degToRad(500.0f));
    FE_EXPECT_TRUE(ctrl.pitchRadians() <= math::degToRad(89.1f));
    ctrl.orbit(0.0f, -math::degToRad(1000.0f));
    FE_EXPECT_TRUE(ctrl.pitchRadians() >= -math::degToRad(89.1f));

    // Dolly zoom moves closer on positive wheel steps and clamps to limits.
    ctrl.setDistanceLimits(0.5f, 50.0f);
    const float dist_before = ctrl.distance();
    ctrl.zoom(3.0f);
    FE_EXPECT_TRUE(ctrl.distance() < dist_before);
    ctrl.zoom(500.0f);
    FE_EXPECT_NEAR(ctrl.distance(), 0.5f, 1e-4f);

    // Pan shifts target point in world space.
    const math::Vec3 target_before = ctrl.target();
    ctrl.pan(32.0f, -16.0f, 720, math::degToRad(50.0f));
    FE_EXPECT_TRUE((ctrl.target() - target_before).length() > 1e-4f);

    // Frame bounds centers target on AABB and fits radius inside FOV.
    const auto bounds = math::AABB::fromMinMax({-2.0f, 0.0f, -2.0f}, {4.0f, 4.0f, 2.0f});
    ctrl.frameBounds(bounds, math::degToRad(50.0f));
    FE_EXPECT_TRUE(ctrl.target().approxEqual({1.0f, 2.0f, 0.0f}, 1e-4f));
    FE_EXPECT_TRUE(ctrl.distance() > 4.0f);

    // Center screen ray of identity camera points straight down -Z.
    scene::CameraComponent persp{};
    const auto center_ray = app::OrbitCameraController::screenPointToWorldRay(
        160.0f, 120.0f, 320, 240, math::Mat4::identity(), persp);
    FE_REQUIRE_TRUE(center_ray.hasValue());
    FE_EXPECT_TRUE(center_ray.value().origin.approxEqual(math::Vec3::zero(), 1e-5f));
    FE_EXPECT_TRUE(center_ray.value().direction.approxEqual({0.0f, 0.0f, -1.0f}, 1e-5f));

    // Orthographic unprojection offsets ray origin while keeping parallel -Z direction.
    scene::CameraComponent ortho{};
    ortho.projection = scene::ProjectionType::Orthographic;
    ortho.orthographic_height = 10.0f;
    const auto top_right_ortho = app::OrbitCameraController::screenPointToWorldRay(
        320.0f, 0.0f, 320, 240, math::Mat4::identity(), ortho);
    FE_REQUIRE_TRUE(top_right_ortho.hasValue());
    FE_EXPECT_TRUE(top_right_ortho.value().origin.x > 6.0f);
    FE_EXPECT_NEAR(top_right_ortho.value().origin.y, 5.0f, 1e-4f);
    FE_EXPECT_TRUE(top_right_ortho.value().direction.approxEqual({0.0f, 0.0f, -1.0f}, 1e-5f));
}

FE_TEST(Viewport, InteractivePickingOrbitDeleteAndUndoRedoSession) {
    platform::HeadlessWindow window;
    FE_REQUIRE_TRUE(window.initialize({.title = "ViewportTest", .width = 320, .height = 240}).hasValue());

    core::EngineConfig cfg;
    cfg.minimum_log_level = core::LogLevel::Error;
    core::Engine engine(cfg);

    app::StudioSessionConfig session_cfg;
    session_cfg.animate_showcase_rig = false;
    app::StudioSession session(
        window,
        std::make_unique<rendering::SoftwareRenderDevice>(),
        session_cfg);

    FE_REQUIRE_TRUE(engine.initialize(&session).hasValue());
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());

    const auto* hero_cube = session.document().scene.findObjectByName("HeroCube");
    FE_REQUIRE_TRUE(hero_cube != nullptr);
    const scene::ObjectId hero_id = hero_cube->id;

    // 1. Left-click at viewport center (160, 120) raycast-picks HeroCube.
    window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left,
        .action = platform::KeyAction::Press,
        .x = 160.0,
        .y = 120.0,
    });
    window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left,
        .action = platform::KeyAction::Release,
        .x = 160.0,
        .y = 120.0,
    });
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());
    FE_EXPECT_EQ(session.selectedObject(), hero_id);

    // 2. Press 'F' to frame HeroCube bounds, then Right-drag to orbit camera + scroll wheel to zoom.
    const auto cam_id = session.document().scene.activeCamera();
    const math::Vec3 initial_scene_cam_pos = session.document().scene.findObject(cam_id)->local_transform.position;
    const math::Vec3 initial_vp_pos = session.cameraController().computeCameraTransform().position;

    window.pushInputEvent(platform::KeyEvent{.key = platform::KeyCode::F, .action = platform::KeyAction::Press});
    window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Right,
        .action = platform::KeyAction::Press,
        .x = 160.0,
        .y = 120.0,
    });
    window.pushInputEvent(platform::CursorMoveEvent{.x = 220.0, .y = 145.0});
    window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Right,
        .action = platform::KeyAction::Release,
        .x = 220.0,
        .y = 145.0,
    });
    window.pushInputEvent(platform::ScrollEvent{.delta_x = 0.0, .delta_y = 2.0});
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());

    const math::Vec3 moved_vp_pos = session.cameraController().computeCameraTransform().position;
    FE_EXPECT_TRUE((moved_vp_pos - initial_vp_pos).length() > 0.25f);
    // Free perspective orbit preserves MainCamera's authored scene transform unless Camera Lens View is active.
    FE_EXPECT_TRUE(session.document().scene.findObject(cam_id)->local_transform.position.approxEqual(
        initial_scene_cam_pos, 1e-5f));

    // 3. Delete selected HeroCube via Delete key -> recorded in CommandStack, then Ctrl+Z undoes & Ctrl+Y redoes.
    window.pushInputEvent(platform::KeyEvent{
        .key = platform::KeyCode::Delete,
        .action = platform::KeyAction::Press,
    });
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());
    FE_EXPECT_TRUE(session.document().scene.findObject(hero_id) == nullptr);
    FE_EXPECT_EQ(session.commandStack().undoCount(), std::size_t{1});

    // Ctrl+Z restores HeroCube with its exact original ObjectId.
    window.pushInputEvent(platform::KeyEvent{
        .key = platform::KeyCode::Z,
        .action = platform::KeyAction::Press,
        .mods = {.ctrl = true},
    });
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());
    FE_REQUIRE_TRUE(session.document().scene.findObject(hero_id) != nullptr);
    FE_EXPECT_EQ(session.document().scene.findObject(hero_id)->name, std::string("HeroCube"));

    // Ctrl+Y redoes deletion.
    window.pushInputEvent(platform::KeyEvent{
        .key = platform::KeyCode::Y,
        .action = platform::KeyAction::Press,
        .mods = {.ctrl = true},
    });
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());
    FE_EXPECT_TRUE(session.document().scene.findObject(hero_id) == nullptr);

    engine.shutdown();
    window.shutdown();
}

FE_TEST(Viewport, Stage4InteractiveGizmoDragCancelAndMeshFacePicking) {
    platform::HeadlessWindow window;
    FE_REQUIRE_TRUE(window.initialize({.title = "GizmoViewportTest", .width = 400, .height = 300}).hasValue());

    core::EngineConfig cfg;
    cfg.minimum_log_level = core::LogLevel::Error;
    core::Engine engine(cfg);

    app::StudioSession session(
        window,
        std::make_unique<rendering::SoftwareRenderDevice>(),
        app::StudioSessionConfig{.animate_showcase_rig = false});

    FE_REQUIRE_TRUE(engine.initialize(&session).hasValue());
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());

    const auto* hero = session.document().scene.findObjectByName("HeroCube");
    FE_REQUIRE_TRUE(hero != nullptr);
    const scene::ObjectId hero_id = hero->id;
    const math::Vec3 orig_pos = hero->local_transform.position;
    session.setSelectedObject(hero_id);

    // Project a point 65% along the active +X Translate handle to exact screen coordinates!
    const auto frame = session.computeActiveGizmoFrame();
    FE_REQUIRE_TRUE(frame.has_value());
    const math::Vec3 handle_world_pt = frame->origin + frame->axes[0] * (frame->scale * 0.65f);

    const auto proj_pt = session.projectWorldPointToScreen(handle_world_pt);
    FE_REQUIRE_TRUE(proj_pt.has_value());
    const float sx = proj_pt->x;
    const float sy = proj_pt->y;

    // 1. Hover over +X handle -> sets hoveredHandle() to TranslateX!
    window.pushInputEvent(platform::CursorMoveEvent{.x = sx, .y = sy});
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());
    FE_EXPECT_EQ(session.hoveredGizmoHandle(), app::GizmoHandle::TranslateX);

    // 2. Begin drag on +X handle, drag right, then press Escape -> cancels cleanly without polluting Undo!
    window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left,
        .action = platform::KeyAction::Press,
        .x = sx,
        .y = sy,
    });
    window.pushInputEvent(platform::CursorMoveEvent{.x = sx + 48.0, .y = sy});
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());
    FE_EXPECT_TRUE(session.isGizmoDragging());
    FE_EXPECT_EQ(session.selectedObject(), hero_id); // Never reselects background object during drag!
    FE_EXPECT_TRUE((session.document().scene.findObject(hero_id)->local_transform.position - orig_pos).length() > 0.05f);

    window.pushInputEvent(platform::KeyEvent{.key = platform::KeyCode::Escape, .action = platform::KeyAction::Press});
    window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left,
        .action = platform::KeyAction::Release,
        .x = sx + 48.0,
        .y = sy,
    });
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());
    FE_EXPECT_FALSE(session.isGizmoDragging());
    FE_EXPECT_TRUE(session.document().scene.findObject(hero_id)->local_transform.position.approxEqual(orig_pos, 1e-4f));
    FE_EXPECT_EQ(session.commandStack().undoCount(), std::size_t{0});

    // 3. Commit a real +X drag -> pushes exactly ONE atomic Undo command and supports Undo/Redo!
    window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left,
        .action = platform::KeyAction::Press,
        .x = sx,
        .y = sy,
    });
    window.pushInputEvent(platform::CursorMoveEvent{.x = sx + 30.0, .y = sy});
    window.pushInputEvent(platform::CursorMoveEvent{.x = sx + 60.0, .y = sy});
    window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left,
        .action = platform::KeyAction::Release,
        .x = sx + 60.0,
        .y = sy,
    });
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());
    const math::Vec3 dragged_pos = session.document().scene.findObject(hero_id)->local_transform.position;
    FE_EXPECT_TRUE(dragged_pos.x > orig_pos.x + 0.1f);
    FE_EXPECT_EQ(session.commandStack().undoCount(), std::size_t{1});

    window.pushInputEvent(platform::KeyEvent{
        .key = platform::KeyCode::Z,
        .action = platform::KeyAction::Press,
        .mods = {.ctrl = true},
    });
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());
    FE_EXPECT_TRUE(session.document().scene.findObject(hero_id)->local_transform.position.approxEqual(orig_pos, 1e-4f));

    // 4. Press Tab to enter Face Edit mode on HeroCube and click viewport center to pick a polygon face!
    window.pushInputEvent(platform::KeyEvent{.key = platform::KeyCode::Tab, .action = platform::KeyAction::Press});
    window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left,
        .action = platform::KeyAction::Press,
        .x = 200.0,
        .y = 150.0,
    });
    window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left,
        .action = platform::KeyAction::Release,
        .x = 200.0,
        .y = 150.0,
    });
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());
    FE_EXPECT_EQ(session.editorMode(), scene::EditMode::Face);
    FE_EXPECT_EQ(session.meshSelection().faces.size(), std::size_t{1});

    engine.shutdown();
    window.shutdown();
}

FE_TEST(Viewport, Stage53ViewNavigationGizmoSmoothPresetsOrthoSyncAndSceneIsolation) {
    platform::HeadlessWindow window;
    FE_REQUIRE_TRUE(window.initialize({.title = "Stage53NavTest", .width = 480, .height = 300}).hasValue());

    app::StudioSession session(
        window,
        std::make_unique<rendering::SoftwareRenderDevice>(),
        app::StudioSessionConfig{.animate_showcase_rig = false});
    core::Engine engine(core::EngineConfig{});
    FE_REQUIRE_TRUE(engine.initialize(&session).hasValue());
    app::EditorController editor(session);

    const auto* hero_obj = session.document().scene.findObjectByName("HeroCube");
    const auto* cam_obj = session.document().scene.findObjectByName("MainCamera");
    const auto* sun_obj = session.document().scene.findObjectByName("SunLight");
    FE_REQUIRE_TRUE(hero_obj != nullptr && cam_obj != nullptr && sun_obj != nullptr);

    const scene::ObjectId hero_id = hero_obj->id;
    const scene::ObjectId cam_id = cam_obj->id;
    const scene::ObjectId sun_id = sun_obj->id;

    FE_REQUIRE_TRUE(editor.selectObject(hero_id).hasValue());
    const math::Transform hero_init = session.document().scene.findObject(hero_id)->local_transform;
    const math::Transform cam_init = session.document().scene.findObject(cam_id)->local_transform;
    const math::Transform sun_init = session.document().scene.findObject(sun_id)->local_transform;

    // 1. Even if Active Scene Camera Lens View was enabled, using the 3D View Navigation Gizmo
    //    detaches cleanly to the editor observer camera and NEVER modifies MainCamera, HeroCube, or SunLight!
    editor.setCameraViewActive(true);
    FE_EXPECT_TRUE(session.isCameraViewActive());

    FE_REQUIRE_TRUE(editor.executeActionJson(R"({"action":"set_viewport_view","view":"pos_x","smooth":true})").hasValue());
    FE_EXPECT_FALSE(session.isCameraViewActive());
    FE_EXPECT_TRUE(session.isViewTransitionActive());

    // Step engine frames until smooth 0.22s transition completes.
    for (int i = 0; i < 26 && session.isViewTransitionActive(); ++i) {
        FE_REQUIRE_TRUE(engine.stepFrame().hasValue());
    }
    FE_EXPECT_FALSE(session.isViewTransitionActive());
    FE_EXPECT_NEAR(session.cameraController().yawRadians(), math::kHalfPi, 1e-2f);
    FE_EXPECT_NEAR(session.cameraController().pitchRadians(), 0.0f, 1e-2f);

    // Verify all 6 principal views & diagonal views orient observer camera accurately.
    struct ViewCheck {
        const char* slug;
        float expected_yaw;
        float expected_pitch;
    };
    const ViewCheck checks[] = {
        {"neg_x", -math::kHalfPi, 0.0f},
        {"pos_y", 0.0f, -math::degToRad(89.0f)},
        {"neg_y", 0.0f, math::degToRad(89.0f)},
        {"pos_z", 0.0f, 0.0f},
        {"neg_z", math::kPi, 0.0f},
        {"diag_px_py_pz", math::degToRad(45.0f), -0.61547971f},
        {"diag_nx_py_nz", math::degToRad(-135.0f), -0.61547971f},
    };
    for (const auto& chk : checks) {
        const std::string payload =
            std::string("{\"action\":\"set_viewport_view\",\"view\":\"") + chk.slug + "\",\"smooth\":false}";
        FE_REQUIRE_TRUE(editor.executeActionJson(payload).hasValue());
        const float yaw_diff = std::remainder(session.cameraController().yawRadians() - chk.expected_yaw, math::kTwoPi);
        FE_EXPECT_NEAR(yaw_diff, 0.0f, 1e-3f);
        FE_EXPECT_NEAR(session.cameraController().pitchRadians(), chk.expected_pitch, 1e-3f);
    }

    // 2. Dragging navigation gizmo orbits editor observer camera only.
    const float yaw_before_drag = session.cameraController().yawRadians();
    FE_REQUIRE_TRUE(editor.executeActionJson(R"({"action":"orbit_editor_camera","dx":64.0,"dy":-28.0})").hasValue());
    FE_EXPECT_TRUE(std::fabs(session.cameraController().yawRadians() - yaw_before_drag) > 0.05f);

    // 3. Perspective <-> Orthographic switching synchronizes zoom height and leaves scene MainCamera untouched.
    FE_EXPECT_EQ(session.viewportProjection(), scene::ProjectionType::Perspective);
    FE_REQUIRE_TRUE(editor.executeActionJson(R"({"action":"toggle_viewport_projection"})").hasValue());
    FE_EXPECT_EQ(session.viewportProjection(), scene::ProjectionType::Orthographic);

    const float ortho_h_before_zoom = session.activeCameraState().camera.orthographic_height;
    FE_REQUIRE_TRUE(session.zoomViewportCamera(2.0f).hasValue());
    const float ortho_h_after_zoom = session.activeCameraState().camera.orthographic_height;
    FE_EXPECT_TRUE(ortho_h_after_zoom < ortho_h_before_zoom);
    FE_EXPECT_EQ(
        session.document().scene.findObject(cam_id)->camera->projection,
        scene::ProjectionType::Perspective);

    // 4. Reset / Home View returns smoothly to Home Perspective view.
    FE_REQUIRE_TRUE(editor.executeActionJson(R"({"action":"reset_viewport_view","smooth":false})").hasValue());
    FE_EXPECT_EQ(session.viewportProjection(), scene::ProjectionType::Perspective);
    FE_EXPECT_TRUE(session.cameraController().target().approxEqual({0.0f, 0.80f, 0.0f}, 1e-3f));

    // Confirm ZERO scene object mutations & ZERO undo stack entries from all navigation gizmo actions!
    FE_EXPECT_EQ(session.selectedObject(), hero_id);
    FE_EXPECT_EQ(session.commandStack().undoCount(), std::size_t{0});
    FE_EXPECT_TRUE(session.document().scene.findObject(hero_id)->local_transform.approxEqual(hero_init, 1e-5f));
    FE_EXPECT_TRUE(session.document().scene.findObject(cam_id)->local_transform.approxEqual(cam_init, 1e-5f));
    FE_EXPECT_TRUE(session.document().scene.findObject(sun_id)->local_transform.approxEqual(sun_init, 1e-5f));

    // 5. HiDPI gizmo sizing check via HeadlessWindow::setContentScale (480x300 @ 1x == 960x600 @ 2x).
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());
    const auto g_1x = session.computeActiveGizmoFrame();
    window.resize(960, 600);
    window.setContentScale(2.0f);
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());
    const auto g_2x = session.computeActiveGizmoFrame();
    FE_REQUIRE_TRUE(g_1x.has_value() && g_2x.has_value());
    FE_EXPECT_NEAR(g_1x->scale, g_2x->scale, 1e-4f);

    engine.shutdown();
    window.shutdown();
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
