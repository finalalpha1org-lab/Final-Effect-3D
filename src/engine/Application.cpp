// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/engine/Application.hpp"

#include <algorithm>

#include "finaleffect/rendering/ImageIO.hpp"

namespace fe::engine {

Application::Application(Engine& engine, platform::Window& window, platform::Clock& clock)
    : engine_(engine), window_(window), clock_(clock) {}

Application::~Application() = default;

core::Status Application::onStartup() {
    return core::successStatus();
}

core::Status Application::onFrameUpdate(const FrameContext& context) {
    (void)context;
    return core::successStatus();
}

core::Status Application::onShutdown() {
    return core::successStatus();
}

core::Status Application::handleResizeEvent() {
    rendering::Renderer* renderer = engine_.renderer();
    if (renderer == nullptr) {
        return core::successStatus();
    }
    const std::uint32_t width = window_.width();
    const std::uint32_t height = window_.height();
    if (width == 0 || height == 0) {
        return core::successStatus();  // minimized: keep the current framebuffer size
    }
    if (width == renderer->width() && height == renderer->height()) {
        return core::successStatus();
    }
    return renderer->resize(width, height);
}

core::Status Application::captureIfRequested() {
    if (screenshotPath_.empty()) {
        return core::successStatus();
    }
    rendering::Renderer* renderer = engine_.renderer();
    if (renderer == nullptr) {
        return core::makeError(core::ErrorCode::NotInitialized, "no renderer is attached", "application");
    }
    rendering::ImageBuffer image;
    core::Status status = renderer->readbackImage(image);
    if (!status.ok()) {
        return status;
    }
    capturedPixels_ = image.pixelCount();
    status = rendering::writePpm(image, screenshotPath_);
    if (!status.ok()) {
        return status;
    }
    core::LogStream(engine_.logger(), core::LogLevel::Info, "application")
        << "captured frame " << image.width << "x" << image.height << " to " << screenshotPath_;
    return core::successStatus();
}

core::Status Application::run(std::uint64_t maxFrames) {
    if (!engine_.isInitialized()) {
        return core::makeError(core::ErrorCode::NotInitialized, "engine must be initialized before run()",
                               "application");
    }
    if (engine_.renderer() == nullptr) {
        return core::makeError(core::ErrorCode::NotInitialized, "no renderer is attached to the engine",
                               "application");
    }

    camera_ = engine_.defaultCamera();

    core::Status status = onStartup();
    if (!status.ok()) {
        return status;
    }

    clock_.reset();
    double previousSeconds = clock_.elapsedSeconds();

    while (!window_.shouldClose() && (maxFrames == 0 || frameIndex_ < maxFrames)) {
        window_.pollEvents();
        if (window_.consumeResizedFlag()) {
            status = handleResizeEvent();
            if (!status.ok()) {
                break;
            }
        }

        const double nowSeconds = clock_.elapsedSeconds();
        const double deltaSeconds = std::max(0.0, nowSeconds - previousSeconds);
        previousSeconds = nowSeconds;
        lastFrameSeconds_ = deltaSeconds;

        FrameContext context{frameIndex_ + 1U, deltaSeconds, nowSeconds, window_, engine_.renderer(),
                             engine_.scene()};
        status = onFrameUpdate(context);
        if (!status.ok()) {
            break;
        }

        status = engine_.renderFrame(camera_, math::Vec3(0.06F, 0.07F, 0.09F));
        if (!status.ok()) {
            break;
        }
        ++frameIndex_;

        const bool lastFrame = window_.shouldClose() || (maxFrames != 0 && frameIndex_ >= maxFrames);
        if (lastFrame) {
            status = captureIfRequested();
            if (!status.ok()) {
                break;
            }
        }

        status = window_.present();
        if (!status.ok()) {
            break;
        }
    }

    const core::Status shutdownStatus = onShutdown();
    return status.ok() ? shutdownStatus : status;
}

}  // namespace fe::engine
