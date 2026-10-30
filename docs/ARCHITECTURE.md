# Final Effect 3D — Stage 1 & Stage 2 Architecture

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
finaleffect::core       (Version, Error/Result, Logger, ResourceStats, Engine, IApplication)
        │
        ▼
finaleffect::math       (Vec2..4, Quat, Mat4, Transform, Ray, AABB, Intersections)
        │
        ▼
finaleffect::scene      (Scene, MeshAsset, Camera, Light, RaycastPicking, CommandStack)
       ╱ ╲
      ▼   ▼
project   rendering     (.fe3d JSON + .fe3dbin chunks)   (Renderer, IRenderDevice, Null, Software)
      ╲   ╱                                                 ├──► finaleffect::rendering_opengl33
       ▼ ▼                                                  └──► finaleffect::rendering_webgl2
finaleffect::app        (StarterScene, OrbitCameraController, StudioSession)
        │
        ├──────────────────────────────► finaleffect_desktop (+ optional finaleffect_window_glfw)
        └──────────────────────────────► finaleffect_web     (Emscripten HTML5 Canvas + WebGL 2.0)
```

---

## 3. Coordinate System & Mathematical Conventions

* **Handedness**: Right-handed 3D coordinate space (`+X × +Y = +Z`).
* **World Axes**: `+X` Right, `+Y` Up, `+Z` Outward (`-Z` is local/camera forward).
* **Matrix Layout**: Column-major `float[16]` (`element(row, col) = data[col * 4 + row]`), matching GLSL, GLSL ES, Metal Shading Language, and WGSL `mat4` layout.
* **Vector Multiplication**: Column-vector convention (`v' = M * v`), root-to-leaf hierarchy composition (`M_world = M_parent * M_local`).
* **Rotations & Angles**: Unit quaternions `(x, y, z, w)` with identity `(0, 0, 0, 1)`; all math APIs use **radians**.
* **Clip-Space Depth Abstraction**: `math::ClipSpaceDepth::NegativeOneToOne` (`[-1, 1]`, OpenGL 3.3, WebGL 2.0, Software) and `ZeroToOne` (`[0, 1]`, future Vulkan, Metal, WebGPU).

---

## 4. Interactive Viewport, Raycast Picking & Undo/Redo Command Stack

### Raycast Picking (`scene::raycastScene`)

1. **Screen-to-World Unprojection**: `OrbitCameraController::screenPointToWorldRay` converts top-left window pixel coordinates `(x, y)` analytically into world-space `math::Ray`s for both perspective and orthographic cameras.
2. **Broad-Phase AABB Culling**: Each `MeshAsset` caches its local-space `math::AABB`. `Scene::computeObjectWorldBounds` transforms local bounds into world space without corner heap allocations, and `intersectRayAabb` culls non-intersecting objects before triangle traversal.
3. **Narrow-Phase Local-Space Möller–Trumbore**: Rays are transformed into object local space via `M_world^-1` so meshes with thousands of triangles avoid per-vertex world transformations, returning exact world hit position, surface normal, distance, and triangle index.

### Deterministic Memory-Bounded Command System (`scene::CommandStack`)

All interactive `Scene` edits execute through reversible `ISceneCommand` transactions:

* **`SetTransformCommand`**: Captures pre/post `Transform` and coalesces consecutive drag updates on the same `ObjectId` when coalescing is enabled.
* **`CreateObjectCommand` / `DeleteObjectCommand`**: Preserves exact `ObjectId`s across undo/redo cycles using `Scene::createObjectWithId` and snapshots full subtrees, mesh/camera/light components, and active camera bindings.
* **`RenameObjectCommand` / `SetVisibilityCommand`**: Reversible object naming and viewport visibility toggles.
* **`SetCameraComponentCommand` / `SetDirectionalLightCommand`**: Reversible camera projection/clipping edits and coalescing directional light color/intensity edits.
* **`ReparentObjectCommand`**: Cycle-checked reversible hierarchy reparenting.
* **`CompoundTransactionCommand`**: Atomic multi-command batch that rolls back already-executed steps in reverse order if any step fails.
* **Bounded Low-Memory Footprint**: `CommandStack` enforces both entry-count and byte-size budgets (default 16 MiB), evicting the oldest undo entries first and reporting live usage in `core::ResourceStats`.

### Portable Studio Editor Controller (`app::EditorController`)

`EditorController` (`include/finaleffect/app/editor_controller.hpp`) sits directly over `StudioSession` without any OS or DOM dependencies:

* Executes all authoring mutations (spawning shared-mesh presets, duplicating, deleting with active-camera protection, renaming, visibility toggling, parent hierarchy editing, XYZ Euler degree transform editing via `Quat::toEulerXYZ`/`fromEulerXYZ`, camera/light component editing, undo/redo, and `.fe3d`/`.fe3dbin` project load/export) against the real `Scene`, `CommandStack`, `Renderer`, and `ProjectSerializer`.
* Exposes both strongly typed C++ methods and a deterministic JSON state/action bridge (`serializeStateJson` / `executeActionJson`) using `finaleffect_project`'s self-contained JSON codec, powering the connected WebAssembly Studio UI (`apps/web/main.cpp` + `web/index.html`, `web/editor.css`, `web/editor.js`) without mock objects or duplicated UI state.
* Passes `selectedObject()` into `Renderer::renderScene`, applying a warm Fresnel rim/selection highlight across `SoftwareRenderDevice`, `OpenGL33RenderDevice`, and `WebGL2RenderDevice`.

---

## 5. Project Format (`.fe3d` Schema `1.1`) & Binary Asset Container (`.fe3dbin` `1.0`)

### `.fe3d` Metadata Format

* `"format"`: `"finaleffect.project"`
* `"schema_version"`: `{ "major": 1, "minor": 1 }` (backward-compatible reader for `1.0` inline meshes and `1.1` binary-chunked bundles)
* `"coordinate_system"`: `"right-handed-y-up"`
* Companion `binary_chunk` references validate strictly against path traversal (`..`, `/`, `\`, drive prefixes rejected).

### `.fe3dbin` Binary Geometry Container (`include/finaleffect/project/binary_asset.hpp`)

Designed for fast, low-memory loading on laptops, browsers, and future mobile targets without unsafe struct memory dumps:

* **Header (32 bytes, little-endian)**: Magic `"FE3DBIN\0"`, `uint16 major=1`, `uint16 minor=0`, `uint32 chunk_count`, `uint64 directory_offset`, `uint32 directory_crc32`, `uint32 reserved`.
* **Portable Encoding**: Every vertex attribute (`position`, `normal`, `color`, `uv` = 44 bytes/vertex) is serialized with `std::bit_cast<uint32_t>(float)` in explicit little-endian byte order (`-0.0f` normalized to `+0.0f` for canonical hashing).
* **Content-Addressed Deduplication**: Chunks are addressed by 64-bit FNV-1a hashes formatted as lossless 16-character hex strings in `.fe3d` JSON (avoiding IEEE-754 JSON `double` 53-bit integer truncation). Duplicate meshes in a scene write only **one** binary payload.
* **IEEE 802.3 CRC-32 Integrity**: Both the chunk directory and every mesh payload carry CRC-32 checksums verified prior to decoding.

---

## 6. Planned Repository Modules (Not Yet Populated)

To avoid shipping empty placeholder directories, native mobile host directories will be added only when their implementations land:

* `platform/ios/`: Objective-C++ / Swift `UIViewController` + `CAMetalLayer` host integration.
* `platform/android/`: Android NDK `GameActivity` / `ANativeWindow` host integration.
