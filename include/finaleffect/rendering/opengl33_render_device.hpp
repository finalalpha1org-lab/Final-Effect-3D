// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/platform/window.hpp"
#include "finaleffect/rendering/render_device.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace finaleffect::rendering {

// Desktop OpenGL 3.3 Core Profile GPU render device.
// Kept in a dedicated optional target (finaleffect_rendering_opengl33) so portable
// core targets remain completely independent of graphics APIs.
class OpenGL33RenderDevice final : public IRenderDevice {
public:
    explicit OpenGL33RenderDevice(platform::GlProcLoaderFn proc_loader);
    ~OpenGL33RenderDevice() override;

    [[nodiscard]] core::Status initialize(std::uint32_t width, std::uint32_t height) override;
    void shutdown() noexcept override;

    [[nodiscard]] core::Status resize(std::uint32_t width, std::uint32_t height) override;
    [[nodiscard]] const DeviceCapabilities& capabilities() const noexcept override { return capabilities_; }

    [[nodiscard]] core::Result<GpuMeshHandle> createMesh(const scene::MeshData& mesh) override;
    [[nodiscard]] core::Status updateMesh(GpuMeshHandle handle, const scene::MeshData& mesh) override;
    void destroyMesh(GpuMeshHandle handle) noexcept override;

    [[nodiscard]] core::Status syncEnvironmentMap(
        const scene::HdriEnvironmentAsset* asset,
        std::uint64_t revision) override;
    [[nodiscard]] core::Status ensureShadowAtlas(std::uint32_t atlas_size) override;
    [[nodiscard]] core::Status beginShadowTilePass(
        std::uint32_t tile_x,
        std::uint32_t tile_y,
        std::uint32_t tile_width,
        std::uint32_t tile_height) override;
    [[nodiscard]] core::Status drawShadowMesh(
        GpuMeshHandle mesh_handle,
        const math::Mat4& light_mvp,
        std::uint32_t index_offset = 0,
        std::uint32_t index_count = 0) override;
    [[nodiscard]] core::Status endShadowPass() override;

    [[nodiscard]] core::Status beginFrame(const FrameClearDesc& clear) override;
    [[nodiscard]] core::Status drawEditorGrid(const EditorGridCommand& command) override;
    [[nodiscard]] core::Status beginOverlayPass(float clear_depth = 1.0f) override;
    [[nodiscard]] core::Status drawMesh(const DrawMeshCommand& command) override;
    [[nodiscard]] core::Status endFrame() override;

    [[nodiscard]] std::size_t liveMeshCount() const noexcept override { return meshes_.size(); }
    [[nodiscard]] std::size_t liveMeshBytes() const noexcept override { return total_gpu_bytes_; }
    [[nodiscard]] std::size_t bufferUpdateCount() const noexcept override { return buffer_updates_; }
    [[nodiscard]] std::size_t liveShadowBytes() const noexcept override { return shadow_gpu_bytes_; }
    [[nodiscard]] std::size_t liveEnvironmentBytes() const noexcept override { return environment_gpu_bytes_; }

private:
    struct GlDispatchTable;
    struct GpuMeshRecord {
        std::uint32_t vao{0};
        std::uint32_t vbo{0};
        std::uint32_t ebo{0};
        std::int32_t index_count{0};
        std::size_t byte_size{0};
        std::size_t vbo_capacity_bytes{0};
        std::size_t ebo_capacity_bytes{0};
    };

    [[nodiscard]] core::Status buildShaderPipeline();
    [[nodiscard]] core::Status ensureGridPipeline();
    [[nodiscard]] core::Status ensureShadowProgram();
    void ensureFallbackTextures() noexcept;

    platform::GlProcLoaderFn proc_loader_{nullptr};
    std::unique_ptr<GlDispatchTable> gl_;
    DeviceCapabilities capabilities_;
    bool initialized_{false};
    bool in_frame_{false};
    bool in_shadow_pass_{false};
    std::uint32_t width_{0};
    std::uint32_t height_{0};

