# Final Effect 3D — Cross-Platform Target Strategy & Roadmap

## 1. Fundamental Separation of Runtime Targets

Final Effect 3D targets three distinct execution models around a single portable C++20 engine core:

1. **Native Desktop Applications** (Windows, Linux, macOS)
2. **Browser Execution** (WebAssembly via Emscripten + HTML5 Canvas / WebGL 2.0 host)
3. **Native Mobile Applications** (iOS and Android native app bundles)

> **Critical Architectural Rule**: Native desktop binaries (`.exe`, Linux ELF, macOS Mach-O) **cannot** execute directly inside desktop or mobile web browsers, nor does desktop OpenGL 3.3 code compile unchanged into a modern browser runtime without a WebGL2/WebGPU adaptation layer and an asynchronous browser event loop.

---

## 2. Per-Platform Architecture & Current Status (Stage 3)

| Target Platform | Stage 3 Status | Host / Window Layer | Rendering Backend Strategy |
| :--- | :--- | :--- | :--- |
| **Linux (x86_64 / arm64)** | Core, `EditorController`, Viewport, `.fe3dbin` & 11 Test Suites **Verified locally & in CI**; OpenGL 3.3 + GLFW verified under Xvfb in CI | `platform/linux/` + `GlfwWindow` / `HeadlessWindow` | OpenGL 3.3 Core baseline + CPU `SoftwareRenderDevice`; future Vulkan |
| **Windows (x64 / arm64)** | Core + `EditorController` + OpenGL 3.3 + WebGL2 adapter tests **Verified in CI (MSVC)** | `platform/windows/` + `GlfwWindow` / `HeadlessWindow` | OpenGL 3.3 Core baseline + CPU `SoftwareRenderDevice`; future Vulkan / Direct3D 12 |
| **macOS (Apple Silicon / Intel)** | Core + `EditorController` + OpenGL 3.3 + WebGL2 adapter tests **Verified in CI (AppleClang)** | `platform/macos/` + `GlfwWindow` / `HeadlessWindow` | OpenGL 3.3 Core forward-compatible prototype (deprecated by Apple); planned native **Metal** backend |
| **Web Browser (Desktop & Mobile Web)** | Connected Studio UI + `finaleffect_web` WASM + `WebGL2RenderDevice` **Compiled via Emscripten & Verified in Real Headless Chromium (Playwright)** | `Html5CanvasWindow` (`apps/web/main.cpp`) + Connected Studio DOM (`web/`) | **WebGL 2.0 (GLSL ES 3.00)** `IRenderDevice` baseline (`NegativeOneToOne` clip depth); planned **WebGPU** (`ZeroToOne` clip depth) |
| **iOS / iPadOS** | Architecture prepared; **not yet built** | Planned Objective-C++ / Swift host (`platform/ios/`) with `CAMetalLayer` & touch gestures | Planned native **Metal** `IRenderDevice` (`ZeroToOne` clip depth) |
| **Android** | Architecture prepared; **not yet built** | Planned NDK `GameActivity` / `ANativeWindow` host (`platform/android/`) | Planned **Vulkan** primary `IRenderDevice` with shared **OpenGL ES 3.0** (`WebGL2RenderDevice` lineage) fallback |

---

## 3. Shared Portable Subsystems Across Desktop & WebAssembly

Both `finaleffect_desktop` and `finaleffect_web` link the exact same unmodified portable libraries:

1. **Frame-Driven Non-Blocking Update Loop**: `core::Engine::stepFrame()` executes one discrete frame tick inside desktop `while (!window->shouldClose())` loops AND inside browser `requestAnimationFrame` (`emscripten_set_main_loop_arg`) callbacks.
2. **Unified Input & Viewport Controller**: `GlfwWindow`, `HeadlessWindow`, and `Html5CanvasWindow` emit identical `platform::InputEvent` structs consumed by `StudioSession` and `OrbitCameraController`.
3. **Low-Memory Binary Assets (`.fe3dbin`)**: Content-addressed binary geometry chunks avoid heavy JSON text parsing and excess heap allocations on memory-constrained laptops, browsers (32 MiB initial WASM heap), and future mobile devices.
4. **Dual Clip-Space Depth Convention**: `math::Mat4` and `scene::CameraComponent` natively support both `[-1, 1]` NDC depth (OpenGL 3.3, OpenGL ES 3.0, WebGL 2.0) and `[0, 1]` NDC depth (Metal, Vulkan, WebGPU, Direct3D 12).
