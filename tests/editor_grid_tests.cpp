// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "test_harness.hpp"

#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/platform/window.hpp"
#include "finaleffect/project/json.hpp"
#include "finaleffect/rendering/editor_grid.hpp"
#include "finaleffect/rendering/null_render_device.hpp"
#include "finaleffect/rendering/software_render_device.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace finaleffect;

struct GridTestRig {
    platform::HeadlessWindow window;
    platform::LocalFileSystem fs;
    std::unique_ptr<core::Engine> engine;
    rendering::SoftwareRenderDevice* raw_device{nullptr};
    std::unique_ptr<app::StudioSession> session;
    std::unique_ptr<app::EditorController> editor;

    bool init(std::uint32_t w = 360, std::uint32_t h = 240) {
        if (!window.initialize({.title = "EditorGridTests", .width = w, .height = h}).hasValue()) {
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

    ~GridTestRig() {
        if (engine != nullptr) {
            engine->shutdown();
        }
        window.shutdown();
    }
};

bool isNonClearPixel(rendering::Rgba8 px, rendering::Rgba8 bg) {
    const int dr = std::abs(static_cast<int>(px.r) - static_cast<int>(bg.r));
    const int dg = std::abs(static_cast<int>(px.g) - static_cast<int>(bg.g));
    const int db = std::abs(static_cast<int>(px.b) - static_cast<int>(bg.b));
    return (dr + dg + db) >= 12;
}

std::size_t countNonBackgroundPixels(const rendering::SoftwareRenderDevice& dev) {
    if (dev.colorBuffer().empty()) {
        return 0;
    }
    const auto bg = dev.pixelAt(0, 0); // Top-left sky corner is pure clear color.
    std::size_t count = 0;
    for (const auto& px : dev.colorBuffer()) {
        if (isNonClearPixel(px, bg)) {
            ++count;
        }
    }
    return count;
}

// Covers Scenarios 1, 2, 3, & 8:
// - Default viewport displays procedural editor grid (major/minor lines + X/Z colored axes + origin).
// - Grid does NOT appear in Scene hierarchy, Scene Outliner JSON, or GPU scene mesh buffers.
// - Grid cannot be selected, transformed, or deleted as a SceneObject.
// - Grid NEVER appears in final scene rendering or serialized project geometry.
FE_TEST(EditorGrid, DefaultGridVisibleExcludedFromOutlinerUnselectableAndOmittedInFinalRender) {
    GridTestRig rig;
    FE_REQUIRE_TRUE(rig.init(360, 240));

    // Starter Showcase has 5 real objects (RigPivot, HeroCube, AccentPyramid, MainCamera, SunLight)
    // and 2 editable meshes (HeroCubeMesh, AccentPyramidMesh) — ZERO fake GroundPlane object!
    const auto& starter_scene = rig.session->document().scene;
    FE_EXPECT_EQ(starter_scene.objectCount(), std::size_t{5});
    FE_EXPECT_EQ(starter_scene.meshCount(), std::size_t{2});
    FE_EXPECT_TRUE(starter_scene.findObjectByName("GroundPlane") == nullptr);

    const auto state_json = project::parseJson(rig.editor->serializeStateJson()).value();
    FE_EXPECT_TRUE(state_json.findKey("viewport")->findKey("editor_grid_visible")->asBool());
    for (const auto& item : state_json.findKey("objects")->asArray()) {
        const std::string nm = item.findKey("name")->asString();
        FE_EXPECT_TRUE(nm != "GroundPlane" && nm != "Grid" && nm != "EditorGrid");
    }

    // Switch to EmptyStage (only MainCamera + SunLight, 0 meshes) and hide scene helpers
    // so the ONLY thing visible in the viewport is the procedural XZ ground grid!
    FE_REQUIRE_TRUE(rig.editor->createNewProject(app::ProjectTemplate::EmptyStage, "GridOnlyCheck").hasValue());
    rig.editor->setEditorHelpersVisible(false);
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), std::size_t{2});
    FE_EXPECT_EQ(rig.session->document().scene.meshCount(), std::size_t{0});

    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    FE_EXPECT_EQ(rig.raw_device->liveMeshCount(), std::size_t{0}); // Zero scene/overlay mesh buffers uploaded!

