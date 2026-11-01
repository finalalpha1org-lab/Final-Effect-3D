# Final Effect 3D — Staged Implementation Phases (`Stage 0` – `Stage 11`)

**Document ID:** `FE3D-SPEC-13`  
**Stage:** Stage 0 — Product Reset, Original Hybrid UI Design & Architecture Specification  
**Baseline:** `36aec73c539595997f4ab4aa8ae48ca2c80fd551` (`main`)

---

## Stage 0 — Product Reset, Audit & Architecture Specification *(Current Stage)*

* **Objectives**: Audit the entire 58,165-LOC repository, establish the Original Hybrid product identity, lock Desktop-only platform scope, and author the 15-document architectural blueprint in `docs/product-redesign/` without modifying existing code.
* **Dependencies**: Merged `main` baseline (`36aec73`).
* **Implementation Tasks**: Complete 10-point baseline audit, run `19/19` CTests, author `FE3D-SPEC-01`..`15`, open review PR to `main`.
* **Required Engine Work**: None (read-only audit).
* **Required UI Work**: None (specification & wireframes only).
* **Tests**: Existing `19/19` CTests & `8/8` CI jobs verified green.
* **Acceptance Criteria**: All 15 documents consistent, repository-grounded, committed on feature branch, PR open for user review and approval before Stage 1 begins.
* **Risks**: Divergent assumptions if existing code is not inspected first (mitigated via full symbol & line-count audit).
* **Estimated Complexity**: **Medium** (Architectural & UX Specification).

---

## Stage 1 — New Desktop Application Shell, Design System Tokens & Home / Project Hub

* **Objectives**: Establish the **Obsidian Slate + Prism Cyan + Solar Amber** CSS/UI token foundation, build the new Desktop **Home / Project Hub**, implement C++ `AppPreferences` & Autosave/Recovery foundations, and update E2E desktop resolution harnesses (`1366×768`, `1920×1080`, `2560×1440`).
* **Dependencies**: Approved Stage 0 Specification.
* **Implementation Tasks**:
  1. Create `AppPreferences` model + JSON serializer (`NavigationProfile`, `ui_scale`, `autosave_interval_sec`, `default_author`, `units`).
  2. Implement rolling autosave snapshot manager & unclean-shutdown recovery detector.
  3. Build modular `web/src/theme/` Design System tokens & core components (`Button`, `NumericScrub`, `Modal`, `SearchField`, `Tooltip`).
  4. Replace `#app-home` (`web/studios.js` 6-studio rail) with the unified **Home / Project Hub** (`Projects & Start`, `Templates`, `Recovery & Autosave`, `Asset Libraries`, `Learning & Docs`, `Preferences`), including instant project search, pinning, File System Access API direct `.fe3d` save/open, and honest empty/error states.
* **Required Engine Work**: `core/app_preferences.hpp/.cpp`, atomic file write (`saveToFile` temp-swap) in `ProjectSerializer` / `LocalFileSystem`, `ProjectHubController` sub-module.
* **Required UI Work**: New `#fe3d-project-hub` view, boot splash refinement, `Preferences (Ctrl+,)` modal.
* **Tests**: New `finaleffect_preferences_recovery_tests` CTest + Playwright Desktop Hub E2E suite (template creation, `.fe3d` open/corrupt error, search/pin recents, recovery banner).
* **Acceptance Criteria**: Zero fake project cards; `< 5 ms` search filter; real WebGL thumbnails; zero regressions in Studio launch.
* **Risks**: Transitioning `IndexedDB` recent-project schema from `v1` to `v2` (mitigate via automatic non-destructive reader for `v1` records).
* **Estimated Complexity**: **Medium**.

---

## Stage 2 — New Main Studio Hybrid UI, Recursive Area Framework & Mobile Decommissioning

