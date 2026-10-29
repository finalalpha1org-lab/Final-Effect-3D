# Final Effect 3D — Cross-Platform Target Strategy & Roadmap

## 1. Fundamental Separation of Runtime Targets

Final Effect 3D targets three distinct execution models around a single portable C++20 engine core:

1. **Native Desktop Applications** (Windows, Linux, macOS)
2. **Browser Execution** (WebAssembly via Emscripten + browser graphics/DOM host)
3. **Native Mobile Applications** (iOS and Android native app bundles)

> **Critical Architectural Rule**: Native desktop binaries (`.exe`, Linux ELF, macOS Mach-O) **cannot** execute directly inside desktop or mobile web browsers, nor does desktop OpenGL 3.3 code compile unchanged into a modern browser runtime without a WebGL2/WebGPU adaptation layer and an asynchronous browser event loop.

---

## 2. Per-Platform Architecture & Current Status

| Target Platform | Stage 1 Status | Host / Window Layer | Rendering Backend Strategy |
| :--- | :--- | :--- | :--- |
| **Linux (x86_64 / arm64)** | Core + Tests **Verified**; OpenGL 3.3 + GLFW implemented | `platform/linux/` + `GlfwWindow` / `HeadlessWindow` | OpenGL 3.3 Core baseline + CPU `SoftwareRenderDevice`; future Vulkan |
| **Windows (x64 / arm64)** | Core + OpenGL 3.3 + GLFW configured & CI-targeted | `platform/windows/` + `GlfwWindow` / `HeadlessWindow` | OpenGL 3.3 Core baseline + CPU `SoftwareRenderDevice`; future Vulkan / Direct3D 12 |
| **macOS (Apple Silicon / Intel)** | Core + OpenGL 3.3 + GLFW configured & CI-targeted | `platform/macos/` + `GlfwWindow` / `HeadlessWindow` | OpenGL 3.3 Core forward-compatible prototype (deprecated by Apple); planned native **Metal** backend |
| **Web Browser (Desktop & Mobile Web)** | Architecture prepared; **not yet built** | Planned Emscripten host (`apps/web/`, `web/`) using `emscripten_set_main_loop` | Planned **WebGL2** `IRenderDevice` baseline (`NegativeOneToOne` clip depth), followed by **WebGPU** (`ZeroToOne` clip depth) |
| **iOS / iPadOS** | Architecture prepared; **not yet built** | Planned Objective-C++ / Swift host (`platform/ios/`) with `CAMetalLayer` & touch input | Planned native **Metal** `IRenderDevice` (`ZeroToOne` clip depth) |
| **Android** | Architecture prepared; **not yet built** | Planned NDK `GameActivity` / `ANativeWindow` host (`platform/android/`) | Planned **Vulkan** primary `IRenderDevice` with **OpenGL ES 3.x** fallback for older GPUs |

---

## 3. How Stage 1 Prepares for WebAssembly, iOS, and Android

Rather than creating empty stub folders and claiming cross-platform support prematurely, Stage 1 establishes the concrete technical prerequisites those targets require:

1. **Frame-Driven Non-Blocking Update Loop**: `core::Engine::stepFrame()` executes one discrete frame tick rather than trapping execution inside an internal infinite `while (true)` loop. This maps directly onto:
   * Browser `requestAnimationFrame` / `emscripten_set_main_loop_arg`
   * iOS `CADisplayLink` / `MTKViewDelegate::drawInMTKView:`
   * Android `Choreographer` / `GameActivity` render callbacks
2. **Graphics-API-Free Core & Scene**: `finaleffect_core`, `finaleffect_math`, `finaleffect_scene`, `finaleffect_project`, and `finaleffect_rendering` have zero dependency on desktop windowing or OpenGL headers.
3. **Dual Clip-Space Depth Convention**: `math::Mat4` and `scene::CameraComponent` natively compute projections for both `[-1, 1]` NDC depth (OpenGL 3.3, OpenGL ES, WebGL2) and `[0, 1]` NDC depth (Metal, Vulkan, WebGPU, Direct3D 12), queried per device via `IRenderDevice::capabilities().clip_space_depth`.
4. **Abstracted File & Storage Services**: Project load/save operates through `platform::IFileSystem`, enabling virtualized Emscripten MEMFS/IDBFS/OPFS storage in browsers and sandboxed app-container storage on iOS and Android without rewriting `ProjectSerializer`.
5. **No Global Singletons**: Multiple `Engine` or `Scene` instances can coexist or be cleanly torn down and recreated within mobile app lifecycle suspension/restoration events.
