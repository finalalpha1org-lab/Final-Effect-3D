# Platform roadmap

This document separates what runs today from what is still planned. Interfaces and
folders are not support: a platform is supported only once it is built and tested on
that platform, and this table says so honestly.

| Target | Today | What is missing |
| --- | --- | --- |
| Linux (headless, CPU renderer) | Built and tested: core, CLI, tests, PPM output | - |
| Linux (windowed, OpenGL) | Source complete, not built in the authoring environment (GLFW unavailable) | Build with `FE3D_ENABLE_OPENGL_RENDERER=ON` on a host with GLFW and a GPU |
| Windows | `core` is portable C++20 and `src/platform/windows/WindowsPaths.cpp` exists; CI job added | An actual build and test run on Windows |
| macOS | Same as Windows (POSIX paths apply) | An actual build and test run on macOS |
| WebAssembly | Not started | Emscripten toolchain file, `Window`/`FileSystem` backends, canvas/WebGL2 renderer, a browser smoke test |
| iOS | Not started | Platform paths, an EGL/Metal or GLES context, lifecycle hooks, an Xcode project |
| Android | Not started | Platform paths, an EGL context, JNI entry points, a Gradle project and NDK build |

## Design constraints that keep the ports possible

* The portable core is standard C++20 with no platform headers, so it compiles for a
  new target as soon as a compiler exists for it.
* Everything host-specific is one file per platform behind the build system:

  | Interface | Location | Portable fallback |
  | --- | --- | --- |
  | `platform::resolveAppPaths` | `src/platform/{posix,windows}/` | - |
  | `platform::FileSystem` | `src/platform/generic/` | portable stdio implementation |
  | `platform::Clock` | `src/platform/generic/` | monotonic + wall clock, plus a manual clock for tests |
  | `platform::Window` | `src/platform/{generic,glfw}/` | `NullWindow` for headless runs |

* The renderer is an interface with a CPU implementation that always exists, so a new
  target can be brought up (and tested) before any GPU code works.

## Order of work for the next stages

1. **Stage 2:** editor shell - document model on top of `Scene`, viewport with the CPU
   renderer first, then the OpenGL path, undo/redo, project save/load wired to the UI.
2. **Stage 3, GPU on desktop:** build and test the OpenGL renderer and the windowed
   application on Linux, Windows and macOS hosts, and add automated headless
   screenshot comparisons.
3. **Stage 4, WebAssembly:** Emscripten preset, `Window` and `FileSystem` backends,
   WebGL2 renderer, browser smoke test in CI.
4. **Stage 5, mobile:** platform paths, GLES/EGL context, lifecycle handling, touch
   input, and a signed build pipeline for iOS and Android.

Each stage is expected to keep the portable core untouched except for additive,
platform-neutral APIs.
