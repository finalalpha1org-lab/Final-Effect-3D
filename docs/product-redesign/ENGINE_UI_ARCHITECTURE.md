# Final Effect 3D — Modular C++20 Engine & UI Architecture

**Document ID:** `FE3D-SPEC-09`  
**Stage:** Stage 0 — Product Reset, Original Hybrid UI Design & Architecture Specification  
**Core Principle:** Authoritative C++20 Engine · Thin Data-Driven UI · Strict Module Isolation

---

## 1. Layered Architectural Diagram

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                              PRESENTATION & HOST SHELL LAYER                           │
│  ┌───────────────────────────────────────┐  ┌────────────────────────────────────────┐ │
│  │  Desktop Web Host (apps/web + web/src)│  │ Native Desktop Host (apps/desktop + ui)│ │
│  │  • Modular TypeScript/ES6 Area Shell  │  │ • GLFW + OpenGL 3.3 Core Window        │ │
│  │  • WebGL 2.0 Canvas + HiDPI Observer  │  │ • Desktop Studio UI Layer (Shared      │ │
│  │  • IndexedDB / File System Access API │  │   AreaLayout + Command/State Bridge)   │ │
│  └───────────────────┬───────────────────┘  └───────────────────┬────────────────────┘ │
└──────────────────────┼──────────────────────────────────────────┼──────────────────────┘
                       │     Typed Action & Dirty-Domain Bridge   │
                       ▼                                          ▼
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        APPLICATION & EDITOR ORCHESTRATION LAYER                        │
│                                   (finaleffect::app)                                   │
│  ┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐ ┌────────────────────┐ │
│  │ WorkspaceManager │ │  EditorRegistry  │ │   ToolRegistry   │ │ Shortcut & Input   │ │
│  │ (Recursive Area  │ │  & SubControllers│ │ (Select/Gizmo/   │ │ Router             │ │
│  │  Split Tree)     │ │ (Model/Anim/Mat) │ │  Modal Operators)│ │ (Hybrid/Bln/Maya)  │ │
│  └────────┬─────────┘ └────────┬─────────┘ └────────┬─────────┘ └─────────┬──────────┘ │
│           └────────────────────┴──────────┬─────────┴─────────────────────┘            │
│                                           ▼                                            │
│                      StudioSession (1..4 ViewportSlots + CommandBus)                   │
└─────────────────────────────┬───────────────────────────┬──────────────────────────────┘
                              ▼                           ▼
┌─────────────────────────────────────────┐ ┌────────────────────────────────────────────┐
│      AUTHORITATIVE DOMAIN & DATA        │ │         PORTABLE RENDERING ENGINE          │
│     (finaleffect::scene & project)      │ │          (finaleffect::rendering)          │
│ • Scene Graph & SelectionSystem         │ │ • Renderer (Multi-Viewport Scissor Pass)   │
│ • CommandStack (Undo/Redo/Coalesce)     │ │ • LightingSystem (PCF Shadow Atlas + IBL)  │
│ • EditableMesh + MeshBVH + ModelingOps  │ │ • EditorGrid & MaterialPreviewRenderer     │
│ • ModifierStack & NodeGraph Core        │ │ • IRenderDevice                            │
│ • PbrMaterial, Camera, Lights           │ │    ├─ OpenGL33RenderDevice (Desktop)       │
│ • AnimationSystem, Armature, Physics    │ │    ├─ WebGL2RenderDevice   (Web)           │
│ • ProjectSerializer (.fe3d / .fe3dbin)  │ │    └─ SoftwareRenderDevice (Headless/CI)   │
└─────────────────────────────────────────┘ └────────────────────────────────────────────┘
                              │                           │
                              ▼                           ▼
┌────────────────────────────────────────────────────────────────────────────────────────┐
│               FOUNDATION LAYER (finaleffect::core, math, platform)                     │
│  Result<T>/Status · Logger · Engine · Vec/Quat/Mat4/Ray/AABB · IWindow · IFileSystem   │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Responsibility Matrix of All 21 Conceptual Modules

