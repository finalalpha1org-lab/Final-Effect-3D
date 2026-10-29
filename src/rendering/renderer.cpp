// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/rendering/renderer.hpp"

#include <utility>

namespace finaleffect::rendering {

Renderer::Renderer(std::unique_ptr<IRenderDevice> device) : device_(std::move(device)) {}

Renderer::~Renderer() {
    shutdown();
}

core::Status Renderer::initialize(std::uint32_t width, std::uint32_t height) {
    if (initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Renderer is already initialized");
    }
    if (!device_) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Renderer requires a non-null IRenderDevice");
    }

    auto st = device_->initialize(width, height);
    if (!st) {
        return st;
    }

    viewport_width_ = width;
    viewport_height_ = height;
    initialized_ = true;
    return core::Status::ok();
}

void Renderer::releaseSceneResources() noexcept {
    if (!device_) {
        gpu_mesh_cache_.clear();
        return;
    }
    for (const auto& [mesh_id, cached] : gpu_mesh_cache_) {
        (void)mesh_id;
        if (cached.handle.isValid()) {
            device_->destroyMesh(cached.handle);
        }
    }
    gpu_mesh_cache_.clear();
}

void Renderer::shutdown() noexcept {
    if (!initialized_) {
        return;
    }
    releaseSceneResources();
    if (device_) {
        device_->shutdown();
    }
    initialized_ = false;
}

core::Status Renderer::resize(std::uint32_t width, std::uint32_t height) {
    if (!initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Renderer::resize called before initialize");
    }
    if (width == viewport_width_ && height == viewport_height_) {
        return core::Status::ok();
    }
    auto st = device_->resize(width, height);
    if (!st) {
        return st;
    }
    viewport_width_ = width;
    viewport_height_ = height;
    return core::Status::ok();
}

core::Result<GpuMeshHandle> Renderer::ensureMeshUploaded(const scene::MeshAsset& asset) {
    const auto it = gpu_mesh_cache_.find(asset.id);
    if (it != gpu_mesh_cache_.end() && it->second.revision == asset.revision) {
        return core::Result<GpuMeshHandle>::ok(it->second.handle);
    }
    if (it != gpu_mesh_cache_.end() && it->second.handle.isValid()) {
        device_->destroyMesh(it->second.handle);
        gpu_mesh_cache_.erase(it);
    }

    auto created = device_->createMesh(asset.data);
    if (!created) {
        return created.error();
    }
    gpu_mesh_cache_.emplace(asset.id, CachedGpuMesh{created.value(), asset.revision});
    return created;
}

core::Status Renderer::renderScene(const scene::Scene& scene, const FrameClearDesc& clear) {
    if (!initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Renderer::renderScene called before initialize");
    }

    auto begin_st = device_->beginFrame(clear);
    if (!begin_st) {
        return begin_st;
    }

    // Minimized window (0x0 framebuffer): complete frame cleanly without projecting 0-aspect camera.
    if (viewport_width_ == 0 || viewport_height_ == 0) {
        return device_->endFrame();
    }

    // Resolve active camera view + projection matrices.
    math::Mat4 view_matrix = math::Mat4::lookAtRH(
        math::Vec3{0.0f, 2.5f, 6.0f},
        math::Vec3::zero(),
        math::Vec3::unitY());
    math::Vec3 camera_pos{0.0f, 2.5f, 6.0f};
    const float aspect = static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_);
    math::Mat4 proj_matrix = scene::CameraComponent{}.computeProjectionMatrix(
        aspect,
        device_->capabilities().clip_space_depth);

    if (scene.activeCamera().isValid()) {
        const auto* cam_obj = scene.findObject(scene.activeCamera());
        if (cam_obj != nullptr && cam_obj->camera.has_value()) {
            auto cam_world_res = scene.computeWorldMatrix(cam_obj->id);
            if (!cam_world_res) {
                (void)device_->endFrame();
                return cam_world_res.error();
            }
            const math::Mat4& cam_world = cam_world_res.value();
            camera_pos = cam_world.transformPoint(math::Vec3::zero());
            if (const auto inv_cam = cam_world.inverted(); inv_cam.has_value()) {
                view_matrix = inv_cam.value();
            }
            proj_matrix = cam_obj->camera->computeProjectionMatrix(
                aspect,
                device_->capabilities().clip_space_depth);
        }
    }

    const math::Mat4 view_proj = proj_matrix * view_matrix;

    // Resolve primary directional light (or fallback studio sun light).
    DirectionalLightUniforms light_uniforms{};
    for (const auto& obj : scene.objects()) {
        if (!obj.visible || !obj.directional_light.has_value()) {
            continue;
        }
        auto light_world = scene.computeWorldMatrix(obj.id);
        if (light_world) {
            // Object's local forward (-Z) is light ray travel direction; negate for surface-to-light vector.
            const math::Vec3 ray_dir = light_world.value().transformDirection(math::Vec3{0.0f, 0.0f, -1.0f}).normalized();
            if (ray_dir.lengthSquared() > math::kStrictEpsilon) {
                light_uniforms.direction_to_light = -ray_dir;
            }
            light_uniforms.radiance = obj.directional_light->color * obj.directional_light->intensity;
            break;
        }
    }

    // Submit visible mesh draw commands.
    for (const auto& obj : scene.objects()) {
        if (!obj.visible || !obj.mesh_id.has_value()) {
            continue;
        }
        const auto* mesh_asset = scene.findMesh(obj.mesh_id.value());
        if (mesh_asset == nullptr) {
            continue;
        }

        auto gpu_handle_res = ensureMeshUploaded(*mesh_asset);
        if (!gpu_handle_res) {
            (void)device_->endFrame();
            return gpu_handle_res.error();
        }

        auto world_res = scene.computeWorldMatrix(obj.id);
        if (!world_res) {
            (void)device_->endFrame();
            return world_res.error();
        }

        DrawMeshCommand cmd{
            .mesh_handle = gpu_handle_res.value(),
            .model_matrix = world_res.value(),
            .view_projection_matrix = view_proj,
            .camera_position = camera_pos,
            .lighting = light_uniforms,
        };
        auto draw_st = device_->drawMesh(cmd);
        if (!draw_st) {
            (void)device_->endFrame();
            return draw_st;
        }
    }

    return device_->endFrame();
}

core::ResourceStats Renderer::collectResourceStats(const scene::Scene& scene) const noexcept {
    core::ResourceStats stats = scene.collectResourceStats();
    if (device_) {
        stats.gpu_mesh_buffer_count = device_->liveMeshCount();
        stats.gpu_mesh_bytes = device_->liveMeshBytes();
    }
    return stats;
}

} // namespace finaleffect::rendering
