# Final Effect 3D — Complete 13-Editor Architecture Specification

**Document ID:** `FE3D-SPEC-06`  
**Stage:** Stage 0 — Product Reset, Original Hybrid UI Design & Architecture Specification  
**Scope:** Full architectural & UX specification for all 13 Studio Editors & Workspaces

---

## 1. Shared Editor Framework Contract

Every editor in Final Effect 3D implements a common architectural interface (`IEditorDescriptor` in UI + C++ domain state/controller in `finaleffect::app`) rather than ad-hoc scripts:

* **Shared Authoritative Scene**: Switching editors or workspaces **never** copies or serializes/deserializes the scene. All 13 editors inspect and mutate the single live `project::ProjectDocument` & `scene::Scene` through `scene::CommandStack`.
* **Uniform Area Header (`26px`)**: Top-left Editor Type selector (`▾`), mode/view controls on the left, local search/filter or snapping in the center, and area split/float/maximize controls on the right.
* **Honest Stage Availability**: Editors already backed by C++ (`Layout`, `Modeling` base, `Shading` PBR, `Animation` timeline, `Rendering` realtime/capture, `Asset/Scene Management` outliner/layers) are active immediately; editors requiring new C++ subsystems (`Sculpting`, `UV Canvas`, `Shader Node Graph`, `Geometry Nodes`, `Rigging/Armature`, `Physics`, `Compositor`, `Video Sequencer`) ship progressively across Stages 4–10 with zero fake tools.

---

## 2. Specification of All 13 Editors

---

### Editor 1: Layout / General (`workspace: layout`)
* **Purpose**: Scene assembly, object blocking, camera staging, lighting placement, and general look-through inspection.
* **Main UI Layout**: Left Tool Rail (`44px`) | Center `3D Viewport` (`70%` width, Single or Quad 4-View) above `Timeline` (`108px` height) | Right stacked `Scene Outliner` (`32%` height) + `Quick Channels & Inspector` (`68%` height).
* **Tool Organization**: `Select (Q)`, `Move (W)`, `Rotate (E)`, `Scale (R)`, `Universal (T)`, `Edit Pivot (D)`, plus Quick Shelf primitives (`Cube`, `Sphere`, `Cylinder`, `Cone`, `Torus`, `Plane`, `Camera`, `Lights`, `Empty`).
* **Menus**: `Object`, `Create`, `Select`, `View`, `Shading`, `Display`, `Panels`.
* **Tool Settings**: Gizmo Orientation (`World`/`Local`/`View`), Pivot (`Active`/`Median`/`BBox`), Grid/Vertex/Edge Snapping (`Step`, `Angle Step`), Selection Masks.
* **Properties**: Full Quick Channels + Object Transform, Display Layer, Assigned Material summary, Camera/Light parameters.
* **Keyboard Shortcuts**: `Q/W/E/R/T/D`, `F` Frame Selection, `A` Frame All, `Ctrl+D` Duplicate, `Ctrl+G` Group, `Ctrl+P` / `Alt+P` Parent/Unparent, `H` Hide, `Shift+H` Show, `4/5/6/7` Shading modes, `Ctrl+Space` Maximize Area.
* **Mouse Interactions**: `LMB` Click/Box-Select & Gizmo drag; `MMB` / `Alt+LMB` Orbit; `Shift+MMB` / `Alt+MMB` Pan; `Wheel` / `Alt+RMB` Zoom; `RMB` Radial Compass.
* **Context Menus**: `Duplicate`, `Delete`, `Group`, `Parent/Unparent`, `Center Pivot`, `Freeze Transforms`, `Assign Material ▸`, `Move to Layer ▸`, `Isolate Select`.
* **Engine Integration**: `StudioSession`, `EditorController`, `TransformGizmo`, `OrbitCameraController`, `Scene`, `picking`. *(Operational today; Quad 4-View & Box Marquee added in Stage 3.)*
* **Dependencies**: Core `scene`, `rendering`, `app`.
* **Expected Future Functionality**: Multi-object snap alignment tools, collection instances (`Duplicate Linked`), bounding-box drop-to-ground helper.

---

