// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/platform/glfw_window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <string>

namespace finaleffect::platform {

GlfwWindow::~GlfwWindow() {
    shutdown();
}

void GlfwWindow::onFramebufferResize(GLFWwindow* handle, int width, int height) noexcept {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(handle));
    if (self != nullptr) {
        self->fb_size_.width = width > 0 ? static_cast<std::uint32_t>(width) : 0u;
        self->fb_size_.height = height > 0 ? static_cast<std::uint32_t>(height) : 0u;
    }
}

core::Status GlfwWindow::initialize(const WindowDesc& desc) {
    if (window_ != nullptr) {
        return core::Status::fail(core::ErrorCode::InvalidState, "GlfwWindow is already initialized");
    }
    if (desc.width == 0 || desc.height == 0) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Window dimensions must be non-zero");
    }

    if (glfwInit() != GLFW_TRUE) {
        const char* err_desc = nullptr;
        const int err_code = glfwGetError(&err_desc);
        std::string detail = (err_desc != nullptr) ? err_desc : ("GLFW error " + std::to_string(err_code));
        return core::Status::fail(
            core::ErrorCode::RenderInitFailed,
            "Failed to initialize GLFW windowing subsystem",
            std::move(detail));
    }
    glfw_initialized_ = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    window_ = glfwCreateWindow(
        static_cast<int>(desc.width),
        static_cast<int>(desc.height),
        desc.title.c_str(),
        nullptr,
        nullptr);

    if (window_ == nullptr) {
        const char* err_desc = nullptr;
        const int err_code = glfwGetError(&err_desc);
        std::string detail = (err_desc != nullptr) ? err_desc : ("GLFW error " + std::to_string(err_code));
        glfwTerminate();
        glfw_initialized_ = false;
        return core::Status::fail(
            core::ErrorCode::RenderInitFailed,
            "Failed to create OpenGL 3.3 Core window",
            std::move(detail));
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, &GlfwWindow::onFramebufferResize);
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(desc.vsync ? 1 : 0);

    int fb_w = 0;
    int fb_h = 0;
    glfwGetFramebufferSize(window_, &fb_w, &fb_h);
    fb_size_.width = fb_w > 0 ? static_cast<std::uint32_t>(fb_w) : 0u;
    fb_size_.height = fb_h > 0 ? static_cast<std::uint32_t>(fb_h) : 0u;

    return core::Status::ok();
}

void GlfwWindow::shutdown() noexcept {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    if (glfw_initialized_) {
        glfwTerminate();
        glfw_initialized_ = false;
    }
    fb_size_ = {0, 0};
}

void GlfwWindow::pollEvents() {
    if (window_ == nullptr) {
        return;
    }
    glfwPollEvents();
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
}

bool GlfwWindow::shouldClose() const noexcept {
    return window_ == nullptr || glfwWindowShouldClose(window_) == GLFW_TRUE;
}

void GlfwWindow::requestClose() noexcept {
    if (window_ != nullptr) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
}

FramebufferSize GlfwWindow::framebufferSize() const noexcept {
    return fb_size_;
}

void GlfwWindow::swapBuffers() {
    if (window_ != nullptr) {
        glfwSwapBuffers(window_);
    }
}

GlProcLoaderFn GlfwWindow::glProcLoader() const noexcept {
    return &glfwGetProcAddress;
}

} // namespace finaleffect::platform
