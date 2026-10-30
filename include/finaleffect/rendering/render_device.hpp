// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/rendering/render_types.hpp"
#include "finaleffect/scene/environment.hpp"
#include "finaleffect/scene/mesh.hpp"

#include <cstddef>
#include <cstdint>

namespace finaleffect::rendering {

// Graphics-API-independent render device boundary.
class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    [[nodiscard]] virtual core::Status initialize(std::uint32_t width, std::uint32_t height) = 0;
    virtual void shutdown() noexcept = 0;

    [[nodiscard]] virtual core::Status resize(std::uint32_t width, std::uint32_t height) = 0;
    [[nodiscard]] virtual const DeviceCapabilities& capabilities() const noexcept = 0;

    [[nodiscard]] virtual core::Result<GpuMeshHandle> createMesh(const scene::MeshData& mesh) = 0;
    [[nodiscard]] virtual core::Status updateMesh(GpuMeshHandle handle, const scene::MeshData& mesh) = 0;
    virtual void destroyMesh(GpuMeshHandle handle) noexcept = 0;

    // Synchronizes the active prefiltered HDRI cubemap resource when `revision` changes (or releases when null).
    [[nodiscard]] virtual core::Status syncEnvironmentMap(
        const scene::HdriEnvironmentAsset* asset,
        std::uint64_t revision) {
        (void)asset;
        (void)revision;
        return core::Status::ok();
    }

    // Real-time Shadow Atlas pass hooks (invoked BEFORE beginFrame only for dirty shadow tiles).
    [[nodiscard]] virtual core::Status ensureShadowAtlas(std::uint32_t atlas_size) {
        (void)atlas_size;
        return core::Status::ok();
    }
    [[nodiscard]] virtual core::Status beginShadowTilePass(
        std::uint32_t tile_x,
        std::uint32_t tile_y,
        std::uint32_t tile_width,
        std::uint32_t tile_height) {
        (void)tile_x;
        (void)tile_y;
        (void)tile_width;
        (void)tile_height;
        return core::Status::ok();
    }
    [[nodiscard]] virtual core::Status drawShadowMesh(
        GpuMeshHandle mesh_handle,
        const math::Mat4& light_mvp,
        std::uint32_t index_offset = 0,
        std::uint32_t index_count = 0) {
        (void)mesh_handle;
        (void)light_mvp;
        (void)index_offset;
        (void)index_count;
        return core::Status::ok();
    }
    [[nodiscard]] virtual core::Status endShadowPass() { return core::Status::ok(); }

    [[nodiscard]] virtual core::Status beginFrame(const FrameClearDesc& clear) = 0;
    // Dedicated viewport-only procedural ground grid pass (depth-tested against opaque scene geometry, non-occluding).
    [[nodiscard]] virtual core::Status drawEditorGrid(const EditorGridCommand& command) = 0;
    // Clears depth buffer only so 3D gizmos & edit-mode cages render cleanly over scene geometry.
    [[nodiscard]] virtual core::Status beginOverlayPass(float clear_depth = 1.0f) = 0;
    [[nodiscard]] virtual core::Status drawMesh(const DrawMeshCommand& command) = 0;
    [[nodiscard]] virtual core::Status endFrame() = 0;

    [[nodiscard]] virtual std::size_t liveMeshCount() const noexcept = 0;
    [[nodiscard]] virtual std::size_t liveMeshBytes() const noexcept = 0;
    [[nodiscard]] virtual std::size_t bufferUpdateCount() const noexcept { return 0; }
    [[nodiscard]] virtual std::size_t liveShadowBytes() const noexcept { return 0; }
    [[nodiscard]] virtual std::size_t liveEnvironmentBytes() const noexcept { return 0; }
};

} // namespace finaleffect::rendering