### Editor 2: Modeling (`workspace: modeling`)
* **Purpose**: Direct topological polygon mesh creation, component editing (`Vertex`, `Edge`, `Face`), precision surface beveling/bridging/cutting, and non-destructive modifiers.
* **Main UI Layout**: Left Modeling Tool Rail (`44px` or `120px` labeled) | Full-height Center `3D Viewport` with Wireframe-on-Shaded enabled by default + bottom-left **Operator Adjust HUD** | Right `Outliner` (`24%`) + `Quick Channels & Geometry/Modifier Inspector` (`76%`) — *Timeline collapsed by default to maximize vertical modeling space*.
* **Tool Organization**:
  * *Transform*: `Select/Marquee (Q)`, `Move (W)`, `Rotate (E)`, `Scale (R)`
  * *Topology*: `Extrude (Ctrl+E)`, `Bevel (Ctrl+B)`, `Bridge`, `Insert Edge Loop (Ctrl+R)`, `Multi-Cut (K)`, `Target Weld`, `Fill Hole`, `Slide Edge`.
* **Menus**: `Mesh` (`Combine`, `Separate`, `Boolean`, `Fill Hole`, `Cleanup`), `Edit Mesh` (`Extrude`, `Bevel`, `Bridge`, `Merge`, `Delete Elements`), `Mesh Tools` (`Insert Edge Loop`, `Multi-Cut`, `Target Weld`), `Normals` (`Shade Smooth`, `Shade Flat`, `Recalculate Outside`, `Flip`).
* **Tool Settings**: Auto-Merge vertices threshold, Symmetry axis (`X`/`Y`/`Z`), Soft/Proportional falloff radius & curve, Keep Faces Together toggle, Normal/Component transform orientation.
* **Properties**: Mesh topology statistics (`Verts`, `Edges`, `Faces`, `Tris`, `Open Boundaries`, `Manifold Status`), Component coordinate readouts, Non-Destructive **Modifier Stack** (`Subdivision Surface`, `Mirror`, `Array`, `Solidify`, `Bevel Modifier` — Stage 4/5).
* **Keyboard Shortcuts**: `Tab` or `F8` Object↔Edit toggle; `1/2/3` (or `F9/F10/F11`) Vertex/Edge/Face sub-modes; `Ctrl+E` Extrude; `Ctrl+B` Bevel; `Ctrl+R` Loop Cut; `K` Multi-Cut; `M` Merge menu; `Alt+Click` Edge/Face Loop select; `Delete` Component delete/dissolve menu.
* **Mouse Interactions**: Component raycast & 2D marquee selection (front-facing vs. X-Ray through-mesh), interactive gizmo Transform/Rotate/Scale around component centroid/normal, interactive drag distance/fraction on Operator HUD.
* **Context Menus**: Mode-sensitive RMB menu (`Vertex`: Merge, Connect, Chamfer; `Edge`: Bevel, Bridge, Loop Cut, Mark Seam, Crease; `Face`: Extrude, Inset, Poke, Separate, Assign Material).
* **Engine Integration**: `scene::EditableMesh`, `scene::ModifyMeshCommand`, `Renderer` in-place `updateMesh`. *(Primitives, Component Pick, Move, Extrude, Delete, Fill Hole operational today; Bevel, Bridge, Loop Cut, Multi-Cut, Weld, Combine/Separate, Component Rotate/Scale scheduled in Stage 4.)*
* **Dependencies**: `EditableMesh` half-edge/polygon manifold kernel, `TransformGizmo`, `CommandStack`.
* **Expected Future Functionality**: Catmull-Clark subdivision preview (`Smooth 3`), quad-remesh helper, boolean union/difference/intersect.

---

