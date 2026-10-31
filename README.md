# Final Effect 3D

**Final Effect 3D** is an original, open-source 3D engine and creative studio foundation written in modern **C++20**. Its long-term engineering goal is to unify 3D modelling, animation, procedural generation, visual effects, compositing, non-linear video editing, simulation, and material authoring on a clean, modular, cross-platform architecture.

Final Effect 3D has its own codebase, architecture, coordinate conventions, rendering abstraction, versioned `.fe3d` project format, and content-addressed `.fe3dbin` binary geometry archive. It is **not** a fork or clone of Blender or any other existing creative suite.

---

## Current Project Status — Stage 5 (`v0.5.0`)

This repository implements **Stages 1–5**:
- **Stage 1 (`v0.1.0`)**: Core Engine, Math, Scene Graph, Rendering Abstraction & Cross-Platform Build Foundation.
- **Stage 2 (`v0.2.0`)**: Interactive Viewport, Raycast Picking, Undo/Redo `CommandStack`, `.fe3dbin` Binary Assets & WebGL 2.0 Backend.
- **Stage 3 (`v0.3.0`)**: Connected Studio Editor UI, `EditorController`, 3D Selection Highlighting & Real-Browser WebAssembly Verification.
- **Stage 4 (`v0.4.0`)**: Interactive 3D Transform Gizmos, Polygon Mesh Editing (`EditableMesh`), Metallic-Roughness PBR Material System + Live 3D Sphere Preview, Modular Workspace Architecture & `.fe3d` Schema `1.2`.
- **Stage 5 (`v0.5.0`)**: Complete Blender/Maya-Inspired Creative Application UI/UX Architecture — Real Engine Splash Screen, Main Home & 6-Studio Selector (`3D Studio` operational + 5 honest planned Studio foundations), Reusable 3D Studio Project Launcher with real Recent Projects & WebGL2 thumbnails, 12 Shared-Scene Workspace Tabs, Customizable/Resizable Docks, and Unified Desktop + Mobile Landscape + Mobile Portrait + iPhone Safari "Request Desktop Website" Support.

### Stage 1 — Autodesk Maya-Style Interface Redesign (`v0.5.0`)

The 3D Studio main window has been rebuilt as a professional Maya-Classic production workspace while preserving the existing C++20 engine, WebGL 2.0 renderer and `.fe3d` pipeline:

- **Main Menu Bar** with File, Edit, Create, Select, Modify, Display, Windows and Help menus plus context-sensitive **Menu Sets** (Modeling `F2`, Rigging `F3`, Animation `F4`, FX `F5`, Rendering `F6`) that rebuild the *Mesh*, *Edit Mesh*, *Mesh Display*, *UV*, *Grouping*, *Pivot*, *Node*, *Keyframes*, *Playback*, *Emitters*, *Lights*, *Environment*, *Cameras* and *Render* menus without resetting the scene, selection, materials or Undo stack. Every menu item is a real command with enabled/disabled state.
- **Status Line** with Object/Vertex/Edge/Face/UV selection masks, type masks (meshes, lights, cameras, helpers), Template/Reference display states, Grid `X` / Point `V` / Curve `C` / Live-Surface snapping, quick create/duplicate/delete/frame, render-frame, and a live **command search** field.
- **Customizable Shelf system** — 9 built-in shelves (General, Poly Modeling, Sculpting, Rigging, Animation, UV Editing, Shading, Rendering, FX) with executable icons, right-click command options (execute / move left / move right / remove), add-command dialog, custom shelf creation, renaming, deletion, factory restore, and independent `localStorage` persistence.
- **Left Tool Box** (Select, Component Select, Multi-Component, Move `W`, Rotate `E`, Scale `R`, Universal, Show Manipulator `T`, Edit Pivot `D`, Last Tool `Y`, World/Local space) wired to the real C++ `TransformGizmo`/`StudioSession` tool system.
- **Central WebGL 2.0 Viewport** with Maya viewport panel menus (Panels / View / Shading / Lighting / Show), camera selector, Wireframe `4` / Shaded `5` / Textured `6` / All Lights `7` / X-Ray / Isolate / Wireframe-on-Shaded display modes, resolution gate, 16:9 camera gate, the retained Stage 5.3 navigation gizmo, and a **RMB marking menu** whose quadrants switch Object/Vertex/Edge/Face/UV modes.
- **Outliner** with hierarchical expand/collapse, multi-select, inline rename, lock/unlock, hide/show, context menu (rename, duplicate, group, parent, unparent, lock, hide, delete), node drag-and-drop reparenting (HTML5 DnD **and** pointer drag) and Scene-Root drop target.
- **Dedicated Channel Box** (Translate/Rotate/Scale X-Y-Z, Visibility, per-channel lock buttons, shape summary, custom keyable attributes) and **Display Layer Editor** (create from selection, assign, visibility, Normal/Template/Reference cycling, rename, delete — never deleting scene objects).
- **Tabbed Attribute Editor** with dynamic per-node tabs (transform, shape, material, environment, diagnostics) that always follow the current selection and preserve all Stage 5.4 lighting, shadow, HDRI/IBL and Light Linking controls.
- **Time Slider / Range Slider / Playback** bound to the real C++20 animation engine (`timeline_*` actions): frame scrubbing, keyframe ticks, Set/Delete Key (`S`), Auto Key toggle, Play/Pause, step, jump-to-key, Start/End, FPS, loop modes and animated scene-start / playback-range fields.
- **Safe Command Line** with a structured parser (no `eval`, no shell): `polyCube`, `polySphere`, `polyCylinder`, `polyCone`, `polyTorus`, `polyPlane`, `camera`, `directionalLight`, `pointLight`, `spotLight`, `areaLight`, `select`, `move/rotate/scale`, `rename`, `duplicate`, `delete`, `group`, `parent`, `unparent`, `freezeTransforms`, `centerPivot`, `wireframe/shaded/textured/lighting`, `xray`, `isolateSelect`, `frameSelected`, `frameAll`, `currentTime`, `setKey`, `play`, `stop`, `undo`, `redo`, `help`, plus command history and error feedback.
- **Spacebar Hotbox** (workspace, tool, camera-view, shading and menu-set regions), **marking menus**, **Hotkey Editor** with user-remappable shortcuts (persisted, suppressed while typing in text fields) and a full **Help Line**.
- **Docking & workspace management**: resizable left Outliner/Tool-Settings dock, right Channel Box + Layer Editor + Attribute Editor dock (stacked / Channel-Box-only / Attribute-Editor-only modes, splitter-resizable), bottom Time-Slider dock, undockable floating panels, collapse/restore per region, save/restore/reset workspace layouts and Maya Classic default.

