// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "test_harness.hpp"

#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/platform/window.hpp"
#include "finaleffect/project/project.hpp"
#include "finaleffect/rendering/lighting_system.hpp"
#include "finaleffect/rendering/renderer.hpp"
#include "finaleffect/rendering/software_render_device.hpp"
#include "finaleffect/scene/environment.hpp"
#include "finaleffect/scene/light.hpp"
#include "finaleffect/scene/primitives.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace finaleffect;

struct LightingTestRig {
    platform::HeadlessWindow window;
    platform::LocalFileSystem fs;
    std::unique_ptr<core::Engine> engine;
    rendering::SoftwareRenderDevice* raw_device{nullptr};
    std::unique_ptr<app::StudioSession> session;
    std::unique_ptr<app::EditorController> editor;

    bool init(std::uint32_t w = 320, std::uint32_t h = 200) {
        if (!window.initialize({.title = "LightingEngineTests", .width = w, .height = h}).hasValue()) {
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

    bool renderViewportFrame() {
        core::FrameTimings timings{};
        timings.delta_seconds = 1.0 / 60.0;
        return session->onUpdate(*engine, timings).hasValue();
    }

    ~LightingTestRig() {
        if (engine != nullptr) {
            engine->shutdown();
        }
        window.shutdown();
    }
};

struct PatchLuma {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    [[nodiscard]] double luma() const noexcept { return 0.2126 * r + 0.7152 * g + 0.0722 * b; }
};

PatchLuma samplePatch(
    const rendering::SoftwareRenderDevice& dev,
    std::uint32_t cx,
    std::uint32_t cy,
    std::uint32_t half_size = 2) {
    PatchLuma sum{};
    std::uint32_t count = 0;
    const std::uint32_t w = dev.width();
    const std::uint32_t h = dev.height();
    const std::uint32_t x0 = (cx > half_size) ? (cx - half_size) : 0u;
    const std::uint32_t y0 = (cy > half_size) ? (cy - half_size) : 0u;
    const std::uint32_t x1 = std::min(w - 1u, cx + half_size);
    const std::uint32_t y1 = std::min(h - 1u, cy + half_size);
    for (std::uint32_t y = y0; y <= y1; ++y) {
        for (std::uint32_t x = x0; x <= x1; ++x) {
            const auto px = dev.pixelAt(x, y);
            sum.r += px.r;
            sum.g += px.g;
            sum.b += px.b;
            ++count;
        }
    }
    if (count > 0u) {
        sum.r /= static_cast<double>(count);
        sum.g /= static_cast<double>(count);
        sum.b /= static_cast<double>(count);
    }
    return sum;
}

std::vector<std::uint8_t> buildSyntheticRadianceHdr(int width, int height) {
    std::string header =
        "#?RADIANCE\n"
        "SOFTWARE=FinalEffect3D_Test\n"
        "FORMAT=32-bit_rle_rgbe\n\n"
        "-Y " +
        std::to_string(height) + " +X " + std::to_string(width) + "\n";
    std::vector<std::uint8_t> bytes(header.begin(), header.end());
    bytes.reserve(bytes.size() + static_cast<std::size_t>(width * height * 4));

    auto encodeRgbe = [](float r, float g, float b, std::uint8_t out[4]) {
        const float v = std::max(r, std::max(g, b));
        if (v < 1.0e-32f) {
            out[0] = out[1] = out[2] = out[3] = 0;
            return;
        }
        int e = 0;
        const float m = std::frexp(v, &e) * 256.0f / v;
        out[0] = static_cast<std::uint8_t>(std::clamp(r * m, 0.0f, 255.0f));
        out[1] = static_cast<std::uint8_t>(std::clamp(g * m, 0.0f, 255.0f));
        out[2] = static_cast<std::uint8_t>(std::clamp(b * m, 0.0f, 255.0f));
        out[3] = static_cast<std::uint8_t>(e + 128);
    };

    for (int y = 0; y < height; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
        for (int x = 0; x < width; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
            const float du = u - 0.25f;
            const float dv = v - 0.30f;
            const float spot = std::exp(-(du * du + dv * dv) * 90.0f);
            const float r = 0.06f + 16.0f * spot;
            const float g = 0.12f + 10.0f * spot;
            const float b = 0.28f + 3.0f * spot;
            std::uint8_t px[4]{};
            encodeRgbe(r, g, b, px);
            bytes.insert(bytes.end(), px, px + 4);
        }
    }
    return bytes;
}

void setTransformPose(
    scene::Scene& sc,
    scene::ObjectId id,
    const math::Vec3& position,
    const math::Quat& rotation = math::Quat::identity(),
    const math::Vec3& scale = {1.0f, 1.0f, 1.0f}) {
    math::Transform tf{};
    tf.position = position;
    tf.rotation = rotation;
    tf.scale = scale;
    (void)sc.setObjectTransform(id, tf);
}

scene::ObjectId setupCameraLookingAt(
    scene::Scene& sc,
    const math::Vec3& cam_pos,
    const math::Vec3& target = {0.0f, 0.0f, 0.0f},
    float fov_deg = 45.0f) {
    const auto cam_id = sc.createObject("TestCamera").value();
    scene::CameraComponent cam{};
    cam.fov_y_radians = math::degToRad(fov_deg);
    cam.near_plane = 0.1f;
    cam.far_plane = 120.0f;
    (void)sc.assignCamera(cam_id, cam);
    (void)sc.setActiveCamera(cam_id);

    const auto forward = (target - cam_pos).normalized();
    const float pitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
    const float yaw = std::atan2(-forward.x, -forward.z);
    const auto rot =
        math::Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, yaw) *
        math::Quat::fromAxisAngle({1.0f, 0.0f, 0.0f}, pitch);
    setTransformPose(sc, cam_id, cam_pos, rot);
    return cam_id;
}

FE_TEST(LightingEngine, IndependentLightTypesAndUndoRedo) {
    LightingTestRig rig;
    FE_REQUIRE_TRUE(rig.init(320, 200));
    FE_REQUIRE_TRUE(rig.editor->createNewProject(app::ProjectTemplate::MinimalCube).hasValue());

    const auto p1 = rig.editor->addPresetObject(app::ScenePreset::PointLight).value();
    const auto p2 = rig.editor->addPresetObject(app::ScenePreset::PointLight).value();
    const auto sp1 = rig.editor->addPresetObject(app::ScenePreset::SpotLight).value();
    const auto ar1 = rig.editor->addPresetObject(app::ScenePreset::AreaLight).value();
    FE_REQUIRE_TRUE(p1.isValid() && p2.isValid() && sp1.isValid() && ar1.isValid());
    FE_REQUIRE_TRUE(p1 != p2);

    scene::PointLightComponent custom_pt{};
    custom_pt.color = {1.0f, 0.2f, 0.1f};
    custom_pt.intensity = 42.5f;
    custom_pt.range = 24.0f;
    custom_pt.falloff = scene::LightFalloffMode::SmoothQuadratic;
    custom_pt.shadow.enabled = true;
    custom_pt.shadow.quality = scene::ShadowQuality::High;
    FE_REQUIRE_TRUE(rig.editor->setPointLightComponent(p1, custom_pt, false).hasValue());

    auto& sc = rig.session->document().scene;
    FE_REQUIRE_TRUE(sc.findObject(p1)->point_light.has_value());
    FE_REQUIRE_TRUE(sc.findObject(p2)->point_light.has_value());
    FE_EXPECT_NEAR(sc.findObject(p1)->point_light->intensity, 42.5f, 1.0e-4f);
    FE_EXPECT_TRUE(std::fabs(sc.findObject(p2)->point_light->intensity - 42.5f) > 1.0f);
    FE_EXPECT_TRUE(sc.findObject(sp1)->lightType() == scene::LightType::Spot);
    FE_EXPECT_TRUE(sc.findObject(ar1)->lightType() == scene::LightType::Area);

    FE_REQUIRE_TRUE(rig.editor->undo().hasValue());
    FE_EXPECT_TRUE(std::fabs(sc.findObject(p1)->point_light->intensity - 42.5f) > 1.0f);
    FE_REQUIRE_TRUE(rig.editor->redo().hasValue());
    FE_EXPECT_NEAR(sc.findObject(p1)->point_light->intensity, 42.5f, 1.0e-4f);

    // Undoable type conversion on p2 -> AreaLight
    FE_REQUIRE_TRUE(rig.editor->convertObjectLightType(p2, scene::LightType::Area).hasValue());
    FE_EXPECT_TRUE(sc.findObject(p2)->lightType() == scene::LightType::Area);
    FE_REQUIRE_TRUE(rig.editor->undo().hasValue());
    FE_EXPECT_TRUE(sc.findObject(p2)->lightType() == scene::LightType::Point);
}

FE_TEST(LightingEngine, AllFourLightTypesPbrAndAreaDifferentiation) {
    auto sw_ptr = std::make_unique<rendering::SoftwareRenderDevice>();
    auto* sw = sw_ptr.get();
    rendering::Renderer renderer(std::move(sw_ptr));
    FE_REQUIRE_TRUE(renderer.initialize(240, 180).hasValue());

    scene::Scene sc("PBR Light Verification");
    scene::EnvironmentLighting env{};
    env.enabled = false;
    FE_REQUIRE_TRUE(sc.setEnvironment(env).hasValue());

    const auto plane_mesh = sc.registerMesh(
        "Floor",
        scene::Primitives::createCheckerPlane(5.0f, 8, {0.85f, 0.85f, 0.85f}, {0.85f, 0.85f, 0.85f})).value();
    scene::PbrMaterial mat{};
    mat.name = "SatinReceiver";
    mat.base_color = {0.85f, 0.85f, 0.88f};
    mat.metallic = 0.35f;
    mat.roughness = 0.28f;
    const auto mat_id = sc.createMaterial(mat).value();

    const auto floor_id = sc.createObject("ReceiverPlane").value();
    FE_REQUIRE_TRUE(sc.assignMesh(floor_id, plane_mesh).hasValue());
    FE_REQUIRE_TRUE(sc.assignMaterial(floor_id, mat_id).hasValue());

    setupCameraLookingAt(sc, {0.0f, 4.2f, 4.2f}, {0.0f, 0.0f, 0.0f}, 45.0f);

    const auto light_id = sc.createObject("TestEmitter").value();
    setTransformPose(
        sc,
        light_id,
        {0.0f, 1.5f, 0.0f},
        math::Quat::fromAxisAngle({1.0f, 0.0f, 0.0f}, -1.5707963f));

    rendering::FrameClearDesc black_clear{};
    black_clear.clear_color = {0.0f, 0.0f, 0.0f, 1.0f};

    // 1. Point light distance attenuation & range cutoff
    scene::PointLightComponent pt{};
    pt.intensity = 18.0f;
    pt.range = 8.0f;
    pt.shadow.enabled = false;
    FE_REQUIRE_TRUE(sc.assignPointLight(light_id, pt).hasValue());
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    const auto center_pt = samplePatch(*sw, 120, 90, 2);
    const auto edge_pt = samplePatch(*sw, 35, 90, 2);
    FE_EXPECT_TRUE(center_pt.luma() > edge_pt.luma() + 18.0);

    pt.range = 1.1f; // Shorter than height above plane (1.5m) -> dark
    FE_REQUIRE_TRUE(sc.assignPointLight(light_id, pt).hasValue());
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    FE_EXPECT_TRUE(samplePatch(*sw, 120, 90, 2).luma() < 2.0);

    // 2. Spot light inner/outer cone cutoff
    scene::SpotLightComponent sp{};
    sp.intensity = 28.0f;
    sp.range = 10.0f;
    sp.inner_cone_radians = math::degToRad(8.0f);
    sp.outer_cone_radians = math::degToRad(16.0f);
    sp.shadow.enabled = false;
    FE_REQUIRE_TRUE(sc.assignSpotLight(light_id, sp).hasValue());
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    FE_EXPECT_TRUE(samplePatch(*sw, 120, 90, 2).luma() > 60.0);
    FE_EXPECT_TRUE(samplePatch(*sw, 55, 90, 2).luma() < 2.0);

    // 3. Area light vs Point light differentiation at identical position, color, and intensity
    pt.intensity = 16.0f;
    pt.range = 12.0f;
    FE_REQUIRE_TRUE(sc.assignPointLight(light_id, pt).hasValue());
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    const auto pt_wide = samplePatch(*sw, 62, 90, 3);
    const auto pt_center2 = samplePatch(*sw, 120, 90, 3);

    scene::AreaLightComponent area{};
    area.shape = scene::AreaLightShape::Rectangle;
    area.intensity = 16.0f;
    area.range = 12.0f;
    area.width = 3.8f;
    area.height = 1.4f;
    area.sample_quality = 3;
    area.shadow.enabled = false;
    FE_REQUIRE_TRUE(sc.assignAreaLight(light_id, area).hasValue());
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    const auto area_wide = samplePatch(*sw, 62, 90, 3);
    const auto area_center = samplePatch(*sw, 120, 90, 3);

    const double pt_spread_ratio = pt_wide.luma() / std::max(1.0, pt_center2.luma());
    const double area_spread_ratio = area_wide.luma() / std::max(1.0, area_center.luma());
    FE_EXPECT_TRUE(area_spread_ratio > pt_spread_ratio + 0.08);

    // Disk emitter vs wide Rectangle emitter produces measurably distinct lateral illumination
    area.shape = scene::AreaLightShape::Disk;
    area.width = 1.1f;
    FE_REQUIRE_TRUE(sc.assignAreaLight(light_id, area).hasValue());
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    const auto disk_wide = samplePatch(*sw, 62, 90, 3);
    FE_EXPECT_TRUE(std::fabs(area_wide.luma() - disk_wide.luma()) > 5.0);
}

FE_TEST(LightingEngine, ShadowsDirtyTileCachingAndDeterministicLimits) {
    auto sw_ptr = std::make_unique<rendering::SoftwareRenderDevice>();
    auto* sw = sw_ptr.get();
    rendering::Renderer renderer(std::move(sw_ptr));
    FE_REQUIRE_TRUE(renderer.initialize(240, 180).hasValue());

    scene::Scene sc("Shadow Verification");
    scene::EnvironmentLighting env{};
    env.enabled = false;
    FE_REQUIRE_TRUE(sc.setEnvironment(env).hasValue());

    const auto plane_mesh = sc.registerMesh(
        "Floor",
        scene::Primitives::createCheckerPlane(5.0f, 4, {0.9f, 0.9f, 0.9f}, {0.9f, 0.9f, 0.9f})).value();
    const auto cube_mesh = sc.registerMesh("Cube", scene::Primitives::createCube(1.0f)).value();
    scene::PbrMaterial white{};
    white.name = "MatteWhite";
    white.base_color = {0.9f, 0.9f, 0.9f};
    white.roughness = 0.75f;
    const auto mat_id = sc.createMaterial(white).value();

    const auto floor_id = sc.createObject("Receiver").value();
    FE_REQUIRE_TRUE(sc.assignMesh(floor_id, plane_mesh).hasValue());
    FE_REQUIRE_TRUE(sc.assignMaterial(floor_id, mat_id).hasValue());

    const auto caster_id = sc.createObject("Occluder").value();
    FE_REQUIRE_TRUE(sc.assignMesh(caster_id, cube_mesh).hasValue());
    FE_REQUIRE_TRUE(sc.assignMaterial(caster_id, mat_id).hasValue());
    setTransformPose(sc, caster_id, {0.0f, 1.4f, 0.0f}, math::Quat::identity(), {1.2f, 0.25f, 1.2f});

    setupCameraLookingAt(sc, {0.0f, 0.90f, 3.8f}, {0.0f, 0.0f, 0.0f}, 45.0f);

    const auto light_id = sc.createObject("ShadowLight").value();
    setTransformPose(
        sc,
        light_id,
        {0.0f, 3.6f, 0.0f},
        math::Quat::fromAxisAngle({1.0f, 0.0f, 0.0f}, -1.5707963f));

    rendering::FrameClearDesc black_clear{};
    black_clear.clear_color = {0.0f, 0.0f, 0.0f, 1.0f};

    scene::SpotLightComponent sp{};
    sp.intensity = 35.0f;
    sp.range = 12.0f;
    sp.inner_cone_radians = math::degToRad(32.0f);
    sp.outer_cone_radians = math::degToRad(48.0f);
    sp.shadow.enabled = false;
    FE_REQUIRE_TRUE(sc.assignSpotLight(light_id, sp).hasValue());
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    const auto unshadowed_center = samplePatch(*sw, 120, 90, 2);
    FE_EXPECT_TRUE(unshadowed_center.luma() > 50.0);

    sp.shadow.enabled = true;
    sp.shadow.quality = scene::ShadowQuality::High;
    sp.shadow.filter = scene::ShadowFilterMode::Pcf3x3;
    FE_REQUIRE_TRUE(sc.assignSpotLight(light_id, sp).hasValue());
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    const auto shadowed_center = samplePatch(*sw, 120, 90, 2);
    FE_EXPECT_TRUE(shadowed_center.luma() < unshadowed_center.luma() * 0.25);

    // Dirty-tile shadow atlas caching: second identical render reuses cached tile (0 rendered)
    const auto diag1 = renderer.lastLightingDiagnostics();
    FE_EXPECT_TRUE(diag1.shadow_tiles_rendered_last_frame >= 1u);
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    const auto diag2 = renderer.lastLightingDiagnostics();
    FE_EXPECT_EQ(diag2.shadow_tiles_rendered_last_frame, 0u);
    FE_EXPECT_TRUE(diag2.shadow_tiles_cached_last_frame >= 1u);

    // Moving caster invalidates cached shadow tile and restores lit floor center
    setTransformPose(sc, caster_id, {3.2f, 1.4f, 0.0f}, math::Quat::identity(), {1.2f, 0.25f, 1.2f});
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    FE_EXPECT_TRUE(renderer.lastLightingDiagnostics().shadow_tiles_rendered_last_frame >= 1u);
    const auto revealed_center = samplePatch(*sw, 120, 90, 2);
    FE_EXPECT_TRUE(revealed_center.luma() > shadowed_center.luma() * 3.0);

    // Point 6-face omnidirectional shadow + Area PCF5x5 soft shadow
    setTransformPose(sc, caster_id, {0.0f, 1.4f, 0.0f}, math::Quat::identity(), {1.2f, 0.25f, 1.2f});
    scene::PointLightComponent pt{};
    pt.intensity = 35.0f;
    pt.range = 12.0f;
    pt.shadow.enabled = true;
    pt.shadow.quality = scene::ShadowQuality::Medium;
    FE_REQUIRE_TRUE(sc.assignPointLight(light_id, pt).hasValue());
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    FE_EXPECT_EQ(renderer.lastLightingDiagnostics().shadow_tiles_rendered_last_frame, 6u);
    FE_EXPECT_TRUE(samplePatch(*sw, 120, 90, 2).luma() < 15.0);

    scene::AreaLightComponent ar{};
    ar.intensity = 35.0f;
    ar.range = 12.0f;
    ar.width = 1.6f;
    ar.height = 1.6f;
    ar.shadow.enabled = true;
    ar.shadow.filter = scene::ShadowFilterMode::Pcf5x5;
    ar.shadow.softness = 2.0f;
    FE_REQUIRE_TRUE(sc.assignAreaLight(light_id, ar).hasValue());
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    FE_EXPECT_TRUE(samplePatch(*sw, 120, 90, 2).luma() < 20.0);

    // Deterministic limits: 20 shadow-casting lights -> clamped to 12 active & <=6 shadow casters with warning
    for (int i = 0; i < 19; ++i) {
        const auto extra = sc.createObject("ExtraLight_" + std::to_string(i)).value();
        scene::SpotLightComponent extra_sp{};
        extra_sp.intensity = 2.0f;
        extra_sp.shadow.enabled = true;
        FE_REQUIRE_TRUE(sc.assignSpotLight(extra, extra_sp).hasValue());
    }
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    const auto lim_diag = renderer.lastLightingDiagnostics();
    FE_EXPECT_EQ(
        lim_diag.active_realtime_lights,
        static_cast<std::uint32_t>(rendering::LightingLimits::kMaxRealtimeLights));
    FE_EXPECT_TRUE(lim_diag.overflow_lights_folded >= 8u);
    FE_EXPECT_TRUE(
        lim_diag.active_shadow_lights <=
        static_cast<std::uint32_t>(
            rendering::LightingLimits::kMaxShadowCasters2D +
            rendering::LightingLimits::kMaxPointShadowCasters));
    FE_EXPECT_TRUE(lim_diag.shadow_limit_clamped_lights > 0u);
    FE_EXPECT_TRUE(!lim_diag.warning_message.empty());
}

FE_TEST(LightingEngine, HdriLoadingRotationAndCorruptFileResilience) {
    LightingTestRig rig;
    FE_REQUIRE_TRUE(rig.init(240, 180));
    FE_REQUIRE_TRUE(rig.editor->createNewProject(app::ProjectTemplate::ShaderBall).hasValue());

    auto& sc = rig.session->document().scene;
    for (const auto& obj : sc.objects()) {
        if (obj.directional_light.has_value()) {
            auto dir = *obj.directional_light;
            dir.enabled = false;
            FE_REQUIRE_TRUE(rig.editor->setDirectionalLightComponent(obj.id, dir, false).hasValue());
        }
    }

    const auto hdr_bytes = buildSyntheticRadianceHdr(32, 16);
    FE_REQUIRE_TRUE(
        rig.editor->loadEnvironmentHdriFromMemory(hdr_bytes, "SyntheticSun.hdr").hasValue());
    FE_REQUIRE_TRUE(sc.environment().hasHdri());
    FE_REQUIRE_TRUE(rig.session->renderFinalScene().hasValue());
    const auto rot0_left = samplePatch(*rig.raw_device, 95, 88, 3);
    const auto rot0_right = samplePatch(*rig.raw_device, 145, 88, 3);
    FE_EXPECT_TRUE(rot0_left.luma() + rot0_right.luma() > 25.0);

    // Rotate HDRI by 180 degrees -> reflection asymmetry flips across the glossy shader ball
    auto env = sc.environment();
    env.rotation_radians = math::degToRad(180.0f);
    FE_REQUIRE_TRUE(rig.editor->setEnvironmentSettings(env, false).hasValue());
    FE_REQUIRE_TRUE(rig.session->renderFinalScene().hasValue());
    const auto rot180_left = samplePatch(*rig.raw_device, 95, 88, 3);
    const auto rot180_right = samplePatch(*rig.raw_device, 145, 88, 3);
    FE_EXPECT_TRUE(std::fabs((rot0_left.luma() - rot0_right.luma()) -
                             (rot180_left.luma() - rot180_right.luma())) > 4.0);

    // Corrupt HDRI bytes fail cleanly while preserving the previous valid HDRI asset & settings
    const std::string bad_hdr = "NOT_A_VALID_RADIANCE_HDR_PAYLOAD";
    const std::span<const std::uint8_t> bad_span(
        reinterpret_cast<const std::uint8_t*>(bad_hdr.data()), bad_hdr.size());
    FE_EXPECT_TRUE(!rig.editor->loadEnvironmentHdriFromMemory(bad_span, "Corrupt.hdr").hasValue());
    FE_REQUIRE_TRUE(sc.environment().hasHdri());
    FE_EXPECT_EQ(sc.environment().hdri_asset->name, std::string("SyntheticSun.hdr"));
    FE_EXPECT_NEAR(sc.environment().rotation_radians, math::degToRad(180.0f), 1.0e-3f);

    // Clear HDRI -> releases IBL environment map cleanly
    FE_REQUIRE_TRUE(rig.editor->clearEnvironmentHdri().hasValue());
    FE_EXPECT_TRUE(!sc.environment().hasHdri());
}

FE_TEST(LightingEngine, LightLinkingModesHierarchyAndDeletionCleanup) {
    auto sw_ptr = std::make_unique<rendering::SoftwareRenderDevice>();
    auto* sw = sw_ptr.get();
    rendering::Renderer renderer(std::move(sw_ptr));
    FE_REQUIRE_TRUE(renderer.initialize(240, 160).hasValue());

    scene::Scene sc("Light Linking Test");
    scene::EnvironmentLighting env{};
    env.enabled = false;
    FE_REQUIRE_TRUE(sc.setEnvironment(env).hasValue());

    const auto sphere_mesh = sc.registerMesh(
        "Sphere", scene::Primitives::createUvSphere(0.7f, 16, 20)).value();
    scene::PbrMaterial mat{};
    mat.name = "Mat";
    mat.base_color = {0.9f, 0.9f, 0.9f};
    mat.roughness = 0.5f;
    const auto mat_id = sc.createMaterial(mat).value();

    const auto left_id = sc.createObject("LeftSphere").value();
    const auto right_id = sc.createObject("RightSphere").value();
    FE_REQUIRE_TRUE(sc.assignMesh(left_id, sphere_mesh).hasValue());
    FE_REQUIRE_TRUE(sc.assignMesh(right_id, sphere_mesh).hasValue());
    FE_REQUIRE_TRUE(sc.assignMaterial(left_id, mat_id).hasValue());
    FE_REQUIRE_TRUE(sc.assignMaterial(right_id, mat_id).hasValue());
    setTransformPose(sc, left_id, {-1.15f, 0.0f, 0.0f});
    setTransformPose(sc, right_id, {1.15f, 0.0f, 0.0f});

    setupCameraLookingAt(sc, {0.0f, 0.0f, 4.2f}, {0.0f, 0.0f, 0.0f}, 45.0f);

    const auto key_id = sc.createObject("LinkedKeyLight").value();
    setTransformPose(
        sc,
        key_id,
        {0.0f, 0.0f, 0.0f},
        math::Quat::fromAxisAngle({1.0f, 0.0f, 0.0f}, -0.35f));
    scene::DirectionalLightComponent dir{};
    dir.intensity = 4.0f;
    dir.shadow.enabled = false;
    FE_REQUIRE_TRUE(sc.assignDirectionalLight(key_id, dir).hasValue());

    rendering::FrameClearDesc black_clear{};
    black_clear.clear_color = {0.0f, 0.0f, 0.0f, 1.0f};

    // Default (All): both spheres lit
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    FE_EXPECT_TRUE(samplePatch(*sw, 74, 80, 3).luma() > 80.0);
    FE_EXPECT_TRUE(samplePatch(*sw, 166, 80, 3).luma() > 80.0);

    // IncludeOnly LeftSphere -> LeftSphere lit, RightSphere dark
    dir.linking.mode = scene::LightLinkMode::IncludeOnly;
    dir.linking.linked_objects = {left_id};
    FE_REQUIRE_TRUE(sc.assignDirectionalLight(key_id, dir).hasValue());
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    FE_EXPECT_TRUE(samplePatch(*sw, 74, 80, 3).luma() > 80.0);
    FE_EXPECT_TRUE(samplePatch(*sw, 166, 80, 3).luma() < 1.0);

    // Exclude LeftSphere -> LeftSphere dark, RightSphere lit
    dir.linking.mode = scene::LightLinkMode::ExcludeList;
    dir.linking.linked_objects = {left_id};
    FE_REQUIRE_TRUE(sc.assignDirectionalLight(key_id, dir).hasValue());
    FE_REQUIRE_TRUE(renderer.renderScene(sc, black_clear).hasValue());
    FE_EXPECT_TRUE(samplePatch(*sw, 74, 80, 3).luma() < 1.0);
    FE_EXPECT_TRUE(samplePatch(*sw, 166, 80, 3).luma() > 80.0);

    // Removing LeftSphere automatically scrubs left_id from key_id's linked_objects
    FE_REQUIRE_TRUE(sc.removeObject(left_id).hasValue());
    const auto* key_obj = sc.findObject(key_id);
    FE_REQUIRE_TRUE(key_obj != nullptr);
    const auto* key_linking = key_obj->lightLinking();
    FE_REQUIRE_TRUE(key_linking != nullptr);
    FE_EXPECT_TRUE(!key_linking->containsDirect(left_id));
}

FE_TEST(LightingEngine, SerializationV13RoundTripAndHelpersExclusion) {
    LightingTestRig rig;
    FE_REQUIRE_TRUE(rig.init(320, 200));

    // Starter scene regression: 5 objects, zero ground grid object in Scene
    auto& sc = rig.session->document().scene;
    FE_EXPECT_EQ(sc.objectCount(), std::size_t{5});

    const auto pt_id = rig.editor->addPresetObject(app::ScenePreset::PointLight).value();
    const auto sp_id = rig.editor->addPresetObject(app::ScenePreset::SpotLight).value();
    const auto ar_id = rig.editor->addPresetObject(app::ScenePreset::AreaLight).value();
    FE_REQUIRE_TRUE(pt_id.isValid() && sp_id.isValid() && ar_id.isValid());

    const auto hdr_bytes = buildSyntheticRadianceHdr(16, 8);
    FE_REQUIRE_TRUE(rig.editor->loadEnvironmentHdriFromMemory(hdr_bytes, "RoundTrip.hdr").hasValue());

    const auto* cube_obj = sc.findObjectByName("HeroCube");
    FE_REQUIRE_TRUE(cube_obj != nullptr);
    scene::LightLinkingSettings link{};
    link.mode = scene::LightLinkMode::ExcludeList;
    link.linked_objects = {cube_obj->id};
    FE_REQUIRE_TRUE(rig.editor->setLightLinking(sp_id, link).hasValue());

    // Editor viewport frame includes 3D helpers & grid; final render strictly excludes them
    FE_REQUIRE_TRUE(rig.renderViewportFrame());
    const auto editor_pixels = rig.raw_device->colorBuffer();
    FE_REQUIRE_TRUE(rig.session->renderFinalScene().hasValue());
    const auto final_before = rig.raw_device->colorBuffer();
    bool differs_from_editor = false;
    for (std::size_t i = 0; i < editor_pixels.size(); ++i) {
        if (editor_pixels[i].r != final_before[i].r ||
            editor_pixels[i].g != final_before[i].g ||
            editor_pixels[i].b != final_before[i].b) {
            differs_from_editor = true;
            break;
        }
    }
    FE_EXPECT_TRUE(differs_from_editor);

    const auto inline_json_res = rig.editor->exportInlineProjectJson();
    FE_REQUIRE_TRUE(inline_json_res.hasValue());
    const std::string& inline_json = inline_json_res.value();
    FE_EXPECT_TRUE(inline_json.find("\"minor\": 3") != std::string::npos);

    FE_REQUIRE_TRUE(rig.editor->createNewProject(app::ProjectTemplate::MinimalCube).hasValue());
    FE_REQUIRE_TRUE(rig.editor->importInlineProjectJson(inline_json).hasValue());
    FE_EXPECT_TRUE(rig.session->document().scene.environment().hasHdri());
    FE_EXPECT_EQ(rig.session->document().scene.objectCount(), std::size_t{8});

    FE_REQUIRE_TRUE(rig.session->renderFinalScene().hasValue());
    const auto final_after = rig.raw_device->colorBuffer();
    int max_channel_diff = 0;
    for (std::size_t i = 0; i < final_before.size(); ++i) {
        max_channel_diff = std::max(
            max_channel_diff,
            std::max({
                std::abs(static_cast<int>(final_before[i].r) - static_cast<int>(final_after[i].r)),
                std::abs(static_cast<int>(final_before[i].g) - static_cast<int>(final_after[i].g)),
                std::abs(static_cast<int>(final_before[i].b) - static_cast<int>(final_after[i].b)),
            }));
    }
    FE_EXPECT_TRUE(max_channel_diff <= 2);

    // Missing external HDRI file resilience test
    const auto tmp_dir = std::filesystem::temp_directory_path() / "fe3d_stage54_missing_hdri";
    std::filesystem::create_directories(tmp_dir);
    project::ProjectDocument doc_ext = rig.session->document();
    auto ext_env = doc_ext.scene.environment();
    ext_env.hdri_asset.reset();
    ext_env.source_path = "nonexistent_external_sky.hdr";
    FE_REQUIRE_TRUE(doc_ext.scene.setEnvironment(ext_env).hasValue());
    const auto missing_fe3d = tmp_dir / "missing_hdri.fe3d";
    FE_REQUIRE_TRUE(project::ProjectSerializer::saveToFile(doc_ext, missing_fe3d, rig.fs).hasValue());

    const auto loaded_res = project::ProjectSerializer::loadFromFile(missing_fe3d, rig.fs);
    FE_REQUIRE_TRUE(loaded_res.hasValue());
    FE_EXPECT_TRUE(!loaded_res.value().load_warnings.empty());
    FE_EXPECT_TRUE(!loaded_res.value().scene.environment().hasHdri());
    std::filesystem::remove_all(tmp_dir);
}

FE_TEST(LightingEngine, EightScenePerformanceAndMemoryBenchmarkSuite) {
    struct BenchRow {
        const char* name;
        double avg_ms;
        std::uint32_t lights;
        std::uint32_t shadows;
        std::size_t shadow_bytes;
        std::size_t ibl_bytes;
    };
    std::vector<BenchRow> rows;

    auto runBench = [&](const char* label, auto&& builder) {
        LightingTestRig rig;
        FE_REQUIRE_TRUE(rig.init(320, 180));
        builder(*rig.session, *rig.editor);

        FE_REQUIRE_TRUE(rig.session->renderFinalScene().hasValue());
        const auto& warmup_d = rig.session->renderer().lastLightingDiagnostics();
        const auto mem_after_warmup = warmup_d.shadow_gpu_bytes + warmup_d.environment_gpu_bytes;

        constexpr int kFrames = 4;
        const auto t0 = std::chrono::steady_clock::now();
        for (int f = 0; f < kFrames; ++f) {
            FE_REQUIRE_TRUE(rig.session->renderFinalScene().hasValue());
        }
        const auto t1 = std::chrono::steady_clock::now();
        const double avg_ms =
            std::chrono::duration<double, std::milli>(t1 - t0).count() / kFrames;
        const auto& d = rig.session->renderer().lastLightingDiagnostics();
        const auto mem_after_loop = d.shadow_gpu_bytes + d.environment_gpu_bytes;
        FE_EXPECT_EQ(mem_after_loop, mem_after_warmup);

        rows.push_back(BenchRow{
            label,
            avg_ms,
            d.active_realtime_lights,
            d.active_shadow_lights,
            d.shadow_gpu_bytes,
            d.environment_gpu_bytes,
        });
    };

    runBench("1. Single Directional + High Shadow", [](auto&, auto& ed) {
        (void)ed.addPresetObject(app::ScenePreset::Plane);
    });
    runBench("2. Point + Spot + Dual Shadows", [](auto& s, auto& ed) {
        (void)ed.addPresetObject(app::ScenePreset::Plane);
        const auto p = ed.addPresetObject(app::ScenePreset::PointLight).value();
        (void)ed.addPresetObject(app::ScenePreset::SpotLight);
        auto pt = *s.document().scene.findObject(p)->point_light;
        pt.shadow.enabled = true;
        (void)ed.setPointLightComponent(p, pt, false);
    });
    runBench("3. Multi-Light Mixed (6 Lights)", [](auto&, auto& ed) {
        (void)ed.addPresetObject(app::ScenePreset::PointLight);
        (void)ed.addPresetObject(app::ScenePreset::PointLight);
        (void)ed.addPresetObject(app::ScenePreset::SpotLight);
        (void)ed.addPresetObject(app::ScenePreset::AreaLight);
        (void)ed.addPresetObject(app::ScenePreset::DirectionalLight);
    });
    runBench("4. Area Lights (Rect+Disk) + PBR", [](auto& s, auto& ed) {
        (void)ed.addPresetObject(app::ScenePreset::Plane);
        (void)ed.addPresetObject(app::ScenePreset::AreaLight);
        const auto a2 = ed.addPresetObject(app::ScenePreset::AreaLight).value();
        auto disk = *s.document().scene.findObject(a2)->area_light;
        disk.shape = scene::AreaLightShape::Disk;
        disk.shadow.enabled = true;
        (void)ed.setAreaLightComponent(a2, disk, false);
    });
    runBench("5. HDRI / IBL + Direct Lights", [&](auto&, auto& ed) {
        FE_REQUIRE_TRUE(ed.loadBuiltInHdriPreset("studio_softbox").hasValue());
        (void)ed.addPresetObject(app::ScenePreset::SpotLight);
    });
    runBench("6. Shadow-Heavy Multi-Caster", [](auto& s, auto& ed) {
        (void)ed.addPresetObject(app::ScenePreset::Plane);
        (void)ed.addPresetObject(app::ScenePreset::Cube);
        (void)ed.addPresetObject(app::ScenePreset::Pyramid);
        (void)ed.addPresetObject(app::ScenePreset::SpotLight);
        const auto ar_id = ed.addPresetObject(app::ScenePreset::AreaLight).value();
        auto ar = *s.document().scene.findObject(ar_id)->area_light;
        ar.shadow.enabled = true;
        (void)ed.setAreaLightComponent(ar_id, ar, false);
    });
    runBench("7. Per-Light Object Light Linking", [](auto& s, auto& ed) {
        const auto sp = ed.addPresetObject(app::ScenePreset::SpotLight).value();
        scene::LightLinkingSettings ll{};
        ll.mode = scene::LightLinkMode::IncludeOnly;
        for (const auto& o : s.document().scene.objects()) {
            if (o.mesh_id.has_value()) {
                ll.linked_objects.push_back(o.id);
                break;
            }
        }
        (void)ed.setLightLinking(sp, ll);
    });
    runBench("8. Starter Scene Baseline", [](auto&, auto&) {});

    std::printf("\n=== Stage 5.4 CPU Reference Benchmark Suite (320x180, steady-state) ===\n");
    for (const auto& r : rows) {
        std::printf(
            "  %-36s | %6.2f ms | lights=%u shadows=%u | shadow=%zu KB ibl=%zu KB\n",
            r.name,
            r.avg_ms,
            r.lights,
            r.shadows,
            r.shadow_bytes / 1024u,
            r.ibl_bytes / 1024u);
    }
}

FE_TEST(LightingEngine, CompleteJsonActionAndUiStateIntegration) {
    LightingTestRig rig;
    FE_REQUIRE_TRUE(rig.init(320, 200));

    auto& sc = rig.session->document().scene;

    // 1. Create all 4 light types via exact JSON payloads dispatched by web/editor.js
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"add_preset","preset":"light"})").hasValue());
    const auto dir_id = rig.session->selectedObject();
    FE_REQUIRE_TRUE(dir_id.isValid() && sc.findObject(dir_id)->lightType() == scene::LightType::Directional);

    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"add_preset","preset":"point_light"})").hasValue());
    const auto pt_id = rig.session->selectedObject();
    FE_REQUIRE_TRUE(pt_id.isValid() && sc.findObject(pt_id)->lightType() == scene::LightType::Point);

    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"add_preset","preset":"spot_light"})").hasValue());
    const auto sp_id = rig.session->selectedObject();
    FE_REQUIRE_TRUE(sp_id.isValid() && sc.findObject(sp_id)->lightType() == scene::LightType::Spot);

    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"add_preset","preset":"area_light"})").hasValue());
    const auto ar_id = rig.session->selectedObject();
    FE_REQUIRE_TRUE(ar_id.isValid() && sc.findObject(ar_id)->lightType() == scene::LightType::Area);

    // 2. Mutate Point range/falloff + shadows via exact nested shadow JSON
    const std::string pt_json =
        "{\"action\":\"set_light\",\"id\":" + std::to_string(pt_id.value) +
        ",\"type\":\"point\",\"enabled\":true,\"color\":[0.25,0.65,1.0],\"intensity\":24.5"
        ",\"range\":19.5,\"falloff\":\"smooth_quadratic\",\"attenuation\":1.8,\"source_radius\":0.15"
        ",\"shadow\":{\"enabled\":true,\"quality\":\"high\",\"filter\":\"pcf5x5\",\"strength\":0.85"
        ",\"bias\":0.003,\"normal_bias\":0.025,\"softness\":1.6},\"coalesce\":false}";
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(pt_json).hasValue());
    FE_EXPECT_NEAR(sc.findObject(pt_id)->point_light->intensity, 24.5f, 1.0e-4f);
    FE_EXPECT_NEAR(sc.findObject(pt_id)->point_light->range, 19.5f, 1.0e-4f);
    FE_EXPECT_TRUE(sc.findObject(pt_id)->point_light->falloff == scene::LightFalloffMode::SmoothQuadratic);
    FE_EXPECT_TRUE(sc.findObject(pt_id)->point_light->shadow.enabled);
    FE_EXPECT_TRUE(sc.findObject(pt_id)->point_light->shadow.quality == scene::ShadowQuality::High);
    FE_EXPECT_TRUE(sc.findObject(pt_id)->point_light->shadow.filter == scene::ShadowFilterMode::Pcf5x5);

    // 3. Mutate Spot cone angles via JSON
    const std::string sp_json =
        "{\"action\":\"set_light\",\"id\":" + std::to_string(sp_id.value) +
        ",\"type\":\"spot\",\"enabled\":true,\"color\":[1.0,0.8,0.4],\"intensity\":31.0"
        ",\"range\":22.0,\"inner_cone_deg\":12.0,\"outer_cone_deg\":29.0,\"cone_falloff\":1.4"
        ",\"shadow\":{\"enabled\":true,\"quality\":\"medium\",\"filter\":\"pcf3x3\"}}";
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(sp_json).hasValue());
    FE_EXPECT_NEAR(math::radToDeg(sc.findObject(sp_id)->spot_light->inner_cone_radians), 12.0f, 1.0e-2f);
    FE_EXPECT_NEAR(math::radToDeg(sc.findObject(sp_id)->spot_light->outer_cone_radians), 29.0f, 1.0e-2f);

    // 4. Mutate Area shape (Rectangle -> Disk) & dimensions + change_light_type via JSON
    const std::string ar_json =
        "{\"action\":\"set_light\",\"id\":" + std::to_string(ar_id.value) +
        ",\"type\":\"area\",\"enabled\":true,\"color\":[0.9,0.95,1.0],\"intensity\":18.0"
        ",\"shape\":\"disk\",\"width\":2.4,\"height\":1.2,\"range\":15.0,\"two_sided\":true,\"sample_quality\":3}";
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(ar_json).hasValue());
    FE_EXPECT_TRUE(sc.findObject(ar_id)->area_light->shape == scene::AreaLightShape::Disk);
    FE_EXPECT_NEAR(sc.findObject(ar_id)->area_light->width, 2.4f, 1.0e-4f);
    FE_EXPECT_TRUE(sc.findObject(ar_id)->area_light->two_sided);
    FE_EXPECT_EQ(sc.findObject(ar_id)->area_light->sample_quality, 3u);

    const std::string conv_json =
        "{\"action\":\"change_light_type\",\"id\":" + std::to_string(dir_id.value) +
        ",\"light_type\":\"area\"}";
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(conv_json).hasValue());
    FE_EXPECT_TRUE(sc.findObject(dir_id)->lightType() == scene::LightType::Area);

    // 5. Light Linking JSON actions (set_light_linking + toggle_light_link_target)
    const auto* hero_cube = sc.findObjectByName("HeroCube");
    FE_REQUIRE_TRUE(hero_cube != nullptr);
    const std::string link_json =
        "{\"action\":\"set_light_linking\",\"id\":" + std::to_string(sp_id.value) +
        ",\"mode\":\"exclude\",\"include_descendants\":true,\"object_ids\":[]}";
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(link_json).hasValue());
    const std::string toggle_json =
        "{\"action\":\"toggle_light_link_target\",\"id\":" + std::to_string(sp_id.value) +
        ",\"target_id\":" + std::to_string(hero_cube->id.value) + ",\"linked\":true}";
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(toggle_json).hasValue());
    FE_EXPECT_TRUE(!sc.doesLightAffectObject(*sc.findObject(sp_id)->lightLinking(), hero_cube->id));

    // 6. HDRI preset + environment intensity/rotation + clear_hdri + Undo/Redo via JSON
    FE_REQUIRE_TRUE(
        rig.editor->executeActionJson(R"({"action":"load_hdri_preset","preset":"warm_sunset"})").hasValue());
    FE_EXPECT_TRUE(sc.environment().hasHdri());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(
        R"({"action":"set_environment","enabled":true,"intensity":2.25,"rotation_deg":95,"diffuse_ibl_scale":1.2,"specular_ibl_scale":1.5,"coalesce":false})")
                        .hasValue());
    FE_EXPECT_NEAR(sc.environment().intensity, 2.25f, 1.0e-4f);
    FE_EXPECT_NEAR(math::radToDeg(sc.environment().rotation_radians), 95.0f, 1.0e-2f);

    // Verify serialized state JSON exposes exact keys expected by web/editor.js
    FE_REQUIRE_TRUE(rig.editor->selectObject(sp_id).hasValue());
    FE_REQUIRE_TRUE(rig.renderViewportFrame());
    const auto state_parsed = project::parseJson(rig.editor->serializeStateJson());
    FE_REQUIRE_TRUE(state_parsed.hasValue());
    const auto& st_root = state_parsed.value();
    const auto* env_j = st_root.findKey("environment");
    const auto* insp_j = st_root.findKey("inspector");
    const auto* diag_j = st_root.findKey("lighting_diagnostics");
    FE_REQUIRE_TRUE(env_j != nullptr && insp_j != nullptr && diag_j != nullptr);
    FE_EXPECT_TRUE(env_j->findKey("has_hdri")->asBool());
    FE_EXPECT_EQ(env_j->findKey("hdri_name")->asString(), std::string("Golden_Sunset_Horizon.hdr"));
    FE_EXPECT_EQ(
        insp_j->findKey("light")->findKey("linking")->findKey("mode")->asString(),
        std::string("exclude"));
    FE_EXPECT_EQ(
        insp_j->findKey("light")->findKey("linking")->findKey("object_ids")->asArray().size(),
        std::size_t{1});
    FE_EXPECT_TRUE(diag_j->findKey("active_lights")->asNumber() >= 5.0);

    // Clear HDRI, then Undo restores Golden_Sunset_Horizon.hdr, Redo clears it again!
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"clear_hdri"})").hasValue());
    FE_EXPECT_TRUE(!sc.environment().hasHdri());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"undo"})").hasValue());
    FE_EXPECT_TRUE(sc.environment().hasHdri());
    FE_REQUIRE_TRUE(rig.editor->executeActionJson(R"({"action":"redo"})").hasValue());
    FE_EXPECT_TRUE(!sc.environment().hasHdri());
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
