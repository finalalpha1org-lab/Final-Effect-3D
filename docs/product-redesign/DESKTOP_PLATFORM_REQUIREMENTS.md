# Final Effect 3D — Desktop Platform Requirements & Mobile Decommissioning Audit

**Document ID:** `FE3D-SPEC-11`  
**Stage:** Stage 0 — Product Reset, Original Hybrid UI Design & Architecture Specification  
**Supported Targets:** Native Desktop (`C++20` + OpenGL 3.3) & Desktop Web (`WebAssembly` + WebGL 2.0)  
**Explicitly Out of Scope:** iOS, Android, Mobile Portrait, Mobile Landscape, Touch-First UI

---

## 1. Supported Desktop Platform Matrix

| Target Platform | OS / Browser Baseline | Graphics API | Build Toolchain & CI Gate |
| :--- | :--- | :--- | :--- |
| **Native Desktop — Linux** | Ubuntu 22.04+ / Fedora / Arch (`x86_64`, `aarch64`) | OpenGL 3.3 Core Profile + GLFW 3.3+ | CMake + Ninja + GCC 12+ / Clang 15+ (`-Werror`) |
| **Native Desktop — Windows** | Windows 10 / 11 (`x64`) | OpenGL 3.3 Core Profile + GLFW 3.3+ | CMake + MSVC 2022 (`/W4 /WX`) |
| **Native Desktop — macOS** | macOS 13+ (`Apple Silicon` & `x86_64`) | OpenGL 3.3 Core Profile *(Metal backend bridge ready via `IRenderDevice` + `ClipSpaceDepth::ZeroToOne`)* | CMake + AppleClang (`-Werror`) |
| **Desktop Web Application** | Desktop Chrome / Edge `115+`, Firefox `115+`, Safari `16.4+` | WebAssembly + WebGL 2.0 (`GLSL ES 3.00`) | Emscripten SDK (`emcmake`) + Playwright Desktop Chromium E2E |
| **Headless CI / Batch Runner** | Linux / Windows / macOS CLI (`--headless`) | Deterministic CPU `SoftwareRenderDevice` / `NullRenderDevice` | Zero windowing/GPU drivers required; runs all CTests anywhere |

---

## 2. Reference Desktop Display & Windowing Requirements

### 2.1 Viewport & Window Breakpoints
Final Effect 3D operates strictly as a **Desktop Workstation Window** (resizable down to `1024 × 640` minimum window clamp, never collapsing into mobile drawers):

| Tier | Resolution | UI Density & Layout Behavior |
| :--- | :---: | :--- |
| **Compact Laptop Minimum** | `1366 × 768` (`1.0x` DPI) | Default Left Tool Rail `40px`, **Default Left Outliner Dock** `196px` (independently collapsible), Right Inspector `288px`, Workspace tabs show scroll chevrons `[‹][›]` only if `> 8` tabs open, 3D Viewport retains `>= 61%` width (`>= 842px`). Zero horizontal scrollbar. |
| **Primary Full HD Reference** | `1920 × 1080` (`1.0x–1.25x` DPI) | Default workstation proportions (`44px` Left Tool Rail, `220px` **Default Left Outliner Dock**, `300px` separate Right Inspector, `>= 58%` 3D Viewport pixel share with Timeline open and `>= 65%` with Timeline collapsed). |
| **QHD / 4K HiDPI Workstation** | `2560 × 1440` & `3840 × 2160` (`1.5x–2.0x` DPI) | Vector SVG iconography and CSS/UI layout scale crisply via `devicePixelRatio` + user `--fe-ui-scale` (`85%–150%`); WebGL 2.0 / OpenGL 3.3 renderbuffer allocates exact physical backing pixels (`cssWidth * dpr`, `cssHeight * dpr`) so wireframes, gizmos, and procedural grid lines remain razor-sharp at `200%` scaling. |

### 2.2 Desktop Input Hardware Requirements
* **Primary Input**: 3-button scroll-wheel desktop mouse (`Left`, `Middle`, `Right`, `Wheel`, optional `Back/Forward` buttons) + standard physical keyboard (`Modifiers: Shift, Ctrl/Cmd, Alt`).
* **2-Button / Trackpad Laptop Accessibility**: Because `Alt + LMB` (Orbit), `Alt + Shift/MMB` (Pan), `Alt + RMB` / `Two-Finger Wheel` (Zoom), and the top-right **Interactive 3D View Axis Gizmo** are built into the `FE3D Hybrid` profile, desktop laptop users without a physical middle mouse button retain 100% navigation capability—without polluting the UI with mobile touch drawers.
* **Graphics Pen Tablet Support (Desktop Wacom / Huion)**: Standard W3C `PointerEvent.pressure` (`0.0..1.0`) and `Alt`/barrel-button navigation work seamlessly on desktop pen displays for Stage 5 Sculpting and Stage 7 Weight Painting.
* **macOS Command/Control Normalization**: `Ctrl+Z/Y/S/O/N/D/G/K/A` maps automatically to `Cmd` (`metaKey`) on macOS Desktop Web & Native macOS while keeping `Ctrl` modifier combos (`Ctrl` snap toggle) conflict-free.

