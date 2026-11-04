# Final Effect 3D — New Application Information Architecture

**Document ID:** `FE3D-SPEC-03`  
**Stage:** Stage 0 — Product Reset, Original Hybrid UI Design & Architecture Specification  
**Scope:** Application-level navigation, environment hierarchy, workspace/editor taxonomy, selection/mode model, and global command architecture

---

## 1. High-Level Application Topology

Final Effect 3D replaces the fragmented "6 separate Creative Studios" concept (`3D Studio`, `Landscaping Studio`, `Character Studio`, etc.) with **one unified professional desktop application** organized into **Two Primary Environments** and **Three Global Overlay Services**:

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│                         FINAL EFFECT 3D APPLICATION                          │
│                  (Single Process / Single Authoritative C++ Core)            │
├──────────────────────────────────────┬───────────────────────────────────────┤
│   ENVIRONMENT 1: HOME / PROJECT HUB  │     ENVIRONMENT 2: MAIN 3D STUDIO     │
│  ──────────────────────────────────  │  ───────────────────────────────────  │
│  • Launch & Boot Diagnostics         │  • Top Application Menu & Project Bar │
│  • Recent Projects + Instant Search  │  • Workspace Switcher (Task Presets)  │
│  • New Project & Category Templates  │  • Context Action & Snapping Strip    │
│  • Open .fe3d / .fe3dbin / Import    │  • Dynamic Studio Area Split Tree     │
│  • Autosave & Crash Recovery Vault   │    ├─ 3D Viewport (Single / 2 / Quad) │
│  • Local & Shared Asset Libraries    │    ├─ Scene Outliner & Layers         │
│  • Learning, Docs & Release Notes    │    ├─ Quick Channels + Deep Inspector │
│                                      │    ├─ Specialized Editors (UV, Nodes, │
│                                      │    │  Timeline, Graph, Compositor…)   │
│                                      │    └─ Console & FE3D Command Bar      │
├──────────────────────────────────────┴───────────────────────────────────────┤
│                       GLOBAL APPLICATION OVERLAY SERVICES                    │
│  [1] Command Palette (Ctrl+K/F3) [2] Context Menu (Space)  [3] Prefs & Modals│
│      Search all registered cmds      Focused editor actions    Ctrl+, / Save │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 1.1 Environment Lifecycle & Non-Destructive Switching
1. **Boot Phase**: Minimal hardware/runtime initialization indicator (`< 400 ms` on Native Desktop; real 4-stage WASM/WebGL2 milestone bar on Desktop Web) transitions directly into **Environment 1: Home / Project Hub** (unless launched via CLI `--project <path.fe3d>`, which opens **Environment 2: Main 3D Studio** directly).
2. **Project Activation**: Creating a project from a Template, opening a `.fe3d` file, or clicking a Recent Project loads `project::ProjectDocument` in C++ `StudioSession` and transitions into **Main 3D Studio**.
3. **Non-Destructive Return to Hub**: Clicking the top-left **FE3D Hub** button inside the Studio opens the Home / Project Hub as a non-destructive view while keeping the active C++ `StudioSession` alive in memory (`Resume Active Project: <Project Name>` badge prominently pinned in the Hub header). Starting or opening a *different* project when `isDocumentDirty() == true` triggers a mandatory **Unsaved Changes Guard** (`Save`, `Don't Save`, `Cancel`).

---

## 2. Information Hierarchy Inside the Main 3D Studio

To combine **Blender's workspace clarity** with **Maya's technical precision** without clutter, Final Effect 3D establishes a strict **4-level conceptual hierarchy**:

```text
Level 1: PROJECT DOCUMENT (.fe3d)
  └── Owns Scene Graph, Meshes, Materials, Node Graphs, Animation Tracks, Render Settings
       │
Level 2: WORKSPACE (Task-Oriented Multi-Area Arrangement)
  └── Layout | Modeling | Sculpting | UV | Shading | GeoNodes | Animation | Rigging | Rendering | Compositing | Physics | Video
       │  (Switching Workspaces NEVER mutates scene data; it arranges Studio Areas & default mode)
       ▼
Level 3: STUDIO AREA (Composable Rectangular Panel Slot in Recursive Split Tree)
  └── Each Area hosts any registered Editor Type via its top-left Editor Selector:
       • 3D Viewport          • Scene Outliner & Display Layers   • Inspector & Quick Channels
       • UV Editor            • Shader Node Graph                 • Geometry Node Graph
       • Timeline & Transport • Dope Sheet / F-Curve Editor       • Asset Browser
       • Render View          • Compositor Graph                  • Video Sequencer
       • Command & Diagnostics Console
       │
Level 4: INTERACTION CONTEXT & ACTIVE TOOL (Inside the Focused Editor)
  └── Object Mode | Component Modes (Vertex / Edge / Face) | Sculpt Mode | Pose Mode | Edit UV
       └── Active Tool (Select, Move, Rotate, Scale, Universal, Extrude, Bevel, Loop Cut, Brush…)
```

