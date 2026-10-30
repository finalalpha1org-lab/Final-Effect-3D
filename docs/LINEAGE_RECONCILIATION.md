# Lineage reconciliation (branch `stage-1-engine-foundation`)

## What happened

Two Stage 1 foundations were written in parallel for this repository:

| Lineage | Where | Shape | Fate |
| --- | --- | --- | --- |
| `fe3d_*` engine (this branch, PR #1) | `stage-1-engine-foundation` | `include/finaleffect/**/PascalCase.hpp`, namespace `fe::*`, targets `fe3d_core` ... `fe3d_engine`, `apps/cli`, dependency-free JSON, CPU rasteriser, `.fe3d` project v1, 38 tests | never merged |
| `finaleffect_*` engine (main, PR #2 and #3) | `main` | `include/finaleffect/**/snake_case.hpp`, namespace `finaleffect::*`, targets `finaleffect_core` ... `finaleffect_desktop`, Stage 1 + Stage 2 viewport/undo-redo/binary assets + Stage 3 Studio editor and WebAssembly, 35 tests | merged first, canonical |

Merging the two trees is not a text merge: 18 paths collide **by case only**
(`math/Mat4.hpp` vs `math/mat4.hpp`, `core/Version.cpp` vs `core/version.cpp`, ...), which breaks
checkout on Windows and macOS; 14 more paths are the same file with different content
(`CMakeLists.txt`, `README.md`, `docs/*`, `tests/*_tests.cpp`, `apps/desktop/main.cpp`); and both
trees define a complete engine, so a union would ship two of everything. GitHub reports the
pull request in that direction as `mergeable_state: dirty` (108 files, 17,233 insertions).

## The reconciliation

The main lineage is kept as the single core of this repository, and this branch now mirrors
it, so the branch contains everything merged into `main` without any duplicate or
case-colliding source. Deliberate decisions:

* **Engine, math, scene, rendering, project, platform, app, web, tests, build system:**
  `main`'s versions, untouched. They are canonical and cover more (Stage 2 viewport,
  undo/redo command stack, `.fe3dbin` binary mesh assets, picking, Studio editor, OpenGL 3.3
  and WebGL 2 device adapters, WebAssembly host).
* **Headless tool:** the `fe3d_tool` CLI from this branch was dropped rather than ported,
  because `finaleffect_desktop --headless --frames N --render-ppm out.ppm` (plus `--project`,
  `--export-project`, `--export-bundle`) already provides the same capability in the canonical
  lineage.
* **Docs:** the canonical `docs/*` are kept. This branch's `docs/ARCHITECTURE.md`,
  `BUILDING.md`, `PLATFORM_ROADMAP.md`, `THIRD_PARTY.md` and `STAGE1_REPORT.md` described the
  `fe3d_*` engine and would now be wrong, so they are replaced by the canonical docs plus this
  record.
* **Tests:** one test story instead of two - the canonical suites and harness are kept.

Nothing is lost: the whole `fe3d_*` lineage remains in this branch's history (commits
`1c99edf3` ... `7db73c20`) and in PR #1, and any file can be recovered with
`git show <sha>:<path>`.

## Verification of the reconciled tree

* Linux x86-64, Clang 22.1.8 (Zig bundled toolchain), CMake 4.4.4, Ninja 1.13.2.
* Configure `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`: OK (Linux target,
  GLFW/OpenGL dev packages absent, desktop host and software runner still built).
* Build `cmake --build build -j 2`: 60/60 targets linked.
* Tests `ctest --test-dir build`: 11/11 suites passed (core, platform, math, scene, commands,
  project, rendering, viewport, editor, rendering_opengl33, rendering_webgl2).
* Headless render: `finaleffect_desktop --headless --frames 2 --width 320 --height 180
  --render-ppm frame.ppm`: engine v0.3.0, software rasteriser, valid 320x180 PPM written.

Not claimed: windowed GLFW run, Windows/macOS hosts, WebAssembly in a real browser.
