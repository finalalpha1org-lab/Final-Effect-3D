// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors
//
// Final Effect 3D — Browser / WebAssembly Studio Host (Emscripten + WebGL 2.0)
// Shares finaleffect_core, finaleffect_math, finaleffect_scene, finaleffect_project,
// finaleffect_rendering, and finaleffect_app (StudioSession + EditorController) unchanged
// with the desktop host.

#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/core/version.hpp"
#include "finaleffect/platform/file_system.hpp"
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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr const char* kCanvasSelector = "#fe3d-canvas";

EM_JS(int, fe3d_is_text_input_focused, (), {
    const el = document.activeElement;
    if (!el) return 0;
    const tag = el.tagName ? el.tagName.toUpperCase() : "";
    if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT" || el.isContentEditable) {
        return 1;
    }
    return 0;
});

EM_JS(void, fe3d_get_viewport_css_size, (double* out_w, double* out_h), {
    const stage = document.getElementById("viewport-stage") || document.getElementById("fe3d-canvas");
    if (!stage) {
        HEAPF64[out_w >> 3] = 960.0;
        HEAPF64[out_h >> 3] = 600.0;
        return;
    }
    const rect = stage.getBoundingClientRect();
    HEAPF64[out_w >> 3] = Math.max(1.0, rect.width);
    HEAPF64[out_h >> 3] = Math.max(1.0, rect.height);
});

