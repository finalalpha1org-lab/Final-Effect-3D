// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/app/starter_scene.hpp"

#include <utility>

namespace finaleffect::app {

StudioSession::StudioSession(
    platform::IWindow& window,
    std::unique_ptr<rendering::IRenderDevice> render_device,
    StudioSessionConfig config)
    : window_(window),
      renderer_(std::move(render_device)),
      config_(config) {}

core::Status StudioSession::onInitialize(core::Engine& engine) {
    const auto fb = window_.framebufferSize();
    auto render_st = renderer_.initialize(fb.width, fb.height);
    if (!render_st) {
        return render_st;
    }

    engine.logger().info(
        "App.Session",
        "Renderer initialized: backend=" + renderer_.device().capabilities().backend_name +
            ", device=" + renderer_.device().capabilities().device_name);

    if (!custom_project_loaded_) {
        document_.metadata.project_name = "Final Effect 3D Starter Project";
        document_.metadata.author = "Final Effect 3D";
        document_.metadata.description = "Stage 1 default 3D showcase project";
        auto starter_res = populateStarterScene(document_.scene);
        if (!starter_res) {
            return starter_res.error();
        }
    }

    return core::Status::ok();
}

core::Status StudioSession::onUpdate(core::Engine&, const core::FrameTimings& timings) {
    window_.pollEvents();

    const auto fb = window_.framebufferSize();
    auto resize_st = renderer_.resize(fb.width, fb.height);
    if (!resize_st) {
        return resize_st;
    }

    if (config_.animate_showcase_rig) {
        if (const auto* rig = document_.scene.findObjectByName("RigPivot"); rig != nullptr) {
            rig_angle_radians_ += static_cast<float>(timings.delta_seconds) * config_.rig_angular_speed_radians;
            math::Transform tf = rig->local_transform;
            tf.rotation = math::Quat::fromAxisAngle(math::Vec3::unitY(), rig_angle_radians_);
            auto tf_st = document_.scene.setObjectTransform(rig->id, tf);
            if (!tf_st) {
                return tf_st;
            }
        }
    }

    auto render_st = renderer_.renderScene(document_.scene);
    if (!render_st) {
        return render_st;
    }

    window_.swapBuffers();
    return core::Status::ok();
}

void StudioSession::onShutdown(core::Engine& engine) noexcept {
    const auto stats = renderer_.collectResourceStats(document_.scene);
    engine.logger().info(
        "App.Session",
        "Shutting down session (objects=" + std::to_string(stats.scene_object_count) +
            ", meshes=" + std::to_string(stats.mesh_asset_count) +
            ", gpu_buffers=" + std::to_string(stats.gpu_mesh_buffer_count) + ")");
    renderer_.shutdown();
}

core::Status StudioSession::loadProjectFromFile(
    const std::filesystem::path& path,
    const platform::IFileSystem& file_system) {
    auto loaded = project::ProjectSerializer::loadFromFile(path, file_system);
    if (!loaded) {
        return loaded.error();
    }
    if (renderer_.isInitialized()) {
        renderer_.releaseSceneResources();
    }
    document_ = std::move(loaded).value();
    custom_project_loaded_ = true;
    rig_angle_radians_ = 0.0f;
    return core::Status::ok();
}

core::Status StudioSession::saveProjectToFile(
    const std::filesystem::path& path,
    platform::IFileSystem& file_system) const {
    return project::ProjectSerializer::saveToFile(document_, path, file_system);
}

} // namespace finaleffect::app