### 2.1 Critical Architectural Rule: Workspaces vs. Editors & Distinct Outliner / Inspector Ownership
* **An Editor** is a self-contained functional view over C++ engine state (e.g., `3D Viewport`, `Scene Outliner`, `Inspector`, `UV Editor`, `Shader Node Graph`).
* **Strict Conceptual Separation (`A2 — Binding`)**:
  * **`Scene Outliner`** is a dedicated **Scene Hierarchy & Display Layer Editor** responsible for scene tree structure, parenting, grouping, visibility/selectability flags, and layer membership.
  * **`Inspector`** is a separate **Property-Editing Editor** responsible for Quick Channels (`tx/ty/tz`, `rx/ry/rz`, `sx/sy/sz`, `vis`) and deep object/component/material/light/camera attributes.
  * **Never merge Outliner and Inspector into one conceptual editor.**
* **Factory-Default Left Outliner Dock (`A2 — Binding`)**: In the redesigned Main Studio, the **Scene Outliner resides in the LEFT dock by default** (independently resizable, collapsible, rearrangeable, and redockable), while the **Inspector resides in the RIGHT dock**. Alternative right-side Outliner arrangements may be chosen or saved by users as custom workspace layouts, but **LEFT is the binding factory default** *(superseding earlier draft proposals that stacked Outliner at top-right above Inspector)*.
* **A Workspace** is a **named, serializable arrangement of Studio Areas** (split tree + ratios + active editor per area + optional interaction mode hint).
* Users are **never trapped**: while in the `Modeling` workspace, a user can split any area, rearrange docks, or switch an area to `UV Editor` or `Shader Node Graph` at any time, save that layout as a custom workspace tab (`+`), or reset the workspace to factory defaults (`Workspace -> Reset Current Workspace`).

---

## 3. Global Navigation & Menu Taxonomy

Unlike Maya's shifting Menu Sets (where top-level menus disappear depending on a dropdown), Final Effect 3D keeps a **stable, predictable 8-menu top Application Menu Bar** at all times, while placing mode-specific operations in the **Context Action Strip**, **Left Tool Rail**, **Context Action Menu (`Space`)**, **8-Direction Radial Action Compass (`RMB` in viewport)**, and **Global Command Palette (`Ctrl+K` / `F3`)**:

| Top-Level Menu | Scope & Responsibilities |
| :--- | :--- |
| **1. File** | `New Project…`, `Open Project…`, `Open Recent ▸`, `Save`, `Save As…`, `Save Binary Bundle (.fe3d + .fe3dbin)…`, `Revert`, `Recover Autosave…`, `Import ▸` (`.fe3d`, `.obj`/`.gltf` in planned stages, `.hdr`/`.pfm` HDRI), `Export ▸`, `Project Metadata…`, `Return to Project Hub`. |
| **2. Edit** | `Undo (Ctrl+Z)`, `Redo (Ctrl+Y / Ctrl+Shift+Z)`, `Repeat Last Action (Shift+R / G)`, `Adjust Last Action…`, `Context Action Menu (Space)`, `Command Palette… (Ctrl+K / F3)`, `Duplicate (Ctrl+D)`, `Duplicate Linked (Alt+D)`, `Delete (Del)`, `Rename Active (F2)`, `Batch Rename…`, `Preferences… (Ctrl+,)`. |
| **3. Create** | Categorized scene object creation: `Mesh ▸` (`Cube`, `Sphere`, `Cylinder`, `Cone`, `Torus`, `Grid Plane`, `Pyramid`), `Light ▸` (`Directional / Sun`, `Point`, `Spot`, `Area`), `Camera`, `Empty / Locator`, `Group Selected (Ctrl+G)`, `Collection / Display Layer`. |
| **4. Select** | `Select All (Ctrl+A)`, `Deselect All (Alt+A)`, `Invert Selection (Ctrl+I)`, `Box Select (B)`, `Select Hierarchy`, `Select Similar`, `Select Grouped / By Layer`, `Select Boundary Loop` (in Mesh Edit), `Selection Masks ▸` (`Meshes`, `Lights`, `Cameras`, `Empties`). |
| **5. Object** *(dynamically titled **Mesh** / **Sculpt** / **Armature** when in those modes)* | In `Object` mode: `Transform ▸` (`Move`, `Rotate`, `Scale`, `Reset Transforms`, `Freeze / Apply Transforms`), `Origin / Pivot ▸` (`Center Pivot`, `Pivot to World Origin`, `Edit Pivot`), `Parent (Ctrl+P)`, `Clear Parent (Alt+P)`, `Shade Smooth / Shade Flat`, `Display Layer ▸`, `Lock / Visibility ▸`. In `Mesh Edit` mode: `Extrude`, `Bevel`, `Bridge`, `Insert Edge Loop`, `Multi-Cut`, `Merge / Weld`, `Fill Hole`, `Combine`, `Separate`, `Normals ▸`. |
| **6. Render** | `Render Current Frame (F12)`, `Open Render View (F11)`, `Environment & HDRI Lighting…`, `Shadow Atlas Diagnostics`, `Viewport Render Capture (PNG)…`. |
| **7. Workspace** | Switch active workspace (`Layout`, `Modeling`, …), `Split Area Horizontally`, `Split Area Vertically`, `Float Active Area as Window`, `Maximize Active Area (Ctrl+Space)`, `Save Custom Workspace…`, `Reset Current Workspace to Default`, `Toggle Fullscreen`. |
| **8. Help** | `Quick Start & Controls Guide`, `Keyboard Shortcut Reference`, `Command Bar Grammar Reference`, `System & GPU Diagnostics…`, `Report Issue`, `About Final Effect 3D (GPL v3.0)`. |

