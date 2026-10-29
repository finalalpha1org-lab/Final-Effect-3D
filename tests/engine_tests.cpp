// SPDX-License-Identifier: GPL-3.0-only
#include <fstream>
#include <memory>
#include <string>

#include "TestFramework.hpp"
#include "finaleffect/engine/Application.hpp"
#include "finaleffect/engine/DemoScene.hpp"
#include "finaleffect/engine/Engine.hpp"
#include "finaleffect/platform/Window.hpp"
#include "finaleffect/rendering/ImageIO.hpp"
#include "finaleffect/rendering/SoftwareRenderer.hpp"

namespace {
using fe::engine::Engine;
using fe::engine::EngineConfig;
using fe::platform::ManualClock;
using fe::platform::NullWindow;
using fe::rendering::RendererInitInfo;
using fe::rendering::SoftwareRenderer;

fe::core::Status attachSoftwareRenderer(Engine& engine, std::uint32_t width, std::uint32_t height) {
    auto renderer = std::make_unique<SoftwareRenderer>();
    RendererInitInfo info;
    info.width = width;
    info.height = height;
    const fe::core::Status initialized = renderer->initialize(info);
    if (!initialized.ok()) {
        return initialized;
    }
    return engine.setRenderer(std::move(renderer));
}

std::string temporaryDirectory(Engine& engine) {
    return fe::platform::joinPath(engine.paths().tempDirectory, "stage1-tests");
}
}  // namespace

FE3D_TEST(engine, initialize_and_shutdown_are_idempotent) {
    Engine engine;
    EngineConfig config;
    config.logLevel = fe::core::LogLevel::Warning;
    config.attachConsoleLogSink = false;

    FE3D_CHECK(engine.initialize(config).ok());
    FE3D_CHECK(engine.isInitialized());
    FE3D_CHECK(!engine.initialize(config).ok());  // second initialize is an error
    FE3D_CHECK(!engine.paths().workingDirectory.empty());
    FE3D_CHECK(!engine.paths().userDataDirectory.empty());
    FE3D_CHECK(!engine.paths().logDirectory.empty());
    FE3D_CHECK_EQ(engine.scene().objectCount(), std::size_t(0));
    FE3D_CHECK_EQ(engine.meshLibrary().size(), std::size_t(3));
    FE3D_CHECK(engine.renderer() == nullptr);

    FE3D_CHECK(engine.shutdown().ok());
    FE3D_CHECK(engine.shutdown().ok());
    FE3D_CHECK(!engine.isInitialized());
    FE3D_CHECK(!engine.renderFrame(engine.defaultCamera(), fe::math::Vec3()).ok());
}

FE3D_TEST(engine, demo_scene_saves_and_loads) {
    Engine writer;
    EngineConfig config;
    config.logLevel = fe::core::LogLevel::Warning;
    config.attachConsoleLogSink = false;
    FE3D_REQUIRE(writer.initialize(config).ok());
    FE3D_REQUIRE(fe::platform::StdFileSystem{}.createDirectories(temporaryDirectory(writer)).ok());

    FE3D_REQUIRE(fe::engine::buildDemoScene(writer.scene()).ok());
    FE3D_CHECK_EQ(writer.scene().objectCount(), std::size_t(4));
    const fe::scene::ObjectId satellite = writer.scene().objects()[3].id;
    FE3D_CHECK(writer.scene().find(satellite)->parent.isValid());

    const std::string path = fe::platform::joinPath(temporaryDirectory(writer), "demo.fe3d");
    FE3D_REQUIRE(writer.saveProject(path).ok());

    Engine reader;
    FE3D_REQUIRE(reader.initialize(config).ok());
    FE3D_REQUIRE(reader.loadProject(path).ok());
    FE3D_CHECK_EQ(reader.scene().objectCount(), std::size_t(4));
    FE3D_CHECK(reader.scene().find(satellite) != nullptr);
    FE3D_CHECK(reader.scene().find(satellite)->mesh == std::string("cube"));
    FE3D_CHECK(reader.scene().worldTransform(satellite).position.isFinite());
    FE3D_CHECK(reader.defaultCamera().position.isFinite());

    // A malformed file must not destroy the scene that is already loaded.
    const std::string brokenPath = fe::platform::joinPath(temporaryDirectory(writer), "broken.fe3d");
    FE3D_REQUIRE(fe::platform::StdFileSystem{}.writeTextFile(brokenPath, "{ nonsense").ok());
    const fe::core::Status brokenLoad = reader.loadProject(brokenPath);
    FE3D_CHECK(!brokenLoad.ok());
    FE3D_CHECK(brokenLoad.error().code() == fe::core::ErrorCode::ParseError);
    FE3D_CHECK_EQ(reader.scene().objectCount(), std::size_t(4));

    const fe::core::Status missingLoad = reader.loadProject(fe::platform::joinPath(temporaryDirectory(writer), "nope.fe3d"));
    FE3D_CHECK(!missingLoad.ok());

    (void)fe::platform::StdFileSystem{}.removeFile(path);
    (void)fe::platform::StdFileSystem{}.removeFile(brokenPath);
    FE3D_REQUIRE(writer.shutdown().ok());
    FE3D_REQUIRE(reader.shutdown().ok());
}

