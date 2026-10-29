# Building & Testing Final Effect 3D

## 1. Requirements

### Portable Core & Automated Tests (All Desktop Platforms)

* **C++20 compiler**:
  * Linux: GCC `>= 12` or Clang `>= 15`
  * macOS: AppleClang (Xcode `>= 15`)
  * Windows: MSVC (Visual Studio 2022 `>= 17.6`)
* **CMake**: `>= 3.22`
* **Network access**: **Not required** for core libraries, CPU software renderer, GPU adapter unit tests, or `.fe3d`/`.fe3dbin` tests.

### Interactive Desktop OpenGL 3.3 Window (Optional)

To build `finaleffect_desktop` with live desktop windowing (`GlfwWindow` + `OpenGL33RenderDevice`):

* System OpenGL development headers/libraries (`OpenGL::GL`)
* **GLFW** `>= 3.3` development package:
  * **Debian / Ubuntu**: `sudo apt-get install libglfw3-dev libgl1-mesa-dev`
  * **Fedora**: `sudo dnf install glfw-devel mesa-libGL-devel`
  * **macOS (Homebrew)**: `brew install glfw`
  * **Windows (vcpkg)**: `vcpkg install glfw3` (or `-DFINALEFFECT_FETCH_GLFW=ON` when GitHub network access is available).

### Browser WebAssembly Target (Optional)

To compile `finaleffect_web` (`apps/web/main.cpp` + `WebGL2RenderDevice`):

* **Emscripten SDK (`emsdk`)** `>= 3.1.50` (`emcmake` / `em++` with C++20 and WebGL 2.0 support).

---

## 2. CMake Options

| Option | Default | Description |
| :--- | :--- | :--- |
| `FINALEFFECT_BUILD_TESTS` | `ON` | Build unit and integration test executables and register them with CTest |
| `FINALEFFECT_BUILD_RENDERER_DESKTOP` | `ON` | Build `finaleffect_rendering_opengl33`, `finaleffect_desktop`, and optional `finaleffect_window_glfw` |
| `FINALEFFECT_BUILD_RENDERER_WEBGL2` | `ON` | Build `finaleffect_rendering_webgl2` (OpenGL ES 3.0 / WebGL 2.0 adapter) and its CTest suite |
| `FINALEFFECT_FETCH_GLFW` | `OFF` | Download pinned GLFW `3.4` via CMake `FetchContent` if system GLFW is not installed |
| `FINALEFFECT_REQUIRE_GLFW` | `OFF` | Fail CMake configuration if `FINALEFFECT_BUILD_RENDERER_DESKTOP=ON` and GLFW + OpenGL are missing |
| `FINALEFFECT_WARNINGS_AS_ERRORS` | `OFF` | Enable `-Werror` (GCC/Clang/Emscripten) or `/WX` (MSVC) |

> **Out-of-source builds are enforced.** Running `cmake .` in the repository root will stop with an explicit error so generated files never pollute the source tree.

---

## 3. Building with CMake Presets

`CMakePresets.json` defines five standard presets:

* `core-debug` / `core-release` — Portable core, math, scene, commands, `.fe3d`/`.fe3dbin`, software renderer, and interactive viewport tests (`8` CTest suites).
* `desktop-debug` / `desktop-release` — Full desktop build + OpenGL 3.3 & WebGL 2.0 adapters + `finaleffect_desktop` (`10` CTest suites).
* `wasm-release` — Emscripten WebAssembly + WebGL 2.0 browser bundle (`requires EMSDK env variable`).

```bash
# Portable core build & test
cmake --preset core-debug
cmake --build --preset core-debug --parallel
ctest --preset core-debug

# Desktop + GPU adapter build & test
cmake --preset desktop-release
cmake --build --preset desktop-release --parallel
ctest --preset desktop-release
```

---

## 4. Building & Serving the WebAssembly Prototype (`finaleffect_web`)

With `emsdk` activated in your shell:

```bash
emcmake cmake -S . -B build/wasm-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DFINALEFFECT_BUILD_RENDERER_DESKTOP=OFF \
  -DFINALEFFECT_BUILD_RENDERER_WEBGL2=ON \
  -DFINALEFFECT_BUILD_TESTS=OFF

cmake --build build/wasm-release --parallel
python3 -m http.server 8080 --directory build/wasm-release
```

Then open `http://localhost:8080/index.html` in a WebGL 2.0-capable desktop or mobile browser. Because the WebAssembly build runs single-threaded without `SharedArrayBuffer` requirements, it works on standard static HTTP hosts without special COOP/COEP headers.

---

## 5. Running the Desktop Application & Viewport Controls

### Interactive Viewport Controls (Desktop GLFW & Browser WebGL2)

* **Left Click**: Raycast pick 3D object under cursor
* **Right Drag** / **Middle Drag** / **Alt + Left Drag**: Orbit camera around focus target
* **Shift + Middle/Right Drag**: Pan camera target in view plane
* **Mouse Wheel**: Exponential dolly zoom
* **`F`**: Frame selected object bounds (or entire scene if nothing is selected)
* **`Delete` / `Backspace`**: Delete selected object (recorded on Undo stack)
* **`Ctrl+Z` / `Ctrl+Y` (or `Ctrl+Shift+Z`)**: Undo / Redo scene commands
* **`Space`**: Pause / resume showcase rig rotation
* **`Escape`**: Close desktop window

### Headless CLI Mode (Works Anywhere, Including Headless Servers/CI)

```bash
./build/desktop-release/finaleffect_desktop \
  --headless \
  --frames 5 \
  --width 640 --height 360 \
  --export-project build/inline_scene.fe3d \
  --export-bundle build/bundled_scene.fe3d \
  --render-ppm build/rendered_frame.ppm
```
