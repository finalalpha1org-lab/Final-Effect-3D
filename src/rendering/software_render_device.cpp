// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/rendering/software_render_device.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/rendering/editor_grid.hpp"
#include "finaleffect/rendering/lighting_system.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <sstream>

namespace finaleffect::rendering {

namespace {

std::uint8_t floatToByte(float v) noexcept {
    const float clamped = std::clamp(v, 0.0f, 1.0f);
    return static_cast<std::uint8_t>(std::lround(clamped * 255.0f));
}

float edgeFunction(float ax, float ay, float bx, float by, float px, float py) noexcept {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

} // namespace

SoftwareRenderDevice::SoftwareRenderDevice() {
    capabilities_.backend = BackendType::Software;
    capabilities_.backend_name = "Software";
    capabilities_.device_name = "Final Effect CPU Reference Rasterizer";
    capabilities_.clip_space_depth = math::ClipSpaceDepth::NegativeOneToOne;
    capabilities_.max_viewport_dimension = 4096;
}

SoftwareRenderDevice::~SoftwareRenderDevice() {
    shutdown();
}

core::Status SoftwareRenderDevice::initialize(std::uint32_t width, std::uint32_t height) {
    if (initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "SoftwareRenderDevice already initialized");
    }
    if (width == 0 || height == 0 ||
        width > capabilities_.max_viewport_dimension ||
        height > capabilities_.max_viewport_dimension) {
        return core::Status::fail(
            core::ErrorCode::InvalidArgument,
            "Invalid initial SoftwareRenderDevice framebuffer dimensions");
    }
    initialized_ = true;
    return resize(width, height);
}

void SoftwareRenderDevice::shutdown() noexcept {
    meshes_.clear();
    color_buffer_.clear();
    depth_buffer_.clear();
    shadow_atlas_depth_.clear();
    shadow_atlas_depth_.shrink_to_fit();
    shadow_atlas_size_ = 0;
    active_tile_x_ = 0;
    active_tile_y_ = 0;
    active_tile_w_ = 0;
    active_tile_h_ = 0;
    environment_bytes_ = 0;
    environment_revision_ = 0;
    total_mesh_bytes_ = 0;
    buffer_updates_ = 0;
    width_ = 0;
    height_ = 0;
    in_shadow_pass_ = false;
    in_frame_ = false;
    initialized_ = false;
}

core::Status SoftwareRenderDevice::resize(std::uint32_t width, std::uint32_t height) {
    if (!initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "SoftwareRenderDevice not initialized");
    }
    if (width > capabilities_.max_viewport_dimension || height > capabilities_.max_viewport_dimension) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Framebuffer resize exceeds max dimension");
    }
    width_ = width;
    height_ = height;
    const std::size_t pixel_count = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    color_buffer_.assign(pixel_count, Rgba8{0, 0, 0, 255});
    depth_buffer_.assign(pixel_count, 1.0f);
    return core::Status::ok();
}

core::Result<GpuMeshHandle> SoftwareRenderDevice::createMesh(const scene::MeshData& mesh) {
    if (!initialized_) {
        return core::Result<GpuMeshHandle>::fail(core::ErrorCode::InvalidState, "Device not initialized");
    }
    auto valid = mesh.validate();
    if (!valid) {
        return valid.error();
    }
    const std::uint64_t id = next_handle_++;
    total_mesh_bytes_ += mesh.byteSize();
    meshes_.emplace(id, mesh);
    return core::Result<GpuMeshHandle>::ok(GpuMeshHandle{id});
}

core::Status SoftwareRenderDevice::updateMesh(GpuMeshHandle handle, const scene::MeshData& mesh) {
    if (!initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Device not initialized");
    }
    const auto it = meshes_.find(handle.value);
    if (it == meshes_.end()) {
        return core::Status::fail(core::ErrorCode::NotFound, "updateMesh referenced unknown GpuMeshHandle");
    }
    auto valid = mesh.validate();
    if (!valid) {
        return valid;
    }
    const std::size_t old_bytes = it->second.byteSize();
    const std::size_t new_bytes = mesh.byteSize();
    it->second = mesh;
    total_mesh_bytes_ = (total_mesh_bytes_ - old_bytes) + new_bytes;
    ++buffer_updates_;
    return core::Status::ok();
}

