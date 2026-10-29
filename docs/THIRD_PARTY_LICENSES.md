# Open-Source License & Third-Party Dependency Inventory

## 1. Project License

* **Project**: Final Effect 3D
* **License**: **GNU General Public License, Version 3, 29 June 2007** (`GPL-3.0`)
* **Repository Files**:
  * [`LICENSE`](../LICENSE) — verbatim GNU GPL v3.0 license text (preserved unchanged).
  * [`NOTICE`](../NOTICE) — repository release notice (preserved unchanged).

All original source files created in Stage 1 and Stage 2 carry SPDX license headers compatible with the repository's GNU GPL v3.0 license (`SPDX-License-Identifier: GPL-3.0-or-later`).

---

## 2. Clean-Room & Upstream Code Reuse Policy

* Final Effect 3D is an **original codebase**. No source files, headers, shaders, math routines, command stacks, or project/binary schemas have been copied from Blender or any other third-party 3D application.
* **Important Legal Note on External GPL/Open-Source Creative Suites**: Large open-source creative applications such as Blender contain components under *different* licenses (including `GPL-2.0-only`, `GPL-2.0-or-later`, `GPL-3.0-or-later`, `LGPL`, `Apache-2.0`, `BSD`, `MIT`, and `Zlib`). For example, `GPL-2.0-only` code is **incompatible** with `GPL-3.0`. No external code may be copied into Final Effect 3D without an explicit file-by-file license compatibility and attribution review.

---

## 3. Third-Party Dependency Inventory (Stage 1 & Stage 2)

### A. Mandatory Core Engine & Unit Test Dependencies

**None.**

`finaleffect_platform`, `finaleffect_core`, `finaleffect_math`, `finaleffect_scene`, `finaleffect_project` (including `.fe3dbin` + CRC-32 + FNV-1a), `finaleffect_rendering`, `finaleffect_rendering_opengl33`, `finaleffect_rendering_webgl2`, `finaleffect_app`, and all 10 unit test suites rely exclusively on the **ISO C++20 Standard Library** and standard OS system libraries.

### B. Optional Host Windowing & Platform Toolchain Dependencies

| Dependency | Version | Source | License | Usage in Final Effect 3D | GPL-3.0 Compatibility & Obligations |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **GLFW** | `>= 3.3` (system) or pinned `3.4` when `FINALEFFECT_FETCH_GLFW=ON` | `https://github.com/glfw/glfw` | **zlib/libpng License** | Optional desktop window, input, and OpenGL 3.3 Core context creation (`finaleffect_window_glfw`) | **Compatible** with GPL-3.0. Retain GLFW's copyright/zlib notice in binary redistributions that link GLFW. |
| **System OpenGL Runtime** (`libGL` / `opengl32.dll` / macOS `OpenGL.framework`) | Host OS ABI | Host OS / Mesa3D (`https://mesa3d.org`) | System Library / MIT / Khronos | Linked dynamically when `FINALEFFECT_HAS_GLFW=ON` | Covered by the **System Library** definition in GNU GPL v3.0 Section 1; Mesa3D uses permissive MIT/X11 licenses compatible with GPL-3.0. |
| **Emscripten SDK Runtime** (`emscripten/html5.h` JS/WASM glue) | `>= 3.1.50` (`3.1.68` in CI) | `https://github.com/emscripten-core/emscripten` | **MIT License / University of Illinois/NCSA Open Source License** | Used only when cross-compiling optional `finaleffect_web` (`apps/web/main.cpp`) to WebAssembly | **Compatible** with GPL-3.0 (permissive dual MIT/NCSA license). Retain standard runtime copyright notices if distributing compiled `.js`/`.wasm` bundles alongside GPLv3 source offer. |
