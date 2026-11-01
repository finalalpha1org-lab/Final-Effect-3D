# Final Effect 3D — Original Hybrid Design System

**Document ID:** `FE3D-SPEC-07`  
**Stage:** Stage 0 — Product Reset, Original Hybrid UI Design & Architecture Specification  
**Design Identity:** **Obsidian Slate · Prism Cyan · Solar Amber**  
**Scope:** Visual tokens, typography, iconography, 17 core UI components, interaction states, density, and accessibility

---

## 1. Visual Identity & Brand Differentiation

Final Effect 3D establishes an unmistakable visual signature engineered for **8+ hour low-fatigue 3D workstation sessions**:

* **Why Not Maya Gray (`#2b2b2b` / `#3b3b3b`)?** Legacy neutral warm-grays flatten hierarchy between headers, docks, and recessed inputs.
* **Why Not Blender Blue-Charcoal (`#303030` / `#4772b3` pill buttons)?** Rounded consumer pills and heavy inner drop-shadows waste horizontal density in numeric matrices.
* **The Final Effect 3D Signature**: Cool-neutral **Obsidian Slate** surfaces (`2–4%` cool blue-slate undertone for crisp contrast against warm lit 3D models) paired with **Prism Cyan (`#27B6D9`)** for active tools/UI focus and **Solar Amber (`#F29D38`)** exclusively for **Active Lead 3D Selection**, giving artists instantaneous peripheral separation between *UI Chrome Focus* (Cyan) and *Scene Selection State* (Amber/Teal).

---

## 2. Color Token System

### 2.1 Surface & Structural Hierarchy
| Token | Hex | Usage |
| :--- | :---: | :--- |
| `--fe-bg-void` | `#0F1115` | Deepest window frame & splitter gutters (`1px–4px` area dividers) |
| `--fe-bg-canvas` | `#161920` | Default 3D Viewport background zenith (subtle gradient to `#1D212A` nadir) |
| `--fe-bg-header` | `#191D24` | Top Application Menu Bar, Area Headers (`26px`), & Footer Status Bar |
| `--fe-bg-panel` | `#1F242D` | Primary Studio Area body (Outliner, Inspector, Timeline) |
| `--fe-bg-elevated` | `#272D38` | Cards, collapsible section headers, inactive buttons, floating popovers |
| `--fe-bg-hover` | `#313947` | Hovered control / tree row surface |
| `--fe-bg-input` | `#14171D` | Recessed numeric fields, text inputs, command bar, & channel cells |
| `--fe-border-hairline` | `#2C3340` | Standard `1px` structural border between controls and sections |
| `--fe-border-strong` | `#3E4859` | Focused popover border, floating window frame, active input outline |

### 2.2 Brand, Selection & Semantic Accents
| Token | Hex | Semantic Meaning |
| :--- | :---: | :--- |
| `--fe-accent-primary` | `#27B6D9` | **Prism Cyan** — Active Tool, Active Workspace Pill, UI Focus Ring, Slider Fill |
| `--fe-accent-primary-hover` | `#3AC6E8` | Hovered primary action / active tool highlight |
| `--fe-select-active` | `#F29D38` | **Solar Amber** — Lead Active 3D Object / Component highlight & Outliner lead badge |
| `--fe-select-secondary` | `#38C695` | **Coral Emerald** — Multi-selected (non-lead) 3D objects in selection set |
| `--fe-state-keyed` | `#D9536F` | Keyframe present on exact current frame (`◆` diamond & subtle `#3A1E26` channel tint) |
| `--fe-state-animated` | `#7E62D9` | Channel has animation curve on another frame (subtle left `2px` bar) |
| `--fe-state-locked` | `#647084` | Locked channel / unpickable Reference layer |
| `--fe-status-success` | `#38C695` | Valid manifold check, save complete, engine ready |
| `--fe-status-warning` | `#E8A63A` | Non-fatal `.fe3d` load warning, non-manifold edge notice, high VRAM |
| `--fe-status-danger` | `#E55353` | Destructive action, command syntax error, shader/context failure |

### 2.3 Canonical 3D Axis & Viewport Grid Colors
* **Axis X**: `#E55353` · **Axis Y**: `#57C469` · **Axis Z**: `#4A90E2`
* **Viewport Ground Grid**: Major `10m` lines `#3A4252`, Unit `1m` lines `#282E3A`, Subdivision `0.1m` lines `#1F242E` (distance-faded via existing analytical shader in `EditorGrid`).

