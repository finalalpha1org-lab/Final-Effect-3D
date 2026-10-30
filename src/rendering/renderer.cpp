// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/rendering/renderer.hpp"
#include "finaleffect/rendering/editor_grid.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace finaleffect::rendering {

namespace {

PbrSurfaceParams surfaceFromMaterial(const scene::PbrMaterial* mat) noexcept {
    if (mat == nullptr) {
        return PbrSurfaceParams{
            .base_color = {1.0f, 1.0f, 1.0f},
            .metallic = 0.05f,
            .roughness = 0.50f,
            .ambient_occlusion = 1.0f,
            .emissive_radiance = {0.0f, 0.0f, 0.0f},
            .opacity = 1.0f,
            .vertex_color_weight = 1.0f,
            .unlit_overlay = false,
        };
    }
    return PbrSurfaceParams{
        .base_color = mat->base_color,
        .metallic = mat->metallic,
        .roughness = mat->roughness,
        .ambient_occlusion = mat->ambient_occlusion,
        .emissive_radiance = mat->emissive_color * mat->emissive_strength,
        .opacity = mat->opacity,
        // Assigned PBR materials drive base_color directly while keeping subtle per-face shading nuance off.
        .vertex_color_weight = 0.0f,
        .unlit_overlay = false,
    };
}

} // namespace

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
        overlay_gpu_pool_.clear();
        return;
    }
    for (const auto& [mesh_id, cached] : gpu_mesh_cache_) {
        (void)mesh_id;
        if (cached.handle.isValid()) {
            device_->destroyMesh(cached.handle);
        }
    }
    gpu_mesh_cache_.clear();

    for (const GpuMeshHandle h : overlay_gpu_pool_) {
        if (h.isValid()) {
            device_->destroyMesh(h);
        }
    }
    overlay_gpu_pool_.clear();
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
    if (it != gpu_mesh_cache_.end()) {
        if (it->second.revision == asset.revision) {
            return core::Result<GpuMeshHandle>::ok(it->second.handle);
        }
        if (it->second.handle.isValid()) {
            auto up_st = device_->updateMesh(it->second.handle, asset.data);
            if (!up_st) {
                return up_st.error();
            }
            it->second.revision = asset.revision;
            return core::Result<GpuMeshHandle>::ok(it->second.handle);
        }
    }

    auto created = device_->createMesh(asset.data);
    if (!created) {
        return created.error();
    }
    gpu_mesh_cache_[asset.id] = CachedGpuMesh{created.value(), asset.revision};
    return created;
}

core::Result<GpuMeshHandle> Renderer::uploadOverlaySlot(std::size_t slot_index, const scene::MeshData& mesh) {
    while (overlay_gpu_pool_.size() <= slot_index) {
        overlay_gpu_pool_.push_back(GpuMeshHandle::invalid());
    }
    GpuMeshHandle& slot = overlay_gpu_pool_[slot_index];
    if (!slot.isValid()) {
        auto created = device_->createMesh(mesh);
        if (!created) {
            return created.error();
        }
        slot = created.value();
        return created;
    }
    auto st = device_->updateMesh(slot, mesh);
    if (!st) {
        return st.error();
    }
    return core::Result<GpuMeshHandle>::ok(slot);
}

