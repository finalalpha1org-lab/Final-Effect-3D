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

// WebGL 2.0 / OpenGL ES 3.0 GPU render device (GLSL ES 3.00 shaders, glClearDepthf).
// Uses dynamic GlProcLoaderFn symbol resolution so the adapter compiles and runs unit
// verification on desktop hosts as well as inside Emscripten WebAssembly builds.
class WebGL2RenderDevice final : public IRenderDevice {
public:
    explicit WebGL2RenderDevice(platform::GlProcLoaderFn proc_loader);
    ~WebGL2RenderDevice() override;

    [[nodiscard]] core::Status initialize(std::uint32_t width, std::uint32_t height) override;
    void shutdown() noexcept override;

    [[nodiscard]] core::Status resize(std::uint32_t width, std::uint32_t height) override;
    [[nodiscard]] const DeviceCapabilities& capabilities() const noexcept override { return capabilities_; }

    [[nodiscard]] core::Result<GpuMeshHandle> createMesh(const scene::MeshData& mesh) override;
    [[nodiscard]] core::Status updateMesh(GpuMeshHandle handle, const scene::MeshData& mesh) override;
    void destroyMesh(GpuMeshHandle handle) noexcept override;

    [[nodiscard]] core::Status beginFrame(const FrameClearDesc& clear) override;
    [[nodiscard]] core::Status drawEditorGrid(const EditorGridCommand& command) override;
    [[nodiscard]] core::Status beginOverlayPass(float clear_depth = 1.0f) override;
    [[nodiscard]] core::Status drawMesh(const DrawMeshCommand& command) override;
    [[nodiscard]] core::Status endFrame() override;

    [[nodiscard]] std::size_t liveMeshCount() const noexcept override { return meshes_.size(); }
    [[nodiscard]] std::size_t liveMeshBytes() const noexcept override { return total_gpu_bytes_; }
    [[nodiscard]] std::size_t bufferUpdateCount() const noexcept override { return buffer_updates_; }

private:
    struct GlEs3DispatchTable;
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

    platform::GlProcLoaderFn proc_loader_{nullptr};
    std::unique_ptr<GlEs3DispatchTable> gl_;
    DeviceCapabilities capabilities_;
    bool initialized_{false};
    bool in_frame_{false};
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
