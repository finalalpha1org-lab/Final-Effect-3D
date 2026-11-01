# Final Effect 3D — Main 3D Studio UI Specification

**Document ID:** `FE3D-SPEC-05`  
**Stage:** Stage 0 — Product Reset, Original Hybrid UI Design & Architecture Specification  
**Environment:** Environment 2 — Main 3D Studio (`#fe3d-studio`)  
**Target Resolutions:** `1920 × 1080`, `2560 × 1440`, `1366 × 768` (Desktop & Desktop Web Only)

---

## 1. Original Hybrid Studio Architecture Overview

The **Main 3D Studio** unifies the best ergonomics of modern 3D software into an original, high-density desktop layout:

* **Two-Tier Top Chrome (`56px` total, down from Stage 1's `~118px`)** maximizing vertical 3D Viewport space.
* **Recursive Studio Area Split Tree (`#studio-area-root`)** allowing any workspace region to split, resize, tab, float, or switch Editor Type.
* **Unified Dual-Tier Inspector** combining Maya-grade **Quick Channels** (compact numeric matrix + MMB scrub + locks + keys) directly above categorized **Deep Attribute Cards** + **Display Layers**.
* **Fast Spatial Workflows**: Left **Tool Rail**, **Customizable Quick Shelf**, **8-Direction Radial Compass (`RMB` / `Tab`)**, **In-Viewport Operator HUD**, **`Ctrl+K` Command Palette**, and **Bottom Status + Safe `FE3D` Command Bar**.

---

## 2. Default `Layout` / `Modeling` Studio Wireframe (`1920 × 1080`)

```text
┌────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ [◈ Hub] File Edit Create Select Object Render Workspace Help │ [Layout][Modeling][Shading][Anim][Render][+]│ 28px Row 1
│                                                              │              Robot_Arm.fe3d*  [🔍 Ctrl+K][⚙]│
├──────────────────────────────────────────────────────────────┴─────────────────────────────────────────────┤
│ [Mode: Object ▾] │ [Space: World ▾] [Pivot: Median ▾] │ [🧲 Grid][Curve][Vert] [Step:0.5] │ [Shelf: ▣ ○ △ +]│ 28px Row 2
├────┬───────────────────────────────────────────────────────────────┬──────────────────┬────────────────────┤
│    │ [▣ 3D Viewport ▾]  Persp · Lit PBR · 1x │ [1-View][2-View][4] │ [☷ Outliner ▾]   │ [≡ Inspector ▾]    │ 26px Area
│[↖] │ ┌───────────────────────────────────────────────────────────┐ │ [🔍 Filter…][+Lyr]│ HeroCube  [Mesh]  │ Headers
│Sel │ │ FPS: 60 ·Objs: 5 ·Tris: 1.4k                  [  +Y   ]  │ │ ▾ ◈ Scene        │ ────────────────── │
│[✥] │ │ Selected: HeroCube                            [-X  +X ]  │ │   ▣ HeroCube  👁🔒│ QUICK CHANNELS     │
│Move│ │                                               [ +Z Persp]│ │   ▣ Pyramid   👁· │ Pos X [  0.000] ·◇ │
│[↻] │ │                                                           │ │   🎥 MainCam  👁· │ Pos Y [  1.250] L◆ │
│Rot │ │                    [ 3D SCENE STAGE ]                     │ │   ☀ SunLight  👁· │ Pos Z [  0.000] ·◇ │
│[⤢] │ │                  (Right-Handed +Y Up)                     │ │ ──────────────── │ Rot X [  0.000] ·◇ │
│Scl │ │                                                           │ │ DISPLAY LAYERS   │ Rot Y [ 35.000] ·◆ │
│[⊕] │ │                                                           │ │ [V][P][N] Default│ Rot Z [  0.000] ·◇ │
│Univ│ │ ┌──────────────────────────┐                              │ │ [V][P][R] RefGeo │ Scl X [  1.000] ·◇ │
│────│ │ │ ▾ Bevel Edges (Last Op)  │                              │ └──────────────────┤ Scl Y [  1.000] ·◇ │
│[⌖] │ │ │ Fraction  [====|==] 0.18 │                              │                    │ Scl Z [  1.000] ·◇ │
│Pvt │ │ │ Segments  [===|===] 3    │                              │ (Note: Outliner &  │ Vis   [✓]       ·◇ │
│────│ │ └──────────────────────────┘                              │  Inspector can sit │ ────────────────── │
│[▣] │ └───────────────────────────────────────────────────────────┘ │  stacked in Right │ [Obj][Geo][Mat][Lt]│
│Box │═══════════════════════════════════════════════════════════════│  Dock OR split    │ ▾ Transform & Pivot│
│    │ [⏱ Timeline ▾]  [⏮][◀][▶ Play][⏭]  Frame:[ 24 ]  1..120 (24f)│  Left / Right)    │   [Freeze] [Reset] │
│    │ 1    10    20   [24]   30    40    50    60    80   100   120 │                  │ ▾ PBR Material     │
│    │ ──◆──────────────┃──────────────────────◆──────────────────── │                  │   [● Preview]      │
├────┴──────────────────┴────────────────────────────────────────────┴──────────────────┴────────────────────┤
│ [FE3D ▸] [Enter command (e.g. bevel 0.2 2, select HeroCube)… ] │ LMB: Select · MMB: Orbit · RMB: Compass   │ 24px Footer
└────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Exact Region Breakdown & Behavioral Contract

### 3.1 Row 1 — Top Application Bar (`28px` fixed height)
* **Left Zone**:
  * **Hub Button (`[◈ Hub]`)**: Non-destructively switches to the Home / Project Hub while preserving the live Studio state.
  * **8 Persistent Application Menus**: `File`, `Edit`, `Create`, `Select`, `Object` *(context-titled `Mesh` / `Sculpt` / `Armature`)*, `Render`, `Workspace`, `Help`. Supports hover-switch once opened, keyboard arrow navigation, shortcut right-alignment, and disabled-reason tooltips.
* **Center Zone — Workspace Switcher Strip**:
  * Horizontal pill tabs (`Layout`, `Modeling`, `Shading`, `Animation`, `Rendering`, plus stage-unlocked tabs and custom user workspaces).
  * Left/Right scroll chevrons on `1366 × 768` if custom tabs overflow; **`[+]`** button to duplicate current layout or load a preset; double-click custom tab to rename; middle-click or context menu to close/reorder custom tabs.
* **Right Zone**:
  * **Active Project Pill**: Shows `<ProjectName.fe3d>` + unsaved asterisk `*` (`isDocumentDirty()`) + quick `Save` icon.
  * **Command Palette Trigger (`[🔍 Ctrl+K]`)**: Opens fuzzy search across all menus, tools, objects, and commands.
  * **Preferences (`[⚙]`)**: Opens global `AppPreferences` modal.

### 3.2 Row 2 — Context Action, Snapping & Quick Shelf Bar (`28px` fixed height)
Adapts cleanly to the active editor & interaction mode without shifting layout height:
* **Interaction Mode Dropdown (`Object Mode`, `Vertex`, `Edge`, `Face`, `Sculpt`, `Pose`)** + quick component mask icons (`▣ Obj`, `• Vert`, `╱ Edge`, `▱ Face`, with `Shift+Click` multi-component toggle).
* **Transform Orientation & Pivot Controls**:
  * `Orientation`: `World`, `Local`, `Normal` *(Stage 4)*, `View`
  * `Pivot Point`: `Active / Object Pivot`, `Median Point`, `Bounding Box Center`, `World Origin`
* **Snapping Cluster**:
  * Magnet toggle (`Snap On/Off`) + target pills (`Grid [X]`, `Edge/Curve [C]`, `Vertex [V]`, `Face/Surface`) + numeric `Step` field (`0.5m` / `15°`).
* **Customizable Quick Shelf Strip (Right side of Row 2, expandable to a dedicated `32px` Shelf Row via toggle)**:
  * Houses user-pinned tools & primitives (`+Cube`, `+Sphere`, `+Cylinder`, `+Torus`, `+Light`, `+Camera`, `Extrude`, `Bevel`, `Freeze`, `Center Pivot`) with shelf category selector (`Polygons`, `Modeling`, `Lighting`, `Custom`) and right-click `Pin to Shelf` from any menu item!

### 3.3 Left Region — Context-Aware Tool Rail (`44px` icon mode / `124px` labeled mode / `0px` collapsed via `T`)
* Displays **modal tools** (tools that stay active and define what `LMB` drag in the viewport does), strictly separated from one-shot action commands:
  * **Universal Tools**: `Select / Box Marquee (Q)`, `Move (W)`, `Rotate (E)`, `Scale (R)`, `Universal Transform (T)`, `Edit Pivot (D)`.
  * **Mode-Contextual Tools**:
    * *Mesh Edit Mode*: `Extrude Region`, `Bevel`, `Insert Edge Loop`, `Multi-Cut`, `Target Weld`.
    * *Sculpt Mode (Stage 5)*: `Draw`, `Clay Strips`, `Smooth`, `Grab`, `Inflate`, `Crease`, `Mask`.
    * *UV Mode (Stage 6)*: `UV Select`, `UV Move/Rotate/Scale`, `Mark Seam`, `Unwrap`.
* Pressing **`Y`** re-activates the previously active tool (`StudioSession::activateLastTool()`).

### 3.4 Dynamic Studio Area Split Tree (`#studio-area-root`)
Every interior workspace panel is a **Studio Area** managed by a binary/n-ary split tree (`AreaNode` in C++/JSON):

* **Area Header (`26px` height)** on every Studio Area:
  * **Left**: `[Editor Type Dropdown ▾]` (switches the area in-place between `3D Viewport`, `Scene Outliner`, `Inspector`, `Timeline`, `Dope Sheet`, `Graph Editor`, `UV Editor`, `Shader Graph`, `Geometry Nodes`, `Asset Browser`, `Render View`, `Compositor`, `Video Sequencer`, `Console`) + editor-local toolbar controls.
  * **Right**: Area menu `[⋮]` (`Split Right`, `Split Down`, `Pop Out as Floating Window`, `Maximize Area [Ctrl+Space]`, `Close / Collapse Area`).
* **Interactive Splitters (`4px` visual hairline, `8px` hit target)**:
  * Dragging any vertical or horizontal splitter updates proportional area ratios in real time (`ResizeObserver` resizes WebGL2/OpenGL viewports without stretching or aspect distortion) and clamps each area to a sensible minimum (`min-width: 200px`, `min-height: 96px`).
* **Tabbed Area Stacks**: Multiple editors can share one docked Area as tabs (e.g., `Inspector` | `Scene & Environment` in the right dock, or `Timeline` | `Console` | `Asset Browser` in the bottom dock).
* **Floating Windows**: Tearing off a tab or selecting `Pop Out as Floating Window` mounts that Area inside an elevated, resizable, draggable floating container clamped within the desktop window, with a 1-click `[Dock Back]` button.
* **Maximize Focused Area (`Ctrl+Space` or `` ` ``)**: Temporarily expands whichever Studio Area is under the mouse cursor to `100%` of the workspace area; pressing `Ctrl+Space` again restores the exact split tree.

### 3.5 Scene Outliner & Display Layer Panel
Can be docked at top-right (above Inspector, Blender-style) or full-left (Maya-style—one click via preset or Area move):
* **Search & Filter Bar**: Instant substring search + type filter (`All`, `Meshes`, `Lights`, `Cameras`, `Groups`).
* **Hierarchical Tree**: Expand/collapse carets, drag-and-drop reparenting onto target nodes (or between siblings to reorder), `Shift+Click` range select, `Ctrl+Click` toggle select, double-click `F2` inline rename.
* **Right-Aligned Restriction Toggles per Row**:
  * `👁` Visibility (`visible`)
  * `🔒` Selectability / Lock (`locked`)
* **Collapsible Display Layer Sub-Panel**:
  * Create layer from selection (`+ Layer`), assign/remove objects, color swatch, `V` (Visible), `P` (Playback Visible), and mode cycling `[N]` Normal -> `[T]` Template (wireframe, unpickable) -> `[R]` Reference (shaded, unpickable).

### 3.6 Unified Dual-Tier Inspector (`Quick Channels + Deep Attributes`)
Solves the Maya Channel Box vs. Attribute Editor split cleanly inside one `320px` default right area:
1. **Object Identity Header**: Active object icon, editable name field, multi-selection count pill (`+2`), node Lock button, and `Pin Inspector` toggle.
2. **Collapsible Tier 1 — Quick Channels Matrix (Pinned by default)**:
   * Ultra-dense `20px` rows for `Translate X/Y/Z`, `Rotate X/Y/Z`, `Scale X/Y/Z`, `Visibility`, plus user `Custom Attributes`.
   * **Middle-Mouse Virtual Slider Scrubbing**: Dragging `MMB` horizontally anywhere on a channel label (or inside the viewport after highlighting a channel) scrubs values smoothly (`Ctrl` = `0.01x` fine precision, `Shift` = `10x` fast step) and coalesces into a single Undo transaction.
   * **Inline Math Expressions**: Typing `1.5 * 2` or `+= 45` in any numeric input evaluates deterministically (via safe arithmetic parser, zero `eval`) before dispatching to C++.
   * **Per-Channel Lock (`🔒`) & Keyframe Diamond (`◇` unkeyed / `◆` keyed at current frame / tinted row when animated)**.
3. **Tier 2 — Categorized Deep Inspector Tabs**:
   * Horizontal icon tabs filtered contextually to the active object's components:
     * `Object & Transform` (Pivot offset, Freeze/Reset/Center Pivot, Parent, Display Layer, Custom Attributes `+ Add Attr`)
     * `Geometry / Modifiers` (Vertex/Edge/Face/Triangle counts, Smooth/Flat normals, Modifier Stack in Stage 4+)
     * `Material & Surface` (Live `96×96` 3D Cook-Torrance preview sphere, Material selector/assign/new, Albedo, Metallic, Roughness, AO, Emissive, Opacity)
     * `Data (Light / Camera)` (Light type switcher, Color, Intensity, Range, Cone Angles, Area size, PCF Shadow toggle/bias/resolution, Light Linking checklist; OR Camera Persp/Ortho, FOV/Ortho Height, Near/Far clip, `Set Active Camera`)
     * `World & Render` (Sky/Ambient colors, `.hdr`/`.pfm` HDRI loader + built-in presets, IBL intensity, skybox rotation, tone mapping exposure, Shadow Atlas diagnostics)

### 3.7 Bottom Status, Mouse Hints & Safe `FE3D` Command Bar (`24px` fixed height)
* **Left — Collapsible `FE3D` Command Bar**:
  * Compact `[FE3D ▸]` input (`260px–380px`, focused via `Ctrl+;`): executes deterministic C++ `EditorController::executeCommandLine` commands with `Up/Down` history and status feedback.
* **Center — Latest Status / Undo Toast**:
  * Green/Amber/Red status dot + concise confirmation (`Extruded 2 faces (+0.35m)`, `Undid: Set Transform`, or structured error message).
* **Right — Dynamic Contextual Mouse & Modifier Prompt + Scene Telemetry**:
  * Dynamically updates based on cursor hover & active modal tool (e.g., `LMB: Box Select · Shift+LMB: Add · MMB: Orbit · RMB: Compass` or during Gizmo drag: `Dragging X (+1.40m) · Ctrl: Snap · Esc/RMB: Cancel`).
  * Compact `Verts / Faces / Tris / Memory` telemetry badge.

---

## 4. Fast Contextual Overlay Controls

1. **8-Direction Radial Action Compass (`Stationary RMB` or `Tab`)**:
   * Opens centered at cursor in `< 16 ms`:
     * **N**: `Vertex Mode` · **E**: `Face Mode` · **S**: `Edge Mode` · **W**: `Object Mode`
     * **NW**: `Frame Selection (F)` · **NE**: `Extrude / Context Op 1` · **SE**: `Bevel / Context Op 2` · **SW**: `Isolate Select`
   * Supports both click-release on wedge **and** flick-gesture (`RMB press -> drag > 28px toward wedge -> release`).
2. **In-Viewport Operator Adjust HUD (Bottom-Left of Active Viewport)**:
   * Appears non-blockingly after running a parametric command (`Create Torus`, `Extrude`, `Bevel`, `Bridge`, `Subdivide`). Modifying any parameter amends the top `CommandStack` entry in-place (`undo` + re-execute with updated parameter) until the user starts the next selection or tool action.
3. **Global Command Palette (`Ctrl+K` or `F3`)**:
   * Centered `520px` modal search input listing every registered action, menu item, workspace, and shortcut with fuzzy matching and recent-command boost.