---

## 4. Unified Selection & Active-Object Semantics

Final Effect 3D adopts an explicit, unambiguous **Lead Active + Multi-Selection** model in C++ (`StudioSession` / `SelectionSystem`) that powers both Maya-style parenting/boolean ordering and Blender-style active inspection:

1. **Selected Set (`selected_objects: vector<ObjectId>`)**: Zero, one, or many objects currently in the selection set. Non-lead multi-selected objects render with **Coral Emerald (`#38C695`)** secondary selection accents (while **Prism Cyan `#27B6D9`** is reserved strictly for active tools, UI focus, and workspace indicators per Decision `A1`).
2. **Active / Lead Object (`active_object: ObjectId`)**: The **most recently clicked** object in `selected_objects`. Rendered with **Solar Amber (`#F29D38`)** highlight in both the 3D Viewport and the Outliner lead-selection indicator.
   * **Inspector & Quick Channels** display the `active_object` (with a badge `HeroCube (+3 selected)` and optional `Alt+Enter` / batch-apply toggle to push a channel edit to all selected objects).
   * **Order-Dependent Operations** (`Parent`, `Bridge`, `Light Link`, `Copy Material to Selected`) treat secondary selected objects as **sources/children** and the **Active/Lead Object** as the **target/parent**—consistent across both Outliner `Ctrl+Click` and 3D Viewport `Shift+Click`.
3. **Component Selection (`MeshElementSelection`)**: When `EditMode` is `Vertex`, `Edge`, or `Face` (or Multi-Component `Shift`-combined mask), selection operates on topology indices of the active mesh object(s), preserving cached selection sets per sub-mode during a modeling session so toggling `Vertex` ↔ `Face` does not unnecessarily destroy selection context.

---

## 5. Binding Keyboard & Menu Disambiguation Architecture (`A3 — Binding`)

Final Effect 3D maintains **three strictly separate fast-access menu systems** plus conflict-free playback and area-maximization bindings:

| Input Trigger | System / Action | Scope & Non-Interference Guarantee |
| :--- | :--- | :--- |
| **`Space`** | **Context Action Menu** | Opens a keyboard-navigable menu tailored to the **focused editor & current interaction mode**. Never executes an action merely on `Space` press; closes with `Escape`; suppressed when any text/numeric/command input owns focus (`Space` inserts `' '`). |
| **`Shift + Space`** | **Play / Pause** | Toggles active animation timeline or simulation playback globally across Studio editors, except when a text/numeric/command input owns focus. |
| **`Ctrl + Space`** | **Maximize / Restore Area** | Expands or restores the Studio Area (or Quad Viewport sub-panel) currently under the mouse pointer. |
| **`Ctrl + K` / `F3`** | **Global Command Palette** | Searches all registered application commands, tools, menus, and workspaces across the entire suite. |
| **`RMB` (Viewport)** | **8-Direction Radial Action Compass** | Directional viewport compass for rapid mode/context switching (`< 4px` stationary right-click). Distinct from both `Space` and `Ctrl+K`. |

---

## 6. Progressive Disclosure of Future Workspaces

Because Stage 0 forbids fake controls while requiring an architecture ready for 13 editors:

* **Default Pinned Workspace Tabs in Early Stages (`Stages 2–4`)**: Only workspaces backed by **live, operational editors** (`Layout`, `Modeling`, `Shading`, `Animation`, `Rendering`) appear in the primary top workspace strip by default.
* **`+ Add Workspace` Dropdown**: Categorizes operational preset layouts (`General`, `3D Authoring`, `LookDev & Lighting`, `Animation`) cleanly. As `Sculpting` (`Stage 5`), `Geometry Nodes` (`Stage 5–6`), `UV Editing` (`Stage 6`), `Rigging` (`Stage 7`), `Physics` (`Stage 9`), and `Compositing / Video Editing` (`Stage 10`) ship in C++, their workspace presets register into the same data-driven `WorkspaceRegistry` without altering top-bar layout rules.
