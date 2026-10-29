// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/engine/Engine.hpp"

#include <utility>

#include "finaleffect/core/Version.hpp"
#include "finaleffect/project/ProjectFile.hpp"
#include "finaleffect/rendering/RenderViewBuilder.hpp"

namespace fe::engine {

Engine::Engine() : logger_(core::LogLevel::Info, "engine") {}

Engine::~Engine() {
    if (initialized_) {
        (void)shutdown();
    }
}

core::Status Engine::initialize(const EngineConfig& config) {
    if (initialized_) {
        return core::makeError(core::ErrorCode::AlreadyExists, "engine is already initialized", "engine");
    }
    logger_.setMinimumLevel(config.logLevel);
    if (config.attachConsoleLogSink) {
        logger_.addSink(std::make_shared<core::OstreamLogSink>(true));
    }

    const core::Result<platform::AppPaths> paths =
        platform::resolveAppPaths(config.organizationName, config.applicationId);
    if (!paths.ok()) {
        return paths.error();
    }
    paths_ = paths.value();

    fileSystem_ = std::make_unique<platform::StdFileSystem>();
    clock_ = std::make_unique<platform::SteadyClock>();

    const core::Status logDirectoryStatus = fileSystem_->createDirectories(paths_.logDirectory);
    if (!logDirectoryStatus.ok()) {
        core::LogStream(logger_, core::LogLevel::Warning, "engine")
            << "cannot create the log directory: " << logDirectoryStatus.error().toString();
    }

    core::LogStream(logger_, core::LogLevel::Info, "engine") << "engine initialized: " << core::buildInfo();
    core::LogStream(logger_, core::LogLevel::Debug, "engine")
        << "user data directory: " << paths_.userDataDirectory;

    initialized_ = true;
    return core::successStatus();
}

core::Status Engine::shutdown() {
    if (!initialized_) {
        return core::successStatus();
    }
    if (renderer_ != nullptr) {
        const core::Status status = renderer_->shutdown();
        if (!status.ok()) {
            core::LogStream(logger_, core::LogLevel::Error, "engine")
                << "renderer shutdown failed: " << status.error().toString();
        }
        renderer_.reset();
    }
    core::LogStream(logger_, core::LogLevel::Info, "engine") << "engine shut down";
    logger_.flush();
    fileSystem_.reset();
    clock_.reset();
    initialized_ = false;
    return core::successStatus();
}

core::Status Engine::setRenderer(std::unique_ptr<rendering::Renderer> renderer) {
    if (!initialized_) {
        return core::makeError(core::ErrorCode::NotInitialized, "engine is not initialized", "engine");
    }
    if (renderer_ != nullptr) {
        (void)renderer_->shutdown();
    }
    renderer_ = std::move(renderer);
    if (renderer_ != nullptr) {
        core::LogStream(logger_, core::LogLevel::Info, "engine")
            << "renderer attached: " << renderer_->capabilities().backendName << " "
            << renderer_->width() << "x" << renderer_->height();
    }
    return core::successStatus();
}

core::Status Engine::renderFrame(const rendering::Camera& camera, const math::Vec3& clearColor) {
    if (!initialized_) {
        return core::makeError(core::ErrorCode::NotInitialized, "engine is not initialized", "engine");
    }
    if (renderer_ == nullptr) {
        return core::makeError(core::ErrorCode::NotInitialized, "no renderer is attached", "engine");
    }
    const core::Result<rendering::RenderView> view =
        rendering::buildRenderView(scene_, camera, meshLibrary_);
    if (!view.ok()) {
        return view.error();
    }
    rendering::FrameInfo frame;
    frame.clearColor = clearColor;

    core::Status status = renderer_->beginFrame(frame);
    if (!status.ok()) {
        return status;
    }
    status = renderer_->submit(view.value());
    if (!status.ok()) {
        return status;
    }
    return renderer_->endFrame();
}

core::Result<rendering::ImageBuffer> Engine::captureFrame(const rendering::Camera& camera) {
    if (renderer_ == nullptr) {
        return core::makeError(core::ErrorCode::NotInitialized, "no renderer is attached", "engine");
    }
    const core::Status status = renderFrame(camera, math::Vec3(0.06F, 0.07F, 0.09F));
    if (!status.ok()) {
        return status.error();
    }
    rendering::ImageBuffer image;
    const core::Status readback = renderer_->readbackImage(image);
    if (!readback.ok()) {
        return readback.error();
    }
    return image;
}

rendering::Camera Engine::defaultCamera() const {
    return rendering::frameScene(scene_, meshLibrary_);
}

core::Status Engine::saveProject(const std::string& path) const {
    project::ProjectDocument document = project::ProjectDocument::fromScene(scene_);
    return project::saveProjectFile(*fileSystem_, path, document);
}

core::Status Engine::loadProject(const std::string& path) {
    core::Result<project::ProjectDocument> document = project::loadProjectFile(*fileSystem_, path);
    if (!document.ok()) {
        return document.error();
    }
    core::Result<scene::Scene> loaded = document.value().toScene();
    if (!loaded.ok()) {
        return loaded.error();
    }
    scene_ = std::move(loaded.value());
    core::LogStream(logger_, core::LogLevel::Info, "engine")
        << "loaded project '" << path << "' with " << scene_.objectCount() << " objects";
    return core::successStatus();
}

}  // namespace fe::engine
