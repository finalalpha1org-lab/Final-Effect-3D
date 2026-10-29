# Architecture

## Goals

1. A portable core that builds and runs with only a C++20 standard library: no GPU,
   no window, no network, no third-party code.
2. Platform differences isolated in one place per platform, never scattered through
   the engine.
3. Deterministic, testable behaviour: no hidden global state, no exceptions on
   expected failure paths.
4. Small files with a single responsibility, so the code stays reviewable.

## Module layers

```
core        errors, results, logging, version, JSON      (no dependencies)
math        Vec2, Vec3, Quat, Mat4, Transform            (core)
scene       SceneObject, Scene, hierarchy, world matrices(scene -> math, core)
rendering   Renderer interface, meshes, camera, images   (rendering -> scene, math, core)
            SoftwareRenderer (CPU), gl/OpenGLRenderer (optional)
project     .fe3d document, validation, atomic save      (project -> scene, core)
engine      Engine, Application, DemoScene               (engine -> everything portable)
platform    FileSystem, Paths, Clock, Window             (interfaces + per-platform impls)
apps        fe3d_tool (CLI, headless), fe3d_desktop      (apps -> engine)
tests       dependency-free framework and suites         (tests -> public headers)
```

Dependencies point one way only. A lower layer never includes a higher one, and no
portable layer includes a platform header.

## Error handling

Expected failures (a missing file, a malformed project, a cyclic reparent) never throw
and never return a bare bool:

* `fe::core::Status` carries an `ErrorCode`, a human-readable message and a source tag.
* `fe::core::Result<T>` is either a value or that error.
* Error codes are stable enumerators (`IoFailure`, `NotFound`, `ParseError`,
  `InvalidArgument`, `Unsupported`, `CycleDetected`, `OutOfRange`, `Internal`), so
  callers and tests can branch on the cause rather than on text.

## Math conventions

* `Scalar` is `float`; the header documents how to switch it.
* Column-major `Mat4`; `m * v` transforms a point, right-handed axes with +Y up and
  -Z forward, so the engine matches common DCC tools.
* `Transform` is a value type holding position, rotation (`Quat`) and scale; its
  matrix is `T * R * S`. `Transform::inverse()` is exact for uniform scale or for an
  unrotated non-uniform scale; a rotation combined with a non-uniform scale produces a
  linear part that no TRS value can represent, so the header documents the limit and
  points at `Mat4::tryInverse` for the exact matrix inverse.
* Projections use OpenGL-style clip space ([-1, 1] depth), including the `w` component,
  so the software renderer and a GPU backend share one camera path.

## Scene model

* `Scene` owns every `SceneObject`. Handles are non-owning and invalidated by any
  structural change; references between objects use `ObjectId` values, never pointers.
* `ObjectId` values are stable across save/load and are never recycled, so a stale
  reference is detectable instead of silently aliasing another object.
* `localTransform` is relative to the parent; `worldMatrix()` walks the ancestor chain
  and multiplies downwards. `validate()` re-checks parents, child lists, ids and cycles,
  which is also what project loading uses to reject corrupt documents.

## Rendering abstraction

`Renderer` is a small interface (describe a target, begin a view, draw meshes, read
pixels back). Two implementations exist:

* `SoftwareRenderer` - a CPU rasteriser with clipping against the near plane and a
  perspective-correct vertex pipeline. It needs no GPU, so tests, CI and the CLI all
  render for real.
* `fe::rendering::gl::OpenGLRenderer` - OpenGL 3.3 Core, function pointers resolved
  through `glfwGetProcAddress`, no GL loader dependency.

`RenderViewBuilder` turns a `Scene` plus a `Camera` into a flat `RenderView` (draw
items with world matrices and world-space bounds), which is what keeps backends from
re-implementing scene traversal.

## Platform abstraction

`FileSystem`, `Paths`, `Clock` and `Window` are interfaces (or free functions) with
one implementation per platform behind the build system:

```
src/platform/posix/PosixPaths.cpp      Linux, macOS, future POSIX hosts
src/platform/windows/WindowsPaths.cpp  Windows (LOCALAPPDATA-style directories)
src/platform/generic/                  portable file system, clock, null window
src/platform/glfw/GlfwWindow.cpp       optional window backend (GLFW)
```

The build selects the source file per platform; there are no `#ifdef` chains inside
the engine, and a headless build simply uses `NullWindow`.

## Engine and application

`Engine` owns the scene, the mesh library, the active renderer and the resolved paths,
and provides the operations the CLI, tests and applications share (load/save a project,
render one frame, capture a screenshot). `Application` owns the frame loop, the camera
and the input-to-camera mapping; it is transport-agnostic and runs headless when no
window backend is present.

## Project file format

`.fe3d` files are JSON with a fixed shape:

```json
{ "format": "finaleffect3d-project", "formatVersion": 1, "application": { ... },
  "revision": 1, "settings": { ... }, "scene": { "objects": [ ... ] } }
```

Loading rejects a wrong `format`, a newer `formatVersion`, missing sections and
invalid scene data, and tolerates unknown keys so future fields do not break older
builds. Saving writes to a temporary file and renames it into place, so an interrupted
save never truncates an existing project. The JSON reader/writer is part of `core`
(original, dependency-free, depth-limited) rather than a third-party library.

## Testing

`tests/TestFramework.hpp` is a small header-only registry with `FE3D_TEST`,
`FE3D_CHECK*` and `FE3D_REQUIRE` macros; `TestRunner.cpp` executes suites and reports
file/line/expression. CTest registers one entry per suite plus an all-suites run, the
CLI smoke tests and (when built) the headless desktop run, so a failure names the
subsystem that broke. Tests exercise the software renderer and the engine end to end,
so the suite needs no GPU, window, network or display.
