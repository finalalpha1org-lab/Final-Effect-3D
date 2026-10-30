// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/rendering/software_render_device.hpp"
#include "finaleffect/math/constants.hpp"

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

math::Vec3 lerpVec3(const math::Vec3& a, const math::Vec3& b, float t) noexcept {
    return a * (1.0f - t) + b * t;
}

math::Vec3 fresnelSchlick(float cos_theta, const math::Vec3& f0) noexcept {
    const float x = std::clamp(1.0f - cos_theta, 0.0f, 1.0f);
    const float x2 = x * x;
    const float x5 = x2 * x2 * x;
    return f0 + (math::Vec3{1.0f, 1.0f, 1.0f} - f0) * x5;
}

float distributionGgx(float n_dot_h, float roughness) noexcept {
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float denom_term = n_dot_h * n_dot_h * (a2 - 1.0f) + 1.0f;
    const float denom = math::kPi * denom_term * denom_term;
    return a2 / std::max(denom, 1.0e-6f);
}

float geometrySchlickGgx(float n_dot_x, float k) noexcept {
    return n_dot_x / std::max(n_dot_x * (1.0f - k) + k, 1.0e-5f);
}

// Energy-conserving CPU reference metallic-roughness Cook-Torrance GGX shading
// matching the GLSL 330 / GLSL ES 3.00 studio fragment shader.
math::Vec3 evaluateSoftwarePbrFragment(
    const math::Vec3& world_pos,
    const math::Vec3& world_normal_raw,
    const math::Vec3& vertex_color,
    const DrawMeshCommand& cmd) noexcept {
    const PbrSurfaceParams& s = cmd.surface;
    const math::Vec3 weighted_vcol = lerpVec3(math::Vec3{1.0f, 1.0f, 1.0f}, vertex_color, s.vertex_color_weight);
    const math::Vec3 albedo = s.base_color.cwiseProduct(weighted_vcol);

    if (s.unlit_overlay) {
        const math::Vec3 tint = cmd.selected ? math::Vec3{0.25f, 0.16f, 0.04f} : math::Vec3::zero();
        return albedo + tint;
    }

    const math::Vec3 n = world_normal_raw.normalized();
    const math::Vec3 v = (cmd.camera_position - world_pos).normalized();
    const float n_dot_v = std::max(n.dot(v), 1.0e-4f);

    const float metallic = std::clamp(s.metallic, 0.0f, 1.0f);
    const float roughness = std::clamp(s.roughness, 0.045f, 1.0f);
    const float ao = std::clamp(s.ambient_occlusion, 0.0f, 1.0f);
    const float k_direct = ((roughness + 1.0f) * (roughness + 1.0f)) * 0.125f;

    const math::Vec3 f0 = lerpVec3(math::Vec3{0.04f, 0.04f, 0.04f}, albedo, metallic);

    auto evalDirectionalLobe = [&](const math::Vec3& l, const math::Vec3& radiance) -> math::Vec3 {
        const float n_dot_l = std::max(n.dot(l), 0.0f);
        if (n_dot_l <= 0.0f) {
            return math::Vec3::zero();
        }
        const math::Vec3 h = (v + l).normalized();
        const float n_dot_h = std::max(n.dot(h), 0.0f);
        const float h_dot_v = std::max(h.dot(v), 0.0f);

        const float ndf = distributionGgx(n_dot_h, roughness);
        const float geom = geometrySchlickGgx(n_dot_v, k_direct) * geometrySchlickGgx(n_dot_l, k_direct);
        const math::Vec3 fresnel = fresnelSchlick(h_dot_v, f0);

        const math::Vec3 kd = (math::Vec3{1.0f, 1.0f, 1.0f} - fresnel) * (1.0f - metallic);
        const math::Vec3 diffuse = kd.cwiseProduct(albedo) * (1.0f / math::kPi);
        const math::Vec3 specular = fresnel * ((ndf * geom) / std::max(4.0f * n_dot_v * n_dot_l, 1.0e-4f));
        return (diffuse + specular).cwiseProduct(radiance) * n_dot_l;
    };

    const math::Vec3 key_l = cmd.lighting.direction_to_light.normalized();
    const math::Vec3 fill_l = math::Vec3{-0.55f, 0.35f, -0.65f}.normalized();
    const math::Vec3 fill_radiance = cmd.lighting.radiance * 0.22f;

    math::Vec3 lo = evalDirectionalLobe(key_l, cmd.lighting.radiance) +
                    evalDirectionalLobe(fill_l, fill_radiance);

    // Hemispheric studio ambient + Fresnel environment sheen scaled by ambient occlusion.
    const float sky_factor = n.y * 0.5f + 0.5f;
    const math::Vec3 env_irradiance = cmd.lighting.ambient * (0.65f + 0.70f * sky_factor);
    const math::Vec3 f_env = fresnelSchlick(n_dot_v, f0);
    const math::Vec3 kd_env = (math::Vec3{1.0f, 1.0f, 1.0f} - f_env) * (1.0f - metallic);
    const math::Vec3 env_spec = f_env.cwiseProduct(env_irradiance) * (1.0f - 0.75f * roughness);
    const math::Vec3 ambient_term = (kd_env.cwiseProduct(albedo).cwiseProduct(env_irradiance) + env_spec) * ao;

    math::Vec3 linear_hdr = ambient_term + lo + s.emissive_radiance;
    if (cmd.selected) {
        linear_hdr = linear_hdr * 0.85f + math::Vec3{0.22f, 0.14f, 0.03f};
    }

    // Reinhard tone mapping + linear-to-sRGB (gamma 2.2) conversion.
    const math::Vec3 mapped{
        linear_hdr.x / (1.0f + std::max(0.0f, linear_hdr.x)),
        linear_hdr.y / (1.0f + std::max(0.0f, linear_hdr.y)),
        linear_hdr.z / (1.0f + std::max(0.0f, linear_hdr.z)),
    };
    constexpr float kInvGamma = 1.0f / 2.2f;
    return math::Vec3{
        std::pow(std::clamp(mapped.x, 0.0f, 1.0f), kInvGamma),
        std::pow(std::clamp(mapped.y, 0.0f, 1.0f), kInvGamma),
        std::pow(std::clamp(mapped.z, 0.0f, 1.0f), kInvGamma),
    };
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
    buffer_updates_ = 0;
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

                const math::Vec3 shaded = evaluateSoftwarePbrFragment(interp_pos, interp_nrm, interp_col, command);

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
