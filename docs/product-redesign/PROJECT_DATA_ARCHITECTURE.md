# Final Effect 3D — Project, Serialization & Asset Data Architecture

**Document ID:** `FE3D-SPEC-10`  
**Stage:** Stage 0 — Product Reset, Original Hybrid UI Design & Architecture Specification  
**Current Production Formats:** `.fe3d` JSON (`v1.0`–`v1.3`) + `.fe3dbin` Binary Geometry Archive (`v1.0`)  
**Coordinate Convention:** `"right-handed-y-up"` (`+X` Right, `+Y` Up, `+Z` Outward)

---

## 1. Non-Breaking Persistence Guarantees

1. **100% Backward Compatibility (`v1.0` .. `v1.3`)**: Every `.fe3d` and `.fe3dbin` file created in earlier versions of Final Effect 3D must load cleanly in all future versions without data loss.
2. **Lossless Unknown-Field Preservation (`ProjectPreservedFields`)**: `ProjectSerializer` (`src/project/project_serializer.cpp`) captures unrecognized JSON keys at root, metadata, scene, editor_state, workspace, material, mesh, and object scopes and writes them back verbatim on save—preventing older builds or external pipelines from stripping newer extension data.
3. **Strict Separation of Scene Content vs. Application Preferences**:
   * **Project File (`.fe3d`)** stores: `metadata`, `scene` (objects, hierarchy, transforms, pivots, locks, custom attributes, display layers, animation tracks, environment/IBL, lights, cameras, materials, meshes/node graphs), `editor_state`, and optional project-embedded `workspace` layout.
   * **User Application Config (`AppPreferences` & `UserWorkspaces`)** stores: Input Navigation Profile, UI Scale, Custom Keymaps, Custom Shelves, Recent Projects History, and Autosave Interval (`AppPaths::userConfigDir()` on Native Desktop; `localStorage` / `IndexedDB` on Desktop Web). Opening someone else's `.fe3d` file **never** overwrites the user's personal keymap or navigation profile, and includes a `"Load UI Layout"` checkbox in the Open dialog (just like Blender!).

---

## 2. Project Lifecycle Operations

### 2.1 Project Creation (`New`)
* Instantiates a fresh `project::ProjectDocument` via `EditorController::createNewProject(ProjectTemplate, name, author)`, resets `CommandStack`, clears active `project_file_path_`, sets `is_dirty_ = false`, and applies the template's initial workspace preset.

### 2.2 Project Opening (`Open`)
* **Validation Pipeline**:
  1. Parse UTF-8 JSON via `project::parseJson` (reports exact line & column on syntax error).
  2. Verify `format == "finaleffect.project"` and supported `version.major == 1`.
  3. If meshes reference external `<stem>.fe3dbin` chunks, load companion `.fe3dbin`, verify magic header `FE3DBIN\0`, version, chunk table bounds, and per-chunk **CRC32** checksums (`src/project/binary_asset.cpp`).
  4. Validate manifold `EditableMesh` polygons (reconstructing from legacy triangle arrays seamlessly for `v1.0`/`v1.1` files) and sanitize dangling IDs, appending human-readable diagnostics to `ProjectDocument::load_warnings`.
  5. Swap into `StudioSession` **atomically**—if deserialization fails at any step, the currently open project in `StudioSession` remains untouched!

### 2.3 Save, Save As & Portable Binary Bundles
* **Inline `.fe3d` Save**: Single self-contained human-readable JSON file—ideal for small/medium scenes and Git version control diffs.
* **Binary Bundle Save (`.fe3d` + `.fe3dbin`)**: Stores lightweight JSON hierarchy/materials in `<name>.fe3d` and deduplicated binary vertex/polygon/UV/HDRI buffers in `<name>.fe3dbin`—mandatory for high-poly Sculpting (`Stage 5`) and large scenes (`10x–25x` smaller and faster to parse than ASCII float arrays).
* **Atomic Write Safety (Native Desktop)**: `saveToFile` writes to `<name>.fe3d.tmp`, flushes stream, and performs an atomic filesystem rename over `<name>.fe3d` so a power loss mid-save never truncates the existing file.
* **Direct Disk Handle Save (Desktop Web)**: Uses the Chromium **File System Access API** (`window.showOpenFilePicker` / `showSaveFilePicker`) when available so **`Ctrl+S` saves directly back to the user's real disk `.fe3d` file** without spawning repetitive `project (1).fe3d` downloads, falling back cleanly to `<a download>` + `IndexedDB` mirror on Firefox/Safari!

