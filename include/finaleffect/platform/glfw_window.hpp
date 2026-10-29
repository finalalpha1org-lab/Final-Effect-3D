// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/platform/window.hpp"

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
    [[nodiscard]] bool shouldClose() const noexcept override;
    void requestClose() noexcept override;

    [[nodiscard]] FramebufferSize framebufferSize() const noexcept override;
    void swapBuffers() override;

    [[nodiscard]] GlProcLoaderFn glProcLoader() const noexcept override;

private:
    static void onFramebufferResize(GLFWwindow* handle, int width, int height) noexcept;

    GLFWwindow* window_{nullptr};
    FramebufferSize fb_size_{0, 0};
    bool glfw_initialized_{false};
};

} // namespace finaleffect::platform
