# Final Effect 3D — Stage 1 Reviewed Implementation Plan: Desktop Application Shell & Home / Project Hub

**Document ID:** `FE3D-SPEC-16`  
**Stage:** Stage 1 Planning Artifact (Authored at Conclusion of Stage 0 Review)  
**Document Status:** **`REVIEWED PLAN — NOT AUTHORIZED FOR IMPLEMENTATION`**  
**Baseline Commit:** `36aec73c539595997f4ab4aa8ae48ca2c80fd551` (`main`)  
**Target Platforms:** Native Desktop (`C++20` / `OpenGL 3.3 Core`) & Desktop Web (`WebAssembly` / `WebGL 2.0`)  
**Target Desktop Viewports:** `1920 × 1080` (Primary), `2560 × 1440` (QHD/HiDPI), `1366 × 768` (Compact Desktop Minimum)

---

> ## IMPLEMENTATION AUTHORIZATION GATE — NOT GRANTED
> **Status: `REVIEWED PLAN — NOT AUTHORIZED FOR IMPLEMENTATION`**  
> Authoring, committing, or reviewing this document does **NOT** authorize Stage 1 code changes. No application source files (`include/`, `src/`, `apps/`, `web/`), build configurations (`CMakeLists.txt`), serializers, or test suites may be modified until the product owner grants **explicit Stage 1 implementation approval**.

---

## 0. Binding Stage 0 Product Decisions Governing Stage 1

Stage 1 inherits and strictly enforces the three binding product decisions locked in Stage 0:

1. **Approved Visual Identity (`Decision A1`)**:
   * **Obsidian Slate** neutral dark surfaces (`#0F1115` void, `#161920` canvas/hub backdrop, `#191D24` headers, `#1F242D` cards/panels, `#272D38` elevated popovers, `#14171D` inputs, `#2C3340` crisp `1px` borders).
   * **Prism Cyan (`#27B6D9`)** for active tools, UI keyboard focus rings, selected navigation/workspace indicators, and primary interactive accents.
   * **Solar Amber (`#F29D38`)** reserved for active/lead 3D selection and primary brand accent highlights (`Coral Emerald #38C695` strictly for secondary multi-selection and semantic success states).
   * Generic Blender (`#303030` / `#4772b3`) and Maya (`#373737` / `#5285a6`) themes are permanently rejected.
2. **Approved Default Outliner Position (`Decision A2` — Stage 2 Boundary Note)**:
   * In the redesigned Main Studio (**Stage 2**), the **Scene Outliner resides in the LEFT dock by default** (independently resizable, collapsible, rearrangeable, and redockable) and remains conceptually separate from the right-docked **Inspector**.
   * **Stage 1 Discipline**: Stage 1 **does NOT modify the Main Studio panel layout or implement the new Left Outliner Studio dock**—Stage 1 preserves the existing Main Studio intact and enters/exits it cleanly via the new Application Shell.
3. **Approved Global Shortcuts, Three-Menu Separation & Focus Guard (`Decision A3`)**:
   * **`Space`** = **Context Action Menu** (editor/interaction-sensitive; keyboard-navigable; `Escape` closes; never executes an action merely on press).
   * **`Shift + Space`** = **Play / Pause** active timeline or simulation.
   * **`Ctrl + Space`** = **Maximize / Restore** Studio Area under pointer.
   * **`Ctrl + K` / `F3`** = **Global Command Palette** (fuzzy search across registered application actions).
   * **`RMB` in Viewport** = **8-Direction Radial Action Compass** *(Stage 3)*.
   * **Strict Keyboard Focus Guard**: Whenever keyboard focus is inside any text input, search box, numeric field, `<textarea>`, or command-entry input, `Space` inserts a normal space character (`' '`) and `Shift+Space` never hijacks typing. All shortcuts are registered in a centralized, user-remappable `ShortcutRegistry`.

---

## 1. Stage 1 Objectives (`C1`)

Stage 1 delivers the **Desktop Application Shell** and **Home / Project Hub** foundation without destabilizing the working C++20 3D engine or existing Studio workflow:

