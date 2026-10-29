# Stage 1 report - engine foundation

Status of the code in this branch, written after actually building and running it. Anything
not listed as verified was not built or run, and is marked accordingly.

## Scope delivered

A new, original C++20 engine foundation: portable core (errors, results, logging, JSON),
math library, scene graph, rendering abstraction with a dependency-free CPU renderer,
`.fe3d` project serialization, platform abstraction, engine/application layer, a headless
CLI tool, a dependency-free test suite, a CMake build with presets, GitHub Actions CI, and
this documentation set. 77 new files; `LICENSE` (GPL-3.0) and `NOTICE` are untouched.

## Verification actually performed

Host: Linux x86-64, Clang 22.1.8 (Zig 0.17.0 bundled toolchain, because the authoring
container had no system C++ compiler), CMake 4.4.4, Ninja 1.13.2.

| Step | Command | Result |
| --- | --- | --- |
| Configure (Debug) | `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` | OK; reports compiler, C++ standard and options |
| Build (Debug) | `cmake --build build -j 2` | 39/39 targets, no compiler warnings |
| Tests (Debug) | `ctest --test-dir build` | 9/9 suites passed |
| Unit tests | `./build/bin/fe3d_tests` | 38/38 tests passed |
| Build (Release, -Werror) | `cmake -S . -B build-release -DFE3D_WARNINGS_AS_ERRORS=ON ...` + build | 39/39 targets, clean |
| Tests (Release) | `ctest --test-dir build-release` | 9/9 suites passed |
| CLI end to end | `fe3d_tool demo-save / inspect / render` | 4 objects written and read back; 240x180 PPM with 4082 non-background pixels |

The nine CTest suites are `core`, `math`, `scene`, `project`, `rendering`, `engine`,
`all`, `cli.info` and `cli.render`. `cli.render` renders a real frame with the CPU
rasteriser, so the render path is exercised on every run without a GPU.

## Defects found and fixed during verification

Compiling and running the code surfaced five real problems, all fixed here:

1. `Error::toString()` called itself instead of the free `toString(ErrorCode)`.
2. `JsonValue::ParseResult` held a `JsonValue` by value while being nested inside it
   (incomplete type); it is now the top-level `JsonParseResult`.
3. The JSON parser accepted leading zeroes (`{"a": 01}`); it now follows RFC 8259.
4. `Transform::fromMatrix` divided by signed scales, so a mirrored basis lost its
   rotation. It now normalises by magnitudes and folds the mirror into the z scale.
   The exactness limit of `Transform::inverse` (a rotation with a non-uniform scale is
   not representable as TRS) is documented in the header and asserted in the tests.
5. Several test expectations were wrong rather than the code: the orthographic depth
   constant, the JSON indentation offset, the world position after a 90 degree parent
   rotation, and a reparent that was expected to be a no-op but was a genuine cycle.
   The corrected expectations each carry a comment explaining the arithmetic.

No warning, and no failure, is suppressed: `FE3D_WARNINGS_AS_ERRORS=ON` is clean and the
whole suite passes without filters.

## Not verified here (do not assume these work)

* `fe3d_rendering_opengl` and `fe3d_desktop`: source complete, but GLFW could not be
  obtained in the authoring container, so they were never compiled. Build them with
  `cmake --preset desktop` on a host with GLFW and a GPU, or with `FE3D_FETCH_GLFW=ON`.
* Windows and macOS: the code is portable and CI jobs are defined, but no build ran on
  those hosts yet, so no claim is made.
* WebAssembly, iOS and Android: not implemented. `docs/PLATFORM_ROADMAP.md` lists what
  each target still needs; the platform interfaces exist so ports stay contained.
* The GitHub Actions workflow has not executed yet.

## Recommended Stage 2

An editor shell on top of the existing document model: viewport using the CPU renderer
first (so it works and is testable everywhere), then the OpenGL path; selection,
transform tools, undo/redo, and project save/load wired into the UI. Keep the portable
core free of platform headers and add automated screenshot comparisons once a GPU path
is verified in CI.
