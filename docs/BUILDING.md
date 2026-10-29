# Building & Testing Final Effect 3D

## 1. Requirements

### Portable Core & Automated Tests (All Desktop Platforms)

* **C++20 compiler**:
  * Linux: GCC `>= 12` or Clang `>= 15`
  * macOS: AppleClang (Xcode `>= 15`)
  * Windows: MSVC (Visual Studio 2022 `>= 17.6`)
* **CMake**: `>= 3.22`
* **Network access**: **Not required** for core libraries, CPU software renderer, or unit tests.

### Interactive Desktop OpenGL 3.3 Window (Optional)

To build `finaleffect_desktop` with live desktop windowing (`GlfwWindow` + `OpenGL33RenderDevice`):

* System OpenGL development headers/libraries (`OpenGL::GL`)
* **GLFW** `>= 3.3` development package:
  * **Debian / Ubuntu**: `sudo apt-get install libglfw3-dev libgl1-mesa-dev`
  * **Fedora**: `sudo dnf install glfw-devel mesa-libGL-devel`
  * **macOS (Homebrew)**: `brew install glfw`
  * **Windows (vcpkg)**: `vcpkg install glfw3` (pass `-DCMAKE_TOOLCHAIN_FILE=...`) or pass `-DFINALEFFECT_FETCH_GLFW=ON` when network access to GitHub is available.

---

## 2. CMake Options

| Option | Default | Description |
| :--- | :--- | :--- |
| `FINALEFFECT_BUILD_TESTS` | `ON` | Build unit and integration test executables and register them with CTest |
| `FINALEFFECT_BUILD_RENDERER_DESKTOP` | `ON` | Build `finaleffect_rendering_opengl33`, `finaleffect_desktop`, and optional `finaleffect_window_glfw` |
| `FINALEFFECT_FETCH_GLFW` | `OFF` | Download pinned GLFW `3.4` via CMake `FetchContent` if system GLFW is not installed |
| `FINALEFFECT_REQUIRE_GLFW` | `OFF` | Fail CMake configuration if `FINALEFFECT_BUILD_RENDERER_DESKTOP=ON` and GLFW + OpenGL are missing |
| `FINALEFFECT_WARNINGS_AS_ERRORS` | `OFF` | Enable `-Werror` (GCC/Clang) or `/WX` (MSVC) |

> **Out-of-source builds are enforced.** Running `cmake .` in the repository root will stop with an explicit error so generated files never pollute the source tree.

---

## 3. Building with CMake Presets

`CMakePresets.json` defines four verified configurations:

* `core-debug` — Core engine + tests only (`FINALEFFECT_BUILD_RENDERER_DESKTOP=OFF`, `Debug`, `-Werror`)
* `core-release` — Core engine + tests only (`FINALEFFECT_BUILD_RENDERER_DESKTOP=OFF`, `Release`, `-Werror`)
* `desktop-debug` — Full desktop build + tests (`FINALEFFECT_BUILD_RENDERER_DESKTOP=ON`, `Debug`, `-Werror`)
* `desktop-release` — Full desktop build + tests (`FINALEFFECT_BUILD_RENDERER_DESKTOP=ON`, `Release`, `-Werror`)

```bash
# Example: Core-only build & test
cmake --preset core-debug
cmake --build --preset core-debug --parallel
ctest --preset core-debug

# Example: Desktop build & test
cmake --preset desktop-release
cmake --build --preset desktop-release --parallel
ctest --preset desktop-release
```

---

## 4. Platform-Specific Manual Commands

### Linux / macOS

```bash
cmake -S . -B build/release \
  -DCMAKE_BUILD_TYPE=Release \
  -DFINALEFFECT_BUILD_RENDERER_DESKTOP=ON \
  -DFINALEFFECT_BUILD_TESTS=ON

cmake --build build/release --parallel
ctest --test-dir build/release --output-on-failure --verbose
```

### Windows (Visual Studio 2022 Multi-Config Generator)

```powershell
cmake -S . -B build/msvc `
  -DFINALEFFECT_BUILD_RENDERER_DESKTOP=ON `
  -DFINALEFFECT_BUILD_TESTS=ON

cmake --build build/msvc --config Release --parallel
ctest --test-dir build/msvc --build-config Release --output-on-failure --verbose
```

---

## 5. Running the Desktop Application

### Interactive OpenGL 3.3 Mode (Requires GLFW + Display)

```bash
./build/desktop-release/finaleffect_desktop
```

* Opens a `1280x720` resizable OpenGL 3.3 Core window rendering the Final Effect 3D starter scene.
* Press `Escape` or close the window to shut down cleanly.
* Load an existing `.fe3d` project with `--project path/to/scene.fe3d`.

### Headless Verification Mode (Works Anywhere, Including Headless Servers/CI)

```bash
./build/desktop-release/finaleffect_desktop \
  --headless \
  --frames 5 \
  --width 640 --height 360 \
  --export-project build/exported_scene.fe3d \
  --render-ppm build/rendered_frame.ppm
```