* **Objectives**: Replace the dual-script legacy Studio DOM (`web/editor.js` + `web/maya_workspace.js`) with the modular **Original Hybrid Main 3D Studio**, introduce the recursive **Studio Area Split Tree**, unify **Quick Channels + Deep Inspector**, add the **`Ctrl+K` Command Palette**, decompose `EditorController` into domain sub-controllers with `DirtyDomainMask`, and cleanly remove audited mobile/touch-drawer code.
* **Dependencies**: Stage 1 (Design System & App Shell).
* **Implementation Tasks**:
  1. Upgrade `WorkspaceState` & `WorkspaceManager` in C++ from 4 fixed slots to recursive `AreaNode` splits + tabbed stacks + floating windows (with automatic `v1.2/v1.3` workspace importer).
  2. Split `editor_controller*.cpp` into domain sub-controllers and add `DirtyDomainMask` delta state queries.
  3. Build the Two-Tier `56px` Top Bar (`8` persistent menus, Workspace Switcher pills, Context Action & Snapping strip, Quick Shelf), Left Tool Rail (`44px`), virtualized Outliner + Display Layers, Unified **Quick Channels + Deep Inspector** (MMB scrub, safe math expressions, channel locks, live PBR preview), **`Ctrl+K` Command Palette**, and `24px` Status + `FE3D` Command Bar.
  4. Audit-verified removal of mobile portrait/landscape CSS, `#mobile-portrait-nav`, and `fe3d_touch_tap`.
  5. Wire Native Desktop Studio UI host bridge so `finaleffect_desktop` shares the same workspace & command contract.
* **Required Engine Work**: Recursive `AreaLayout` in `WorkspaceManager` (`.fe3d` v1.4 workspace extension with v1.3 compatibility), `EditorController` decomposition, `DirtyDomainMask`, safe arithmetic expression evaluator.
* **Required UI Work**: Modular `web/src/` ES module Studio shell (`AreaTreeRenderer`, `OutlinerEditor`, `InspectorEditor`, `TimelineEditor`, `ConsoleEditor`, `CommandPalette`).
* **Tests**: `finaleffect_area_workspace_tests` CTest + full Desktop Playwright E2E suite across `1366×768`, `1920×1080`, and `2560×1440` (`>= 68%` viewport area at `1080p`, zero horizontal overflow, full feature parity with Stage 5.4 + Maya Stage 1 capabilities).
* **Acceptance Criteria**: Single clean UI codebase; zero duplicate JS scene state; all existing C++ features accessible and undoable; mobile legacy code cleanly retired.
* **Risks**: Highest UI refactoring surface area (mitigated by keeping C++ `Scene`/`Renderer` untouched during Stage 2 and porting E2E suites systematically).
* **Estimated Complexity**: **High**.

---

## Stage 3 — Multi-Viewport (`Quad 4-View`), Picking, Marquee & Interaction Architecture

* **Objectives**: Deliver **True Simultaneous 4-View Quad & 2-Split Viewports** in C++ `Renderer` (`glViewport` + `glScissor`), **2D Box Marquee Selection** (Objects & Components, occluded vs. X-Ray), **`FE3D Hybrid` Input Profile**, **8-Direction Radial Compass**, **Outliner Drag-and-Drop Reparenting**, **Asset Drag-to-Viewport Placement**, and **In-Viewport Operator Adjust HUD** (`amendLastCommand`).
* **Dependencies**: Stage 2 (Hybrid Area Shell).
* **Implementation Tasks**:
  1. Refactor `StudioSession` to own `std::array<ViewportSubPanel, 4>` with independent `OrbitCameraController`, `ProjectionType`, and `ViewportDisplayOptions`.
  2. Add `IRenderDevice::setViewportAndScissor` to `OpenGL33RenderDevice`, `WebGL2RenderDevice`, and `SoftwareRenderDevice`; render shared PCF Shadow Atlas once per frame across all sub-panels.
  3. Implement C++ `boxSelectObjects` & `boxSelectComponents` (with depth/ray occlusion check when X-Ray is off).
  4. Implement `CommandStack::amendTopCommand` + bottom-left **Operator Adjust HUD** and Outliner drag-reparenting.
  5. Add on-demand viewport sleep when scene & camera are stationary.
* **Required Engine Work**: Multi-viewport `StudioSession` + `Renderer` scissor pass, frustum/NDC marquee selection in `picking.cpp` & `EditableMesh`, `amendTopCommand`.
* **Required UI Work**: Sub-panel active border & header controls, `Space`/`Ctrl+Space` sub-panel maximize, 2D marquee overlay rect, 8-Direction Radial Compass gesture flick, Operator Adjust HUD, Outliner drag-drop.
* **Tests**: `finaleffect_multiview_marquee_tests` CTest (verifying 4 distinct camera quadrants in `SoftwareRenderDevice` + marquee occlusion) + Playwright Quad-View & Marquee E2E tests.
* **Acceptance Criteria**: Live simultaneous `Persp + Top + Front + Right` rendering at `60 FPS`; accurate occluded vs. X-Ray box selection; instant sub-panel maximize/restore.
* **Risks**: Framebuffer coordinate Y-flip (`top-left` CSS vs `bottom-left` GL scissor) on HiDPI displays (mitigated by unit-tested coordinate mapper + pixel-quadrant color tests).
* **Estimated Complexity**: **High**.