1. **Replace the Placeholder Multi-Studio Startup Screen**: Retire the 6-studio placeholder rail in `web/studios.js` (`Landscaping Studio`, `Character Studio`, `VFX Studio`, etc.) and `#app-home` markup in `web/index.html` in favor of a focused, desktop-grade **Final Effect 3D Home / Project Hub** (`#fe3d-project-hub`).
2. **Establish the Approved Design System Token Foundation**: Introduce `Obsidian Slate · Prism Cyan · Solar Amber` CSS custom properties (`tokens.css`) and reusable, accessible desktop UI primitives (`Button`, `Dialog`, `TextField`, `Select`, `SearchInput`, `EmptyState`, `ErrorBanner`, `CommandPalette`) scoped cleanly so the existing `#app-studio-3d` stylesheet remains conflict-free until Stage 2.
3. **Deliver Honest, End-to-End Project Lifecycle Operations**: Support real template creation (`StarterShowcase`, `MinimalCube`, `EmptyStage`, `ShaderBall`), valid `.fe3d` (`v1.0`–`v1.3`) and `.fe3dbin` (`v1.0`) opening, non-destructive corrupted-file error handling, platform-honest saving/exporting, persistent recent-project history, instant `< 5 ms` search, sorting, pinning, single-entry removal, history clearing, and missing-file detection.
4. **Introduce a Clean Top-Level Application Shell (`AppShellController`)**: Switch seamlessly between `Home / Project Hub` and the **existing Main Studio** (`#app-studio-3d`) in `< 16 ms` without full-page reloads, preserving live C++ `StudioSession` state, prompting before discarding unsaved scene changes, and pausing continuous 3D viewport redraws (`_fe3d_set_home_view_occluding(1)`) whenever Home occludes the Studio.
5. **Preserve Existing Studio Stability**: Keep all existing Main Studio modeling, lighting, material, camera, outliner, channel box, timeline, and `.fe3d` capabilities `100%` operational until Stage 2 replaces the Studio shell.

---

## 2. Strict Desktop Platform Scope & Restrictions (`C2`)

### 2.1 Supported Platforms
* **Native Desktop**: `C++20` + `OpenGL 3.3 Core` + `GLFW` (`apps/desktop/`, `finaleffect_desktop`) on Linux, Windows, and macOS, plus `--headless` `SoftwareRenderDevice` / `NullRenderDevice` for deterministic CI verification.
* **Desktop Web**: `C++20` compiled via Emscripten to `WebAssembly` + `WebGL 2.0` (`apps/web/`, `web/`) on desktop Chromium, Edge, Firefox, and Safari.

### 2.2 Target Desktop Viewport Matrix
| Viewport Tier | Resolution | Stage 1 Layout Contract |
| :--- | :---: | :--- |
| **Primary Full HD Target** | `1920 × 1080` (`1.0x–1.25x` DPI) | `44px` Top Identity Bar, `228px` Left Hub Rail, centered fluid Main Hub Content (`max-width: 1440px`), `3–4` column Recent/Template card grid, `26px` Status Footer. Zero horizontal overflow. |
| **QHD / 4K Workstation** | `2560 × 1440` (`1.5x–2.0x` HiDPI) | Crisp SVG iconography, configurable `--fe-ui-scale` (`85%–150%`), `4–5` column card grid, crisp HiDPI thumbnail rendering. |
| **Compact Laptop Minimum** | `1366 × 768` (`1.0x` DPI) | `204px` Left Hub Rail, `2–3` column card grid or dense `34px`-row Table view, all dialogs fit vertically inside `768px` without clipped footer buttons or horizontal scrollbars. |

### 2.3 Explicit Platform Exclusions
* **No iOS or Android targets**.
* **No mobile portrait (`390 × 844`), mobile landscape (`844 × 390`), or tablet responsive layouts** in the new Shell or Home Hub.
* **No touch-first UI inflation, coarse-pointer drawers, or pinch-gesture routers** in Stage 1 code.
* **No separate mobile app work** *(existing legacy mobile rules inside `#app-studio-3d` remain untouched during Stage 1 and are removed cleanly in Stage 2 alongside the Studio DOM replacement)*.

---

## 3. Specification & Wireframes of All 7 Required Stage 1 Home Views (`C3`)