### 2.4 Text Contrast Hierarchy (WCAG AA Compliant on `#1F242D`)
* `--fe-text-bright`: `#F0F3F8` (`13.8:1` contrast — active labels, numeric values, headings)
* `--fe-text-primary`: `#CFD6E2` (`10.2:1` contrast — standard body, menu items, tree names)
* `--fe-text-muted`: `#8C96A8` (`5.1:1` contrast — property labels, units, shortcut hints)
* `--fe-text-disabled`: `#566072` (disabled controls — always paired with tooltip explaining *why* disabled)

---

## 3. Typography, Geometry & Spacing Scale

### 3.1 Typography
* **UI Sans (`--fe-font-sans`)**: `"Inter"`, `-apple-system`, `"Segoe UI"`, `Roboto`, `sans-serif`.
* **Technical / Numeric Mono (`--fe-font-mono`)**: `"JetBrains Mono"`, `"SFMono-Regular"`, `Consolas`, `monospace` with `font-variant-numeric: tabular-nums` enforced on **all** channel inputs, frame counters, coordinates, and telemetry so numbers never jitter horizontally while scrubbing.
* **Type Scale (at `100%` UI Scale)**:
  * `10px` (`600` uppercase, `+0.04em` tracking): Section headers (`QUICK CHANNELS`, `DISPLAY LAYERS`)
  * `11px` (`400`/`500`): Numeric fields, channel labels, status bar, timeline ticks, tree badges
  * `12px` (`400`/`500`): Menus, toolbar buttons, Outliner rows, Inspector labels
  * `14px` / `18px` (`600`): Modal titles & Project Hub section headers

### 3.2 Corner Radius, Borders & Elevation
* **Micro Controls (`3px` radius)**: Buttons, numeric inputs, dropdowns, channel cells, workspace pills.
* **Containers (`4px` radius)**: Context menus, floating operator HUD, Project Hub template cards.
* **Modals & Floating Windows (`6px` radius)**: `1px solid var(--fe-border-strong)` + crisp technical elevation shadow `0 12px 32px rgba(0, 0, 0, 0.58)` (never blurry neon glows).
* **4px Spacing & Sizing Grid**:
  * Padding tokens: `2px`, `4px`, `6px`, `8px`, `12px`, `16px`.
  * Standard row heights: Quick Channel row `20px`; Control/Button height `22px` (`24px` in Hub); Area Header `26px`; Top Bar row `28px`.

### 3.3 Custom Geometric Icon System
* Original `16 × 16` viewBox vector icons (`1.5px` stroke, miter/round joins, pixel-grid aligned).
* **Strict Semantic Color Coding on Icons**:
  * *Neutral (`#CFD6E2`)*: General UI navigation & view icons
  * *Prism Cyan (`#27B6D9`)*: Active modal tools & mesh topology operations
  * *Solar Amber (`#F29D38`)*: Object/Transform origin & pivot icons
  * *Emerald (`#38C695`)*: Scene data blocks (Mesh, Camera, Light)

---

## 4. Specification of All 17 Core UI Components

1. **Menus (`MenuBar` & `MenuDropdown`)**: `28px` trigger bar; dropdown opens on click, switches on hover while open, aligns `[Icon 16px] [Label] [Shortcut Mono Muted]`, supports submenus (`▸`) clamped within window bounds, and closes on `Escape` or outside click.
2. **Toolbars (`ContextStrip` & `AreaHeader`)**: Single-line `26px–28px` flex strips with `1px` vertical dividers; never wrap onto unexpected second lines (graceful overflow menu `[»]` at narrow area widths).
3. **Buttons (`Primary`, `Secondary`, `Ghost`, `Danger`)**: `22px` height in Studio (`28px` in Hub/Modals), `0 8px` horizontal padding, clear `:hover`, `:active` (`1px` press feedback), `:focus-visible` (`1px` `#27B6D9` ring), and `:disabled` states.
4. **Icon Buttons**: `24 × 24px` square hit target (`32 × 32px` in Left Tool Rail), active toggle state uses `rgba(39, 182, 217, 0.18)` fill + `1px solid #27B6D9`.
5. **Dropdowns / Selects**: Custom keyboard-accessible select list (`Up/Down/Enter/Esc` + type-ahead filter when `> 8` items) matching dark theme surfaces across all OSes.
6. **Numeric Scrub Inputs (`ChannelNumberField`)**:
   * Click once to select full text and type numbers or safe arithmetic (`+0.5`, `*2`, `360/8`).
   * Drag horizontally with `LMB` on scrub arrows **or `MMB` anywhere on row label** to scrub continuously (`Ctrl` = `0.01x` fine, `Shift` = `10x` coarse, `Escape` / `RMB` during drag restores initial value!).
   * Optional left `2px` axis color bar (`X` Red, `Y` Green, `Z` Blue).
