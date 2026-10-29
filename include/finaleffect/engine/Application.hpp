// Final Effect 3D - application frame loop.
// SPDX-License-Identifier: GPL-3.0-only
//
// Separation of concerns:
//   * Engine owns subsystems and knows nothing about time or input.
//   * Application owns the loop, the camera and the frame policy; the desktop host
//     subclasses it for input handling.
//   * Nothing here allocates per frame and nothing reads global state, so a
//     headless run produces the same result as a windowed run.
#pragma once

#include <cstdint>
#include <string>

#include "finaleffect/core/Error.hpp"
#include "finaleffect/engine/Engine.hpp"
#include "finaleffect/platform/Clock.hpp"
#include "finaleffect/platform/Window.hpp"
#include "finaleffect/rendering/Renderer.hpp"

namespace fe::engine {

struct FrameContext {
    std::uint64_t frameIndex = 0;
    double deltaSeconds = 0.0;
    double elapsedSeconds = 0.0;
    platform::Window& window;
    rendering::Renderer* renderer = nullptr;
    scene::Scene& scene;
};

class Application {
public:
    Application(Engine& engine, platform::Window& window, platform::Clock& clock);
    virtual ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /// Captures the last rendered frame into a PPM file before it is presented.
    /// Empty (the default) disables capture. Used by the CLI, tests and CI.
    void setScreenshotPath(std::string path) { screenshotPath_ = std::move(path); }
    const std::string& screenshotPath() const noexcept { return screenshotPath_; }

    /// Runs until the window closes, or until maxFrames frames were produced
    /// (0 = unlimited, used by headless runs).
    core::Status run(std::uint64_t maxFrames = 0);

    rendering::Camera& camera() noexcept { return camera_; }
    const rendering::Camera& camera() const noexcept { return camera_; }

    std::uint64_t frameCount() const noexcept { return frameIndex_; }
    double lastFrameSeconds() const noexcept { return lastFrameSeconds_; }
    std::size_t capturedPixelCount() const noexcept { return capturedPixels_; }

protected:
    /// Called once after the engine is verified to be initialized.
    virtual core::Status onStartup();
    /// Called once per frame, before rendering. Override for input and animation.
    virtual core::Status onFrameUpdate(const FrameContext& context);
    /// Called once after the loop ends, also when it ended because of an error.
    virtual core::Status onShutdown();

private:
    core::Status handleResizeEvent();
    core::Status captureIfRequested();

    Engine& engine_;
    platform::Window& window_;
    platform::Clock& clock_;
    rendering::Camera camera_;
    std::string screenshotPath_;
    std::size_t capturedPixels_ = 0;
    std::uint64_t frameIndex_ = 0;
    double lastFrameSeconds_ = 0.0;
};

}  // namespace fe::engine
