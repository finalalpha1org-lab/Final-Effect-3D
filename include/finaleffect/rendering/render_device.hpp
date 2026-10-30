// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/rendering/render_types.hpp"
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
};

} // namespace finaleffect::rendering