### 3.0 Master Desktop Shell & Home Hub Frame (`1920 × 1080`)
```text
┌──────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ [◈] FINAL EFFECT 3D   v0.6.0 · GPL-3.0 · WebGL 2.0 / GL 3.3    [▶ Resume: Robot.fe3d*] [🔍 Ctrl+K][⚙]│ 44px Top Bar
├────────────────────────┬─────────────────────────────────────────────────────────────────────────────┤
│  PRIMARY ACTIONS       │                                                                             │
│  ┌──────────────────┐  │                                                                             │
│  │ + New Project…   │  │                     ACTIVE HOME VIEW CONTAINER                              │
│  ├──────────────────┤  │           (Projects & Start | Templates | Recovery & Autosave |             │
│  │ 📂 Open Project… │  │               Preferences | About & System Diagnostics)                     │
│  └──────────────────┘  │                                                                             │
│                        │                                                                             │
│  NAVIGATION            │                                                                             │
│  ● Projects & Start    │                                                                             │
│  ○ Templates           │                                                                             │
│  ○ Recovery & Autosave │                                                                             │
│  ────────────────────  │                                                                             │
│  SYSTEM                │                                                                             │
│  ○ Preferences (Ctrl+,)│                                                                             │
│  ○ About / System Info │                                                                             │
├────────────────────────┴─────────────────────────────────────────────────────────────────────────────┤
│ Engine Ready · Right-Handed +Y Up · Format: .fe3d v1.3 / .fe3dbin v1.0 · GPU: <Real Renderer>        │ 26px Footer
└──────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

### 3.1 View 1 — `Projects & Start` (Empty State & Populated State)
* **Primary Actions**: `+ New Project… (Ctrl+N)`, `Open Project… (Ctrl+O)`, and conditional **`▶ Resume Active Session (<name>)`** button (visible whenever a project is currently loaded in `StudioSession`, showing `*` when `isDocumentDirty() == true`).
* **Quick-Start Templates Strip**: 4 compact cards mapped 1-to-1 to the real C++ `ProjectTemplate` enum (`Starter Showcase`, `Minimal Cube`, `Empty Stage`, `Shader Ball`). Clicking a card creates that template immediately (after an unsaved-work confirmation if the current Studio session is dirty) and enters the Studio.
* **Recent Projects Section**:
  * **Clean Empty State (Mandatory on First Launch)**: Dashed `#2C3340` panel reading *"No recent projects yet. Start from a template above (`Ctrl+N`) or open an existing `.fe3d` project from disk (`Ctrl+O`)."* **Zero sample, demo, or fabricated project entries.**
  * **Populated State**: Supports `Grid` (`16:10` real captured WebGL/OpenGL viewport thumbnail, or neutral geometric SVG fallback badge if saved headless without thumbnail) and `Table` (`Name`, `Location/Origin`, `Last Opened`, `Size`, `Schema`, `Actions`) layouts.
  * **Search & Sort**: Instant substring search (`Ctrl+F`) across `project_name`, `file_name`, `author`, and `path` (`< 5 ms` for `100` entries); Sort by `Last Opened`, `Name (A–Z)`, or `File Size`; filter toggle for `Pinned ★`.
  * **Per-Entry Actions**: `Open`, `Pin / Unpin (★)`, `Export .fe3d Copy` (on Web when cached in `IndexedDB`), `Remove from Recent List`, plus `Clear Unpinned Recent History` with confirmation dialog.
  * **Missing-File State**: When a Native Desktop path no longer exists on disk (or a Web `FileSystemFileHandle` permission/file was revoked and no `IndexedDB` payload exists), the card displays an explicit `Missing File` badge with `[Locate File…]` and `[Remove Entry]` instead of silently failing.

### 3.2 View 2 — `Templates`
* Displays **exclusively real templates** implemented in `EditorController::createNewProject` (`src/app/editor_controller.cpp`):
  1. **`Starter Showcase` (`ProjectTemplate::StarterShowcase`)** — Default showcase mesh, perspective camera, directional sun light with shadows, ground grid.
  2. **`Minimal Cube` (`ProjectTemplate::MinimalCube`)** — Single unit editable polygon cube at world origin `(0, 0, 0)` with default lighting and camera.
  3. **`Empty Modeling Stage` (`ProjectTemplate::EmptyStage`)** — Clean right-handed `+Y` world origin with procedural grid and camera, ready for from-scratch modeling or asset import.
  4. **`Shader Ball Studio` (`ProjectTemplate::ShaderBall`)** — High-segment UV sphere configured with Cook-Torrance GGX PBR material and studio lighting for material look-development.
* Selecting any template pre-populates the project setup panel (`Project Name`, `Author`, `Units`, `FPS`) and launches directly into the Studio. **Zero `"Coming Soon"` or fake domain studio cards.**

