// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/app/starter_scene.hpp"
#include "finaleffect/math/constants.hpp"

#include <cmath>
#include <utility>
#include <variant>

namespace finaleffect::app {

StudioSession::StudioSession(
    platform::IWindow& window,
    std::unique_ptr<rendering::IRenderDevice> render_device,
    StudioSessionConfig config)
    : window_(window),
      renderer_(std::move(render_device)),
      config_(config),
      command_stack_(scene::CommandStackLimits{
          .max_entries = 128,
          .max_memory_bytes = config.command_history_max_bytes,
      }) {}

void StudioSession::syncCameraControllerFromScene() noexcept {
    math::Vec3 focus{0.0f, 0.65f, 0.0f};
    if (custom_project_loaded_) {
        focus = math::Vec3::zero();
    }
    const scene::ObjectId cam_id = document_.scene.activeCamera();
    if (cam_id.isValid()) {
        if (const auto* cam_obj = document_.scene.findObject(cam_id); cam_obj != nullptr) {
            camera_controller_.syncFromCameraTransform(cam_obj->local_transform, focus);
        }
    }
}

core::Status StudioSession::applyCameraControllerToScene() {
    const scene::ObjectId cam_id = document_.scene.activeCamera();
    if (!cam_id.isValid()) {
        return core::Status::ok();
    }
    const auto* cam_obj = document_.scene.findObject(cam_id);
    if (cam_obj == nullptr) {
        return core::Status::ok();
    }
    return document_.scene.setObjectTransform(cam_id, camera_controller_.computeCameraTransform());
}

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
        document_.metadata.description = "Stage 2 interactive 3D showcase project";
        auto starter_res = populateStarterScene(document_.scene);
        if (!starter_res) {
            return starter_res.error();
        }
    }

    syncCameraControllerFromScene();
    return core::Status::ok();
}

std::optional<scene::RaycastHit> StudioSession::pickAtScreenPosition(float screen_x, float screen_y) {
    const auto fb = window_.framebufferSize();
    if (fb.width == 0 || fb.height == 0) {
        return std::nullopt;
    }

    math::Mat4 cam_world = math::Mat4::lookAtRH(
        math::Vec3{0.0f, 2.5f, 6.0f},
        math::Vec3::zero(),
        math::Vec3::unitY()).inverted().value_or(math::Mat4::identity());
    scene::CameraComponent cam_comp{};

    if (document_.scene.activeCamera().isValid()) {
        if (const auto* cam_obj = document_.scene.findObject(document_.scene.activeCamera());
            cam_obj != nullptr && cam_obj->camera.has_value()) {
            if (auto cw = document_.scene.computeWorldMatrix(cam_obj->id); cw.hasValue()) {
                cam_world = cw.value();
            }
            cam_comp = cam_obj->camera.value();
        }
    }

    auto ray_res = OrbitCameraController::screenPointToWorldRay(
        screen_x, screen_y, fb.width, fb.height, cam_world, cam_comp);
    if (!ray_res) {
        return std::nullopt;
    }

    auto hit = scene::raycastScene(
        document_.scene,
        ray_res.value(),
        scene::RaycastOptions{.max_distance = cam_comp.far_plane});
    selected_object_ = hit.has_value() ? hit->object_id : scene::ObjectId{};
    return hit;
}

core::Status StudioSession::frameSelectionOrScene() {
    float fov_y = math::degToRad(48.0f);
    if (document_.scene.activeCamera().isValid()) {
        if (const auto* cam_obj = document_.scene.findObject(document_.scene.activeCamera());
            cam_obj != nullptr && cam_obj->camera.has_value()) {
            fov_y = cam_obj->camera->fov_y_radians;
        }
    }

    math::AABB target_bounds = math::AABB::empty();
    if (selected_object_.isValid() && document_.scene.findObject(selected_object_) != nullptr) {
        auto bounds_res = document_.scene.computeObjectWorldBounds(selected_object_);
        if (bounds_res && bounds_res.value().isValid()) {
            target_bounds = bounds_res.value();
        }
    }

    if (!target_bounds.isValid()) {
        for (const auto& obj : document_.scene.objects()) {
            if (!obj.visible || !obj.mesh_id.has_value()) {
                continue;
            }
            if (auto b = document_.scene.computeObjectWorldBounds(obj.id); b && b.value().isValid()) {
                target_bounds.expand(b.value());
            }
        }
    }

    if (target_bounds.isValid()) {
        camera_controller_.frameBounds(target_bounds, fov_y);
        return applyCameraControllerToScene();
    }
    return core::Status::ok();
}

