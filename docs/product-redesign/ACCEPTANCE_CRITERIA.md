# Final Effect 3D — Objective Acceptance Criteria & Verification Gates

**Document ID:** `FE3D-SPEC-14`  
**Stage:** Stage 0 — Product Reset, Original Hybrid UI Design & Architecture Specification  
**Enforcement:** Automated C++20 CTest Suites + GitHub Actions 8-Job Matrix + Desktop Chromium WebGL 2.0 Playwright E2E

---

## 1. Universal Non-Negotiable Quality Gates (Every Stage `0..11`)

No Pull Request in any stage may be merged to `main` unless **all 8** universal gates pass:

1. **Zero Compiler Warnings (`-Werror` / `/WX`)**: Compiles cleanly under C++20 on Linux GCC (Debug & Release), Linux Clang (Release), macOS AppleClang (Release), Windows MSVC (Debug & Release), and Emscripten `em++`.
2. **100% C++ Unit & Integration Test Pass**: Every existing CTest (`19` baseline suites + all newly added stage suites) passes deterministically in `--headless` software/null mode without requiring a physical GPU.
3. **Zero Fake / Decorative UI Rule**: Every visible menu item, toolbar button, shelf icon, radial wedge, inspector control, and shortcut executes real C++ engine state or persistent workspace state—verified by automated E2E state assertions (`window.__FE3D_STUDIO__.getState()`), never screenshot-only cosmetics.
4. **Authoritative C++ State Invariant**: Scene graph, mesh topology, materials, cameras, lights, selection, and undo/redo live exclusively in C++. Clearing JS UI memory and re-syncing from `EditorController` reproduces 100% of scene & selection state.
5. **100% Undo / Redo Completeness**: Every scene-, mesh-, material-, light-, layer-, modifier-, node-, or animation-mutating user action pushes a reversible `ICommand` onto `CommandStack`; `Ctrl+Z` followed by `Ctrl+Y` restores exact state within floating-point epsilon (`1e-5f`).
6. **Backward `.fe3d` Compatibility**: Existing `.fe3d` (`v1.0`–`v1.3`) and `.fe3dbin` (`v1.0`) test fixtures load with zero data loss and preserve unrecognized extension fields in `ProjectPreservedFields`.
7. **Desktop Layout & Viewport Geometry Gate (`1366×768`, `1920×1080`, `2560×1440`)**:
   * Zero horizontal document overflow (`documentElement.scrollWidth <= window.innerWidth`).
   * Zero bounding-box overlap between Top Chrome, Left Tool Rail, Studio Areas, Status Bar, and Viewport HUD widgets.
   * On `1920 × 1080` in default `Layout` / `Modeling` workspaces, the 3D Viewport Area occupies **`>= 68%` of total window area** (`>= 1,410,000` CSS px²).
8. **Legal & Repository Integrity**: `LICENSE` (GNU GPL v3.0), `NOTICE`, and `.gitignore` remain preserved; zero proprietary icons, fonts, or third-party assets with incompatible licenses are introduced.

---

## 2. Stage-by-Stage Functional Acceptance Criteria

### 2.1 Stage 0 Gate (Current Specification Phase)
* [x] Repository HEAD (`36aec73c539595997f4ab4aa8ae48ca2c80fd551`) and PR #6 ancestry verified.
* [x] Baseline `19/19` CTests executed and verified passing (`100%`).
* [x] All `15` required specification documents authored inside `docs/product-redesign/` with internal cross-consistency, real repository symbol citations, ASCII wireframes, and realistic staged sequencing.
* [x] Zero modifications made to `include/`, `src/`, `apps/`, `web/`, `tests/`, or `CMakeLists.txt`.
* [x] Feature branch pushed and Pull Request opened to `main` and **left open for user review**.

### 2.2 Stage 1 Gate (Application Shell & Home / Project Hub)
* Boot splash reflects real C++ milestones and halts cleanly on simulated WebGL failure.
* Home / Project Hub displays **zero fake recent projects** on fresh storage, filters 50+ recent items in `< 5 ms`, pins/unpins projects, creates real C++ projects from categorized templates, opens valid `.fe3d`/`.fe3dbin` files, surfaces structured errors on corrupted `.fe3d` files without crashing, and recovers simulated crash autosave snapshots.
* `Preferences (Ctrl+,)` persists Navigation Profile, UI Scale (`85%–150%`), and Autosave interval independently of `.fe3d` scene files.