### 3.3 View 3 — `New Project` Dialog / View (`Ctrl+N`)
* Accessible from `+ New Project…` (`Ctrl+N`) in both the Home Hub and Studio shell bridge:
  * **Inputs**: `Project Name` (default `"Untitled Project"`, trimmed & validated non-empty), `Author` (default from `AppPreferences.defaultAuthor`), `Template` (`StarterShowcase` | `MinimalCube` | `EmptyStage` | `ShaderBall`), `Scene Units` (`Meters`, `Centimeters`, `Millimeters`), and `Frame Rate` (`24`, `30`, `60` FPS -> applied to `TimelineSettings::fps`).
  * **Dirty-Session Protection**: If `EditorController::isDocumentDirty()` is `true`, prompts explicitly (`Save & Continue`, `Discard Unsaved Changes`, `Cancel`) before replacing `StudioSession`'s active document.
  * **Keyboard Behavior**: Focus starts in `Project Name` (selected for immediate typing); `Space` inside `Project Name` or `Author` types literal `' '`; `Enter` submits; `Escape` cancels cleanly.

### 3.4 View 4 — `Open Project` Dialog / Flow (`Ctrl+O` & Drag-and-Drop)
* **Supported Formats**: `.fe3d` (`v1.0`, `v1.1`, `v1.2`, `v1.3`) single-file JSON projects and split `.fe3d` + companion `.fe3dbin` (`v1.0`) binary geometry bundles.
* **Platform-Specific File Access & Honest Fallback**:
  1. **Desktop Web — Runtime Feature Detection**:
     * Checks `const supportsFileSystemAccess = typeof window.showOpenFilePicker === "function" && typeof window.showSaveFilePicker === "function" && window.isSecureContext;`.
     * **When `true` (Desktop Chromium / Edge)**: Uses `showOpenFilePicker` to acquire a `FileSystemFileHandle` (plus optional companion `.fe3dbin` handle/file if referenced) so subsequent `Save (Ctrl+S)` writes directly back to the user's file handle after browser permission.
     * **When `false` (Desktop Firefox / Safari / Non-Secure Localhost)**: Uses standard `<input type="file" accept=".fe3d,.fe3dbin" multiple>` and window drag-and-drop for Open, and uses deterministic `<a download="<filename>.fe3d">` export + `IndexedDB` recent-project cache for Save/Save As—displaying truthful UI copy (`"Exported <name>.fe3d"` rather than pretending direct disk overwrite occurred).
  2. **Native Desktop**: Uses `LocalFileSystem` direct path read and atomic temp-file swap (`<name>.fe3d.tmp` -> `<name>.fe3d`) on Save.
* **Non-Destructive Error Handling**:
  * Validates JSON syntax, schema version, and `.fe3dbin` CRC32 chunks inside C++ `ProjectSerializer` **before** replacing the active `StudioSession` document.
  * On failure (corrupt JSON, truncated `.fe3dbin`, unsupported major version `> 1`), renders a dismissible `#hub-error-banner` showing the exact C++ `core::Error` message and **leaves any already-loaded Studio project 100% intact**.

### 3.5 View 5 — `Recovery & Autosave`
* **Architectural Honesty Requirement**: Baseline commit `36aec73` does **not** yet implement a background C++ autosave ring-buffer writer.
* **Stage 1 Contract**:
  * **Option A (If real `IndexedDB` / `LocalFileSystem` dirty-session snapshot storage is implemented in Phase 1.8)**: Lists only real captured recovery snapshots (`project_name`, `captured_at`, `byte_size`, `object_count`) with working `[Recover into Studio]` and `[Discard Snapshot]` buttons, and shows an empty state when zero snapshots exist.
  * **Option B (When running in an environment where autosave storage is disabled or unavailable)**: Displays an explicit, truthful **Unavailable / Empty Recovery State** explaining that automatic background snapshots are not active in the current runtime and reminding users to save `.fe3d` checkpoints via `Ctrl+S`.
  * **Strictly Forbidden**: Fake recovery cards, hardcoded timestamps, or non-functional `"Autosave every 5 min"` checkboxes disconnected from real persistence.