### Editor 3: Sculpting (`workspace: sculpting`)
* **Purpose**: Organic high-resolution surface sculpting using dynamic brush strokes, spatial BVH acceleration, symmetry, and masking.
* **Main UI Layout**: Left `Brush Shelf` (`64px` visual brush icons) | Maximized Center `3D Viewport` (`82%` width, MatCap / Clay studio shading, Timeline & Outliner collapsed by default) | Compact Right `Brush & Stroke Inspector` (`260px`).
* **Tool Organization**: `Draw`, `Clay Strips`, `Inflate`, `Blob`, `Crease`, `Smooth (Shift-hold)`, `Flatten`, `Grab`, `Snake Hook`, `Pinch`, `Mask`, `Box Trim`.
* **Menus**: `Sculpt` (`Symmetry`, `Rebuild Subdivisions`, `Voxel Remesh`), `Brush`, `Stroke`, `Mask` (`Invert`, `Clear`, `Smooth Mask`, `Extract Masked Mesh`).
* **Tool Settings** *(Top Context Bar)*: `Radius [F-drag]` (`px` or world units), `Strength [Shift+F-drag]` (`0.0–1.0`), `Add / Subtract (Ctrl-hold)`, `Mirror X / Y / Z` radial toggles, `Falloff Curve`, `Stroke Method` (`Dots`, `Space`, `Anchored`, `Stabilize Stroke`).
* **Properties**: Remesh voxel size, Multiresolution subdivision level, MatCap clay shading override, Dyntopo / adaptive detail toggle.
* **Keyboard Shortcuts**: `F` interactive brush radius resize; `Shift+F` brush strength; `Shift` (hold) temporary `Smooth` brush; `Ctrl` (hold) invert brush direction; `Alt+M` clear mask; `X/G/S/C` quick brush hotkeys.
* **Mouse Interactions**: `LMB` stroke raycasts cursor onto spatial `MeshBVH` surface, accumulating dab displacements within brush radius and streaming modified vertex normals/positions to GPU via `glBufferSubData`; on `pointerup`, commits a compressed delta `SculptStrokeCommand` to `CommandStack`. *(Supports pressure sensitivity via W3C `PointerEvent.pressure` on desktop pen tablets!)*
* **Context Menus**: `RMB` quick brush radius/strength/falloff popup.
* **Engine Integration**: New `scene::SculptSystem` + `math::MeshBVH` + `SculptStrokeCommand` (**Stage 5**).
* **Dependencies**: `EditableMesh`, spatial BVH octree/BVH-tree, fast partial normal recomputation, `IRenderDevice::updateMesh`.
* **Expected Future Functionality**: Voxel remeshing, alpha stamp textures, face sets, multiresolution displacement layers.

---

### Editor 4: UV Editing (`workspace: uv_editing`)
* **Purpose**: Unwrapping 3D polygon meshes into distortion-free 2D texture coordinates (`FaceCorner::uv`), seam authoring, island packing, and texel-density inspection.
* **Main UI Layout**: `50% / 50%` Side-by-Side Split — Left Area: **2D UV Canvas Editor** (`[0,1]` UV grid + UDIM tile overlay + texture backdrop) | Right Area: **3D Viewport** (in `Edge`/`Face` mode with UV Checker map shading active) | Far-Right `UV & Unwrap Inspector` (`280px`).
* **Tool Organization**:
  * *3D Viewport*: `Mark Seam`, `Clear Seam`, `Planar / Cylindrical / Spherical / Box Project`, `Unwrap`.
  * *2D UV Canvas*: `UV Select (Vertex / Edge / Face / Island)`, `UV Move (W)`, `UV Rotate (E)`, `UV Scale (R)`, `Pin UV (P)`, `Relax`, `Pack Islands`.
* **Menus**: `UV` (`Unwrap Angle-Based / Conformal`, `Smart UV Project`, `Box/Planar/Cylindrical Project`, `Seams from Islands`), `Select` (`Select Island [L]`, `Select Overlapped`, `Select Flipped`), `UV Modify` (`Align X/Y`, `Stitch`, `Split`, `Pack Islands`, `Normalize`).
* **Tool Settings**: `UV Sync Selection` (mirrors 3D↔2D selection), Snap to Pixels / Grid, Island Margin (`0.005`), Stretch Overlay (`Angle` / `Area` heatmap), Checkerboard resolution (`512`, `1024`, `2048`).
* **Properties**: Selected UV vertex `(U, V)` coordinates, Texel Density (`px/m`), Texture image backdrop selector.
* **Keyboard Shortcuts**: `U` Unwrap menu; `L` Select linked UV island under cursor; `P` / `Alt+P` Pin / Unpin UVs; `Ctrl+E -> Mark Seam`.
* **Mouse Interactions**: `MMB` / `Alt+MMB` 2D canvas pan; `Wheel` cursor-centered 2D zoom; `LMB` select/marquee & 2D UV gizmo transform.
* **Context Menus**: `Unwrap`, `Straighten UV Loop`, `Align U / Align V`, `Flip Horizontal / Vertical`, `Pack`.
* **Engine Integration**: `EditableMesh` (`FaceCorner::uv` + new `seam_edges` bitset + C++ `uv::Unwrapper` & `ModifyUvCommand` in **Stage 6**).
* **Dependencies**: `EditableMesh`, 2D orthographic canvas renderer (WebGL2/GL33 or hardware-accelerated canvas overlay), PBR texture sampling.
* **Expected Future Functionality**: Multi-tile UDIM workflow, interactive live-unwrap on seam cut.

---

