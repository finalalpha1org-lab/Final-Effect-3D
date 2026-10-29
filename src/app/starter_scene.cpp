// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/starter_scene.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/scene/primitives.hpp"

namespace finaleffect::app {

core::Result<StarterSceneHandles> populateStarterScene(scene::Scene& scene) {
    scene.clear();
    scene.setName("Final Effect Stage 1 Showcase");

    auto cube_mesh = scene.registerMesh(
        "HeroCubeMesh",
        scene::Primitives::createCube(1.35f, math::Vec3{0.88f, 0.46f, 0.18f}));
    if (!cube_mesh) return cube_mesh.error();

    auto pyramid_mesh = scene.registerMesh(
        "AccentPyramidMesh",
        scene::Primitives::createPyramid(0.85f, 1.15f, math::Vec3{0.20f, 0.68f, 0.88f}));
    if (!pyramid_mesh) return pyramid_mesh.error();

    auto ground_mesh = scene.registerMesh(
        "StudioGroundMesh",
        scene::Primitives::createCheckerPlane(4.5f, 6));
    if (!ground_mesh) return ground_mesh.error();

    StarterSceneHandles handles{};

    // Ground plane at y = 0.
    auto ground_res = scene.createObject("GroundPlane");
    if (!ground_res) return ground_res.error();
    handles.ground_plane = ground_res.value();
    if (auto st = scene.assignMesh(handles.ground_plane, ground_mesh.value()); !st) return st.error();

    // Hierarchical rig pivot at center.
    auto pivot_res = scene.createObject("RigPivot");
    if (!pivot_res) return pivot_res.error();
    handles.rig_pivot = pivot_res.value();

    // Hero cube parented to RigPivot.
    auto cube_res = scene.createObject("HeroCube", handles.rig_pivot);
    if (!cube_res) return cube_res.error();
    handles.hero_cube = cube_res.value();
    math::Transform cube_tf;
    cube_tf.position = {0.0f, 0.85f, 0.0f};
    cube_tf.rotation = math::Quat::fromEulerXYZ(
        {math::degToRad(15.0f), math::degToRad(25.0f), 0.0f});
    if (auto st = scene.setObjectTransform(handles.hero_cube, cube_tf); !st) return st.error();
    if (auto st = scene.assignMesh(handles.hero_cube, cube_mesh.value()); !st) return st.error();

    // Orbiting accent pyramid parented to RigPivot.
    auto pyr_res = scene.createObject("AccentPyramid", handles.rig_pivot);
    if (!pyr_res) return pyr_res.error();
    handles.accent_pyramid = pyr_res.value();
    math::Transform pyr_tf;
    pyr_tf.position = {1.85f, 0.70f, 0.0f};
    if (auto st = scene.setObjectTransform(handles.accent_pyramid, pyr_tf); !st) return st.error();
    if (auto st = scene.assignMesh(handles.accent_pyramid, pyramid_mesh.value()); !st) return st.error();

    // Studio camera looking downward toward scene center.
    auto cam_res = scene.createObject("MainCamera");
    if (!cam_res) return cam_res.error();
    handles.camera = cam_res.value();

    const math::Vec3 eye{0.0f, 2.75f, 5.6f};
    const math::Vec3 target{0.0f, 0.65f, 0.0f};
    const math::Mat4 view = math::Mat4::lookAtRH(eye, target, math::Vec3::unitY());
    // Camera world orientation: pitch downward around X to face target along -Z.
    const float pitch = -std::atan2(eye.y - target.y, eye.z - target.z);
    (void)view;
    math::Transform cam_tf;
    cam_tf.position = eye;
    cam_tf.rotation = math::Quat::fromAxisAngle(math::Vec3::unitX(), pitch);
    if (auto st = scene.setObjectTransform(handles.camera, cam_tf); !st) return st.error();

    scene::CameraComponent cam_comp;
    cam_comp.projection = scene::ProjectionType::Perspective;
    cam_comp.fov_y_radians = math::degToRad(48.0f);
    cam_comp.near_plane = 0.1f;
    cam_comp.far_plane = 200.0f;
    if (auto st = scene.assignCamera(handles.camera, cam_comp); !st) return st.error();
    if (auto st = scene.setActiveCamera(handles.camera); !st) return st.error();

    // Directional key light angled down and left.
    auto light_res = scene.createObject("SunLight");
    if (!light_res) return light_res.error();
    handles.sun_light = light_res.value();
    math::Transform light_tf;
    light_tf.rotation = math::Quat::fromEulerXYZ(
        {math::degToRad(-50.0f), math::degToRad(35.0f), 0.0f});
    if (auto st = scene.setObjectTransform(handles.sun_light, light_tf); !st) return st.error();
    if (auto st = scene.assignDirectionalLight(
            handles.sun_light,
            scene::DirectionalLightComponent{.color = {1.0f, 0.98f, 0.94f}, .intensity = 1.15f});
        !st) {
        return st.error();
    }

    return core::Result<StarterSceneHandles>::ok(handles);
}

} // namespace finaleffect::app
