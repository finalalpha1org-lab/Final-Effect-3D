# Final Effect 3D — Home / Project Hub UI Specification

**Document ID:** `FE3D-SPEC-04`  
**Stage:** Stage 0 — Product Reset, Original Hybrid UI Design & Architecture Specification  
**Environment:** Environment 1 — Desktop Home / Project Hub (`#fe3d-project-hub`)  
**Target Resolutions:** `1920 × 1080` (Primary), `2560 × 1440` (QHD), `1366 × 768` (Compact Desktop Minimum)

---

## 1. Purpose & Design Principles

The **Home / Project Hub** is the professional entry point to Final Effect 3D. It replaces the previous multi-studio placeholder rail (`Landscaping Studio`, `Character Studio`, etc.) with a focused, zero-fluff desktop workstation hub modeled after high-efficiency DCC & engineering hubs (fast keyboard-navigable project launch, real file history, crash recovery, and clean separation between **Templates** and **User Projects**).

### 1.1 Strict Honesty Rules
* **Zero Fake Data**: Recent Projects are populated strictly from real `.fe3d` / `.fe3dbin` files opened or saved by the user (`LocalFileSystem` recent index on Native Desktop; `IndexedDB` + `localStorage` index on Desktop Web). On a clean install, the **Empty State** is displayed honestly.
* **Templates vs. User Files Never Mix**: Built-in C++ engine templates (`Starter Showcase`, `Modeling Blank`, `LookDev Shader Ball`, `Lighting Stage`) live exclusively in the **Templates** strip/section—never injected as fake recent files.
* **Zero Idle GPU Waste**: While the Home Hub is visible and occluding the Studio, continuous 60 FPS 3D viewport rendering remains paused (`fe3d_is_home_view_occluding() == 1`).

---

## 2. Desktop Layout Wireframe (`1920 × 1080` & `1366 × 768`)

```text
┌──────────────────────────────────────────────────────────────────────────────────────────────────┐
│ [◈] FINAL EFFECT 3D   v0.6.0-dev (GPLv3 · OpenGL 3.3 / WebGL 2.0)    [▶ Resume: Robot.fe3d] [⚙] │ 44px Header
├───────────────────────┬──────────────────────────────────────────────────────────────────────────┤
│  PRIMARY ACTIONS      │  [🔍 Search recent projects, tags, or paths…          Ctrl+F] [Grid|List]│
│  ┌─────────────────┐  │                                                                          │
│  │ + New Project…  │  │  START FROM TEMPLATE (Real C++20 Engine Presets)                         │
│  ├─────────────────┤  │  ┌────────────────┐ ┌────────────────┐ ┌────────────────┐ ┌────────────┐ │
│  │ 📂 Open .fe3d…  │  │  │ [3D Wire Icon] │ │ [Grid Origin]  │ │ [PBR Sphere]   │ │ [Spot Rig] │ │
│  └─────────────────┘  │  │ General Scene  │ │ Empty Modeling │ │ LookDev Shader │ │ Studio     │ │
│                       │  │ Cube+Cam+Sun   │ │ Clean +Y Grid  │ │ Studio IBL+Ball│ │ 3-Pt Light │ │
│  HUB NAVIGATION       │  └────────────────┘ └────────────────┘ └────────────────┘ └────────────┘ │
│  ● Projects & Start   │  ──────────────────────────────────────────────────────────────────────  │
│  ○ Templates          │  RECENT PROJECTS (4)                         Sort: [Last Modified ▾]     │
│  ○ Recovery & Autosave│  ┌──────────────────────┐ ┌──────────────────────┐ ┌───────────────────┐ │
│  ○ Asset Libraries    │  │ [Real WebGL/GL Thumb]│ │ [Real WebGL/GL Thumb]│ │ [Real GL Thumb]   │ │
│  ○ Learning & Docs    │  │ ★ Mech_Torso_v03.fe3d│ │ Prop_Crate.fe3d      │ │ Arch_Room.fe3d    │ │
│                       │  │ 2h ago · 14.2 KB     │ │ Yesterday · Bundle   │ │ Oct 28 · 48.1 KB  │ │
│  ───────────────────  │  │ [Open]      [⋯ Menu] │ │ [Open]      [⋯ Menu] │ │ [Open]   [⋯ Menu] │ │
│  SYSTEM               │  └──────────────────────┘ └──────────────────────┘ └───────────────────┘ │
│  ⚙ Preferences (Ctrl+,│                                                                          │
│  ℹ Release & GPU Info │  [!] AUTOSAVE BANNER (Shown ONLY when an unrecovered crash snapshot      │
│                       │      exists): "1 unsaved session snapshot found (2m ago)" [Recover…]     │
├───────────────────────┴──────────────────────────────────────────────────────────────────────────┤
│ Engine Ready · Right-Handed +Y Up · Schema .fe3d v1.3 · GPU: WebGL 2.0 / GL 3.3   Check Updates  │ 26px Footer
└──────────────────────────────────────────────────────────────────────────────────────────────────┘
```