---

## Stage 4 — Complete Polygon Modeling & Non-Destructive Modifier Foundation

* **Objectives**: Transform Final Effect 3D into a capable polygon modeler with **Component Rotate/Scale Gizmos**, **Normal/Component Space**, **Edge/Face Loop Selection (`Alt+Click`)**, **`Bevel`**, **`Bridge`**, **`Insert Edge Loop`**, **`Multi-Cut`**, **`Merge / Target Weld`**, **`Combine` & `Separate`**, **Core `ModifierStack` (`Mirror`, `Subdivision`, `Solidify`)**, and **Wavefront `.obj` Import/Export**.
* **Dependencies**: Stage 3 (Marquee, Component Picking, Operator Adjust HUD).
* **Implementation Tasks**:
  1. Extend `TransformGizmo` in Component mode to support `Rotate`, `Scale`, and `Normal` orientation around selection centroid.
  2. Implement topology-validated C++ operators in `src/scene/modeling/`: `bevelEdges/Faces`, `bridgeEdgeLoops/Faces`, `insertEdgeLoop`, `multiCutFaceSplit`, `mergeVerticesByDistance`, `targetWeld`, `combineMeshes`, `separateSelectedFaces`, and `selectEdgeLoop`/`selectEdgeRing`.
  3. Introduce non-destructive `ModifierStack` (`Mirror`, `Catmull-Clark Subdivision`, `Solidify`) evaluated cleanly between base `EditableMesh` and GPU `MeshData`.
  4. Implement zero-dependency C++20 `.obj` + `.mtl` importer/exporter.
* **Required Engine Work**: `scene/modeling/*.cpp`, `scene/modifier_stack.cpp`, `project/obj_io.cpp`, `ModifyMeshCommand` integration with Operator Adjust HUD.
* **Required UI Work**: Modeling Tool Rail additions, Modifier Stack cards in `Geometry` Inspector tab, Face-Orientation overlay toggle, `.obj` File Import/Export dialogs.
* **Tests**: Exhaustive manifold invariants CTest suite (`finaleffect_modeling_ops_tests` + `finaleffect_modifier_tests` + `.obj` round-trip tests) + Playwright Modeling workflow E2E.
* **Acceptance Criteria**: Every modeling operator preserves manifold validity (or returns structured `Status` error without corrupting topology) and supports live parameter adjustment in the Operator HUD + full Undo/Redo.
* **Risks**: Edge-case n-gon bevel/multi-cut degeneracies (mitigated by strict pre-flight half-edge validation and epsilon clamping before mutating `EditableMesh`).
* **Estimated Complexity**: **Very High**.

---

## Stage 5 — Digital Sculpting (`MeshBVH`) & Procedural Geometry Nodes Core

* **Objectives**: Add high-poly real-time **Sculpting** (`MeshBVH` spatial acceleration, core brushes, XYZ symmetry, compressed undo strokes) and introduce the reusable **`finaleffect::nodes::NodeGraph`** kernel powering **Geometry Nodes** + **Attribute Spreadsheet**.
* **Dependencies**: Stage 4 (`EditableMesh` & `ModifierStack`).
* **Implementation Tasks**:
  1. Implement `math::MeshBVH` refittable bounding-volume hierarchy for sub-millisecond ray-surface & sphere-brush queries on `100k–500k` triangle meshes, plus localized vertex-normal updates & `glBufferSubData` partial streaming.
  2. Implement `SculptSystem` brushes (`Draw`, `Clay Strips`, `Smooth`, `Grab`, `Inflate`, `Crease`, `Flatten`, `Pinch`, `Mask`), radial `X/Y/Z` symmetry, and delta-compressed `SculptStrokeCommand`.
  3. Build `finaleffect::nodes::NodeGraph` DAG engine + `GeometryGraphEvaluator` (`Grid`, `Cube`, `Subdivide`, `Transform`, `Distribute Points`, `Instance on Points`, `Set Position`, `Noise/Math`) integrated as a `GeometryNodesModifier`.
  4. Build the reusable UI `NodeCanvas` editor + `Geometry Spreadsheet` editor.
