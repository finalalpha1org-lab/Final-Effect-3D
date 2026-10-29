// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/platform/Window.hpp"

#include <chrono>

namespace fe::platform {
namespace {

double monotonicSeconds() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double>(now).count();
}

}  // namespace

const char* toString(Key key) noexcept {
    switch (key) {
        case Key::Escape: return "Escape";
        case Key::Space: return "Space";
        case Key::Enter: return "Enter";
        case Key::Left: return "Left";
        case Key::Right: return "Right";
        case Key::Up: return "Up";
        case Key::Down: return "Down";
        case Key::W: return "W";
        case Key::A: return "A";
        case Key::S: return "S";
        case Key::D: return "D";
        case Key::R: return "R";
        case Key::F: return "F";
        case Key::KeyCount: break;
    }
    return "Unknown";
}

Window::~Window() = default;

NullWindow::NullWindow(std::uint32_t framesUntilClose) : framesUntilClose_(framesUntilClose) {}

core::Status NullWindow::create(const WindowDesc& desc) {
    if (desc.width == 0 || desc.height == 0) {
        return core::makeError(core::ErrorCode::InvalidArgument, "window size must be non-zero",
                               "null window");
    }
    if (open_) {
        return core::makeError(core::ErrorCode::AlreadyExists, "window is already open", "null window");
    }
    desc_ = desc;
    open_ = true;
    closeRequested_ = false;
    resized_ = false;
    frames_ = 0;
    startSeconds_ = monotonicSeconds();
    return core::successStatus();
}

void NullWindow::destroy() {
    open_ = false;
}

bool NullWindow::isOpen() const {
    return open_;
}

void NullWindow::pollEvents() {
    if (!open_) {
        return;
    }
    ++frames_;
    resized_ = false;
    if (framesUntilClose_ != 0 && frames_ >= framesUntilClose_) {
        closeRequested_ = true;
    }
}

bool NullWindow::shouldClose() const {
    return closeRequested_ || !open_;
}

void NullWindow::requestClose() {
    closeRequested_ = true;
}

std::uint32_t NullWindow::width() const {
    return desc_.width;
}

std::uint32_t NullWindow::height() const {
    return desc_.height;
}

bool NullWindow::consumeResizedFlag() {
    const bool value = resized_;
    resized_ = false;
    return value;
}

bool NullWindow::isKeyDown(Key key) const {
    const auto index = static_cast<std::size_t>(key);
    return index < static_cast<std::size_t>(Key::KeyCount) ? keys_[index] : false;
}

double NullWindow::timeSeconds() const {
    return monotonicSeconds() - startSeconds_;
}

void* NullWindow::nativeHandle() const {
    return nullptr;
}

const char* NullWindow::backendName() const {
    return "null";
}

void NullWindow::setKeyDown(Key key, bool down) {
    const auto index = static_cast<std::size_t>(key);
    if (index < static_cast<std::size_t>(Key::KeyCount)) {
        keys_[index] = down;
    }
}

void NullWindow::simulateResize(std::uint32_t width, std::uint32_t height) {
    desc_.width = width;
    desc_.height = height;
    resized_ = true;
}

}  // namespace fe::platform
