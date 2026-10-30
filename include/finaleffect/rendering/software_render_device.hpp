// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/rendering/render_device.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace finaleffect::rendering {

struct Rgba8 {
    std::uint8_t r{0};
    std::uint8_t g{0};
    std::uint8_t b{0};
    std::uint8_t a{255};

    constexpr bool operator==(const Rgba8&) const noexcept = default;
};

// Portable CPU reference 3D rasterizer (RGBA8 color + float32 depth buffer).
// Enables deterministic 3D rendering verification in headless/CI environments
// without requiring GPU drivers or a window server.
class SoftwareRenderDevice final : public IRenderDevice {
public:
    SoftwareRenderDevice();
    ~SoftwareRenderDevice() override;

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
    [[nodiscard]] std::size_t liveMeshBytes() const noexcept override { return total_mesh_bytes_; }
    [[nodiscard]] std::size_t bufferUpdateCount() const noexcept override { return buffer_updates_; }

    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] Rgba8 pixelAt(std::uint32_t x, std::uint32_t y) const noexcept;
    [[nodiscard]] const std::vector<Rgba8>& colorBuffer() const noexcept { return color_buffer_; }
    [[nodiscard]] std::string encodePpm() const;

private:
    DeviceCapabilities capabilities_;
    bool initialized_{false};
    bool in_frame_{false};
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::vector<Rgba8> color_buffer_;
    std::vector<float> depth_buffer_;
    std::uint64_t next_handle_{1};
    std::unordered_map<std::uint64_t, scene::MeshData> meshes_;
    std::size_t total_mesh_bytes_{0};
    std::size_t buffer_updates_{0};
};

} // namespace finaleffect::rendering