* **Required Engine Work**: `math/mesh_bvh.cpp`, `scene/sculpt_system.cpp`, `nodes/node_graph.cpp`, `nodes/geometry_nodes.cpp`.
* **Required UI Work**: `Sculpting` workspace (Brush rail, `F`/`Shift+F` radial cursor ring, tablet pressure wiring) & `Geometry Nodes` workspace (`NodeCanvas` + `Spreadsheet`).
* **Tests**: `finaleffect_sculpt_bvh_tests`, `finaleffect_geometry_nodes_tests`, and Playwright Sculpt/GeoNodes E2E.
* **Acceptance Criteria**: `60 FPS` brush strokes on `100k+` vertices; exact delta undo/redo; deterministic DAG evaluation with cycle rejection.
* **Risks**: Full-mesh GPU re-upload stalls on large sculpt meshes (mitigated via dirty vertex index spans & `glBufferSubData` range updates).
* **Estimated Complexity**: **Very High**.

---

## Stage 6 — 2D UV Editing, Shader Node Graph, Image Textures & glTF 2.0

* **Objectives**: Deliver the interactive **2D UV Canvas Editor** + C++ unwrapping algorithms, **Shader Node Graph** compiling directly to GLSL 330 / GLSL ES 3.00, PBR image texture map slots (`Albedo`, `Normal`, `ORM`, `Emissive`), persistent **Asset Library**, and **glTF 2.0 (`.gltf`/`.glb`)** interchange.
* **Dependencies**: Stage 5 (reuses `NodeCanvas` & `NodeGraph` DAG!).
* **Implementation Tasks**:
  1. Add `seam_edges` & `pinned_uvs` to `EditableMesh`; implement C++ `uv::Unwrapper` (Planar, Box, Cylindrical, Spherical, and LSCM Conformal Unwrap) + `packUvIslands`.
  2. Build the 2D `UV Editor` Studio Area (UDIM/0..1 grid, UV island pick/marquee/transform, UV Sync Selection, Stretch & Checkerboard overlays).
  3. Implement C++ `ShaderGraphCompiler` emitting validated GLSL fragment code into `OpenGL33RenderDevice` & `WebGL2RenderDevice`, plus PNG/JPG 2D texture uploads (`IRenderDevice::createTexture2D`).
  4. Implement glTF 2.0 (`.gltf`/`.glb`) import/export and cross-project Asset Browser drag-and-drop.
* **Required Engine Work**: `scene/uv_unwrapper.cpp`, `rendering/shader_graph_compiler.cpp`, `IRenderDevice` 2D material texture support, `project/gltf_io.cpp`.
* **Required UI Work**: `UV Editing` workspace, `Shader Node Graph` in `Shading` workspace, texture slot pickers, Asset Library manager.
* **Tests**: `finaleffect_uv_tests`, `finaleffect_shader_graph_tests`, `finaleffect_gltf_tests`, Playwright UV & Shading E2E.
* **Acceptance Criteria**: Distortion-checked UV unwraps; live procedural/texture node compilation across both GL 3.3 and WebGL 2.0 without shader crashes; lossless PBR `.glb` round-trip.
* **Risks**: GLSL 330 vs. GLSL ES 3.00 precision/syntax differences in generated shader graphs (mitigated by backend-neutral IR AST emitter tested on both devices).
* **Estimated Complexity**: **Very High**.

---

## Stage 7 — Curve Animation (`Dope Sheet` + `Graph Editor`) & Skeletal Rigging (`IK` + `Skinning`)

