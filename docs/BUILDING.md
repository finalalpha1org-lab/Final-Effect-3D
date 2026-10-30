# Building & Testing Final Effect 3D (`v0.4.0`)

## 1. Requirements

### Portable Core & Automated Tests (All Desktop Platforms)

* **C++20 compiler**:
  * Linux: GCC `>= 12` or Clang `>= 15`
  * macOS: AppleClang (Xcode `>= 15`)
  * Windows: MSVC (Visual Studio 2022 `>= 17.6`)
* **CMake**: `>= 3.22`
* **Network access**: **Not required** for core libraries, 3D gizmos, `EditableMesh`, PBR material preview, CPU software renderer, GPU adapter unit tests, or `.fe3d`/`.fe3dbin` tests.

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

* **Emscripten SDK (`emsdk`)** `>= 3.1.50` (`3.1.68` in CI).

---

## 2. CMake Options

| Option | Default | Description |
| :--- | :--- | :--- |
| `FINALEFFECT_BUILD_TESTS` | `ON` | Build all 15 unit and integration test executables and register them with CTest |
| `FINALEFFECT_BUILD_RENDERER_DESKTOP` | `ON` | Build `finaleffect_rendering_opengl33`, `finaleffect_desktop`, and optional `finaleffect_window_glfw` |
| `FINALEFFECT_BUILD_RENDERER_WEBGL2` | `ON` | Build `finaleffect_rendering_webgl2` (OpenGL ES 3.0 / WebGL 2.0 PBR adapter) and its CTest suite |
| `FINALEFFECT_FETCH_GLFW` | `OFF` | Download pinned GLFW `3.4` via CMake `FetchContent` if system GLFW is not installed |
| `FINALEFFECT_REQUIRE_GLFW` | `OFF` | Fail CMake configuration if `FINALEFFECT_BUILD_RENDERER_DESKTOP=ON` and GLFW + OpenGL are missing |
| `FINALEFFECT_WARNINGS_AS_ERRORS` | `OFF` | Enable `-Werror` (GCC/Clang/Emscripten) or `/WX` (MSVC) |

> **Out-of-source builds are enforced.** Running `cmake .` in the repository root stops with an explicit error so generated files never pollute the source tree.

---

## 3. Building with CMake Presets & Running the 15 CTest Suites

`CMakePresets.json` defines five standard presets:

* `core-debug` / `core-release` — Portable core, math, scene, commands, gizmos, `EditableMesh`, PBR materials, `WorkspaceManager`, `.fe3d v1.2`/`.fe3dbin`, software renderer, viewport, and `EditorController` tests (`13` CTest suites).
* `desktop-debug` / `desktop-release` — Full desktop build + OpenGL 3.3 & WebGL 2.0 PBR adapters + `finaleffect_desktop` (`15` CTest suites when both GPU adapter targets are enabled).
* `wasm-release` — Emscripten WebAssembly + WebGL 2.0 browser Studio bundle (`requires EMSDK env variable`).

```bash
# Portable core build & test
cmake --preset core-debug
cmake --build --preset core-debug --parallel
ctest --preset core-debug --output-on-failure

# Desktop + GPU adapter build & test
cmake --preset desktop-release -DFINALEFFECT_BUILD_RENDERER_WEBGL2=ON
cmake --build --preset desktop-release --parallel
ctest --preset desktop-release --output-on-failure
```

---

## 4. Building & Serving the Connected WebAssembly Studio (`finaleffect_web`)

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

Open `http://localhost:8080/index.html` in any WebGL 2.0-capable browser to launch the connected Stage 4 Studio workspace (Mode & Gizmo toolbars, Scene Outliner, WebGL 2.0 PBR 3D Viewport, Mesh Modeling panel, PBR Material Editor with live 96×96 3D sphere preview, Modular Bottom Dock, and `.fe3d v1.2` / `.fe3dbin` import/export). Single-threaded WASM compilation avoids `SharedArrayBuffer` / COOP-COEP hosting restrictions.

### Automated Real-Browser Chromium E2E Verification (`web/e2e/`)

```bash
cd web/e2e
npm install
npx playwright install --with-deps chromium
npx playwright test --reporter=list
```

Launches Headless Chromium with WebGL 2.0 (`--use-gl=angle --use-angle=swiftshader`) and runs 7 end-to-end suites verifying WASM boot, WebGL 2.0 PBR rasterization, 3D gizmo hover/drag + coalesced undo, Face/Vertex polygon picking + extrusion + hole boundary fill, live 96×96 3D sphere material preview updates, workspace preset persistence across page reload, and `.fe3d v1.2` + `.fe3dbin` round-trips.

---

## 5. Interactive Viewport & Editing Shortcuts (Desktop GLFW & Browser WebGL2)

* **Left Click**: Pick 3D object (`Object` mode) or vertex / edge / polygon face (`Vertex` / `Edge` / `Face` mode); drag hovered 3D Gizmo handle (`Move` / `Rotate` / `Scale`)
* **`Shift + Left Click`**: Additive/toggle multi-element selection in Mesh Edit modes
* **`W` / `E` / `R`**: Switch 3D Transform Gizmo between **Move**, **Rotate**, and **Scale**
* **`1` / `2` / `3` / `4`**: Switch between **Object**, **Vertex**, **Edge**, and **Face** modes (`Tab` toggles Object ↔ Face mode)
* **`Escape`**: Cancel active 3D gizmo drag and restore exact pre-drag transform/mesh state
* **Right Drag** / **Middle Drag** / **Alt + Left Drag**: Orbit camera around focus target
* **Shift + Middle/Right Drag**: Pan camera target in view plane
* **Mouse Wheel**: Exponential dolly zoom
* **`F`**: Frame selected object bounds (or entire scene if nothing is selected)
* **`Delete` / `Backspace`**: Delete selected mesh elements (in Mesh Edit mode) or selected object (in Object mode)
* **`Ctrl+Z` / `Ctrl+Y` (or `Ctrl+Shift+Z`)**: Undo / Redo commands
* **`Space`**: Pause / resume showcase rig animation

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
