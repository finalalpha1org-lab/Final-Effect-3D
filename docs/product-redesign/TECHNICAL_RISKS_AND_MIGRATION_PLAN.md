# Final Effect 3D — Technical Risks, Refactoring Sequence & Desktop Migration Plan

**Document ID:** `FE3D-SPEC-15`  
**Stage:** Stage 0 — Product Reset, Original Hybrid UI Design & Architecture Specification  
**Baseline Commit:** `36aec73c539595997f4ab4aa8ae48ca2c80fd551` (`main`)

---

## 1. Technical Risk Register & Concrete Mitigations

| ID | Risk Description | Likelihood | Impact | Concrete Architectural Mitigation |
| :-: | :--- | :---: | :---: | :--- |
| **R1** | **UI Regression During Removal of Dual Legacy Scripts (`web/editor.js` + `web/maya_workspace.js`)** — Replacing `6,874` LOC of intertwined DOM code could accidentally drop an existing working capability (e.g., Light Linking UI, HDRI preset loader, Display Layer `T/R` toggle, or `.fe3dbin` multi-file upload). | Medium | High | Build a **1-to-1 Action & Feature Parity Checklist** directly from `EditorController::executeActionJson`'s action table (`64` actions) before retiring legacy JS; verify every single action in the new Desktop Playwright suite before deleting old modules. |
| **R2** | **Multi-Viewport GPU State & Scissor Leakage (`Stage 3`)** — Rendering 4 independent sub-viewports on one WebGL 2.0 / OpenGL 3.3 canvas can leak `glScissor`, depth-clear, or aspect ratios across quadrants, or quadruple shadow-map cost. | Medium | High | Compute `LightingSystem` PCF Shadow Atlas **once per frame** prior to the sub-panel loop; wrap sub-panel draws in an RAII `ScopedViewportScissor` guard on `IRenderDevice`; test quadrant isolation on `SoftwareRenderDevice` in CTest. |
| **R3** | **Non-Manifold Topology Edge Cases in Advanced Modeling (`Stage 4`)** — `Bevel`, `Multi-Cut`, and `Bridge` on concave n-gons or open boundaries can produce zero-area faces or flipped half-edges. | High | High | Operate on a transactional working copy of `EditableMesh`, run `validateAndRebuild()` before committing to `ModifyMeshCommand`, and reject degenerate cuts cleanly with a descriptive status toast rather than corrupting indices. |
| **R4** | **Bridge Serialization Bottleneck on Large Scenes (`2,000+` objects / `60 Hz` playback)** — Full-scene JSON serialization per frame causes GC pressure in WebAssembly/JS. | Medium | Medium | Introduce `DirtyDomainMask` in Stage 2 (`ENGINE_UI_ARCHITECTURE.md`) so `60 Hz` drags/scrubs serialize only the `~1 KB` `kInspector \| kTimeline \| kViewport` delta, plus virtualized `21px` rows in Outliner & Spreadsheet. |
| **R5** | **GLSL Dialect Divergence (`OpenGL 3.3 GLSL 330` vs. `WebGL 2.0 GLSL ES 3.00` + MSVC `C2026` 16 KB String Limit)** — Generated Shader Graphs or multi-pass shaders may fail on one backend or exceed MSVC raw string literal limits (`16,380` bytes). | Medium | High | Share a common C++ shader-chunk builder with header macros (`precision highp float;` injected only on ES 3.00), enforce `<= 12 KB` per C++ string literal chunk in CI, and compile every shader test against both `opengl33_device_tests` and `webgl2_device_tests`. |
| **R6** | **Native Desktop vs. Desktop Web Feature Drift** — Adding UI panels only to `web/` while leaving `apps/desktop/` as a bare GLFW window creates two unequal products. | Medium | High | Keep 100% of `WorkspaceManager` (`AreaNode` math), `ShortcutRegistry`, `ToolRegistry`, and `EditorController` in C++ `finaleffect_app`, and drive Native Desktop Studio mode from the shared `web/src/` UI bundle over the native C++ bridge while preserving zero-dependency `--headless` / GLFW viewport builds. |
| **R7** | **Schema Evolution Breaking `.fe3d` v1.0–v1.3 User Files** — Replacing 4-slot `WorkspaceState` with recursive `AreaNode` trees or linear TRS tracks with Bezier curves could reject existing `.fe3d` files. | Low | Critical | Keep `v1.0–v1.3` deserialization adapters permanently in `project_serializer.cpp` with dedicated golden-file regression tests in `tests/project_tests.cpp`. |