| # | Module | Namespace / Path | Authoritative Ownership & Boundary |
| :-: | :--- | :--- | :--- |
| 1 | **Application Core** | `finaleffect::core` | Frame loop, `Engine` RAII lifecycle, `Result<T>`/`Status`, `Logger`, `ResourceStats`, `AppPreferences` model. |
| 2 | **Window Management** | `finaleffect::platform` | `IWindow`, `GlfwWindow`, `HeadlessWindow`, `Html5CanvasWindow`, HiDPI framebuffer scale, OS drag-drop events. |
| 3 | **Workspace Manager** | `finaleffect::app::workspace` | Recursive `AreaNode` binary split tree, tabbed editor stacks, floating panel descriptors, workspace presets, JSON persistence. |
| 4 | **Editor Framework** | `finaleffect::app::editors` | Registry of the 13 editor descriptors (`EditorId`, header schema, valid modes, context menus, shortcut scope). |
| 5 | **UI Component System** | `web/src/ui/` & `desktop/ui/` | Stateless visual widgets (`NumericScrub`, `Tree`, `NodeCanvas`, `RadialCompass`, `Tabs`) driven strictly by C++ state projections. |
| 6 | **Input & Shortcut Manager** | `finaleffect::app::input` | Canonical keymap bindings (`FE3D Hybrid`, `Blender`, `Maya`, custom user overrides), input focus guard, pointer capture arbitration across multi-viewports. |
| 7 | **Scene System** | `finaleffect::scene` | `Scene`, `SceneObject`, parent-child transforms, Display Layers, Collections, object lifecycle & IDs. |
| 8 | **Selection System** | `finaleffect::scene` | `Lead Active Object` + `Selected Objects` set + per-mode `MeshElementSelection` + `ObjectSelectionMask` + 3D raycast & 2D frustum marquee queries. |
| 9 | **Tool System** | `finaleffect::app::tools` | Active modal tool state machine (`ITool`: `onActivate`, `onPointerDown/Move/Up`, `onCancel`, `buildOverlays`, `lastOperatorParams` for Adjust-Last-Action HUD). |
| 10 | **Command System** | `finaleffect::app::commands` | Typed action dispatcher (`CommandBus`) + deterministic `FE3D` text command parser (`executeCommandLine`), logging every action for repeatability. |
| 11 | **Undo/Redo** | `finaleffect::scene::CommandStack` | Byte-bounded (`16–64 MB`) reversible `ICommand` history, drag coalescing, and `amendLastCommand` for the Operator Adjust HUD. |
| 12 | **Geometry System** | `finaleffect::scene` | `EditableMesh` (n-gon/quad manifold topology, UVs, vertex colors, normals), `MeshData` triangulation, `MeshBVH` spatial index, `ModifierStack` pipeline. |
| 13 | **Modeling System** | `finaleffect::scene::modeling` | Pure C++ topology operators (`extrude`, `bevel`, `bridge`, `insertEdgeLoop`, `multiCut`, `mergeVertices`, `combine`, `separate`) wrapped in `ModifyMeshCommand`. |
| 14 | **Material System** | `finaleffect::scene` | `PbrMaterial`, texture asset handles, live `MaterialPreviewRenderer`, and Shader Graph compilation to GLSL. |
| 15 | **Node System** | `finaleffect::nodes` | Reusable typed DAG kernel (`NodeGraph`, `NodeId`, `SocketId`, topological sort, cycle detection, JSON serialization) shared by **Shader Nodes**, **Geometry Nodes**, and **Compositor Nodes**! |
| 16 | **Rendering System** | `finaleffect::rendering` | Multi-viewport `Renderer`, `LightingSystem` (PCF Shadow Atlas + HDRI IBL), `EditorGrid`, AOV debug passes, shared GLSL chunk library across `OpenGL33` & `WebGL2`. |
| 17 | **Camera System** | `finaleffect::scene` & `app` | `CameraComponent` (Persp/Ortho, FOV, clip planes, sensor gate) + per-sub-panel `OrbitCameraController`. |
| 18 | **Animation System** | `finaleffect::animation` | `TimelineSettings`, per-channel Bezier `AnimationCurve`, Dope Sheet key summaries, `ArmatureComponent` & skinning evaluator. |
| 19 | **Physics System** | `finaleffect::physics` | Deterministic Rigid Body, Collider, Particle, and Cloth solvers + frame-indexed `SimulationCache`. |
| 20 | **Project & Asset Mgmt** | `finaleffect::project` | Recent projects index, Autosave ring buffer, Crash Recovery marker, Asset Library catalog, missing-resource resolver. |
| 21 | **Serialization** | `finaleffect::project` | Versioned `.fe3d` JSON (`v1.0`–`v1.4+`) + `.fe3dbin` CRC32 binary chunk container + lossless `ProjectPreservedFields`. |

---

## 3. Decomposing the `EditorController` Monolith & Dirty-Domain Bridge