void SoftwareRenderDevice::destroyMesh(GpuMeshHandle handle) noexcept {
    const auto it = meshes_.find(handle.value);
    if (it != meshes_.end()) {
        total_mesh_bytes_ -= it->second.byteSize();
        meshes_.erase(it);
    }
}

core::Status SoftwareRenderDevice::syncEnvironmentMap(
    const scene::HdriEnvironmentAsset* asset,
    std::uint64_t revision) {
    if (!initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Device not initialized");
    }
    if (asset == nullptr) {
        environment_bytes_ = 0;
        environment_revision_ = 0;
        return core::Status::ok();
    }
    if (environment_revision_ != revision) {
        environment_revision_ = revision;
        environment_bytes_ = asset->prefiltered_cubemap.gpuByteSize();
    }
    return core::Status::ok();
}

core::Status SoftwareRenderDevice::ensureShadowAtlas(std::uint32_t atlas_size) {
    if (!initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Device not initialized");
    }
    if (atlas_size == 0u) {
        shadow_atlas_depth_.clear();
        shadow_atlas_depth_.shrink_to_fit();
        shadow_atlas_size_ = 0u;
        return core::Status::ok();
    }
    if (shadow_atlas_size_ != atlas_size) {
        shadow_atlas_size_ = atlas_size;
        shadow_atlas_depth_.assign(
            static_cast<std::size_t>(atlas_size) * static_cast<std::size_t>(atlas_size),
            1.0f);
    }
    return core::Status::ok();
}

core::Status SoftwareRenderDevice::beginShadowTilePass(
    std::uint32_t tile_x,
    std::uint32_t tile_y,
    std::uint32_t tile_width,
    std::uint32_t tile_height) {
    if (!initialized_ || in_frame_ || shadow_atlas_size_ == 0u) {
        return core::Status::fail(
            core::ErrorCode::InvalidState,
            "SoftwareRenderDevice::beginShadowTilePass called in invalid state");
    }
    if (tile_x + tile_width > shadow_atlas_size_ || tile_y + tile_height > shadow_atlas_size_) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Shadow tile exceeds atlas bounds");
    }
    active_tile_x_ = tile_x;
    active_tile_y_ = tile_y;
    active_tile_w_ = tile_width;
    active_tile_h_ = tile_height;
    in_shadow_pass_ = true;

    for (std::uint32_t y = tile_y; y < tile_y + tile_height; ++y) {
        auto* row = shadow_atlas_depth_.data() + static_cast<std::size_t>(y) * shadow_atlas_size_;
        std::fill(row + tile_x, row + tile_x + tile_width, 1.0f);
    }
    return core::Status::ok();
}

