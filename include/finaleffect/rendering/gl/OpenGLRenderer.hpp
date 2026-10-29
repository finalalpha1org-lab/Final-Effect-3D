// Final Effect 3D - OpenGL 3.3 Core desktop renderer (optional backend).
// SPDX-License-Identifier: GPL-3.0-only
//
// Compiled only when FE3D_ENABLE_OPENGL_RENDERER=ON. It is deliberately thin: it
// implements the same Renderer interface as the CPU reference renderer, so the
// engine, the scene and the project format do not change when it is enabled.
//
// No third-party OpenGL loader is used. Entry points are resolved through
// glfwGetProcAddress, which keeps the dependency list at exactly one optional
// library (GLFW, zlib licence) and avoids shipping generated loader code.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "finaleffect/rendering/Renderer.hpp"

namespace fe::platform {
class GlfwWindow;
}

namespace fe::rendering::gl {

class OpenGLRenderer final : public Renderer {
public:
    OpenGLRenderer();
    ~OpenGLRenderer() override;

    /// The window must already own a current OpenGL context.
    core::Status attachWindow(platform::GlfwWindow& window);

    core::Status initialize(const RendererInitInfo& info) override;
    core::Status resize(std::uint32_t width, std::uint32_t height) override;
    core::Status beginFrame(const FrameInfo& frame) override;
    core::Status submit(const RenderView& view) override;
    core::Status endFrame() override;

    /// Reads the current back buffer (GL_RGBA, GL_UNSIGNED_BYTE). Call after
    /// endFrame() and before the host swaps buffers.
    core::Status readbackImage(ImageBuffer& out) const override;

    core::Status shutdown() override;

    const RendererCapabilities& capabilities() const noexcept override;
    bool isInitialized() const noexcept override;
    std::uint32_t width() const noexcept override;
    std::uint32_t height() const noexcept override;

    std::size_t uploadedMeshCount() const noexcept { return meshLookup_.size(); }

private:
    struct GpuMesh {
        std::uint32_t vertexArray = 0;
        std::uint32_t vertexBuffer = 0;
        std::uint32_t indexBuffer = 0;
        std::int32_t indexCount = 0;
    };

    core::Status uploadMesh(const MeshData& mesh, const GpuMesh*& out);
    core::Status buildProgram(std::string& log);
    void releaseResources();

    struct GLApi;
    std::unique_ptr<GLApi> api_;
    platform::GlfwWindow* window_ = nullptr;

    std::vector<GpuMesh> gpuMeshes_;
    std::vector<std::pair<const MeshData*, std::size_t>> meshLookup_;

    RendererCapabilities capabilities_;
    std::uint32_t program_ = 0;
    std::int32_t uniformModel_ = -1;
    std::int32_t uniformViewProjection_ = -1;
    std::int32_t uniformColor_ = -1;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    bool initialized_ = false;
};

}  // namespace fe::rendering::gl
