# Final Effect 3D — Product Vision & Original Hybrid Direction

**Document ID:** `FE3D-SPEC-01`  
**Stage:** Stage 0 — Product Reset, Original Hybrid UI Design & Architecture Specification  
**Baseline Commit:** `36aec73c539595997f4ab4aa8ae48ca2c80fd551` (`main`, PR #6 merged)  
**Target Platforms:** Native Desktop (`C++20` + OpenGL 3.3) & Desktop Web (`WebAssembly` + WebGL 2.0)  
**License:** GNU GPL v3.0-or-later

---

## 1. Executive Summary

**Final Effect 3D** is an open-source, C++20-first, desktop-grade 3D creation suite engineered to bridge the historical divide in 3D authoring software:

* **Blender** pioneered fast discoverability, fluid workspace switching, flexible area splitting, non-blocking operator panels, and unified multi-discipline workflows—yet frequently buries precision channel locking, hierarchical rig/layer inspection, and structured command automation behind deep popovers or Python-only scripting.
* **Autodesk Maya** established industry-standard technical precision—compact transform channels with middle-mouse virtual sliders, per-channel locks, display layers (`Normal` / `Template` / `Reference`), deterministic command echoing, and multi-camera orthographic/perspective production layouts—yet burdens artists with 1990s multi-row toolbar clutter, deep menu-set hunting, and fragmented legacy editors.

Final Effect 3D does **not** clone Blender and does **not** clone Maya. Instead, Stage 0 establishes an **Original Hybrid Product Identity**—combining **speed and workspace fluidity** with **engineering-grade numerical and hierarchical precision** inside a clean, original visual language (**Obsidian Slate + Prism Cyan + Solar Amber**) backed by a single authoritative C++20 engine core.

---

## 2. Core Product Philosophy

### 2.1 Five Non-Negotiable Design Pillars

1. **Authoritative C++20 Engine Core, Thin Declarative UI**  
   All scene objects, polygon topology (`scene::EditableMesh`), transforms, materials, lights, cameras, keyframe tracks, selection sets, undo/redo transactions (`scene::CommandStack`), and `.fe3d` / `.fe3dbin` serialization live exclusively in C++20. No UI layer—whether HTML/CSS/JS over WebAssembly or Native Desktop UI—ever duplicates scene state or fabricates simulated viewport visuals.
2. **Immediate Discoverability + Zero-Friction Technical Depth ("Progressive Precision")**  
   New and intermediate artists can launch a project, pick tools from a clean left Tool Rail, switch task workspaces with one click, and search any command via the **Command Palette (`Ctrl+K`)**. Technical artists simultaneously get an always-accessible **Quick Channels Matrix** (with MMB virtual slider scrubbing, per-channel locks, and keyframe diamonds), **Display Layers**, **8-Direction Radial Compass**, and a deterministic, `eval`-free **FE3D Command Bar**.
3. **Viewport-First Working Density**  
   On a `1920 × 1080` desktop monitor, at least **68% of total screen pixels** in the default `Layout` and `Modeling` workspaces belong to the active 3D Viewport(s), with compact `26px–30px` chrome headers, `22px` numeric rows, zero consumer-style marketing padding, and instant `Ctrl+Space` area maximization.
4. **Radical Architectural Honesty (Zero Fake Controls)**  
   Every visible button, menu item, slider, shelf action, and shortcut must execute real C++ engine logic or persistent workspace/UI state. Capabilities scheduled for later implementation stages are **never** rendered as dead or mocked buttons; architectural extension points are registered cleanly as stages ship.
5. **Extensible Multi-Editor Architecture Without Future Rewrites**  
   The workspace shell, area layout tree, editor registry, tool registry, command bus, and inspector schema are designed up front so future stages—Modeling (`Stage 4`), Sculpting & Procedural Geometry (`Stage 5`), UV & Shader Nodes (`Stage 6`), Animation & Rigging (`Stage 7`), Lighting & Rendering (`Stage 8`), Physics (`Stage 9`), and Compositing/VSE (`Stage 10`)—plug in as modular subsystems without redesigning the shell.

---

## 3. What Final Effect 3D Synthesizes vs. What It Rejects

| Dimension | Adopted & Refined Principle | Explicitly Rejected Anti-Pattern | Final Effect 3D Original Synthesis |
| :--- | :--- | :--- | :--- |
| **Top Chrome** | Task-oriented Workspace Tabs + Global Scene/Snap Controls | 3–4 stacked horizontal icon toolbars & menu-set dropdowns that rewrite the menubar | **Two-Tier Studio Bar (`56px` total)**: `28px` App Menu + Workspace Pills + Project Status, over a `28px` Context Action & Snap Bar (`Mode`, `Transform Space`, `Pivot`, `Snapping`, `Viewport Layout`). |
| **Panel Layout** | Composable rectangular **Studio Areas** with switchable Editor Type headers & splitters | Rigid, hard-coded single-purpose sidebars that cannot switch editor types | **Dynamic Area Split Tree (`AreaNode`)** supporting docked splits, tabbed area stacks, tear-off floating windows, and 1-click Workspace Presets. |
| **Property Editing** | Pinned compact numeric transform matrix + categorized deep inspector | Hunting across separate "Channel Box" vs "Attribute Editor" windows or 14 tiny vertical tab icons | **Unified Dual-Tier Inspector**: Pinned **Quick Channels Matrix** (`tx/ty/tz`, `rx/ry/rz`, `sx/sy/sz`, `vis`, Lock, Key, MMB Scrub) over tabbed **Deep Inspector Cards** + **Display Layers**. |
| **Viewport Navigation** | Effortless simultaneous support for both 3-button mouse and `Alt`+mouse workflows | Forcing users into an ideological war between `MMB` vs `Alt+LMB` on first launch | **FE3D Hybrid Input Profile**: `MMB` Orbit, `Shift+MMB` Pan, `Wheel` Zoom work out of the box **and** `Alt+LMB` Orbit, `Alt+MMB` Pan, `Alt+RMB` Dolly also work seamlessly (`Blender Pure` and `Maya Pure` profiles optional in Preferences). |
| **Fast Context Actions** | Spatial muscle-memory radial menus + searchable command palette | Giant full-screen 5-zone Hotbox text matrices covering the entire 3D model | **8-Direction Radial Compass (`RMB` / `Tab`)** for instant mode/tool switching + **`Ctrl+K` Command Palette** + bottom-left **In-Viewport Operator HUD** after executing a tool. |
| **Command Automation** | Structured, human-readable, deterministic command execution & history | Unsafe `eval()`, opaque internal actions, or requiring scripting for basic batch edits | **FE3D Command Bar** (`Ctrl+;` or footer toggle): typed C++ command grammar echoed cleanly in the Console Log with 100% `CommandStack` undo/redo. |

---

## 4. Strict Platform Scope

Final Effect 3D is exclusively a **Desktop-First Professional Creation Suite**:

1. **Native Desktop Application**: `C++20` + `OpenGL 3.3 Core` (`apps/desktop/`, `finaleffect_rendering_opengl33`, `finaleffect_window_glfw`) on Linux, Windows, and macOS.
2. **Desktop Web Application**: `C++20` compiled via Emscripten to `WebAssembly` + `WebGL 2.0` (`apps/web/`, `finaleffect_rendering_webgl2`, `web/`) running in desktop Chromium, Firefox, Safari, and Edge browsers.

### 4.1 Reference Desktop Resolutions
* **Primary Production Target:** `1920 × 1080` (`1.0x`–`2.0x` DPI)
* **QHD / 4K Workstation Target:** `2560 × 1440` and `3840 × 2160` (HiDPI crisp vector icons & device-pixel framebuffer scaling)
* **Minimum Supported Desktop Window:** `1366 × 768` (compact laptop workstation without horizontal scrollbars or clipped gizmos)

### 4.2 Explicitly Out of Scope
* iOS and Android native or mobile-web targets.
* Mobile portrait (`390 × 844`) and mobile landscape (`844 × 390`) layouts.
* Mobile bottom navigation drawers (`.mobile-portrait-nav`), touch-first gesture routers, and coarse-pointer UI inflation.
* *(Note: Existing mobile code remains untouched during Stage 0 per safety rules and is scheduled for audited, surgical removal in Stage 1/2 as detailed in `DESKTOP_PLATFORM_REQUIREMENTS.md` and `TECHNICAL_RISKS_AND_MIGRATION_PLAN.md`.)*

---

## 5. Long-Term Product Capability Horizon

Final Effect 3D's architecture is designed to grow across **20 integrated creative domains** without structural rewrites:

| Capability Domain | Baseline Status (`v0.5.0` / `36aec73`) | Planned Delivery Stage |
| :--- | :--- | :---: |
| **1. Project Management & Hub** | Operational `.fe3d` v1.3 + `.fe3dbin` v1.0, Templates, IndexedDB Recents | Refined in **Stage 1** |
| **2. Customizable Studio Shell & Workspaces** | Operational `WorkspaceManager` + Stage 1 Maya shell (monolithic DOM) | Rebuilt as Hybrid Area Shell in **Stage 2** |
| **3. Camera Systems & Multi-View Viewport** | Operational Persp/Ortho, Orbit/Pan/Zoom, Camera objects, single canvas | True Quad 4-View & Marquee in **Stage 3** |
| **4. 3D Modeling & Polygon Mesh Editing** | Operational Primitives, `EditableMesh`, Move, Extrude, Delete, Fill Hole | Full Bevel/Bridge/LoopCut/Weld/Combine in **Stage 4** |
| **5. Modifiers (Non-Destructive Stack)** | Not yet implemented | **Stage 4–5** |
| **6. Digital Sculpting** | Not yet implemented | **Stage 5** |
| **7. Procedural Geometry & Geometry Nodes** | Structural workspace slot only | **Stage 5–6** |
| **8. UV Editing & Unwrapping** | Per-corner UVs stored in `EditableMesh`; no interactive 2D UV editor yet | **Stage 6** |
| **9. Materials, Shading & Shader Nodes** | Operational Cook-Torrance PBR + 3D preview sphere; no node graph yet | Node-based Shader Graph in **Stage 6** |
| **10. Asset Management** | Basic in-project mesh/material list; no persistent library browser yet | **Stage 6–8** |
| **11. Animation (Timeline, Dope Sheet, Curves)** | Operational linear TRS keyframes, Time/Range slider, playback | Bezier Graph Editor & Dope Sheet in **Stage 7** |
| **12. Rigging & Armatures** | Object hierarchy, pivots, channel locks, custom attrs; no bones/skinning yet | Joints, IK & Linear Blend Skinning in **Stage 7** |
| **13. Character Creation** | Planned future domain built atop Sculpting + Rigging + Blendshapes | **Stage 7+** |
| **14. Lighting & Environment (IBL)** | Operational Directional/Point/Spot/Area lights, PCF Shadow Atlas, HDRI IBL, Light Linking | Volumetric/IES/Bake polish in **Stage 8** |
| **15. Rendering (Realtime & Offline)** | Operational Realtime PBR + Render Current Frame capture | Progressive Accumulation / AOV Passes in **Stage 8** |
| **16. Physics & Rigid/Cloth Simulation** | Not yet implemented | **Stage 9** |
| **17. Particles & Visual Effects** | Not yet implemented | **Stage 9** |
| **18. Compositing** | Structural workspace slot only | Framebuffer Post-FX & 2D Node Compositor in **Stage 10** |
| **19. Video Editing (Sequencer)** | Structural workspace slot only | Timeline Clip/Image-Sequence VSE in **Stage 10** |
| **20. Diagnostics, Scripting & Extensibility** | Operational Console Log + safe structured `FE3D` Command Line | Typed Command SDK & Batch Runner in **Stage 2–11** |

---

## 6. Stage 0 Documentation Package Map

All 15 documents in `docs/product-redesign/` form one unified, cross-referenced specification:

1. [`PRODUCT_VISION.md`](./PRODUCT_VISION.md) — Product identity, hybrid philosophy, platform scope, and long-term horizon.
2. [`EXISTING_SYSTEM_AUDIT.md`](./EXISTING_SYSTEM_AUDIT.md) — Line-by-line audit of the 58,165-LOC repository at `36aec73`.
3. [`NEW_APPLICATION_INFORMATION_ARCHITECTURE.md`](./NEW_APPLICATION_INFORMATION_ARCHITECTURE.md) — Navigation model, Home Hub vs. Main Studio hierarchy, modes, and state flow.
4. [`HOME_UI_SPECIFICATION.md`](./HOME_UI_SPECIFICATION.md) — Complete desktop Home / Project Hub wireframes, states, and C++ integration.
5. [`MAIN_STUDIO_UI_SPECIFICATION.md`](./MAIN_STUDIO_UI_SPECIFICATION.md) — Complete Original Hybrid 3D Studio layout, Area system, Quick Channels, and toolbars.
6. [`EDITOR_ARCHITECTURE.md`](./EDITOR_ARCHITECTURE.md) — Detailed specification of all 13 specialized editors.
7. [`DESIGN_SYSTEM.md`](./DESIGN_SYSTEM.md) — Obsidian Slate + Prism Cyan visual tokens, typography, iconography, components, and density rules.
8. [`VIEWPORT_SPECIFICATION.md`](./VIEWPORT_SPECIFICATION.md) — Single/Split/Quad 4-View rendering, picking, gizmos, overlays, and shading modes.
9. [`ENGINE_UI_ARCHITECTURE.md`](./ENGINE_UI_ARCHITECTURE.md) — Modular C++20 engine boundaries, command/state bridge, and Desktop/Web parity architecture.
10. [`PROJECT_DATA_ARCHITECTURE.md`](./PROJECT_DATA_ARCHITECTURE.md) — `.fe3d` / `.fe3dbin` persistence, autosave, crash recovery, external assets, and schema migrations.
11. [`DESKTOP_PLATFORM_REQUIREMENTS.md`](./DESKTOP_PLATFORM_REQUIREMENTS.md) — Native Desktop & Desktop Web targets, HiDPI, windowing, and mobile code removal audit.
12. [`FEATURE_ROADMAP.md`](./FEATURE_ROADMAP.md) — Capability matrix across all creative disciplines from current baseline to v1.0.
13. [`IMPLEMENTATION_PHASES.md`](./IMPLEMENTATION_PHASES.md) — Staged engineering execution plan (`Stage 0` through `Stage 11`).
14. [`ACCEPTANCE_CRITERIA.md`](./ACCEPTANCE_CRITERIA.md) — Objective, testable verification gates for every stage and subsystem.
15. [`TECHNICAL_RISKS_AND_MIGRATION_PLAN.md`](./TECHNICAL_RISKS_AND_MIGRATION_PLAN.md) — Risk register, refactoring sequence, backward compatibility guarantees, and rollback safety.