EM_JS(void, fe3d_client_to_canvas_css, (double client_x, double client_y, double* out_x, double* out_y), {
    const canvas = document.getElementById("fe3d-canvas");
    if (!canvas) {
        HEAPF64[out_x >> 3] = client_x;
        HEAPF64[out_y >> 3] = client_y;
        return;
    }
    const rect = canvas.getBoundingClientRect();
    HEAPF64[out_x >> 3] = client_x - rect.left;
    HEAPF64[out_y >> 3] = client_y - rect.top;
});

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
        syncCanvasSize(desc.width, desc.height);

        EmscriptenWebGLContextAttributes attrs{};
        emscripten_webgl_init_context_attributes(&attrs);
        attrs.majorVersion = 2;
        attrs.minorVersion = 0;
        attrs.depth = EM_TRUE;
        attrs.stencil = EM_FALSE;
        attrs.antialias = EM_TRUE;
        attrs.alpha = EM_FALSE;
        attrs.preserveDrawingBuffer = EM_TRUE;
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

        (void)emscripten_set_mousedown_callback(kCanvasSelector, this, EM_TRUE, &Html5CanvasWindow::onMouseDown);
        (void)emscripten_set_mouseup_callback(
            EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, &Html5CanvasWindow::onMouseUp);
        (void)emscripten_set_mousemove_callback(
            EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, &Html5CanvasWindow::onMouseMove);
        (void)emscripten_set_wheel_callback(kCanvasSelector, this, EM_TRUE, &Html5CanvasWindow::onWheel);
        (void)emscripten_set_keydown_callback(
            EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, &Html5CanvasWindow::onKey);
        (void)emscripten_set_keyup_callback(
            EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, &Html5CanvasWindow::onKey);

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

    void pollEvents() override {
        syncCanvasSize(fb_size_.width, fb_size_.height);
    }

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
    void syncCanvasSize(std::uint32_t fallback_w, std::uint32_t fallback_h) {
        dpr_ = std::clamp(emscripten_get_device_pixel_ratio(), 1.0, 3.0);
        double css_w = static_cast<double>(fallback_w);
        double css_h = static_cast<double>(fallback_h);
        fe3d_get_viewport_css_size(&css_w, &css_h);

        const auto next_w = static_cast<std::uint32_t>(std::max(1.0, std::round(css_w * dpr_)));
        const auto next_h = static_cast<std::uint32_t>(std::max(1.0, std::round(css_h * dpr_)));
        if (next_w != fb_size_.width || next_h != fb_size_.height) {
            fb_size_.width = next_w;
            fb_size_.height = next_h;
            (void)emscripten_set_canvas_element_size(
                kCanvasSelector,
                static_cast<int>(fb_size_.width),
                static_cast<int>(fb_size_.height));
        }
    }

    void mapClientToFramebuffer(long client_x, long client_y, double& out_fb_x, double& out_fb_y) const {
        double css_x = 0.0;
        double css_y = 0.0;
        fe3d_client_to_canvas_css(
            static_cast<double>(client_x),
            static_cast<double>(client_y),
            &css_x,
            &css_y);
        out_fb_x = css_x * dpr_;
        out_fb_y = css_y * dpr_;
    }

    static bool mapMouseButton(unsigned short button, finaleffect::platform::MouseButton& out) noexcept {
        if (button == 0) {
            out = finaleffect::platform::MouseButton::Left;
            return true;
        }
        if (button == 1) {
            out = finaleffect::platform::MouseButton::Middle;
            return true;
        }
        if (button == 2) {
            out = finaleffect::platform::MouseButton::Right;
            return true;
        }
        return false;
    }

    static EM_BOOL onMouseDown(int /*event_type*/, const EmscriptenMouseEvent* ev, void* user_data) {
        auto* self = static_cast<Html5CanvasWindow*>(user_data);
        if (self == nullptr || ev == nullptr) {
            return EM_FALSE;
        }
        finaleffect::platform::MouseButton btn{};
        if (!mapMouseButton(ev->button, btn)) {
            return EM_FALSE;
        }
        self->pointer_captured_ = true;
        double fb_x = 0.0;
        double fb_y = 0.0;
        self->mapClientToFramebuffer(ev->clientX, ev->clientY, fb_x, fb_y);
        self->pending_events_.emplace_back(finaleffect::platform::MouseButtonEvent{
            .button = btn,
            .action = finaleffect::platform::KeyAction::Press,
            .x = fb_x,
            .y = fb_y,
            .mods = extractMouseMods(*ev),
        });
        // Allow default browser focus transition onto #fe3d-canvas so active UI <input> blurs cleanly.
        return EM_FALSE;
    }

    static EM_BOOL onMouseUp(int /*event_type*/, const EmscriptenMouseEvent* ev, void* user_data) {
        auto* self = static_cast<Html5CanvasWindow*>(user_data);
        if (self == nullptr || ev == nullptr || !self->pointer_captured_) {
            return EM_FALSE;
        }
        finaleffect::platform::MouseButton btn{};
        if (!mapMouseButton(ev->button, btn)) {
            return EM_FALSE;
        }
        self->pointer_captured_ = false;
        double fb_x = 0.0;
        double fb_y = 0.0;
        self->mapClientToFramebuffer(ev->clientX, ev->clientY, fb_x, fb_y);
        self->pending_events_.emplace_back(finaleffect::platform::MouseButtonEvent{
            .button = btn,
            .action = finaleffect::platform::KeyAction::Release,
            .x = fb_x,
            .y = fb_y,
            .mods = extractMouseMods(*ev),
        });
        return EM_FALSE;
    }

    static EM_BOOL onMouseMove(int /*event_type*/, const EmscriptenMouseEvent* ev, void* user_data) {
        auto* self = static_cast<Html5CanvasWindow*>(user_data);
        if (self == nullptr || ev == nullptr || !self->pointer_captured_) {
            return EM_FALSE;
        }
        double fb_x = 0.0;
        double fb_y = 0.0;
        self->mapClientToFramebuffer(ev->clientX, ev->clientY, fb_x, fb_y);
        self->pending_events_.emplace_back(finaleffect::platform::CursorMoveEvent{
            .x = fb_x,
            .y = fb_y,
        });
        return EM_FALSE;
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
        // Do not intercept keyboard typing when an editor <input>, <select>, or <textarea> has focus.
        if (fe3d_is_text_input_focused() != 0) {
            return EM_FALSE;
        }
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

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl_ctx_{0};
    finaleffect::platform::FramebufferSize fb_size_{0, 0};
    double dpr_{1.0};
    bool should_close_{false};
    bool pointer_captured_{false};
    std::vector<finaleffect::platform::InputEvent> pending_events_;
};

struct WebHostContext {
    finaleffect::platform::LocalFileSystem fs;
    finaleffect::core::Engine engine;
    Html5CanvasWindow window;
    std::unique_ptr<finaleffect::app::StudioSession> session;
    std::unique_ptr<finaleffect::app::EditorController> editor;
    int ready_state{0}; // 0 = initializing, 1 = ready, -1 = error
    std::string init_error;
    std::string state_json_buffer{"{}"};
    std::string export_json_buffer;