---

## 3. Audited Mobile Code Decommissioning Plan (For Stage 1–2 Execution)

Per **Section 2 & Section 14 Restrictions**, **zero mobile code is deleted during Stage 0**. Every mobile dependency has been audited below to confirm it is **not** required by Desktop Web or Native Desktop before scheduled removal in Stages 1–2:

| File | Exact Mobile Artifact | Desktop Dependency Check | Scheduled Action (Stage 1–2) |
| :--- | :--- | :--- | :--- |
| `web/index.html:1497-1505` | `<nav class="mobile-portrait-nav" id="mobile-portrait-nav">` (`7` bottom drawer buttons) | Hidden (`display: none`) on all desktop viewports (`>= 960px`). Zero desktop callers. | **Safe to Remove in Stage 2** when Studio DOM is rebuilt. |
| `web/index.html:533, 802, 907, 1041, 1359` | `#btn-landscape-inspector-toggle` & `4×` `.mp-drawer-close-btn` | Hidden on desktop (`display: none`). Desktop uses dock collapse chevrons & `Ctrl+Space`. | **Safe to Remove in Stage 2**. |
| `web/editor.css:2504-3034, 4336-4359` | `~555` lines of `body[data-layout-mode="portrait"]` and `body[data-layout-mode="landscape"]` rules | Only matched when JS sets `data-layout-mode="portrait" \| "landscape"` below `960px`. **Caution:** Desktop compact windows (`1024px–1366px`) must use clean desktop flex/grid min-widths, never `landscape`/`portrait` modes! | **Safe to Remove in Stage 2**; lock layout mode strictly to Desktop workstation behavior. |
| `web/editor.js` (`computeResponsiveLayoutMode`, `setMobilePortraitDrawer`, 2-finger pinch state, `+Multi` touch pill) | Mobile layout switcher & multi-touch gesture recognizer | **Dependency Audit:** Standard desktop pointer capture (`pointerdown`/`pointermove`/`pointerup` for `mouse` & `pen`) MUST be kept; only `pointerType === "touch"` two-finger pinch & `< 960px` portrait drawer state are mobile-specific. | **Refactor cleanly in Stage 2** into a pure Desktop Mouse/Pen/Keyboard Input Controller. |
| `apps/web/main.cpp` (`fe3d_touch_tap`) | 1 exported C wrapper calling `pickAtScreenPosition` | Only called by mobile touch-tap branch in `web/editor.js`. Desktop uses `onMouseDown`/`onMouseUp` / `pickViewportPixel`. | **Remove export in Stage 2** after `web/` caller is retired. |
| `docs/ARCHITECTURE.md` & `docs/PLATFORM_ROADMAP.md` | References to future `platform/ios/` & `platform/android/` | Documentation only; no CMake targets exist for iOS/Android. | **Update docs in Stage 1** to reflect Desktop-First product scope. |
| `web/e2e/studio.spec.mjs` & `capture-baseline.mjs` | Mobile `390×844` & `844×390` viewport test blocks | E2E tests only. | **Replace in Stage 1–2** with Desktop `1366×768`, `1920×1080`, and `2560×1440` HiDPI E2E verification suites. |

---

## 4. Desktop Browser Compatibility & Sandboxing Rules

1. **Zero Server Backend Requirement**: The Desktop Web build (`finaleffect_web`) compiles to static `.html` + `.js` + `.wasm` files runnable over any static HTTP server (COOP/COEP compatible, single-threaded WASM default for maximum cross-browser hosting compatibility, optional SIMD128 `-msimd128` acceleration).
2. **Browser Shortcut Conflict Prevention**: Calling `preventDefault()` when Studio canvas/shell is active cleanly intercepts `Ctrl+S` (Save Project instead of "Save Webpage"), `Ctrl+O` (Open `.fe3d`), `Ctrl+D` (Duplicate instead of Bookmark), `Ctrl+G` (Group instead of Find Next), `Space` / `Tab` / `Alt` (prevent Windows browser `Alt` menu-bar focus steal), while leaving `F11` (OS Fullscreen) and `Ctrl+Shift+I` (DevTools) functional when needed.
3. **WebGL Context Loss Resilience**: Listens to `webglcontextlost` / `webglcontextrestored` on `#fe3d-canvas`; because C++ `Scene` owns authoritative CPU copies of every `EditableMesh`, `PbrMaterial`, and `EnvironmentLighting`, `Renderer::releaseSceneResources()` + re-upload restores all GPU VAOs/VBOs/shaders/textures without losing a single vertex of user work!
