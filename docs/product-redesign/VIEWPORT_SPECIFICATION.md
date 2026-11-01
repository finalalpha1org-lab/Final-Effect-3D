# Final Effect 3D — 3D Viewport Architecture Specification

**Document ID:** `FE3D-SPEC-08`  
**Stage:** Stage 0 — Product Reset, Original Hybrid UI Design & Architecture Specification  
**Authoritative Renderer:** C++20 `finaleffect::rendering::Renderer` (`OpenGL 3.3 Core` & `WebGL 2.0`)

---

## 1. Authoritative Single-Source-of-Truth Principle

The 3D Viewport is **100% rendered by the C++20 engine** (`Renderer` -> `IRenderDevice`). JavaScript/HTML (on Desktop Web) and the Desktop UI Chrome (on Native Desktop) provide **only** lightweight peripheral HUD chrome (Area Header, Operator Adjust popover, text labels, 2D marquee rectangle border) over the hardware framebuffer. There is **never** a Three.js/Babylon.js duplicate scene or simulated canvas mockup.

---

## 2. Multi-Viewport Layout Architecture (`Single`, `2-Split`, `True Quad 4-View`)

To deliver true Maya/Blender production capability in Stage 3, `StudioSession` upgrades from a single `OrbitCameraController` to an array of **4 independent `ViewportSubPanel` states** inside any 3D Viewport Area:

```text
┌─────────────────────────────┬─────────────────────────────┐
│ [Top · Ortho · Wireframe]   │ [Persp · Lit PBR · Active]  │  ← 1px #27B6D9 active sub-panel border
│         +X →                │      +Y                     │
│       +Z ↓                  │       │  / -Z               │
│                             │       └─── +X               │
├─────────────────────────────┼─────────────────────────────┤
│ [Front · Ortho · Wireframe] │ [Right · Ortho · Wireframe] │
│         +Y ↑                │         +Y ↑                │
│         └─── +X →           │         └─── -Z →           │
└─────────────────────────────┴─────────────────────────────┘
```

### 2.1 Per-Sub-Panel Independent State (`ViewportSubPanel`)
Each sub-panel (`0..3`) owns its own:
1. **`OrbitCameraController`** (or bound scene `CameraId` when looking through a Scene Camera such as `MainCamera`).
2. **`ProjectionType`** (`Perspective` vs. `Orthographic` with synchronized ortho scale).
3. **`ViewportViewPreset`** (`Free Persp`, `Top +Y`, `Bottom -Y`, `Front +Z`, `Back -Z`, `Right +X`, `Left -X`, `Active Scene Camera`).
4. **`ViewportDisplayOptions`** (so `Top/Front/Right` can run in `Wireframe` + `X-Ray` while `Persp` runs simultaneously in `Lit PBR + Shadows`!).

### 2.2 Single-Pass Multi-Viewport GPU Execution
* Within one frame, C++ `StudioSession::onUpdate`:
  1. Updates the shared `LightingSystem` PCF Shadow Atlas **once** per frame (shared across all lit sub-panels!).
  2. Iterates visible `ViewportSubPanel` rectangles (`1` in Single, `2` in Two-Up, `4` in Quad):
     * Calls `IRenderDevice::setViewportAndScissor(rect.x, rect.y, rect.w, rect.h)` (converting top-left UI coordinates to bottom-left OpenGL framebuffer coordinates `fb_h - (y + h)` multiplied by `devicePixelRatio`).
     * Clears color + depth inside the scissor rect and calls `Renderer::renderScene(...)` with that sub-panel's `ViewportCameraOverride` and `ViewportDisplayOptions`.
* **Quick Maximize Sub-Panel (`Space` tap or `Ctrl+Space`)**: Hovering the mouse over any of the 4 quad sub-panels and tapping the maximize shortcut toggles between that single sub-panel at `100%` of the Viewport Area and the 4-panel split instantly.

---

## 3. Viewport Camera & Navigation Specification

### 3.1 Navigation Profiles (`AppPreferences.navigation_profile`)
| Action | `FE3D Hybrid` (Default) | `Blender Profile` | `Maya Profile` |
| :--- | :--- | :--- | :--- |
| **Orbit / Tumble** | `MMB Drag` **or** `Alt + LMB Drag` | `MMB Drag` | `Alt + LMB Drag` |
| **Track / Pan** | `Shift + MMB Drag` **or** `Alt + MMB Drag` | `Shift + MMB Drag` | `Alt + MMB Drag` |
| **Dolly / Zoom** | `Mouse Wheel` **or** `Ctrl + MMB` **or** `Alt + RMB Drag` | `Wheel` / `Ctrl + MMB` | `Alt + RMB Drag` / `Wheel` |
| **Frame Selected** | `F` or `Numpad .` | `Numpad .` / `F` | `F` |
| **Frame All** | `A` or `Home` | `Home` | `A` |
| **View Axis Gizmo** | Top-Right interactive 3D XYZ widget (click axis cone for Ortho snap; drag widget to orbit; click center/label for Persp↔Ortho) | Same | Same |