### Editor 5: Shading / Material Editor (`workspace: shading`)
* **Purpose**: Authoring physically based materials, procedural/image texture graphs, and studio HDRI look-development.
* **Main UI Layout**: Top Half (`55%` height): `3D Viewport` in **Lit PBR + Studio HDRI** mode | Bottom Half (`45%` height): **Shader Node Graph Editor** (`Stage 6`) alongside `Asset/Material Shelf` | Right: `Material & World Inspector` with live `96×96` 3D Sphere/Cube/ShaderBall preview.
* **Tool Organization**: Node Graph tools (`Select`, `Cut Link [Ctrl+RMB drag]`, `Reroute`, `Frame Nodes`, `Add Node [Shift+A]`).
* **Menus**: `Material` (`New`, `Duplicate`, `Assign to Selection`, `Remove Unused`), `Add Node` (`Input`, `Texture`, `Color`, `Vector/Normal`, `BSDF Shader`, `Math`, `Output`), `Environment` (`HDRI Presets`, `Load .hdr/.pfm`).
* **Tool Settings**: Auto-compile shader graph toggle, Preview mesh shape (`Sphere`, `Rounded Cube`, `Cylinder`), HDRI rotation quick slider, Snap nodes to grid.
* **Properties**: `PbrMaterial` channels (`Base Color`, `Metallic`, `Roughness`, `IOR`, `Specular`, `Normal Strength`, `AO`, `Emissive`, `Opacity` / Alpha Mode) + texture map slots (`Albedo`, `Normal`, `ORM`, `Emissive`).
* **Keyboard Shortcuts**: `Shift+A` Add Shader Node search; `F` Frame selected nodes; `M` Mute/Bypass node; `Ctrl+G` Group nodes; `Delete` Delete node; `X` Disconnect.
* **Mouse Interactions**: Port-to-port bezier noodle drag with socket type color coding (`Yellow`: Color, `Gray`: Float, `Purple`: Vector/Normal, `Emerald`: Shader BSDF), `MMB` pan, `Wheel` zoom, box-select nodes.
* **Context Menus**: `Add Node…`, `Duplicate Nodes`, `Preview Node Output on Mesh (Ctrl+Shift+LMB)`, `Reset Material`.
* **Engine Integration**: `scene::PbrMaterial`, `MaterialPreviewRenderer`, `EnvironmentLighting` *(operational today)* + Stage 6 C++ `ShaderGraph` -> GLSL 330 / GLSL ES 3.00 code generator.
* **Dependencies**: `rendering::LightingSystem`, `IRenderDevice` dynamic material texture/shader compilation.
* **Expected Future Functionality**: Clearcoat, Subsurface approximation, Triplanar projection node, procedural Noise/Voronoi/Brick GLSL nodes.

---

### Editor 6: Geometry Nodes (`workspace: geometry_nodes`)
* **Purpose**: Non-destructive, procedural DAG geometry generation, scattering, instancing, and attribute math integrated into an object's modifier pipeline.
* **Main UI Layout**: Top Area (`55%`): `3D Viewport` + `Geometry Spreadsheet` (`Vertex / Edge / Face / Instance` attribute table, `25%` width) | Bottom Area (`45%`): **Geometry Node Graph Editor** | Right: `Modifier & Exposed Inputs Inspector`.
* **Tool Organization**: `Select`, `Add Node (Shift+A)`, `Cut Noodle`, `Group`, `Pin Graph`.
* **Menus**: `Add` (`Primitives`, `Mesh Operations`, `Transform`, `Instances / Scatter`, `Curve`, `Attributes`, `Utilities / Math`, `Group Input/Output`), `Node`.
* **Tool Settings**: Evaluation timing overlay (`ms` per node badge), Freeze/Bake procedural output to static `EditableMesh`.
* **Properties**: Exposed `Group Input` parameters rendered automatically as clean numeric sliders/toggles in the Object's Modifier Inspector!
* **Keyboard Shortcuts**: `Shift+A` Node search, `M` Mute node, `Alt+Shift+LMB` Viewer node inspect into Spreadsheet.
* **Mouse Interactions**: Shared Node Canvas component (identical pan/zoom/noodle UX as Shader & Compositor graphs; `Teal` Geometry sockets, `Blue` Int/Bool, `Gray` Float, `Purple` Vec3).
* **Context Menus**: `Bake to Editable Mesh`, `Expose Socket to Inspector`, `Group Selected`.
* **Engine Integration**: C++ `procedural::GeometryGraphEvaluator` operating on `EditableMesh` & instance transforms (**Stage 5–6**), reusing the unified `NodeGraph` data model.
* **Dependencies**: `EditableMesh`, `NodeGraph` core, `ModifierStack`.
* **Expected Future Functionality**: Point-cloud Poisson scatter, curve-to-mesh sweep, procedural booleans.

