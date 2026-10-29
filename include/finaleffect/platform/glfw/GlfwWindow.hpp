// Final Effect 3D - GLFW window backend (desktop host).
// SPDX-License-Identifier: GPL-3.0-only
//
// This is the only file in the engine that mentions GLFW. It is compiled only
// when FE3D_ENABLE_OPENGL_RENDERER=ON.
//
// GLFW requires process-wide initialization. Rather than hiding that behind a
// singleton, this backend keeps a reference count in this translation unit and
// shuts GLFW down when the last window is destroyed. That is the whole extent of
// the global state, and it is contained here.
#pragma once

#include <cstdint>

#include "finaleffect/platform/Window.hpp"

struct GLFWwindow;

namespace fe::platform {

class GlfwWindow final : public Window {
public:
    GlfwWindow() = default;
    ~GlfwWindow() override;

    core::Status create(const WindowDesc& desc) override;
    void destroy() override;
    bool isOpen() const override;

    void pollEvents() override;
    bool shouldClose() const override;
    void requestClose() override;

    std::uint32_t width() const override;
    std::uint32_t height() const override;
    bool consumeResizedFlag() override;

    bool isKeyDown(Key key) const override;

    double timeSeconds() const override;
    core::Status present() override;
    void* nativeHandle() const override;
    const char* backendName() const override;

    /// False when the window was created without a graphics context.
    bool hasGraphicsContext() const { return graphicsContext_; }
    core::Status makeContextCurrent();
    core::Status swapBuffers();

    /// Called from GLFW callbacks.
    void onFramebufferResized(int width, int height);

    static Key mapKey(int glfwKey) noexcept;

private:
    GLFWwindow* window_ = nullptr;
    bool graphicsContext_ = false;
    bool ownsGlfwInit_ = false;
    bool resized_ = false;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace fe::platform