* **Objectives**: Upgrade linear TRS tracks to full **Bezier F-Curve Animation** (`Dope Sheet` + `Graph Editor`) and implement **Skeletal Armature Rigging**, **Linear Blend Skinning**, **Weight Painting**, and **2-Bone / FABRIK IK**.
* **Dependencies**: Stage 4 (`EditableMesh`) & Stage 5 (`MeshBVH` for weight painting).
* **Implementation Tasks**:
  1. Replace fixed TRS linear keyframes with channel-addressed `AnimationCurve` (`Constant`, `Linear`, `Bezier` with `Auto-Clamped`/`Free`/`Vector` tangents), maintaining automatic migration for `.fe3d` v1.3 tracks.
  2. Build the `Dope Sheet` and interactive `F-Curve Graph Editor` Studio Areas + 3D Motion Paths.
  3. Implement `ArmatureComponent` (joint hierarchy, bind pose, pose transforms), normalized 4-bone-per-vertex `SkinCluster` (GPU vertex skinning + CPU fallback), interactive Weight Paint brush, Constraints (`Parent`, `Track/Aim`, `Copy Transform`), and IK Solver.
* **Required Engine Work**: `animation/curve_evaluator.cpp`, `animation/armature_skinning.cpp`, `animation/ik_solver.cpp`, vertex shader skinning path.
* **Required UI Work**: `Animation` (`Dope Sheet` + `Graph Editor`) & `Rigging` (`Joint tools` + `Weight Paint` heatmap) workspaces.
* **Tests**: `finaleffect_animation_curve_tests`, `finaleffect_rigging_ik_tests`, Playwright Animation & Rigging E2E.
* **Acceptance Criteria**: Smooth overshoot-free `Auto-Clamped` Bezier tangents; normalized skin weights (`sum == 1.0`); real-time `60 FPS` rigged character playback.
* **Risks**: Gimbal lock & quaternion vs. Euler curve evaluation (mitigated by explicit per-object `RotationOrder` + Euler F-curves matching Maya/Blender animator expectations).
* **Estimated Complexity**: **Very High**.

---

## Stage 8 — Production Lighting, Progressive Rendering, Render View & AOV Passes

* **Objectives**: Build the dedicated **Render View Editor** (`A/B` snapshot wipe, pixel RGB/EV inspector, render region), **Progressive Accumulation Supersampling / TAA (`1..64` samples)**, **AOV Render Passes** (`Combined`, `Albedo`, `Normal`, `Roughness`, `Depth`, `Shadows`, `Object ID`), **SSAO**, **ACES/AgX-style Tone Mapping**, and **Animation Frame Sequence Rendering**.
* **Dependencies**: Stage 6 (Textures/Shaders) & Stage 7 (Animation).
* **Implementation Tasks**:
  1. Add jittered sub-pixel projection accumulation FBO (`RGBA16F`) + AOV G-buffer/pass output in `Renderer` (`OpenGL33` & `WebGL2` `EXT_color_buffer_float` with `RGBA8` fallback).
  2. Add Screen-Space Ambient Occlusion (SSAO) & ACES Filmic / Neutral tone-mapping operators.
  3. Build the `Render View` Studio Area (pan/zoom, A/B slot slider, channel/AOV selector, PNG & sequence export).
* **Required Engine Work**: `rendering/progressive_renderer.cpp`, `rendering/post_tonemap_ssao.cpp`, PNG encoder (`project/png_writer.cpp`).
* **Required UI Work**: `Rendering` workspace & `Render View` editor.
* **Tests**: `finaleffect_render_passes_tests` + Playwright Render View & AOV E2E.
* **Acceptance Criteria**: Clean anti-aliased multi-sample convergence, accurate AOV pass readouts, cancellable sequence render.
* **Risks**: `RGBA16F` FBO extension availability on software WebGL2 CI runners (mitigated via automatic runtime capability query & `RGBA8` fallback).
* **Estimated Complexity**: **High**.

---

## Stage 9 — Rigid Body Physics, Cloth, Force Fields & Particle Systems

* **Objectives**: Implement deterministic C++20 **Rigid Body Dynamics**, **Collision Shapes**, **Force Fields**, **Particle Emitters**, **PBD Cloth**, and **Timeline `SimulationCache` Baking**.
* **Dependencies**: Stage 7 (Timeline) & Stage 4 (`EditableMesh` colliders).
* **Implementation Tasks**:
  1. Implement impulse-based Rigid Body solver (`Box`, `Sphere`, `Capsule`, `Convex Hull`, static `Mesh` colliders) + `Hinge`/`Point`/`Fixed` constraints.
  2. Implement instanced `ParticleSystem` + Position-Based Dynamics (`PBD`) cloth solver + `Wind`/`Turbulence`/`Vortex` fields.
  3. Build frame-indexed `SimulationCache` (live forward playback + deterministic scrubbing once baked + `Bake to Keyframes`).