---

### Editor 7: Animation (`workspace: animation`)
* **Purpose**: Keyframe authoring, timing adjustment (Dope Sheet), Bezier curve tangent polishing (Graph Editor), and real-time camera/character playback.
* **Main UI Layout**: Top-Left: `Camera Shot Preview` (`30%` width, locked to Active Camera without overlays) | Top-Center: Working `3D Viewport` (`48%`) | Right: `Quick Channels & Inspector` (`22%`) | Bottom (`38%` height): Tabbed **Timeline Transport + Dope Sheet + F-Curve Graph Editor**.
* **Tool Organization**: Keyframe `Select / Box`, `Move Keys (G/W)`, `Scale Keys (R)` (time-stretch), `Insert Key (I / S)`, `Tangent Handle Edit`.
* **Menus**: `Playback` (`Play/Stop`, `Audio Scrub`, `Frame Dropping`, `Loop / Once / Ping-Pong`), `Key` (`Insert Keyframe`, `Delete Keyframes`, `Interpolation Mode ▸ Constant / Linear / Bezier`, `Handle Type ▸ Auto Clamped / Vector / Free`), `Channel` (`Extrapolation ▸ Constant / Linear / Cyclic`).
* **Tool Settings**: `Auto Key` toggle, Keying Set filter (`Location`, `Rotation`, `Scale`, `Available`, `Custom Attrs`), Snap Keys (`Nearest Frame`, `Second`), Normalize Curves toggle (in Graph Editor).
* **Properties**: Selected keyframe exact `(Frame, Value)` numeric fields, Left/Right Bezier handle coordinates, FPS (`24`, `25`, `30`, `60`), Scene Start/End & Playback Sub-Range.
* **Keyboard Shortcuts**: `Space` (or `Alt+V` per user keymap) Play/Pause; `S` (or `I`) Insert Keyframe; `,` / `.` Previous/Next Keyframe; `Left` / `Right` Step 1 frame; `Shift+Left` / `Shift+Right` Jump to Start/End.
* **Mouse Interactions**: Drag playhead to scrub C++ scene at `60 Hz`; box-select & drag keyframe diamonds in Dope Sheet; drag Bezier tangent handles in F-Curve Graph Editor with axis-colored curves (`X`: `#EF5350`, `Y`: `#66BB6A`, `Z`: `#42A5F5`).
* **Context Menus**: `Insert Key`, `Duplicate Keys`, `Delete Keys`, `Tangent -> Auto Clamped / Linear / Stepped`.
* **Engine Integration**: `scene::TimelineSettings`, `ObjectAnimationTrack` *(linear TRS operational today; upgraded to per-channel Bezier `AnimationCurve` + Dope Sheet + Graph Editor in **Stage 7**)*.
* **Dependencies**: `Scene`, `CommandStack`, `Quick Channels` keyframe status sync.
* **Expected Future Functionality**: Motion Paths in 3D Viewport, Ghosting / Onion-skinning, NLA Action clips.

---

### Editor 8: Rigging (`workspace: rigging`)
* **Purpose**: Skeletal joint hierarchy creation, skin weight binding/painting, inverse kinematics (IK), constraints, and custom rig control attributes.
* **Main UI Layout**: Left Rigging Tool Rail | Center `3D Viewport` (with X-Ray Joints overlay enabled) | Left-Inner `Scene & Joint Outliner` (`220px`) | Right `Quick Channels, Constraints & Skin Weights Inspector` (`320px`).
* **Tool Organization**: `Create Joint Tool`, `Insert Joint`, `Orient Joint`, `Pose Transform (W/E/R)`, `Paint Skin Weights`, `Edit Pivot (D)`.
* **Menus**: `Skeleton` (`Create Joint`, `Mirror Joints`, `Orient Joint`, `Connect IK Handle`), `Skin` (`Bind Skin — Smooth Heat/Distance`, `Detach Skin`, `Normalize Weights`, `Prune Small Weights`, `Copy Skin Weights`), `Constrain` (`Parent`, `Point`, `Orient`, `Aim / Track To`, `Pole Vector`).
* **Tool Settings**: Joint display radius, Symmetry axis, Weight Paint `Weight (0..1)` / `Add / Subtract / Smooth` / `Auto-Normalize`.
* **Properties**: Joint `Bind Pose` matrix, Preferred IK angles, Rotation Order (`XYZ`, `ZXY`, `YZX`), Channel Locks (hide/lock unused rig channels), Custom Control Attributes (`IK_FK_Blend`, `FootRoll`).
* **Keyboard Shortcuts**: `Ctrl+Tab` Object ↔ Pose Mode; `Alt+G / Alt+R / Alt+S` Reset Pose to Rest/Bind Pose; `P` / `Shift+P` Parent / Unparent joints.
* **Mouse Interactions**: Click-place joint chains in orthographic/perspective views; paint influence heatmap (`Blue 0.0 -> Green 0.5 -> Red 1.0`) directly on mesh vertices; pose IK/FK controllers in real time.
* **Context Menus**: `Assume Bind Pose`, `Select Hierarchy`, `Lock Non-Keyable Channels`, `Add Constraint ▸`.
* **Engine Integration**: Existing hierarchy, pivots, channel locks & custom attributes *(operational today)* + new C++ `scene::ArmatureComponent`, `SkinCluster`, GPU/CPU Linear Blend Skinning, & 2-Bone/FABRIK IK Solver (**Stage 7**).
* **Dependencies**: `Scene` hierarchy, `EditableMesh`, vertex shader bone palette / CPU skinning fallback, `Animation` curves.
* **Expected Future Functionality**: Blendshapes / Morph Targets, Driven Keys (attribute-to-attribute expressions).