    const auto bg = rig.raw_device->pixelAt(2, 2);
    std::size_t grid_pixels = 0;
    std::size_t red_x_axis_pixels = 0;
    std::size_t blue_z_axis_pixels = 0;
    for (const auto& px : rig.raw_device->colorBuffer()) {
        if (isNonClearPixel(px, bg)) {
            ++grid_pixels;
            if (static_cast<int>(px.r) > static_cast<int>(px.b) + 35 &&
                static_cast<int>(px.r) > static_cast<int>(px.g) + 30) {
                ++red_x_axis_pixels;
            }
            if (static_cast<int>(px.b) > static_cast<int>(px.r) + 35 &&
                static_cast<int>(px.b) > static_cast<int>(px.g) + 12) {
                ++blue_z_axis_pixels;
            }
        }
    }
    FE_EXPECT_TRUE(grid_pixels > 400);
    FE_EXPECT_TRUE(red_x_axis_pixels > 15);
    FE_EXPECT_TRUE(blue_z_axis_pixels > 15);

    // Project (0, 0, 0) world origin and points on X/Z axes; clicking them must NOT select any object!
    const auto origin_sp = rig.session->projectWorldPointToScreen({0.0f, 0.0f, 0.0f});
    const auto x_axis_sp = rig.session->projectWorldPointToScreen({1.5f, 0.0f, 0.0f});
    const auto z_axis_sp = rig.session->projectWorldPointToScreen({0.0f, 0.0f, 1.5f});
    FE_REQUIRE_TRUE(origin_sp.has_value() && x_axis_sp.has_value() && z_axis_sp.has_value());
    FE_EXPECT_TRUE(isNonClearPixel(
        rig.raw_device->pixelAt(
            static_cast<std::uint32_t>(origin_sp->x),
            static_cast<std::uint32_t>(origin_sp->y)),
        bg));

    FE_EXPECT_FALSE(rig.editor->pickViewportPixel(origin_sp->x, origin_sp->y).has_value());
    FE_EXPECT_FALSE(rig.editor->pickViewportPixel(x_axis_sp->x, x_axis_sp->y).has_value());
    FE_EXPECT_FALSE(rig.editor->pickViewportPixel(z_axis_sp->x, z_axis_sp->y).has_value());
    FE_EXPECT_FALSE(rig.session->selectedObject().isValid());
    FE_EXPECT_FALSE(rig.session->computeActiveGizmoFrame().has_value());

    // Pressing Delete over unselected grid does nothing and adds zero Undo commands.
    rig.window.pushInputEvent(platform::KeyEvent{
        .key = platform::KeyCode::Delete,
        .action = platform::KeyAction::Press,
    });
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), std::size_t{2});
    FE_EXPECT_EQ(rig.session->commandStack().undoCount(), std::size_t{0});

    // Final scene render (StudioSession::renderFinalScene) MUST exclude the grid completely!
    FE_REQUIRE_TRUE(rig.session->renderFinalScene().hasValue());
    FE_EXPECT_EQ(countNonBackgroundPixels(*rig.raw_device), std::size_t{0});

    // Project serialization MUST NOT serialize any grid geometry or grid object.
    const auto exported_json = rig.editor->exportInlineProjectJson().value();
    const auto parsed_doc = project::parseJson(exported_json).value();
    FE_EXPECT_EQ(parsed_doc.findKey("scene")->findKey("meshes")->asArray().size(), std::size_t{0});
    FE_EXPECT_EQ(parsed_doc.findKey("scene")->findKey("objects")->asArray().size(), std::size_t{2});
}

