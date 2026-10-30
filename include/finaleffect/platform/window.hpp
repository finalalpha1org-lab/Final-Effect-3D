// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/platform/input.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace finaleffect::platform {

using GlProcAddressFn = void (*)(void);
using GlProcLoaderFn = GlProcAddressFn (*)(const char* symbol_name);

struct FramebufferSize {
    std::uint32_t width{0};
    std::uint32_t height{0};
};

struct WindowDesc {
    std::string title{"Final Effect 3D"};
    std::uint32_t width{1280};
    std::uint32_t height{720};
    bool resizable{true};
    bool vsync{true};
};

class IWindow {
public:
    virtual ~IWindow() = default;

    [[nodiscard]] virtual core::Status initialize(const WindowDesc& desc) = 0;
    virtual void shutdown() noexcept = 0;

    virtual void pollEvents() = 0;
    [[nodiscard]] virtual std::vector<InputEvent> consumeInputEvents() {
        return {};
    }

    [[nodiscard]] virtual bool shouldClose() const noexcept = 0;
    virtual void requestClose() noexcept = 0;

    [[nodiscard]] virtual FramebufferSize framebufferSize() const noexcept = 0;
    virtual void swapBuffers() = 0;

    // Optional proc-address loader for OpenGL-backed desktop windows; returns nullptr on non-GL windows.
    [[nodiscard]] virtual GlProcLoaderFn glProcLoader() const noexcept {
        return nullptr;
    }
};

class HeadlessWindow final : public IWindow {
public:
    [[nodiscard]] core::Status initialize(const WindowDesc& desc) override;
    void shutdown() noexcept override;

    void pollEvents() override;
    [[nodiscard]] std::vector<InputEvent> consumeInputEvents() override;

    [[nodiscard]] bool shouldClose() const noexcept override;
    void requestClose() noexcept override;

    [[nodiscard]] FramebufferSize framebufferSize() const noexcept override;
    void swapBuffers() override;

    void resize(std::uint32_t width, std::uint32_t height) noexcept;
    void pushInputEvent(const InputEvent& event);

    [[nodiscard]] std::uint64_t swappedFrameCount() const noexcept {
        return swapped_frames_;
    }

private:
    bool initialized_{false};
    bool should_close_{false};
    FramebufferSize size_{0, 0};
    std::uint64_t swapped_frames_{0};
    std::vector<InputEvent> pending_events_;
};

} // namespace finaleffect::platform
