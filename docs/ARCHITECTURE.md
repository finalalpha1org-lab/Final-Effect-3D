# Final Effect 3D — Stage 1 Architecture

## 1. Architectural Principles

Final Effect 3D is architected as a modular C++20 engine core surrounded by thin, target-specific platform and rendering adapters:

1. **Strict Engine vs. Application Separation**: `finaleffect::core::Engine` manages runtime state transitions, frame timing, file access, paths, and logging. Domain logic (`finaleffect::app::StudioSession`) implements `core::IApplication` and never embeds platform lifecycle boilerplate.
2. **Zero Global Mutable State**: There are no singleton managers or global loggers. `Engine` explicitly owns its `Logger`, `IFileSystem`, and `IClock` instances via RAII.
3. **API-Clean Portable Core**: Graphics API types and headers never appear in `core`, `math`, `scene`, `project`, `platform`, or portable `rendering`.
4. **CMake-Driven Platform Isolation**: OS-specific path logic lives in dedicated translation units (`platform/linux/`, `platform/windows/`, `platform/macos/`) selected by CMake rather than scattering `#ifdef` conditionals throughout engine code.
5. **Deterministic Error Handling**: Fallible operations return `core::Result<T>` or `core::Status` (`Result<void>`) carrying structured `ErrorCode`, human-readable messages, and diagnostic context.

---

## 2. Module Dependency Graph

```text
finaleffect::platform   (IFileSystem, IClock, AppPaths, IWindow)
        │
        ▼
finaleffect::core       (Version, Error/Result, Logger, ResourceStats, Engine, IApplication)
        │
        ▼
finaleffect::math       (Vec2, Vec3, Vec4, Quat, Mat4, Transform, ClipSpaceDepth)
        │
        ▼
finaleffect::scene      (ObjectId, MeshId, MeshData, Primitives, Camera, Light, Scene)
       ╱ ╲
      ▼   ▼
project   rendering     (ProjectSerializer/.fe3d JSON)  (Renderer, IRenderDevice, Null, Software)
      ╲   ╱                                                       │
       ▼ ▼                                                        ▼ (optional desktop target)
finaleffect::app        (StarterScene, StudioSession)   finaleffect::rendering_opengl33
        │                                                         │
        └──────────────────────┬──────────────────────────────────┘
                               ▼
                      finaleffect_desktop  (+ optional finaleffect_window_glfw)
```

---

## 3. Coordinate System & Mathematical Conventions

All portable engine modules and serialized `.fe3d` files enforce a single documented coordinate system:

* **Handedness**: Right-handed 3D coordinate space (`+X × +Y = +Z`).
* **World Axes**:
  * `+X`: Right
  * `+Y`: Up
  * `+Z`: Outward toward viewer (`-Z` is local/camera forward)
* **Matrix Layout**: Column-major `float[16]` (`element(row, col) = data[col * 4 + row]`), directly matching GLSL, Metal Shading Language, and WGSL `mat4` memory layout.
* **Vector Multiplication**: Column-vector convention (`v' = M * v`). Hierarchical world matrices compose from root to leaf (`M_world = M_root * ... * M_parent * M_local`).
* **Rotations & Angles**: Unit quaternions `(x, y, z, w)` with identity `(0, 0, 0, 1)`. All math APIs accept angles in **radians**.
* **Clip-Space Depth Abstraction**: `math::ClipSpaceDepth` supports both:
  * `NegativeOneToOne` (`[-1, 1]` NDC depth — used by OpenGL 3.3 Core, WebGL2, and `SoftwareRenderDevice`).
  * `ZeroToOne` (`[0, 1]` NDC depth — ready for future Vulkan, Metal, and WebGPU backends).

---

## 4. Ownership, Scene Graph & Low-Memory Design

* **Single-Owner `Scene`**: `scene::Scene` owns all `SceneObject` records and `MeshAsset` payloads in contiguous `std::vector` containers indexed by `O(1)` hash tables (`ObjectId` and `MeshId`, 64-bit non-zero monotonic handles).
* **Mesh Asset Deduplication**: `SceneObject` stores an optional `MeshId` handle rather than duplicating vertex/index vectors. Multiple scene instances reference one CPU `MeshAsset` and upload **one** GPU vertex/index buffer in `rendering::Renderer`.
* **Hierarchy Invariants**: All parent-child changes flow through `Scene::setParent`, which walks the ancestor chain to reject self-parenting and multi-hop cycles (`ErrorCode::HierarchyCycle`). `Scene::removeObject` supports both `RemovalMode::Recursive` (iterative stack traversal) and `RemovalMode::ReparentChildren`, automatically clearing `active_camera_id` if the active camera is removed.
* **GPU Resource Separation**: `Scene` never holds OpenGL VAO/VBO IDs or backend pointers. `rendering::Renderer` maintains an internal `MeshId -> GpuMeshHandle` table synchronized by asset revision and deterministically frees all GPU buffers during `releaseSceneResources()` / `shutdown()`.
* **Resource Telemetry**: `core::ResourceStats` exposes live object counts, shared mesh counts, CPU vertex/index byte totals, and active GPU mesh buffer byte totals so memory budgets can be measured quantitatively before introducing custom allocators in later stages.

---

## 5. Project Format Specification (`.fe3d` Schema `1.0`)

Stage 1 defines an original, human-inspectable JSON project format:

* `"format"`: `"finaleffect.project"`
* `"schema_version"`: `{ "major": 1, "minor": 0 }`
* `"coordinate_system"`: `"right-handed-y-up"`
* `"metadata"`: `project_name`, `author`, `description`, `engine_version`
* `"scene"`: `name`, `active_camera_id`, `meshes[]`, `objects[]`

### Safety & Validation Guarantees

* **No Raw Memory Dumps**: Every field is explicitly serialized and validated; no `reinterpret_cast` struct dumps are ever read or written.
* **Bit-Exact Float Round-Trips**: Single-precision floats are formatted locale-independently using 9 significant digits (`%.9g`), preserving exact IEEE-754 `binary32` values across save/load cycles.
* **Adversarial Input Hardening**:
  * Nesting depth is capped (default 64 levels) to prevent stack exhaustion.
  * Malformed syntax reports `ErrorCode::ParseError` with exact `line` and `col`.
  * Unsupported major versions or newer minor versions fail cleanly with `ErrorCode::UnsupportedVersion`.
  * Non-finite numbers, out-of-bounds triangle indices, dangling `mesh_id` / `parent_id` / `active_camera_id`, and cyclic parent graphs are rejected cleanly before returning a `ProjectDocument`.
* **Future Binary Assets**: Large geometry caches, simulations, and textures in Stage 2+ will be stored as chunked external/companion binary streams referenced by content URI/hash from `.fe3d` metadata rather than bloating JSON text.

---

## 6. Planned Repository Modules (Not Yet Populated)

To avoid shipping empty placeholder files or misleading stub directories, the following planned directories from the long-term layout will be added only when their real implementations land:

* `apps/web/` and `web/`: Emscripten WebAssembly browser entry point, HTML/Canvas shell, and async asset loader.
* `platform/ios/`: Objective-C++ / Swift `UIViewController` + `CAMetalLayer` host integration.
* `platform/android/`: Android NDK `GameActivity` / `ANativeWindow` host integration.
* `third_party/`: Vendored or manifest-pinned dependencies if offline third-party libraries are introduced in future stages.
