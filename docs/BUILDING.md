# Building and verifying

## Requirements

* CMake 3.20 or newer (presets need 3.23+; the presets declare that explicitly).
* A C++20 compiler: GCC 11+, Clang 14+, AppleClang 14+, MSVC 19.30+ (Visual Studio 2022).
* Ninja, Make or another CMake generator.
* Nothing else. The core, the CLI and the tests have no third-party dependency, need
  no GPU, no window, no display and no network access.

OpenGL is optional and only matters for the desktop application:

* GLFW 3.4 for the window and the GL context (`FE3D_FETCH_GLFW=ON` downloads and
  builds the pinned release when no system GLFW is found; the core build never does).

## Configure, build, test

```sh
cmake --preset core-release              # portable core, CLI, tests
cmake --build --preset core-release
ctest --preset core-release --output-on-failure
```

Equivalent explicit form, for any compiler or generator:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Presets: `core-debug`, `core-release`, `core-ci` (Release plus warnings-as-errors,
the preset CI uses), `desktop` (OpenGL renderer plus windowed application).

## Options

| Option | Default | Effect |
| --- | --- | --- |
| `FE3D_BUILD_TESTS` | ON | Build the test executable and register CTest entries |
| `FE3D_BUILD_CLI` | ON | Build `fe3d_tool` (headless) |
| `FE3D_BUILD_SOFTWARE_RENDERER` | ON | Build the dependency-free CPU renderer |
| `FE3D_BUILD_DESKTOP_APP` | OFF | Build the windowed application (needs the OpenGL renderer) |
| `FE3D_ENABLE_OPENGL_RENDERER` | OFF | Build the OpenGL 3.3 renderer (needs GLFW) |
| `FE3D_FETCH_GLFW` | OFF | Download and build the pinned GLFW when it is missing |
| `FE3D_WARNINGS_AS_ERRORS` | OFF | Treat warnings as errors |

Configure with `-DFE3D_ENABLE_OPENGL_RENDERER=ON` and, if GLFW is not installed,
`-DFE3D_FETCH_GLFW=ON`, then `-DFE3D_BUILD_DESKTOP_APP=ON` for the windowed build.

## Options for the tool

```sh
./build/core-release/bin/fe3d_tool info
./build/core-release/bin/fe3d_tool demo-save demo.fe3d
./build/core-release/bin/fe3d_tool inspect demo.fe3d
./build/core-release/bin/fe3d_tool render demo.fe3d --out frame.ppm --width 640 --height 360 --verbose
```

`render` writes a binary PPM (P6). Without a project path it renders a built-in
scene, which makes it usable as a smoke test on a machine with nothing installed.

The desktop application, once built, accepts
`--headless --frames N --width W --height H --screenshot frame.ppm` for automated runs.

## How to verify a build honestly

1. `cmake --preset core-ci` - a Release configure with warnings-as-errors; it fails if
   any translation unit warns.
2. `cmake --build --preset core-ci` - all libraries, the tool and the tests compile.
3. `ctest --preset core-ci --output-on-failure` - nine suites: `core`, `math`, `scene`,
   `project`, `rendering`, `engine`, `all`, `cli.info`, `cli.render`.
4. `fe3d_tool render` and check the reported non-background pixel count is non-zero.

Anything not covered by those steps - the OpenGL renderer, the windowed application,
Windows and macOS hosts, WebAssembly, iOS and Android - must be reported as unverified
until it has actually been built and run on the relevant host.

## Verification recorded for Stage 1

* Linux x86-64, Clang 22.1.8 (via Zig 0.17.0's bundled toolchain, no system compiler),
  CMake 4.4.4, Ninja 1.13.2.
* `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`: configure OK.
* `cmake --build build`: 39/39 targets, no warnings.
* `ctest --test-dir build`: 9/9 suites passed; `fe3d_tests` reports 38/38 tests.
* `cmake --build build-release -DFE3D_WARNINGS_AS_ERRORS=ON`: 39/39 targets, clean.
* `fe3d_tool demo-save/inspect/render`: 4 objects written and read back, 240x180 PPM
  with 4082 non-background pixels.