### 3.6 View 6 — `Preferences` (`Ctrl+,`)
* Persisted via `PreferencesService` (`fe3d.preferences.v1` in `localStorage` on Desktop Web; `preferences.json` in user config path on Native Desktop)—**never written into `.fe3d` scene files** so opening external projects never mutates user preferences.
* **Exposes ONLY Functional Stage 1 Settings**:
  1. **UI Scale / Density**: `85%`, `100%` (default), `115%`, `125%` — immediately updates root `--fe-ui-scale` CSS variable and triggers viewport `ResizeObserver` recalculation.
  2. **Startup Environment**: `Open Home / Project Hub` (default) vs. `Boot Directly into Default Studio Scene`.
  3. **Default Project Author**: Pre-fills `Author` in `New Project`.
  4. **Viewport Navigation Profile**: `FE3D Hybrid (MMB + Alt+Mouse)` / `Blender` / `Maya` *(exposed only when wired directly to the active viewport input handler)*.
  5. **Recent Projects Retention**: Max entries (`10`, `20`, `50`) + `Clear Unpinned Recent Projects`.
  6. **Keyboard Shortcuts**: Read/remap bindings managed by `ShortcutRegistry` with `Reset to Defaults` (`Space`, `Shift+Space`, `Ctrl+Space`, `Ctrl+K`/`F3`, `Ctrl+N`, `Ctrl+O`, `Ctrl+S`, `Ctrl+,`).

### 3.7 View 7 — `About / System Info`
* Displays **100% runtime-queried truthful telemetry**:
  * Application Name (`Final Effect 3D`), Version & Build Target (`WebAssembly / Emscripten` or `Native C++20`), Active Graphics Device (`OpenGL 3.3 Core` or `WebGL 2.0`), Real GPU Renderer & Vendor (`GL_RENDERER` / `WEBGL_debug_renderer_info` when exposed by browser), Supported Project Formats (`.fe3d v1.0–v1.3`, `.fe3dbin v1.0`), Coordinate System (`Right-Handed +Y Up`), `[Copy Diagnostics JSON]` button for bug reports, and `GNU GPL v3.0-or-later` license summary.

---

## 4. Stage 1 Modular Architecture Requirements (`C4`)

### 4.1 Module & Directory Layout
Stage 1 introduces a clean ES-module directory structure under `web/src/` (loaded cleanly without requiring heavy third-party frameworks) paired with focused C++ bridge hooks where required:

```text
web/src/
├── main_shell.js                    # Boot sequence, WASM readiness listener, AppShell bootstrap
├── theme/
│   ├── tokens.css                   # Approved Obsidian Slate + Prism Cyan (#27B6D9) + Solar Amber (#F29D38)
│   └── shell_hub.css                # Scoped exclusively to .fe-shell / #fe3d-project-hub / .fe-modal
├── shell/
│   ├── app_shell_controller.js      # Environment router (Home <-> Studio), dirty-session guard, occlusion pause
│   ├── shortcut_registry.js         # Centralized remappable shortcuts + strict editable-input focus guard
│   ├── command_action_registry.js   # Typed action descriptors + Ctrl+K / F3 Global Command Palette
│   └── error_boundary.js            # Structured error banners, unhandled rejection capture, non-destructive alerts
├── services/
│   ├── project_operations.js        # IProjectOperations + Web (FS Access API + fallback) / Native adapters
│   ├── recent_projects_service.js   # IndexedDB/Storage v2 index (v1 migration), search, sort, pin, thumbnails
│   ├── recovery_service.js          # Real snapshot adapter OR honest capability-unavailable state reporter
│   └── preferences_service.js       # Schema-validated AppPreferences (strictly separate from .fe3d scene data)
├── ui/
│   ├── button.js                    # Accessible desktop buttons, icon buttons, shortcut badges
│   ├── dialog.js                    # Focus-trapped modal dialog (Esc close, backdrop guard)
│   ├── form_controls.js             # TextField, Select, SearchInput (with focus-guard integration)
│   └── feedback_states.js           # EmptyState, LoadingCard, ErrorBanner, ConfirmUnsavedDialog
└── hub/
    ├── home_view_registry.js        # Lifecycle registry (mount / update / focus / dispose) for Hub views
    ├── home_sidebar_nav.js          # Left navigation rail + Primary CTAs + Resume Session pill
    ├── views/
    │   ├── projects_start_view.js   # Quick Templates + Recent Projects Grid/Table + Search/Sort/Pin
    │   ├── templates_view.js        # 4 verified C++ templates + inline project metadata launch
    │   ├── new_project_dialog.js    # Validated New Project modal
    │   ├── open_project_flow.js     # File picker + drag-drop + .fe3dbin pairing + error preservation
    │   ├── recovery_view.js         # Real recovery list or honest unavailable state
    │   ├── preferences_modal.js     # Functional-only AppPreferences UI
    │   └── about_system_view.js     # Truthful version, GPU, schema, and GPLv3 diagnostics
```