7. **Sliders (`BoundedRangeField`)**: Combined track + inline editable numeric readout in one `20px` cell (e.g., `Roughness [██████░░░░ 0.42]`), supporting soft min/max (`0..1`) with manual override where physically valid (e.g., `Intensity > 10`).
8. **Color Pickers**: Swatch button displaying split `[sRGB Display | Alpha Checker]`; popover provides `HSV Wheel/Square`, `Linear RGB` vs `sRGB Hex`, `Kelvin Color Temperature` helper (for lights), and scene palette swatches.
9. **Search Fields**: Recessed `#14171D` field with leading magnifier icon, instant `1`-keystroke clear `[×]` button, and `Escape` to clear-then-blur.
10. **Tabs**:
    * *Workspace Pills* (Top Bar): `22px` height with `2px` bottom Prism Cyan indicator when active.
    * *Area / Inspector Tabs*: Compact icon+label tabs supporting middle-click close (on custom tabs) and drag-to-tear-off.
11. **Panel / Area Headers**: `26px` height, `#191D24` background, top-left `[Editor Type ▾]` switcher, active-focus `1px` subtle top highlight when keyboard focus resides inside that Studio Area.
12. **Tree Views (`OutlinerTree`)**: Virtualized `21px` rows (capable of `10,000+` objects at `60 FPS` without DOM bloat), guide lines on hover, drop-target insertion indicators (`above`, `onto-parent`, `below`), and right-aligned action columns.
13. **Property Sections (`AccordionCard`)**: Collapsible `22px` header with chevron, persistent open/closed state per user workspace, `6px` internal vertical gap.
14. **Tooltips**: Delayed `350 ms` hover popover showing: **Title**, **1-sentence functional description**, **Keyboard Shortcut pill**, and **FE3D Command equivalent** (e.g., `Command: bevel`).
15. **Context Menus & 8-Direction Radial Compass**: Stationary `RMB` opens the compact Radial Compass (`110px` radius) + optional secondary linear action list below; cursor movement `> 28px` highlights directional wedge immediately.
16. **Modal Dialogs**: Non-destructive dark backdrop (`rgba(8, 10, 14, 0.62)`), draggable header, `Escape` cancel, explicit primary/cancel footer buttons, focus-trapped for keyboard navigation.
17. **Progress Indicators**: Deterministic progress bars with stage text + item count (used only for real asynchronous tasks: WASM boot, large `.fe3dbin` load, HDRI spherical-harmonics bake, multi-sample render, physics bake).

---

## 5. Core Interaction & Usability Contracts

* **Input Focus Guard**: Whenever focus is inside an `<input>`, `<textarea>`, numeric field, or Command Bar, single-key viewport/tool shortcuts (`Q/W/E/R/S/F/Delete/Space/1..7`) are strictly suppressed so typing object names or expressions never mutates the scene accidentally.
* **Undo/Redo Feedback**: Every undoable mutation updates the footer status pill (`Undid: Extrude Faces`) and marks `ProjectDocument` dirty (`*`). Drag operations (`Gizmo`, `MMB Scrub`, `Color Picker`, `Slider`) coalesce intermediate frames into **one** atomic undo step (`allow_coalesce = true`), while `Escape` mid-drag aborts cleanly back to the pre-drag state.
* **Configurable UI Scale & HiDPI**: Global `--fe-ui-scale` factor (`0.85`, `1.0`, `1.15`, `1.25`, `1.5`) scales typography and row heights cleanly on `1080p`, `1440p`, and `4K` monitors while keeping the 3D Viewport rendered at native `devicePixelRatio` resolution.
