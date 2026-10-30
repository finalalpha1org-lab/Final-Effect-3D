// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/rendering/null_render_device.hpp"

namespace finaleffect::rendering {

NullRenderDevice::NullRenderDevice() {
    capabilities_.backend = BackendType::Null;
    capabilities_.backend_name = "Null";
    capabilities_.device_name = "Final Effect Null Validation Device";
    capabilities_.clip_space_depth = math::ClipSpaceDepth::NegativeOneToOne;
    capabilities_.max_viewport_dimension = 16384;
}

NullRenderDevice::~NullRenderDevice() {
    shutdown();
}

core::Status NullRenderDevice::initialize(std::uint32_t width, std::uint32_t height) {
    if (initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "NullRenderDevice already initialized");
    }
    if (width == 0 || height == 0 ||
        width > capabilities_.max_viewport_dimension ||
        height > capabilities_.max_viewport_dimension) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid initial viewport dimensions");
    }
    width_ = width;
    height_ = height;
    initialized_ = true;
    in_frame_ = false;
    return core::Status::ok();
}

void NullRenderDevice::shutdown() noexcept {
    live_meshes_.clear();
    total_bytes_ = 0;
    buffer_updates_ = 0;
    in_frame_ = false;
    initialized_ = false;
}

core::Status NullRenderDevice::resize(std::uint32_t width, std::uint32_t height) {
    if (!initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "NullRenderDevice not initialized");
    }
    if (width > capabilities_.max_viewport_dimension || height > capabilities_.max_viewport_dimension) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Viewport resize exceeds device limits");
    }
    width_ = width;
    height_ = height;
    return core::Status::ok();
}

core::Result<GpuMeshHandle> NullRenderDevice::createMesh(const scene::MeshData& mesh) {
    if (!initialized_) {
        return core::Result<GpuMeshHandle>::fail(core::ErrorCode::InvalidState, "Device not initialized");
    }
    auto valid = mesh.validate();
    if (!valid) {
        return valid.error();
    }
    const std::uint64_t id = next_handle_++;
    const std::size_t bytes = mesh.byteSize();
    live_meshes_.emplace(id, bytes);
    total_bytes_ += bytes;
    return core::Result<GpuMeshHandle>::ok(GpuMeshHandle{id});
}

core::Status NullRenderDevice::updateMesh(GpuMeshHandle handle, const scene::MeshData& mesh) {
    if (!initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Device not initialized");
    }
    const auto it = live_meshes_.find(handle.value);
    if (it == live_meshes_.end()) {
        return core::Status::fail(core::ErrorCode::NotFound, "updateMesh referenced unknown GpuMeshHandle");
    }
    auto valid = mesh.validate();
    if (!valid) {
        return valid;
    }
    const std::size_t new_bytes = mesh.byteSize();
    total_bytes_ = (total_bytes_ - it->second) + new_bytes;
    it->second = new_bytes;
    ++buffer_updates_;
    return core::Status::ok();
}

void NullRenderDevice::destroyMesh(GpuMeshHandle handle) noexcept {
    const auto it = live_meshes_.find(handle.value);
    if (it != live_meshes_.end()) {
        total_bytes_ -= it->second;
        live_meshes_.erase(it);
    }
}

core::Status NullRenderDevice::beginFrame(const FrameClearDesc&) {
    if (!initialized_ || in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Invalid beginFrame state");
    }
    in_frame_ = true;
    return core::Status::ok();
}

core::Status NullRenderDevice::drawEditorGrid(const EditorGridCommand&) {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "drawEditorGrid called outside active frame");
    }
    if (width_ == 0 || height_ == 0) {
        return core::Status::ok();
    }
    ++total_grid_passes_;
    return core::Status::ok();
}

core::Status NullRenderDevice::beginOverlayPass(float) {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "beginOverlayPass called outside active frame");
    }
    return core::Status::ok();
}

core::Status NullRenderDevice::drawMesh(const DrawMeshCommand& command) {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "drawMesh called outside active frame");
    }
    if (!live_meshes_.contains(command.mesh_handle.value)) {
        return core::Status::fail(core::ErrorCode::NotFound, "drawMesh referenced unknown GpuMeshHandle");
    }
    if (width_ == 0 || height_ == 0) {
        return core::Status::ok(); // Minimized surface safely skips draw.
    }
    ++total_draw_calls_;
    return core::Status::ok();
}

core::Status NullRenderDevice::endFrame() {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "endFrame called without matching beginFrame");
    }
    in_frame_ = false;
    ++completed_frames_;
    return core::Status::ok();
}

} // namespace finaleffect::rendering
