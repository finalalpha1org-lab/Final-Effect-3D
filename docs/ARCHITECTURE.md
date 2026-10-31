# Final Effect 3D — Stage 1–5 Architecture (`v0.5.0`)

## 1. Architectural Principles

Final Effect 3D is architected as a modular C++20 engine core surrounded by thin, target-specific platform and rendering adapters:

1. **Strict Engine vs. Application Separation**: `finaleffect::core::Engine` manages runtime state transitions, frame timing, file access, paths, and logging. Domain logic (`finaleffect::app::StudioSession`) implements `core::IApplication` and never embeds platform lifecycle boilerplate.
2. **Zero Global Mutable State**: There are no singleton managers or global loggers. `Engine` explicitly owns its `Logger`, `IFileSystem`, and `IClock` instances via RAII.
3. **API-Clean Portable Core**: Graphics API and DOM/windowing headers never appear in `core`, `math`, `scene`, `project`, `platform`, `app`, or portable `rendering`.
4. **CMake-Driven Platform Isolation**: OS-specific path logic (`platform/linux/`, `platform/windows/`, `platform/macos/`) and host entry points (`apps/desktop/`, `apps/web/`) are selected by CMake rather than scattering `#ifdef` conditionals throughout engine code.
5. **Deterministic Error Handling**: Fallible operations return `core::Result<T>` or `core::Status` (`Result<void>`) carrying structured `ErrorCode`, human-readable messages, and diagnostic context.

---

## 2. Module Dependency Graph

```text
finaleffect::platform   (IFileSystem, IClock, AppPaths, IWindow, InputEvent)
        │
        ▼
finaleffect::core       (Version 0.5.0, Error/Result, Logger, ResourceStats, Engine, IApplication)
        │
        ▼
finaleffect::math       (Vec2..4, Quat, Mat4, Transform, Ray, AABB, Intersections)
        │
        ▼
finaleffect::scene      (Scene, EditableMesh, PbrMaterial, Camera, Light, RaycastPicking, CommandStack)
       ╱ ╲
      ▼   ▼
project   rendering     (.fe3d v1.2 + .fe3dbin v1.0)    (Renderer, IRenderDevice, Software, MaterialPreview)
      ╲   ╱                                                 ├──► finaleffect::rendering_opengl33
       ▼ ▼                                                  └──► finaleffect::rendering_webgl2
finaleffect::app        (TransformGizmo, WorkspaceManager, OrbitCameraController, StudioSession, EditorController)
        │
        ├──────────────────────────────► finaleffect_desktop (+ optional finaleffect_window_glfw)
        └──────────────────────────────► finaleffect_web     (Emscripten HTML5 Canvas + WebGL 2.0)
```

---

## 3. Coordinate System & Mathematical Conventions

* **Handedness**: Right-handed 3D coordinate space (`+X × +Y = +Z`).
* **World Axes**: `+X` Right, `+Y` Up, `+Z` Outward (`-Z` is local/camera forward).
* **Matrix Layout**: Column-major `float[16]` (`element(row, col) = data[col * 4 + row]`), matching GLSL 330, GLSL ES 3.00, Metal Shading Language, and WGSL `mat4` layout.
* **Vector Multiplication**: Column-vector convention (`v' = M * v`), root-to-leaf hierarchy composition (`M_world = M_parent * M_local`).
* **Rotations & Angles**: Unit quaternions `(x, y, z, w)` with identity `(0, 0, 0, 1)`; all math APIs use **radians** (converted to/from XYZ Euler degrees only at Inspector boundaries).
* **Clip-Space Depth Abstraction**: `math::ClipSpaceDepth::NegativeOneToOne` (`[-1, 1]`, OpenGL 3.3, WebGL 2.0, Software) and `ZeroToOne` (`[0, 1]`, future Vulkan, Metal, WebGPU).

---

## 4. Stage 4 Professional Editing Architecture

### A. 3D Transform Gizmo System (`app::TransformGizmo`)

`TransformGizmo` (`include/finaleffect/app/transform_gizmo.hpp`) implements genuine 3D geometric manipulation without screen-space approximations:

