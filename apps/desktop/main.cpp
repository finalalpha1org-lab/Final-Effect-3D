// Final Effect 3D - desktop host application.
// SPDX-License-Identifier: GPL-3.0-only
//
// Windowed mode (default) opens a GLFW window and renders with the OpenGL 3.3
// backend. Headless mode (--headless) creates no native window and renders with
// the CPU reference renderer, which is what CI uses.
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "finaleffect/core/Version.hpp"
#include "finaleffect/engine/Application.hpp"
#include "finaleffect/engine/DemoScene.hpp"
#include "finaleffect/engine/Engine.hpp"
#include "finaleffect/platform/Window.hpp"
#include "finaleffect/platform/glfw/GlfwWindow.hpp"
#include "finaleffect/rendering/SoftwareRenderer.hpp"
#include "finaleffect/rendering/gl/OpenGLRenderer.hpp"

namespace {

struct DesktopOptions {
    bool headless = false;
    std::uint64_t frames = 0;
    std::string screenshotPath;
    std::string scenePath;
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    bool verbose = false;
};

void printUsage() {
    std::cout << "Final Effect 3D desktop host " << fe::core::fullVersionString() << "\n\n"
              << "usage: fe3d_desktop [options]\n\n"
              << "options:\n"
              << "  --headless              no window, CPU reference renderer (CI friendly)\n"
              << "  --frames <n>            stop after n frames\n"
              << "  --screenshot <file.ppm> capture the last frame\n"
              << "  --scene <file.fe3d>     load a project file instead of the sample scene\n"
              << "  --width <pixels>        window/framebuffer width (default 1280)\n"
              << "  --height <pixels>       window/framebuffer height (default 720)\n"
              << "  --verbose               debug logging\n"
              << "  --help                  print this text\n\n"
              << "controls: ESC quit, arrow keys orbit and zoom, R reframe the scene\n";
}

bool parseArguments(int argc, char** argv, DesktopOptions& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            return false;
        }
        if (argument == "--headless") {
            options.headless = true;
            continue;
        }
        if (argument == "--verbose") {
            options.verbose = true;
            continue;
        }
        if (argument == "--frames" || argument == "--screenshot" || argument == "--scene" ||
            argument == "--width" || argument == "--height") {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << argument << "\n";
                return false;
            }
            const std::string value = argv[++i];
            if (argument == "--frames") {
                options.frames = static_cast<std::uint64_t>(std::strtoull(value.c_str(), nullptr, 10));
            } else if (argument == "--screenshot") {
                options.screenshotPath = value;
            } else if (argument == "--scene") {
                options.scenePath = value;
            } else {
                const int parsed = std::atoi(value.c_str());
                if (parsed <= 0 || parsed > 16384) {
                    std::cerr << "invalid value for " << argument << ": " << value << "\n";
                    return false;
                }
                if (argument == "--width") {
                    options.width = static_cast<std::uint32_t>(parsed);
                } else {
                    options.height = static_cast<std::uint32_t>(parsed);
                }
            }
            continue;
        }
        std::cerr << "unknown option " << argument << "\n";
        return false;
    }
    return true;
}

/// Camera interaction: orbit around the scene centre, zoom, reframe, quit.
class DesktopApplication final : public fe::engine::Application {
public:
    DesktopApplication(fe::engine::Engine& engine, fe::platform::Window& window, fe::platform::Clock& clock)
        : fe::engine::Application(engine, window, clock), engine_(engine) {}

protected:
    fe::core::Status onStartup() override {
        deriveOrbitFromCamera();
        return fe::core::successStatus();
    }

    fe::core::Status onFrameUpdate(const fe::engine::FrameContext& context) override {
        using fe::platform::Key;
        if (context.window.isKeyDown(Key::Escape)) {
            context.window.requestClose();
            return fe::core::successStatus();
        }
        if (context.window.isKeyDown(Key::R)) {
            camera() = engine_.defaultCamera();
            deriveOrbitFromCamera();
        }

        const auto step = static_cast<fe::math::Scalar>(context.deltaSeconds);
        if (context.window.isKeyDown(Key::Left)) {
            yaw_ -= 1.6F * step;
        }
        if (context.window.isKeyDown(Key::Right)) {
            yaw_ += 1.6F * step;
        }
        if (context.window.isKeyDown(Key::Up)) {
            distance_ *= 1.0F - fe::math::clamp(1.6F * step, 0.0F, 0.5F);
        }
        if (context.window.isKeyDown(Key::Down)) {
            distance_ *= 1.0F + fe::math::clamp(1.6F * step, 0.0F, 0.5F);
        }
        distance_ = fe::math::clamp(distance_, 0.2F, 10000.0F);

        const fe::math::Vec3 direction(
            static_cast<fe::math::Scalar>(std::cos(pitch_) * std::sin(yaw_)),
            static_cast<fe::math::Scalar>(std::sin(pitch_)),
            static_cast<fe::math::Scalar>(std::cos(pitch_) * std::cos(yaw_)));
        camera().target = target_;
        camera().position = target_ + direction * distance_;
        return fe::core::successStatus();
    }

private:
    void deriveOrbitFromCamera() {
        const fe::math::Vec3 offset = camera().position - camera().target;
        const fe::math::Scalar length = offset.length();
        distance_ = length > 1.0e-4F ? length : 1.0F;
        const fe::math::Vec3 direction = offset / distance_;
        pitch_ = std::asin(fe::math::clamp(direction.y, -1.0F, 1.0F));
        yaw_ = std::atan2(direction.x, direction.z);
        target_ = camera().target;
    }