### 4.2 Critical Architectural Invariants
1. **Zero Full-Document JSON Polling Loops**: `AppShellController` and Hub views update strictly on user commands, async file I/O completion, and C++ `stateRevision()` change notifications—preserving the event/delta synchronization model toward Stage 2's C++ `DirtyDomainMask`.
2. **Strict CSS Isolation from Legacy Studio**: All Stage 1 styles use `.fe-shell`, `#fe3d-project-hub`, and `.fe-modal` namespaces + `--fe-*` custom properties so existing `#app-studio-3d` rules in `web/editor.css` suffer zero selector collisions prior to Stage 2.
3. **Strict Separation of User Preferences vs. Scene Files**: `PreferencesService` never writes to `ProjectDocument`, and `ProjectOperations.openProject()` never mutates `AppPreferences`.
4. **Centralized Shortcut Focus Guard (`ShortcutRegistry.shouldIgnoreKeyEvent(event)`)**:
   * Returns `true` whenever `event.target` (or `document.activeElement`) matches `input:not([type="checkbox"]):not([type="radio"]):not([type="button"])`, `textarea`, `select`, or `[contenteditable="true"]`.
   * Guarantees `Space` and `Shift+Space` behave as standard text input keys inside every search box, project-name field, numeric channel input, Outliner rename box, and `FE3D` Command Bar.
   * Keeps **Context Action Menu (`Space`)**, **Global Command Palette (`Ctrl+K` / `F3`)**, and **Radial Action Compass (`RMB` in viewport, Stage 3)** architecturally separate in `command_action_registry.js`.

---

## 5. Ordered 14-Phase Stage 1 Execution Plan (`C5`)

*(To be executed strictly in sequence **only after** explicit Stage 1 implementation authorization is granted.)*

| Phase | Title | Concrete Engineering Scope & Verification |
| :-: | :--- | :--- |
| **1.1** | **Audit Existing Startup, Home & Project Paths** | Trace exact boot, `#boot-splash`, `#app-home`, `web/studios.js`, `#btn-home-hub`, `_fe3d_set_home_view_occluding`, `_fe3d_create_new_project`, `_fe3d_load_project_json`, `_fe3d_save_project_json`, and `IndexedDB` (`fe3d_projects_db`) touchpoints so zero Studio entry hooks are broken. |
| **1.2** | **Introduce Approved Design Tokens & Shared Desktop Components** | Author `web/src/theme/tokens.css` (**Obsidian Slate · Prism Cyan `#27B6D9` · Solar Amber `#F29D38` · Coral Emerald `#38C695`**) and build `Button`, `Dialog`, `TextField`, `Select`, `SearchInput`, `EmptyState`, and `ErrorBanner` in `web/src/ui/`. |
| **1.3** | **Implement Application Shell & Environment Switcher** | Build `AppShellController` managing `Boot -> Home <-> Studio` transitions without page reloads, wiring `[◈ Hub]` / `Resume Active Session`, dirty-flag (`*`) header sync, and `_fe3d_set_home_view_occluding(1/0)` GPU idle pause. |
| **1.4** | **Implement Project Operations Interface (`IProjectOperations`)** | Build `WebProjectAdapter` (runtime `showOpenFilePicker`/`showSaveFilePicker` feature detection + `<input type="file">` / `<a download>` fallback + `.fe3dbin` companion pairing) and Native Desktop CLI/file parity hooks. |
| **1.5** | **Implement Recent Projects Service** | Build `RecentProjectsService` with non-destructive `v1 -> v2` `IndexedDB` record migration, real viewport thumbnail capture on save, `< 5 ms` search/sort, pin/unpin, single-entry remove, clear unpinned history, and missing-file status detection. |
| **1.6** | **Implement Preferences Service** | Build schema-validated `PreferencesService` (`uiScale`, `startupMode`, `defaultAuthor`, `navigationProfile`, `recentLimit`, `keymapOverrides`) persisted independently of `.fe3d` scene documents. |
| **1.7** | **Build `Projects & Start` and `Templates` Views** | Implement `projects_start_view.js` (honest empty state, populated grid/table, search/sort/pin/remove/clear) and `templates_view.js` (strictly the `4` real C++ `ProjectTemplate` presets). |
| **1.8** | **Build `New Project` and `Open Project` Flows** | Implement `new_project_dialog.js` (metadata validation + unsaved-changes confirmation guard) and `open_project_flow.js` (drag-and-drop + picker + atomic C++ validation preserving active Studio session on corrupt `.fe3d` failure). |
| **1.9** | **Build `Recovery & Autosave` View** | Connect strictly to real implemented dirty-session recovery snapshots or render the explicit honest unavailable/empty state (**zero fake snapshots**). |
| **1.10** | **Build `Preferences` Modal & `About / System Info` View** | Implement `preferences_modal.js` (`Ctrl+,`, functional-only controls) and `about_system_view.js` (live version, backend, GPU renderer, `.fe3d` schema, GPLv3 license, `[Copy Diagnostics]`). |
| **1.11** | **Integrate Entry Into & Return From Existing Studio** | Replace `web/studios.js` wiring in `web/index.html` with `web/src/main_shell.js`; verify `New`, `Open`, `Template`, `Recent`, `Save`, `Save As`, and `Return to Hub` round-trip seamlessly with `#app-studio-3d`. |
| **1.12** | **Add Centralized Shortcut Registry & Command Palette Foundation** | Wire `ShortcutRegistry` (`Ctrl+N`, `Ctrl+O`, `Ctrl+S`, `Ctrl+F`, `Ctrl+,`, `Ctrl+K`/`F3`, `Escape`) + `CommandPalette` + strict text-input focus guard (`Space` / `Shift+Space` never hijack inputs). |
| **1.13** | **Desktop Layout Polish & Error-Handling Hardening** | Audit bounding-box geometry, keyboard tab order, focus rings (`#27B6D9`), contrast, and overflow at `1920×1080`, `2560×1440` (`2x` DPI), and `1366×768`. |
| **1.14** | **Desktop Unit, Integration & Playwright E2E Test Suite** | Add unit tests for `RecentProjectsService`, `PreferencesService`, `ShortcutRegistry` focus guard, and `ProjectOperations` fallbacks + full Desktop Chromium WebGL 2.0 E2E coverage across all 3 desktop viewports. |

