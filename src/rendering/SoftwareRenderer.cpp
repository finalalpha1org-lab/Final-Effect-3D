// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/rendering/SoftwareRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fe::rendering {
namespace {

constexpr math::Scalar kAmbient = 0.22F;
const math::Vec3 kLightDirection = math::Vec3(0.35F, 0.85F, 0.4F).normalized();

struct ScreenVertex {
    math::Scalar x = 0.0F;
    math::Scalar y = 0.0F;
    math::Scalar depth = 0.0F;
    bool valid = false;
};

std::uint8_t toByte(math::Scalar linearValue) noexcept {
    const math::Scalar clamped = math::clamp(linearValue, 0.0F, 1.0F);
    return static_cast<std::uint8_t>(clamped * 255.0F + 0.5F);
}

/// Homogeneous clip-space position (x, y, z, w) of a point after M * v.
struct ClipVertex {
    math::Scalar x = 0.0F;
    math::Scalar y = 0.0F;
    math::Scalar z = 0.0F;
    math::Scalar w = 0.0F;
};

ClipVertex transformClip(const math::Mat4& matrix, const math::Vec3& point) noexcept {
    ClipVertex result;
    result.x = matrix.at(0, 0) * point.x + matrix.at(0, 1) * point.y + matrix.at(0, 2) * point.z + matrix.at(0, 3);
    result.y = matrix.at(1, 0) * point.x + matrix.at(1, 1) * point.y + matrix.at(1, 2) * point.z + matrix.at(1, 3);
    result.z = matrix.at(2, 0) * point.x + matrix.at(2, 1) * point.y + matrix.at(2, 2) * point.z + matrix.at(2, 3);
    result.w = matrix.at(3, 0) * point.x + matrix.at(3, 1) * point.y + matrix.at(3, 2) * point.z + matrix.at(3, 3);
    return result;
}

ScreenVertex projectToScreen(const ClipVertex& clip, std::uint32_t width, std::uint32_t height) noexcept {
    ScreenVertex result;
    if (clip.w <= 1.0e-5F) {
        return result;  // behind or on the near plane: Stage 1 skips the triangle.
    }
    const math::Scalar invW = 1.0F / clip.w;
    const math::Scalar ndcX = clip.x * invW;
    const math::Scalar ndcY = clip.y * invW;
    const math::Scalar ndcZ = clip.z * invW;

    result.x = (ndcX * 0.5F + 0.5F) * static_cast<math::Scalar>(width);
    result.y = (1.0F - (ndcY * 0.5F + 0.5F)) * static_cast<math::Scalar>(height);
    result.depth = ndcZ;
    result.valid = true;
    return result;
}

math::Scalar edgeFunction(const ScreenVertex& a, const ScreenVertex& b, math::Scalar px, math::Scalar py) noexcept {
    return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
}

}  // namespace

SoftwareRenderer::~SoftwareRenderer() = default;

core::Status SoftwareRenderer::initialize(const RendererInitInfo& info) {
    if (info.width == 0 || info.height == 0) {
        return core::makeError(core::ErrorCode::InvalidArgument,
                               "software renderer needs a non-zero framebuffer size",
                               "software renderer");
    }
    capabilities_.backendName = "cpu-rasterizer";
    capabilities_.offscreenOnly = true;
    capabilities_.supportsReadback = true;
    capabilities_.supportsDepthTest = true;
    if (!resize(info.width, info.height).ok()) {
        return core::makeError(core::ErrorCode::InitializationFailed,
                               "failed to allocate the CPU framebuffer",
                               "software renderer");
    }
    initialized_ = true;
    return core::successStatus();
}

core::Status SoftwareRenderer::resize(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0) {
        return core::makeError(core::ErrorCode::InvalidArgument, "framebuffer size must be non-zero",
                               "software renderer");
    }
    width_ = width;
    height_ = height;
    image_.width = width;
    image_.height = height;
    image_.rgba.assign(static_cast<std::size_t>(width) * height * 4U, 255U);
    depthBuffer_.assign(static_cast<std::size_t>(width) * height, std::numeric_limits<math::Scalar>::max());
    return core::successStatus();
}