namespace {

bool isObjectEffectivelyVisible(const scene::Scene& scene, scene::ObjectId id) noexcept {
    scene::ObjectId cur = id;
    std::size_t guard = 0;
    const std::size_t max_depth = scene.objectCount() + 1;
    while (cur.isValid() && guard++ < max_depth) {
        const auto* obj = scene.findObject(cur);
        if (obj == nullptr || !obj->visible) {
            return false;
        }
        cur = obj->parent_id;
    }
    return true;
}

math::Mat4 computeOrthonormalCameraViewMatrix(const math::Mat4& cam_world) noexcept {
    math::Vec3 right{cam_world(0, 0), cam_world(1, 0), cam_world(2, 0)};
    math::Vec3 up{cam_world(0, 1), cam_world(1, 1), cam_world(2, 1)};
    math::Vec3 back{cam_world(0, 2), cam_world(1, 2), cam_world(2, 2)};
    const math::Vec3 eye = cam_world.transformPoint(math::Vec3::zero());

    if (right.lengthSquared() > math::kStrictEpsilon &&
        up.lengthSquared() > math::kStrictEpsilon &&
        back.lengthSquared() > math::kStrictEpsilon) {
        back = back.normalized();
        right = up.cross(back);
        if (right.lengthSquared() > math::kStrictEpsilon) {
            right = right.normalized();
            up = back.cross(right).normalized();

            math::Mat4 view = math::Mat4::identity();
            view(0, 0) = right.x; view(0, 1) = right.y; view(0, 2) = right.z; view(0, 3) = -right.dot(eye);
            view(1, 0) = up.x;    view(1, 1) = up.y;    view(1, 2) = up.z;    view(1, 3) = -up.dot(eye);
            view(2, 0) = back.x;  view(2, 1) = back.y;  view(2, 2) = back.z;  view(2, 3) = -back.dot(eye);
            return view;
        }
    }
    if (const auto inv = cam_world.inverted(); inv.has_value()) {
        return inv.value();
    }
    return math::Mat4::identity();
}

} // namespace