* **Handles & Procedural 3D Meshes**: Generates 3D shafts + arrow cones + dual-axis planar quads for **Translate** (`X`, `Y`, `Z`, `XY`, `XZ`, `YZ`), 3D torus-tube rings for **Rotate** (`X`, `Y`, `Z`), and 3D shafts + box tips + uniform center cube for **Scale** (`X`, `Y`, `Z`, `Uniform`).
* **Constant Screen-Space Sizing**: `computeFrame` scales gizmo geometry proportionally to camera distance (`dist * tan(fov/2) * 0.30`) in Perspective projection and `orthographic_height * 0.16` in Orthographic projection.
* **Exact 3D Ray-Handle Picking & Drag Math**:
  * Axis shafts/tips use analytical closest-approach between the world `math::Ray` and finite 3D handle segments (`closestRaySegment`, including degenerate zero-length point projection for `ScaleUniform`), constrained onto a camera-facing drag plane containing the active axis.
  * Planar handles (`XY`/`XZ`/`YZ`) intersect directly with their 3D world/local plane.
  * Rotation handles (`RotateX`/`RotateY`/`RotateZ`) intersect the true 3D axis-perpendicular ring plane (with perpendicular fallback plane at near-grazing angles) and compute signed angular deltas via `atan2((v0 × v1)·axis, v0·v1)`.
* **Hierarchy-Correct World & Local Space**: World deltas convert cleanly through `M_parent^-1` into local object `Transform` updates (`SetTransformCommand` with drag coalescing) or local `EditableMesh` vertex translations (`ModifyMeshCommand`) when in Mesh Edit mode, with instant `Escape` cancellation restoring the pre-drag snapshot.
* **Depth-Cleared Overlay Pass**: Rendered after scene geometry via `IRenderDevice::beginOverlayPass()`, clearing only the depth buffer so gizmos and selected sub-element overlays remain visible through occluding geometry while preserving 3D handle self-occlusion.

### B. Polygon Mesh Editing Foundation (`scene::EditableMesh`)

`EditableMesh` (`include/finaleffect/scene/editable_mesh.hpp`) separates authoring polygon topology from triangulated GPU buffers:

* **Representation**: Shared `positions` (`Vec3`), polygon `faces` (`PolygonFace` with arbitrary vertex valence `>= 3`, per-corner `uvs`, `colors`, optional smooth normals, cached `face_normal`, and optional `material_id`), and canonical undirected `edges` (`Edge` with sorted endpoint indices and incident face count so `incident_faces == 1` identifies open boundary edges).
* **Explicit Modes**: `scene::EditMode::{Object, Vertex, Edge, Face}` with single/Shift-additive/clear viewport picking (`pickVertex`, `pickEdge`, `pickFace`) and highlighted 3D overlay markers rendered in `StudioSession`.
* **Topology-Validated Modeling Operations**:
  * **Move Vertices / Edges / Faces**: Translates unique affected vertices, recomputes normals and local `AABB`, and updates GPU buffers in place.
  * **Face Extrusion (`extrudeFaces`)**: Duplicates boundary loop vertices, stitches quad side walls with consistent outward winding, offsets extruded cap faces along their area-weighted normal, and selects the new cap faces.
  * **Delete Faces / Dissolve or Delete Edges & Vertices**: Rejects operations that would leave fewer than 1 valid polygon face or produce degenerate/non-manifold faces **before** mutating the mesh.
  * **Boundary-Loop Face Fill (`selectOpenBoundaryLoop` / `fillBoundaryLoop`)**: Traces ordered open boundary edges (`incident_faces == 1`) and constructs a new manifold polygon face with outward-consistent winding, rejecting non-boundary or self-intersecting selections cleanly.
* **Undo/Redo**: Every mesh mutation executes via `scene::ModifyMeshCommand` (with drag coalescing during interactive gizmo pulls and exact byte-footprint accounting in `CommandStack`).

### C. Metallic-Roughness PBR Material System & Live 3D Sphere Preview

* **Material Model (`scene::PbrMaterial`)**: Stable `MaterialId`, `name`, `base_color`, `metallic`, `roughness` (clamped `>= 0.045` to prevent GGX singularities), `ambient_occlusion`, `emissive_color`, `emissive_strength`, and `opacity`. Safe deletion (`Scene::removeMaterial` + `DeleteMaterialCommand`) automatically clears dangling object and per-face references and restores them on Undo.
* **Consistent Multi-Backend PBR Shading**:
  * **OpenGL 3.3 Core (GLSL 330)** & **WebGL 2.0 (GLSL ES 3.00)**: Energy-conserving metallic-roughness Cook-Torrance microfacet BRDF with Trowbridge-Reitz GGX normal distribution, Smith Schlick-GGX geometry masking/shadowing, Schlick Fresnel (`F0 = mix(0.04, albedo, metallic)`), hemisphere ambient occlusion, emissive radiance, Reinhard HDR tone mapping, and sRGB gamma output.
  * **CPU `SoftwareRenderDevice`**: Evaluates the same Cook-Torrance GGX + Schlick Fresnel + Reinhard + sRGB pipeline per fragment on the CPU (documented single key+fill directional light + hemisphere irradiance approximation without IBL cubemaps).
  * **Multi-Material Submeshes & Transparency**: `EditableMesh::triangulate()` groups triangles by `material_id` into `SubMeshRange` slices. `Renderer` draws opaque submeshes front-to-back with depth writes enabled, then sorts translucent (`opacity < 0.999`) submeshes back-to-front with alpha blending.
