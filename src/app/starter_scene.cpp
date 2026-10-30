// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/starter_scene.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/scene/editable_mesh.hpp"

#include <cmath>

namespace finaleffect::app {

core::Result<StarterSceneHandles> populateStarterScene(scene::Scene& scene) {
    scene.clear();
    scene.setName("Final Effect Stage 1 Showcase");

    auto cube_mesh = scene.registerEditableMesh(
        "HeroCubeMesh",
        scene::EditableMesh::createCube(1.35f, math::Vec3{0.88f, 0.46f, 0.18f}));
    if (!cube_mesh) return cube_mesh.error();

    auto pyramid_mesh = scene.registerEditableMesh(
        "AccentPyramidMesh",
        scene::EditableMesh::createPyramid(0.85f, 1.15f, math::Vec3{0.20f, 0.68f, 0.88f}));
    if (!pyramid_mesh) return pyramid_mesh.error();

    // Register starter PBR materials for immediate metallic-roughness editing & preview.
    auto copper_mat = scene.createMaterial(scene::PbrMaterial{
        .name = "Anodized Copper",
        .base_color = {0.88f, 0.46f, 0.20f},
        .metallic = 0.84f,
        .roughness = 0.24f,
        .ambient_occlusion = 1.0f,
    });
    if (!copper_mat) return copper_mat.error();

    auto cobalt_mat = scene.createMaterial(scene::PbrMaterial{
        .name = "Cobalt Ceramic",
        .base_color = {0.20f, 0.68f, 0.90f},
        .metallic = 0.14f,
        .roughness = 0.22f,
        .ambient_occlusion = 1.0f,
    });
    if (!cobalt_mat) return cobalt_mat.error();

    auto glass_mat = scene.createMaterial(scene::PbrMaterial{
        .name = "Frosted Studio Glass",
        .base_color = {0.72f, 0.88f, 0.98f},
        .metallic = 0.04f,
        .roughness = 0.14f,
        .ambient_occlusion = 1.0f,
        .opacity = 0.55f,
    });
    if (!glass_mat) return glass_mat.error();

    StarterSceneHandles handles{};

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
    if (auto st = scene.assignMaterial(handles.hero_cube, copper_mat.value()); !st) return st.error();

    // Orbiting accent pyramid parented to RigPivot.
    auto pyr_res = scene.createObject("AccentPyramid", handles.rig_pivot);
    if (!pyr_res) return pyr_res.error();
    handles.accent_pyramid = pyr_res.value();
    math::Transform pyr_tf;
    pyr_tf.position = {1.85f, 0.70f, 0.0f};
    if (auto st = scene.setObjectTransform(handles.accent_pyramid, pyr_tf); !st) return st.error();
    if (auto st = scene.assignMesh(handles.accent_pyramid, pyramid_mesh.value()); !st) return st.error();
    if (auto st = scene.assignMaterial(handles.accent_pyramid, cobalt_mat.value()); !st) return st.error();

    // Studio scene camera positioned inside the stage framing the hero rig from a 3/4 angle.
    auto cam_res = scene.createObject("MainCamera");
    if (!cam_res) return cam_res.error();
    handles.camera = cam_res.value();

    const math::Vec3 eye{1.55f, 2.05f, 1.95f};
    const math::Vec3 target{0.0f, 0.72f, 0.0f};
    const math::Vec3 cam_offset = eye - target;
    const float cam_horiz = std::sqrt(cam_offset.x * cam_offset.x + cam_offset.z * cam_offset.z);
    const float cam_yaw = std::atan2(cam_offset.x, cam_offset.z);
    const float cam_pitch = -std::atan2(cam_offset.y, cam_horiz);
    math::Transform cam_tf;
    cam_tf.position = eye;
    cam_tf.rotation = math::Quat::fromEulerXYZ({cam_pitch, cam_yaw, 0.0f});
    if (auto st = scene.setObjectTransform(handles.camera, cam_tf); !st) return st.error();

    scene::CameraComponent cam_comp;
    cam_comp.projection = scene::ProjectionType::Perspective;
    cam_comp.fov_y_radians = math::degToRad(48.0f);
    cam_comp.near_plane = 0.1f;
    cam_comp.far_plane = 200.0f;
    if (auto st = scene.assignCamera(handles.camera, cam_comp); !st) return st.error();
    if (auto st = scene.setActiveCamera(handles.camera); !st) return st.error();

    // Directional key light elevated on stage-left, shining down-right onto the hero rig.
    auto light_res = scene.createObject("SunLight");
    if (!light_res) return light_res.error();
    handles.sun_light = light_res.value();
    math::Transform light_tf;
    light_tf.position = {-1.45f, 2.25f, 0.85f};
    light_tf.rotation = math::Quat::fromEulerXYZ(
        {math::degToRad(-42.0f), math::degToRad(-52.0f), 0.0f});
    if (auto st = scene.setObjectTransform(handles.sun_light, light_tf); !st) return st.error();
    if (auto st = scene.assignDirectionalLight(
            handles.sun_light,
            scene::DirectionalLightComponent{
                .color = {1.0f, 0.98f, 0.94f},
                .intensity = 1.65f,
                .enabled = true,
                .shadow = {
                    .enabled = true,
                    .strength = 1.0f,
                    .quality = scene::ShadowQuality::High,
                    .bias = 0.0020f,
                    .normal_bias = 0.015f,
                    .softness = 1.0f,
                },
            });
        !st) {
        return st.error();
    }

    return core::Result<StarterSceneHandles>::ok(handles);
}

} // namespace finaleffect::app