---

### Editor 9: Rendering (`workspace: rendering`)
* **Purpose**: High-quality frame rendering, progressive sampling/anti-aliasing, AOV pass inspection (`Beauty`, `Albedo`, `Normal`, `Roughness`, `Depth`, `Shadow`), tone mapping, and image sequence output.
* **Main UI Layout**: Center **Render View Framebuffer** (`65%` width, 1:1 pixel zoom/pan, A/B snapshot wipe comparison, color-picker pixel inspector) | Top-Right mini `3D Camera/Light Viewport` (`35%`) | Bottom-Right `Render, Output & Environment Settings Inspector`.
* **Tool Organization**: `Render Region (Crop Marquee)`, `Pan/Zoom Render Buffer`, `Pixel EV/RGB Loupe`, `A/B Snapshot Slider`.
* **Menus**: `Render` (`Render Frame [F12]`, `Render Animation Sequence…`, `Cancel Render [Esc]`), `View` (`Channel: RGBA / RGB / Alpha / Luminance`, `Pass: Combined / Albedo / Normal / Depth / Shadows`, `Fit to Window`, `1:1 100%`), `Image` (`Save PNG…`, `Store in Slot A / B`).
* **Tool Settings**: Render Resolution (`1920×1080`, `2560×1440`, `3840×2160`, `%` scale `25%–200%`), Temporal Supersampling / TAA samples (`1..64`), Shadow Atlas resolution (`1024..4096`), Tone Mapper (`ACES Filmic`, `AgX-inspired Neutral`, `Reinhard`, `Linear`), Exposure & Gamma.
* **Properties**: Active Camera gate/aspect ratio, Film Transparent Background (`RGBA`), Environment HDRI & IBL controls, Light Linking matrix.
* **Keyboard Shortcuts**: `F12` Render Current Frame; `F11` Toggle Render View; `1..5` Switch Render Slots; `H` Fit Image; `J` Swap A/B Slot.
* **Mouse Interactions**: `MMB` pan image, `Wheel` zoom (`10%–1600%` nearest-neighbor pixel grid at `>400%`), `LMB` drag A/B comparison divider or sample `(X, Y, R, G, B, Luma)` under cursor.
* **Context Menus**: `Save Image (PNG)…`, `Copy Render Diagnostics`, `Clear Render Region`.
* **Engine Integration**: `Renderer`, `LightingSystem`, `_fe3d_render_final_frame` *(single-frame PBR + shadow render operational today; multi-sample progressive accumulation, AOV debug passes, transparent film & A/B Render Buffer added in **Stage 8**)*.
* **Dependencies**: `IRenderDevice` offscreen FBO readback (`glReadPixels` / `SoftwareRenderDevice`).
* **Expected Future Functionality**: Screen-Space Reflections (SSR), Screen-Space Ambient Occlusion (SSAO), offline ray-traced path-tracing backend option.

---

