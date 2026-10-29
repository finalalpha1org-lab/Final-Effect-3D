// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors
//
// Final Effect 3D — Browser / WebAssembly Host (Emscripten + WebGL 2.0)
// Shares finaleffect_core, finaleffect_math, finaleffect_scene, finaleffect_project,
// finaleffect_rendering, and finaleffect_app unchanged with the desktop host.

#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/core/version.hpp"
#include "finaleffect/platform/window.hpp"
#include "finaleffect/rendering/webgl2_render_device.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr const char* kCanvasSelector = "#fe3d-canvas";

finaleffect::platform::GlProcAddressFn resolveEmscriptenWebGlProc(const char* symbol_name) {
    void* raw = emscripten_webgl_get_proc_address(symbol_name);
    finaleffect::platform::GlProcAddressFn fn = nullptr;
    static_assert(sizeof(fn) == sizeof(raw));
    std::memcpy(&fn, &raw, sizeof(fn));
    return fn;
}

finaleffect::platform::KeyModifiers extractMouseMods(const EmscriptenMouseEvent& ev) noexcept {
    return finaleffect::platform::KeyModifiers{
        .shift = ev.shiftKey != 0,
        .ctrl = ev.ctrlKey != 0,
        .alt = ev.altKey != 0,
        .super = ev.metaKey != 0,
    };
}

finaleffect::platform::KeyCode translateDomKeyCode(std::string_view code) noexcept {
    using finaleffect::platform::KeyCode;
    if (code == "Escape") return KeyCode::Escape;
    if (code == "Delete") return KeyCode::Delete;
    if (code == "Backspace") return KeyCode::Backspace;
    if (code == "Space") return KeyCode::Space;
    if (code == "KeyF") return KeyCode::F;
    if (code == "KeyZ") return KeyCode::Z;
    if (code == "KeyY") return KeyCode::Y;
    if (code == "KeyW") return KeyCode::W;
    if (code == "KeyA") return KeyCode::A;
    if (code == "KeyS") return KeyCode::S;
    if (code == "KeyD") return KeyCode::D;
    if (code == "ShiftLeft" || code == "ShiftRight") return KeyCode::LeftShift;
    if (code == "ControlLeft" || code == "ControlRight") return KeyCode::LeftControl;
    if (code == "AltLeft" || code == "AltRight") return KeyCode::LeftAlt;
    return KeyCode::Unknown;
}

class Html5CanvasWindow final : public finaleffect::platform::IWindow {
public:
    Html5CanvasWindow() = default;
    ~Html5CanvasWindow() override { shutdown(); }

    [[nodiscard]] finaleffect::core::Status initialize(const finaleffect::platform::WindowDesc& desc) override {
        dpr_ = std::clamp(emscripten_get_device_pixel_ratio(), 1.0, 3.0);

        double css_w = static_cast<double>(desc.width);
        double css_h = static_cast<double>(desc.height);
        if (emscripten_get_element_css_size(kCanvasSelector, &css_w, &css_h) != EMSCRIPTEN_RESULT_SUCCESS ||
            css_w <= 1.0 || css_h <= 1.0) {
            css_w = static_cast<double>(desc.width);
            css_h = static_cast<double>(desc.height);
        }

        fb_size_.width = static_cast<std::uint32_t>(std::max(1.0, std::round(css_w * dpr_)));
        fb_size_.height = static_cast<std::uint32_t>(std::max(1.0, std::round(css_h * dpr_)));
        (void)emscripten_set_canvas_element_size(
            kCanvasSelector,
            static_cast<int>(fb_size_.width),
            static_cast<int>(fb_size_.height));

        EmscriptenWebGLContextAttributes attrs{};
        emscripten_webgl_init_context_attributes(&attrs);
        attrs.majorVersion = 2;
        attrs.minorVersion = 0;
        attrs.depth = EM_TRUE;
        attrs.stencil = EM_FALSE;
        attrs.antialias = EM_TRUE;
        attrs.alpha = EM_FALSE;
        attrs.enableExtensionsByDefault = EM_TRUE;

        webgl_ctx_ = emscripten_webgl_create_context(kCanvasSelector, &attrs);
        if (webgl_ctx_ <= 0) {
            return finaleffect::core::Status::fail(
                finaleffect::core::ErrorCode::RenderInitFailed,
                "Failed to create HTML5 WebGL 2.0 context on #fe3d-canvas");
        }
        if (emscripten_webgl_make_context_current(webgl_ctx_) != EMSCRIPTEN_RESULT_SUCCESS) {
            emscripten_webgl_destroy_context(webgl_ctx_);
            webgl_ctx_ = 0;
            return finaleffect::core::Status::fail(
                finaleffect::core::ErrorCode::RenderInitFailed,
                "Failed to activate HTML5 WebGL 2.0 context");
        }

        (void)emscripten_set_mousedown_callback(kCanvasSelector, this, EM_TRUE, &Html5CanvasWindow::onMouseButton);
        (void)emscripten_set_mouseup_callback(kCanvasSelector, this, EM_TRUE, &Html5CanvasWindow::onMouseButton);
        (void)emscripten_set_mousemove_callback(kCanvasSelector, this, EM_TRUE, &Html5CanvasWindow::onMouseMove);
        (void)emscripten_set_wheel_callback(kCanvasSelector, this, EM_TRUE, &Html5CanvasWindow::onWheel);
        (void)emscripten_set_keydown_callback(
            EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, &Html5CanvasWindow::onKey);
        (void)emscripten_set_keyup_callback(
            EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, &Html5CanvasWindow::onKey);
        (void)emscripten_set_resize_callback(
            EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_FALSE, &Html5CanvasWindow::onResize);

        should_close_ = false;
        return finaleffect::core::Status::ok();
    }

