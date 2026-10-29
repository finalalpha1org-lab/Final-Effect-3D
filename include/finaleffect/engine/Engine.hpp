// Final Effect 3D - engine composition root.
// SPDX-License-Identifier: GPL-3.0-only
//
// The Engine owns every long-lived subsystem and hands out references to them.
// Ownership is explicit and there is no service locator: subsystems are created
// by initialize() in a fixed order and destroyed by shutdown() in reverse order.
// Nothing in the engine reads global state.
#pragma once

#include <memory>
#include <string>

#include "finaleffect/core/Error.hpp"
#include "finaleffect/core/Log.hpp"
#include "finaleffect/platform/Clock.hpp"
#include "finaleffect/platform/FileSystem.hpp"
#include "finaleffect/platform/Paths.hpp"
#include "finaleffect/rendering/MeshLibrary.hpp"
#include "finaleffect/rendering/Renderer.hpp"
#include "finaleffect/scene/Scene.hpp"

namespace fe::engine {

struct EngineConfig {
    core::LogLevel logLevel = core::LogLevel::Info;
    std::string organizationName = "FinalEffect3D";
    std::string applicationId = "FinalEffect3D";
    bool attachConsoleLogSink = true;
};

class Engine {
public:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    /// Resolves application paths, starts logging and creates the empty scene.
    core::Status initialize(const EngineConfig& config = EngineConfig{});
    /// Idempotent: shuts the renderer down (if any) and releases subsystems.
    core::Status shutdown();
    bool isInitialized() const noexcept { return initialized_; }

    core::Logger& logger() noexcept { return logger_; }
    platform::FileSystem& fileSystem() noexcept { return *fileSystem_; }
    platform::Clock& clock() noexcept { return *clock_; }
    const platform::AppPaths& paths() const noexcept { return paths_; }

    scene::Scene& scene() noexcept { return scene_; }
    const scene::Scene& scene() const noexcept { return scene_; }
    rendering::MeshLibrary& meshLibrary() noexcept { return meshLibrary_; }

    /// Takes ownership of a renderer. Passing nullptr detaches the current one.
    core::Status setRenderer(std::unique_ptr<rendering::Renderer> renderer);
    rendering::Renderer* renderer() noexcept { return renderer_.get(); }
    const rendering::Renderer* renderer() const noexcept { return renderer_.get(); }

    /// Builds the RenderView for the scene and submits one frame.
    core::Status renderFrame(const rendering::Camera& camera, const math::Vec3& clearColor);

    /// Renders one frame offscreen and returns the pixels.
    core::Result<rendering::ImageBuffer> captureFrame(const rendering::Camera& camera);

    /// Camera that frames the current scene from a three-quarter view.
    rendering::Camera defaultCamera() const;

    core::Status saveProject(const std::string& path) const;
    core::Status loadProject(const std::string& path);

private:
    core::Logger logger_;
    std::unique_ptr<platform::FileSystem> fileSystem_;
    std::unique_ptr<platform::Clock> clock_;
    platform::AppPaths paths_;
    scene::Scene scene_;
    rendering::MeshLibrary meshLibrary_;
    std::unique_ptr<rendering::Renderer> renderer_;
    bool initialized_ = false;
};

}  // namespace fe::engine
