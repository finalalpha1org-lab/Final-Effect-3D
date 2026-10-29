// Final Effect 3D - renderer abstraction.
// SPDX-License-Identifier: GPL-3.0-only
//
// Boundary rules:
//   * Nothing above this interface knows about OpenGL, Vulkan, Metal, WebGPU,
//     windows or swap chains.
//   * The scene never owns GPU resources and GPU resources never own scene data.
//     Data flows one way: scene -> RenderView -> Renderer.
//   * Backends implement Renderer; the portable core only talks to the interface.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "finaleffect/core/Error.hpp"
#include "finaleffect/math/Mat4.hpp"
#include "finaleffect/math/Vec3.hpp"

namespace fe::rendering {

/// Vertex layout shared by every current and planned backend.
struct Vertex {
    math::Vec3 position;
    math::Vec3 normal;
};

/// CPU-side geometry. Backends upload this; they never modify it.
struct MeshData {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    std::size_t triangleCount() const noexcept { return indices.size() / 3U; }
    bool isValid() const noexcept { return !indices.empty() && indices.size() % 3U == 0U; }
};

/// One drawable submission: geometry reference plus its world placement.
struct RenderItem {
    math::Mat4 worldMatrix;
    const MeshData* mesh = nullptr;
    math::Vec3 color{0.82F, 0.82F, 0.86F};
    bool visible = true;
};

struct Camera {
    math::Vec3 position{3.0F, 2.0F, 4.0F};
    math::Vec3 target{0.0F, 0.0F, 0.0F};
    math::Vec3 up{0.0F, 1.0F, 0.0F};
    math::Scalar fieldOfViewY = 0.87266463F;  // 50 degrees
    math::Scalar nearPlane = 0.1F;
    math::Scalar farPlane = 500.0F;

    static Camera lookingAt(const math::Vec3& eye, const math::Vec3& focus, const math::Vec3& upAxis) noexcept;

    math::Mat4 viewMatrix() const noexcept;
    math::Mat4 projectionMatrix(math::Scalar aspect) const noexcept;
    math::Mat4 viewProjectionMatrix(math::Scalar aspect) const noexcept;
};

/// Everything a backend needs for one frame.
struct RenderView {
    Camera camera;
    std::vector<RenderItem> items;
};

/// Per-frame settings that do not come from the scene.
struct FrameInfo {
    math::Vec3 clearColor{0.06F, 0.07F, 0.09F};
};

struct RendererInitInfo {
    std::string applicationName = "Final Effect 3D";
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    bool verticalSync = true;
};

struct RendererCapabilities {
    /// Backend name reported in logs, for example "cpu-rasterizer" or "opengl-3.3-core".
    std::string backendName;
    bool offscreenOnly = false;
    bool supportsReadback = false;
    bool supportsDepthTest = true;
};

/// 8-bit RGBA image, rows top to bottom.
struct ImageBuffer {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;

    bool isEmpty() const noexcept { return width == 0 || height == 0 || rgba.size() != pixelCount() * 4U; }
    std::size_t pixelCount() const noexcept { return static_cast<std::size_t>(width) * height; }
};

/// Backend boundary. Implementations must be safe to shut down twice.
class Renderer {
public:
    Renderer() = default;
    virtual ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    virtual core::Status initialize(const RendererInitInfo& info) = 0;
    virtual core::Status resize(std::uint32_t width, std::uint32_t height) = 0;
    virtual core::Status beginFrame(const FrameInfo& frame) = 0;
    virtual core::Status submit(const RenderView& view) = 0;
    virtual core::Status endFrame() = 0;

    /// Optional frame capture used by tests, CLI screenshots and CI smoke tests.
    /// Backends that cannot read back return UnsupportedOperation.
    virtual core::Status readbackImage(ImageBuffer& out) const;

    virtual core::Status shutdown() = 0;

    virtual const RendererCapabilities& capabilities() const noexcept = 0;
    virtual bool isInitialized() const noexcept = 0;
    virtual std::uint32_t width() const noexcept = 0;
    virtual std::uint32_t height() const noexcept = 0;
};

}  // namespace fe::rendering
