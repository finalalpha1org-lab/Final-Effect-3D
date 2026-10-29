// Final Effect 3D - dependency-free CPU reference renderer.
// SPDX-License-Identifier: GPL-3.0-only
//
// Purpose: make the renderer boundary real and testable on machines without a
// GPU or a display (headless CI, servers, containers). It renders the same
// RenderView that a GPU backend consumes, so it validates scene, camera and
// matrix maths end to end.
//
// Stage 1 limitations (deliberate, documented):
//   * flat shading, one directional light, no shadows or textures;
//   * no near-plane clipping: triangles crossing the near plane are skipped;
//   * no anti-aliasing.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "finaleffect/rendering/Renderer.hpp"

namespace fe::rendering {

class SoftwareRenderer final : public Renderer {
public:
    SoftwareRenderer() = default;
    ~SoftwareRenderer() override;

    core::Status initialize(const RendererInitInfo& info) override;
    core::Status resize(std::uint32_t width, std::uint32_t height) override;
    core::Status beginFrame(const FrameInfo& frame) override;
    core::Status submit(const RenderView& view) override;
    core::Status endFrame() override;
    core::Status readbackImage(ImageBuffer& out) const override;
    core::Status shutdown() override;

    const RendererCapabilities& capabilities() const noexcept override;
    bool isInitialized() const noexcept override;
    std::uint32_t width() const noexcept override;
    std::uint32_t height() const noexcept override;

    /// Triangles submitted during the last completed frame (diagnostics/tests).
    std::size_t lastFrameTriangleCount() const noexcept { return lastFrameTriangles_; }
    /// Pixels written during the last completed frame (diagnostics/tests).
    std::size_t lastFrameShadedPixelCount() const noexcept { return lastFrameShadedPixels_; }

private:
    void clear(const math::Vec3& clearColor);
    void rasterizeItem(const RenderItem& item, const math::Mat4& viewProjection);

    RendererCapabilities capabilities_;
    bool initialized_ = false;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    ImageBuffer image_;
    std::vector<math::Scalar> depthBuffer_;
    std::size_t frameTriangles_ = 0;
    std::size_t frameShadedPixels_ = 0;
    std::size_t lastFrameTriangles_ = 0;
    std::size_t lastFrameShadedPixels_ = 0;
    bool frameOpen_ = false;
};

}  // namespace fe::rendering