### Implemented Subsystems

| Subsystem | Library / Target | Status | Summary |
| :--- | :--- | :--- | :--- |
| **Platform** | `finaleffect::platform` | Implemented & Verified | Isolated OS path resolution (Linux XDG, Windows AppData, macOS Library, Generic), file I/O, clocks, `IWindow` + portable `InputEvent` routing (`W`/`E`/`R`/`Tab`/`1..4`/`Escape` + modifier masks) |
| **Core** | `finaleffect::core` | Implemented & Verified | Deterministic `Engine` lifecycle (`v0.5.0`), `IApplication` separation, `Result<T>`/`Error`, severity-filtered `Logger`, `ResourceStats` telemetry (including `material_count` & `gpu_buffer_update_count`) |
| **Math** | `finaleffect::math` | Implemented & Verified | Right-handed Y-up `Vec2`/`Vec3`/`Vec4`/`Quat` (XYZ Euler conversion)/`Mat4`/`Transform`, `Ray`, `AABB`, ray-AABB & Möller–Trumbore ray-triangle intersections |
| **Scene, Editable Mesh & Materials** | `finaleffect::scene` | Implemented & Verified | Single-owner `Scene` graph, polygon-capable `EditableMesh` (`Vertex`/`Edge`/`Face` selection, extrusion, dissolve/delete, boundary-loop fill, per-face materials, topology validation), `PbrMaterial` (`MaterialId`), and bounded Undo/Redo `CommandStack` |
| **Project & `.fe3dbin`** | `finaleffect::project` | Implemented & Verified | Self-contained RFC 8259 JSON codec, `.fe3d` (`v1.2`, backward-compatible with `v1.0` & `v1.1`, lossless preservation of unknown future fields), `WorkspaceState` persistence, and content-addressed `.fe3dbin` (`v1.0`) binary geometry chunks with FNV-1a-64 & IEEE CRC-32 |
| **Rendering & Material Preview** | `finaleffect::rendering` | Implemented & Verified | API-agnostic `Renderer` + `IRenderDevice` (`updateMesh`, `beginOverlayPass`, multi-material submesh draws, alpha blending), CPU Cook-Torrance `SoftwareRenderDevice`, and live 3D sphere `MaterialPreviewRenderer` |
| **Desktop GL 3.3** | `finaleffect::rendering_opengl33` | Implemented & Verified | OpenGL 3.3 Core Profile VAO/VBO/EBO (`glBufferSubData` in-place mesh updates) + GLSL 330 Cook-Torrance GGX PBR shader backend (`GlfwWindow` when system GLFW + OpenGL are present) |
| **WebGL 2.0 / GLES 3.0** | `finaleffect::rendering_webgl2` | Implemented & Verified | WebGL 2.0 / OpenGL ES 3.0 VAO/VBO/EBO (`glBufferSubData` in-place mesh updates) + GLSL ES 3.00 (`#version 300 es`) Cook-Torrance GGX PBR shader backend |
| **App, Gizmos & Workspace** | `finaleffect::app`, `finaleffect_desktop` | Implemented & Verified | `TransformGizmo` (3D Move/Rotate/Scale in World & Local space), `WorkspaceManager` (all 12 creative studio workspace presets, dock split ratios, panel ownership & non-duplicating input routing), `StudioSession` (mouse + touch/pointer camera & gizmo hooks), `OrbitCameraController`, and `EditorController` (`ProjectTemplate` launcher actions) |
| **Connected Multi-Device Creative Suite** | `finaleffect_web` (`apps/web/`, `web/`) | Implemented & Browser-Verified | 3-Level Creative Software UI (`Splash -> Main Home / 6-Studio Selector / Project Launcher -> Blender-Inspired 3D Studio`) with 12 shared-scene workspace tabs, Left Tool Shelf, Priority WebGL 2.0 3D Viewport, Right Outliner + Vertical-Tabbed Properties & 96×96 PBR Sphere Preview, Resizable Bottom Multi-Editor Dock, unified Touch/Pointer gestures, and Desktop + Mobile Landscape + Mobile Portrait + Safari Desktop Mode verified in Chromium (`web/e2e/`) |