core::Status SoftwareRenderDevice::drawShadowMesh(
    GpuMeshHandle mesh_handle,
    const math::Mat4& light_mvp,
    std::uint32_t index_offset,
    std::uint32_t index_count) {
    if (!initialized_ || !in_shadow_pass_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "drawShadowMesh called outside shadow pass");
    }
    const auto it = meshes_.find(mesh_handle.value);
    if (it == meshes_.end()) {
        return core::Status::fail(core::ErrorCode::NotFound, "Unknown GpuMeshHandle in drawShadowMesh");
    }
    if (active_tile_w_ == 0u || active_tile_h_ == 0u || shadow_atlas_size_ == 0u) {
        return core::Status::ok();
    }

    const scene::MeshData& mesh = it->second;
    const std::size_t total_indices = mesh.indices.size();
    const std::size_t idx_begin = std::min(static_cast<std::size_t>(index_offset), total_indices);
    const std::size_t idx_cnt = (index_count == 0u)
                                    ? (total_indices - idx_begin)
                                    : std::min(static_cast<std::size_t>(index_count), total_indices - idx_begin);
    const std::size_t idx_end = idx_begin + idx_cnt;

    struct ShadowVertex {
        float sx{0.0f};
        float sy{0.0f};
        float depth{1.0f};
        bool valid{false};
    };

    const float fx = static_cast<float>(active_tile_x_);
    const float fy = static_cast<float>(active_tile_y_);
    const float fw = static_cast<float>(active_tile_w_);
    const float fh = static_cast<float>(active_tile_h_);

    std::vector<ShadowVertex> projected(mesh.vertices.size());
    for (std::size_t vi = 0; vi < mesh.vertices.size(); ++vi) {
        const math::Vec4 clip = light_mvp * math::Vec4(mesh.vertices[vi].position, 1.0f);
        if (clip.w <= 1.0e-4f) {
            continue;
        }
        const float inv_w = 1.0f / clip.w;
        const float ndc_x = clip.x * inv_w;
        const float ndc_y = clip.y * inv_w;
        const float ndc_z = clip.z * inv_w;
        projected[vi] = ShadowVertex{
            .sx = fx + (ndc_x * 0.5f + 0.5f) * fw,
            .sy = fy + (ndc_y * 0.5f + 0.5f) * fh,
            .depth = ndc_z * 0.5f + 0.5f,
            .valid = true,
        };
    }

    const int min_tile_x = static_cast<int>(active_tile_x_);
    const int max_tile_x = static_cast<int>(active_tile_x_ + active_tile_w_) - 1;
    const int min_tile_y = static_cast<int>(active_tile_y_);
    const int max_tile_y = static_cast<int>(active_tile_y_ + active_tile_h_) - 1;

    for (std::size_t ti = idx_begin; ti + 2 < idx_end; ti += 3) {
        ShadowVertex v0 = projected[mesh.indices[ti + 0]];
        ShadowVertex v1 = projected[mesh.indices[ti + 1]];
        ShadowVertex v2 = projected[mesh.indices[ti + 2]];
        if (!v0.valid || !v1.valid || !v2.valid) {
            continue;
        }

        float area = edgeFunction(v0.sx, v0.sy, v1.sx, v1.sy, v2.sx, v2.sy);
        if (std::fabs(area) <= 1.0e-5f) {
            continue;
        }
        if (area < 0.0f) {
            std::swap(v1, v2);
            area = -area;
        }
        const float inv_area = 1.0f / area;

        const int x0 = std::max(min_tile_x, static_cast<int>(std::floor(std::min({v0.sx, v1.sx, v2.sx}))));
        const int x1 = std::min(max_tile_x, static_cast<int>(std::ceil(std::max({v0.sx, v1.sx, v2.sx}))));
        const int y0 = std::max(min_tile_y, static_cast<int>(std::floor(std::min({v0.sy, v1.sy, v2.sy}))));
        const int y1 = std::min(max_tile_y, static_cast<int>(std::ceil(std::max({v0.sy, v1.sy, v2.sy}))));

        for (int py = y0; py <= y1; ++py) {
            const float sample_y = static_cast<float>(py) + 0.5f;
            float* row = shadow_atlas_depth_.data() + static_cast<std::size_t>(py) * shadow_atlas_size_;
            for (int px = x0; px <= x1; ++px) {
                const float sample_x = static_cast<float>(px) + 0.5f;
                const float w0 = edgeFunction(v1.sx, v1.sy, v2.sx, v2.sy, sample_x, sample_y);
                const float w1 = edgeFunction(v2.sx, v2.sy, v0.sx, v0.sy, sample_x, sample_y);
                const float w2 = edgeFunction(v0.sx, v0.sy, v1.sx, v1.sy, sample_x, sample_y);
                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                    continue;
                }
                const float depth = (w0 * v0.depth + w1 * v1.depth + w2 * v2.depth) * inv_area;
                if (depth >= 0.0f && depth <= 1.0f && depth < row[px]) {
                    row[px] = depth;
                }
            }
        }
    }
    return core::Status::ok();
}

