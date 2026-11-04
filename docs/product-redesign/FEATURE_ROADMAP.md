# Final Effect 3D — Complete Product Feature Roadmap

**Document ID:** `FE3D-SPEC-12`  
**Stage:** Stage 0 — Product Reset, Original Hybrid UI Design & Architecture Specification  
**Scope:** End-to-end capability matrix across all 20 product domains (Verified Baseline vs. Staged Delivery)

---

## 1. Master Feature Status & Delivery Matrix

Legend:
* **✅ Operational (`v0.5.0` Baseline)** — Exists in C++20 & verified by CTests / E2E today (to be surfaced cleanly in the new Hybrid UI).
* **🔄 Refactored / Upgraded** — Core exists today; redesigned or expanded for Desktop Hybrid architecture.
* **🆕 Planned New Subsystem** — Engineered cleanly in the designated implementation stage.

| Domain | Operational Today (`36aec73` — Verified Baseline) | Planned Hybrid Upgrade (`Stages 1–4`) | Advanced Expansion (`Stages 5–11`) |
| :--- | :--- | :--- | :--- |
| **1. Home / Project Hub** | ✅ Templates (`4`: `StarterShowcase`, `MinimalCube`, `EmptyStage`, `ShaderBall`), Open `.fe3d`/`.fe3dbin`, IndexedDB Recents + WebGL thumbnails | 🔄 **Stage 1**: **Obsidian Slate · Prism Cyan · Solar Amber** Desktop Hub, Instant Search, Pinning, Honest File System Access API + fallback Open/Save, Functional `Preferences (Ctrl+,)`, Honest Recovery/About views | 🆕 **Stage 6–8**: Multi-folder Asset Library catalogs & Autosave ring-buffer vault |
| **2. Studio Shell & Workspaces** | ✅ `WorkspaceManager` presets, 4-slot dock, Outliner/CB tear-off | 🔄 **Stage 2**: Two-Tier `56px` Top Chrome, **Default Left Outliner Dock** (resizable/collapsible/redockable, separate from Right Inspector), Recursive **Studio Area Split Tree**, **`Space` Context Action Menu**, **`Shift+Space` Play/Pause**, **`Ctrl+Space` Maximize**, **`Ctrl+K`/`F3` Command Palette**, Custom Shelves | 🆕 **Stage 11**: Multi-monitor tear-off window sync & layout export packs |
| **3. Viewport & Camera System** | ✅ Persp/Ortho, Orbit/Pan/Zoom, 3D Axis Gizmo, Camera objects, Wire/Shaded/PBR/X-Ray/Isolate | 🔄 **Stage 3**: **True 4-View Quad Split (`glViewport` + `glScissor`)**, independent sub-panel shading, **2D Box Marquee Select**, `FE3D Hybrid` nav profile, **`RMB` 8-Direction Radial Action Compass**, On-Demand sleep | 🆕 **Stage 8**: Safe-frame/gate composition guides, DOF preview, SSR/SSAO |
| **4. 3D Modeling & Mesh Editing** | ✅ Manifold `EditableMesh`, 7 Primitives, Vertex/Edge/Face pick & move, `Extrude`, `Delete`, `Fill Hole` | 🆕 **Stage 4**: **`Bevel`**, **`Bridge`**, **`Insert Edge Loop`**, **`Multi-Cut`**, **`Merge / Target Weld`**, **`Combine` & `Separate`**, Component Rotate/Scale Gizmo, **In-Viewport Operator HUD**, `.obj` I/O | 🆕 **Stage 5**: Boolean ops & Quad Remesh helper |
| **5. Non-Destructive Modifiers** | ❌ None | 🆕 **Stage 4**: Core `ModifierStack` (`Mirror`, `Subdivision Surface`, `Solidify`) | 🆕 **Stage 5**: `Array`, `Bevel Modifier`, `Displace`, `Geometry Nodes` modifier |
| **6. Digital Sculpting** | ❌ None | — | 🆕 **Stage 5**: `MeshBVH` spatial tree, `Draw`, `Clay`, `Smooth`, `Grab`, `Inflate`, `Crease`, `Mask` brushes, XYZ Symmetry, `SculptStrokeCommand` |
| **7. Procedural & Geometry Nodes** | ❌ Foundation placeholder only | — | 🆕 **Stage 5–6**: Shared `NodeGraph` DAG + `GeometryGraphEvaluator` (`Grid/Scatter/Instance/Transform/Math`) + Attribute Spreadsheet |
| **8. UV Editing** | ✅ Per-corner `Vec2` UVs in `EditableMesh` & `.fe3d` | — | 🆕 **Stage 6**: Interactive **2D UV Canvas Editor**, Seam marking, Planar/Box/Cylindrical & Angle-Based Unwrap, Island Transform & Pack, Stretch & Checker overlays |
| **9. Materials & Shader Nodes** | ✅ Cook-Torrance GGX PBR (`Albedo`, `Metallic`, `Roughness`, `AO`, `Emissive`, `Opacity`) + 3D Sphere Preview | 🔄 **Stage 2–3**: Drag-and-drop material onto Viewport/Outliner | 🆕 **Stage 6**: **Shader Node Graph** -> GLSL compiler, Image Texture maps (`Albedo/Normal/ORM`), Procedural `Noise/Voronoi/Checker` nodes, glTF 2.0 I/O |
| **10. Scene Outliner & Layers** | ✅ Hierarchy tree, Parent/Unparent/Group, Visibility, Lock, Rename, Search, Display Layers (`V/P/T/R`) | 🔄 **Stage 2–3**: **Factory-Default Left Dock** (resizable/collapsible/redockable, separate editor from Inspector), Virtualized rows, **Drag-and-Drop Reparenting**, Collections/Filter tags | 🆕 **Stage 8**: View Layer render overrides |
| **11. Inspector & Quick Channels** | ✅ Channel Box (MMB scrub, channel locks, custom attrs) + Attribute Editor tabs | 🔄 **Stage 2**: Separate Right-Docked **Unified Quick Channels + Deep Inspector**, safe inline math expressions (`12/3`, `+=15`), multi-object `Alt+Enter` batch apply | 🆕 **Stage 7**: Driven channel expressions |
| **12. Animation System** | ✅ Timeline, Range Slider, Keyframe ticks, Linear TRS interpolation, Auto-Key | 🔄 **Stage 2**: **`Shift+Space` Play/Pause** & `Space` Animation Context Action Menu | 🆕 **Stage 7**: Per-channel **Bezier `AnimationCurve`**, **Dope Sheet**, **F-Curve Graph Editor**, Motion Paths |
| **13. Rigging & Character** | ✅ Transform hierarchy, custom pivots, channel locks, custom attributes | — | 🆕 **Stage 7**: **Armature / Joint Chains**, **Linear Blend Skinning** (GPU/CPU), Weight Painting, **2-Bone & FABRIK IK**, Constraints, Blendshapes |
| **14. Lighting & Environment** | ✅ Directional/Point/Spot/Area lights, PCF Shadow Atlas, `.hdr`/`.pfm` IBL + SH, Light Linking | 🔄 **Stage 2**: Integrated cleanly into Hybrid Data/World Inspector | 🆕 **Stage 8**: Area shadow softness, Kelvin temperature helper, Volumetric fog |
| **15. Rendering** | ✅ Realtime PBR + Offscreen frame capture (`_fe3d_render_final_frame`) | — | 🆕 **Stage 8**: Dedicated **Render View** (`A/B` wipe, pixel loupe), **Progressive Accumulation TAA (`1..64` samples)**, **AOV Passes** (`Albedo`, `Normal`, `Depth`, `Shadow`), PNG Sequence output |
| **16. Physics & Simulation** | ❌ None | — | 🆕 **Stage 9**: Deterministic **Rigid Body Dynamics**, Passive/Active Colliders, Hinges, **Particle Emitter System**, PBD Cloth, Frame `SimulationCache` |
| **17. Compositing** | ❌ Foundation placeholder only | — | 🆕 **Stage 10**: 2D **Compositor Node Graph** (`Bloom/Glare`, `Exposure/Tonemap`, `Curves`, `Color Balance`, `Vignette`, `Alpha Over`, `Depth Mist`) |
| **18. Video Editing (VSE)** | ❌ Foundation placeholder only | — | 🆕 **Stage 10**: **Multi-Track Video Sequencer** (3D Scene Camera Strips, Image Sequences, Razor Split, Crossfades, Sequence Render) |
| **19. Asset Management** | ✅ In-project Mesh & Material registries | 🔄 **Stage 3**: Drag-to-Viewport grid/surface asset placement | 🆕 **Stage 6–8**: Persistent cross-project User Asset Library & `.fe3d` Asset Bundles |
| **20. Command Bar & Extensibility** | ✅ Safe structured `FE3D` Command Line + `CommandStack` | 🔄 **Stage 1–3**: Centralized `ShortcutRegistry` (remappable + strict input focus guard) + three distinct menus (`Space` Context Action Menu, `Ctrl+K`/`F3` Global Command Palette, `RMB` Radial Compass) + typed `CommandBus` echo | 🆕 **Stage 11**: Macro recorder (sequence of structured `FE3D` commands saved to Shelf button!) |