    void shutdown() noexcept override {
        pending_events_.clear();
        if (webgl_ctx_ > 0) {
            (void)emscripten_webgl_destroy_context(webgl_ctx_);
            webgl_ctx_ = 0;
        }
        should_close_ = true;
    }

    void pollEvents() override {}

    [[nodiscard]] std::vector<finaleffect::platform::InputEvent> consumeInputEvents() override {
        std::vector<finaleffect::platform::InputEvent> out;
        out.swap(pending_events_);
        return out;
    }

    [[nodiscard]] bool shouldClose() const noexcept override { return should_close_; }
    void requestClose() noexcept override { should_close_ = true; }

    [[nodiscard]] finaleffect::platform::FramebufferSize framebufferSize() const noexcept override {
        return fb_size_;
    }

    void swapBuffers() override {
        // Browser compositor presents WebGL2 default framebuffer automatically at end of rAF callback.
    }

    [[nodiscard]] finaleffect::platform::GlProcLoaderFn glProcLoader() const noexcept override {
        return &resolveEmscriptenWebGlProc;
    }

private:
    static EM_BOOL onMouseButton(int event_type, const EmscriptenMouseEvent* ev, void* user_data) {
        auto* self = static_cast<Html5CanvasWindow*>(user_data);
        if (self == nullptr || ev == nullptr) {
            return EM_FALSE;
        }
        finaleffect::platform::MouseButton btn = finaleffect::platform::MouseButton::Left;
        if (ev->button == 0) {
            btn = finaleffect::platform::MouseButton::Left;
        } else if (ev->button == 1) {
            btn = finaleffect::platform::MouseButton::Middle;
        } else if (ev->button == 2) {
            btn = finaleffect::platform::MouseButton::Right;
        } else {
            return EM_FALSE;
        }

        const auto action = (event_type == EMSCRIPTEN_EVENT_MOUSEDOWN)
                                ? finaleffect::platform::KeyAction::Press
                                : finaleffect::platform::KeyAction::Release;
        self->pending_events_.emplace_back(finaleffect::platform::MouseButtonEvent{
            .button = btn,
            .action = action,
            .x = static_cast<double>(ev->targetX) * self->dpr_,
            .y = static_cast<double>(ev->targetY) * self->dpr_,
            .mods = extractMouseMods(*ev),
        });
        return EM_TRUE;
    }

    static EM_BOOL onMouseMove(int /*event_type*/, const EmscriptenMouseEvent* ev, void* user_data) {
        auto* self = static_cast<Html5CanvasWindow*>(user_data);
        if (self == nullptr || ev == nullptr) {
            return EM_FALSE;
        }
        self->pending_events_.emplace_back(finaleffect::platform::CursorMoveEvent{
            .x = static_cast<double>(ev->targetX) * self->dpr_,
            .y = static_cast<double>(ev->targetY) * self->dpr_,
        });
        return EM_TRUE;
    }

    static EM_BOOL onWheel(int /*event_type*/, const EmscriptenWheelEvent* ev, void* user_data) {
        auto* self = static_cast<Html5CanvasWindow*>(user_data);
        if (self == nullptr || ev == nullptr) {
            return EM_FALSE;
        }
        const double step = (ev->deltaY < 0.0) ? 1.0 : ((ev->deltaY > 0.0) ? -1.0 : 0.0);
        if (step != 0.0) {
            self->pending_events_.emplace_back(finaleffect::platform::ScrollEvent{
                .delta_x = 0.0,
                .delta_y = step,
                .mods = extractMouseMods(ev->mouse),
            });
        }
        return EM_TRUE;
    }

