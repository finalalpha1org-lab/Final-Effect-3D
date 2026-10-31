// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "test_harness.hpp"

#include "finaleffect/app/orbit_camera_controller.hpp"
#include "finaleffect/app/transform_gizmo.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/scene/commands.hpp"
#include "finaleffect/scene/scene.hpp"

namespace {

using namespace finaleffect;

FE_TEST(Gizmo, TranslateAxisAndPlanePickingPerspectiveAndOrtho) {
    const math::Transform cam_tf{
        .position = {3.5f, 3.0f, 5.5f},
        .rotation = math::Quat::fromEulerXYZ({math::degToRad(-25.0f), math::degToRad(30.0f), 0.0f}),
        .scale = {1.0f, 1.0f, 1.0f},
    };
    const math::Mat4 cam_world = cam_tf.toMatrix();

    scene::CameraComponent persp{};
    persp.projection = scene::ProjectionType::Perspective;
    persp.fov_y_radians = math::degToRad(50.0f);

    const auto frame = app::TransformGizmo::computeFrame(
        math::Mat4::identity(), cam_world, persp, app::GizmoMode::Translate, app::GizmoSpace::World);
    FE_EXPECT_TRUE(frame.scale > 0.1f);

    // Ray aimed directly at +X shaft midpoint must pick TranslateX.
    const math::Vec3 x_target = frame.origin + frame.axes[0] * (0.6f * frame.scale);
    const math::Ray x_ray(cam_tf.position, (x_target - cam_tf.position).normalized());
    const auto x_pick = app::TransformGizmo::pickHandle(app::GizmoMode::Translate, frame, x_ray);
    FE_REQUIRE_TRUE(x_pick.has_value());
    FE_EXPECT_EQ(x_pick->handle, app::GizmoHandle::TranslateX);

    // Ray aimed at XY plane quad center must pick TranslateXY.
    const math::Vec3 xy_target =
        frame.origin + (frame.axes[0] + frame.axes[1]) * (0.36f * frame.scale);
    const math::Ray xy_ray(cam_tf.position, (xy_target - cam_tf.position).normalized());
    const auto xy_pick = app::TransformGizmo::pickHandle(app::GizmoMode::Translate, frame, xy_ray);
    FE_REQUIRE_TRUE(xy_pick.has_value());
    FE_EXPECT_EQ(xy_pick->handle, app::GizmoHandle::TranslateXY);

    // Orthographic camera frame & +Z axis drag constraining Y/X to 0.
    scene::CameraComponent ortho{};
    ortho.projection = scene::ProjectionType::Orthographic;
    ortho.orthographic_height = 8.0f;
    const auto ortho_frame = app::TransformGizmo::computeFrame(
        math::Mat4::identity(), cam_world, ortho, app::GizmoMode::Translate, app::GizmoSpace::World);

    app::TransformGizmo gizmo;
    const math::Vec3 z_start = ortho_frame.origin + ortho_frame.axes[2] * (0.5f * ortho_frame.scale);
    const math::Ray ray0(z_start + math::Vec3{4.0f, 4.0f, 0.0f}, {-0.70710678f, -0.70710678f, 0.0f});
    FE_REQUIRE_TRUE(gizmo.beginDrag(
        app::GizmoHandle::TranslateZ,
        ortho_frame,
        ray0,
        cam_world,
        math::Transform::identity(),
        math::Mat4::identity()).hasValue());

    // Parallel orthographic ray shifted +2.5 along Z and +1.2 along Y -> movement constrained strictly to Z!
    const math::Ray ray1(ray0.origin + math::Vec3{0.0f, 1.2f, 2.5f}, ray0.direction);
    const auto out = gizmo.updateDrag(ray1);
    FE_REQUIRE_TRUE(out.hasValue());
    FE_EXPECT_NEAR(out.value().local_transform.position.x, 0.0f, 1e-4f);
    FE_EXPECT_NEAR(out.value().local_transform.position.y, 0.0f, 1e-4f);
    FE_EXPECT_NEAR(out.value().local_transform.position.z, 2.5f, 1e-4f);

    // Cancel drag restores exact identity transform.
    const auto cancelled = gizmo.cancelDrag();
    FE_EXPECT_FALSE(gizmo.isDragging());
    FE_EXPECT_TRUE(cancelled.approxEqual(math::Transform::identity(), 1e-6f));
}

FE_TEST(Gizmo, RotateRingScaleHandlesUndoCoalesceAndMeshGeneration) {
    scene::Scene scene;
    scene::CommandStack stack;
    const auto obj_id = scene.createObject("GizmoCube").value();

    const math::Transform cam_tf{
        .position = {0.0f, 4.0f, 5.0f},
        .rotation = math::Quat::fromEulerXYZ({math::degToRad(-38.0f), 0.0f, 0.0f}),
        .scale = {1.0f, 1.0f, 1.0f},
    };
    const math::Mat4 cam_world = cam_tf.toMatrix();
    scene::CameraComponent cam{};

    // 1. RotateY ring pick at 45 deg in XZ plane (away from ring-crossing poles) & exact 90-degree rotation around +Y.
    const auto rot_frame = app::TransformGizmo::computeFrame(
        math::Mat4::identity(), cam_world, cam, app::GizmoMode::Rotate, app::GizmoSpace::World);
    const float diag_r = 0.92f * rot_frame.scale * 0.70710678f;
    const math::Vec3 ring_pt0 = rot_frame.origin + math::Vec3{diag_r, 0.0f, diag_r};
    const math::Ray r0(cam_tf.position, (ring_pt0 - cam_tf.position).normalized());

    const auto ring_pick = app::TransformGizmo::pickHandle(app::GizmoMode::Rotate, rot_frame, r0);
    FE_REQUIRE_TRUE(ring_pick.has_value());
    FE_EXPECT_EQ(ring_pick->handle, app::GizmoHandle::RotateY);

    app::TransformGizmo gizmo;
    FE_REQUIRE_TRUE(gizmo.beginDrag(
        app::GizmoHandle::RotateY,
        rot_frame,
        r0,
        cam_world,
        math::Transform::identity(),
        math::Mat4::identity()).hasValue());

    // Dragging +90 deg counter-clockwise around +Y moves (+d, 0, +d) -> (+d, 0, -d), rotating +X -> -Z!
    const math::Vec3 ring_pt90 = rot_frame.origin + math::Vec3{diag_r, 0.0f, -diag_r};
    const math::Ray r90(cam_tf.position, (ring_pt90 - cam_tf.position).normalized());
    const auto rot_out = gizmo.updateDrag(r90);
    FE_REQUIRE_TRUE(rot_out.hasValue());
    const math::Vec3 rotated_x = rot_out.value().local_transform.rotation.rotate({1.0f, 0.0f, 0.0f});
    FE_EXPECT_TRUE(rotated_x.approxEqual({0.0f, 0.0f, -1.0f}, 1e-4f));
    gizmo.endDrag();

    // 2. ScaleUniform & ScaleX picking + continuous drag coalescing on CommandStack.
    const auto scl_frame = app::TransformGizmo::computeFrame(
        math::Mat4::identity(), cam_world, cam, app::GizmoMode::Scale, app::GizmoSpace::Local);
    const math::Ray center_ray(cam_tf.position, (scl_frame.origin - cam_tf.position).normalized());
    const auto center_pick = app::TransformGizmo::pickHandle(app::GizmoMode::Scale, scl_frame, center_ray);
    FE_REQUIRE_TRUE(center_pick.has_value());
    FE_EXPECT_EQ(center_pick->handle, app::GizmoHandle::ScaleUniform);

    const math::Vec3 sx_pt = scl_frame.origin + math::Vec3{0.6f * scl_frame.scale, 0.0f, 0.0f};
    const math::Ray sx_ray0(cam_tf.position, (sx_pt - cam_tf.position).normalized());
    const auto sx_pick = app::TransformGizmo::pickHandle(app::GizmoMode::Scale, scl_frame, sx_ray0);
    FE_REQUIRE_TRUE(sx_pick.has_value());
    FE_EXPECT_EQ(sx_pick->handle, app::GizmoHandle::ScaleX);

    FE_REQUIRE_TRUE(gizmo.beginDrag(
        app::GizmoHandle::ScaleX,
        scl_frame,
        sx_ray0,
        cam_world,
        scene.findObject(obj_id)->local_transform,
        math::Mat4::identity()).hasValue());

    for (int step = 1; step <= 4; ++step) {
        const math::Vec3 target = sx_pt + math::Vec3{0.25f * static_cast<float>(step) * scl_frame.scale, 0.0f, 0.0f};
        const auto step_out = gizmo.updateDrag(math::Ray(cam_tf.position, (target - cam_tf.position).normalized()));
        FE_REQUIRE_TRUE(step_out.hasValue());
        FE_REQUIRE_TRUE(stack.execute(
            scene,
            std::make_unique<scene::SetTransformCommand>(obj_id, step_out.value().local_transform, true)).hasValue());
    }
    gizmo.endDrag();

    FE_EXPECT_EQ(stack.undoCount(), static_cast<std::size_t>(1));
    FE_EXPECT_NEAR(scene.findObject(obj_id)->local_transform.scale.x, 2.0f, 1e-3f);
    FE_EXPECT_NEAR(scene.findObject(obj_id)->local_transform.scale.y, 1.0f, 1e-5f);

    // Single undo restores initial unit scale!
    FE_REQUIRE_TRUE(stack.undo(scene).hasValue());
    FE_EXPECT_NEAR(scene.findObject(obj_id)->local_transform.scale.x, 1.0f, 1e-5f);

    // 3. Verify 3D gizmo render meshes pass full MeshData::validate() across all 3 modes.
    FE_EXPECT_TRUE(app::TransformGizmo::buildRenderMesh(app::GizmoMode::Translate, rot_frame).validate().hasValue());
    FE_EXPECT_TRUE(app::TransformGizmo::buildRenderMesh(app::GizmoMode::Rotate, rot_frame).validate().hasValue());
    FE_EXPECT_TRUE(app::TransformGizmo::buildRenderMesh(app::GizmoMode::Scale, scl_frame).validate().hasValue());
}

FE_TEST(Gizmo, Stage53ScreenSpaceSizingHiDpiAndMobileLandscape) {
    scene::CameraComponent persp{};
    persp.projection = scene::ProjectionType::Perspective;
    persp.fov_y_radians = math::degToRad(48.0f);

    // 1. Camera distance invariance: 3m vs 24m maintains identical screen-space angular size.
    math::Transform near_cam{.position = {0.0f, 0.0f, 3.0f}};
    math::Transform far_cam{.position = {0.0f, 0.0f, 24.0f}};
    const auto f_near = app::TransformGizmo::computeFrame(
        math::Mat4::identity(), near_cam.toMatrix(), persp,
        app::GizmoMode::Translate, app::GizmoSpace::World, 1920, 1080, 1.0f);
    const auto f_far = app::TransformGizmo::computeFrame(
        math::Mat4::identity(), far_cam.toMatrix(), persp,
        app::GizmoMode::Translate, app::GizmoSpace::World, 1920, 1080, 1.0f);
    FE_EXPECT_NEAR(f_far.scale / 24.0f, f_near.scale / 3.0f, 1e-4f);

    // 2. High-DPI consistency: 3840x2160 at 2.0x DPR yields exact same world scale as 1920x1080 at 1.0x DPR.
    const auto f_hidpi = app::TransformGizmo::computeFrame(
        math::Mat4::identity(), near_cam.toMatrix(), persp,
        app::GizmoMode::Translate, app::GizmoSpace::World, 3840, 2160, 2.0f);
    FE_EXPECT_NEAR(f_hidpi.scale, f_near.scale, 1e-5f);

    // 3. Mobile Landscape (844x240 stage) keeps gizmo accessible (>= 80 logical px) without overflowing height.
    const auto f_mobile = app::TransformGizmo::computeFrame(
        math::Mat4::identity(), near_cam.toMatrix(), persp,
        app::GizmoMode::Translate, app::GizmoSpace::World, 844 * 2, 240 * 2, 2.0f);
    const float world_per_logical_px =
        (2.0f * 3.0f * std::tan(persp.fov_y_radians * 0.5f)) / 240.0f;
    const float mobile_logical_px = f_mobile.scale / world_per_logical_px;
    FE_EXPECT_TRUE(mobile_logical_px >= 80.0f);
    FE_EXPECT_TRUE(mobile_logical_px <= 240.0f * 0.37f);
}

FE_TEST(Gizmo, Stage53PlaneCenterGrazingRotateLocalSpaceAndHighlightStates) {
    const math::Transform cam_tf{
        .position = {3.2f, 2.8f, 4.8f},
        .rotation = math::Quat::fromEulerXYZ({math::degToRad(-26.0f), math::degToRad(32.0f), 0.0f}),
        .scale = {1.0f, 1.0f, 1.0f},
    };
    const math::Mat4 cam_world = cam_tf.toMatrix();
    scene::CameraComponent persp{};

    const auto frame = app::TransformGizmo::computeFrame(
        math::Mat4::identity(), cam_world, persp,
        app::GizmoMode::Translate, app::GizmoSpace::World, 1280, 720, 1.0f);

    // 1. Center free-view move handle (TranslateView) picking & camera-plane constrained drag.
    const math::Ray center_ray(cam_tf.position, (frame.origin - cam_tf.position).normalized());
    const auto center_pick = app::TransformGizmo::pickHandle(app::GizmoMode::Translate, frame, center_ray);
    FE_REQUIRE_TRUE(center_pick.has_value());
    FE_EXPECT_EQ(center_pick->handle, app::GizmoHandle::TranslateView);

    app::TransformGizmo gizmo;
    FE_REQUIRE_TRUE(gizmo.beginDrag(
        app::GizmoHandle::TranslateView, frame, center_ray,
        cam_world, math::Transform::identity(), math::Mat4::identity()).hasValue());
    const math::Vec3 cam_right = cam_world.transformDirection({1.0f, 0.0f, 0.0f}).normalized();
    const math::Vec3 cam_fwd = cam_world.transformDirection({0.0f, 0.0f, -1.0f}).normalized();
    const math::Vec3 view_goal = frame.origin + cam_right * 1.25f;
    const auto view_out = gizmo.updateDrag(math::Ray(cam_tf.position, (view_goal - cam_tf.position).normalized()));
    FE_REQUIRE_TRUE(view_out.hasValue());
    FE_EXPECT_NEAR(view_out.value().world_translation_delta.dot(cam_fwd), 0.0f, 1e-4f);
    FE_EXPECT_NEAR(view_out.value().world_translation_delta.dot(cam_right), 1.25f, 1e-3f);
    gizmo.endDrag();

    // 2. XZ and YZ planar handle picking & strict two-axis plane movement.
    const math::Vec3 xz_pt = frame.origin + (frame.axes[0] + frame.axes[2]) * (0.39f * frame.scale);
    const math::Ray xz_ray0(cam_tf.position, (xz_pt - cam_tf.position).normalized());
    const auto xz_pick = app::TransformGizmo::pickHandle(app::GizmoMode::Translate, frame, xz_ray0);
    FE_REQUIRE_TRUE(xz_pick.has_value());
    FE_EXPECT_EQ(xz_pick->handle, app::GizmoHandle::TranslateXZ);

    FE_REQUIRE_TRUE(gizmo.beginDrag(
        app::GizmoHandle::TranslateXZ, frame, xz_ray0,
        cam_world, math::Transform::identity(), math::Mat4::identity()).hasValue());
    const math::Vec3 xz_pt1 = xz_pt + math::Vec3{1.5f, 0.0f, -0.8f};
    const auto xz_out = gizmo.updateDrag(math::Ray(cam_tf.position, (xz_pt1 - cam_tf.position).normalized()));
    FE_REQUIRE_TRUE(xz_out.hasValue());
    FE_EXPECT_NEAR(xz_out.value().local_transform.position.x, 1.5f, 1e-3f);
    FE_EXPECT_NEAR(xz_out.value().local_transform.position.y, 0.0f, 1e-5f);
    FE_EXPECT_NEAR(xz_out.value().local_transform.position.z, -0.8f, 1e-3f);
    gizmo.endDrag();

    const math::Vec3 yz_pt = frame.origin + (frame.axes[1] + frame.axes[2]) * (0.39f * frame.scale);
    const auto yz_pick = app::TransformGizmo::pickHandle(
        app::GizmoMode::Translate, frame, math::Ray(cam_tf.position, (yz_pt - cam_tf.position).normalized()));
    FE_REQUIRE_TRUE(yz_pick.has_value());
    FE_EXPECT_EQ(yz_pick->handle, app::GizmoHandle::TranslateYZ);

    // 3. Grazing-angle rotation ring picking & smooth edge-on tangent rotation.
    const math::Transform grazing_cam{.position = {0.0f, 0.08f, 5.0f}};
    const math::Mat4 grazing_cam_world = grazing_cam.toMatrix();
    const auto grazing_rot_frame = app::TransformGizmo::computeFrame(
        math::Mat4::identity(), grazing_cam_world, persp,
        app::GizmoMode::Rotate, app::GizmoSpace::World, 1280, 720, 1.0f);
    const math::Vec3 front_arc = grazing_rot_frame.origin + math::Vec3{0.0f, 0.0f, 0.92f * grazing_rot_frame.scale};
    const math::Ray gr0(grazing_cam.position, (front_arc - grazing_cam.position).normalized());
    const auto gr_pick = app::TransformGizmo::pickHandle(app::GizmoMode::Rotate, grazing_rot_frame, gr0);
    FE_REQUIRE_TRUE(gr_pick.has_value());
    FE_EXPECT_EQ(gr_pick->handle, app::GizmoHandle::RotateY);

    FE_REQUIRE_TRUE(gizmo.beginDrag(
        app::GizmoHandle::RotateY, grazing_rot_frame, gr0,
        grazing_cam_world, math::Transform::identity(), math::Mat4::identity()).hasValue());
    const math::Vec3 dragged_arc = front_arc + math::Vec3{0.6f * grazing_rot_frame.scale, 0.0f, 0.0f};
    const auto gr_out = gizmo.updateDrag(
        math::Ray(grazing_cam.position, (dragged_arc - grazing_cam.position).normalized()));
    FE_REQUIRE_TRUE(gr_out.hasValue());
    FE_EXPECT_TRUE(std::fabs(gr_out.value().local_transform.rotation.y) > 0.10f);
    gizmo.endDrag();

    // 4. Local-space Move respects object's local rotated axes.
    math::Transform rotated_obj = math::Transform::identity();
    rotated_obj.rotation = math::Quat::fromAxisAngle(math::Vec3::unitY(), math::kHalfPi); // +X_local == -Z_world
    const auto local_frame = app::TransformGizmo::computeFrame(
        rotated_obj.toMatrix(), cam_world, persp,
        app::GizmoMode::Translate, app::GizmoSpace::Local, 1280, 720, 1.0f);
    FE_EXPECT_TRUE(local_frame.axes[0].approxEqual({0.0f, 0.0f, -1.0f}, 1e-4f));

    // 5. Uniform scale handle drag scales all 3 axes equally.
    const auto scl_frame = app::TransformGizmo::computeFrame(
        math::Mat4::identity(), cam_world, persp,
        app::GizmoMode::Scale, app::GizmoSpace::Local, 1280, 720, 1.0f);
    FE_REQUIRE_TRUE(gizmo.beginDrag(
        app::GizmoHandle::ScaleUniform, scl_frame, center_ray,
        cam_world, math::Transform::identity(), math::Mat4::identity()).hasValue());
    const math::Vec3 uni_goal = scl_frame.origin + cam_right * (0.75f * scl_frame.scale);
    const auto uni_out = gizmo.updateDrag(math::Ray(cam_tf.position, (uni_goal - cam_tf.position).normalized()));
    FE_REQUIRE_TRUE(uni_out.hasValue());
    FE_EXPECT_TRUE(uni_out.value().local_transform.scale.x > 1.2f);
    FE_EXPECT_NEAR(uni_out.value().local_transform.scale.x, uni_out.value().local_transform.scale.y, 1e-5f);
    FE_EXPECT_NEAR(uni_out.value().local_transform.scale.x, uni_out.value().local_transform.scale.z, 1e-5f);
    gizmo.endDrag();

    // 6. Hover & Active highlight states produce distinct vertex colors + valid meshes, and near-handle halo works.
    const auto mesh_idle = app::TransformGizmo::buildRenderMesh(
        app::GizmoMode::Translate, frame, app::GizmoHandle::None, app::GizmoHandle::None);
    const auto mesh_hover = app::TransformGizmo::buildRenderMesh(
        app::GizmoMode::Translate, frame, app::GizmoHandle::TranslateX, app::GizmoHandle::None);
    const auto mesh_active = app::TransformGizmo::buildRenderMesh(
        app::GizmoMode::Translate, frame, app::GizmoHandle::TranslateX, app::GizmoHandle::TranslateX);
    FE_EXPECT_TRUE(mesh_hover.validate().hasValue());
    FE_EXPECT_TRUE(mesh_active.validate().hasValue());

    bool found_hover_diff = false;
    bool found_active_diff = false;
    for (std::size_t i = 0; i < mesh_idle.vertices.size() && i < mesh_hover.vertices.size(); ++i) {
        if (!mesh_idle.vertices[i].color.approxEqual(mesh_hover.vertices[i].color, 1e-3f)) {
            found_hover_diff = true;
        }
        if (!mesh_hover.vertices[i].color.approxEqual(mesh_active.vertices[i].color, 1e-3f)) {
            found_active_diff = true;
        }
    }
    FE_EXPECT_TRUE(found_hover_diff);
    FE_EXPECT_TRUE(found_active_diff);

    // Near-handle halo ray (just outside pickHandle radius at 0.155*s from +X shaft) triggers isNearAnyHandle.
    const math::Vec3 near_x = frame.origin + frame.axes[0] * (0.5f * frame.scale) + frame.axes[1] * (0.15f * frame.scale);
    const math::Ray near_ray(cam_tf.position, (near_x - cam_tf.position).normalized());
    FE_EXPECT_TRUE(app::TransformGizmo::isNearAnyHandle(app::GizmoMode::Translate, frame, near_ray));
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