* **Live 3D Material Preview (`rendering::MaterialPreviewRenderer`)**: Owns a dedicated offscreen `SoftwareRenderDevice` + procedural UV sphere + 3-point studio lighting rig, re-rendering live RGBA8 pixels whenever the active material changes and blitting directly into `<canvas id="material-preview-canvas">` via WebAssembly shared memory.

### D. Modular Workspace Architecture (`app::WorkspaceManager`)

* Manages panel descriptors (`Viewport3D`, `SceneOutliner`, `Properties`, `MaterialEditor`, `AssetBrowser`, `Timeline`, `Console`), dock slots (`Left`, `Right`, `Bottom`, `Center`), proportional split ratios, and built-in presets (`Default Studio`, `Modeling`, `Shading`, `Animation Prep`).
* Enforces non-duplicating input routing (`routePointerDown`, `routeKeyEvent`) so active text fields and non-viewport panels never leak shortcuts or pointer drags into 3D viewport camera/gizmo controllers, while active viewport drags remain captured until pointer release.

### E. In-Place GPU Buffer Updates (`IRenderDevice::updateMesh`)

* Each `MeshAsset` carries a monotonically increasing `geometry_revision`. `Renderer` tracks uploaded `(revision, vertex_capacity, index_capacity)` per `MeshId`: when vertex/edge/face movement mutates an existing mesh without exceeding buffer capacity, `OpenGL33RenderDevice` and `WebGL2RenderDevice` stream updated vertices/indices in place via `glBufferSubData` without reallocating VAOs/VBOs/EBOs, incrementing `ResourceStats::gpu_buffer_update_count`.

---

## 4b. Autodesk Maya-Style Interface Architecture (Stage 1 Redesign)

### A. UI State Ownership

JavaScript never owns scene data. `web/editor.js` is a **pure renderer of the engine state snapshot** produced by `EditorController::serializeStateJson()` and every mutation is a JSON action dispatched through `fe3d_dispatch_action` into the C++ `EditorController`/`StudioSession`/`Scene`/`CommandStack` pipeline:

- Selection (`select_object`, `additive`), hierarchy (`group_selected`, `parent_selected`, `unparent_selected`, `reparent_object`), rename, lock (`set_object_lock`), channel locks (`set_channel_locks`), custom attributes (`add/update/delete_custom_attribute`).
- Toolbox & Menu Sets (`set_active_tool`, `activate_last_tool`, `set_menu_set`), selection masks (`set_selection_masks`), snapping (`set_snap_settings`), display modes (`set_viewport_display`), isolate select and X-Ray.
- Display Layers (`create/delete/rename/set_display_layer_state/assign_display_layer/select_display_layer`) mapped to `StudioSession::DisplayLayer` and applied by `renderer::ViewportDisplayOptions` (hidden / template wireframe / reference) without ever deleting geometry.
- Real keyframe animation (`timeline_set_frame`, `timeline_set_ranges`, `timeline_set_playing`, `timeline_step`, `timeline_jump_key`, `timeline_set_fps`, `timeline_set_loop_mode`, `timeline_set_auto_key`, `timeline_set_key`, `timeline_delete_key`) evaluated by `StudioSession::evaluateTimelineAtCurrentFrame()` (position lerp, quaternion slerp, scale lerp) directly onto `SceneObject::local_transform`.
- Safe Command Line (`execute_command_line`) implemented as a structured tokenizer + command table in `EditorController::executeCommandLine` — **no JavaScript evaluation, no shell, no file-system access**.

`EditorController::serializeStateJson()` additionally exposes `editor.menu_set`, `editor.active_tool`, `editor.selection_masks`, `editor.snap`, `viewport.shading_mode`, `viewport.xray_enabled`, `viewport.isolate_select`, per-object `locked`/`display_layer_id`/`is_selected`, `display_layers`, `timeline` and `command_history` so the Maya panels are fully engine-driven.