### 2.1 Region Dimensions
* **Top Identity Bar (`44px` height)**:
  * Left: Original geometric **Final Effect 3D Prism Emblem (`◈`)**, product wordmark, version pill (`v0.6.0 · GPL-3.0`), and active graphics backend badge (`OpenGL 3.3 Core` or `WebGL 2.0`).
  * Right: Conditional **`Resume Active Session (<ProjectName.fe3d>)`** accent button (shown whenever a Studio session is already loaded in memory) + **`Preferences (Ctrl+,)`** button.
* **Left Navigation Rail (`228px` fixed width on `1920×1080`; `204px` on `1366×768`)**:
  * **Primary CTA Stack**: `+ New Project… (Ctrl+N)` (`Solar Amber` / `Prism Cyan` primary button) and `Open Project… (Ctrl+O)` (opens `.fe3d`, `.fe3d` + `.fe3dbin` pair).
  * **Hub Views**:
    1. `Projects & Start` *(Default view: Quick Templates strip + Recent Projects)*
    2. `Templates` *(Full categorized template browser with metadata setup)*
    3. `Recovery & Autosave` *(Real autosave ring-buffer snapshots & crash recovery)*
    4. `Asset Libraries` *(Configured asset directories & starter PBR/Mesh packs as Asset Browser ships)*
    5. `Learning & Documentation` *(Built-in offline/repo docs viewer: Controls Guide, Shortcut Map, `.fe3d` Spec, Command Grammar)*
  * **Bottom System Links**: `Application Preferences (Ctrl+,)` and `About / GPU & Version Info`.
* **Main Hub Content Area (`flex: 1`, `max-width: 1440px`, scrollable vertically only)**:
  * Top utility row with **Instant Search Input (`Ctrl+F`)**, tag/type filter (`All`, `Pinned ★`, `Bundles`), Sort selector (`LastOpened`, `Name A–Z`, `File Size`), and `Grid` / `Compact Table` view toggle.
* **Bottom Status Strip (`26px` height)**:
  * Displays engine readiness, coordinate convention (`Right-Handed +Y Up`), `.fe3d` schema version, detected GPU renderer string, and static/manual version status (never phones home without explicit user action).

---

## 3. Specification of Required Hub Capabilities

### 3.1 New Project Dialog & Template System
Clicking a quick template card opens the project immediately with sensible defaults (`Untitled Project`), whereas clicking **`+ New Project… (Ctrl+N)`** or switching to the **`Templates`** tab exposes full project configuration:

* **Project Metadata Fields**:
  * `Project Name` (default: `Untitled Project`, validated non-empty)
  * `Author` (pre-filled from `AppPreferences.default_author`)
  * `Units & Grid Scale` (`Meters — 1.0`, `Centimeters — 0.01`, `Millimeters — 0.001`; persisted in project metadata/editor settings)
  * `Default Frame Rate` (`24 fps`, `30 fps`, `60 fps` -> initializes `scene::TimelineSettings::fps`)
  * `Initial Workspace` (auto-selected by template, e.g., `Layout`, `Modeling`, `Shading`)
* **Template Categories** (backed by `EditorController::createNewProject(ProjectTemplate, ...)`):
  * **General**: `General 3D Scene` (HeroCube, Camera, Key Directional Light, Ground Grid)
  * **Modeling**: `Clean Modeling Stage` (Empty origin with procedural +Y grid & `Modeling` workspace active)
  * **LookDev & Shading**: `Shader Ball Studio` (High-tessellation UV sphere + studio softbox HDRI IBL + `Shading` workspace)
  * **Lighting & Rendering**: `Three-Point Lighting Stage` (Backdrop cyclorama plane + Key/Fill/Rim lights + PCF shadows)
  * *(Additional categories—`Sculpting Base Mesh`, `Animation Rig Stage`, `2D/VSE Sequence`—are unlocked strictly in the stages that implement their C++ backing systems.)*

### 3.2 Open Project (`.fe3d` & `.fe3dbin`)
* Supports both single-file inline `.fe3d` JSON projects (`v1.0`, `v1.1`, `v1.2`, `v1.3+`) and split binary bundles (`.fe3d` + companion `.fe3dbin`).
* **Desktop Web Drag-and-Drop**: Dropping a `.fe3d` (and optional `.fe3dbin`) file anywhere onto the Home Hub window validates and opens it directly via Emscripten `MEMFS` -> `EditorController::loadProjectFile`.
* **Native Desktop**: Invokes OS file picker (`*.fe3d`) or accepts file drop via `GlfwWindow` drop callback (`glfwSetDropCallback`).
* If `ProjectDocument::load_warnings` is non-empty (e.g., unknown forward-version extension fields preserved in `ProjectPreservedFields` or missing companion texture), opens the Studio and surfaces a non-blocking **Load Diagnostics Banner** listing every warning verbatim.