---

## 2. Safe Migration Plan: From Current Codebase (`36aec73`) to Original Hybrid Architecture

### Phase M0 — Baseline Lock & Preservation *(Completed in Stage 0)*
* `main` (`36aec73c539595997f4ab4aa8ae48ca2c80fd551`) remains untouched and serves as the verified rollback reference point (`19/19` CTests green, `8/8` CI jobs green).
* All Stage 0 work lives exclusively under `docs/product-redesign/`.

### Phase M1 — Additive Engine Refactoring First (Zero Broken Builds)
Before touching the Studio DOM in Stage 1–2:
1. Extract `EditorController` domain sub-controllers (`src/app/controllers/*.cpp`) while keeping `EditorController`'s public C++ header and JSON action names **100% backward-compatible** so all `19` existing CTests continue passing at every commit.
2. Extend `WorkspaceState` with `area_tree` while keeping a deterministic converter `upgradeLegacyFourSlotWorkspace(const WorkspaceState&)` so `tests/workspace_tests.cpp` and `.fe3d` `v1.2/v1.3` files load seamlessly.

### Phase M2 — Clean Modular Desktop UI Swap (`web/src/` → Bundled/ES Modules)
Instead of patching a third script onto `web/editor.js` (`4,422` LOC) + `web/maya_workspace.js` (`2,452` LOC):
1. **Stage 1** replaces `#app-home` and `web/studios.js` with modular `web/src/hub/` + `web/src/theme/tokens.css` (**Obsidian Slate + Prism Cyan + Solar Amber**), while keeping `#app-studio-3d` working smoothly.
2. **Stage 2** replaces `#app-studio-3d`, `web/editor.js`, and `web/maya_workspace.js` cleanly with modular `web/src/studio/` (`AreaTree`, `TopBar`, `ToolRail`, `ViewportArea`, `OutlinerArea`, `InspectorArea`, `TimelineArea`, `CommandPalette`, `CommandBar`), mapping all `64` existing C++ `EditorController` actions to the new Hybrid controls.

### Phase M3 — Audited Mobile Decommissioning Sequence (Stage 2)
Executed **only** during Stage 2 after verifying zero desktop callers (`DESKTOP_PLATFORM_REQUIREMENTS.md` Section 3):
1. Remove `<nav id="mobile-portrait-nav">`, `#btn-landscape-inspector-toggle`, and `.mp-drawer-close-btn` from `web/index.html`.
2. Remove `~555` lines of `body[data-layout-mode="portrait" | "landscape"]` from CSS and lock layout sizing to desktop workstation min-widths (`1366×768` reference minimum, graceful down to `1024×640` window resize).
3. Remove two-finger touch pinch/drawer code in JS and `fe3d_touch_tap` in `apps/web/main.cpp`, keeping standard W3C `mouse` and desktop tablet `pen` pointer events intact.
4. Update `web/e2e/capture-baseline.mjs` and `web/e2e/studio.spec.mjs` to test Desktop `1366×768`, `1920×1080`, and `2560×1440` (`1x` and `2x` HiDPI) exclusively.

---

## 3. Repository & Branch Safety Rules Across All Future Stages

1. **One Stage = One Dedicated Feature Branch + Full CI Verification + Reviewable PR** — Never combine multi-stage rewrites into unreviewable mega-commits, and never force-push over `main`.
2. **Continuous Test Expansion** — Every stage adds dedicated C++20 unit tests (`tests/*_tests.cpp`) and Desktop Chromium WebGL 2.0 E2E tests (`web/e2e/*.spec.mjs`) before the stage is marked complete.
3. **Stage Gate Approval** — Implementation of **Stage 1** does **not** begin until this **Stage 0 Specification Package** has been reviewed and approved by the project owner.