core::Status SoftwareRenderDevice::endShadowPass() {
    if (!initialized_ || !in_shadow_pass_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "endShadowPass called without beginShadowTilePass");
    }
    in_shadow_pass_ = false;
    return core::Status::ok();
}

core::Status SoftwareRenderDevice::beginFrame(const FrameClearDesc& clear) {
    if (!initialized_ || in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Invalid SoftwareRenderDevice::beginFrame state");
    }
    in_frame_ = true;

    const Rgba8 clear_px{
        floatToByte(clear.clear_color.x),
        floatToByte(clear.clear_color.y),
        floatToByte(clear.clear_color.z),
        floatToByte(clear.clear_color.w),
    };
    std::fill(color_buffer_.begin(), color_buffer_.end(), clear_px);
    std::fill(depth_buffer_.begin(), depth_buffer_.end(), clear.clear_depth);
    return core::Status::ok();
}

core::Status SoftwareRenderDevice::drawEditorGrid(const EditorGridCommand& command) {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(
            core::ErrorCode::InvalidState,
            "SoftwareRenderDevice::drawEditorGrid called outside active frame");
    }
    if (width_ == 0 || height_ == 0 || command.opacity <= 1.0e-4f) {
        return core::Status::ok();
    }

    const float inv_w = 1.0f / static_cast<float>(width_);
    const float inv_h = 1.0f / static_cast<float>(height_);
    const float dx_ndc = 2.0f * inv_w;
    const float dy_ndc = 2.0f * inv_h;
    const bool unrolled_persp =
        (!command.is_orthographic) && (std::fabs(command.camera_right.y) <= 1.0e-5f);

    for (std::uint32_t py = 0; py < height_; ++py) {
        const float ndc_y = 1.0f - (static_cast<float>(py) + 0.5f) * dy_ndc;
        if (unrolled_persp) {
            const float row_dy =
                command.camera_forward.y + command.camera_up.y * (ndc_y * command.frustum_half_y);
            if (std::fabs(row_dy) <= 1.0e-6f || (-command.camera_position.y / row_dy) <= 0.0f) {
                continue;
            }
        }

        const std::size_t row_offset = static_cast<std::size_t>(py) * static_cast<std::size_t>(width_);
        for (std::uint32_t px = 0; px < width_; ++px) {
            const float ndc_x = (static_cast<float>(px) + 0.5f) * dx_ndc - 1.0f;
            const EditorGridPixelSample sample =
                evaluateEditorGridAtNdc(ndc_x, ndc_y, dx_ndc, dy_ndc, command);
            if (!sample.visible) {
                continue;
            }
            const std::size_t idx = row_offset + static_cast<std::size_t>(px);
            if (sample.window_depth >= depth_buffer_[idx]) {
                continue;
            }
            const Rgba8 dst = color_buffer_[idx];
            const float a = sample.alpha;
            const float inv_a = 1.0f - a;
            color_buffer_[idx] = Rgba8{
                floatToByte(sample.color.x * a + (static_cast<float>(dst.r) / 255.0f) * inv_a),
                floatToByte(sample.color.y * a + (static_cast<float>(dst.g) / 255.0f) * inv_a),
                floatToByte(sample.color.z * a + (static_cast<float>(dst.b) / 255.0f) * inv_a),
                255,
            };
        }
    }
    return core::Status::ok();
}

core::Status SoftwareRenderDevice::beginOverlayPass(float clear_depth) {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(
            core::ErrorCode::InvalidState,
            "SoftwareRenderDevice::beginOverlayPass called outside active frame");
    }
    std::fill(depth_buffer_.begin(), depth_buffer_.end(), clear_depth);
    return core::Status::ok();
}