### 3.3 Recent Projects & Project Search
* **Data Source**: Real persistent index (`fe3d.recent_projects.v2`). Each entry stores:
  * `id` (SHA-256 or deterministic path/name key), `project_name`, `file_name`, `path_or_origin`, `schema_version`, `modified_epoch_ms`, `byte_size`, `is_bundle`, `pinned`, `object_count`, `thumbnail_data_url` (`16:10` `320×200` real viewport capture).
* **Search (`Ctrl+F`)**: Filters `Recent Projects` and `Templates` in `< 5 ms` by substring match on `project_name`, `file_name`, `author`, or `path`.
* **Per-Card Context Actions (`⋯` or Right-Click)**:
  * `Open Project`
  * `Pin / Unpin to Top (★)`
  * `Duplicate as New Project…`
  * `Export Portable .fe3d Copy` *(Desktop Web downloads cached payload from `IndexedDB`)*
  * `Remove from Recent History` *(removes index entry & cached thumbnail only; never deletes user disk files)*
* **Clear Unpinned History** action with confirmation prompt.

### 3.4 File Recovery & Autosave Vault
* Backed by the periodic C++ autosave service specified in `PROJECT_DATA_ARCHITECTURE.md`:
  * Writes rolling snapshots (`autosave_slot_0..2.fe3d`) every `N` minutes (default `3 min`, configurable in Preferences) when `isDocumentDirty() == true`.
  * Marks session clean on graceful close/save; if an unclean session marker is detected on next boot, displays a non-intrusive **Recovery Banner** at the top of `Projects & Start` and lists all recoverable snapshots with timestamp, project name, object count, and `[Recover into Studio]` / `[Discard]` actions.

### 3.5 Asset Library, Learning & Preferences Entries
* **Asset Libraries View**: Displays built-in procedural primitives, studio HDRI presets (`Studio Softbox`, `Golden Sunset`, `Overcast Courtyard`), and PBR material presets currently shipped in the engine, plus user-imported `.fe3d` asset packs in Stage 6+.
* **Learning & Documentation View**: Renders built-in quick-reference cards directly from truthful application metadata:
  * Mouse & Keyboard Navigation Cheat Sheet (dynamically reflects user's active Input Profile!)
  * Structured `FE3D` Command Bar Reference
  * `.fe3d` Open File Format & GNU GPL v3.0 License Summary
* **Application Preferences (`Ctrl+,`)**: Opens the global Preferences modal (`Input & Navigation`, `Interface & DPI`, `Viewport & GPU`, `Files & Autosave`, `Keymap`).

---

## 4. Explicit UI States (Empty, Loading, Error, Populated)

| State | Visual & Behavioral Specification |
| :--- | :--- |
| **1. Initial Boot / Loading** | Compact centered initialization card showing real C++ boot stage (`1. Runtime Heap`, `2. Graphics Context`, `3. PBR & Shadow Shaders`, `4. Scene & Workspace Engine`) with elapsed ms. Transitions automatically in `< 1 frame` once `_fe3d_is_ready() == 1`. |
| **2. Empty Recent Projects (First Launch)** | Displays a clean, dashed-border `Recent Projects` panel stating: *"No recent projects yet. Start from a template above or open an existing `.fe3d` project file (`Ctrl+O`)."* **Zero sample/mock project cards.** |
| **3. Populated Recent Projects** | Responsive desktop grid (`repeat(auto-fill, minmax(250px, 1fr))` in Grid mode; dense `34px`-row sortable table in List mode), pinned (`★`) items sorted first. |
| **4. Missing / Corrupted File Error** | Inline dismissible `#hub-error-banner` (`#3A181C` surface, `1px` `#E55353` border) displaying the exact C++ `core::Error` code, line/offset diagnostic from `ProjectSerializer`, and `[Remove Broken Entry]` if triggered from a stale Recent card. |
| **5. Fatal Graphics Context Error** | If OpenGL 3.3 / WebGL 2.0 initialization fails, stays on the Diagnostic Error screen with copyable GPU/Browser report and CLI `--headless` instructions (never enters a broken Hub or Studio). |

---

## 5. Keyboard Accessibility in the Home Hub

* `Ctrl+N`: Focus / open New Project configuration
* `Ctrl+O`: Trigger Open `.fe3d` file dialog
* `Ctrl+F` or `/`: Focus Project Search input
* `Ctrl+,`: Open Application Preferences
* `Enter`: Open highlighted Template or Recent Project card
* `Escape`: Close modal or return to active 3D Studio session (when `Resume Active Session` is available)
