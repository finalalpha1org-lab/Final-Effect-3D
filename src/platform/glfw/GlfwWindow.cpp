// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/platform/glfw/GlfwWindow.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <mutex>
#include <utility>

namespace fe::platform {
namespace {

std::mutex g_glfwMutex;
int g_glfwWindowCount = 0;

core::Error glfwError(std::string message) {
    const char* description = nullptr;
    const int code = glfwGetError(&description);
    std::string context = "glfw";
    if (description != nullptr) {
        context += " code ";
        context += std::to_string(code);
    }
    return core::makeError(core::ErrorCode::InitializationFailed, std::move(message), std::move(context));
}

bool acquireGlfw() {
    std::lock_guard<std::mutex> lock(g_glfwMutex);
    if (g_glfwWindowCount == 0) {
        if (glfwInit() != GLFW_TRUE) {
            return false;
        }
    }
    ++g_glfwWindowCount;
    return true;
}

void releaseGlfw() {
    std::lock_guard<std::mutex> lock(g_glfwMutex);
    if (g_glfwWindowCount > 0) {
        --g_glfwWindowCount;
        if (g_glfwWindowCount == 0) {
            glfwTerminate();
        }
    }
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
    if (self != nullptr) {
        self->onFramebufferResized(width, height);
    }
}

}  // namespace

Key GlfwWindow::mapKey(int glfwKey) noexcept {
    switch (glfwKey) {
        case GLFW_KEY_ESCAPE: return Key::Escape;
        case GLFW_KEY_SPACE: return Key::Space;
        case GLFW_KEY_ENTER: return Key::Enter;
        case GLFW_KEY_LEFT: return Key::Left;
        case GLFW_KEY_RIGHT: return Key::Right;
        case GLFW_KEY_UP: return Key::Up;
        case GLFW_KEY_DOWN: return Key::Down;
        case GLFW_KEY_W: return Key::W;
        case GLFW_KEY_A: return Key::A;
        case GLFW_KEY_S: return Key::S;
        case GLFW_KEY_D: return Key::D;
        case GLFW_KEY_R: return Key::R;
        case GLFW_KEY_F: return Key::F;
        default: break;
    }
    return Key::KeyCount;
}

GlfwWindow::~GlfwWindow() {
    destroy();
}

core::Status GlfwWindow::create(const WindowDesc& desc) {
    if (window_ != nullptr) {
        return core::makeError(core::ErrorCode::AlreadyExists, "GLFW window is already created", "glfw");
    }
    if (desc.width == 0 || desc.height == 0) {
        return core::makeError(core::ErrorCode::InvalidArgument, "window size must be non-zero", "glfw");
    }
    if (!acquireGlfw()) {
        return glfwError("glfwInit failed");
    }
    ownsGlfwInit_ = true;

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);
    if (desc.createGraphicsContext) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, desc.graphicsApiMajor);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, desc.graphicsApiMinor);
        if (desc.graphicsApiCoreProfile) {
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        }
    } else {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }

    window_ = glfwCreateWindow(static_cast<int>(desc.width),
                               static_cast<int>(desc.height),
                               desc.title.c_str(),
                               nullptr,
                               nullptr);
    if (window_ == nullptr) {
        releaseGlfw();
        ownsGlfwInit_ = false;
        return glfwError("glfwCreateWindow failed");
    }

    graphicsContext_ = desc.createGraphicsContext;
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
    glfwGetFramebufferSize(window_, &width_, &height_);
    if (graphicsContext_) {
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(desc.verticalSync ? 1 : 0);
    }
    return core::successStatus();
}

void GlfwWindow::destroy() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    if (ownsGlfwInit_) {
        releaseGlfw();
        ownsGlfwInit_ = false;
    }
    graphicsContext_ = false;
    width_ = 0;
    height_ = 0;
    resized_ = false;
}

bool GlfwWindow::isOpen() const {
    return window_ != nullptr;
}

void GlfwWindow::pollEvents() {
    glfwPollEvents();
}

bool GlfwWindow::shouldClose() const {
    return window_ == nullptr || glfwWindowShouldClose(window_) == GLFW_TRUE;
}

void GlfwWindow::requestClose() {
    if (window_ != nullptr) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
}

std::uint32_t GlfwWindow::width() const {
    return static_cast<std::uint32_t>(width_ > 0 ? width_ : 0);
}

std::uint32_t GlfwWindow::height() const {
    return static_cast<std::uint32_t>(height_ > 0 ? height_ : 0);
}

bool GlfwWindow::consumeResizedFlag() {
    const bool value = resized_;
    resized_ = false;
    return value;
}

bool GlfwWindow::isKeyDown(Key key) const {
    if (window_ == nullptr) {
        return false;
    }
    for (int glfwKey = GLFW_KEY_SPACE; glfwKey <= GLFW_KEY_LAST; ++glfwKey) {
        if (mapKey(glfwKey) == key) {
            return glfwGetKey(window_, glfwKey) == GLFW_PRESS;
        }
    }
    return false;
}

double GlfwWindow::timeSeconds() const {
    return glfwGetTime();
}

core::Status GlfwWindow::present() {
    return swapBuffers();
}

void* GlfwWindow::nativeHandle() const {
    return window_;
}

const char* GlfwWindow::backendName() const {
    return "glfw";
}

core::Status GlfwWindow::makeContextCurrent() {
    if (window_ == nullptr || !graphicsContext_) {
        return core::makeError(core::ErrorCode::GraphicsUnavailable,
                               "window has no graphics context",
                               "glfw");
    }
    glfwMakeContextCurrent(window_);
    return core::successStatus();
}

core::Status GlfwWindow::swapBuffers() {
    if (window_ == nullptr || !graphicsContext_) {
        return core::makeError(core::ErrorCode::GraphicsUnavailable,
                               "window has no graphics context",
                               "glfw");
    }
    glfwSwapBuffers(window_);
    return core::successStatus();
}

void GlfwWindow::onFramebufferResized(int width, int height) {
    width_ = width;
    height_ = height;
    resized_ = true;
}

}  // namespace fe::platform