    static EM_BOOL onKey(int event_type, const EmscriptenKeyboardEvent* ev, void* user_data) {
        auto* self = static_cast<Html5CanvasWindow*>(user_data);
        if (self == nullptr || ev == nullptr) {
            return EM_FALSE;
        }
        const auto code = translateDomKeyCode(ev->code);
        if (code == finaleffect::platform::KeyCode::Unknown) {
            return EM_FALSE;
        }
        finaleffect::platform::KeyAction action = finaleffect::platform::KeyAction::Press;
        if (event_type == EMSCRIPTEN_EVENT_KEYUP) {
            action = finaleffect::platform::KeyAction::Release;
        } else if (ev->repeat != 0) {
            action = finaleffect::platform::KeyAction::Repeat;
        }
        self->pending_events_.emplace_back(finaleffect::platform::KeyEvent{
            .key = code,
            .action = action,
            .mods = {
                .shift = ev->shiftKey != 0,
                .ctrl = ev->ctrlKey != 0 || ev->metaKey != 0,
                .alt = ev->altKey != 0,
                .super = ev->metaKey != 0,
            },
        });
        return EM_TRUE;
    }

    static EM_BOOL onResize(int /*event_type*/, const EmscriptenUiEvent* /*ev*/, void* user_data) {
        auto* self = static_cast<Html5CanvasWindow*>(user_data);
        if (self == nullptr) {
            return EM_FALSE;
        }
        self->dpr_ = std::clamp(emscripten_get_device_pixel_ratio(), 1.0, 3.0);
        double css_w = 0.0;
        double css_h = 0.0;
        if (emscripten_get_element_css_size(kCanvasSelector, &css_w, &css_h) == EMSCRIPTEN_RESULT_SUCCESS &&
            css_w > 1.0 && css_h > 1.0) {
            self->fb_size_.width = static_cast<std::uint32_t>(std::max(1.0, std::round(css_w * self->dpr_)));
            self->fb_size_.height = static_cast<std::uint32_t>(std::max(1.0, std::round(css_h * self->dpr_)));
            (void)emscripten_set_canvas_element_size(
                kCanvasSelector,
                static_cast<int>(self->fb_size_.width),
                static_cast<int>(self->fb_size_.height));
        }
        return EM_FALSE;
    }

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl_ctx_{0};
    finaleffect::platform::FramebufferSize fb_size_{1280, 720};
    double dpr_{1.0};
    bool should_close_{false};
    std::vector<finaleffect::platform::InputEvent> pending_events_;
};

struct WebHostContext {
    finaleffect::core::Engine engine;
    Html5CanvasWindow window;
    std::unique_ptr<finaleffect::app::StudioSession> session;

    explicit WebHostContext(const finaleffect::core::EngineConfig& cfg) : engine(cfg) {}
};

void onBrowserFrame(void* user_data) {
    auto* ctx = static_cast<WebHostContext*>(user_data);
    if (ctx == nullptr) {
        return;
    }
    if (ctx->window.shouldClose()) {
        ctx->engine.shutdown();
        ctx->window.shutdown();
        emscripten_cancel_main_loop();
        return;
    }
    auto st = ctx->engine.stepFrame();
    if (!st) {
        std::cerr << "[FinalEffect3D Web] Frame error: " << st.error().format() << '\n';
        ctx->engine.shutdown();
        ctx->window.shutdown();
        emscripten_cancel_main_loop();
    }
}

} // namespace

int main() {
    finaleffect::core::EngineConfig cfg;
    cfg.application_name = "Final Effect 3D Web Prototype";

    auto ctx = std::make_unique<WebHostContext>(cfg);
    auto win_st = ctx->window.initialize({
        .title = "Final Effect 3D — WebAssembly Viewport",
        .width = 1280,
        .height = 720,
    });
    if (!win_st) {
        std::cerr << "[FinalEffect3D Web] Canvas init failed: " << win_st.error().format() << '\n';
        return EXIT_FAILURE;
    }

    auto webgl_device = std::make_unique<finaleffect::rendering::WebGL2RenderDevice>(
        ctx->window.glProcLoader());
    ctx->session = std::make_unique<finaleffect::app::StudioSession>(
        ctx->window, std::move(webgl_device));

    auto init_st = ctx->engine.initialize(ctx->session.get());
    if (!init_st) {
        std::cerr << "[FinalEffect3D Web] Engine init failed: " << init_st.error().format() << '\n';
        ctx->window.shutdown();
        return EXIT_FAILURE;
    }

    // Release unique_ptr ownership into the Emscripten browser animation loop.
    emscripten_set_main_loop_arg(&onBrowserFrame, ctx.release(), 0, EM_TRUE);
    return EXIT_SUCCESS;
}
