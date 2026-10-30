// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "test_harness.hpp"

#include "finaleffect/app/starter_scene.hpp"
#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/platform/clock.hpp"
#include "finaleffect/platform/paths.hpp"
#include "finaleffect/platform/window.hpp"
#include "finaleffect/rendering/material_preview.hpp"
#include "finaleffect/rendering/null_render_device.hpp"
#include "finaleffect/rendering/renderer.hpp"
#include "finaleffect/rendering/software_render_device.hpp"
#include "finaleffect/scene/primitives.hpp"

#include <filesystem>
#include <memory>

namespace {

using namespace finaleffect;

FE_TEST(Rendering, RendererDeduplicatesSharedMeshesAndHandlesMinimize) {
    auto null_dev = std::make_unique<rendering::NullRenderDevice>();
    auto* dev_ptr = null_dev.get();
    rendering::Renderer renderer(std::move(null_dev));

    FE_REQUIRE_TRUE(renderer.initialize(640, 360).hasValue());

    scene::Scene scene;
    const auto shared_mesh = scene.registerMesh("SharedCube", scene::Primitives::createCube(1.0f)).value();
    const auto a = scene.createObject("CubeA").value();
    const auto b = scene.createObject("CubeB").value();
    const auto hidden = scene.createObject("HiddenCube").value();

    FE_REQUIRE_TRUE(scene.assignMesh(a, shared_mesh).hasValue());
    FE_REQUIRE_TRUE(scene.assignMesh(b, shared_mesh).hasValue());
    FE_REQUIRE_TRUE(scene.assignMesh(hidden, shared_mesh).hasValue());
    FE_REQUIRE_TRUE(scene.setObjectVisible(hidden, false).hasValue());

    FE_REQUIRE_TRUE(renderer.renderScene(scene).hasValue());

    // Two visible objects drew, but only ONE GPU mesh buffer was uploaded.
    FE_EXPECT_EQ(dev_ptr->liveMeshCount(), static_cast<std::size_t>(1));
    FE_EXPECT_EQ(dev_ptr->totalDrawCalls(), static_cast<std::uint64_t>(2));
    FE_EXPECT_EQ(dev_ptr->completedFrames(), static_cast<std::uint64_t>(1));

    // Minimized 0x0 resize must not crash or fail.
    FE_REQUIRE_TRUE(renderer.resize(0, 0).hasValue());
    FE_REQUIRE_TRUE(renderer.renderScene(scene).hasValue());
    FE_EXPECT_EQ(dev_ptr->completedFrames(), static_cast<std::uint64_t>(2));

    renderer.shutdown();
    FE_EXPECT_EQ(dev_ptr->liveMeshCount(), static_cast<std::size_t>(0));
}

FE_TEST(Rendering, SoftwareRasterizerProjectsLit3DGeometryAndOccludesByDepth) {
    auto sw_dev = std::make_unique<rendering::SoftwareRenderDevice>();
    auto* sw_ptr = sw_dev.get();
    rendering::Renderer renderer(std::move(sw_dev));

    FE_REQUIRE_TRUE(renderer.initialize(128, 128).hasValue());

    scene::Scene scene;
    const auto red_mesh = scene.registerMesh(
        "FarRedCube",
        scene::Primitives::createCube(1.6f, math::Vec3{0.95f, 0.10f, 0.10f})).value();
    const auto blue_mesh = scene.registerMesh(
        "NearBlueCube",
        scene::Primitives::createCube(1.2f, math::Vec3{0.10f, 0.35f, 0.95f})).value();

    // Submit the nearer blue cube FIRST and the farther red cube SECOND to verify true depth-buffer occlusion.
    const auto near_obj = scene.createObject("NearCube").value();
    math::Transform near_tf;
    near_tf.position = {0.0f, 0.0f, 1.0f};
    FE_REQUIRE_TRUE(scene.setObjectTransform(near_obj, near_tf).hasValue());
    FE_REQUIRE_TRUE(scene.assignMesh(near_obj, blue_mesh).hasValue());

    const auto far_obj = scene.createObject("FarCube").value();
    math::Transform far_tf;
    far_tf.position = {0.0f, 0.0f, -1.5f};
    FE_REQUIRE_TRUE(scene.setObjectTransform(far_obj, far_tf).hasValue());
    FE_REQUIRE_TRUE(scene.assignMesh(far_obj, red_mesh).hasValue());

    const auto cam_obj = scene.createObject("Camera").value();
    math::Transform cam_tf;
    cam_tf.position = {0.0f, 0.0f, 4.5f};
    FE_REQUIRE_TRUE(scene.setObjectTransform(cam_obj, cam_tf).hasValue());
    FE_REQUIRE_TRUE(scene.assignCamera(cam_obj, scene::CameraComponent{}).hasValue());
    FE_REQUIRE_TRUE(scene.setActiveCamera(cam_obj).hasValue());

    rendering::FrameClearDesc clear;
    clear.clear_color = {0.05f, 0.05f, 0.05f, 1.0f};
    FE_REQUIRE_TRUE(renderer.renderScene(scene, clear).hasValue());

    const auto corner_px = sw_ptr->pixelAt(2, 2);
    const auto center_px = sw_ptr->pixelAt(64, 64);

    // Corner remains clear color (~13), while center shows the closer blue cube (blue > red).
    FE_EXPECT_TRUE(corner_px.r < 20 && corner_px.b < 20);
    FE_EXPECT_TRUE(center_px.b > center_px.r + 60);
}

FE_TEST(Rendering, StudioSessionEndToEndAnimationAndProjectReload) {
    platform::HeadlessWindow window;
    FE_REQUIRE_TRUE(window.initialize({.title = "Test", .width = 96, .height = 64}).hasValue());

    auto clock = std::make_unique<platform::ManualClock>(0.0);
    auto* clock_ptr = clock.get();

    core::EngineConfig cfg;
    cfg.attach_console_log_sink = false;
    core::Engine engine(cfg, nullptr, std::move(clock));

    app::StudioSession session(window, std::make_unique<rendering::SoftwareRenderDevice>());
    FE_REQUIRE_TRUE(engine.initialize(&session).hasValue());

    const auto* pyr_before = session.document().scene.findObjectByName("AccentPyramid");
    FE_REQUIRE_TRUE(pyr_before != nullptr);
    const math::Vec3 pos_before =
        session.document().scene.computeWorldMatrix(pyr_before->id).value().transformPoint(math::Vec3::zero());

    for (int i = 0; i < 5; ++i) {
        clock_ptr->advance(0.1);
        FE_REQUIRE_TRUE(engine.stepFrame().hasValue());
    }

    const math::Vec3 pos_after =
        session.document().scene.computeWorldMatrix(pyr_before->id).value().transformPoint(math::Vec3::zero());
    // RigPivot rotated around Y, so child AccentPyramid moved in world XZ space.
    FE_EXPECT_FALSE(pos_before.approxEqual(pos_after, 1e-3f));

    const auto tmp = platform::queryAppPaths("finaleffect3d_sessiontest").value().temp_directory / "saved.fe3d";
    std::error_code ec;
    std::filesystem::remove_all(tmp.parent_path(), ec);
    FE_REQUIRE_TRUE(session.saveProjectToFile(tmp, engine.fileSystem()).hasValue());
    FE_REQUIRE_TRUE(session.loadProjectFromFile(tmp, engine.fileSystem()).hasValue());

    clock_ptr->advance(0.016);
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());

