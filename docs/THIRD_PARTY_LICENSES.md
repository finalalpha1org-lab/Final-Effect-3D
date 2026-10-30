# Open-Source License & Third-Party Dependency Inventory

## 1. Project License

* **Project**: Final Effect 3D
* **License**: **GNU General Public License, Version 3, 29 June 2007** (`GPL-3.0`)
* **Repository Files**:
  * [`LICENSE`](../LICENSE) — verbatim GNU GPL v3.0 license text (preserved unchanged).
  * [`NOTICE`](../NOTICE) — repository release notice (preserved unchanged).

All original source files created in Stages 1–4 (`v0.1.0` through `v0.4.0`) carry SPDX license headers compatible with the repository's GNU GPL v3.0 license (`SPDX-License-Identifier: GPL-3.0-or-later`).

---

## 2. Clean-Room & Upstream Code Reuse Policy

* Final Effect 3D is an **original codebase**. No source files, headers, shaders, 3D gizmo math, mesh-editing data structures, PBR BRDF shaders, workspace layout managers, command stacks, or project/binary schemas have been copied from Blender or any other third-party 3D application.
* **Important Legal Note on External GPL/Open-Source Creative Suites**: Large open-source creative applications such as Blender contain components under *different* licenses (including `GPL-2.0-only`, `GPL-2.0-or-later`, `GPL-3.0-or-later`, `LGPL`, `Apache-2.0`, `BSD`, `MIT`, and `Zlib`). For example, `GPL-2.0-only` code is **incompatible** with `GPL-3.0`. No external code may be copied into Final Effect 3D without an explicit file-by-file license compatibility and attribution review.

---

## 3. Third-Party Dependency Inventory (Stages 1–4, `v0.4.0`)

### A. Mandatory Core Engine, Editing Tools, Workspace & C++ Unit Test Dependencies

**None (zero new dependencies introduced in Stage 4).**

`finaleffect_platform`, `finaleffect_core`, `finaleffect_math`, `finaleffect_scene` (including `EditableMesh` and `PbrMaterial`), `finaleffect_project` (including `.fe3d v1.2`, `.fe3dbin v1.0`, CRC-32, and FNV-1a), `finaleffect_rendering` (including `SoftwareRenderDevice` and `MaterialPreviewRenderer`), `finaleffect_rendering_opengl33`, `finaleffect_rendering_webgl2`, `finaleffect_app` (`TransformGizmo`, `WorkspaceManager`, `StudioSession`, `EditorController`), and all 15 C++ CTest suites rely exclusively on the **ISO C++20 Standard Library** and standard OS system libraries. Likewise, the connected browser Studio UI (`web/index.html`, `web/editor.css`, `web/editor.js`) uses standard HTML5/CSS3/ES2022 browser APIs with zero runtime npm packages or external CDN assets.

### B. Optional Host Windowing, Platform Toolchain & Browser Test Dependencies

| Dependency | Version | Source | License | Usage in Final Effect 3D | GPL-3.0 Compatibility & Obligations |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **GLFW** | `>= 3.3` (system) or pinned `3.4` when `FINALEFFECT_FETCH_GLFW=ON` | `https://github.com/glfw/glfw` | **zlib/libpng License** | Optional desktop window, input, and OpenGL 3.3 Core context creation (`finaleffect_window_glfw`) | **Compatible** with GPL-3.0. Retain GLFW's copyright/zlib notice in binary redistributions that link GLFW. |
| **System OpenGL Runtime** (`libGL` / `opengl32.dll` / macOS `OpenGL.framework`) | Host OS ABI | Host OS / Mesa3D (`https://mesa3d.org`) | System Library / MIT / Khronos | Linked dynamically when `FINALEFFECT_HAS_GLFW=ON` | Covered by the **System Library** definition in GNU GPL v3.0 Section 1; Mesa3D uses permissive MIT/X11 licenses compatible with GPL-3.0. |
| **Emscripten SDK Runtime** (`emscripten/html5.h` JS/WASM glue) | `>= 3.1.50` (`3.1.68` in CI) | `https://github.com/emscripten-core/emscripten` | **MIT License / University of Illinois/NCSA Open Source License** | Used only when cross-compiling optional `finaleffect_web` (`apps/web/main.cpp`) to WebAssembly | **Compatible** with GPL-3.0 (permissive dual MIT/NCSA license). Retain standard runtime copyright notices if distributing compiled `.js`/`.wasm` bundles alongside GPLv3 source offer. |
| **Playwright Test (`@playwright/test`)** | `1.48.2` | `https://registry.npmjs.org/@playwright/test` (`https://github.com/microsoft/playwright`) | **Apache License 2.0** | Dev/CI-only real-browser Chromium WebGL 2.0 end-to-end test runner in `web/e2e/` (not linked into or distributed with Final Effect 3D binaries) | **Compatible** with GPL-3.0 (Apache-2.0 is one-way compatible with GPLv3; moreover, Playwright runs purely as an external automated test harness). |