    fe::engine::Engine& engine_;
    fe::math::Vec3 target_{0.0F, 0.0F, 0.0F};
    fe::math::Scalar yaw_ = 0.0F;
    fe::math::Scalar pitch_ = 0.0F;
    fe::math::Scalar distance_ = 1.0F;
};

}  // namespace

int main(int argc, char** argv) {
    DesktopOptions options;
    if (!parseArguments(argc, argv, options)) {
        printUsage();
        return argc <= 1 ? 0 : 2;
    }

    fe::engine::Engine engine;
    fe::engine::EngineConfig config;
    config.logLevel = options.verbose ? fe::core::LogLevel::Debug : fe::core::LogLevel::Info;
    config.attachConsoleLogSink = true;

    fe::core::Status status = engine.initialize(config);
    if (!status.ok()) {
        std::cerr << "engine initialization failed: " << status.error().toString() << "\n";
        return 1;
    }

    status = options.scenePath.empty() ? fe::engine::buildDemoScene(engine.scene())
                                       : engine.loadProject(options.scenePath);
    if (!status.ok()) {
        std::cerr << "cannot prepare the scene: " << status.error().toString() << "\n";
        (void)engine.shutdown();
        return 1;
    }

    std::unique_ptr<fe::platform::GlfwWindow> glfwWindow;
    std::unique_ptr<fe::platform::NullWindow> nullWindow;
    std::unique_ptr<fe::engine::Application> application;

    if (options.headless) {
        nullWindow = std::make_unique<fe::platform::NullWindow>(
            options.frames == 0 ? 1U : static_cast<std::uint32_t>(options.frames));
        fe::platform::WindowDesc desc;
        desc.title = std::string(fe::core::kProductName) + " (headless)";
        desc.width = options.width;
        desc.height = options.height;
        desc.createGraphicsContext = false;
        status = nullWindow->create(desc);
        if (!status.ok()) {
            std::cerr << "cannot create the headless window: " << status.error().toString() << "\n";
            (void)engine.shutdown();
            return 1;
        }

        auto renderer = std::make_unique<fe::rendering::SoftwareRenderer>();
        fe::rendering::RendererInitInfo info;
        info.applicationName = std::string(fe::core::kProductName);
        info.width = options.width;
        info.height = options.height;
        info.verticalSync = false;
        status = renderer->initialize(info);
        if (!status.ok()) {
            std::cerr << "cannot initialize the CPU renderer: " << status.error().toString() << "\n";
            (void)engine.shutdown();
            return 1;
        }
        status = engine.setRenderer(std::move(renderer));
        if (!status.ok()) {
            std::cerr << "cannot attach the renderer: " << status.error().toString() << "\n";
            (void)engine.shutdown();
            return 1;
        }

        application = std::make_unique<DesktopApplication>(engine, *nullWindow, engine.clock());
    } else {
        glfwWindow = std::make_unique<fe::platform::GlfwWindow>();
        fe::platform::WindowDesc desc;
        desc.title = std::string(fe::core::kProductName);
        desc.width = options.width;
        desc.height = options.height;
        desc.verticalSync = true;
        desc.createGraphicsContext = true;
        desc.graphicsApiMajor = 3;
        desc.graphicsApiMinor = 3;
        desc.graphicsApiCoreProfile = true;
        status = glfwWindow->create(desc);
        if (!status.ok()) {
            std::cerr << "cannot open a window: " << status.error().toString() << "\n";
            std::cerr << "suggested action: " << fe::core::suggestedAction(status.error().code()) << "\n";
            (void)engine.shutdown();
            return 1;
        }

        auto renderer = std::make_unique<fe::rendering::gl::OpenGLRenderer>();
        status = renderer->attachWindow(*glfwWindow);
        if (!status.ok()) {
            std::cerr << "cannot attach the OpenGL renderer: " << status.error().toString() << "\n";
            (void)engine.shutdown();
            return 1;
        }
        fe::rendering::RendererInitInfo info;
        info.applicationName = std::string(fe::core::kProductName);
        info.width = glfwWindow->width();
        info.height = glfwWindow->height();
        info.verticalSync = true;
        status = renderer->initialize(info);
        if (!status.ok()) {
            std::cerr << "cannot initialize the OpenGL renderer: " << status.error().toString() << "\n";
            glfwWindow->destroy();
            (void)engine.shutdown();
            return 1;
        }
        status = engine.setRenderer(std::move(renderer));
        if (!status.ok()) {
            std::cerr << "cannot attach the renderer: " << status.error().toString() << "\n";
            glfwWindow->destroy();
            (void)engine.shutdown();
            return 1;
        }

        application = std::make_unique<DesktopApplication>(engine, *glfwWindow, engine.clock());
    }

    application->setScreenshotPath(options.screenshotPath);
    status = application->run(options.frames);

    std::cout << "rendered " << application->frameCount() << " frame(s)";
    if (engine.renderer() != nullptr) {
        std::cout << " with " << engine.renderer()->capabilities().backendName;
    }
    std::cout << '\n';

    if (glfwWindow != nullptr) {
        glfwWindow->destroy();
    }
    const fe::core::Status shutdownStatus = engine.shutdown();
    if (!status.ok()) {
        std::cerr << "error: " << status.error().toString() << "\n";
        std::cerr << "suggested action: " << fe::core::suggestedAction(status.error().code()) << "\n";
        return 1;
    }
    return shutdownStatus.ok() ? 0 : 1;
}