---

## 6. Explicit Stage 1 Exclusions (`C6 — Strict Scope Lock`)

To prevent scope creep and mid-refactor regressions, the following items are **strictly excluded** from Stage 1:

| Excluded Work Area | Deferred To | Reason for Exclusion in Stage 1 |
| :--- | :---: | :--- |
| **Main Studio Interior UI Redesign** (Two-Tier `56px` Studio Top Bar, Recursive `AreaNode` Split Tree, Unified Quick Channels + Deep Inspector) | **Stage 2** | Stage 1 establishes the Shell & Home Hub while keeping the existing Studio operational; mixing both doubles UI regression risk. |
| **Left Outliner Dock Implementation in Studio** | **Stage 2** | Default Left Outliner (`Decision A2`) is a binding Stage 2 Main Studio architecture requirement, not a Stage 1 Hub task. |
| **Legacy Studio Mobile DOM/CSS Deletion** | **Stage 2** | Removing `#mobile-portrait-nav` and `editor.css` portrait rules happens atomically when `#app-studio-3d` is replaced in Stage 2. |
| **Multi-Viewport (`Quad 4-View`), Box Marquee & `RMB` Radial Compass** | **Stage 3** | Requires C++ multi-scissor `ViewportSubPanel` pass and Studio Area shell from Stage 2. |
| **New Polygon Modeling Operators & Modifiers** (`Bevel`, `Bridge`, `Loop Cut`, `Multi-Cut`, `.obj`) | **Stage 4** | Pure C++ topology & Studio Operator HUD work scheduled after viewport/marquee upgrades. |
| **Digital Sculpting (`MeshBVH`) & Geometry Nodes** | **Stage 5** | Advanced geometry & DAG evaluation stage. |
| **2D UV Editor, Shader Node Graph & glTF 2.0** | **Stage 6** | Builds atop Stage 5's shared `NodeGraph` + `NodeCanvas`. |
| **Bezier Curve Animation (`Dope Sheet`/`Graph Editor`) & Skeletal Rigging** | **Stage 7** | Animation & character deformation stage. |
| **Progressive Rendering, Render View & AOV Passes** | **Stage 8** | Rendering pipeline expansion stage. |
| **Physics, Cloth & Particle Simulation** | **Stage 9** | Simulation engine stage. |
| **2D Node Compositor & Video Sequence Editor (`VSE`)** | **Stage 10** | Post-production pipeline stage. |
| **`.fe3d` Schema Version Bump (`v1.3` → `v1.4`)** | **Stage 2 / 4** | Stage 1 reads and writes standard `.fe3d` `v1.0–v1.3` + `.fe3dbin` `v1.0` with zero format changes. |
| **Any Mobile / Tablet / iOS / Android / Touch-First UI Work** | **Permanently Excluded** | Final Effect 3D is strictly Desktop-First (`Native Desktop` + `Desktop Web`). |