core::Status Renderer::renderScene(
    const scene::Scene& scene,
    const FrameClearDesc& clear,
    scene::ObjectId selected_object,
    std::span<const ViewportOverlayMesh> overlays,
    const ViewportCameraOverride* camera_override,
    const ViewportGridOptions* grid_options) {
    if (!initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Renderer::renderScene called before initialize");
    }

    auto begin_st = device_->beginFrame(clear);
    if (!begin_st) {
        return begin_st;
    }

    if (viewport_width_ == 0 || viewport_height_ == 0) {
        return device_->endFrame();
    }

    math::Mat4 view_matrix = math::Mat4::lookAtRH(
        math::Vec3{0.0f, 2.5f, 6.0f},
        math::Vec3::zero(),
        math::Vec3::unitY());
    math::Vec3 camera_pos{0.0f, 2.5f, 6.0f};
    const float aspect = static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_);
    math::Mat4 proj_matrix = scene::CameraComponent{}.computeProjectionMatrix(
        aspect,
        device_->capabilities().clip_space_depth);

    if (camera_override != nullptr) {
        camera_pos = camera_override->camera_world.transformPoint(math::Vec3::zero());
        view_matrix = computeOrthonormalCameraViewMatrix(camera_override->camera_world);
        proj_matrix = camera_override->camera.computeProjectionMatrix(
            aspect,
            device_->capabilities().clip_space_depth);
    } else if (scene.activeCamera().isValid()) {
        const auto* cam_obj = scene.findObject(scene.activeCamera());
        if (cam_obj != nullptr && cam_obj->camera.has_value()) {
            auto cam_world_res = scene.computeWorldMatrix(cam_obj->id);
            if (!cam_world_res) {
                (void)device_->endFrame();
                return cam_world_res.error();
            }
            const math::Mat4& cam_world = cam_world_res.value();
            camera_pos = cam_world.transformPoint(math::Vec3::zero());
            view_matrix = computeOrthonormalCameraViewMatrix(cam_world);
            proj_matrix = cam_obj->camera->computeProjectionMatrix(
                aspect,
                device_->capabilities().clip_space_depth);
        }
    }

    const math::Mat4 view_proj = proj_matrix * view_matrix;

    DirectionalLightUniforms light_uniforms{};
    bool scene_has_any_light_component = false;
    std::size_t active_light_count = 0;

    for (const auto& obj : scene.objects()) {
        if (!obj.directional_light.has_value()) {
            continue;
        }
        scene_has_any_light_component = true;
        if (!isObjectEffectivelyVisible(scene, obj.id)) {
            continue;
        }
        auto light_world = scene.computeWorldMatrix(obj.id);
        if (!light_world) {
            continue;
        }
        math::Vec3 dir_to_light{0.45f, 0.80f, 0.40f};
        const math::Vec3 ray_dir =
            light_world.value().transformDirection(math::Vec3{0.0f, 0.0f, -1.0f}).normalized();
        if (ray_dir.lengthSquared() > math::kStrictEpsilon) {
            dir_to_light = -ray_dir;
        }
        // Subtle positional falloff boost near origin is avoided for true directional light;
        // however, world position & rotation of the light node update world transform & helper rays.
        const math::Vec3 rad = obj.directional_light->color * obj.directional_light->intensity;

        if (active_light_count == 0) {
            light_uniforms.direction_to_light = dir_to_light;
            light_uniforms.radiance = rad;
        } else if ((active_light_count - 1) < DirectionalLightUniforms::kMaxExtraLights) {
            const std::size_t slot = active_light_count - 1;
            light_uniforms.extra_lights[slot] = DirectionalLightLobe{
                .direction_to_light = dir_to_light,
                .radiance = rad,
            };
            light_uniforms.extra_light_count = static_cast<std::uint32_t>(slot + 1);
        } else {
            // Fold overflow directional lights into the last extra lobe so >4 lights still contribute illumination.
            auto& last = light_uniforms.extra_lights[DirectionalLightUniforms::kMaxExtraLights - 1];
            const float w_prev = last.radiance.length();
            const float w_add = rad.length();
            if ((w_prev + w_add) > math::kStrictEpsilon) {
                last.direction_to_light =
                    (last.direction_to_light * w_prev + dir_to_light * w_add).normalized();
            }
            last.radiance += rad;
        }
        ++active_light_count;
    }

    if (scene_has_any_light_component && active_light_count == 0) {
        light_uniforms.radiance = math::Vec3::zero();
    }

    struct TranslucentDrawItem {
        DrawMeshCommand cmd;
        float dist_sq{0.0f};
    };
    std::vector<TranslucentDrawItem> translucent_queue;

    for (const auto& obj : scene.objects()) {
        if (!obj.mesh_id.has_value() || !isObjectEffectivelyVisible(scene, obj.id)) {
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

        const scene::PbrMaterial* obj_mat =
            obj.material_id.has_value() ? scene.findMaterial(obj.material_id.value()) : nullptr;
        const bool is_sel = (selected_object.isValid() && obj.id == selected_object);
        const math::Vec3 world_center = world_res.value().transformPoint(mesh_asset->local_bounds.center());
        const float cam_dist_sq = (world_center - camera_pos).lengthSquared();

        auto submitSubmesh = [&](std::uint32_t offset, std::uint32_t count, const scene::PbrMaterial* mat) -> core::Status {
            DrawMeshCommand cmd{
                .mesh_handle = gpu_handle_res.value(),
                .model_matrix = world_res.value(),
                .view_projection_matrix = view_proj,
                .camera_position = camera_pos,
                .lighting = light_uniforms,
                .selected = is_sel,
                .surface = surfaceFromMaterial(mat),
                .index_offset = offset,
                .index_count = count,
            };
            if (cmd.surface.opacity < 0.999f) {
                translucent_queue.push_back(TranslucentDrawItem{std::move(cmd), cam_dist_sq});
                return core::Status::ok();
            }
            return device_->drawMesh(cmd);
        };

        if (mesh_asset->data.submeshes.empty()) {
            auto st = submitSubmesh(0, 0, obj_mat);
            if (!st) {
                (void)device_->endFrame();
                return st;
            }
        } else {
            for (const auto& sm : mesh_asset->data.submeshes) {
                const scene::PbrMaterial* face_mat =
                    sm.material_id.isValid() ? scene.findMaterial(sm.material_id) : nullptr;
                const scene::PbrMaterial* resolved_mat = (face_mat != nullptr) ? face_mat : obj_mat;
                auto st = submitSubmesh(sm.index_offset, sm.index_count, resolved_mat);
                if (!st) {
                    (void)device_->endFrame();
                    return st;
                }
            }
        }
    }

    // Dedicated viewport-only procedural XZ ground grid pass (Y = 0).
    // Never creates SceneObjects or mesh assets; depth-tested against opaque scene meshes without
    // modifying the depth buffer; omitted completely when grid_options == nullptr (final scene renders).
    if (grid_options != nullptr) {
        const auto grid_cmd = buildEditorGridCommand(
            view_matrix,
            proj_matrix,
            camera_pos,
            device_->capabilities().clip_space_depth,
            *grid_options);
        if (grid_cmd.has_value()) {
            auto grid_st = device_->drawEditorGrid(*grid_cmd);
            if (!grid_st) {
                (void)device_->endFrame();
                return grid_st;
            }
        }
    }

    // Sort translucent submeshes back-to-front relative to active camera.
    if (!translucent_queue.empty()) {
        std::stable_sort(
            translucent_queue.begin(),
            translucent_queue.end(),
            [](const TranslucentDrawItem& a, const TranslucentDrawItem& b) {
                return a.dist_sq > b.dist_sq;
            });
        for (const auto& item : translucent_queue) {
            auto st = device_->drawMesh(item.cmd);
            if (!st) {
                (void)device_->endFrame();
                return st;
            }
        }
    }

    // Pass 1 of Overlays: Scene-depth-tested 3D editor helpers (Camera bodies/frustums & Light gizmos).
    // Drawn while the scene depth buffer is still intact so foreground scene meshes occlude helpers properly.
    std::size_t overlay_slot = 0;
    auto submitOverlayItem = [&](const ViewportOverlayMesh& ov) -> core::Status {
        auto ov_handle = uploadOverlaySlot(overlay_slot++, *ov.mesh_data);
        if (!ov_handle) {
            return ov_handle.error();
        }
        DrawMeshCommand ov_cmd{
            .mesh_handle = ov_handle.value(),
            .model_matrix = ov.model_matrix,
            .view_projection_matrix = view_proj,
            .camera_position = camera_pos,
            .lighting = light_uniforms,
            .selected = false,
            .surface = PbrSurfaceParams{
                .base_color = {1.0f, 1.0f, 1.0f},
                .opacity = 1.0f,
                .vertex_color_weight = 1.0f,
                .unlit_overlay = true,
            },
        };
        return device_->drawMesh(ov_cmd);
    };

    for (const auto& ov : overlays) {
        if (!ov.depth_test_scene || ov.mesh_data == nullptr ||
            ov.mesh_data->vertices.empty() || ov.mesh_data->indices.empty()) {
            continue;
        }
        auto draw_st = submitOverlayItem(ov);
        if (!draw_st) {
            (void)device_->endFrame();
            return draw_st;
        }
    }

    // Pass 2 of Overlays: Foreground X-ray manipulators (transform gizmos + mesh edit overlays) over cleared depth.
    bool began_overlay = false;
    for (const auto& ov : overlays) {
        if (ov.depth_test_scene || ov.mesh_data == nullptr ||
            ov.mesh_data->vertices.empty() || ov.mesh_data->indices.empty()) {
            continue;
        }
        if (!began_overlay) {
            auto ov_st = device_->beginOverlayPass(1.0f);
            if (!ov_st) {
                (void)device_->endFrame();
                return ov_st;
            }
            began_overlay = true;
        }
        auto draw_st = submitOverlayItem(ov);
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
        stats.gpu_buffer_update_count = device_->bufferUpdateCount();
    }
    return stats;
}

} // namespace finaleffect::rendering