---

## 2. Dependency Order Rationale

Why does our roadmap order **Shell/Hub (`Stage 1–2`) → Multi-Viewport & Picking (`Stage 3`) → Modeling & Modifiers (`Stage 4`) → Sculpting & Procedural (`Stage 5`) → UV & Shader Nodes (`Stage 6`) → Animation & Rigging (`Stage 7`) → Rendering (`Stage 8`) → Physics (`Stage 9`) → Compositing/VSE (`Stage 10`)**?

1. **Zero Rework on UI Containers**: Building the **Recursive Studio Area Split Tree** and **Design System** in `Stages 1–2` means every subsequent editor (`UV`, `Shader Graph`, `Graph Editor`, `Render View`, `Compositor`, `Sequencer`) mounts into an already-working resizable/tabbed/floatable Studio Area.
2. **Viewport & Selection Before Modeling**: `Stage 4` Modeling (`Bevel`, `Bridge`, `Loop Cut`) relies heavily on `Stage 3`'s **2D Box Marquee Selection**, **Component Normal Gizmo Space**, **Quad 4-View**, and **In-Viewport Operator Adjust HUD**.
3. **Shared `NodeGraph` Kernel Built Once**: Building the C++ `finaleffect::nodes::NodeGraph` DAG + shared UI `NodeCanvas` component in `Stage 5–6` powers **Geometry Nodes**, **Shader Nodes**, and **Compositor Nodes** with a single codebase!
4. **UVs & Textures Before Final Rendering**: `Stage 6` (UV Unwrapping + Image Textures + Shader Graph) feeds directly into `Stage 8` (Progressive Rendering + AOV Passes), which in turn feeds `Stage 10` (Compositing + Video Sequencer).