core::Status StudioSession::processInputEvent(const platform::InputEvent& event) {
    if (const auto* mb = std::get_if<platform::MouseButtonEvent>(&event)) {
        last_cursor_x_ = mb->x;
        last_cursor_y_ = mb->y;
        has_cursor_pos_ = true;

        if (mb->action == platform::KeyAction::Press) {
            press_cursor_x_ = mb->x;
            press_cursor_y_ = mb->y;
            if (mb->button == platform::MouseButton::Middle || mb->button == platform::MouseButton::Right) {
                drag_mode_ = mb->mods.shift ? DragMode::Pan : DragMode::Orbit;
            } else if (mb->button == platform::MouseButton::Left) {
                if (mb->mods.alt) {
                    drag_mode_ = mb->mods.shift ? DragMode::Pan : DragMode::Orbit;
                } else {
                    drag_mode_ = DragMode::PendingLeftClick;
                }
            }
        } else if (mb->action == platform::KeyAction::Release) {
            if (drag_mode_ == DragMode::PendingLeftClick && mb->button == platform::MouseButton::Left) {
                const double dx = mb->x - press_cursor_x_;
                const double dy = mb->y - press_cursor_y_;
                if ((dx * dx + dy * dy) <= 16.0) {
                    (void)pickAtScreenPosition(static_cast<float>(mb->x), static_cast<float>(mb->y));
                }
            }
            drag_mode_ = DragMode::None;
        }
        return core::Status::ok();
    }

    if (const auto* cm = std::get_if<platform::CursorMoveEvent>(&event)) {
        if (!has_cursor_pos_) {
            last_cursor_x_ = cm->x;
            last_cursor_y_ = cm->y;
            has_cursor_pos_ = true;
            return core::Status::ok();
        }
        const auto dx = static_cast<float>(cm->x - last_cursor_x_);
        const auto dy = static_cast<float>(cm->y - last_cursor_y_);
        last_cursor_x_ = cm->x;
        last_cursor_y_ = cm->y;

        if (drag_mode_ == DragMode::Orbit) {
            camera_controller_.orbit(
                -dx * config_.orbit_radians_per_pixel,
                -dy * config_.orbit_radians_per_pixel);
            return applyCameraControllerToScene();
        }
        if (drag_mode_ == DragMode::Pan) {
            float fov_y = math::degToRad(48.0f);
            if (const auto* cam_obj = document_.scene.findObject(document_.scene.activeCamera());
                cam_obj != nullptr && cam_obj->camera.has_value()) {
                fov_y = cam_obj->camera->fov_y_radians;
            }
            camera_controller_.pan(dx, dy, window_.framebufferSize().height, fov_y);
            return applyCameraControllerToScene();
        }
        return core::Status::ok();
    }

    if (const auto* sc = std::get_if<platform::ScrollEvent>(&event)) {
        if (std::fabs(sc->delta_y) > 1.0e-5) {
            camera_controller_.zoom(static_cast<float>(sc->delta_y));
            return applyCameraControllerToScene();
        }
        return core::Status::ok();
    }

    if (const auto* key = std::get_if<platform::KeyEvent>(&event)) {
        if (key->action != platform::KeyAction::Press) {
            return core::Status::ok();
        }

        if (key->key == platform::KeyCode::F && !key->mods.ctrl) {
            return frameSelectionOrScene();
        }

        if (key->key == platform::KeyCode::Space && !key->mods.ctrl) {
            config_.animate_showcase_rig = !config_.animate_showcase_rig;
            return core::Status::ok();
        }

        if ((key->key == platform::KeyCode::Delete || key->key == platform::KeyCode::Backspace) &&
            selected_object_.isValid() &&
            selected_object_ != document_.scene.activeCamera() &&
            document_.scene.findObject(selected_object_) != nullptr) {
            const scene::ObjectId to_delete = selected_object_;
            selected_object_ = {};
            return command_stack_.execute(
                document_.scene,
                std::make_unique<scene::DeleteObjectCommand>(
                    to_delete, scene::RemovalMode::Recursive));
        }

        if (key->mods.ctrl && key->key == platform::KeyCode::Z && !key->mods.shift) {
            if (command_stack_.canUndo()) {
                auto st = command_stack_.undo(document_.scene);
                if (selected_object_.isValid() && document_.scene.findObject(selected_object_) == nullptr) {
                    selected_object_ = {};
                }
                return st;
            }
            return core::Status::ok();
        }

        if (key->mods.ctrl &&
            (key->key == platform::KeyCode::Y || (key->key == platform::KeyCode::Z && key->mods.shift))) {
            if (command_stack_.canRedo()) {
                auto st = command_stack_.redo(document_.scene);
                if (selected_object_.isValid() && document_.scene.findObject(selected_object_) == nullptr) {
                    selected_object_ = {};
                }
                return st;
            }
            return core::Status::ok();
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

    for (const auto& ev : window_.consumeInputEvents()) {
        auto ev_st = processInputEvent(ev);
        if (!ev_st) {
            return ev_st;
        }
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

    auto render_st = renderer_.renderScene(document_.scene, {}, selected_object_);
    if (!render_st) {
        return render_st;
    }

    window_.swapBuffers();
    return core::Status::ok();
}

void StudioSession::onShutdown(core::Engine& engine) noexcept {
    const auto stats = collectResourceStats();
    engine.logger().info(
        "App.Session",
        "Shutting down session (objects=" + std::to_string(stats.scene_object_count) +
            ", meshes=" + std::to_string(stats.mesh_asset_count) +
            ", gpu_buffers=" + std::to_string(stats.gpu_mesh_buffer_count) +
            ", undo_cmds=" + std::to_string(stats.undo_command_count) + ")");
    renderer_.shutdown();
}

core::Status StudioSession::resetStarterProject() {
    if (renderer_.isInitialized()) {
        renderer_.releaseSceneResources();
    }
    document_ = project::ProjectDocument{};
    document_.metadata.project_name = "Final Effect 3D Starter Project";
    document_.metadata.author = "Final Effect 3D";
    document_.metadata.description = "Interactive 3D studio showcase project";
    auto starter_res = populateStarterScene(document_.scene);
    if (!starter_res) {
        return starter_res.error();
    }
    command_stack_.clear();
    selected_object_ = {};
    custom_project_loaded_ = false;
    rig_angle_radians_ = 0.0f;
    syncCameraControllerFromScene();
    return core::Status::ok();
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
    command_stack_.clear();
    selected_object_ = {};
    custom_project_loaded_ = true;
    rig_angle_radians_ = 0.0f;
    syncCameraControllerFromScene();
    return core::Status::ok();
}

core::Status StudioSession::saveProjectToFile(
    const std::filesystem::path& path,
    platform::IFileSystem& file_system) const {
    return project::ProjectSerializer::saveToFile(document_, path, file_system);
}

core::Status StudioSession::saveProjectBundleToFile(
    const std::filesystem::path& fe3d_path,
    platform::IFileSystem& file_system) const {
    return project::ProjectSerializer::saveBundleToFile(document_, fe3d_path, file_system);
}

core::ResourceStats StudioSession::collectResourceStats() const noexcept {
    core::ResourceStats stats = renderer_.collectResourceStats(document_.scene);
    stats.undo_command_count = command_stack_.undoCount();
    stats.redo_command_count = command_stack_.redoCount();
    stats.command_history_bytes = command_stack_.totalMemoryBytes();
    return stats;
}

} // namespace finaleffect::app