    std::uint32_t shader_program_{0};
    std::int32_t loc_model_{-1};
    std::int32_t loc_normal_mat_{-1};
    std::int32_t loc_view_proj_{-1};
    std::int32_t loc_camera_pos_{-1};
    std::int32_t loc_light_dir_{-1};
    std::int32_t loc_light_radiance_{-1};
    std::int32_t loc_extra_light_dirs_{-1};
    std::int32_t loc_extra_light_radiances_{-1};
    std::int32_t loc_ambient_{-1};
    std::int32_t loc_selection_tint_{-1};
    std::int32_t loc_base_color_{-1};
    std::int32_t loc_pbr_params_{-1};
    std::int32_t loc_emissive_radiance_{-1};
    std::int32_t loc_surface_flags_{-1};

    // Stage 5.4 unified multi-light, shadow atlas, light-linking, and SH9 + Cubemap IBL uniforms.
    std::int32_t loc_light_meta_{-1};
    std::int32_t loc_light_mask_{-1};
    std::int32_t loc_light_pos_type_{-1};
    std::int32_t loc_light_dir_range_{-1};
    std::int32_t loc_light_radiance_atten_{-1};
    std::int32_t loc_light_shape_{-1};
    std::int32_t loc_light_tangent_shadow_{-1};
    std::int32_t loc_light_shadow_params_{-1};
    std::int32_t loc_shadow_2d_vp_{-1};
    std::int32_t loc_shadow_2d_rect_{-1};
    std::int32_t loc_point_shadow_params_{-1};
    std::int32_t loc_point_shadow_far_{-1};
    std::int32_t loc_env_params_{-1};
    std::int32_t loc_env_rot_scales_{-1};
    std::int32_t loc_overflow_irradiance_{-1};
    std::int32_t loc_ibl_sh_{-1};
    std::int32_t loc_shadow_atlas_{-1};
    std::int32_t loc_ibl_cubemap_{-1};

    // Real-time 2D Depth Shadow Atlas & prefiltered RGBA16F IBL cubemap GPU resources.
    std::uint32_t shadow_program_{0};
    std::int32_t loc_shadow_mvp_{-1};
    std::uint32_t shadow_fbo_{0};
    std::uint32_t shadow_depth_tex_{0};
    std::uint32_t shadow_atlas_size_{0};
    std::size_t shadow_gpu_bytes_{0};

    std::uint32_t ibl_cubemap_tex_{0};
    std::uint64_t environment_revision_{0};
    std::size_t environment_gpu_bytes_{0};

    std::uint32_t fallback_tex_2d_{0};
    std::uint32_t fallback_cubemap_{0};
    const PreparedSceneLighting* uploaded_scene_lighting_{nullptr};

    // Dedicated viewport-only procedural XZ ground grid GPU program & clip-space fullscreen pass.
    std::uint32_t grid_program_{0};
    std::uint32_t grid_vao_{0};
    std::uint32_t grid_vbo_{0};
    std::uint32_t grid_ebo_{0};
    std::int32_t loc_grid_cam_pos_{-1};
    std::int32_t loc_grid_cam_right_{-1};
    std::int32_t loc_grid_cam_up_{-1};
    std::int32_t loc_grid_cam_fwd_{-1};
    std::int32_t loc_grid_frustum_{-1};
    std::int32_t loc_grid_depth_{-1};
    std::int32_t loc_grid_steps_{-1};
    std::int32_t loc_grid_fade_{-1};
    std::int32_t loc_grid_phase_x_{-1};
    std::int32_t loc_grid_phase_z_{-1};
    std::int32_t loc_grid_axis_px_{-1};
    std::int32_t loc_grid_ndc_step_{-1};

    std::uint64_t next_handle_{1};
    std::unordered_map<std::uint64_t, GpuMeshRecord> meshes_;
    std::size_t total_gpu_bytes_{0};
    std::size_t buffer_updates_{0};
};

} // namespace finaleffect::rendering