### 2.3 Stage 2 Gate (Main Studio Hybrid UI & Area Framework)
* Two-Tier Top Chrome (`<= 58px` height), Left Tool Rail, Recursive **Studio Area Split Tree** (split horizontal/vertical, resize splitters, switch Editor Type in any area header, tear off floating window, dock back, `Ctrl+Space` maximize/restore), virtualized Outliner + Display Layers, Unified **Quick Channels + Deep Inspector** (MMB scrub, inline math `10/2`, per-channel lock, live 3D PBR preview sphere), `Ctrl+K` Command Palette, and safe `FE3D` Command Bar (`eval(...)` rejected with zero execution).
* Input Focus Guard verified: typing `q w e r s f x Delete Space` in any text/number/command input never changes tools, sets keyframes, opens popups, or deletes scene objects.
* All audited mobile-only DOM/CSS/JS rules removed cleanly; Desktop E2E passes at `1366×768`, `1920×1080`, and `2560×1440` (`deviceScaleFactor: 2`).

### 2.4 Stage 3 Gate (Quad 4-View Viewport, Marquee & Operator HUD)
* Quad 4-View renders 4 simultaneous live sub-panels (`Top`, `Persp`, `Front`, `Right`) with independent camera projections and shading modes (`Wireframe` ortho + `Lit PBR` persp) verified by per-quadrant framebuffer pixel readback.
* 2D Box Marquee selects multiple objects and mesh components accurately, excluding back-facing occluded vertices when X-Ray is off and including them when X-Ray is on.
* `FE3D Hybrid` navigation (`MMB` **and** `Alt+LMB/MMB/RMB`), 8-Direction Radial Compass, Outliner drag-and-drop reparenting, and bottom-left **Operator Adjust HUD** (`amendTopCommand`) pass automated E2E tests.

### 2.5 Stage 4 Gate (Polygon Modeling & Modifiers)
* `Bevel`, `Bridge`, `Insert Edge Loop`, `Multi-Cut`, `Merge/Weld`, `Combine`, and `Separate` pass C++ manifold validation (`validateAndRebuild() == ok`), live Operator HUD parameter amendment, and `Ctrl+Z`/`Ctrl+Y` round-trips.
* Component `Move`, `Rotate`, and `Scale` gizmos work in `World`, `Local`, and `Component Normal` orientations.
* Non-destructive `Mirror`, `Subdivision`, and `Solidify` modifiers evaluate in real time and serialize in `.fe3d` v1.4; `.obj` import/export round-trips geometry & UVs cleanly.

### 2.6 Stages 5 – 11 Gates (Summary Metrics)
* **Stage 5 (Sculpt & GeoNodes)**: `>= 60 FPS` sculpt dab updates on `100k`-vertex sphere via `MeshBVH` + `glBufferSubData`; deterministic Geometry Node DAG evaluation & Spreadsheet readout.
* **Stage 6 (UV & Shader Graph)**: Overlap-free LSCM/Projection UV unwrap + island pack; Shader Node Graph compiles and renders identically on `OpenGL 3.3` and `WebGL 2.0`; `.glb` round-trips PBR materials & meshes.
* **Stage 7 (Animation & Rigging)**: Bezier F-Curve tangent editing in Graph Editor matches C++ `AnimationCurve` evaluation; skinned character mesh deforms with normalized weights (`|sum - 1.0| < 1e-4`) and interactive 2-bone IK.
* **Stage 8 (Rendering)**: Progressive accumulation reduces edge aliasing monotonically across `1 -> 16` samples; all AOV passes (`Albedo`, `Normal`, `Depth`, `Shadows`) render non-empty, physically valid buffers in `Render View`.
* **Stage 9 (Physics)**: Identical initial state produces bit-identical rigid-body/particle positions at frame `120` across repeated runs; baked `SimulationCache` scrubs arbitrarily in `< 2 ms`.
* **Stage 10 (Compositor & VSE)**: Node Compositor bloom/grading updates live viewer; Video Sequencer cuts seamlessly between two 3D Scene Camera strips and exports frame sequence.
* **Stage 11 (Release v1.0)**: 500-step automated stress test exhibits zero memory growth leaks, zero console errors, and `100%` green multi-OS CI & Desktop E2E.
