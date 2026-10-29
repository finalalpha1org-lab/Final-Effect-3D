# Final Effect 3D

Final Effect 3D is an open-source, cross-platform 3D creative engine written in C++20.
It is an original codebase: it does not fork, wrap or reimplement another editor.

**Stage 1 ships the engine foundation, not a modelling application.** This repository
contains the portable engine core, a dependency-free CPU reference renderer, a scene
and project-file format, a headless command-line tool, and a unit/integration test
suite. There is no editor UI yet, and GPU backends are optional and incomplete.

## Status

Verified in this repository on Linux x86-64 with Clang 22.1.8 (C++20), CMake 4.4.4 and
Ninja 1.13.2: the table below separates what was actually built and executed from what
is only designed. See `docs/BUILDING.md` for the exact commands.

| Component | State |
| --- | --- |
| CMake build (options, presets, warning set) | Built in Debug and Release, warnings-as-errors clean |
| `fe3d_core` (version, errors, logging, JSON) | Built, unit-tested |
| `fe3d_math` (Vec2/Vec3/Quat/Mat4/Transform) | Built, unit-tested |
| `fe3d_scene` (objects, hierarchy, world matrices) | Built, unit-tested |
| `fe3d_rendering` (renderer interface, meshes, CPU rasterizer, PPM output) | Built, unit-tested |
| `fe3d_project` (`.fe3d` project files, atomic save, validation) | Built, unit-tested |
| `fe3d_engine` (engine, application loop, demo scene) | Built, unit-tested |
| `fe3d_tool` CLI (`info`, `demo-save`, `inspect`, `render`) | Built, executed end to end |
| Test suite (38 tests / 9 CTest suites) | Passing in Debug and Release |
| OpenGL 3.3 renderer + windowed desktop app | Source complete, **not built here**: needs GLFW, which is an opt-in download |
| Windows / macOS builds | **Not verified here**: no such host was available; CI matrix added |
| WebAssembly, iOS, Android | **Not implemented**; honest plan in `docs/PLATFORM_ROADMAP.md` |

## Quick start

```sh
cmake --preset core-release
cmake --build --preset core-release
ctest --preset core-release --output-on-failure
```

Render the built-in demo scene without a GPU or a window:

```sh
./build/core-release/bin/fe3d_tool demo-save demo.fe3d
./build/core-release/bin/fe3d_tool inspect demo.fe3d
./build/core-release/bin/fe3d_tool render demo.fe3d --out frame.ppm --width 640 --height 360
```

## Repository layout

```
CMakeLists.txt          top-level build, options, configuration summary
CMakePresets.json       core-debug / core-release / core-ci / desktop presets
cmake/                  compiler warning set, dependency resolution
include/finaleffect/    public headers, one directory per module
src/                    implementation, one directory per module
apps/cli/               fe3d_tool, the headless tool
apps/desktop/           windowed application (needs the OpenGL renderer)
tests/                  dependency-free test framework and suites
docs/                   architecture, building, platform roadmap, third-party notes
```

Layering is strict: `core -> math -> scene -> rendering -> project -> engine -> apps`.
The portable core never includes a platform header or a third-party header, and only
`src/platform/<platform>/` contains host-specific code.

## Documentation

* `docs/ARCHITECTURE.md` - module boundaries, conventions, data flow
* `docs/BUILDING.md` - requirements, options, presets, how to verify a build
* `docs/PLATFORM_ROADMAP.md` - what runs where today, and what each target still needs
* `docs/THIRD_PARTY.md` - every third-party dependency with its licence and obligations

## License

Final Effect 3D is distributed under the GNU General Public License version 3.0
(`LICENSE`, SPDX `GPL-3.0-only`). Each source file carries the same identifier.
Third-party components keep their own licences; see `docs/THIRD_PARTY.md`.