---

## 7. Stage 1 Objective Acceptance Criteria (`C7`)

Stage 1 cannot be marked complete unless **every** criterion below is verified by automated tests:

1. **Desktop Viewport & Visual Identity Verification**:
   * Renders with zero horizontal overflow, zero clipped dialogs, and zero overlapping regions at **`1920 × 1080`**, **`2560 × 1440`** (`deviceScaleFactor: 2`), and **`1366 × 768`**.
   * Computed CSS surface and accent variables match the approved **Obsidian Slate · Prism Cyan (`#27B6D9`) · Solar Amber (`#F29D38`)** palette (`Decision A1`).
2. **Honest Empty State**:
   * Launching with clean browser/disk storage renders the empty Recent Projects state with **zero fake projects**, **zero sample cards**, and **zero fake thumbnails**.
3. **Real C++ Template Launch**:
   * Launching each of the `4` templates (`StarterShowcase`, `MinimalCube`, `EmptyStage`, `ShaderBall`) creates the corresponding real C++ `ProjectDocument` in `StudioSession` and enters the Studio smoothly without a page reload.
4. **Open, Save & Corrupted-File Resilience**:
   * Opening valid `.fe3d` (`v1.0–v1.3`) and `.fe3d` + `.fe3dbin` bundles succeeds and populates scene objects accurately in C++.
   * Web File System Access API is feature-detected at runtime with verified clean fallback (`<input type="file">` / `<a download>`) when unavailable.
   * Opening malformed JSON or a corrupted `.fe3dbin` surfaces a structured error banner and **preserves any already-open Studio project untouched**.
5. **Recent Projects Persistence & Management**:
   * Saved/opened projects persist across page/application reloads, filter in `< 5 ms` via Search (`Ctrl+F`), sort accurately, pin/unpin (`★`), remove single entries, clear unpinned history, and flag missing files honestly.
6. **Honest Recovery & Functional-Only Preferences**:
   * Recovery screen reflects real stored snapshots or displays the honest unavailable/empty state (**zero fabricated entries**).
   * Every control in `Preferences (Ctrl+,)` produces a measurable runtime effect and persists separately from `.fe3d` scene files.
7. **Keyboard Shortcuts & Focus-Guard Safety (`Decision A3`)**:
   * `Ctrl+N`, `Ctrl+O`, `Ctrl+F`, `Ctrl+,`, `Ctrl+K` / `F3`, and `Escape` work deterministically.
   * Automated focus-guard tests confirm that pressing `Space` and `Shift+Space` inside search, project-name, author, numeric, or command inputs inserts normal text and **never** triggers menus or playback.
8. **Zero Regression Gate**:
   * All `19/19` baseline C++ CTests + new Stage 1 tests pass under `-Werror` / `/WX`, and all existing Studio workflows remain functional after entering Studio from the new Home Hub.

---

## 8. Stage 1 Deliverables (`C8`) & Authorization Lock

When Stage 1 is eventually authorized and executed, its deliverables will consist of:
1. Scoped **Obsidian Slate · Prism Cyan · Solar Amber** token stylesheet (`web/src/theme/tokens.css` & `shell_hub.css`) and reusable desktop UI primitives (`web/src/ui/`).
2. Modular **Application Shell** (`AppShellController`, `ShortcutRegistry` with focus guard, `CommandActionRegistry` + `Ctrl+K`/`F3` Command Palette, `ErrorBoundary`).
3. Platform-honest **Project, Recent Projects, Recovery, and Preferences Services** (`web/src/services/` + C++ preference/session hooks where needed).
4. Complete **7-View Desktop Home / Project Hub** (`web/src/hub/`) replacing `web/studios.js` and `#app-home`.
5. Automated unit, integration, and Desktop Playwright E2E test suite (`1920×1080`, `2560×1440`, `1366×768`) + green 8-job GitHub Actions CI matrix.

> **FINAL GATE REMINDER:**  
> This document (`STAGE_1_REVIEWED_PLAN.md`) is a **reviewed planning specification only**. Stage 1 implementation remains **LOCKED (`NOT AUTHORIZED FOR IMPLEMENTATION`)** pending explicit product-owner instruction to begin Stage 1 coding.