### B. Window Region Model

```
┌ Menu Set select │ File Edit Create Select Modify Display Windows [Mesh EditMesh ...] Help │ workspace tabs ┐
├ Status Line: masks │ template/reference │ snaps │ create/dup/del/frame/render │ command search │ toggles ┤
├ Shelf tabs (General … FX) + executable shelf items                                                          ┤
├ Tool Box │ Outliner + Tool Settings │ Viewport Panel Bar + WebGL 2.0 Viewport │ Channel Box + Layers ┤
│          │                         │  (HUD, nav cluster, 3D view gizmo, marking menu, Hotbox)  │ Attribute Editor ┤
├─────────────────────── Time Slider + Range Slider + bottom editors ──────────────────────────────────────────┤
├ Command Line (structured parser) │ Help Line + engine telemetry ────────────────────────────────────────────┤
```

Left/right/bottom docks (and the Channel Box vs Attribute Editor split) are resizable, collapsible, switchable between *stacked / Channel-Box-only / Attribute-Editor-only*, and can be undocked into floating windows. Shelf commands, custom shelves and hotkey overrides persist in `localStorage` through `FE3D_REGISTRY` and are completely independent of `.fe3d` scene files.

### C. Shortcut & Input Routing

- `platform::KeyCode` now covers `Q W E R T Y A S D F G H P X C V`, `Insert`, `1–7`, `F2–F6`, `F8–F12`, `Space`, `Delete`, `Backspace`, `Escape`, and modifiers.
- `StudioSession::processInputEvent` maps them to Maya semantics: `Q` Select, `W` Move, `E` Rotate, `R` Scale, `T` Show Manipulator, `Y` Last Tool, `D`/`Insert` Edit Pivot, `A` Frame All, `F` Frame Selected, `H` Hide, `X`/`V`/`C` grid/point/curve snapping, `Tab`/`F8` object↔component, `F9/F10/F11/F12` vertex/edge/face/UV, `1/2/3` display smoothness, `4/5/6/7` wireframe/shaded/textured/all-lights, and \(Ctrl+Z / Ctrl+Y\) Undo/Redo.
- Alt+LMB orbit, Alt+MMB pan, Alt+RMB dolly, MMB pan and RMB orbit preserve the Stage 5 viewport navigation contract; `Alt+RMB` zoom was added as a `DragMode::Zoom` path.
- `Space` is claimed for the **Hotbox** (the C++ key handler consults `window.__FE3D_IS_KEY_OVERRIDDEN__`), while the showcase turntable keeps its toolbar toggle and `timeline_toggle_play` hotkey. Keys are never delivered while an `<input>`, `<textarea>`, `<select>`, content-editable element, modal or non-Studio view has focus.
- The Hotkey Editor rewrites the shortcut map; remapped combinations are reported to the WebAssembly key handler so a key is never executed twice.

---

## 5. Project Format (`.fe3d` Schema `1.2`) & Binary Asset Container (`.fe3dbin` `1.0`)

* **Schema Version**: `{ "major": 1, "minor": 2 }` while maintaining full backward compatibility when loading `.fe3d` `1.0` and `1.1` files and `.fe3dbin` `1.0` binary geometry bundles.
* **Stage 4 Persisted Data**: Full `materials` table, per-object `material_id`, polygon `editable_mesh` topology (`positions`, n-gon `faces`, corner `uvs`/`colors`, per-face `material_id`), `editor_state` (`edit_mode`, `gizmo_tool`, `gizmo_space`, `selected_object_id`, `active_material_id`, `mesh_selection`), and `workspace` layout state.
* **Lossless Forward/Unknown Field Preservation**: Unknown top-level, scene-level, object-level, mesh-level, and material-level JSON properties are captured in `ProjectDocument::preserved_fields` (`ProjectPreservedFields`) alongside structured `load_warnings`, ensuring future or external extensions are never silently discarded during load/save round-trips.

---

## 6. Stage 5 Complete Creative Application & Multi-Device UI/UX Architecture (`v0.5.0`)

### A. Three-Level Application Flow

