// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/platform/glfw_window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <string>
#include <utility>

namespace finaleffect::platform {

namespace {

KeyModifiers translateGlfwMods(int mods) noexcept {
    return KeyModifiers{
        .shift = (mods & GLFW_MOD_SHIFT) != 0,
        .ctrl = (mods & GLFW_MOD_CONTROL) != 0,
        .alt = (mods & GLFW_MOD_ALT) != 0,
        .super = (mods & GLFW_MOD_SUPER) != 0,
    };
}

KeyAction translateGlfwAction(int action) noexcept {
    if (action == GLFW_RELEASE) {
        return KeyAction::Release;
    }
    if (action == GLFW_REPEAT) {
        return KeyAction::Repeat;
    }
    return KeyAction::Press;
}

KeyCode translateGlfwKey(int key) noexcept {
    switch (key) {
        case GLFW_KEY_ESCAPE:
            return KeyCode::Escape;
        case GLFW_KEY_DELETE:
            return KeyCode::Delete;
        case GLFW_KEY_BACKSPACE:
            return KeyCode::Backspace;
        case GLFW_KEY_SPACE:
            return KeyCode::Space;
        case GLFW_KEY_F:
            return KeyCode::F;
        case GLFW_KEY_Z:
            return KeyCode::Z;
        case GLFW_KEY_Y:
            return KeyCode::Y;
        case GLFW_KEY_W:
            return KeyCode::W;
        case GLFW_KEY_A:
            return KeyCode::A;
        case GLFW_KEY_S:
            return KeyCode::S;
        case GLFW_KEY_D:
            return KeyCode::D;
        case GLFW_KEY_LEFT_SHIFT:
            return KeyCode::LeftShift;
        case GLFW_KEY_LEFT_CONTROL:
            return KeyCode::LeftControl;
        case GLFW_KEY_LEFT_ALT:
            return KeyCode::LeftAlt;
        default:
            return KeyCode::Unknown;
    }
}

} // namespace

GlfwWindow::~GlfwWindow() {
    shutdown();
}

void GlfwWindow::scaleCursorToFramebuffer(double& x, double& y) const noexcept {
    if (window_ == nullptr) {
        return;
    }
    int win_w = 0;
    int win_h = 0;
    glfwGetWindowSize(window_, &win_w, &win_h);
    if (win_w > 0 && win_h > 0) {
        x *= static_cast<double>(fb_size_.width) / static_cast<double>(win_w);
        y *= static_cast<double>(fb_size_.height) / static_cast<double>(win_h);
    }
}

void GlfwWindow::onFramebufferResize(GLFWwindow* handle, int width, int height) noexcept {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(handle));
    if (self != nullptr) {
        self->fb_size_.width = width > 0 ? static_cast<std::uint32_t>(width) : 0u;
        self->fb_size_.height = height > 0 ? static_cast<std::uint32_t>(height) : 0u;
    }
}

void GlfwWindow::onKey(GLFWwindow* handle, int key, int /*scancode*/, int action, int mods) noexcept {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(handle));
    if (self == nullptr) {
        return;
    }
    const KeyCode code = translateGlfwKey(key);
    if (code == KeyCode::Escape && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(handle, GLFW_TRUE);
    }
    if (code != KeyCode::Unknown) {
        self->pending_events_.emplace_back(KeyEvent{
            .key = code,
            .action = translateGlfwAction(action),
            .mods = translateGlfwMods(mods),
        });
    }
}

void GlfwWindow::onMouseButton(GLFWwindow* handle, int button, int action, int mods) noexcept {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(handle));
    if (self == nullptr) {
        return;
    }
    MouseButton mb = MouseButton::Left;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        mb = MouseButton::Left;
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        mb = MouseButton::Right;
    } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        mb = MouseButton::Middle;
    } else {
        return;
    }

    double cx = 0.0;
    double cy = 0.0;
    glfwGetCursorPos(handle, &cx, &cy);
    self->scaleCursorToFramebuffer(cx, cy);

    self->pending_events_.emplace_back(MouseButtonEvent{
        .button = mb,
        .action = translateGlfwAction(action),
        .x = cx,
        .y = cy,
        .mods = translateGlfwMods(mods),
    });
}

void GlfwWindow::onCursorPos(GLFWwindow* handle, double xpos, double ypos) noexcept {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(handle));
    if (self != nullptr) {
        self->scaleCursorToFramebuffer(xpos, ypos);
        self->pending_events_.emplace_back(CursorMoveEvent{.x = xpos, .y = ypos});
    }
}

void GlfwWindow::onScroll(GLFWwindow* handle, double xoffset, double yoffset) noexcept {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(handle));
    if (self != nullptr) {
        KeyModifiers mods{};
        mods.shift = glfwGetKey(handle, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                     glfwGetKey(handle, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        mods.ctrl = glfwGetKey(handle, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                    glfwGetKey(handle, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
        mods.alt = glfwGetKey(handle, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
                   glfwGetKey(handle, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
        self->pending_events_.emplace_back(ScrollEvent{
            .delta_x = xoffset,
            .delta_y = yoffset,
            .mods = mods,
        });
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
    glfwSetKeyCallback(window_, &GlfwWindow::onKey);
    glfwSetMouseButtonCallback(window_, &GlfwWindow::onMouseButton);
    glfwSetCursorPosCallback(window_, &GlfwWindow::onCursorPos);
    glfwSetScrollCallback(window_, &GlfwWindow::onScroll);

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(desc.vsync ? 1 : 0);

    int fb_w = 0;
    int fb_h = 0;
    glfwGetFramebufferSize(window_, &fb_w, &fb_h);
    fb_size_.width = fb_w > 0 ? static_cast<std::uint32_t>(fb_w) : 0u;
    fb_size_.height = fb_h > 0 ? static_cast<std::uint32_t>(fb_h) : 0u;
    pending_events_.clear();

    return core::Status::ok();
}

void GlfwWindow::shutdown() noexcept {
    pending_events_.clear();
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
    if (window_ != nullptr) {
        glfwPollEvents();
    }
}

std::vector<InputEvent> GlfwWindow::consumeInputEvents() {
    std::vector<InputEvent> events;
    events.swap(pending_events_);
    return events;
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
