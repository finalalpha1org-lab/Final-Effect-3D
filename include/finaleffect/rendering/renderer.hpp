// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/resource_stats.hpp"
#include "finaleffect/core/result.hpp"
#include "finaleffect/rendering/lighting_system.hpp"
#include "finaleffect/rendering/render_device.hpp"
#include "finaleffect/scene/object_id.hpp"
#include "finaleffect/scene/scene.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace finaleffect::rendering {

struct ViewportOverlayMesh {
    const scene::MeshData* mesh_data{nullptr};
    math::Mat4 model_matrix{math::Mat4::identity()};
    // When true, draws in the scene depth buffer BEFORE depth is cleared for foreground X-ray gizmos.
    bool depth_test_scene{false};
};

struct ViewportCameraOverride {
    math::Mat4 camera_world{math::Mat4::identity()};
    scene::CameraComponent camera{};
};

// High-level portable renderer: bridges Scene data, PBR materials, submeshes, and
// 3D viewport overlays to an abstract IRenderDevice without leaking GPU handles into Scene.
class Renderer {
public:
    explicit Renderer(std::unique_ptr<IRenderDevice> device);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    [[nodiscard]] core::Status initialize(std::uint32_t width, std::uint32_t height);
    void shutdown() noexcept;

    [[nodiscard]] core::Status resize(std::uint32_t width, std::uint32_t height);
    [[nodiscard]] core::Status renderScene(
        const scene::Scene& scene,
        const FrameClearDesc& clear = {},
        scene::ObjectId selected_object = {},
        std::span<const ViewportOverlayMesh> overlays = {},
        const ViewportCameraOverride* camera_override = nullptr,
        const ViewportGridOptions* grid_options = nullptr);

    void releaseSceneResources() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }
    [[nodiscard]] IRenderDevice& device() noexcept { return *device_; }
    [[nodiscard]] const IRenderDevice& device() const noexcept { return *device_; }

    [[nodiscard]] core::ResourceStats collectResourceStats(const scene::Scene& scene) const noexcept;
    [[nodiscard]] const LightingDiagnostics& lastLightingDiagnostics() const noexcept {
        return last_lighting_diagnostics_;
    }
    void invalidateShadowCache() noexcept { cached_shadow_tile_signatures_.clear(); }

private:
    struct CachedGpuMesh {
        GpuMeshHandle handle{GpuMeshHandle::invalid()};
        std::uint64_t revision{0};
    };

    [[nodiscard]] core::Result<GpuMeshHandle> ensureMeshUploaded(const scene::MeshAsset& asset);
    [[nodiscard]] core::Result<GpuMeshHandle> uploadOverlaySlot(std::size_t slot_index, const scene::MeshData& mesh);

    std::unique_ptr<IRenderDevice> device_;
    bool initialized_{false};
    std::uint32_t viewport_width_{0};
    std::uint32_t viewport_height_{0};
    std::unordered_map<scene::MeshId, CachedGpuMesh> gpu_mesh_cache_;
    std::vector<GpuMeshHandle> overlay_gpu_pool_;
    std::uint32_t cached_shadow_atlas_size_{0};
    std::unordered_map<std::uint32_t, std::uint64_t> cached_shadow_tile_signatures_;
    LightingDiagnostics last_lighting_diagnostics_{};
};

} // namespace finaleffect::rendering