    engine.shutdown();
    window.shutdown();
    std::filesystem::remove_all(tmp.parent_path(), ec);
}

FE_TEST(Rendering, Live3DMaterialPreviewAndInPlaceMeshBufferUpdates) {
    rendering::MaterialPreviewRenderer preview;
    FE_REQUIRE_TRUE(preview.initialize(80, 80).hasValue());
    FE_EXPECT_EQ(preview.width(), 80u);
    FE_EXPECT_EQ(preview.height(), 80u);
    FE_EXPECT_TRUE(preview.rawRgbaBytes() != nullptr);

    // 1. Dark matte dielectric vs bright emissive cyan on the shadow side of the 3D sphere
    scene::PbrMaterial matte{
        .name = "CharcoalMatte",
        .base_color = {0.10f, 0.10f, 0.12f},
        .metallic = 0.0f,
        .roughness = 0.92f,
    };
    FE_REQUIRE_TRUE(preview.renderMaterial(matte).hasValue());
    const auto shadow_matte = preview.pixelAt(28, 52);

    scene::PbrMaterial emissive = matte;
    emissive.emissive_color = {0.10f, 0.92f, 1.0f};
    emissive.emissive_strength = 3.0f;
    FE_REQUIRE_TRUE(preview.renderMaterial(emissive).hasValue());
    const auto shadow_emissive = preview.pixelAt(28, 52);
    FE_EXPECT_TRUE(shadow_emissive.b > shadow_matte.b + 80);

    // 2. Mirror-polished gold vs translucent glass
    scene::PbrMaterial gold{
        .name = "Gold",
        .base_color = {1.0f, 0.76f, 0.30f},
        .metallic = 1.0f,
        .roughness = 0.08f,
    };
    FE_REQUIRE_TRUE(preview.renderMaterial(gold).hasValue());
    const auto gold_center = preview.pixelAt(40, 40);

    scene::PbrMaterial clear_glass = gold;
    clear_glass.opacity = 0.15f;
    FE_REQUIRE_TRUE(preview.renderMaterial(clear_glass).hasValue());
    const auto glass_center = preview.pixelAt(40, 40);
    FE_EXPECT_TRUE(gold_center != glass_center);
    FE_EXPECT_TRUE(preview.renderGeneration() >= 4u);

    // 3. Verify in-place GPU buffer update (updateMesh) and overlay pool reuse without buffer leaks!
    auto sw_dev = std::make_unique<rendering::SoftwareRenderDevice>();
    auto* sw_ptr = sw_dev.get();
    rendering::Renderer renderer(std::move(sw_dev));
    FE_REQUIRE_TRUE(renderer.initialize(96, 96).hasValue());

    scene::Scene scene;
    const auto mesh_id = scene.registerEditableMesh("EditableCube", scene::EditableMesh::createCube(1.5f)).value();
    const auto obj_id = scene.createObject("Cube").value();
    FE_REQUIRE_TRUE(scene.assignMesh(obj_id, mesh_id).hasValue());

    const scene::MeshData gizmo_stub = scene::Primitives::createPyramid(0.3f, 0.5f, {1.0f, 0.2f, 0.2f});
    const rendering::ViewportOverlayMesh ov{.mesh_data = &gizmo_stub, .model_matrix = math::Mat4::identity()};

    FE_REQUIRE_TRUE(renderer.renderScene(scene, {}, obj_id, {&ov, 1}).hasValue());
    FE_EXPECT_EQ(sw_ptr->liveMeshCount(), std::size_t{2}); // 1 scene mesh + 1 reusable overlay slot
    FE_EXPECT_EQ(sw_ptr->bufferUpdateCount(), std::size_t{0});

    // Mutate EditableMesh across 3 frames -> updates existing GPU buffer in-place without allocating new handles!
    for (int i = 0; i < 3; ++i) {
        scene::EditableMesh edited = scene.findMesh(mesh_id)->editable_mesh.value();
        const std::uint32_t top = 4;
        FE_REQUIRE_TRUE(edited.translateFaces({&top, 1}, {0.0f, 0.1f, 0.0f}).hasValue());
        FE_REQUIRE_TRUE(scene.updateMeshEditable(mesh_id, std::move(edited)).hasValue());
        FE_REQUIRE_TRUE(renderer.renderScene(scene, {}, obj_id, {&ov, 1}).hasValue());
    }
    FE_EXPECT_EQ(sw_ptr->liveMeshCount(), std::size_t{2});
    // 3 scene mesh updates + 3 overlay slot updates = 6 in-place buffer updates, zero leaks!
    FE_EXPECT_EQ(sw_ptr->bufferUpdateCount(), std::size_t{6});

    renderer.shutdown();
    FE_EXPECT_EQ(sw_ptr->liveMeshCount(), std::size_t{0});
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
