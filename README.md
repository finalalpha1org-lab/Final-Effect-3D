# Final Effect 3D

**Final Effect 3D** is an original, open-source 3D engine and creative studio foundation written in modern **C++20**. Its long-term engineering goal is to unify 3D modelling, animation, procedural generation, visual effects, compositing, non-linear video editing, simulation, and material authoring on a clean, modular, cross-platform architecture.

Final Effect 3D has its own codebase, architecture, coordinate conventions, rendering abstraction, and versioned `.fe3d` project format. It is **not** a fork or clone of Blender or any other existing creative suite.

---

## Current Project Status — Stage 1 Foundation (`v0.1.0`)

This repository currently implements **Stage 1: C++ Core Engine Foundation & Cross-Platform Build Architecture**. It is an engine and build foundation, not yet an end-user 3D authoring application.

### Implemented & Verified in Stage 1

| Subsystem | Library Target | Status | Summary |
| :--- | :--- | :--- | :--- |
| **Platform** | `finaleffect::platform` | Implemented | Isolated OS path resolution (Linux XDG, Windows AppData, macOS Library, Generic), file I/O, monotonic & manual clocks, window abstraction |
| **Core** | `finaleffect::core` | Implemented | Deterministic `Engine` lifecycle, `IApplication` separation, `Result<T>`/`Error` reporting, severity-filtered non-singleton `Logger`, `ResourceStats` telemetry |
| **Math** | `finaleffect::math` | Implemented | Right-handed Y-up `Vec2`, `Vec3`, `Vec4`, `Quat` (with safe slerp/antipodal handling), column-major `Mat4` (`[-1,1]` and `[0,1]` clip depth), `Transform` |
| **Scene** | `finaleffect::scene` | Implemented | Single-owner `Scene` graph, typed `ObjectId`/`MeshId`, deduplicated `MeshAsset` storage, cycle-checked parent-child hierarchy, `CameraComponent`, `DirectionalLightComponent`, procedural primitives |
| **Project** | `finaleffect::project` | Implemented | Self-contained RFC 8259 JSON DOM/parser/serializer and versioned `.fe3d` (`v1.0`) project serializer with strict schema, numeric, and hierarchy validation |
| **Rendering** | `finaleffect::rendering` | Implemented | API-agnostic `Renderer` + `IRenderDevice` boundary, `NullRenderDevice`, and deterministic CPU 3D `SoftwareRenderDevice` rasterizer |
| **Desktop GL 3.3** | `finaleffect::rendering_opengl33` | Implemented (Optional) | OpenGL 3.3 Core Profile VAO/VBO/EBO + GLSL 330 shader backend (`GlfwWindow` enabled when system GLFW + OpenGL are present) |
| **App / Host** | `finaleffect::app`, `finaleffect_desktop` | Implemented | `StudioSession` + starter 3D showcase scene + desktop/headless host CLI |

### Planned for Future Stages (Not Yet Implemented)

* Interactive studio UI, viewport gizmos, and tool system (`Stage 2+`).
* Browser WebAssembly build via Emscripten + WebGL2 (and future WebGPU) adapter (`apps/web/`, `web/`).
* Native iOS host + Metal rendering backend (`platform/ios/`).
* Native Android host + Vulkan / OpenGL ES rendering backend (`platform/android/`).
* High-throughput desktop Vulkan / Metal / Direct3D 12 backends, binary geometry/texture containers, modelling, animation, VFX, and compositing nodes.

---

## Repository Structure

```text
Final-Effect-3D/
├── .github/workflows/ci.yml     # Cross-platform CI (Linux, macOS, Windows + Xvfb OpenGL)
├── CMakeLists.txt               # Root C++20 CMake build definition
├── CMakePresets.json            # Standard core-only and desktop Debug/Release presets
├── LICENSE                      # GNU General Public License v3.0 (preserved verbatim)
├── NOTICE                       # Release notice
├── README.md                    # Project overview
├── apps/
│   └── desktop/                 # Desktop & headless verification host executable
├── cmake/                       # Modular CMake platform, warning, and dependency scripts
├── docs/
│   ├── ARCHITECTURE.md          # Engine design, ownership rules, coordinate & project specs
│   ├── BUILDING.md              # Reproducible build, preset, and CLI instructions
│   ├── PLATFORM_ROADMAP.md      # Desktop, WebAssembly, iOS, and Android target strategy
│   └── THIRD_PARTY_LICENSES.md  # License compliance & third-party dependency inventory
├── include/finaleffect/         # Public C++20 engine headers (core, math, scene, rendering, platform, project, app)
├── platform/                    # OS-specific platform adapters (linux, windows, macos)
├── src/                         # Portable module implementations
└── tests/                       # Automated unit and integration test suites
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

### 2. Build Desktop Target & Run Tests

```bash
cmake --preset desktop-release
cmake --build --preset desktop-release --parallel
ctest --preset desktop-release
```

* If system **GLFW (`>= 3.3`)** and **OpenGL** development headers are installed, `./build/desktop-release/finaleffect_desktop` launches an interactive **OpenGL 3.3 Core** 3D window displaying the rotating Final Effect 3D starter scene.
* On headless machines or containers without X11/Wayland/OpenGL, run headless verification with the built-in CPU reference 3D rasterizer:

```bash
./build/desktop-release/finaleffect_desktop --headless --frames 5 \
  --export-project build/showcase.fe3d \
  --render-ppm build/frame.ppm
```

See [docs/BUILDING.md](docs/BUILDING.md) for detailed instructions on Linux, Windows, and macOS.

---

## Documentation

* [Architecture & Design Specification](docs/ARCHITECTURE.md)
* [Building & Testing Guide](docs/BUILDING.md)
* [Cross-Platform Roadmap (Desktop, WebAssembly, iOS, Android)](docs/PLATFORM_ROADMAP.md)
* [Open-Source & Third-Party License Inventory](docs/THIRD_PARTY_LICENSES.md)

---

## License

Final Effect 3D is licensed under the **GNU General Public License Version 3 (GPL-3.0)** — see [LICENSE](LICENSE) and [NOTICE](NOTICE) for full terms.