### Editor 10: Compositing (`workspace: compositing`)
* **Purpose**: Non-destructive post-render image & AOV pass compositing (Bloom/Glare, Color Grading, Vignette, Depth Mist, Alpha Over, Chroma/Luma Key) via a 2D node graph.
* **Main UI Layout**: Top Area (`55%`): Live **Composited Backdrop / Split Viewer** | Bottom Area (`45%`): **Compositor Node Graph** | Right: `Active Compositor Node & Color Management Inspector`.
* **Tool Organization**: Shared `NodeGraph` tools (`Select`, `Add Node [Shift+A]`, `Cut Link`, `Viewer Connect [Ctrl+Shift+LMB]`, `Split Wipe`).
* **Menus**: `Add` (`Input -> Render Layers / Image`, `Color -> Exposure / Curves / Color Balance / Tonemap`, `Filter -> Glare/Bloom / Blur / Sharpen`, `Matte`, `Transform`, `Output -> Composite / File Output`).
* **Tool Settings**: `Use Nodes` master toggle, Proxy Preview Resolution (`Full`, `Half`, `Quarter`), Auto-recomposite on edit.
* **Properties**: Selected post-processing node parameters (e.g., Bloom threshold/knee/radius, Lift/Gamma/Gain color wheels).
* **Keyboard Shortcuts**: `Shift+A` Add Node, `M` Mute/Bypass node, `V` / `Alt+V` Zoom backdrop, `Ctrl+Shift+LMB` route node to Viewer.
* **Mouse Interactions**: Identical to Shader/Geometry Node Canvas + draggable A/B Viewer split line on the image preview.
* **Context Menus**: `Connect to Viewer`, `Mute`, `Duplicate`, `Reset Node Values`.
* **Engine Integration**: C++ `compositor::CompositorGraph` executing ping-pong fullscreen GLSL shaders (or CPU tile fallback) over rendered `RenderLayer` RGBA16F/RGBA8 buffers (**Stage 10**).
* **Dependencies**: `NodeGraph` core, `Rendering` AOV FBO outputs.
* **Expected Future Functionality**: Cryptomatte/Object-ID mask pass, lens distortion, LUT (`.cube`) loader.

---

### Editor 11: Physics / Simulation (`workspace: physics`)
* **Purpose**: Authoring, caching, and debugging Rigid Body dynamics, collision shapes, gravity/force fields, cloth/soft-body springs, and particle emitters synchronized with the Scene Timeline.
* **Main UI Layout**: Center `3D Viewport` (with collision hull & force-vector wireframe overlays) | Bottom `Simulation Cache Timeline` (`Baked Frame Bar` indicator) | Right `Physics & Cache Inspector`.
* **Tool Organization**: `Select`, `Transform`, `Add Force Field (Wind / Turbulence / Vortex)`, `Connect Rigid Body Constraint (Hinge / Fixed / Point)`.
* **Menus**: `Physics` (`Add Active Rigid Body`, `Add Passive Collider`, `Add Particle Emitter`, `Bake All Dynamics`, `Free All Bakes`, `Apply Visual Transform to Scene`).
* **Tool Settings**: Simulation Substeps per Frame (`1..16`), Solver Iterations, Scene Gravity `(0, -9.81, 0)`, Collision Margin visualization toggle.
* **Properties**: Rigid Body (`Active`/`Passive`, `Mass`, `Friction`, `Restitution/Bounciness`, `Collision Shape: Box / Sphere / Capsule / Convex Hull / Triangle Mesh`), Particle System (`Emission Rate`, `Lifetime`, `Initial Velocity`, `Instance Mesh`), Frame Cache (`Start`, `End`, `Memory Footprint`).
* **Keyboard Shortcuts**: `Space` Run/Step live simulation on timeline; `Ctrl+Alt+B` Bake simulation cache.
* **Mouse Interactions**: Standard 3D viewport selection + interactive force-field radius/direction handles.
* **Context Menus**: `Set Active Rigid Body`, `Set Passive Ground Collider`, `Bake to Keyframes`, `Clear Cache`.
* **Engine Integration**: Deterministic C++20 `physics::PhysicsWorld` + `SimulationCache` integrated into `StudioSession::onUpdate` (**Stage 9**).
* **Dependencies**: `Scene`, `TimelineSettings`, `EditableMesh` convex/AABB bounds.
* **Expected Future Functionality**: Position-Based Dynamics (PBD) cloth solver, GPU instanced particle billboarding/meshes.

---