* **Required Engine Work**: `physics/physics_world.cpp`, `physics/pbd_cloth.cpp`, `physics/particle_system.cpp`, `physics/simulation_cache.cpp`.
* **Required UI Work**: `Physics` workspace, collider/force wireframe overlays, Inspector Physics cards, Timeline cache bar.
* **Tests**: `finaleffect_physics_determinism_tests` + Playwright Physics simulation E2E.
* **Acceptance Criteria**: Bit-reproducible simulation steps for fixed timestep (`1/60s` or substeps); zero frame hitching when scrubbing baked cache.
* **Risks**: Tunneling on fast rigid bodies (mitigated via configurable substeps `1..16` and swept sphere/box bounds).
* **Estimated Complexity**: **High**.

---

## Stage 10 — 2D Node Compositor & Multi-Track Video Sequence Editor (`VSE`)

* **Objectives**: Complete the post-production pipeline with the **2D Node Compositor** (operating directly on Stage 8 `Combined` + `AOV` buffers) and **Multi-Track Video Sequencer** (assembling live 3D Scene Camera strips, rendered image sequences, titles, and crossfades).
* **Dependencies**: Stage 5 (`NodeGraph` & `NodeCanvas`) and Stage 8 (`Render View` & AOV buffers).
* **Implementation Tasks**:
  1. Implement `compositor::CompositorGraph` GPU/CPU image nodes (`Render Layer Input`, `Exposure`, `RGB Curves`, `Lift/Gamma/Gain`, `Bloom/Glare`, `Gaussian Blur`, `Depth Mist`, `Vignette`, `Alpha Over`, `Composite/Viewer Output`).
  2. Implement `sequencer::SequenceTimeline` (multi-channel strips: `SceneCameraStrip`, `ImageSequenceStrip`, `ColorStrip`, `TransitionCrossfade`, Razor split, trim, slip).
  3. Build `Compositing` & `Video Editing` Studio Areas.
* **Required Engine Work**: `compositor/*.cpp`, `sequencer/*.cpp`.
* **Required UI Work**: `Compositing` backdrop + node editor & `Video Editing` multi-track timeline + program monitor.
* **Tests**: `finaleffect_compositor_tests`, `finaleffect_sequencer_tests`, Playwright Compositing & VSE E2E.
* **Acceptance Criteria**: Real-time post-FX recompositing on parameter tweak; frame-accurate camera cut assembly in Sequencer.
* **Risks**: Memory usage of long uncompressed image sequences (mitigated via LRU frame ring-cache).
* **Estimated Complexity**: **High**.

---

## Stage 11 — System Integration, Performance Optimization, Packaging & v1.0 Release Readiness

* **Objectives**: End-to-end profiling across `100k+`-polygon scenes, Native Desktop installer/archive packaging (`Linux AppImage/tar.gz`, `Windows zip/installer`, `macOS .app`), Command Macro Recorder, complete user documentation, and `v1.0.0` stabilization.
* **Dependencies**: Stages 1–10.
* **Implementation Tasks**:
  1. Frustum culling, draw-call batching/instancing, Outliner/Spreadsheet UI virtualization audit, WASM `-msimd128` optimization.
  2. Macro Recorder (saves sequences of structured `FE3D` commands as custom Shelf tools).
  3. Cross-platform release packaging workflow & benchmark regression gate in GitHub Actions CI.
* **Required Engine Work**: `Renderer` frustum culling & instanced draw path, memory profiler diagnostics.
* **Required UI Work**: Performance Profiler HUD, Macro Recorder modal, final keyboard navigation & accessibility polish.
* **Tests**: Full stress/soak suite (`1,000` objects, `250k` triangles, `500` undo/redo cycles, `.fe3d` v1.0→v1.5 compatibility matrix).
* **Acceptance Criteria**: Zero memory leaks, zero compiler warnings (`-Werror`/`/WX`), `60 FPS` interactive target met across all 13 editors, `100%` CTest & Desktop E2E pass.
* **Risks**: Platform packaging quirks across Linux/Windows/macOS (mitigated via automated GitHub Actions release matrix).
* **Estimated Complexity**: **Medium–High**.
