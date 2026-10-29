// Final Effect 3D - window and input interface.
// SPDX-License-Identifier: GPL-3.0-only
//
// Windows are a platform service, not part of the renderer. A window may exist
// without a graphics context (headless testing) and a graphics context is always
// created by a window backend, because that is how desktop, browser and mobile
// platforms all work.
#pragma once

#include <cstdint>
#include <string>

#include "finaleffect/core/Error.hpp"

namespace fe::platform {

enum class Key : int {
    Escape = 0,
    Space,
    Enter,
    Left,
    Right,
    Up,
    Down,
    W,
    A,
    S,
    D,
    R,
    F,
    KeyCount,
};

const char* toString(Key key) noexcept;

struct WindowDesc {
    std::string title = "Final Effect 3D";
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    bool resizable = true;
    bool verticalSync = true;
    /// Ask the backend to create a graphics context (OpenGL on desktop, WebGL2 in
    /// a browser). Headless windows ignore this flag.
    bool createGraphicsContext = true;
    int graphicsApiMajor = 3;
    int graphicsApiMinor = 3;
    bool graphicsApiCoreProfile = true;
};

class Window {
public:
    Window() = default;
    virtual ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    virtual core::Status create(const WindowDesc& desc) = 0;
    virtual void destroy() = 0;
    virtual bool isOpen() const = 0;

    virtual void pollEvents() = 0;
    virtual bool shouldClose() const = 0;
    virtual void requestClose() = 0;

    virtual std::uint32_t width() const = 0;
    virtual std::uint32_t height() const = 0;
    virtual bool consumeResizedFlag() = 0;

    virtual bool isKeyDown(Key key) const = 0;

    /// Monotonic seconds since the backend started.
    virtual double timeSeconds() const = 0;

    /// Presents the rendered frame. Desktop backends swap buffers here; headless
    /// windows do nothing. Called by the frame loop after rendering.
    virtual core::Status present() { return core::successStatus(); }

    /// Opaque native handle (GLFWwindow* on desktop). nullptr when headless.
    virtual void* nativeHandle() const = 0;
    virtual const char* backendName() const = 0;
};

/// Headless window used by tests, CI and --headless runs. It creates no native
/// window and no graphics context, and can close itself after N frames.
class NullWindow final : public Window {
public:
    explicit NullWindow(std::uint32_t framesUntilClose = 0);

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
    void* nativeHandle() const override;
    const char* backendName() const override;

    void setKeyDown(Key key, bool down);
    void simulateResize(std::uint32_t width, std::uint32_t height);
    std::uint32_t frameCount() const { return frames_; }

private:
    WindowDesc desc_;
    bool open_ = false;
    bool closeRequested_ = false;
    bool resized_ = false;
    bool keys_[static_cast<std::size_t>(Key::KeyCount)] = {};
    std::uint32_t frames_ = 0;
    std::uint32_t framesUntilClose_ = 0;
    double startSeconds_ = 0.0;
};

}  // namespace fe::platform