1. **Level 1 — Real Engine Initialization Splash Screen (`#app-splash`)**:
   * Driven strictly by actual Emscripten module loading (`Module.setStatus`) and C++20 initialization milestones exported from `apps/web/main.cpp` (`_fe3d_is_ready`, `_fe3d_get_boot_stage`, `_fe3d_get_boot_message`, `_fe3d_get_init_error`).
   * Never uses fake timers; if WebAssembly loading or WebGL 2.0 context creation fails, displays diagnostic details and blocks transition into broken screens.
2. **Level 2 — Main Home, Extensible 6-Studio Selector & Reusable Project Launcher (`#app-home`, `web/studios.js`)**:
   * **6 Creative Studios**: `3D Studio` (fully operational) alongside `Landscaping Studio`, `Character Studio`, `2D Studio`, `Sculpting Studio`, and `Shade Studio` (honestly presented as planned architectural slots with zero fake launch controls or dummy tools).
   * **Reusable Project Launcher**: Implemented for `3D Studio` (`ProjectTemplate::{StarterShowcase, MinimalCube, EmptyStage, ShaderBall}` via `EditorController::createNewProject`), real `.fe3d` / `.fe3dbin` VFS import with corruption diagnostics, and real Recent Projects history (`localStorage` metadata + `IndexedDB` `.fe3d` payload cache + genuine WebGL 2.0 canvas thumbnails; `Remove` deletes only the recent record, never user files).
3. **Level 3 — Professional Blender/Maya-Inspired 3D Studio Workspace (`#app-studio-3d`)**:
   * **5 Workspace Regions**: Top App Menubar + 12 Workspace Tabs + Contextual Mode Header, Left Collapsible Tool Shelf, Center Priority WebGL 2.0 3D Viewport, Right Stacked Scene Outliner + Vertical-Tabbed Properties/Material Inspector (with live 96×96 3D Cook-Torrance sphere preview), and Resizable/Collapsible Bottom Multi-Editor Area.
   * **12 Shared-Scene Workspaces**: `Layout`, `Modeling`, `Sculpting`, `Shading`, `Animation`, `Rendering`, `Compositing`, `VFX`, `Geometry Nodes`, `UV Editing`, `2D Animation`, and `Video Editing` (`WorkspaceManager::applyPreset`). Every workspace is a reusable editor arrangement over the **exact same live C++ `Scene`, selection, materials, cameras, lights, and `CommandStack`**. Unfinished node/UV/compositor/sequencer editors are clearly labeled as structural foundations rather than fake tools.
   * **Independent Layout Persistence**: Custom dock split ratios (`set_dock_ratios`) and saved layouts (`fe3d.saved_workspace_layout.v2`) persist separately from `.fe3d` scene objects and materials.

### B. Unified Desktop, Mobile Landscape, Mobile Portrait & Safari Desktop Mode Strategy

* **Single Unified Application**: `#fe3d-canvas` and the C++20 `StudioSession` remain mounted across all views and layout modes without duplicating DOM state or resetting WebGL 2.0 resources.
* **Viewport-Dimension Responsive Modes (Never Gated on `pointer: coarse`)**:
  * **Desktop Mode (`width >= 960px`)**: Full multi-panel desktop studio on `1366×768`, `1920×1080`, Tablet Landscape `1024×768`, AND iPhone Safari **Request Desktop Website** (`~980px` width in both portrait and landscape, even when `hasTouch` is true).
  * **Mobile Landscape Pro Mode (`width < 960px && width > height`)**: Preferred compact horizontal creative workspace maximizing the 3D Viewport with quick toggleable side Inspector/Outliner panels.
  * **Mobile Portrait Mode (`width < 960px && height >= width`)**: Viewport-first vertical layout keeping the live 3D Viewport visible above accessible bottom editor drawers (`Viewport`, `Outliner`, `Inspector`, `Materials`, `Assets`, `Timeline`, `Console`).
* **Conflict-Free Pointer & Multi-Touch Input**: W3C Pointer Events route touch gizmo handle hits (`_fe3d_hit_test_gizmo` -> `_fe3d_pointer_event`) directly to `TransformGizmo`, `< 8px` single-finger taps to `_fe3d_touch_tap` (with optional `+Multi` additive toggle), single-finger drags to `_fe3d_camera_orbit`/`pan`/`zoom`, and two-finger gestures to simultaneous pinch-zoom and midpoint pan.

---

## 7. Planned Repository Modules (Stage 6+)

* `platform/ios/`: Objective-C++ / Swift `UIViewController` + `CAMetalLayer` host integration.
* `platform/android/`: Android NDK `GameActivity` / `ANativeWindow` host integration.
