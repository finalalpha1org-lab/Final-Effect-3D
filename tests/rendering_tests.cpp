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

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