### 3.1 Domain Sub-Controllers (Refactoring `5,191`-LOC `editor_controller*.cpp`)
Instead of appending every future feature into `editor_controller.cpp`, `EditorController` becomes a lightweight **Coordinator & Facade** delegating to focused C++ files (`< 600` LOC each):
* `src/app/controllers/project_hub_controller.cpp`
* `src/app/controllers/hierarchy_layer_controller.cpp`
* `src/app/controllers/transform_channel_controller.cpp`
* `src/app/controllers/modeling_controller.cpp`
* `src/app/controllers/material_env_controller.cpp`
* `src/app/controllers/animation_controller.cpp`
* `src/app/controllers/workspace_area_controller.cpp`

### 3.2 Dirty-Domain Delta Bridge (High-FPS Scalability)
Currently `fe3d_get_state_json()` serializes the entire scene on every revision. The upgraded bridge introduces a bitmask `DirtyDomainMask`:

```cpp
enum DirtyDomain : std::uint32_t {
    kNone        = 0,
    kProject     = 1u << 0, // Metadata, dirty flag, warnings
    kOutliner    = 1u << 1, // Hierarchy, names, visibility, layers
    kSelection   = 1u << 2, // Active/selected IDs, component counts
    kInspector   = 1u << 3, // Active object channels, material, light/cam
    kViewport    = 1u << 4, // Sub-panel modes, camera readouts, telemetry
    kTimeline    = 1u << 5, // Current frame, play state, keyframe ticks
    kWorkspace   = 1u << 6, // Area split tree, active workspace
    kAssets      = 1u << 7, // Material/Mesh/Texture catalog
    kConsole     = 1u << 8, // New status/log entries
};
```

* During a `60 Hz` Gizmo drag or Timeline scrub, **only** `kInspector | kViewport | kTimeline` are serialized (`< 1.5 KB` JSON instead of `250 KB`!), keeping Outliner, Asset Browser, and Material Catalog completely untouched at zero CPU/GC cost.
* Large pixel buffers (Material Preview `96×96` RGBA, Render View output, UV Checker) continue to transfer via **zero-copy WebAssembly `HEAPU8` pointer views** (`_fe3d_material_preview_ptr`, `_fe3d_render_buffer_ptr`).
* **Stage 1 → Stage 2 Transition Rule (Zero Polling Loops)**: In Stage 1 (Home / Project Hub + App Shell), UI views update strictly on explicit user actions, file-load callbacks, and C++ `stateRevision()` change notifications—never via `setInterval` full-document JSON polling loops. Stage 2 completes the C++ `DirtyDomainMask` split for all 13 Studio editors.

### 3.3 UI Editor & Panel Lifecycle / Ownership Contract

Every top-level environment (`HomeHubEnvironment`, `MainStudioEnvironment`) and every docked/floating Studio Editor (`IEditorView`: `OutlinerEditor`, `InspectorEditor`, `ViewportEditor`, `TimelineEditor`, etc.) obeys a deterministic 6-method lifecycle managed by `ApplicationShell` and `AreaTreeController`:

```text
create(descriptor) ──► mount(hostElement, areaId) ──► onDomainUpdate(dirtyMask, slice)
                                │                                │
                                ├─► onResize(width, height, dpr) ◄┘
                                ├─► onFocusChange(isFocused) ──► getContextActions() [for Space menu]
                                ▼
                            dispose()  (unsubscribes all listeners & ResizeObservers cleanly; zero leaks)
```

1. **Strict Ownership Separation**:
   * **C++ Engine Owns**: Scene graph, `EditableMesh` geometry, materials, lights, cameras, selection sets (`active_object` + `selected_objects`), `CommandStack` undo/redo, and `.fe3d` serialization.
   * **App Shell Services Own**: `AppPreferences` (`localStorage` / user config dir, strictly separate from scene `.fe3d` files), `RecentProjectsService`, and active environment routing (`Home` ↔ `Studio`).
   * **UI Editor Instances Own Only Ephemeral View State**: Scroll position, filter input text, collapsed inspector card toggles, and hover highlights. Destroying and remounting any editor panel (`Outliner` in the **Default Left Dock**, `Inspector` in the **Right Dock**, or any split area) loses zero scene or selection data.
