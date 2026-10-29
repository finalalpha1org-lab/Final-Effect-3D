# Final Effect 3D

**Final Effect 3D** is an original, open-source 3D engine and creative studio foundation written in modern **C++20**. Its long-term engineering goal is to unify 3D modelling, animation, procedural generation, visual effects, compositing, non-linear video editing, simulation, and material authoring on a clean, modular, cross-platform architecture.

Final Effect 3D has its own codebase, architecture, coordinate conventions, rendering abstraction, versioned `.fe3d` project format, and content-addressed `.fe3dbin` binary geometry archive. It is **not** a fork or clone of Blender or any other existing creative suite.

---

## Current Project Status — Stage 1 & Stage 2 Foundation (`v0.2.0`)

This repository implements **Stage 1 (Core Engine & Build Foundation)** and **Stage 2 (Interactive Viewport, Undo/Redo Transactions, `.fe3dbin` Binary Assets & WebAssembly/WebGL2 Prototype)**.

### Implemented Subsystems

| Subsystem | Library / Target | Status | Summary |
| :--- | :--- | :--- | :--- |
| **Platform** | `finaleffect::platform` | Implemented & Verified | Isolated OS path resolution (Linux XDG, Windows AppData, macOS Library, Generic), file I/O, clocks, `IWindow` + portable `InputEvent` routing |
| **Core** | `finaleffect::core` | Implemented & Verified | Deterministic `Engine` lifecycle, `IApplication` separation, `Result<T>`/`Error`, severity-filtered `Logger`, `ResourceStats` telemetry |
| **Math** | `finaleffect::math` | Implemented & Verified | Right-handed Y-up `Vec2`/`Vec3`/`Vec4`/`Quat`/`Mat4`/`Transform`, `Ray`, `AABB`, slab ray-AABB & Möller–Trumbore ray-triangle intersections |
| **Scene & Picking** | `finaleffect::scene` | Implemented & Verified | Single-owner `Scene` graph, typed `ObjectId`/`MeshId`, world AABB computation, hierarchical `raycastScene` picking, and memory-bounded Undo/Redo `CommandStack` (`SetTransform`, `CreateObject`, `DeleteObject`, `ReparentObject`, `CompoundTransaction`) |
| **Project & `.fe3dbin`** | `finaleffect::project` | Implemented & Verified | Self-contained RFC 8259 JSON parser/serializer, `.fe3d` (`v1.1`, backward-compatible with `v1.0`), and content-addressed `.fe3dbin` (`v1.0`) binary geometry chunks with FNV-1a-64 deduplication & IEEE CRC-32 validation |
| **Rendering** | `finaleffect::rendering` | Implemented & Verified | API-agnostic `Renderer` + `IRenderDevice`, `NullRenderDevice`, and CPU 3D `SoftwareRenderDevice` rasterizer |
| **Desktop GL 3.3** | `finaleffect::rendering_opengl33` | Implemented (Adapter Verified) | OpenGL 3.3 Core Profile VAO/VBO/EBO + GLSL 330 shader backend (`GlfwWindow` enabled when system GLFW + OpenGL are present) |
| **WebGL 2.0 / GLES 3.0** | `finaleffect::rendering_webgl2` | Implemented (Adapter Verified) | WebGL 2.0 / OpenGL ES 3.0 VAO/VBO/EBO + GLSL ES 3.00 (`#version 300 es`, `glClearDepthf`) backend |
| **App / Viewport** | `finaleffect::app`, `finaleffect_desktop` | Implemented & Verified | `StudioSession` + `OrbitCameraController` (orbit, pan, dolly zoom, frame selection, click-to-pick, `Delete`, `Ctrl+Z`/`Ctrl+Y` undo/redo) |
| **WebAssembly Host** | `finaleffect_web` (`apps/web/`, `web/`) | Implemented (CI-targeted via Emscripten) | Emscripten HTML5 Canvas + WebGL 2.0 browser host sharing `core`, `math`, `scene`, `project`, `rendering`, and `app` unchanged |

