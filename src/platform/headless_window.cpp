// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/platform/window.hpp"

#include <utility>

namespace finaleffect::platform {

core::Status HeadlessWindow::initialize(const WindowDesc& desc) {
    if (desc.width == 0 || desc.height == 0) {
        return core::Status::fail(
            core::ErrorCode::InvalidArgument,
            "HeadlessWindow initial dimensions must be non-zero");
    }
    size_ = FramebufferSize{desc.width, desc.height};
    should_close_ = false;
    swapped_frames_ = 0;
    pending_events_.clear();
    initialized_ = true;
    return core::Status::ok();
}

void HeadlessWindow::shutdown() noexcept {
    initialized_ = false;
    should_close_ = true;
    pending_events_.clear();
}

void HeadlessWindow::pollEvents() {}

std::vector<InputEvent> HeadlessWindow::consumeInputEvents() {
    std::vector<InputEvent> events;
    events.swap(pending_events_);
    return events;
}

bool HeadlessWindow::shouldClose() const noexcept {
    return !initialized_ || should_close_;
}

void HeadlessWindow::requestClose() noexcept {
    should_close_ = true;
}

FramebufferSize HeadlessWindow::framebufferSize() const noexcept {
    return size_;
}

void HeadlessWindow::swapBuffers() {
    if (initialized_) {
        ++swapped_frames_;
    }
}

void HeadlessWindow::resize(std::uint32_t width, std::uint32_t height) noexcept {
    size_ = FramebufferSize{width, height};
}

void HeadlessWindow::pushInputEvent(const InputEvent& event) {
    pending_events_.push_back(event);
}

} // namespace finaleffect::platform