core::Status SoftwareRenderer::beginFrame(const FrameInfo& frame) {
    if (!initialized_) {
        return core::makeError(core::ErrorCode::NotInitialized, "renderer was not initialized",
                               "software renderer");
    }
    if (frameOpen_) {
        return core::makeError(core::ErrorCode::InvalidArgument,
                               "beginFrame called twice without endFrame",
                               "software renderer");
    }
    clear(frame.clearColor);
    frameTriangles_ = 0;
    frameShadedPixels_ = 0;
    frameOpen_ = true;
    return core::successStatus();
}

core::Status SoftwareRenderer::submit(const RenderView& view) {
    if (!frameOpen_) {
        return core::makeError(core::ErrorCode::InvalidArgument, "submit called outside a frame",
                               "software renderer");
    }
    const math::Scalar aspect = static_cast<math::Scalar>(width_) / static_cast<math::Scalar>(height_);
    const math::Mat4 viewProjection = view.camera.viewProjectionMatrix(aspect);
    for (const RenderItem& item : view.items) {
        if (!item.visible || item.mesh == nullptr || !item.mesh->isValid()) {
            continue;
        }
        rasterizeItem(item, viewProjection);
    }
    return core::successStatus();
}

core::Status SoftwareRenderer::endFrame() {
    if (!frameOpen_) {
        return core::makeError(core::ErrorCode::InvalidArgument, "endFrame called without beginFrame",
                               "software renderer");
    }
    frameOpen_ = false;
    lastFrameTriangles_ = frameTriangles_;
    lastFrameShadedPixels_ = frameShadedPixels_;
    return core::successStatus();
}

core::Status SoftwareRenderer::readbackImage(ImageBuffer& out) const {
    if (!initialized_) {
        return core::makeError(core::ErrorCode::NotInitialized, "renderer was not initialized",
                               "software renderer");
    }
    out = image_;
    return core::successStatus();
}

core::Status SoftwareRenderer::shutdown() {
    initialized_ = false;
    frameOpen_ = false;
    image_ = ImageBuffer{};
    depthBuffer_.clear();
    width_ = 0;
    height_ = 0;
    return core::successStatus();
}

const RendererCapabilities& SoftwareRenderer::capabilities() const noexcept {
    return capabilities_;
}

bool SoftwareRenderer::isInitialized() const noexcept {
    return initialized_;
}

std::uint32_t SoftwareRenderer::width() const noexcept {
    return width_;
}

std::uint32_t SoftwareRenderer::height() const noexcept {
    return height_;
}

void SoftwareRenderer::clear(const math::Vec3& clearColor) {
    const std::uint8_t r = toByte(clearColor.x);
    const std::uint8_t g = toByte(clearColor.y);
    const std::uint8_t b = toByte(clearColor.z);
    for (std::size_t i = 0; i < image_.pixelCount(); ++i) {
        const std::size_t offset = i * 4U;
        image_.rgba[offset + 0U] = r;
        image_.rgba[offset + 1U] = g;
        image_.rgba[offset + 2U] = b;
        image_.rgba[offset + 3U] = 255U;
    }
    std::fill(depthBuffer_.begin(), depthBuffer_.end(), std::numeric_limits<math::Scalar>::max());
}