2. **Conceptual Editor Independence (`A2 — Binding`)**: `OutlinerEditor` (`finaleffect.editor.outliner`, factory-default **LEFT dock**) and `InspectorEditor` (`finaleffect.editor.inspector`, **RIGHT dock**) are registered as separate `EditorId` descriptors with independent `DirtyDomain` subscriptions (`kOutliner` vs. `kInspector`) and must never be coupled into one component.
3. **Centralized Shortcut & Three-Menu Routing (`A3 — Binding`)**: `ShortcutRegistry` sits above individual editors, enforcing the text-input focus guard first, then routing `Space` to the focused editor's `getContextActions()` (**Context Action Menu**), `Shift+Space` to `TransportController::togglePlayPause()`, `Ctrl+Space` to `AreaTreeController::toggleMaximizeHoveredArea()`, `Ctrl+K` / `F3` to `CommandPalette`, and stationary viewport `RMB` to `RadialActionCompass`.

---

## 4. Native Desktop & Desktop Web UI Parity Strategy

A critical architectural finding from `EXISTING_SYSTEM_AUDIT.md` is that `apps/desktop/main.cpp` currently renders only a bare GLFW 3D viewport without studio panels, whereas `web/` provides the full Studio UI. To satisfy Section 2 (*"Native Desktop and Desktop Web must share the same core engine and project behavior"*), Stage 0 evaluates three architectural options for Native Desktop UI:

| Option | Pros | Cons | Verdict |
| :--- | :--- | :--- | :--- |
| **A. Duplicate Full UI Twice (HTML/CSS in `web/` + Dear ImGui/Qt in `apps/desktop/`)** | Zero web runtime in native binary | Maintaining **two** complete 13-editor UI implementations (`Outliner`, `Node Graph`, `Graph Editor`, `Design System`) guarantees drift and doubles engineering cost. | **Rejected** for full duplication |
| **B. Unified C++ Engine + Shared Modular Web UI Bundle (Served via Desktop Web Browser *and* Native OS WebView2 / WebKitGTK / WKWebView host or C++ GPU Canvas)** | 100% identical pixel-accurate Obsidian Slate Design System, Node Graphs, and 13 Editors across Desktop Web & Native Desktop; zero duplicate UI code. | Native WebView host requires optional OS WebView SDK when building full GUI desktop package. | **Recommended Primary Path**, with **Headless/GLFW Direct Viewport** preserved! |
| **C. Pure Custom C++ OpenGL Widget Toolkit from Scratch** | Single C++ binary | Building accessible text layout, IME, flexbox docking, CSS styling, and SVG icons from scratch in raw OpenGL 3.3 takes months before writing a single 3D tool. | **Rejected** (violates Section 11: *"Do not build a new framework unnecessarily"*) |

### 4.1 Concrete Dual-Target Execution Architecture
1. **100% Shared C++20 Engine Core**: `StudioSession`, `EditorController`, `WorkspaceManager` (`AreaNode` layout math), `ShortcutRegistry`, `Scene`, `EditableMesh`, `Renderer`, and `ProjectSerializer` compile natively into `finaleffect_app` (`Linux/Windows/macOS` + `Emscripten WASM`).
2. **Desktop Web (`finaleffect_web`)**: Runs `web/src/` directly over in-process WebAssembly + WebGL 2.0.
3. **Native Desktop (`finaleffect_desktop`)**:
   * **Always builds with zero external UI dependencies** when only C++20 + OpenGL 3.3 + GLFW are present: supports full interactive multi-viewport OpenGL 3.3 rendering, keyboard/mouse `ShortcutRegistry` & `FE3D Hybrid` navigation, CLI `.fe3d` project load/save, and `--headless` deterministic CI rendering.
   * **Desktop Studio Shell Host Mode**: Connects the exact same `web/src/` Original Hybrid UI bundle to the native C++ `EditorController` JSON/binary bridge (via lightweight local loopback/IPC or optional native WebView adapter) so Native Desktop users get the **exact same Home Hub and 13-Editor Studio** backed by native `LocalFileSystem` disk I/O and hardware rendering, with zero UI code duplication.

---

## 5. Extension Protocol: Adding a Future Subsystem Without Modifying Unrelated Modules

To add a new capability (e.g., **Sculpting** in Stage 5):
1. **Domain Layer**: Add `src/scene/sculpt_system.cpp` + `SculptStrokeCommand : public scene::ICommand` (touches zero Lighting, Material, or Animation files).
2. **Controller Layer**: Add `src/app/controllers/sculpt_controller.cpp` and register its typed actions in `CommandBus`.
3. **Tool & Editor Registry**: Register `EditorId::Sculpting` and Sculpt brush `ITool` entries in `ToolRegistry`.
4. **UI Layer**: Add `web/src/editors/sculpt/` module implementing `IEditorView`—automatically surfaced in the Area Editor Selector, Tool Rail, Inspector, and `Ctrl+K` Command Palette!