// Covers Scenarios 4 & 5 + Depth Occlusion:
// - Creating a real Plane object ('Add -> Mesh -> Grid Plane') remains a full editable mesh object,
//   survives save/load round-trips, and legacy projects with Plane objects load without loss.
// - Hiding the grid hides ONLY the procedural grid without hiding/deleting scene objects or materials.
// - Foreground scene objects properly occlude the procedural grid and are NEVER obscured by grid lines.
FE_TEST(EditorGrid, UserPlaneMeshPreservationToggleSafetyAndForegroundDepthOcclusion) {
    GridTestRig rig;
    FE_REQUIRE_TRUE(rig.init(320, 220));

    // Create a real user Plane object in Starter Showcase and verify it is a full EditableMesh!
    const auto plane_res = rig.editor->addPresetObject(app::ScenePreset::Plane);
    FE_REQUIRE_TRUE(plane_res.hasValue());
    const scene::ObjectId plane_id = plane_res.value();
    const auto* plane_obj = rig.session->document().scene.findObject(plane_id);
    FE_REQUIRE_TRUE(plane_obj != nullptr && plane_obj->mesh_id.has_value());
    FE_EXPECT_EQ(plane_obj->name, std::string("Plane"));

    const auto* plane_mesh = rig.session->document().scene.findMesh(plane_obj->mesh_id.value());
    FE_REQUIRE_TRUE(plane_mesh != nullptr && plane_mesh->editable_mesh.has_value());
    FE_EXPECT_EQ(plane_mesh->name, std::string("StudioPlaneMesh"));
    FE_EXPECT_EQ(plane_mesh->editable_mesh->faceCount(), std::size_t{16});

    // Extrude a face on the user Plane object to prove mesh editing works cleanly on Plane objects!
    FE_REQUIRE_TRUE(rig.editor->setEditorMode(scene::EditMode::Face).hasValue());
    FE_REQUIRE_TRUE(rig.editor->selectMeshElement(scene::EditMode::Face, 5, false).hasValue());
    FE_REQUIRE_TRUE(rig.editor->extrudeSelectedFaces(0.45f).hasValue());
    FE_EXPECT_TRUE(
        rig.session->document().scene.findMesh(plane_obj->mesh_id.value())->editable_mesh->faceCount() > 16);
    FE_REQUIRE_TRUE(rig.editor->setEditorMode(scene::EditMode::Object).hasValue());

    // Save & reload inline .fe3d and verify the user Plane object and its edited topology persist intact!
    const auto saved_with_plane = rig.editor->exportInlineProjectJson().value();
    FE_REQUIRE_TRUE(rig.editor->createNewProject(app::ProjectTemplate::EmptyStage).hasValue());
    FE_REQUIRE_TRUE(rig.editor->importInlineProjectJson(saved_with_plane).hasValue());
    const auto* reloaded_plane = rig.session->document().scene.findObjectByName("Plane");
    FE_REQUIRE_TRUE(reloaded_plane != nullptr && reloaded_plane->mesh_id.has_value());
    FE_EXPECT_TRUE(
        rig.session->document().scene.findMesh(reloaded_plane->mesh_id.value())->editable_mesh->faceCount() > 16);

    // Depth occlusion & grid visibility toggle safety test:
    // Create MinimalCube (MainCamera + SunLight + Cube at (0, 0.65, 0), bottom at Y = 0).
    FE_REQUIRE_TRUE(rig.editor->createNewProject(app::ProjectTemplate::MinimalCube, "DepthCheck").hasValue());
    rig.editor->setEditorHelpersVisible(false);
    rig.session->setSelectedObject({});
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), std::size_t{3});
    FE_EXPECT_EQ(rig.session->document().scene.meshCount(), std::size_t{1});

    const auto* cube = rig.session->document().scene.findObjectByName("Cube");
    FE_REQUIRE_TRUE(cube != nullptr);
    const auto cube_center_sp = rig.session->projectWorldPointToScreen({0.0f, 0.65f, 0.0f});
    const auto ground_side_sp = rig.session->projectWorldPointToScreen({2.0f, 0.0f, 0.0f});
    FE_REQUIRE_TRUE(cube_center_sp.has_value() && ground_side_sp.has_value());

    // Render with procedural grid VISIBLE:
    rig.editor->setEditorGridVisible(true);
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    const auto cube_px_grid_on = rig.raw_device->pixelAt(
        static_cast<std::uint32_t>(cube_center_sp->x),
        static_cast<std::uint32_t>(cube_center_sp->y));
    const auto x_axis_px_grid_on = rig.raw_device->pixelAt(
        static_cast<std::uint32_t>(ground_side_sp->x),
        static_cast<std::uint32_t>(ground_side_sp->y));
    const auto bg = rig.raw_device->pixelAt(2, 2);
    FE_EXPECT_TRUE(isNonClearPixel(x_axis_px_grid_on, bg));

    // Toggle procedural grid OFF via JSON action:
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"toggle_editor_grid"})").hasValue());
    FE_EXPECT_FALSE(rig.session->isEditorGridVisible());
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), std::size_t{3});
    FE_EXPECT_TRUE(rig.session->document().scene.findObject(cube->id)->visible);

    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    const auto cube_px_grid_off = rig.raw_device->pixelAt(
        static_cast<std::uint32_t>(cube_center_sp->x),
        static_cast<std::uint32_t>(cube_center_sp->y));
    const auto x_axis_px_grid_off = rig.raw_device->pixelAt(
        static_cast<std::uint32_t>(ground_side_sp->x),
        static_cast<std::uint32_t>(ground_side_sp->y));

    // Foreground Cube pixel is 100% IDENTICAL whether grid is ON or OFF (grid never draws in front of Cube!),
    // while uncovered ground X-axis pixel disappears cleanly when grid is OFF!
    FE_EXPECT_EQ(cube_px_grid_on.r, cube_px_grid_off.r);
    FE_EXPECT_EQ(cube_px_grid_on.g, cube_px_grid_off.g);
    FE_EXPECT_EQ(cube_px_grid_on.b, cube_px_grid_off.b);
    FE_EXPECT_FALSE(isNonClearPixel(x_axis_px_grid_off, bg));
}

