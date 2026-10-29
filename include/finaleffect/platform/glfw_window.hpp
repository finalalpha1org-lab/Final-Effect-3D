// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/platform/window.hpp"

#include <vector>

struct GLFWwindow;

namespace finaleffect::platform {

// Desktop GLFW window + OpenGL 3.3 Core context adapter.
// Compiled only when FINALEFFECT_HAS_GLFW is enabled in CMake.
class GlfwWindow final : public IWindow {
public:
    GlfwWindow() = default;
    ~GlfwWindow() override;

    GlfwWindow(const GlfwWindow&) = delete;
    GlfwWindow& operator=(const GlfwWindow&) = delete;

    [[nodiscard]] core::Status initialize(const WindowDesc& desc) override;
    void shutdown() noexcept override;

    void pollEvents() override;
    [[nodiscard]] std::vector<InputEvent> consumeInputEvents() override;

    [[nodiscard]] bool shouldClose() const noexcept override;
    void requestClose() noexcept override;

    [[nodiscard]] FramebufferSize framebufferSize() const noexcept override;
    void swapBuffers() override;

    [[nodiscard]] GlProcLoaderFn glProcLoader() const noexcept override;

private:
    static void onFramebufferResize(GLFWwindow* handle, int width, int height) noexcept;
    static void onKey(GLFWwindow* handle, int key, int scancode, int action, int mods) noexcept;
    static void onMouseButton(GLFWwindow* handle, int button, int action, int mods) noexcept;
    static void onCursorPos(GLFWwindow* handle, double xpos, double ypos) noexcept;
    static void onScroll(GLFWwindow* handle, double xoffset, double yoffset) noexcept;

    void scaleCursorToFramebuffer(double& x, double& y) const noexcept;

    GLFWwindow* window_{nullptr};
    FramebufferSize fb_size_{0, 0};
    bool glfw_initialized_{false};
    std::vector<InputEvent> pending_events_;
};

} // namespace finaleffect::platform
