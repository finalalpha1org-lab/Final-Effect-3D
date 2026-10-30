// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/resource_stats.hpp"
#include "finaleffect/core/result.hpp"
#include "finaleffect/rendering/render_device.hpp"
#include "finaleffect/scene/object_id.hpp"
#include "finaleffect/scene/scene.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace finaleffect::rendering {

// High-level portable renderer: bridges Scene data to an abstract IRenderDevice
// while keeping GPU/backend mesh handles completely separate from Scene objects.
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
        scene::ObjectId selected_object = {});

    void releaseSceneResources() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }
    [[nodiscard]] IRenderDevice& device() noexcept { return *device_; }
    [[nodiscard]] const IRenderDevice& device() const noexcept { return *device_; }

    [[nodiscard]] core::ResourceStats collectResourceStats(const scene::Scene& scene) const noexcept;

private:
    struct CachedGpuMesh {
        GpuMeshHandle handle{GpuMeshHandle::invalid()};
        std::uint64_t revision{0};
    };

    [[nodiscard]] core::Result<GpuMeshHandle> ensureMeshUploaded(const scene::MeshAsset& asset);

    std::unique_ptr<IRenderDevice> device_;
    bool initialized_{false};
    std::uint32_t viewport_width_{0};
    std::uint32_t viewport_height_{0};
    std::unordered_map<scene::MeshId, CachedGpuMesh> gpu_mesh_cache_;
};

} // namespace finaleffect::rendering