// Covers Scenarios 6, 7, & 9:
// - Zoom-adaptive multi-scale LOD C0 continuity & double-precision camera-relative phases at 100,000+ units.
// - Perspective & Orthographic (3/4, Top +Y, and Front +Z edge-on horizon) views all work cleanly.
// - Transform Gizmos and Camera/DirectionalLight helpers continue to pick & drag cleanly with the grid active.
FE_TEST(EditorGrid, MultiScaleLodLargeCoordinatePrecisionOrthoViewsAndGizmoHelperCoexistence) {
    // 1. Direct C0 continuity & large-distance precision checks on buildEditorGridCommand / evaluateEditorGridAtNdc:
    scene::CameraComponent persp_cam{};
    persp_cam.projection = scene::ProjectionType::Perspective;
    persp_cam.fov_y_radians = math::degToRad(48.0f);
    persp_cam.near_plane = 0.1f;
    persp_cam.far_plane = 500.0f;

    // Far world coordinates (camera at X = 125000.25, Z = -340000.50, looking down at Y = 0):
    const math::Vec3 far_eye{125000.25f, 4.0f, -340000.50f};
    const math::Vec3 far_target{125000.25f, 0.0f, -340003.50f};
    const math::Mat4 far_view = math::Mat4::lookAtRH(far_eye, far_target, math::Vec3::unitY());
    const math::Mat4 far_proj = persp_cam.computeProjectionMatrix(
        16.0f / 9.0f, math::ClipSpaceDepth::NegativeOneToOne);

    const auto far_cmd = rendering::buildEditorGridCommand(
        far_view,
        far_proj,
        far_eye,
        125000.25,
        -340000.50,
        math::ClipSpaceDepth::NegativeOneToOne,
        rendering::ViewportGridOptions{.enabled = true, .opacity = 1.0f, .focus_distance = 5.0f});
    FE_REQUIRE_TRUE(far_cmd.has_value());
    // Wrapped phase stays bounded within [-step, +step] despite 340,000-unit camera offset!
    FE_EXPECT_TRUE(std::fabs(far_cmd->phase_x.y) <= far_cmd->step_minor + 1e-4f);
    FE_EXPECT_TRUE(std::fabs(far_cmd->phase_z.y) <= far_cmd->step_minor + 1e-4f);

    // Continuity across decade transition boundary (focus_distance just below vs just above decade step):
    const auto cmd_before_decade = rendering::buildEditorGridCommand(
        far_view, far_proj, far_eye, 125000.25, -340000.50, math::ClipSpaceDepth::NegativeOneToOne,
        rendering::ViewportGridOptions{.enabled = true, .opacity = 1.0f, .focus_distance = 3.099f});
    const auto cmd_after_decade = rendering::buildEditorGridCommand(
        far_view, far_proj, far_eye, 125000.25, -340000.50, math::ClipSpaceDepth::NegativeOneToOne,
        rendering::ViewportGridOptions{.enabled = true, .opacity = 1.0f, .focus_distance = 3.105f});
    FE_REQUIRE_TRUE(cmd_before_decade.has_value() && cmd_after_decade.has_value());
    const auto s_before = rendering::evaluateEditorGridAtNdc(0.0f, 0.0f, 2.0f / 640.0f, 2.0f / 360.0f, *cmd_before_decade);
    const auto s_after = rendering::evaluateEditorGridAtNdc(0.0f, 0.0f, 2.0f / 640.0f, 2.0f / 360.0f, *cmd_after_decade);
    FE_EXPECT_NEAR(s_before.alpha, s_after.alpha, 0.03f);

    // 2. End-to-end Perspective, Orthographic 3/4, Top (+Y) Ortho, and Front (+Z) Ortho views in StudioSession:
    GridTestRig rig;
    FE_REQUIRE_TRUE(rig.init(320, 220));
    FE_REQUIRE_TRUE(rig.editor->createNewProject(app::ProjectTemplate::EmptyStage, "OrthoGridCheck").hasValue());
    rig.editor->setEditorHelpersVisible(false);

    // Zoom in vs zoom out changes grid framebuffer coverage smoothly as spacing adapts:
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    const auto zoom_default_buf = rig.raw_device->colorBuffer();
    FE_REQUIRE_TRUE(rig.session->zoomViewportCamera(4.0f).hasValue());
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    FE_EXPECT_TRUE(rig.raw_device->colorBuffer() != zoom_default_buf);
    FE_EXPECT_TRUE(countNonBackgroundPixels(*rig.raw_device) > 250);

    // Switch to Orthographic 3/4 view:
    rig.editor->setViewportProjection(scene::ProjectionType::Orthographic);
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    FE_EXPECT_TRUE(countNonBackgroundPixels(*rig.raw_device) > 300);

    // Snap to Top (+Y) Orthographic view -> grid fills viewport with X & Z axes:
    rig.editor->setViewportViewPreset(app::ViewportViewPreset::TopPosY, false);
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    FE_EXPECT_TRUE(countNonBackgroundPixels(*rig.raw_device) > 400);

    // Snap to Front (+Z) Orthographic view (edge-on to Y = 0) -> renders crisp Y = 0 ground horizon line!
    rig.editor->setViewportViewPreset(app::ViewportViewPreset::FrontPosZ, false);
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    FE_EXPECT_TRUE(countNonBackgroundPixels(*rig.raw_device) >= 320);

    // 3. Verify Camera & DirectionalLight helpers + Transform Gizmos still work seamlessly with grid enabled:
    FE_REQUIRE_TRUE(rig.editor->newStarterProject().hasValue());
    rig.editor->setEditorHelpersVisible(true);
    rig.editor->setEditorGridVisible(true);
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());

    const auto* sun = rig.session->document().scene.findObjectByName("SunLight");
    FE_REQUIRE_TRUE(sun != nullptr);
    const auto sun_wm = rig.session->document().scene.computeWorldMatrix(sun->id).value();
    const auto sun_sp = rig.session->projectWorldPointToScreen(sun_wm.transformPoint(math::Vec3::zero()));
    FE_REQUIRE_TRUE(sun_sp.has_value());
    const auto picked_sun = rig.editor->pickViewportPixel(sun_sp->x, sun_sp->y);
    FE_REQUIRE_TRUE(picked_sun.has_value());
    FE_EXPECT_EQ(picked_sun->object_id, sun->id);

    // Select HeroCube and drag its +X Translate gizmo handle while procedural grid is active:
    const auto* hero = rig.session->document().scene.findObjectByName("HeroCube");
    FE_REQUIRE_TRUE(hero != nullptr);
    const float hero_x_before = hero->local_transform.position.x;
    rig.session->setSelectedObject(hero->id);
    const auto gframe = rig.session->computeActiveGizmoFrame();
    FE_REQUIRE_TRUE(gframe.has_value());
    const auto handle_sp = rig.session->projectWorldPointToScreen(
        gframe->origin + gframe->axes[0] * (gframe->scale * 0.65f));
    FE_REQUIRE_TRUE(handle_sp.has_value());

    rig.window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left,
        .action = platform::KeyAction::Press,
        .x = handle_sp->x,
        .y = handle_sp->y,
    });
    rig.window.pushInputEvent(platform::CursorMoveEvent{
        .x = handle_sp->x + 36.0,
        .y = handle_sp->y,
    });
    rig.window.pushInputEvent(platform::MouseButtonEvent{
        .button = platform::MouseButton::Left,
        .action = platform::KeyAction::Release,
        .x = handle_sp->x + 36.0,
        .y = handle_sp->y,
    });
    FE_REQUIRE_TRUE(rig.engine->stepFrame().hasValue());
    FE_EXPECT_TRUE(
        std::fabs(rig.session->document().scene.findObject(hero->id)->local_transform.position.x - hero_x_before) > 0.05f);
    FE_REQUIRE_TRUE(rig.editor->undo().hasValue());
    FE_EXPECT_NEAR(
        rig.session->document().scene.findObject(hero->id)->local_transform.position.x, hero_x_before, 1e-4f);

    // Verify NullRenderDevice tracks drawEditorGrid passes only when grid_options != nullptr && visible.
    auto null_dev = std::make_unique<rendering::NullRenderDevice>();
    auto* null_ptr = null_dev.get();
    rendering::Renderer null_renderer(std::move(null_dev));
    FE_REQUIRE_TRUE(null_renderer.initialize(640, 360).hasValue());
    FE_REQUIRE_TRUE(null_renderer.renderScene(rig.session->document().scene).hasValue());
    FE_EXPECT_EQ(null_ptr->totalGridPasses(), std::uint64_t{0});
    const rendering::ViewportGridOptions g_vis{.enabled = true, .focus_distance = 6.5f};
    FE_REQUIRE_TRUE(
        null_renderer.renderScene(rig.session->document().scene, {}, {}, {}, nullptr, &g_vis).hasValue());
    FE_EXPECT_EQ(null_ptr->totalGridPasses(), std::uint64_t{1});
    null_renderer.shutdown();
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