void SoftwareRenderer::rasterizeItem(const RenderItem& item, const math::Mat4& viewProjection) {
    const MeshData& mesh = *item.mesh;
    const math::Mat4 mvp = viewProjection * item.worldMatrix;

    for (std::size_t triangle = 0; triangle + 2U < mesh.indices.size(); triangle += 3U) {
        const Vertex& v0 = mesh.vertices[mesh.indices[triangle + 0U]];
        const Vertex& v1 = mesh.vertices[mesh.indices[triangle + 1U]];
        const Vertex& v2 = mesh.vertices[mesh.indices[triangle + 2U]];

        const ScreenVertex s0 = projectToScreen(transformClip(mvp, v0.position), width_, height_);
        const ScreenVertex s1 = projectToScreen(transformClip(mvp, v1.position), width_, height_);
        const ScreenVertex s2 = projectToScreen(transformClip(mvp, v2.position), width_, height_);
        if (!s0.valid || !s1.valid || !s2.valid) {
            continue;
        }
        ++frameTriangles_;

        const math::Scalar area = edgeFunction(s0, s1, s2.x, s2.y);
        if (math::approxZero(area, 1.0e-6F)) {
            continue;
        }
        const math::Scalar invArea = 1.0F / area;

        // Flat shading with the face normal in world space.
        const math::Vec3 worldNormal0 = item.worldMatrix.transformVector(v0.normal).normalized();
        const math::Vec3 worldNormal1 = item.worldMatrix.transformVector(v1.normal).normalized();
        const math::Vec3 worldNormal2 = item.worldMatrix.transformVector(v2.normal).normalized();
        const math::Vec3 faceNormal = ((worldNormal0 + worldNormal1 + worldNormal2) / 3.0F).normalized();
        const math::Scalar lambert = std::fmax(0.0F, math::dot(faceNormal, kLightDirection));
        const math::Scalar intensity = kAmbient + (1.0F - kAmbient) * lambert;
        const math::Vec3 shaded = item.color * intensity;

        const auto minX = static_cast<std::int32_t>(std::floor(std::fmin(s0.x, std::fmin(s1.x, s2.x))));
        const auto maxX = static_cast<std::int32_t>(std::ceil(std::fmax(s0.x, std::fmax(s1.x, s2.x))));
        const auto minY = static_cast<std::int32_t>(std::floor(std::fmin(s0.y, std::fmin(s1.y, s2.y))));
        const auto maxY = static_cast<std::int32_t>(std::ceil(std::fmax(s0.y, std::fmax(s1.y, s2.y))));

        const std::int32_t clampedMinX = std::max<std::int32_t>(0, minX);
        const std::int32_t clampedMaxX = std::min<std::int32_t>(static_cast<std::int32_t>(width_) - 1, maxX);
        const std::int32_t clampedMinY = std::max<std::int32_t>(0, minY);
        const std::int32_t clampedMaxY = std::min<std::int32_t>(static_cast<std::int32_t>(height_) - 1, maxY);

        for (std::int32_t y = clampedMinY; y <= clampedMaxY; ++y) {
            for (std::int32_t x = clampedMinX; x <= clampedMaxX; ++x) {
                const math::Scalar px = static_cast<math::Scalar>(x) + 0.5F;
                const math::Scalar py = static_cast<math::Scalar>(y) + 0.5F;
                const math::Scalar w0 = edgeFunction(s1, s2, px, py) * invArea;
                const math::Scalar w1 = edgeFunction(s2, s0, px, py) * invArea;
                const math::Scalar w2 = edgeFunction(s0, s1, px, py) * invArea;
                if (w0 < 0.0F || w1 < 0.0F || w2 < 0.0F) {
                    continue;
                }
                const math::Scalar depth = w0 * s0.depth + w1 * s1.depth + w2 * s2.depth;
                const std::size_t index = static_cast<std::size_t>(y) * width_ + static_cast<std::size_t>(x);
                if (depth >= depthBuffer_[index]) {
                    continue;
                }
                depthBuffer_[index] = depth;
                const std::size_t offset = index * 4U;
                image_.rgba[offset + 0U] = toByte(shaded.x);
                image_.rgba[offset + 1U] = toByte(shaded.y);
                image_.rgba[offset + 2U] = toByte(shaded.z);
                image_.rgba[offset + 3U] = 255U;
                ++frameShadedPixels_;
            }
        }
    }
}

}  // namespace fe::rendering