### Planned for Future Stages (Deferred Beyond Stage 5)

* **Stage 6+**: Operational implementations of the 5 planned Studios (`Landscaping Studio`, `Character Studio`, `2D Studio`, `Sculpting Studio`, `Shade Studio`), procedural node editors (`Shader Editor`, `Geometry Nodes`, `Compositor`), interactive 2D `UV Editor` unwrapping, non-destructive Modifier Stack operators, keyframe curve channels in the Timeline/Dope Sheet, PBR 2D image texture maps & IBL cubemaps, native desktop GPU-rendered dockable GUI chrome, WebGPU backend, and native iOS (Metal) / Android (Vulkan / GLES 3.0) hosts.

---

## Repository Structure

```text
Final-Effect-3D/
├── .github/workflows/ci.yml     # 8-job CI (Linux GCC/Clang, macOS AppleClang, Windows MSVC, Xvfb GL3.3, WASM + Chromium E2E)
├── CMakeLists.txt               # Root C++20 CMake build definition (v0.5.0)
├── CMakePresets.json            # Core, Desktop, and Emscripten WebAssembly presets
├── LICENSE                      # GNU General Public License v3.0 (preserved verbatim)
├── NOTICE                       # Release notice (preserved verbatim)
├── README.md                    # Project overview
├── apps/
│   ├── desktop/                 # Desktop GLFW & headless verification host executable
│   └── web/                     # Emscripten HTML5 Canvas + WebGL 2.0 Studio bridge, boot telemetry & touch C-ABI exports
├── cmake/                       # Modular CMake platform, warning, and dependency scripts
├── docs/
│   ├── ARCHITECTURE.md          # Engine design, Stage 5 3-level UI/UX, 12 workspaces, gizmos, EditableMesh, PBR, .fe3d v1.2
│   ├── BUILDING.md              # Reproducible desktop, headless, Emscripten, and Playwright instructions
│   ├── PLATFORM_ROADMAP.md      # Desktop, WebAssembly, Mobile Web, iOS, and Android target strategy
│   └── THIRD_PARTY_LICENSES.md  # License compliance & third-party dependency inventory
├── include/finaleffect/         # Public C++20 engine headers (core, math, scene, rendering, platform, project, app)
├── platform/                    # OS-specific platform adapters (linux, windows, macos)
├── src/                         # Portable module implementations
├── tests/                       # 15 automated CTest unit and integration suites
└── web/
    ├── index.html               # 3-Level Splash + Main Home / Launcher + Blender-inspired 3D Studio shell
    ├── editor.css               # High-density dark pro design system (Desktop, Mobile Landscape & Mobile Portrait)
    ├── studios.js               # Extensible 6-Studio registry, 12 workspace definitions, vector icons & IndexedDB recents
    ├── editor.js                # Unified C++20 WASM <-> Studio UI, touch gesture & responsive layout controller
    └── e2e/                     # Multi-device Playwright WebGL 2.0 E2E suite (Desktop, Mobile, Tablet & Safari Desktop Mode)
```

---

## Quick Start

Core engine compilation and unit testing require **zero network access** and **zero external third-party libraries** — only a C++20 compiler and CMake (`>= 3.22`).

### 1. Build and Test Portable Core + Editing Subsystems

```bash
cmake --preset core-release
cmake --build --preset core-release --parallel
ctest --preset core-release
```

### 2. Build Desktop + GPU Adapters & Run All 15 Test Suites

```bash
cmake --preset desktop-release
cmake --build --preset desktop-release --parallel
ctest --preset desktop-release
```

* On headless machines or containers without X11/Wayland/OpenGL, run headless verification with `.fe3d` (`v1.2`) + `.fe3dbin` binary bundle export and Cook-Torrance software-rendered PPM frame output:

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
