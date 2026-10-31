// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/rendering/renderer.hpp"
#include "finaleffect/rendering/editor_grid.hpp"

#include <algorithm>
#include <chrono>
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

DirectionalLightUniforms buildLegacyLightUniforms(const PreparedSceneLighting& prepared) noexcept {
    DirectionalLightUniforms u{};
    u.ambient = prepared.environment.enabled
                    ? (prepared.environment.fallback_ambient * prepared.environment.intensity)
                    : math::Vec3::zero();
    if (prepared.light_count == 0u) {
        u.direction_to_light = {0.0f, 1.0f, 0.0f};
        u.radiance = {0.0f, 0.0f, 0.0f};
        return u;
    }
    bool has_primary = false;
    for (std::uint32_t i = 0; i < prepared.light_count; ++i) {
        const auto& l = prepared.lights[i];
        const math::Vec3 dir_to = -l.direction;
        if (!has_primary) {
            u.direction_to_light = dir_to;
            u.radiance = l.radiance;
            has_primary = true;
        } else if (u.extra_light_count < DirectionalLightUniforms::kMaxExtraLights) {
            u.extra_lights[u.extra_light_count++] = DirectionalLightLobe{
                .direction_to_light = dir_to,
                .radiance = l.radiance,
            };
        }
    }
    return u;
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
        cached_shadow_tile_signatures_.clear();
        cached_shadow_atlas_size_ = 0u;
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
    cached_shadow_tile_signatures_.clear();
    cached_shadow_atlas_size_ = 0u;
    (void)device_->syncEnvironmentMap(nullptr, 0u);
    (void)device_->ensureShadowAtlas(0u);
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

    const auto frame_start = std::chrono::steady_clock::now();

    // Minimized window (0x0 framebuffer): complete frame cleanly without projecting 0-aspect camera.
    if (viewport_width_ == 0 || viewport_height_ == 0) {
        auto begin_st = device_->beginFrame(clear);
        if (!begin_st) {
            return begin_st;
        }
        return device_->endFrame();
    }

    PreparedSceneLighting scene_lighting = prepareSceneLighting(scene);

    // 1. Synchronize HDRI prefiltered cubemap resource (cached by revision; zero upload when unchanged).
    if (auto env_st = device_->syncEnvironmentMap(
            scene_lighting.environment.hdri_asset,
            scene_lighting.environment.hdri_revision);
        !env_st) {
        return env_st;
    }

    // 2. Render ONLY dirty Shadow Atlas tiles BEFORE binding/clearing the main viewport framebuffer.
    if (!scene_lighting.shadow_tiles.empty()) {
        if (scene_lighting.shadow_atlas_size != cached_shadow_atlas_size_) {
            if (auto st = device_->ensureShadowAtlas(scene_lighting.shadow_atlas_size); !st) {
                return st;
            }
            cached_shadow_atlas_size_ = scene_lighting.shadow_atlas_size;
            cached_shadow_tile_signatures_.clear();
        }

        for (auto it = cached_shadow_tile_signatures_.begin(); it != cached_shadow_tile_signatures_.end();) {
            const bool active = std::any_of(
                scene_lighting.shadow_tiles.begin(),
                scene_lighting.shadow_tiles.end(),
                [&](const ShadowTileDescriptor& t) { return t.tile_id == it->first; });
            if (!active) {
                it = cached_shadow_tile_signatures_.erase(it);
            } else {
                ++it;
            }
        }

        bool rendered_any_tile = false;
        for (const auto& tile : scene_lighting.shadow_tiles) {
            const auto sig_it = cached_shadow_tile_signatures_.find(tile.tile_id);
            if (sig_it != cached_shadow_tile_signatures_.end() &&
                sig_it->second == tile.caster_signature) {
                ++scene_lighting.diagnostics.shadow_tiles_cached_last_frame;
                continue;
            }

            if (auto st = device_->beginShadowTilePass(tile.x, tile.y, tile.width, tile.height); !st) {
                return st;
            }
            rendered_any_tile = true;

            for (const auto& caster : tile.casters) {
                const auto* caster_mesh = scene.findMesh(caster.mesh_id);
                if (caster_mesh == nullptr) {
                    continue;
                }
                auto handle_res = ensureMeshUploaded(*caster_mesh);
                if (!handle_res) {
                    (void)device_->endShadowPass();
                    return handle_res.error();
                }
                if (auto st = device_->drawShadowMesh(
                        handle_res.value(),
                        tile.light_view_proj * caster.world_matrix,
                        0u,
                        0u);
                    !st) {
                    (void)device_->endShadowPass();
                    return st;
                }
            }

            cached_shadow_tile_signatures_[tile.tile_id] = tile.caster_signature;
            ++scene_lighting.diagnostics.shadow_tiles_rendered_last_frame;
        }

        if (rendered_any_tile) {
            if (auto st = device_->endShadowPass(); !st) {
                return st;
            }
        }
    } else if (cached_shadow_atlas_size_ != 0u) {
        (void)device_->ensureShadowAtlas(0u);
        cached_shadow_atlas_size_ = 0u;
        cached_shadow_tile_signatures_.clear();
    }

    // 3. Main Viewport Frame Pass.
    auto begin_st = device_->beginFrame(clear);
    if (!begin_st) {
        return begin_st;
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
    const DirectionalLightUniforms light_uniforms = buildLegacyLightUniforms(scene_lighting);

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
        const std::uint32_t obj_light_mask = scene_lighting.computeObjectLightMask(scene, obj.id);

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
                .scene_lighting = &scene_lighting,
                .light_mask = obj_light_mask,
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

    auto end_st = device_->endFrame();
    const auto frame_end = std::chrono::steady_clock::now();
    scene_lighting.diagnostics.shadow_gpu_bytes = device_->liveShadowBytes();
    scene_lighting.diagnostics.environment_gpu_bytes = device_->liveEnvironmentBytes();
    scene_lighting.diagnostics.last_frame_render_ms =
        std::chrono::duration<double, std::milli>(frame_end - frame_start).count();
    last_lighting_diagnostics_ = std::move(scene_lighting.diagnostics);
    return end_st;
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