    explicit WebHostContext(const finaleffect::core::EngineConfig& cfg) : engine(cfg) {}
};

WebHostContext* g_web_ctx = nullptr;

void onBrowserFrame(void* user_data) {
    auto* ctx = static_cast<WebHostContext*>(user_data);
    if (ctx == nullptr || ctx->ready_state != 1) {
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

extern "C" {

EMSCRIPTEN_KEEPALIVE int fe3d_is_ready() {
    return g_web_ctx != nullptr ? g_web_ctx->ready_state : 0;
}

EMSCRIPTEN_KEEPALIVE const char* fe3d_get_init_error() {
    return g_web_ctx != nullptr ? g_web_ctx->init_error.c_str() : "";
}

EMSCRIPTEN_KEEPALIVE const char* fe3d_get_state_json() {
    if (g_web_ctx == nullptr || g_web_ctx->editor == nullptr) {
        return "{}";
    }
    g_web_ctx->state_json_buffer = g_web_ctx->editor->serializeStateJson();
    return g_web_ctx->state_json_buffer.c_str();
}

EMSCRIPTEN_KEEPALIVE int fe3d_dispatch_action(const char* action_json) {
    if (g_web_ctx == nullptr || g_web_ctx->editor == nullptr || action_json == nullptr) {
        return 0;
    }
    const auto st = g_web_ctx->editor->executeActionJson(action_json);
    return st.hasValue() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE const char* fe3d_export_inline_project() {
    if (g_web_ctx == nullptr || g_web_ctx->editor == nullptr) {
        return "";
    }
    auto res = g_web_ctx->editor->exportInlineProjectJson();
    if (!res) {
        g_web_ctx->export_json_buffer.clear();
        return "";
    }
    g_web_ctx->export_json_buffer = std::move(res).value();
    return g_web_ctx->export_json_buffer.c_str();
}

EMSCRIPTEN_KEEPALIVE int fe3d_save_bundle_to_vfs(const char* fe3d_vfs_path) {
    if (g_web_ctx == nullptr || g_web_ctx->editor == nullptr || fe3d_vfs_path == nullptr) {
        return 0;
    }
    const auto st = g_web_ctx->editor->saveBundleProject(fe3d_vfs_path, g_web_ctx->fs);
    return st.hasValue() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int fe3d_load_file_from_vfs(const char* fe3d_vfs_path) {
    if (g_web_ctx == nullptr || g_web_ctx->editor == nullptr || fe3d_vfs_path == nullptr) {
        return 0;
    }
    const auto st = g_web_ctx->editor->loadProjectFile(fe3d_vfs_path, g_web_ctx->fs);
    return st.hasValue() ? 1 : 0;
}

} // extern "C"

int main() {
    finaleffect::core::EngineConfig cfg;
    cfg.application_name = "Final Effect 3D Studio Web";

    auto ctx = std::make_unique<WebHostContext>(cfg);
    g_web_ctx = ctx.get();

    auto win_st = ctx->window.initialize({
        .title = "Final Effect 3D — Studio WebAssembly",
        .width = 960,
        .height = 600,
    });
    if (!win_st) {
        ctx->ready_state = -1;
        ctx->init_error = win_st.error().format();
        std::cerr << "[FinalEffect3D Web] Canvas init failed: " << ctx->init_error << '\n';
        ctx.release();
        return EXIT_FAILURE;
    }

    auto webgl_device = std::make_unique<finaleffect::rendering::WebGL2RenderDevice>(
        ctx->window.glProcLoader());
    ctx->session = std::make_unique<finaleffect::app::StudioSession>(
        ctx->window, std::move(webgl_device));

    auto init_st = ctx->engine.initialize(ctx->session.get());
    if (!init_st) {
        ctx->ready_state = -1;
        ctx->init_error = init_st.error().format();
        std::cerr << "[FinalEffect3D Web] Engine init failed: " << ctx->init_error << '\n';
        ctx->window.shutdown();
        ctx.release();
        return EXIT_FAILURE;
    }

    ctx->editor = std::make_unique<finaleffect::app::EditorController>(*ctx->session);
    ctx->ready_state = 1;

    // Release unique_ptr ownership into the Emscripten browser animation loop.
    emscripten_set_main_loop_arg(&onBrowserFrame, ctx.release(), 0, EM_TRUE);
    return EXIT_SUCCESS;
}