### Editor 12: Video Editing (`workspace: video_editing`)
* **Purpose**: Assembling rendered 3D camera shots, image sequences, title cards, transitions, and audio reference tracks on a non-linear multi-track timeline (`Video Sequence Editor`).
* **Main UI Layout**: Top-Left `Media / Scene Shot Bin` (`25%`) | Top-Center `Program Video Preview Monitor` (`50%`, with safe-area guides & waveform/scopes option) | Top-Right `Strip Inspector` (`25%`) | Bottom (`46%` height): **Multi-Track Sequencer Timeline**.
* **Tool Organization**: `Select Strip (Q)`, `Blade / Razor Cut (K)`, `Slip Strip Contents (S)`, `Crossfade Transition`.
* **Menus**: `Add Strip` (`3D Scene Camera Strip`, `Image / Sequence`, `Color Solid`, `Crossfade`), `Strip` (`Split [K]`, `Trim Start/End`, `Mute [H]`, `Snap to Playhead`), `Sequence Render`.
* **Tool Settings**: Ripple Edit toggle, Snap to Strip Edges / Markers, Proxy resolution, Waveform display.
* **Properties**: Strip `Start Frame`, `Duration`, `Channel`, `Blend Mode` (`Alpha Over`, `Cross`, `Add`, `Multiply`), `Opacity`, `Transform (Pos/Scale/Crop)`, `Fade In/Out`.
* **Keyboard Shortcuts**: `Space` Play/Pause, `J / K / L` Shuttle Reverse / Pause / Forward, `C` or `K` Razor Split at Playhead, `[` / `]` Trim In/Out.
* **Mouse Interactions**: Drag strips across tracks, drag strip left/right trim handles, scrub playhead, zoom timeline horizontally with `Ctrl+Wheel`.
* **Context Menus**: `Split at Playhead`, `Add Crossfade`, `Link/Unlink Strips`, `Delete (Ripple)`.
* **Engine Integration**: C++ `sequencer::SequenceTimeline` capable of referencing live `Scene` cameras as **Scene Strips** plus rendered image frames (**Stage 10**).
* **Dependencies**: `Renderer`, `TimelineSettings`, image I/O.
* **Expected Future Functionality**: Export frame sequence / WebCodecs MP4/WebM muxing on Desktop Web & FFmpeg pipe optional adapter on Native Desktop.

---

### Editor 13: Asset / Scene Management (`area: asset_browser` & `scene_outliner`)
* **Purpose**: Managing reusable project and library assets (Meshes, PBR Materials, Node Groups, Lighting Rigs, HDRIs) alongside heavy-scene organization (Display Layers, Collections, Orphan Data cleanup, Missing File relinking).
* **Main UI Layout**: Can run as a dedicated bottom/side **Asset Browser Area** (`Left Category Tree` + `Thumbnail Grid` + `Asset Metadata Preview`) OR full-screen **Asset & Scene Manager Workspace**.
* **Tool Organization**: `Browse`, `Drag-to-Viewport Place` (snaps asset onto 3D surface or ground grid under cursor!), `Mark Selected as Asset`, `Purge Unused Data-Blocks`.
* **Menus**: `Asset` (`Mark as Asset`, `Clear Asset`, `Regenerate Thumbnail`, `Export Asset Bundle (.fe3d)`), `Library` (`Current Project`, `Built-In Studio Library`, `User Library Folders`), `Scene Cleanup` (`Purge Unused Meshes/Materials`, `External Files Report`).
* **Tool Settings**: Thumbnail size (`Small 64px`, `Medium 96px`, `Large 144px`), Import mode (`Append Independent Copy` vs. `Instance Mesh`), Snap-to-Surface on drop.
* **Properties**: Asset Name, Category Tags, Author, License, Polygon/Texture budget stats, Dependency list.
* **Keyboard Shortcuts**: `Ctrl+F` Search assets, `Delete` Remove custom user asset, `F2` Rename.
* **Mouse Interactions**: Drag Material card directly onto any 3D object in the Viewport (or Outliner row) to assign `material_id` immediately via undoable `AssignMaterialCommand`; drag Mesh/Light preset onto the 3D ground grid to spawn at exact world ray-grid intersection!
* **Context Menus**: `Spawn at World Origin`, `Assign to Selected Objects`, `Duplicate`, `Export Selected Asset…`.
* **Engine Integration**: `Scene` mesh/material registries (`scene.meshes()`, `scene.materials()`), `MaterialPreviewRenderer`, `ProjectSerializer` *(in-project asset tables operational today; drag-and-drop viewport placement & persistent multi-project User Asset Library added in **Stages 3 & 6**)*.
* **Dependencies**: `ProjectSerializer`, `LocalFileSystem` / `IndexedDB` library store, `Renderer` thumbnail generator.
* **Expected Future Functionality**: Tag-based smart collections, external `.fe3d` reference linking with non-destructive transform overrides.