### Planned for Future Stages (Not Yet Implemented)

* Interactive studio docking UI, 3D transform gizmo handles, and mesh-editing tools (`Stage 3+`).
* Browser WebGPU backend alongside the WebGL 2.0 baseline.
* Native iOS host + Metal rendering backend (`platform/ios/`).
* Native Android host + Vulkan / OpenGL ES rendering backend (`platform/android/`).
* High-throughput desktop Vulkan / Metal / Direct3D 12 backends, node-based VFX/compositing, animation timelines, and NLE sequencing.

---

## Repository Structure

```text
Final-Effect-3D/
├── .github/workflows/ci.yml     # Cross-platform CI (Linux, macOS, Windows, Xvfb GL3.3, Emscripten WASM)
├── CMakeLists.txt               # Root C++20 CMake build definition
├── CMakePresets.json            # Core, Desktop, and Emscripten WebAssembly presets
├── LICENSE                      # GNU General Public License v3.0 (preserved verbatim)
├── NOTICE                       # Release notice
├── README.md                    # Project overview
├── apps/
│   ├── desktop/                 # Desktop & headless verification host executable
│   └── web/                     # Emscripten HTML5 Canvas + WebGL 2.0 browser host
├── cmake/                       # Modular CMake platform, warning, and dependency scripts
├── docs/
│   ├── ARCHITECTURE.md          # Engine design, undo/redo, raycast, .fe3d/.fe3dbin specs
│   ├── BUILDING.md              # Reproducible desktop, headless, and Emscripten build instructions
│   ├── PLATFORM_ROADMAP.md      # Desktop, WebAssembly, iOS, and Android target strategy
│   └── THIRD_PARTY_LICENSES.md  # License compliance & third-party dependency inventory
├── include/finaleffect/         # Public C++20 engine headers (core, math, scene, rendering, platform, project, app)
├── platform/                    # OS-specific platform adapters (linux, windows, macos)
├── src/                         # Portable module implementations
├── tests/                       # 10 automated CTest unit and integration suites
└── web/
    └── index.html               # Minimal dark-theme HTML5 canvas shell for finaleffect_web
```

---

## Quick Start

Core engine compilation and unit testing require **zero network access** and **zero external third-party libraries** — only a C++20 compiler and CMake (`>= 3.22`).

### 1. Build and Test Portable Core Only

```bash
cmake --preset core-release
cmake --build --preset core-release --parallel
ctest --preset core-release
```

### 2. Build Desktop + GPU Adapters & Run All 10 Test Suites

```bash
cmake --preset desktop-release
cmake --build --preset desktop-release --parallel
ctest --preset desktop-release
```

* On headless machines or containers without X11/Wayland/OpenGL, run headless verification with `.fe3d` + `.fe3dbin` binary bundle export and PPM frame output:

```bash
./build/desktop-release/finaleffect_desktop --headless --frames 5 \
  --export-bundle build/showcase.fe3d \
  --render-ppm build/frame.ppm
```

### 3. Build Browser WebAssembly Target (Requires Emscripten SDK)

```bash
emcmake cmake -S . -B build/wasm-release -DCMAKE_BUILD_TYPE=Release \
  -DFINALEFFECT_BUILD_RENDERER_DESKTOP=OFF -DFINALEFFECT_BUILD_TESTS=OFF
cmake --build build/wasm-release --parallel
```

See [docs/BUILDING.md](docs/BUILDING.md) for full details across Linux, Windows, macOS, and Emscripten.

---

## Documentation

* [Architecture & Design Specification](docs/ARCHITECTURE.md)
* [Building & Testing Guide](docs/BUILDING.md)
* [Cross-Platform Roadmap (Desktop, WebAssembly, iOS, Android)](docs/PLATFORM_ROADMAP.md)
* [Open-Source & Third-Party License Inventory](docs/THIRD_PARTY_LICENSES.md)

---

## License

Final Effect 3D is licensed under the **GNU General Public License Version 3 (GPL-3.0)** — see [LICENSE](LICENSE) and [NOTICE](NOTICE) for full terms.