FE3D_TEST(engine, headless_application_renders_frames_and_captures) {
    Engine engine;
    EngineConfig config;
    config.logLevel = fe::core::LogLevel::Warning;
    config.attachConsoleLogSink = false;
    FE3D_REQUIRE(engine.initialize(config).ok());
    FE3D_REQUIRE(fe::engine::buildDemoScene(engine.scene()).ok());

    FE3D_REQUIRE(fe::platform::StdFileSystem{}.createDirectories(temporaryDirectory(engine)).ok());
    FE3D_REQUIRE(attachSoftwareRenderer(engine, 96, 64).ok());
    FE3D_REQUIRE(engine.renderer() != nullptr);
    FE3D_CHECK_EQ(engine.renderer()->capabilities().backendName, std::string("cpu-rasterizer"));

    NullWindow window(3);
    fe::platform::WindowDesc desc;
    desc.width = 96;
    desc.height = 64;
    desc.createGraphicsContext = false;
    FE3D_REQUIRE(window.create(desc).ok());
    FE3D_CHECK_EQ(std::string(window.backendName()), std::string("null"));

    ManualClock clock;
    fe::engine::Application application(engine, window, clock);

    const std::string screenshotPath = fe::platform::joinPath(temporaryDirectory(engine), "frame.ppm");
    application.setScreenshotPath(screenshotPath);

    const fe::core::Status status = application.run(3);
    FE3D_CHECK_MSG(status.ok(), status.ok() ? "" : status.error().toString());
    FE3D_CHECK_EQ(application.frameCount(), std::uint64_t(3));
    FE3D_CHECK(application.capturedPixelCount() == 96U * 64U);
    FE3D_CHECK(application.camera().position.isFinite());

    std::ifstream captured(screenshotPath, std::ios::binary);
    FE3D_REQUIRE(captured.is_open());
    std::string header;
    std::getline(captured, header);
    FE3D_CHECK_EQ(header, std::string("P6"));
    captured.close();
    (void)fe::platform::StdFileSystem{}.removeFile(screenshotPath);

    FE3D_CHECK(engine.shutdown().ok());
}

FE3D_TEST(engine, application_requires_an_initialized_engine_with_a_renderer) {
    Engine engine;
    EngineConfig config;
    config.logLevel = fe::core::LogLevel::Warning;
    config.attachConsoleLogSink = false;
    NullWindow window(1);
    FE3D_REQUIRE(window.create(fe::platform::WindowDesc{}).ok());
    ManualClock clock;
    fe::engine::Application application(engine, window, clock);

    const fe::core::Status withoutEngine = application.run(1);
    FE3D_CHECK(!withoutEngine.ok());
    FE3D_CHECK(withoutEngine.error().code() == fe::core::ErrorCode::NotInitialized);

    FE3D_REQUIRE(engine.initialize(config).ok());
    const fe::core::Status withoutRenderer = application.run(1);
    FE3D_CHECK(!withoutRenderer.ok());
    FE3D_CHECK(withoutRenderer.error().code() == fe::core::ErrorCode::NotInitialized);
    FE3D_REQUIRE(engine.shutdown().ok());
}

FE3D_TEST(engine, null_window_lifecycle_and_input) {
    NullWindow window(2);
    FE3D_CHECK(!window.isOpen());
    FE3D_CHECK(window.consumeResizedFlag() == false);

    fe::platform::WindowDesc desc;
    desc.width = 320;
    desc.height = 200;
    FE3D_REQUIRE(window.create(desc).ok());
    FE3D_CHECK(window.isOpen());
    FE3D_CHECK_EQ(window.width(), std::uint32_t(320));
    FE3D_CHECK(!window.shouldClose());
    FE3D_CHECK(window.nativeHandle() == nullptr);

    window.setKeyDown(fe::platform::Key::Escape, true);
    FE3D_CHECK(window.isKeyDown(fe::platform::Key::Escape));
    FE3D_CHECK(!window.isKeyDown(fe::platform::Key::W));
    FE3D_CHECK_EQ(std::string(fe::platform::toString(fe::platform::Key::W)), std::string("W"));

    window.simulateResize(640, 480);
    FE3D_CHECK(window.consumeResizedFlag());
    FE3D_CHECK(!window.consumeResizedFlag());

    window.pollEvents();
    FE3D_CHECK(!window.shouldClose());
    window.pollEvents();
    FE3D_CHECK(window.shouldClose());

    window.destroy();
    FE3D_CHECK(!window.isOpen());

    fe::platform::WindowDesc invalid;
    invalid.width = 0;
    invalid.height = 0;
    FE3D_CHECK(!window.create(invalid).ok());
}