---

## 3. Autosave & Crash Recovery Architecture

```text
User Edits Scene (CommandStack push) ──► is_dirty_ = true
                                              │
        Every N mins (default 3m, idle > 2s between drags)
                                              ▼
      Write Rolling Snapshot: recovery/<session_uuid>_slot_{0,1,2}.fe3d
      Update session_lock.json { pid/tab_id, project_name, path, timestamp, clean_exit: false }
                                              │
          ┌───────────────────────────────────┴───────────────────────────────────┐
          ▼ (Normal Quit / Explicit Save)                                         ▼ (Crash / Tab Kill)
  Set clean_exit = true; prune temp slots                    Next Launch detects clean_exit == false
                                                             ──► Home Hub surfaces Recovery Banner!
```

* Never overwrites the user's primary `.fe3d` file silently.
* Never triggers an autosave in the middle of an active pointer/gizmo/sculpt drag (`!session.isInteractiveDragActive()`).

---

## 4. Schema Versioning & Migration Plan (`v1.3` → `v1.4+`)

| Schema Version | Introduced In | Added / Migrated Fields | Backward/Forward Strategy |
| :---: | :---: | :--- | :--- |
| `1.0` – `1.1` | Early Core | Scene hierarchy, transforms, triangle meshes, cameras, directional light | Upgraded automatically on load (`EditableMesh::fromMeshData`). |
| `1.2` | Stage 4–5.4 | `EditableMesh` polygons/UVs, `PbrMaterial`, Point/Spot/Area lights, Light Linking, HDRI/IBL, 4-slot `workspace` | Fully supported. |
| `1.3` | Current (`main`) | Pivots, node/channel locks, custom attributes, `display_layers`, `timeline` & TRS `animation_tracks` | Current default output format. |
| **`1.4`** *(Planned)* | New Stage 2–4 | Recursive `workspace.area_tree` (`AreaNode` split layout) + `object.modifiers` stack (`Subdiv`, `Mirror`, `Array`, `Bevel`) + `mesh.seam_edges` | Loader upgrades `1.2/1.3` 4-slot `workspace` into equivalent `AreaNode` split tree automatically; `1.3` readers preserve unknown `1.4` keys via `ProjectPreservedFields` with a version warning. |
| **`1.5`** *(Planned)* | New Stage 6–7 | `node_graphs` (`shader_graphs`, `geometry_graphs`), Bezier `animation_curves`, `armatures` & `skin_clusters` | Large skinning weight & sculpt displacement buffers stored as typed chunks (`SKIN`, `SCLPT`) inside `.fe3dbin` `v1.1`. |

---

## 5. External Assets, Missing Resource Handling & Import/Export

### 5.1 Asset Reference Modes
External binary resources (`.hdr`/`.pfm` environment maps, PBR albedo/normal/roughness PNG/JPG textures, linked `.fe3d` component assets) support two explicit storage modes:
1. **Relative Path Reference** (`assets/textures/wood_albedo.png` relative to `.fe3d` directory on Native Desktop).
2. **Packed Bundle Chunk** (`chunk://sha256-...` embedded inside `.fe3dbin`—default on Desktop Web so projects remain 100% portable across machines and browsers!).

### 5.2 Missing Asset Diagnostics & Non-Destructive Fallback
If an external texture or `.fe3dbin` chunk is missing on load:
* **Never fails the entire project open** if scene hierarchy is intact: substitutes a unmistakable procedural **Magenta/Slate Missing-Resource Checker** in GPU rendering, preserves the original `asset_path` string on the material/mesh untouched (so saving does not erase the path!), and populates the **Missing External Files Resolver** (`File -> External Data -> Find Missing Files…`) to relink by folder scan.

### 5.3 Standard Interchange Import / Export Roadmap
* **Current (`v0.5.0`)**: Native `.fe3d` (`v1.0–v1.3`), `.fe3dbin` (`v1.0`), Radiance `.hdr` & `.pfm` HDRI maps, `.ppm`/`.png` frame output.
* **Planned Interchange (`Stages 4 & 6`)**:
  * **Wavefront `.obj` + `.mtl`** (zero-dependency C++20 parser/writer for universal quad/n-gon polygon & UV interchange).
  * **glTF 2.0 (`.gltf` / `.glb`)** (industry-standard Metallic-Roughness PBR, hierarchy, camera, skinning, and animation interchange).
