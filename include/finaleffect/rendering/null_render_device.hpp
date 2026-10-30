// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/rendering/render_device.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace finaleffect::rendering {

class NullRenderDevice final : public IRenderDevice {
public:
    NullRenderDevice();
    ~NullRenderDevice() override;

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

    [[nodiscard]] std::size_t liveMeshCount() const noexcept override { return live_meshes_.size(); }
    [[nodiscard]] std::size_t liveMeshBytes() const noexcept override { return total_bytes_; }
    [[nodiscard]] std::size_t bufferUpdateCount() const noexcept override { return buffer_updates_; }
    [[nodiscard]] std::size_t liveShadowBytes() const noexcept override { return shadow_bytes_; }
    [[nodiscard]] std::size_t liveEnvironmentBytes() const noexcept override { return environment_bytes_; }

    [[nodiscard]] std::uint64_t completedFrames() const noexcept { return completed_frames_; }
    [[nodiscard]] std::uint64_t totalDrawCalls() const noexcept { return total_draw_calls_; }
    [[nodiscard]] std::uint64_t totalShadowDrawCalls() const noexcept { return total_shadow_draw_calls_; }
    [[nodiscard]] std::uint64_t totalGridPasses() const noexcept { return total_grid_passes_; }
    [[nodiscard]] std::uint32_t viewportWidth() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t viewportHeight() const noexcept { return height_; }

private:
    DeviceCapabilities capabilities_;
    bool initialized_{false};
    bool in_frame_{false};
    bool in_shadow_pass_{false};
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::uint64_t next_handle_{1};
    std::unordered_map<std::uint64_t, std::size_t> live_meshes_;
    std::size_t total_bytes_{0};
    std::size_t buffer_updates_{0};
    std::size_t shadow_bytes_{0};
    std::size_t environment_bytes_{0};
    std::uint64_t environment_revision_{0};
    std::uint64_t completed_frames_{0};
    std::uint64_t total_draw_calls_{0};
    std::uint64_t total_shadow_draw_calls_{0};
    std::uint64_t total_grid_passes_{0};
};

} // namespace finaleffect::rendering