core::Status SoftwareRenderDevice::drawMesh(const DrawMeshCommand& command) {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "drawMesh called outside active frame");
    }
    const auto it = meshes_.find(command.mesh_handle.value);
    if (it == meshes_.end()) {
        return core::Status::fail(core::ErrorCode::NotFound, "Unknown GpuMeshHandle in SoftwareRenderDevice");
    }
    if (width_ == 0 || height_ == 0) {
        return core::Status::ok();
    }

    const scene::MeshData& mesh = it->second;
    const std::size_t total_indices = mesh.indices.size();
    const std::size_t idx_begin = std::min(static_cast<std::size_t>(command.index_offset), total_indices);
    const std::size_t idx_count = (command.index_count == 0)
                                      ? (total_indices - idx_begin)
                                      : std::min(static_cast<std::size_t>(command.index_count), total_indices - idx_begin);
    const std::size_t idx_end = idx_begin + idx_count;

    const math::Mat4 mvp = command.view_projection_matrix * command.model_matrix;
    const auto inv_model_opt = command.model_matrix.inverted();
    const math::Mat4 normal_matrix =
        inv_model_opt.has_value() ? inv_model_opt->transposed() : command.model_matrix;

    const float f_width = static_cast<float>(width_);
    const float f_height = static_cast<float>(height_);
    const float alpha = std::clamp(command.surface.opacity, 0.0f, 1.0f);
    const bool translucent = (alpha < 0.999f) && !command.surface.unlit_overlay;

    struct ProjectedVertex {
        float sx{0.0f};
        float sy{0.0f};
        float depth{0.0f};
        float inv_w{1.0f};
        math::Vec3 world_pos{};
        math::Vec3 world_normal{};
        math::Vec3 color{};
        bool valid{false};
    };

    std::vector<ProjectedVertex> projected(mesh.vertices.size());
    for (std::size_t vi = 0; vi < mesh.vertices.size(); ++vi) {
        const auto& v = mesh.vertices[vi];
        const math::Vec4 clip = mvp * math::Vec4(v.position, 1.0f);
        if (clip.w <= 1.0e-4f) {
            projected[vi].valid = false;
            continue;
        }
        const float inv_w = 1.0f / clip.w;
        const float ndc_x = clip.x * inv_w;
        const float ndc_y = clip.y * inv_w;
        const float ndc_z = clip.z * inv_w;

        projected[vi] = ProjectedVertex{
            .sx = (ndc_x * 0.5f + 0.5f) * f_width,
            .sy = (1.0f - (ndc_y * 0.5f + 0.5f)) * f_height,
            .depth = ndc_z * 0.5f + 0.5f,
            .inv_w = inv_w,
            .world_pos = command.model_matrix.transformPoint(v.position),
            .world_normal = normal_matrix.transformDirection(v.normal).normalized(),
            .color = v.color,
            .valid = true,
        };
    }

    for (std::size_t ti = idx_begin; ti + 2 < idx_end; ti += 3) {
        const auto& v0 = projected[mesh.indices[ti + 0]];
        const auto& v1 = projected[mesh.indices[ti + 1]];
        const auto& v2 = projected[mesh.indices[ti + 2]];
        if (!v0.valid || !v1.valid || !v2.valid) {
            continue;
        }

        const float area = edgeFunction(v0.sx, v0.sy, v1.sx, v1.sy, v2.sx, v2.sy);
        if (area <= 1.0e-5f) {
            continue;
        }
        const float inv_area = 1.0f / area;

        const float min_x_f = std::floor(std::min({v0.sx, v1.sx, v2.sx}));
        const float max_x_f = std::ceil(std::max({v0.sx, v1.sx, v2.sx}));
        const float min_y_f = std::floor(std::min({v0.sy, v1.sy, v2.sy}));
        const float max_y_f = std::ceil(std::max({v0.sy, v1.sy, v2.sy}));

        const int x0 = std::max(0, static_cast<int>(min_x_f));
        const int x1 = std::min(static_cast<int>(width_) - 1, static_cast<int>(max_x_f));
        const int y0 = std::max(0, static_cast<int>(min_y_f));
        const int y1 = std::min(static_cast<int>(height_) - 1, static_cast<int>(max_y_f));

        for (int py = y0; py <= y1; ++py) {
            const float sample_y = static_cast<float>(py) + 0.5f;
            for (int px = x0; px <= x1; ++px) {
                const float sample_x = static_cast<float>(px) + 0.5f;
                const float w0 = edgeFunction(v1.sx, v1.sy, v2.sx, v2.sy, sample_x, sample_y);
                const float w1 = edgeFunction(v2.sx, v2.sy, v0.sx, v0.sy, sample_x, sample_y);
                const float w2 = edgeFunction(v0.sx, v0.sy, v1.sx, v1.sy, sample_x, sample_y);

                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                    continue;
                }

                const float b0 = w0 * inv_area;
                const float b1 = w1 * inv_area;
                const float b2 = w2 * inv_area;

                const float depth = b0 * v0.depth + b1 * v1.depth + b2 * v2.depth;
                if (depth < 0.0f || depth > 1.0f) {
                    continue;
                }

                const std::size_t idx =
                    static_cast<std::size_t>(py) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(px);
                if (depth >= depth_buffer_[idx]) {
                    continue;
                }

                if (!translucent) {
                    depth_buffer_[idx] = depth;
                }

                const float p_w0 = b0 * v0.inv_w;
                const float p_w1 = b1 * v1.inv_w;
                const float p_w2 = b2 * v2.inv_w;
                const float denom = p_w0 + p_w1 + p_w2;
                const float inv_denom = (denom > 1.0e-8f) ? (1.0f / denom) : 1.0f;

                const math::Vec3 interp_pos =
                    (v0.world_pos * p_w0 + v1.world_pos * p_w1 + v2.world_pos * p_w2) * inv_denom;
                const math::Vec3 interp_nrm =
                    (v0.world_normal * p_w0 + v1.world_normal * p_w1 + v2.world_normal * p_w2) * inv_denom;
                const math::Vec3 interp_col =
                    (v0.color * p_w0 + v1.color * p_w1 + v2.color * p_w2) * inv_denom;

                const math::Vec3 shaded = evaluateUnifiedPbrFragmentCpu(
                    interp_pos,
                    interp_nrm,
                    interp_col,
                    command,
                    shadow_atlas_depth_,
                    shadow_atlas_size_);

                if (translucent) {
                    const Rgba8 dst = color_buffer_[idx];
                    const float inv_a = 1.0f - alpha;
                    color_buffer_[idx] = Rgba8{
                        floatToByte(shaded.x * alpha + (static_cast<float>(dst.r) / 255.0f) * inv_a),
                        floatToByte(shaded.y * alpha + (static_cast<float>(dst.g) / 255.0f) * inv_a),
                        floatToByte(shaded.z * alpha + (static_cast<float>(dst.b) / 255.0f) * inv_a),
                        255,
                    };
                } else {
                    color_buffer_[idx] = Rgba8{
                        floatToByte(shaded.x),
                        floatToByte(shaded.y),
                        floatToByte(shaded.z),
                        255,
                    };
                }
            }
        }
    }

    return core::Status::ok();
}

core::Status SoftwareRenderDevice::endFrame() {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "endFrame called without matching beginFrame");
    }
    in_frame_ = false;
    return core::Status::ok();
}

Rgba8 SoftwareRenderDevice::pixelAt(std::uint32_t x, std::uint32_t y) const noexcept {
    if (x >= width_ || y >= height_) {
        return Rgba8{0, 0, 0, 0};
    }
    return color_buffer_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)];
}

std::string SoftwareRenderDevice::encodePpm() const {
    std::ostringstream oss;
    oss << "P3\n" << width_ << ' ' << height_ << "\n255\n";
    for (std::uint32_t y = 0; y < height_; ++y) {
        for (std::uint32_t x = 0; x < width_; ++x) {
            const Rgba8 px = pixelAt(x, y);
            oss << static_cast<unsigned int>(px.r) << ' '
                << static_cast<unsigned int>(px.g) << ' '
                << static_cast<unsigned int>(px.b) << (x + 1 == width_ ? '\n' : ' ');
        }
    }
    return oss.str();
}

} // namespace finaleffect::rendering
