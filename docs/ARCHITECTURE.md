# Final Effect 3D — Stage 1–4 Architecture (`v0.4.0`)

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
finaleffect::core       (Version 0.4.0, Error/Result, Logger, ResourceStats, Engine, IApplication)
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

## 5. Project Format (`.fe3d` Schema `1.2`) & Binary Asset Container (`.fe3dbin` `1.0`)

* **Schema Version**: `{ "major": 1, "minor": 2 }` while maintaining full backward compatibility when loading `.fe3d` `1.0` and `1.1` files and `.fe3dbin` `1.0` binary geometry bundles.
* **Stage 4 Persisted Data**: Full `materials` table, per-object `material_id`, polygon `editable_mesh` topology (`positions`, n-gon `faces`, corner `uvs`/`colors`, per-face `material_id`), `editor_state` (`edit_mode`, `gizmo_tool`, `gizmo_space`, `selected_object_id`, `active_material_id`, `mesh_selection`), and `workspace` layout state.
* **Lossless Forward/Unknown Field Preservation**: Unknown top-level, scene-level, object-level, mesh-level, and material-level JSON properties are captured in `ProjectDocument::preserved_fields` (`ProjectPreservedFields`) alongside structured `load_warnings`, ensuring future or external extensions are never silently discarded during load/save round-trips.

---

## 6. Planned Repository Modules (Stage 5+)

* `platform/ios/`: Objective-C++ / Swift `UIViewController` + `CAMetalLayer` host integration.
* `platform/android/`: Android NDK `GameActivity` / `ANativeWindow` host integration.
