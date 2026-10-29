// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/rendering/software_render_device.hpp"

#include <algorithm>
#include <cmath>
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
    total_mesh_bytes_ = 0;
    width_ = 0;
    height_ = 0;
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

void SoftwareRenderDevice::destroyMesh(GpuMeshHandle handle) noexcept {
    const auto it = meshes_.find(handle.value);
    if (it != meshes_.end()) {
        total_mesh_bytes_ -= it->second.byteSize();
        meshes_.erase(it);
    }
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
    const math::Mat4 mvp = command.view_projection_matrix * command.model_matrix;
    const auto inv_model_opt = command.model_matrix.inverted();
    const math::Mat4 normal_matrix =
        inv_model_opt.has_value() ? inv_model_opt->transposed() : command.model_matrix;

    const math::Vec3 light_dir = command.lighting.direction_to_light.normalized();
    const float f_width = static_cast<float>(width_);
    const float f_height = static_cast<float>(height_);

    struct ProjectedVertex {
        float sx{0.0f};
        float sy{0.0f};
        float depth{0.0f};
        float inv_w{1.0f};
        math::Vec3 shaded_color{};
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

        const math::Vec3 world_normal = normal_matrix.transformDirection(v.normal).normalized();
        const float n_dot_l = std::max(0.0f, world_normal.dot(light_dir));
        const math::Vec3 lit = v.color.cwiseProduct(
            command.lighting.ambient + command.lighting.radiance * n_dot_l);

        projected[vi] = ProjectedVertex{
            .sx = (ndc_x * 0.5f + 0.5f) * f_width,
            .sy = (1.0f - (ndc_y * 0.5f + 0.5f)) * f_height,
            .depth = ndc_z * 0.5f + 0.5f,
            .inv_w = inv_w,
            .shaded_color = lit,
            .valid = true,
        };
    }

    for (std::size_t ti = 0; ti + 2 < mesh.indices.size(); ti += 3) {
        const auto& v0 = projected[mesh.indices[ti + 0]];
        const auto& v1 = projected[mesh.indices[ti + 1]];
        const auto& v2 = projected[mesh.indices[ti + 2]];
        if (!v0.valid || !v1.valid || !v2.valid) {
            continue;
        }

        // Front-facing CCW in Y-down screen coordinates yields positive edgeFunction(v0, v1, v2).
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

                depth_buffer_[idx] = depth;

                // Perspective-correct color interpolation.
                const float p_w0 = b0 * v0.inv_w;
                const float p_w1 = b1 * v1.inv_w;
                const float p_w2 = b2 * v2.inv_w;
                const float denom = p_w0 + p_w1 + p_w2;
                const float inv_denom = (denom > 1.0e-8f) ? (1.0f / denom) : 1.0f;

                const math::Vec3 color =
                    (v0.shaded_color * p_w0 + v1.shaded_color * p_w1 + v2.shaded_color * p_w2) * inv_denom;

                color_buffer_[idx] = Rgba8{
                    floatToByte(color.x),
                    floatToByte(color.y),
                    floatToByte(color.z),
                    255,
                };
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
