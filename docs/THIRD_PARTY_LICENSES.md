# Open-Source License & Third-Party Dependency Inventory

## 1. Project License

* **Project**: Final Effect 3D
* **License**: **GNU General Public License, Version 3, 29 June 2007** (`GPL-3.0`)
* **Repository Files**:
  * [`LICENSE`](../LICENSE) — verbatim GNU GPL v3.0 license text (preserved unchanged).
  * [`NOTICE`](../NOTICE) — repository release notice (preserved unchanged).

All original source files created in Stage 1 carry SPDX license headers compatible with the repository's GNU GPL v3.0 license (`SPDX-License-Identifier: GPL-3.0-or-later`).

---

## 2. Clean-Room & Upstream Code Reuse Policy

* Final Effect 3D is an **original codebase**. No source files, headers, shaders, math routines, or project schemas have been copied from Blender or any other third-party 3D application.
* **Important Legal Note on External GPL/Open-Source Creative Suites**: Large open-source creative applications such as Blender contain components under *different* licenses (including `GPL-2.0-only`, `GPL-2.0-or-later`, `GPL-3.0-or-later`, `LGPL`, `Apache-2.0`, `BSD`, `MIT`, and `Zlib`). For example, `GPL-2.0-only` code is **incompatible** with `GPL-3.0`. No external code may be copied into Final Effect 3D without an explicit file-by-file license compatibility and attribution review.

---

## 3. Third-Party Dependency Inventory (Stage 1)

### A. Mandatory Core Engine & Unit Test Dependencies

**None.**

`finaleffect_platform`, `finaleffect_core`, `finaleffect_math`, `finaleffect_scene`, `finaleffect_project`, `finaleffect_rendering`, `finaleffect_app`, and all unit test suites rely exclusively on the **ISO C++20 Standard Library** and standard OS system libraries.

### B. Optional Desktop Windowing & Graphics Dependencies

| Dependency | Version | Source | License | Usage in Final Effect 3D | GPL-3.0 Compatibility & Obligations |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **GLFW** | `>= 3.3` (system) or pinned `3.4` when `FINALEFFECT_FETCH_GLFW=ON` | `https://github.com/glfw/glfw` | **zlib/libpng License** | Optional desktop window, input, and OpenGL 3.3 Core context creation (`finaleffect_window_glfw`) | **Compatible** with GPL-3.0. Source distributions do not require modification notices unless GLFW itself is altered; binary redistributions linking GLFW should retain GLFW's copyright/zlib notice in release documentation. |
| **System OpenGL Runtime** (`libGL` / `opengl32.dll` / macOS `OpenGL.framework`) | Host OS ABI | Host OS / Mesa3D (`https://mesa3d.org`) | System Library / MIT / Khronos | Linked dynamically when `FINALEFFECT_HAS_GLFW=ON` | Covered by the **System Library** definition in GNU GPL v3.0 Section 1; Mesa3D user-space drivers use permissive MIT/X11 licenses compatible with GPL-3.0. |