### 3.2 Viewport Camera & Lens Controls
* **Field of View (`FOV`)**: `10°–120°` vertical/focal-length (`18mm–200mm` 35mm-equivalent readout), default `45°` (`~50mm` natural modeling lens with minimal perspective distortion).
* **Near / Far Clip Planes**: Default `0.05m .. 1000.0m` (configurable per viewport & per scene camera to eliminate Z-fighting at micro or architectural scales).
* **Look Through Selected / Scene Camera**: Resolves camera frustum with safe-frame / resolution-gate letterbox overlay (`16:9`, `1:1`, custom render resolution) and optional `Lock Camera to View` toggle so navigating frames the actual scene `Camera` object undoably.

---

## 4. Selection, Picking & Transform Gizmos

### 4.1 Object & Component Picking
1. **Point Click (`LMB` release `< 4px` drag)**: Casts world `math::Ray` through the active sub-panel's camera (`picking.cpp` in Object mode; `EditableMesh::pickVertex/Edge/Face` in Component modes), honoring `ObjectSelectionMask` and Display Layer `Template`/`Reference` unpickable flags.
2. **2D Box Marquee Selection (`LMB` drag `>= 4px` in `Select Tool Q`, or `B` modal)**:
   * UI draws a crisp `1px` dashed `#27B6D9` marquee rectangle (`rgba(39, 182, 217, 0.12)` fill) over the sub-panel and passes `(x0, y0, x1, y1, modifier)` to C++ `EditorController::boxSelectViewportRect(...)`.
   * **Object Mode**: Projects object world `AABB` corners / mesh triangles against the 6-plane camera frustum of the screen rectangle.
   * **Component Mode (`Vertex`/`Edge`/`Face`)**: Projects vertices/centroids into sub-panel NDC; when `X-Ray` is **off**, performs visibility ray/depth occlusion check so back-facing occluded vertices are excluded; when `X-Ray` is **on**, selects through the entire mesh!
   * **Modifiers**: `No Mod` = Replace; `Shift` = Add (last or nearest becomes Lead Active); `Ctrl` = Subtract.

### 4.2 3D Transform Gizmos (`TransformGizmo`)
* **Modes**: `Move (W)` (XYZ shafts + XY/XZ/YZ planar quads + view-plane center square), `Rotate (E)` (XYZ gimbal rings + view-aligned outer trackball ring), `Scale (R)` (XYZ shafts + planar scale quads + center uniform cube), `Universal (T)` (combined Move + Rotate arcs + Scale cubes), and `Edit Pivot (D)`.
* **Orientations**: `World`, `Local`, `Component Normal` (average normal of selected faces/vertices in Mesh Edit mode), and `View`.
* **Snapping Integration**: Holding `Ctrl` temporarily inverts the magnet toggle for instant on-the-fly Grid (`X`), Edge (`C`), or Vertex (`V`) snapping during any gizmo drag.

---

## 5. Shading Modes, Overlays & Performance Controls

### 5.1 Four Canonical Viewport Shading Modes (Area Header Pills + `4 / 5 / 6 / 7` & `Z` Pie)
1. **Wireframe (`4`)**: Clean unlit topology lines colored by Display Layer, Object State (`Lead Amber`, `Selected Emerald`, `Standard Slate`), or Component highlight.
2. **Solid Studio (`5`)**: High-readability studio directional lighting / MatCap clay shading (`Default Material` or per-object viewport color) with optional Cavity highlight—ideal for fast Modeling & Sculpting without texture/shadow overhead.
3. **Material Preview / LookDev (`6`)**: Full Cook-Torrance GGX PBR materials illuminated by built-in Studio HDRI IBL (`LightingSystem` spherical harmonics + environment reflection), independent of scene light setup.
4. **Rendered Realtime (`7`)**: Full scene lights (`Directional`, `Point`, `Spot`, `Area`), Light Linking masks, dynamic 2D PCF Shadow Atlas, user `.hdr`/`.pfm` Skybox/IBL, and tone mapping (`ACES Filmic` / `Reinhard`).

### 5.2 Independent Viewport Overlays Popover
Toggleable independently per sub-panel without changing the Shading Mode:
* `Floor Grid & World Axes` (`EditorGrid` infinite adaptive metric grid)
* `Wireframe on Shaded`
* `X-Ray Transparency` (`0.15–0.85` opacity slider)
* `3D Light & Camera Frustum Helpers`
* `Face Orientation` (Blue front-face / Red flipped back-face diagnostic tint for Modeling!)
* `Mesh Smoothness` (`1` Base Cage, `2` Cage + Subdiv, `3` Smooth Preview)
* `Isolate Selected (Shift+I / /)`
* `Statistics HUD` (`FPS`, `Frame ms`, `Objects`, `Verts`, `Faces`, `Tris`, `Shadow Atlas`)

### 5.3 Viewport Performance Controls
* **On-Demand vs. Continuous Redraw**: When neither timeline playback, turntable rig, camera damping, nor pointer drag is active, the viewport sleeps after presenting a clean frame and wakes immediately on input or `stateRevision()` change—cutting idle laptop GPU/battery usage to near `0%` while sustaining locked `60–144 FPS` during interaction.
* **Interactive Degradation Options** (in Viewport Performance popover): Optional `Pause Shadows During Orbit` and `Pixel Scale (100% / 75% / 50%)` for 4K integrated-GPU laptops.
