// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/application.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/platform/window.hpp"
#include "finaleffect/project/project.hpp"
#include "finaleffect/rendering/renderer.hpp"

#include <filesystem>
#include <memory>

namespace finaleffect::app {

struct StudioSessionConfig {
    bool animate_showcase_rig{true};
    float rig_angular_speed_radians{0.65f};
};

// Application layer session connecting project/scene state, window surface, and Renderer.
class StudioSession final : public core::IApplication {
public:
    StudioSession(
        platform::IWindow& window,
        std::unique_ptr<rendering::IRenderDevice> render_device,
        StudioSessionConfig config = {});

    [[nodiscard]] core::Status onInitialize(core::Engine& engine) override;
    [[nodiscard]] core::Status onUpdate(core::Engine& engine, const core::FrameTimings& timings) override;
    void onShutdown(core::Engine& engine) noexcept override;

    [[nodiscard]] core::Status loadProjectFromFile(
        const std::filesystem::path& path,
        const platform::IFileSystem& file_system);

    [[nodiscard]] core::Status saveProjectToFile(
        const std::filesystem::path& path,
        platform::IFileSystem& file_system) const;

    [[nodiscard]] project::ProjectDocument& document() noexcept { return document_; }
    [[nodiscard]] const project::ProjectDocument& document() const noexcept { return document_; }
    [[nodiscard]] rendering::Renderer& renderer() noexcept { return renderer_; }
    [[nodiscard]] const rendering::Renderer& renderer() const noexcept { return renderer_; }

private:
    platform::IWindow& window_;
    rendering::Renderer renderer_;
    StudioSessionConfig config_;
    project::ProjectDocument document_{};
    bool custom_project_loaded_{false};
    float rig_angle_radians_{0.0f};
};

} // namespace finaleffect::app
