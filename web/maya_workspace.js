// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Autodesk Maya Classic Stage 1 Workspace, Menu Sets, Shelves,
// Channel Box, Display Layer Editor, Time/Range Slider, Hotbox, Marking Menu & Safe Command Line

(function () {
  "use strict";

  // Enable unified JS Maya Shortcut & Hotbox Router so C++ Html5CanvasWindow avoids duplicate key handling.
  window.__FE3D_MAYA_SHORTCUT_ROUTER__ = true;

  const REG = window.FE3D_REGISTRY || { ICONS: {} };
  const SHELVES_STORAGE_KEY = "fe3d.maya_shelves.v1";
  const HOTKEYS_STORAGE_KEY = "fe3d.maya_hotkeys.v1";
  const MENUSET_STORAGE_KEY = "fe3d.maya_menuset.v1";
  const CMD_HISTORY_KEY = "fe3d.maya_cmd_history.v1";

  // =========================================================================
  // 1. CONTEXT-AWARE MAYA MENU SETS (Modeling, Rigging, Animation, FX, Rendering + Arnold)
  // =========================================================================
  const MENU_SETS = {
    modeling: [
      {
        id: "ms_mesh",
        label: "Mesh",
        items: [
          { label: "Combine / Group Selected", kbd: "Ctrl+G", cmd: "group" },
          { label: "Fill Hole (Boundary Loop)", cmd: "fill_hole" },
          { label: "Select Open Boundary Loop", cmd: "select_boundary" },
          { sep: true },
          { label: "Freeze Transformations", cmd: "freeze_transforms" },
          { label: "Center Pivot", cmd: "center_pivot" },
        ],
      },
      {
        id: "ms_edit_mesh",
        label: "Edit Mesh",
        items: [
          { label: "Extrude Selected Faces", cmd: "extrude_faces" },
          { label: "Duplicate Object", kbd: "Ctrl+D", cmd: "duplicate" },
          { label: "Delete Elements / Object", kbd: "Del", cmd: "smart_delete" },
        ],
      },
      {
        id: "ms_mesh_tools",
        label: "Mesh Tools",
        items: [
          { label: "Object Mode", kbd: "F8", cmd: "mode_object" },
          { label: "Vertex Component Tool", kbd: "F9", cmd: "mode_vertex" },
          { label: "Edge Component Tool", kbd: "F10", cmd: "mode_edge" },
          { label: "Polygon Face Tool", kbd: "F11", cmd: "mode_face" },
          { sep: true },
          { label: "Open Tool Settings", cmd: "focus_tool_settings" },
        ],
      },
      {
        id: "ms_mesh_display",
        label: "Mesh Display",
        items: [
          { label: "Base Cage Display", kbd: "1", cmd: "smooth_1" },
          { label: "Cage + Wireframe Display", kbd: "2", cmd: "smooth_2" },
          { label: "Smooth Preview Display", kbd: "3", cmd: "smooth_3" },
          { sep: true },
          { label: "Toggle Wireframe on Shaded", cmd: "toggle_wos" },
        ],
      },
      {
        id: "ms_uv",
        label: "UV",
        items: [
          { label: "UV Editor (Stage 1 Foundation)", cmd: "open_uv_editor" },
          { label: "Inspect Mesh UV & Topology", cmd: "focus_shape_tab" },
        ],
      },
    ],
    rigging: [
      {
        id: "ms_skeleton",
        label: "Skeleton",
        items: [
          { label: "Create Joint / Locator Pivot", preset: "locator" },
          { label: "Create Rig Group", kbd: "Ctrl+G", cmd: "group" },
          { label: "Center Joint / Object Pivot", cmd: "center_pivot" },
          { label: "Edit Pivot Manipulator", kbd: "D", cmd: "edit_pivot" },
        ],
      },
      {
        id: "ms_skin",
        label: "Skin",
        items: [
          { label: "Freeze Bind Pose Transforms", cmd: "freeze_transforms" },
          { label: "Reset Bind Pose Transforms", cmd: "reset_transforms" },
        ],
      },
      {
        id: "ms_deform",
        label: "Deform",
        items: [
          { label: "Add Custom Rig Attribute…", cmd: "open_add_attr" },
          { label: "Lock Selected Node", cmd: "lock_node" },
          { label: "Unlock Selected Node", cmd: "unlock_node" },
        ],
      },
      {
        id: "ms_constrain",
        label: "Constrain",
        items: [
          { label: "Parent Hierarchy", kbd: "P", cmd: "parent" },
          { label: "Unparent to World", kbd: "Shift+P", cmd: "unparent" },
          { label: "Select Hierarchy", cmd: "select_hierarchy" },
        ],
      },
    ],
    animation: [
      {
        id: "ms_key",
        label: "Key",
        items: [
          { label: "Set Keyframe", kbd: "S", cmd: "set_keyframe" },
          { label: "Delete Keyframe at Current Frame", cmd: "delete_keyframe" },
          { sep: true },
          { label: "Toggle Auto Keyframe", cmd: "toggle_autokey" },
        ],
      },
      {
        id: "ms_playback",
        label: "Playback",
        items: [
          { label: "Play / Pause Timeline", cmd: "toggle_play" },
          { label: "Go to Start of Range", cmd: "goto_start" },
          { label: "Step Back One Frame", cmd: "step_prev" },
          { label: "Step Forward One Frame", cmd: "step_next" },
          { label: "Go to End of Range", cmd: "goto_end" },
        ],
      },
      {
        id: "ms_visualize",
        label: "Visualize",
        items: [
          { label: "Toggle Showcase Turntable Rig", cmd: "toggle_rig" },
          { label: "Show Time & Range Slider", cmd: "open_timeline" },
        ],
      },
    ],
    fx: [
      {
        id: "ms_fields",
        label: "Fields/Solvers",
        items: [
          { label: "Create Emitter Locator", preset: "locator" },
          { label: "Create Directional Field Light", preset: "light" },
          { label: "Play FX Timeline", cmd: "toggle_play" },
        ],
      },
      {
        id: "ms_effects",
        label: "Effects",
        items: [
          { label: "Bifrost / Geometry Graph (Foundation)", cmd: "open_geonodes_editor" },
          { label: "Create Surface Plane Collider", preset: "plane" },
        ],
      },
    ],
    rendering: [
      {
        id: "ms_lighting_shading",
        label: "Lighting/Shading",
        items: [
          { label: "Create New Standard PBR Material", cmd: "create_new_material" },
          { label: "Open PBR Material Attribute Tab", cmd: "focus_material_editor" },
          { sep: true },
          { label: "Use All Lights", kbd: "7", cmd: "lighting_all" },
          { label: "Use Default Headlamp Lighting", cmd: "lighting_default" },
          { label: "Toggle Use Default Material", cmd: "toggle_default_mat" },
        ],
      },
      {
        id: "ms_render",
        label: "Render",
        items: [
          { label: "Render Current Frame…", cmd: "render_current_frame" },
          { label: "Environment & HDRI IBL Settings", cmd: "focus_render_tab" },
          { label: "Render Compositor (Foundation)", cmd: "open_compositor_editor" },
        ],
      },
      {
        id: "ms_arnold",
        label: "Arnold",
        items: [
          { label: "Arnold-Compatible RenderView…", cmd: "render_current_frame" },
          { sep: true },
          { label: "Lights -> Area Light (Quad/Disk)", preset: "area_light" },
          { label: "Lights -> Photometric Spot Light", preset: "spot_light" },
          { label: "Lights -> SkyDome Studio Softbox IBL", cmd: "hdri_softbox" },
          { label: "Lights -> SkyDome Golden Sunset IBL", cmd: "hdri_sunset" },
          { sep: true },
          { label: "Inspect Shadow Atlas & Light Linking", cmd: "focus_render_tab" },
        ],
      },
    ],
  };

  // =========================================================================
  // 2. DEFAULT MAYA SHELVES (Persisted independently of .fe3d scene data)
  // =========================================================================
  function getDefaultShelves() {
    return [
      {
        id: "general",
        name: "General",
        builtin: true,
        items: [
          { id: "gen_sel", label: "Select", icon: "select", tool: "select", help: "Select Tool (Q)" },
          { id: "gen_mov", label: "Move", icon: "move", tool: "move", help: "Move Tool (W)" },
          { id: "gen_rot", label: "Rotate", icon: "rotate", tool: "rotate", help: "Rotate Tool (E)" },
          { id: "gen_scl", label: "Scale", icon: "scale", tool: "scale", help: "Scale Tool (R)" },
          { id: "gen_grp", label: "Group", icon: "outliner", cmd: "group", help: "Group Selected Objects (Ctrl+G)" },
          { id: "gen_frz", label: "Freeze", icon: "objectProps", cmd: "freeze_transforms", help: "Freeze Transformations" },
          { id: "gen_cpv", label: "CenterPiv", icon: "pivot", cmd: "center_pivot", help: "Center Pivot" },
        ],
      },
      {
        id: "poly_modeling",
        name: "Poly Modeling",
        builtin: true,
        items: [
          { id: "pm_sph", label: "Sphere", icon: "sphere", preset: "sphere", help: "Create Polygon Sphere" },
          { id: "pm_cub", label: "Cube", icon: "cube", preset: "cube", help: "Create Polygon Cube" },
          { id: "pm_cyl", label: "Cylinder", icon: "cylinder", preset: "cylinder", help: "Create Polygon Cylinder" },
          { id: "pm_con", label: "Cone", icon: "cone", preset: "cone", help: "Create Polygon Cone" },
          { id: "pm_tor", label: "Torus", icon: "torus", preset: "torus", help: "Create Polygon Torus" },
          { id: "pm_pln", label: "Plane", icon: "plane", preset: "plane", help: "Create Polygon Plane" },
          { id: "pm_ext", label: "Extrude", icon: "face", cmd: "extrude_faces", help: "Extrude Selected Polygon Faces" },
          { id: "pm_fil", label: "FillHole", icon: "vertex", cmd: "fill_hole", help: "Fill Open Boundary Loop" },
          { id: "pm_frz", label: "Freeze", icon: "objectProps", cmd: "freeze_transforms", help: "Freeze Transformations" },
        ],
      },
      {
        id: "sculpting",
        name: "Sculpting",
        builtin: true,
        items: [
          { id: "sc_sph", label: "QuadSph", icon: "sphere", preset: "sphere", help: "Create Sculpt Base Sphere" },
          { id: "sc_vtx", label: "Vertex", icon: "vertex", cmd: "mode_vertex", help: "Vertex Edit Mode (F9)" },
          { id: "sc_sm3", label: "Smooth3", icon: "sculpt", cmd: "smooth_3", help: "Smooth Display Level (3)" },
          { id: "sc_wos", label: "WireShd", icon: "edge", cmd: "toggle_wos", help: "Toggle Wireframe on Shaded" },
        ],
      },
      {
        id: "rigging",
        name: "Rigging",
        builtin: true,
        items: [
          { id: "rg_loc", label: "Locator", icon: "pivot", preset: "locator", help: "Create Joint / Locator Node" },
          { id: "rg_grp", label: "Group", icon: "outliner", cmd: "group", help: "Group Hierarchy (Ctrl+G)" },
          { id: "rg_par", label: "Parent", icon: "character", cmd: "parent", help: "Parent to Lead Selection (P)" },
          { id: "rg_unp", label: "Unparent", icon: "character", cmd: "unparent", help: "Unparent to World Root (Shift+P)" },
          { id: "rg_piv", label: "EditPiv", icon: "pivot", cmd: "edit_pivot", help: "Edit Pivot Tool (D)" },
          { id: "rg_att", label: "+Attr", icon: "channelBox", cmd: "open_add_attr", help: "Add Custom Rig Attribute" },
        ],
      },
      {
        id: "animation",
        name: "Animation",
        builtin: true,
        items: [
          { id: "an_key", label: "SetKey", icon: "keyframe", cmd: "set_keyframe", help: "Set Keyframe at Current Frame (S)" },
          { id: "an_dky", label: "DelKey", icon: "timeline", cmd: "delete_keyframe", help: "Delete Keyframe at Current Frame" },
          { id: "an_auto", label: "AutoKey", icon: "play", cmd: "toggle_autokey", help: "Toggle Auto Keyframe Recording" },
          { id: "an_ply", label: "Play", icon: "play", cmd: "toggle_play", help: "Play / Pause Animation Timeline" },
          { id: "an_cam", label: "Camera", icon: "camera", preset: "camera", help: "Create Shot Camera" },
        ],
      },
      {
        id: "uv_editing",
        name: "UV Editing",
        builtin: true,
        items: [
          { id: "uv_ed", label: "UV View", icon: "assets", cmd: "open_uv_editor", help: "Open UV Texture Editor Panel" },
          { id: "uv_tex", label: "Textured", icon: "material", cmd: "shading_textured", help: "Shaded & Textured Mode (6)" },
          { id: "uv_fce", label: "Faces", icon: "face", cmd: "mode_face", help: "Select Polygon Faces (F11)" },
        ],
      },
      {
        id: "shading",
        name: "Shading",
        builtin: true,
        items: [
          { id: "sh_new", label: "+PBR Mat", icon: "material", cmd: "create_new_material", help: "Create Cook-Torrance PBR Material" },
          { id: "sh_ae", label: "Mat AE", icon: "shade", cmd: "focus_material_editor", help: "Open Material Attribute Editor" },
          { id: "sh_sfb", label: "Softbox", icon: "sceneData", cmd: "hdri_softbox", help: "Studio Softbox HDRI IBL" },
          { id: "sh_sun", label: "Sunset", icon: "light", cmd: "hdri_sunset", help: "Golden Sunset HDRI IBL" },
        ],
      },
      {
        id: "rendering",
        name: "Rendering",
        builtin: true,
        items: [
          { id: "rn_dir", label: "DirLight", icon: "light", preset: "light", help: "Create Directional Light" },
          { id: "rn_pnt", label: "PtLight", icon: "light", preset: "point_light", help: "Create Point Light" },
          { id: "rn_spt", label: "SpotLgt", icon: "light", preset: "spot_light", help: "Create Spot Light" },
          { id: "rn_are", label: "AreaLgt", icon: "light", preset: "area_light", help: "Create Area Light (Quad/Disk)" },
          { id: "rn_frm", label: "Render", icon: "renderFrame", cmd: "render_current_frame", help: "Render Clean Final Frame" },
        ],
      },
      {
        id: "fx",
        name: "FX",
        builtin: true,
        items: [
          { id: "fx_emit", label: "Emitter", icon: "pivot", preset: "locator", help: "Create FX Emitter Locator" },
          { id: "fx_col", label: "Collider", icon: "plane", preset: "plane", help: "Create Ground Collider Plane" },
          { id: "fx_sim", label: "SimPlay", icon: "play", cmd: "toggle_play", help: "Play FX Timeline" },
        ],
      },
    ];
  }

  // =========================================================================
  // 3. CUSTOMIZABLE MAYA HOTKEY DEFINITIONS
  // =========================================================================
  const DEFAULT_HOTKEYS = [
    { id: "tool_select", label: "Select Tool", key: "KeyQ", ctrl: false, shift: false },
    { id: "tool_move", label: "Move Tool", key: "KeyW", ctrl: false, shift: false },
    { id: "tool_rotate", label: "Rotate Tool", key: "KeyE", ctrl: false, shift: false },
    { id: "tool_scale", label: "Scale Tool", key: "KeyR", ctrl: false, shift: false },
    { id: "tool_manip", label: "Show Manipulator", key: "KeyT", ctrl: false, shift: false },
    { id: "tool_last", label: "Repeat Last Tool", key: "KeyY", ctrl: false, shift: false },
    { id: "tool_pivot", label: "Edit Pivot Tool", key: "KeyD", ctrl: false, shift: false },
    { id: "frame_sel", label: "Frame Selection", key: "KeyF", ctrl: false, shift: false },
    { id: "frame_all", label: "Frame All Scene", key: "KeyA", ctrl: false, shift: false },
    { id: "shade_wire", label: "Wireframe Display", key: "Digit4", ctrl: false, shift: false },
    { id: "shade_smooth", label: "Smooth Shaded Display", key: "Digit5", ctrl: false, shift: false },
    { id: "shade_tex", label: "Shaded & Textured PBR", key: "Digit6", ctrl: false, shift: false },
    { id: "shade_lights", label: "Use All Lights Toggle", key: "Digit7", ctrl: false, shift: false },
    { id: "set_key", label: "Set Keyframe", key: "KeyS", ctrl: false, shift: false },
    { id: "group_sel", label: "Group Selected", key: "KeyG", ctrl: true, shift: false },
    { id: "duplicate_sel", label: "Duplicate Selected", key: "KeyD", ctrl: true, shift: false },
    { id: "parent_sel", label: "Parent Selected", key: "KeyP", ctrl: false, shift: false },
    { id: "unparent_sel", label: "Unparent Selected", key: "KeyP", ctrl: false, shift: true },
  ];

  // =========================================================================
  // STATE
  // =========================================================================
  let activeMenuSet = "modeling";
  let shelves = [];
  let activeShelfId = "poly_modeling";
  let hotkeys = [];
  let activeRightMode = "split"; // "split" | "cb" | "ae" | "tools"
  let activeLayerId = 0;
  let outlinerDockedLeft = false;
  let commandHistory = [];
  let commandHistoryIdx = -1;
  let lastCbSignature = "";
  let lastLayersSignature = "";
  let lastTimelineSignature = "";
  let spacebarHeld = false;
  let lastMouseClient = { x: 640, y: 400 };
  const floatingPanels = new Map(); // panelId -> { placeholder, originalParent, winEl }

  function loadStoredJson(key, fallback) {
    try {
      const raw = window.localStorage.getItem(key);
      if (!raw) return fallback;
      return JSON.parse(raw);
    } catch (_) {
      return fallback;
    }
  }

  function saveStoredJson(key, val) {
    try {
      window.localStorage.setItem(key, JSON.stringify(val));
    } catch (_) {}
  }

  function studio() {
    return window.__FE3D_STUDIO__ || null;
  }

  function dispatch(payload) {
    const s = studio();
    if (!s || !s.dispatch) return false;
    return s.dispatch(payload);
  }

  function state() {
    const s = studio();
    return s && s.getState ? s.getState() : null;
  }

  function isTextEditingElement(el) {
    if (!el || !el.tagName) return false;
    const tag = el.tagName.toUpperCase();
    if (tag === "TEXTAREA" || tag === "SELECT") return true;
    if (el.isContentEditable) return true;
    if (tag === "INPUT") {
      const t = (el.type || "text").toLowerCase();
      return t !== "button" && t !== "checkbox" && t !== "radio" && t !== "range" && t !== "color";
    }
    return false;
  }

  function setHelpLine(msg) {
    const el = document.getElementById("maya-help-line");
    if (el && msg) el.textContent = msg;
  }

  // =========================================================================
  // CENTRAL MAYA COMMAND EXECUTOR (Used by Menus, Shelves, Hotbox, Marking Menu & Hotkeys)
  // =========================================================================
  function executeMayaCommand(cmd) {
    const s = studio();
    if (!s) return;
    s.closeAllMenus();
    s.closeContextMenu();
    hideShelfPopup();
    hideHotbox();

    const st = state();
    const vp = (st && st.viewport) || {};
    const ed = (st && st.editor) || {};
    const insp = st ? st.inspector : null;
    const tl = (st && st.timeline) || {};

    switch (cmd) {
      case "repeat_last_tool":
        dispatch({ action: "activate_last_tool" });
        break;
      case "duplicate":
        dispatch({ action: "duplicate_selected" });
        break;
      case "duplicate_special":
        if (dispatch({ action: "duplicate_selected" })) {
          const after = state();
          if (after && after.inspector) {
            const p = after.inspector.position || [0, 0, 0];
            dispatch({
              action: "set_transform",
              id: after.inspector.id,
              position: [p[0] + 1.5, p[1], p[2]],
              rotation_deg: after.inspector.rotation_deg || [0, 0, 0],
              scale: after.inspector.scale || [1, 1, 1],
            });
          }
        }
        break;
      case "smart_delete":
        if (ed.mode && ed.mode !== "object") {
          dispatch({ action: "delete_selected_elements" });
        } else {
          dispatch({ action: "delete_selected" });
        }
        break;
      case "group":
      case "create_empty_group":
        if (cmd === "create_empty_group" && !insp) {
          dispatch({ action: "add_preset", preset: "locator" });
        } else {
          dispatch({ action: "group_selected" });
        }
        break;
      case "parent":
        dispatch({ action: "parent_selected" });
        break;
      case "unparent":
        dispatch({ action: "unparent_selected" });
        break;
      case "select_all":
        dispatch({ action: "select_all" });
        break;
      case "invert_selection":
        dispatch({ action: "invert_selection" });
        break;
      case "select_hierarchy":
        dispatch({ action: "select_hierarchy" });
        break;
      case "select_similar":
        dispatch({ action: "select_similar" });
        break;
      case "mode_object":
        dispatch({ action: "set_editor_mode", mode: "object" });
        break;
      case "mode_vertex":
        dispatch({ action: "set_editor_mode", mode: "vertex" });
        break;
      case "mode_edge":
        dispatch({ action: "set_editor_mode", mode: "edge" });
        break;
      case "mode_face":
        dispatch({ action: "set_editor_mode", mode: "face" });
        break;
      case "freeze_transforms":
        dispatch({ action: "freeze_transforms" });
        break;
      case "reset_transforms":
        if (insp) dispatch({ action: "reset_transform", id: insp.id });
        break;
      case "center_pivot":
        dispatch({ action: "center_pivot" });
        break;
      case "edit_pivot":
        dispatch({ action: "set_active_tool", tool: "pivot" });
        break;
      case "lock_node":
        if (insp) dispatch({ action: "set_object_locked", id: insp.id, locked: true });
        break;
      case "unlock_node":
        if (insp) dispatch({ action: "set_object_locked", id: insp.id, locked: false });
        break;
      case "open_add_attr": {
        setRightEditorMode("cb");
        s.setRightDockCollapsed(false);
        const bar = document.getElementById("maya-add-attr-bar");
        if (bar) {
          bar.hidden = false;
          const inp = document.getElementById("cb-new-attr-name");
          if (inp) inp.focus();
        }
        break;
      }
      case "frame_selection":
        dispatch({ action: "frame_selection" });
        break;
      case "frame_all":
        dispatch({ action: "frame_all" });
        break;
      case "home_view":
        dispatch({ action: "reset_viewport_home_view" });
        break;
      case "select_active_camera":
        if (vp.active_camera_id) dispatch({ action: "select_object", id: vp.active_camera_id });
        break;
      case "hide_selection":
        if (insp) dispatch({ action: "set_visibility", id: insp.id, visible: false });
        break;
      case "show_selection":
        dispatch({ action: "execute_command_line", command: "showHidden" });
        break;
      case "toggle_isolate":
        dispatch({ action: "set_isolate_select", enabled: !vp.isolate_select });
        break;
      case "toggle_wos":
        dispatch({ action: "set_viewport_display", wireframe_on_shaded: !vp.wireframe_on_shaded });
        break;
      case "toggle_xray":
        dispatch({ action: "set_viewport_display", xray_mode: !vp.xray_mode });
        break;
      case "toggle_default_mat":
        dispatch({ action: "set_viewport_display", default_material: !vp.default_material });
        break;
      case "lighting_default":
        dispatch({ action: "set_viewport_display", use_all_lights: false });
        break;
      case "lighting_all":
        dispatch({ action: "set_viewport_display", use_all_lights: true });
        break;
      case "shading_textured":
        dispatch({ action: "set_viewport_display", shading_mode: "shaded_textured" });
        break;
      case "smooth_1":
        dispatch({ action: "set_viewport_display", mesh_display_smoothness: 1 });
        break;
      case "smooth_2":
        dispatch({ action: "set_viewport_display", mesh_display_smoothness: 2 });
        break;
      case "smooth_3":
        dispatch({ action: "set_viewport_display", mesh_display_smoothness: 3 });
        break;
      case "extrude_faces": {
        const distEl = document.getElementById("mesh-extrude-dist");
        const dist = distEl ? Number(distEl.value) || 0.35 : 0.35;
        dispatch({ action: "extrude_selected_faces", distance: dist });
        break;
      }
      case "fill_hole":
        dispatch({ action: "fill_selected_hole" });
        break;
      case "select_boundary":
        dispatch({ action: "select_boundary_loop" });
        break;
      case "set_keyframe":
        dispatch({ action: "set_keyframe" });
        break;
      case "delete_keyframe":
        dispatch({ action: "delete_keyframe" });
        break;
      case "toggle_autokey":
        dispatch({ action: "set_timeline", auto_key: !tl.auto_key });
        break;
      case "toggle_play":
        dispatch({ action: "set_timeline_playing", playing: !tl.playing });
        break;
      case "goto_start":
        dispatch({ action: "set_current_frame", frame: Number(tl.playback_start) || 1 });
        break;
      case "goto_end":
        dispatch({ action: "set_current_frame", frame: Number(tl.playback_end) || 120 });
        break;
      case "step_prev":
        dispatch({ action: "set_current_frame", frame: Math.round((Number(tl.current_frame) || 1) - 1) });
        break;
      case "step_next":
        dispatch({ action: "set_current_frame", frame: Math.round((Number(tl.current_frame) || 1) + 1) });
        break;
      case "toggle_rig":
        dispatch({ action: "set_rig_playing", playing: !vp.rig_playing });
        break;
      case "create_new_material":
        dispatch({ action: "create_material", name: "aiStandardSurface", base_color: [0.78, 0.8, 0.84] });
        setRightEditorMode("ae");
        s.setInspectorCategory("material");
        break;
      case "hdri_softbox":
        dispatch({ action: "load_hdri_preset", preset: "studio_softbox" });
        break;
      case "hdri_sunset":
        dispatch({ action: "load_hdri_preset", preset: "warm_sunset" });
        break;
      case "render_current_frame":
        openRenderViewModal();
        break;
      case "focus_outliner": {
        s.setRightDockCollapsed(false);
        dispatch({ action: "set_panel_visible", panel: "scene_outliner", visible: true });
        break;
      }
      case "focus_channel_box":
        s.setRightDockCollapsed(false);
        setRightEditorMode("cb");
        break;
      case "focus_attribute_editor":
        s.setRightDockCollapsed(false);
        setRightEditorMode("ae");
        break;
      case "focus_tool_settings":
        s.setRightDockCollapsed(false);
        setRightEditorMode("tools");
        break;
      case "focus_material_editor":
        s.setRightDockCollapsed(false);
        setRightEditorMode("ae");
        s.setInspectorCategory("material");
        break;
      case "focus_shape_tab":
        s.setRightDockCollapsed(false);
        setRightEditorMode("ae");
        s.setInspectorCategory("modifiers");
        break;
      case "focus_render_tab":
        s.setRightDockCollapsed(false);
        setRightEditorMode("ae");
        s.setInspectorCategory("render");
        break;
      case "open_timeline":
        s.setBottomEditor("timeline");
        break;
      case "open_uv_editor":
        s.setBottomEditor("uv_foundation");
        break;
      case "open_geonodes_editor":
        s.setBottomEditor("geonodes_foundation");
        break;
      case "open_compositor_editor":
        s.setBottomEditor("compositor_foundation");
        break;
      case "float_outliner":
        floatPanel("panel-outliner", "Outliner");
        break;
      case "float_channel_box":
        floatPanel("panel-channel-box", "Channel Box / Layer Editor");
        break;
      case "redock_all_panels":
        redockAllFloatingPanels();
        break;
      case "cmd_help":
        dispatch({ action: "execute_command_line", command: "help" });
        s.setBottomEditor("console");
        break;
      case "about_fe3d":
        setHelpLine("Final Effect 3D v0.5.0 · Open-Source C++20 / WebGL 2.0 Creative Suite (GNU GPLv3)");
        break;
      default:
        break;
    }
  }

  // =========================================================================
  // MENU SETS RENDERING
  // =========================================================================
  function renderMenuSetMenus() {
    const host = document.getElementById("maya-menuset-menus");
    if (!host) return;
    const defs = MENU_SETS[activeMenuSet] || MENU_SETS.modeling;
    host.innerHTML = "";

    defs.forEach((menuDef) => {
      const wrap = document.createElement("div");
      wrap.className = "menu-item";
      const trig = document.createElement("button");
      trig.type = "button";
      trig.className = "menu-trigger menuset-trigger";
      trig.dataset.menuTrigger = menuDef.id;
      trig.dataset.testid = "menuset-menu-" + menuDef.label.toLowerCase().replace(/[^a-z0-9]+/g, "-");
      trig.textContent = menuDef.label;

      const drop = document.createElement("div");
      drop.className = "menu-dropdown";
      drop.dataset.menuPanel = menuDef.id;
      drop.hidden = true;

      menuDef.items.forEach((it) => {
        if (it.sep) {
          const sep = document.createElement("div");
          sep.className = "menu-sep";
          drop.appendChild(sep);
          return;
        }
        const btn = document.createElement("button");
        btn.type = "button";
        btn.innerHTML =
          "<span>" +
          it.label +
          "</span>" +
          (it.kbd ? "<kbd>" + it.kbd + "</kbd>" : "");
        if (it.preset) {
          btn.dataset.preset = it.preset;
        } else if (it.cmd) {
          btn.dataset.mayaCmd = it.cmd;
        }
        drop.appendChild(btn);
      });

      wrap.appendChild(trig);
      wrap.appendChild(drop);
      host.appendChild(wrap);
    });
  }

  function setMenuSet(nextSet) {
    if (!MENU_SETS[nextSet]) return;
    activeMenuSet = nextSet;
    saveStoredJson(MENUSET_STORAGE_KEY, activeMenuSet);
    const sel = document.getElementById("maya-menuset-select");
    if (sel && sel.value !== activeMenuSet) sel.value = activeMenuSet;
    renderMenuSetMenus();
    setHelpLine("Menu Set switched to " + nextSet.charAt(0).toUpperCase() + nextSet.slice(1));
  }

  // =========================================================================
  // CUSTOMIZABLE SHELF CONTROLLER
  // =========================================================================
  function loadShelves() {
    const saved = loadStoredJson(SHELVES_STORAGE_KEY, null);
    if (saved && Array.isArray(saved.shelves) && saved.shelves.length > 0) {
      shelves = saved.shelves;
      activeShelfId = saved.activeShelfId || shelves[0].id;
    } else {
      shelves = getDefaultShelves();
      activeShelfId = "poly_modeling";
    }
  }

  function persistShelves() {
    saveStoredJson(SHELVES_STORAGE_KEY, {
      activeShelfId: activeShelfId,
      shelves: shelves,
    });
  }

  function renderShelves() {
    const tabsHost = document.getElementById("maya-shelf-tabs");
    const iconsHost = document.getElementById("maya-shelf-icons");
    if (!tabsHost || !iconsHost) return;

    if (!shelves.some((sh) => sh.id === activeShelfId) && shelves.length > 0) {
      activeShelfId = shelves[0].id;
    }

    tabsHost.innerHTML = "";
    shelves.forEach((sh) => {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.role = "tab";
      btn.className = "maya-shelf-tab" + (sh.id === activeShelfId ? " is-active" : "");
      btn.dataset.testid = "shelf-tab-" + sh.id;
      btn.setAttribute("aria-selected", sh.id === activeShelfId ? "true" : "false");
      btn.textContent = sh.name;
      btn.addEventListener("click", () => {
        activeShelfId = sh.id;
        persistShelves();
        renderShelves();
      });
      tabsHost.appendChild(btn);
    });

    const activeShelf = shelves.find((sh) => sh.id === activeShelfId) || shelves[0];
    iconsHost.innerHTML = "";
    if (!activeShelf) return;

    activeShelf.items.forEach((item, idx) => {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.className = "maya-shelf-item";
      btn.dataset.testid = "shelf-item-" + item.id;
      btn.title = (item.help || item.label) + " (RMB for Shelf Options)";
      if (item.help) btn.dataset.help = item.help;

      const icSvg = (REG.ICONS && REG.ICONS[item.icon]) || REG.ICONS.cube || "";
      btn.innerHTML =
        '<span class="btn-ic">' +
        icSvg +
        '</span><span class="maya-shelf-item-lbl">' +
        item.label +
        "</span>";

      btn.addEventListener("click", () => {
        hideShelfPopup();
        if (item.preset) {
          dispatch({ action: "add_preset", preset: item.preset });
        } else if (item.tool) {
          dispatch({ action: "set_active_tool", tool: item.tool });
        } else if (item.cmd) {
          executeMayaCommand(item.cmd);
        } else if (item.mel) {
          dispatch({ action: "execute_command_line", command: item.mel });
        }
      });

      btn.addEventListener("contextmenu", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        openShelfItemPopup(ev.clientX, ev.clientY, activeShelf, idx);
      });

      iconsHost.appendChild(btn);
    });
  }

  function hideShelfPopup() {
    const pop = document.getElementById("maya-shelf-popup");
    if (pop) pop.hidden = true;
  }

  function showPopupAt(pop, x, y) {
    pop.style.left = "8px";
    pop.style.top = "8px";
    pop.hidden = false;
    const r = pop.getBoundingClientRect();
    const maxX = Math.max(8, (window.innerWidth || 1280) - r.width - 8);
    const maxY = Math.max(8, (window.innerHeight || 720) - r.height - 8);
    pop.style.left = Math.min(maxX, Math.max(8, x)) + "px";
    pop.style.top = Math.min(maxY, Math.max(8, y)) + "px";
  }

  function openShelfItemPopup(x, y, shelf, itemIdx) {
    const pop = document.getElementById("maya-shelf-popup");
    if (!pop) return;
    const item = shelf.items[itemIdx];
    pop.innerHTML = "";

    const head = document.createElement("div");
    head.className = "ctx-menu-header";
    head.textContent = "Shelf Item · " + item.label;
    pop.appendChild(head);

    function addRow(lbl, fn, disabled, testId) {
      const b = document.createElement("button");
      b.type = "button";
      b.textContent = lbl;
      b.disabled = Boolean(disabled);
      if (testId) b.dataset.testid = testId;
      b.addEventListener("click", (ev) => {
        ev.stopPropagation();
        hideShelfPopup();
        fn();
      });
      pop.appendChild(b);
    }

    addRow("Open Tool Settings", () => executeMayaCommand("focus_tool_settings"), false, "shelf-ctx-options");
    addRow(
      "Move Left",
      () => {
        if (itemIdx > 0) {
          const tmp = shelf.items[itemIdx - 1];
          shelf.items[itemIdx - 1] = shelf.items[itemIdx];
          shelf.items[itemIdx] = tmp;
          persistShelves();
          renderShelves();
        }
      },
      itemIdx === 0,
      "shelf-ctx-move-left"
    );
    addRow(
      "Move Right",
      () => {
        if (itemIdx < shelf.items.length - 1) {
          const tmp = shelf.items[itemIdx + 1];
          shelf.items[itemIdx + 1] = shelf.items[itemIdx];
          shelf.items[itemIdx] = tmp;
          persistShelves();
          renderShelves();
        }
      },
      itemIdx >= shelf.items.length - 1,
      "shelf-ctx-move-right"
    );
    addRow(
      "Remove from Shelf",
      () => {
        shelf.items.splice(itemIdx, 1);
        persistShelves();
        renderShelves();
      },
      false,
      "shelf-ctx-remove"
    );

    showPopupAt(pop, x, y);
  }

  function openShelfGearMenu(x, y) {
    const pop = document.getElementById("maya-shelf-popup");
    if (!pop) return;
    const activeShelf = shelves.find((sh) => sh.id === activeShelfId) || shelves[0];
    pop.innerHTML = "";

    const head = document.createElement("div");
    head.className = "ctx-menu-header";
    head.textContent = "Shelf Editor · " + (activeShelf ? activeShelf.name : "");
    pop.appendChild(head);

    function addBtn(lbl, fn, testId, disabled) {
      const b = document.createElement("button");
      b.type = "button";
      b.textContent = lbl;
      b.disabled = Boolean(disabled);
      if (testId) b.dataset.testid = testId;
      b.addEventListener("click", (ev) => {
        ev.stopPropagation();
        hideShelfPopup();
        fn();
      });
      pop.appendChild(b);
    }

    addBtn(
      "+ Pin Active Tool to Shelf",
      () => {
        if (!activeShelf) return;
        const st = state();
        const t = (st && st.editor && st.editor.active_tool) || "move";
        activeShelf.items.push({
          id: "custom_" + Date.now(),
          label: t.slice(0, 6).toUpperCase(),
          icon: t === "rotate" ? "rotate" : t === "scale" ? "scale" : t === "select" ? "select" : "move",
          tool: t,
          help: "Pinned Tool: " + t,
        });
        persistShelves();
        renderShelves();
      },
      "shelf-gear-add-tool"
    );

    addBtn(
      "+ Pin Freeze Transforms to Shelf",
      () => {
        if (!activeShelf) return;
        activeShelf.items.push({
          id: "frz_" + Date.now(),
          label: "Freeze",
          icon: "objectProps",
          cmd: "freeze_transforms",
          help: "Freeze Transformations",
        });
        persistShelves();
        renderShelves();
      },
      "shelf-gear-add-freeze"
    );

    addBtn(
      "+ Create Custom Shelf",
      () => {
        const customNum = shelves.filter((s) => !s.builtin).length + 1;
        const newId = "custom_shelf_" + Date.now();
        shelves.push({
          id: newId,
          name: "Custom" + customNum,
          builtin: false,
          items: [
            { id: newId + "_cub", label: "Cube", icon: "cube", preset: "cube", help: "Create Polygon Cube" },
            { id: newId + "_frz", label: "Freeze", icon: "objectProps", cmd: "freeze_transforms", help: "Freeze Transformations" },
          ],
        });
        activeShelfId = newId;
        persistShelves();
        renderShelves();
      },
      "shelf-gear-new-shelf"
    );

    addBtn(
      "Delete Active Custom Shelf",
      () => {
        if (!activeShelf || activeShelf.builtin) return;
        shelves = shelves.filter((s) => s.id !== activeShelf.id);
        activeShelfId = shelves[0].id;
        persistShelves();
        renderShelves();
      },
      "shelf-gear-delete-shelf",
      !activeShelf || activeShelf.builtin
    );

    addBtn(
      "Reset All Shelves to Default",
      () => {
        shelves = getDefaultShelves();
        activeShelfId = "poly_modeling";
        persistShelves();
        renderShelves();
      },
      "shelf-gear-reset"
    );

    showPopupAt(pop, x, y);
  }

  // =========================================================================
  // RIGHT EDITOR DOCK MODES (Split CB+AE | Channel Box | Attribute Editor | Tool Settings)
  // =========================================================================
  function setRightEditorMode(mode) {
    activeRightMode = mode;
    const cbPanel = document.getElementById("panel-channel-box");
    const aePanel = document.getElementById("properties-shell");
    const tsPanel = document.getElementById("panel-tool-settings");

    document.querySelectorAll("[data-right-mode-btn]").forEach((btn) => {
      btn.classList.toggle("is-active", btn.dataset.rightModeBtn === mode);
    });

    if (cbPanel && !floatingPanels.has("panel-channel-box")) {
      cbPanel.hidden = mode !== "split" && mode !== "cb";
    }
    if (aePanel) {
      aePanel.hidden = mode !== "split" && mode !== "ae";
    }
    if (tsPanel) {
      tsPanel.hidden = mode !== "tools";
    }
  }

  // =========================================================================
  // DEDICATED MAYA CHANNEL BOX & DISPLAY LAYER EDITOR
  // =========================================================================
  const CHANNEL_DEFS = [
    { key: "tx", label: "Translate X", bit: 1 << 0, group: "pos", idx: 0, step: 0.1 },
    { key: "ty", label: "Translate Y", bit: 1 << 1, group: "pos", idx: 1, step: 0.1 },
    { key: "tz", label: "Translate Z", bit: 1 << 2, group: "pos", idx: 2, step: 0.1 },
    { key: "rx", label: "Rotate X", bit: 1 << 3, group: "rot", idx: 0, step: 1.0 },
    { key: "ry", label: "Rotate Y", bit: 1 << 4, group: "rot", idx: 1, step: 1.0 },
    { key: "rz", label: "Rotate Z", bit: 1 << 5, group: "rot", idx: 2, step: 1.0 },
    { key: "sx", label: "Scale X", bit: 1 << 6, group: "scl", idx: 0, step: 0.05 },
    { key: "sy", label: "Scale Y", bit: 1 << 7, group: "scl", idx: 1, step: 0.05 },
    { key: "sz", label: "Scale Z", bit: 1 << 8, group: "scl", idx: 2, step: 0.05 },
    { key: "v", label: "Visibility", bit: 1 << 9, group: "vis", idx: 0, step: 1 },
  ];

  function renderChannelBox(st) {
    const emptyEl = document.getElementById("maya-cb-empty");
    const wrapEl = document.getElementById("maya-cb-table-wrap");
    const rowsEl = document.getElementById("maya-cb-rows");
    const nameEl = document.getElementById("maya-cb-node-name");
    const lockNodeBtn = document.getElementById("btn-cb-lock-node");
    if (!emptyEl || !wrapEl || !rowsEl) return;

    const insp = st ? st.inspector : null;
    if (!insp) {
      emptyEl.hidden = false;
      wrapEl.hidden = true;
      lastCbSignature = "empty";
      return;
    }

    emptyEl.hidden = true;
    wrapEl.hidden = false;
    if (nameEl) nameEl.textContent = insp.name;
    if (lockNodeBtn) {
      lockNodeBtn.textContent = insp.locked ? "Locked" : "Unlocked";
      lockNodeBtn.classList.toggle("is-active", Boolean(insp.locked));
    }

    // Do not clobber active numeric typing inside the Channel Box!
    if (rowsEl.contains(document.activeElement) && isTextEditingElement(document.activeElement)) {
      return;
    }

    const sig = JSON.stringify([
      insp.id,
      insp.name,
      insp.visible,
      insp.locked,
      insp.locked_channels_mask,
      insp.position,
      insp.rotation_deg,
      insp.scale,
      insp.has_keyframe_at_current,
      insp.custom_attributes,
    ]);
    if (sig === lastCbSignature) return;
    lastCbSignature = sig;

    const mask = Number(insp.locked_channels_mask) || 0;
    const nodeLocked = Boolean(insp.locked);
    const keyedNow = Boolean(insp.has_keyframe_at_current);

    rowsEl.innerHTML = "";

    CHANNEL_DEFS.forEach((ch) => {
      const chLocked = nodeLocked || (mask & ch.bit) !== 0;
      const row = document.createElement("div");
      row.className =
        "maya-cb-row" +
        (chLocked ? " is-ch-locked" : "") +
        (keyedNow ? " is-ch-keyed" : "");

      const lbl = document.createElement("span");
      lbl.className = "maya-cb-lbl";
      lbl.textContent = ch.label;
      lbl.title = "Drag horizontally on label to scrub " + ch.label;

      let rawVal = 0;
      if (ch.group === "pos") rawVal = insp.position[ch.idx];
      else if (ch.group === "rot") rawVal = insp.rotation_deg[ch.idx];
      else if (ch.group === "scl") rawVal = insp.scale[ch.idx];
      else rawVal = insp.visible ? 1 : 0;

      const inp = document.createElement("input");
      inp.className = "maya-cb-input";
      inp.dataset.testid = "cb-" + ch.key;
      if (ch.group === "vis") {
        inp.type = "text";
        inp.value = insp.visible ? "on" : "off";
      } else {
        inp.type = "number";
        inp.step = String(ch.step);
        inp.value = Number(rawVal).toFixed(3);
      }
      inp.disabled = chLocked;

      inp.addEventListener("change", () => {
        if (ch.group === "vis") {
          const vStr = String(inp.value).trim().toLowerCase();
          const vis = vStr === "on" || vStr === "1" || vStr === "true";
          dispatch({ action: "set_visibility", id: insp.id, visible: vis });
          return;
        }
        const num = Number(inp.value);
        if (!Number.isFinite(num)) return;
        const nextPos = insp.position.slice();
        const nextRot = insp.rotation_deg.slice();
        const nextScl = insp.scale.slice();
        if (ch.group === "pos") nextPos[ch.idx] = num;
        else if (ch.group === "rot") nextRot[ch.idx] = num;
        else if (ch.group === "scl") nextScl[ch.idx] = num;
        dispatch({
          action: "set_transform",
          id: insp.id,
          position: nextPos,
          rotation_deg: nextRot,
          scale: nextScl,
          coalesce: false,
        });
      });

      // Virtual horizontal drag scrub on Channel Box label
      if (ch.group !== "vis" && !chLocked) {
        lbl.addEventListener("pointerdown", (ev) => {
          if (ev.button !== 0 && ev.button !== 1) return;
          ev.preventDefault();
          lbl.setPointerCapture(ev.pointerId);
          const startX = ev.clientX;
          const startVal = Number(rawVal) || 0;
          let latestVal = startVal;
          function onMove(mev) {
            const delta = (mev.clientX - startX) * ch.step * 0.25;
            latestVal = startVal + delta;
            const p = insp.position.slice();
            const r = insp.rotation_deg.slice();
            const s = insp.scale.slice();
            if (ch.group === "pos") p[ch.idx] = latestVal;
            else if (ch.group === "rot") r[ch.idx] = latestVal;
            else if (ch.group === "scl") s[ch.idx] = Math.max(0.01, latestVal);
            dispatch({
              action: "set_transform",
              id: insp.id,
              position: p,
              rotation_deg: r,
              scale: s,
              coalesce: true,
            });
          }
          function onUp(uev) {
            lbl.removeEventListener("pointermove", onMove);
            lbl.removeEventListener("pointerup", onUp);
            try {
              lbl.releasePointerCapture(uev.pointerId);
            } catch (_) {}
          }
          lbl.addEventListener("pointermove", onMove);
          lbl.addEventListener("pointerup", onUp);
        });
      }

      const lockBtn = document.createElement("button");
      lockBtn.type = "button";
      lockBtn.className = "maya-cb-lock-btn" + ((mask & ch.bit) !== 0 ? " is-locked" : "");
      lockBtn.dataset.testid = "cb-lock-" + ch.key;
      lockBtn.title = (mask & ch.bit) !== 0 ? "Unlock " + ch.label : "Lock " + ch.label;
      lockBtn.textContent = (mask & ch.bit) !== 0 ? "L" : "·";
      lockBtn.addEventListener("click", () => {
        const nextMask = (mask ^ ch.bit) >>> 0;
        dispatch({ action: "set_channel_locks", id: insp.id, mask: nextMask });
      });

      row.appendChild(lbl);
      row.appendChild(inp);
      row.appendChild(lockBtn);
      rowsEl.appendChild(row);
    });

    // Render Custom Attributes section
    const customAttrs = insp.custom_attributes || [];
    if (customAttrs.length > 0) {
      const sep = document.createElement("div");
      sep.className = "maya-cb-subhead";
      sep.textContent = "EXTRA / CUSTOM ATTRIBUTES";
      rowsEl.appendChild(sep);

      customAttrs.forEach((ca) => {
        const row = document.createElement("div");
        row.className = "maya-cb-row" + (ca.locked || nodeLocked ? " is-ch-locked" : "");

        const lbl = document.createElement("span");
        lbl.className = "maya-cb-lbl";
        lbl.textContent = ca.name;

        const inp = document.createElement("input");
        inp.type = "number";
        inp.step = "0.1";
        inp.className = "maya-cb-input";
        inp.dataset.testid = "cb-custom-" + ca.name;
        inp.value = Number(ca.value).toFixed(3);
        inp.disabled = Boolean(ca.locked || nodeLocked);
        inp.addEventListener("change", () => {
          const nv = Number(inp.value);
          if (Number.isFinite(nv)) {
            dispatch({
              action: "set_custom_attribute",
              id: insp.id,
              name: ca.name,
              value: nv,
              coalesce: false,
            });
          }
        });

        const delBtn = document.createElement("button");
        delBtn.type = "button";
        delBtn.className = "maya-cb-lock-btn";
        delBtn.title = "Delete Custom Attribute '" + ca.name + "'";
        delBtn.textContent = "×";
        delBtn.addEventListener("click", () => {
          dispatch({ action: "delete_custom_attribute", id: insp.id, name: ca.name });
        });

        row.appendChild(lbl);
        row.appendChild(inp);
        row.appendChild(delBtn);
        rowsEl.appendChild(row);
      });
    }
  }

  function renderDisplayLayers(st) {
    const listEl = document.getElementById("maya-layer-list");
    if (!listEl) return;
    const layers = (st && st.display_layers) || [];
    if (layers.length > 0 && !layers.some((l) => l.id === activeLayerId)) {
      activeLayerId = layers[layers.length - 1].id;
    } else if (layers.length === 0) {
      activeLayerId = 0;
    }

    const sig = JSON.stringify([activeLayerId, layers]);
    if (sig === lastLayersSignature) return;
    lastLayersSignature = sig;

    listEl.innerHTML = "";

    // Always show Maya's built-in defaultLayer at bottom + user layers above
    layers.forEach((layer) => {
      const row = document.createElement("div");
      row.className = "maya-layer-row" + (layer.id === activeLayerId ? " is-active" : "");
      row.dataset.testid = "display-layer-row-" + layer.name;

      // [V] Visibility toggle
      const vBtn = document.createElement("button");
      vBtn.type = "button";
      vBtn.className = "layer-box-btn" + (layer.visible ? " is-on" : "");
      vBtn.dataset.testid = "layer-vis-" + layer.name;
      vBtn.title = "Toggle Layer Visibility";
      vBtn.textContent = layer.visible ? "V" : "";
      vBtn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        dispatch({
          action: "update_display_layer",
          layer_id: layer.id,
          visible: !layer.visible,
        });
      });

      // [P] Playback visibility toggle
      const pBtn = document.createElement("button");
      pBtn.type = "button";
      pBtn.className = "layer-box-btn" + (layer.playback_visible ? " is-on" : "");
      pBtn.title = "Visible During Playback";
      pBtn.textContent = layer.playback_visible ? "P" : "";
      pBtn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        dispatch({
          action: "update_display_layer",
          layer_id: layer.id,
          playback_visible: !layer.playback_visible,
        });
      });

      // [ / T / R] Display Type cycle (Normal -> Template -> Reference -> Normal)
      const dtBtn = document.createElement("button");
      dtBtn.type = "button";
      dtBtn.className = "layer-box-btn layer-mode-" + layer.display_type;
      dtBtn.dataset.testid = "layer-mode-" + layer.name;
      dtBtn.title = "Cycle Normal / Template (T) / Reference (R)";
      dtBtn.textContent =
        layer.display_type === "template" ? "T" : layer.display_type === "reference" ? "R" : "";
      dtBtn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        const nextType =
          layer.display_type === "normal"
            ? "template"
            : layer.display_type === "template"
            ? "reference"
            : "normal";
        dispatch({
          action: "update_display_layer",
          layer_id: layer.id,
          display_type: nextType,
        });
      });

      const nameSpan = document.createElement("span");
      nameSpan.className = "maya-layer-name";
      nameSpan.textContent = layer.name + " (" + (layer.member_ids || []).length + ")";
      nameSpan.title = "Click to make active · Double-click to rename";
      nameSpan.addEventListener("dblclick", (ev) => {
        ev.stopPropagation();
        const inp = document.createElement("input");
        inp.type = "text";
        inp.className = "outliner-inline-rename";
        inp.value = layer.name;
        nameSpan.replaceWith(inp);
        inp.focus();
        inp.select();
        let done = false;
        function finish(commit) {
          if (done) return;
          done = true;
          const nn = (inp.value || "").trim();
          if (commit && nn && nn !== layer.name) {
            dispatch({ action: "rename_display_layer", layer_id: layer.id, name: nn });
          } else {
            lastLayersSignature = "";
            renderDisplayLayers(state());
          }
        }
        inp.addEventListener("keydown", (ke) => {
          if (ke.key === "Enter") finish(true);
          else if (ke.key === "Escape") finish(false);
        });
        inp.addEventListener("blur", () => finish(true));
      });

      const selBtn = document.createElement("button");
      selBtn.type = "button";
      selBtn.className = "ghost-btn layer-mini-act";
      selBtn.title = "Select Objects in Layer";
      selBtn.textContent = "Sel";
      selBtn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        dispatch({ action: "select_display_layer_objects", layer_id: layer.id });
      });

      const delBtn = document.createElement("button");
      delBtn.type = "button";
      delBtn.className = "ghost-btn layer-mini-act";
      delBtn.dataset.testid = "layer-del-" + layer.name;
      delBtn.title = "Delete Display Layer (Preserves Scene Objects)";
      delBtn.textContent = "×";
      delBtn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        dispatch({ action: "delete_display_layer", layer_id: layer.id });
      });

      row.addEventListener("click", () => {
        activeLayerId = layer.id;
        lastLayersSignature = "";
        renderDisplayLayers(state());
      });

      row.appendChild(vBtn);
      row.appendChild(pBtn);
      row.appendChild(dtBtn);
      row.appendChild(nameSpan);
      row.appendChild(selBtn);
      row.appendChild(delBtn);
      listEl.appendChild(row);
    });

    const defRow = document.createElement("div");
    defRow.className = "maya-layer-row is-default-layer";
    defRow.innerHTML =
      '<span class="layer-box-static">V</span><span class="layer-box-static">P</span><span class="layer-box-static"></span><span class="maya-layer-name">defaultLayer</span>';
    listEl.appendChild(defRow);
  }

  // =========================================================================
  // MAYA TIME SLIDER & RANGE SLIDER
  // =========================================================================
  function renderMayaTimeSlider(st) {
    const tl = st && st.timeline;
    if (!tl) return;

    const sig = JSON.stringify(tl);
    const cvs = document.getElementById("maya-timeslider-canvas");
    if (sig !== lastTimelineSignature) {
      lastTimelineSignature = sig;
      const curF = Math.round(Number(tl.current_frame) || 1);
      const pStart = Math.round(Number(tl.playback_start) || 1);
      const pEnd = Math.round(Number(tl.playback_end) || 120);
      const sFrame = Math.round(Number(tl.start_frame) || 1);
      const eFrame = Math.round(Number(tl.end_frame) || 240);

      const scrub = document.getElementById("timeline-scrubber");
      const fNum = document.getElementById("timeline-frame-num");
      if (scrub && document.activeElement !== scrub) {
        scrub.min = String(pStart);
        scrub.max = String(pEnd);
        scrub.value = String(curF);
      }
      if (fNum && document.activeElement !== fNum) {
        fNum.value = String(curF);
      }

      const setUnfocused = (id, v) => {
        const el = document.getElementById(id);
        if (el && document.activeElement !== el) el.value = String(v);
      };
      setUnfocused("tl-start-frame", sFrame);
      setUnfocused("tl-play-start", pStart);
      setUnfocused("tl-play-end", pEnd);
      setUnfocused("tl-end-frame", eFrame);
      setUnfocused("tl-fps-select", Math.round(Number(tl.fps) || 24));
      setUnfocused("tl-loop-select", tl.playback_mode || "continuous");

      const autoBtn = document.getElementById("btn-tl-autokey");
      if (autoBtn) autoBtn.classList.toggle("is-autokey-on", Boolean(tl.auto_key));

      const thumb = document.getElementById("maya-range-thumb");
      if (thumb) {
        const span = Math.max(1, eFrame - sFrame);
        const leftPct = Math.max(0, Math.min(100, ((pStart - sFrame) / span) * 100));
        const widthPct = Math.max(4, Math.min(100 - leftPct, ((pEnd - pStart) / span) * 100));
        thumb.style.left = leftPct.toFixed(1) + "%";
        thumb.style.width = widthPct.toFixed(1) + "%";
      }
    }

    if (!cvs) return;
    const rect = cvs.getBoundingClientRect();
    const w = Math.max(120, Math.round(rect.width || 480));
    const h = 28;
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    if (cvs.width !== Math.round(w * dpr) || cvs.height !== Math.round(h * dpr)) {
      cvs.width = Math.round(w * dpr);
      cvs.height = Math.round(h * dpr);
    }
    const ctx = cvs.getContext("2d");
    if (!ctx) return;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    const pStart = Number(tl.playback_start) || 1;
    const pEnd = Math.max(pStart + 1, Number(tl.playback_end) || 120);
    const curF = Number(tl.current_frame) || 1;
    const span = pEnd - pStart;
    const xForFrame = (f) => 8 + ((f - pStart) / span) * (w - 16);

    ctx.fillStyle = "#252525";
    ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = "#181818";
    ctx.strokeRect(0.5, 0.5, w - 1, h - 1);

    // Draw ruler ticks
    const approxStep = Math.max(1, Math.round(span / Math.max(4, Math.floor(w / 52))));
    ctx.fillStyle = "#9a9a9a";
    ctx.strokeStyle = "#484848";
    ctx.font = "10px ui-monospace, SFMono-Regular, Menlo, monospace";
    for (let f = Math.ceil(pStart); f <= Math.floor(pEnd); ++f) {
      const x = Math.round(xForFrame(f)) + 0.5;
      const isMajor = (f - Math.ceil(pStart)) % approxStep === 0 || f === Math.floor(pEnd);
      ctx.beginPath();
      ctx.moveTo(x, isMajor ? 4 : 17);
      ctx.lineTo(x, h - 2);
      ctx.stroke();
      if (isMajor) {
        ctx.fillText(String(f), x + 2, 12);
      }
    }

    // Draw selected object's keyframes in classic Maya red!
    const selKeys = tl.selected_key_frames || [];
    ctx.fillStyle = "#e04848";
    selKeys.forEach((kf) => {
      if (kf >= pStart && kf <= pEnd) {
        const kx = Math.round(xForFrame(kf));
        ctx.fillRect(kx - 1, 2, 3, h - 4);
      }
    });

    // Draw current frame scrubbing playhead block
    const cx = xForFrame(Math.max(pStart, Math.min(pEnd, curF)));
    ctx.fillStyle = "rgba(82, 133, 166, 0.42)";
    ctx.fillRect(cx - 5, 1, 10, h - 2);
    ctx.fillStyle = "#7cc0ea";
    ctx.fillRect(cx - 1, 0, 2, h);
  }

  // =========================================================================
  // STATUS LINE, VIEWPORT TOOLBAR & TOOL SETTINGS SYNC
  // =========================================================================
  function syncToolAndViewportBars(st) {
    if (!st) return;
    const vp = st.viewport || {};
    const ed = st.editor || {};
    const snap = ed.snapping || {};
    const mask = ed.selection_mask || {};
    const activeTool = ed.active_tool || "move";

    document.querySelectorAll("#tool-shelf [data-maya-tool]").forEach((btn) => {
      btn.classList.toggle("is-active", btn.dataset.mayaTool === activeTool);
    });

    // Selection masks
    document.querySelectorAll("[data-sel-mask]").forEach((btn) => {
      const k = btn.dataset.selMask;
      btn.classList.toggle("is-active", Boolean(mask[k] !== false));
    });

    // Snapping toggles
    document.querySelectorAll("[data-snap-toggle]").forEach((btn) => {
      const k = btn.dataset.snapToggle;
      btn.classList.toggle("is-active", Boolean(snap[k]));
    });

    // Viewport shading & display buttons
    const shadingMode = vp.shading_mode || "shaded_textured";
    document.querySelectorAll("#vp-panel-toolbar [data-vp-shading]").forEach((btn) => {
      btn.classList.toggle("is-active", btn.dataset.vpShading === shadingMode);
    });
    const setToggle = (id, on) => {
      const el = document.getElementById(id);
      if (el) el.classList.toggle("is-active", Boolean(on));
    };
    setToggle("btn-vp-wos", vp.wireframe_on_shaded);
    setToggle("btn-vp-default-mat", vp.default_material);
    setToggle("btn-vp-all-lights", vp.use_all_lights !== false);
    setToggle("btn-vp-xray", vp.xray_mode);
    setToggle("btn-vp-isolate", vp.isolate_select);
    setToggle("btn-vp-grid", vp.editor_grid_visible !== false);
    setToggle("btn-vp-helpers", vp.editor_helpers_visible !== false);

    const smooth = Number(vp.mesh_display_smoothness) || 1;
    document.querySelectorAll("[data-vp-smooth]").forEach((btn) => {
      btn.classList.toggle("is-active", Number(btn.dataset.vpSmooth) === smooth);
    });

    // Workspace label
    const wsLbl = document.getElementById("maya-ws-active-name");
    if (wsLbl && st.workspace && st.workspace.layout_name) {
      wsLbl.textContent = st.workspace.layout_name;
    }

    // Tool settings panel sync
    const tsToolLbl = document.getElementById("ts-active-tool-label");
    if (tsToolLbl) tsToolLbl.textContent = activeTool.toUpperCase();
    const tsSpace = document.getElementById("ts-axis-space");
    if (tsSpace && document.activeElement !== tsSpace) {
      tsSpace.value = ed.gizmo_space || "world";
    }
    const tsGrid = document.getElementById("ts-grid-step");
    if (tsGrid && document.activeElement !== tsGrid && snap.grid_step) {
      tsGrid.value = String(snap.grid_step);
    }

    // Marking Menu component availability
    const canMesh = Boolean(ed.editable_mesh && ed.editable_mesh.editable);
    ["mm-vertex", "mm-edge", "mm-face"].forEach((id) => {
      const b = document.getElementById(id) || document.querySelector('[data-testid="' + id + '"]');
      if (b) b.disabled = !canMesh;
    });
  }

  // =========================================================================
  // FLOATING UNDOCKED PANELS & LEFT/RIGHT OUTLINER DOCKING
  // =========================================================================
  function floatPanel(panelId, titleText) {
    const panel = document.getElementById(panelId);
    const host = document.getElementById("maya-floating-host");
    if (!panel || !host || floatingPanels.has(panelId)) return;

    const placeholder = document.createComment("float-anchor-" + panelId);
    const originalParent = panel.parentNode;
    originalParent.insertBefore(placeholder, panel);

    const win = document.createElement("div");
    win.className = "maya-floating-window";
    win.dataset.testid = "floating-win-" + panelId;
    win.style.left = 90 + floatingPanels.size * 36 + "px";
    win.style.top = 120 + floatingPanels.size * 32 + "px";

    const bar = document.createElement("div");
    bar.className = "maya-floating-titlebar";
    bar.innerHTML =
      "<strong>" +
      titleText +
      '</strong><button type="button" class="ghost-btn" data-testid="redock-' +
      panelId +
      '">Dock</button>';

    bar.querySelector("button").addEventListener("click", () => redockPanel(panelId));

    // Drag floating window
    bar.addEventListener("pointerdown", (ev) => {
      if (ev.target.tagName === "BUTTON") return;
      ev.preventDefault();
      bar.setPointerCapture(ev.pointerId);
      const sx = ev.clientX;
      const sy = ev.clientY;
      const rect = win.getBoundingClientRect();
      function onMove(mev) {
        win.style.left = Math.max(4, rect.left + (mev.clientX - sx)) + "px";
        win.style.top = Math.max(26, rect.top + (mev.clientY - sy)) + "px";
      }
      function onUp(uev) {
        bar.removeEventListener("pointermove", onMove);
        bar.removeEventListener("pointerup", onUp);
        try {
          bar.releasePointerCapture(uev.pointerId);
        } catch (_) {}
      }
      bar.addEventListener("pointermove", onMove);
      bar.addEventListener("pointerup", onUp);
    });

    panel.hidden = false;
    win.appendChild(bar);
    win.appendChild(panel);
    host.appendChild(win);

    floatingPanels.set(panelId, { placeholder, originalParent, winEl: win });
  }

  function redockPanel(panelId) {
    const rec = floatingPanels.get(panelId);
    const panel = document.getElementById(panelId);
    if (!rec || !panel) return;
    rec.originalParent.insertBefore(panel, rec.placeholder);
    rec.placeholder.remove();
    rec.winEl.remove();
    floatingPanels.delete(panelId);
    setRightEditorMode(activeRightMode);
  }

  function redockAllFloatingPanels() {
    Array.from(floatingPanels.keys()).forEach(redockPanel);
  }

  function toggleOutlinerLeftRightDock() {
    const outliner = document.getElementById("panel-outliner");
    const dockLeft = document.getElementById("dock-left");
    const splitLeft = document.getElementById("splitter-left");
    const dockRight = document.getElementById("dock-right");
    const splitOutliner = document.getElementById("splitter-outliner");
    if (!outliner || !dockLeft || !dockRight) return;

    if (floatingPanels.has("panel-outliner")) redockPanel("panel-outliner");

    outlinerDockedLeft = !outlinerDockedLeft;
    if (outlinerDockedLeft) {
      dockLeft.hidden = false;
      if (splitLeft) splitLeft.hidden = false;
      dockLeft.appendChild(outliner);
      if (splitOutliner) splitOutliner.hidden = true;
    } else {
      dockRight.insertBefore(outliner, dockRight.firstChild);
      dockLeft.hidden = true;
      if (splitLeft) splitLeft.hidden = true;
      if (splitOutliner) splitOutliner.hidden = false;
    }
  }

  // =========================================================================
  // RENDER CURRENT FRAME PREVIEW MODAL
  // =========================================================================
  function openRenderViewModal() {
    const s = studio();
    const modal = document.getElementById("maya-render-modal");
    const img = document.getElementById("maya-render-preview-img");
    const canvas = document.getElementById("fe3d-canvas");
    if (!s || !modal || !img || !canvas) return;

    s.renderFinalFrame();
    try {
      img.src = canvas.toDataURL("image/png");
    } catch (_) {}
    modal.hidden = false;
  }

  // =========================================================================
  // HOTBOX & HOTKEY EDITOR
  // =========================================================================
  function showHotbox() {
    const hb = document.getElementById("maya-hotbox");
    if (!hb) return;
    hb.hidden = false;
    const card = hb.querySelector(".hotbox-crosshair");
    if (card) {
      const cx = Math.min(Math.max(220, lastMouseClient.x), (window.innerWidth || 1280) - 220);
      const cy = Math.min(Math.max(150, lastMouseClient.y), (window.innerHeight || 720) - 150);
      card.style.left = cx + "px";
      card.style.top = cy + "px";
    }
  }

  function hideHotbox() {
    spacebarHeld = false;
    const hb = document.getElementById("maya-hotbox");
    if (hb) hb.hidden = true;
  }

  function loadHotkeys() {
    const saved = loadStoredJson(HOTKEYS_STORAGE_KEY, null);
    if (saved && Array.isArray(saved) && saved.length === DEFAULT_HOTKEYS.length) {
      hotkeys = saved;
    } else {
      hotkeys = DEFAULT_HOTKEYS.map((h) => Object.assign({}, h));
    }
  }

  function renderHotkeyEditor() {
    const host = document.getElementById("maya-hotkey-list");
    if (!host) return;
    host.innerHTML = "";
    hotkeys.forEach((hk) => {
      const row = document.createElement("div");
      row.className = "maya-hotkey-row";
      const lbl = document.createElement("span");
      lbl.textContent = hk.label;

      const inp = document.createElement("input");
      inp.type = "text";
      inp.className = "maya-hotkey-input";
      inp.dataset.testid = "hotkey-input-" + hk.id;
      inp.value = (hk.ctrl ? "Ctrl+" : "") + (hk.shift ? "Shift+" : "") + hk.key.replace(/^Key|^Digit/, "");
      inp.title = "Press a key to rebind '" + hk.label + "'";
      inp.readOnly = true;
      inp.addEventListener("keydown", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        if (ev.key === "Shift" || ev.key === "Control" || ev.key === "Alt" || ev.key === "Meta") return;
        hk.key = ev.code;
        hk.ctrl = ev.ctrlKey || ev.metaKey;
        hk.shift = ev.shiftKey;
        saveStoredJson(HOTKEYS_STORAGE_KEY, hotkeys);
        renderHotkeyEditor();
      });

      row.appendChild(lbl);
      row.appendChild(inp);
      host.appendChild(row);
    });
  }

  function executeHotkeyById(hkId) {
    const st = state();
    const vp = (st && st.viewport) || {};
    switch (hkId) {
      case "tool_select": dispatch({ action: "set_active_tool", tool: "select" }); break;
      case "tool_move":   dispatch({ action: "set_active_tool", tool: "move" }); break;
      case "tool_rotate": dispatch({ action: "set_active_tool", tool: "rotate" }); break;
      case "tool_scale":  dispatch({ action: "set_active_tool", tool: "scale" }); break;
      case "tool_manip":  dispatch({ action: "set_active_tool", tool: "show_manip" }); break;
      case "tool_last":   dispatch({ action: "activate_last_tool" }); break;
      case "tool_pivot":  dispatch({ action: "set_active_tool", tool: "pivot" }); break;
      case "frame_sel":   dispatch({ action: "frame_selection" }); break;
      case "frame_all":   dispatch({ action: "frame_all" }); break;
      case "shade_wire":  dispatch({ action: "set_viewport_display", shading_mode: "wireframe" }); break;
      case "shade_smooth":dispatch({ action: "set_viewport_display", shading_mode: "shaded" }); break;
      case "shade_tex":   dispatch({ action: "set_viewport_display", shading_mode: "shaded_textured" }); break;
      case "shade_lights":dispatch({ action: "set_viewport_display", use_all_lights: !(vp.use_all_lights !== false) }); break;
      case "set_key":     dispatch({ action: "set_keyframe" }); break;
      case "group_sel":   dispatch({ action: "group_selected" }); break;
      case "duplicate_sel": dispatch({ action: "duplicate_selected" }); break;
      case "parent_sel":  dispatch({ action: "parent_selected" }); break;
      case "unparent_sel":dispatch({ action: "unparent_selected" }); break;
      default: break;
    }
  }

  // =========================================================================
  // EVENT WIRING & INITIALIZATION
  // =========================================================================
  function initMayaWorkspace() {
    loadShelves();
    loadHotkeys();
    commandHistory = loadStoredJson(CMD_HISTORY_KEY, []);
    const savedMs = loadStoredJson(MENUSET_STORAGE_KEY, "modeling");
    if (MENU_SETS[savedMs]) activeMenuSet = savedMs;

    const msSelect = document.getElementById("maya-menuset-select");
    if (msSelect) {
      msSelect.value = activeMenuSet;
      msSelect.addEventListener("change", () => setMenuSet(msSelect.value));
    }
    renderMenuSetMenus();
    renderShelves();
    renderHotkeyEditor();

    // Track mouse position for Spacebar Hotbox placement & Help Line hover hints
    window.addEventListener("pointermove", (ev) => {
      lastMouseClient.x = ev.clientX;
      lastMouseClient.y = ev.clientY;
    });

    document.addEventListener("mouseover", (ev) => {
      const target = ev.target && ev.target.closest ? ev.target.closest("[data-help],[title]") : null;
      if (target) {
        const msg = target.getAttribute("data-help") || target.getAttribute("title");
        if (msg) setHelpLine(msg);
      }
    });

    // Delegated clicks for dynamic Menu-Set menus, Hotbox, Marking Menu & Viewport menus
    document.addEventListener("click", (ev) => {
      const trigger = ev.target.closest ? ev.target.closest("#maya-menuset-menus [data-menu-trigger]") : null;
      if (trigger) {
        ev.stopPropagation();
        const s = studio();
        const item = trigger.closest(".menu-item");
        const panel = item ? item.querySelector(".menu-dropdown") : null;
        const wasOpen = item && item.classList.contains("is-open");
        if (s) s.closeAllMenus();
        if (item && panel && !wasOpen) {
          item.classList.add("is-open");
          panel.style.left = "0px";
          panel.hidden = false;
        }
        return;
      }

      const presetBtn = ev.target.closest
        ? ev.target.closest("#maya-menuset-menus [data-preset], #maya-hotbox [data-preset]")
        : null;
      if (presetBtn) {
        ev.stopPropagation();
        const s = studio();
        if (s) s.closeAllMenus();
        hideHotbox();
        dispatch({ action: "add_preset", preset: presetBtn.dataset.preset });
        return;
      }

      const mayaCmdBtn = ev.target.closest ? ev.target.closest("[data-maya-cmd]") : null;
      if (mayaCmdBtn) {
        ev.stopPropagation();
        executeMayaCommand(mayaCmdBtn.dataset.mayaCmd);
        return;
      }

      const mayaToolBtn = ev.target.closest ? ev.target.closest("[data-maya-tool]") : null;
      if (mayaToolBtn) {
        hideHotbox();
        dispatch({ action: "set_active_tool", tool: mayaToolBtn.dataset.mayaTool });
        return;
      }

      const vpShadingBtn = ev.target.closest ? ev.target.closest("[data-vp-shading]") : null;
      if (vpShadingBtn) {
        const s = studio();
        if (s) s.closeAllMenus();
        hideHotbox();
        dispatch({ action: "set_viewport_display", shading_mode: vpShadingBtn.dataset.vpShading });
        return;
      }

      const vpSmoothBtn = ev.target.closest ? ev.target.closest("[data-vp-smooth]") : null;
      if (vpSmoothBtn) {
        dispatch({
          action: "set_viewport_display",
          mesh_display_smoothness: Number(vpSmoothBtn.dataset.vpSmooth) || 1,
        });
        return;
      }

      const vpPresetBtn = ev.target.closest ? ev.target.closest("[data-vp-preset]") : null;
      if (vpPresetBtn) {
        const s = studio();
        if (s) s.closeAllMenus();
        hideHotbox();
        const p = vpPresetBtn.dataset.vpPreset;
        if (p === "perspective") {
          dispatch({ action: "set_camera_view", active: false });
          dispatch({ action: "set_viewport_projection", projection: "perspective" });
        } else {
          dispatch({ action: "set_camera_view", active: false });
          dispatch({ action: "set_viewport_projection", projection: "orthographic" });
          dispatch({ action: "set_viewport_view_preset", preset: p });
        }
        return;
      }

      const mayaWsBtn = ev.target.closest ? ev.target.closest("[data-maya-ws]") : null;
      if (mayaWsBtn) {
        const s = studio();
        if (s) s.closeAllMenus();
        hideHotbox();
        const wsName = mayaWsBtn.dataset.mayaWs;
        redockAllFloatingPanels();
        if (outlinerDockedLeft) toggleOutlinerLeftRightDock();
        if (wsName === "maya_classic") {
          setRightEditorMode("split");
          dispatch({ action: "apply_workspace_preset", preset: "maya_classic" });
        } else if (wsName === "rigging") {
          setMenuSet("rigging");
          dispatch({ action: "apply_workspace_preset", preset: "rigging" });
        } else if (s && s.activateWorkspaceById) {
          s.activateWorkspaceById(wsName, true);
        }
        return;
      }

      const mmBtn = ev.target.closest ? ev.target.closest("[data-mm-mode]") : null;
      if (mmBtn && !mmBtn.disabled) {
        ev.stopPropagation();
        const s = studio();
        if (s) s.closeContextMenu();
        dispatch({ action: "set_editor_mode", mode: mmBtn.dataset.mmMode });
        return;
      }

      const pop = document.getElementById("maya-shelf-popup");
      if (pop && !pop.hidden && !pop.contains(ev.target) && ev.target.id !== "btn-shelf-gear") {
        hideShelfPopup();
      }
    });

    // Shelf Gear button
    const gearBtn = document.getElementById("btn-shelf-gear");
    if (gearBtn) {
      gearBtn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        const r = gearBtn.getBoundingClientRect();
        openShelfGearMenu(r.left, r.bottom + 4);
      });
    }

    // Status Line Selection Masks & Snapping
    document.querySelectorAll("[data-sel-mask]").forEach((btn) => {
      btn.addEventListener("click", () => {
        const st = state();
        const m = (st && st.editor && st.editor.selection_mask) || {};
        const key = btn.dataset.selMask;
        const payload = { action: "set_selection_mask" };
        payload[key] = !(m[key] !== false);
        dispatch(payload);
      });
    });

    document.querySelectorAll("[data-snap-toggle]").forEach((btn) => {
      btn.addEventListener("click", () => {
        const st = state();
        const sn = (st && st.editor && st.editor.snapping) || {};
        const key = btn.dataset.snapToggle;
        const payload = { action: "set_snapping" };
        payload[key] = !sn[key];
        dispatch(payload);
      });
    });

    // Right Editor Mode buttons
    document.querySelectorAll("[data-right-mode-btn]").forEach((btn) => {
      btn.addEventListener("click", () => {
        const s = studio();
        if (s) s.setRightDockCollapsed(false);
        setRightEditorMode(btn.dataset.rightModeBtn);
      });
    });

    // Quick Layout buttons on Left Tool Box
    document.querySelectorAll("[data-quick-layout]").forEach((btn) => {
      btn.addEventListener("click", () => {
        const ql = btn.dataset.quickLayout;
        if (ql === "persp") {
          if (outlinerDockedLeft) toggleOutlinerLeftRightDock();
        } else if (ql === "outliner_persp") {
          if (!outlinerDockedLeft) toggleOutlinerLeftRightDock();
        }
      });
    });

    const outlinerDockBtn = document.getElementById("btn-outliner-dock-toggle");
    if (outlinerDockBtn) {
      outlinerDockBtn.addEventListener("click", toggleOutlinerLeftRightDock);
    }

    // Viewport Panel Toolbar toggles
    const bindSimpleBtn = (id, fn) => {
      const el = document.getElementById(id);
      if (el) el.addEventListener("click", fn);
    };
    bindSimpleBtn("btn-tool-last", () => executeMayaCommand("repeat_last_tool"));
    bindSimpleBtn("btn-render-frame-preview", openRenderViewModal);
    bindSimpleBtn("btn-close-render-modal", () => {
      const m = document.getElementById("maya-render-modal");
      if (m) m.hidden = true;
    });
    bindSimpleBtn("btn-vp-wos", () => executeMayaCommand("toggle_wos"));
    bindSimpleBtn("btn-vp-default-mat", () => executeMayaCommand("toggle_default_mat"));
    bindSimpleBtn("btn-vp-all-lights", () => {
      const st = state();
      const cur = st && st.viewport ? st.viewport.use_all_lights !== false : true;
      dispatch({ action: "set_viewport_display", use_all_lights: !cur });
    });
    bindSimpleBtn("btn-vp-xray", () => executeMayaCommand("toggle_xray"));
    bindSimpleBtn("btn-vp-isolate", () => executeMayaCommand("toggle_isolate"));
    bindSimpleBtn("btn-vp-grid", () => dispatch({ action: "toggle_editor_grid" }));
    bindSimpleBtn("btn-vp-helpers", () => dispatch({ action: "toggle_editor_helpers" }));

    const vpCamSel = document.getElementById("vp-camera-select");
    if (vpCamSel) {
      vpCamSel.addEventListener("change", () => {
        const v = vpCamSel.value;
        if (v === "active_cam") {
          dispatch({ action: "set_camera_view", active: true });
        } else if (v === "persp") {
          dispatch({ action: "set_camera_view", active: false });
          dispatch({ action: "set_viewport_projection", projection: "perspective" });
        } else {
          dispatch({ action: "set_camera_view", active: false });
          dispatch({ action: "set_viewport_projection", projection: "orthographic" });
          dispatch({ action: "set_viewport_view_preset", preset: v });
        }
      });
    }

    // Channel Box & Custom Attribute controls
    bindSimpleBtn("btn-cb-key-all", () => executeMayaCommand("set_keyframe"));
    bindSimpleBtn("btn-cb-freeze", () => executeMayaCommand("freeze_transforms"));
    bindSimpleBtn("btn-cb-add-attr-toggle", () => {
      const bar = document.getElementById("maya-add-attr-bar");
      if (bar) bar.hidden = !bar.hidden;
    });
    bindSimpleBtn("btn-cb-create-attr", () => {
      const nEl = document.getElementById("cb-new-attr-name");
      const vEl = document.getElementById("cb-new-attr-val");
      const name = nEl ? nEl.value.trim() : "";
      const val = vEl ? Number(vEl.value) || 0 : 0;
      if (name) {
        dispatch({ action: "add_custom_attribute", name: name, value: val });
        if (nEl) nEl.value = "";
      }
    });
    bindSimpleBtn("btn-cb-lock-node", () => {
      const st = state();
      if (st && st.inspector) {
        dispatch({ action: "set_object_locked", id: st.inspector.id, locked: !st.inspector.locked });
      }
    });

    // Display Layer Editor buttons
    bindSimpleBtn("btn-layer-create-empty", () => {
      dispatch({ action: "create_display_layer", assign_selected: false });
    });
    bindSimpleBtn("btn-layer-create-selected", () => {
      dispatch({ action: "create_display_layer", assign_selected: true });
    });
    bindSimpleBtn("btn-layer-assign", () => {
      if (activeLayerId) dispatch({ action: "assign_to_display_layer", layer_id: activeLayerId });
    });

    // Tool Settings inputs
    const tsSpace = document.getElementById("ts-axis-space");
    if (tsSpace) {
      tsSpace.addEventListener("change", () => {
        dispatch({ action: "set_gizmo_space", space: tsSpace.value });
      });
    }
    const tsGrid = document.getElementById("ts-grid-step");
    if (tsGrid) {
      tsGrid.addEventListener("change", () => {
        const g = Number(tsGrid.value);
        if (Number.isFinite(g) && g >= 0.05) {
          dispatch({ action: "set_snapping", grid_step: g });
        }
      });
    }
    bindSimpleBtn("btn-reset-tool-settings", () => {
      dispatch({ action: "set_gizmo_space", space: "world" });
      dispatch({ action: "set_snapping", grid: false, grid_step: 0.5, point: false, curve: false, surface: false });
    });

    // Maya Time Slider & Range Slider Transport
    bindSimpleBtn("btn-tl-start", () => executeMayaCommand("goto_start"));
    bindSimpleBtn("btn-tl-prev", () => executeMayaCommand("step_prev"));
    bindSimpleBtn("btn-tl-next", () => executeMayaCommand("step_next"));
    bindSimpleBtn("btn-tl-end", () => executeMayaCommand("goto_end"));
    bindSimpleBtn("btn-tl-set-key", () => executeMayaCommand("set_keyframe"));
    bindSimpleBtn("btn-tl-del-key", () => executeMayaCommand("delete_keyframe"));
    bindSimpleBtn("btn-tl-autokey", () => executeMayaCommand("toggle_autokey"));

    const bindTimelineField = (id, key) => {
      const el = document.getElementById(id);
      if (!el) return;
      el.addEventListener("change", () => {
        const v = Number(el.value);
        if (!Number.isFinite(v)) return;
        const payload = { action: "set_timeline" };
        payload[key] = v;
        dispatch(payload);
      });
    };
    bindTimelineField("tl-start-frame", "start_frame");
    bindTimelineField("tl-play-start", "playback_start");
    bindTimelineField("tl-play-end", "playback_end");
    bindTimelineField("tl-end-frame", "end_frame");
    bindTimelineField("tl-fps-select", "fps");

    const loopSel = document.getElementById("tl-loop-select");
    if (loopSel) {
      loopSel.addEventListener("change", () => {
        dispatch({ action: "set_timeline", playback_mode: loopSel.value });
      });
    }

    // Safe Command Line Form & History
    const cmdForm = document.getElementById("maya-command-form");
    const cmdInput = document.getElementById("maya-command-input");
    if (cmdForm && cmdInput) {
      cmdForm.addEventListener("submit", (ev) => {
        ev.preventDefault();
        const text = cmdInput.value.trim();
        if (!text) return;
        commandHistory.push(text);
        if (commandHistory.length > 50) commandHistory.shift();
        commandHistoryIdx = commandHistory.length;
        saveStoredJson(CMD_HISTORY_KEY, commandHistory);
        dispatch({ action: "execute_command_line", command: text });
        cmdInput.value = "";
      });

      cmdInput.addEventListener("keydown", (ev) => {
        if (ev.key === "ArrowUp" && commandHistory.length > 0) {
          ev.preventDefault();
          commandHistoryIdx = Math.max(0, commandHistoryIdx - 1);
          cmdInput.value = commandHistory[commandHistoryIdx] || "";
        } else if (ev.key === "ArrowDown" && commandHistory.length > 0) {
          ev.preventDefault();
          commandHistoryIdx = Math.min(commandHistory.length, commandHistoryIdx + 1);
          cmdInput.value = commandHistory[commandHistoryIdx] || "";
        }
      });
    }

    // Hotkey Reset
    bindSimpleBtn("btn-reset-hotkeys", () => {
      hotkeys = DEFAULT_HOTKEYS.map((h) => Object.assign({}, h));
      saveStoredJson(HOTKEYS_STORAGE_KEY, hotkeys);
      renderHotkeyEditor();
    });

    // =========================================================================
    // MAYA ALT + MOUSE VIEWPORT NAVIGATION (Alt+LMB Orbit, Alt+MMB Pan, Alt+RMB Zoom)
    // =========================================================================
    const vpCanvas = document.getElementById("fe3d-canvas");
    if (vpCanvas) {
      let navDrag = null;
      vpCanvas.addEventListener(
        "pointerdown",
        (ev) => {
          if (ev.pointerType !== "mouse") return;
          const s = studio();
          if (!s || !s.wasm) return;
          if (ev.altKey || ev.button === 1) {
            ev.preventDefault();
            ev.stopImmediatePropagation();
            s.closeAllMenus();
            s.closeContextMenu();
            const mode =
              ev.button === 1
                ? "pan"
                : ev.button === 2
                ? "dolly"
                : "orbit";
            navDrag = { id: ev.pointerId, x: ev.clientX, y: ev.clientY, mode: mode };
            try {
              vpCanvas.setPointerCapture(ev.pointerId);
            } catch (_) {}
          }
        },
        true
      );

      window.addEventListener(
        "pointermove",
        (ev) => {
          if (!navDrag || ev.pointerId !== navDrag.id) return;
          const s = studio();
          if (!s || !s.wasm) return;
          const dx = ev.clientX - navDrag.x;
          const dy = ev.clientY - navDrag.y;
          navDrag.x = ev.clientX;
          navDrag.y = ev.clientY;
          if (navDrag.mode === "orbit") {
            s.wasm.cameraOrbit(dx, dy);
          } else if (navDrag.mode === "pan") {
            s.wasm.cameraPan(dx, dy);
          } else if (navDrag.mode === "dolly") {
            s.wasm.cameraZoom((dx - dy) * 0.045);
          }
        },
        true
      );

      const endNavDrag = (ev) => {
        if (!navDrag || ev.pointerId !== navDrag.id) return;
        ev.stopImmediatePropagation();
        try {
          vpCanvas.releasePointerCapture(ev.pointerId);
        } catch (_) {}
        navDrag = null;
        const s = studio();
        if (s && s.getState) s.getState();
      };
      window.addEventListener("pointerup", endNavDrag, true);
      window.addEventListener("pointercancel", endNavDrag, true);
    }

    // =========================================================================
    // GLOBAL MAYA KEYBOARD SHORTCUT & HOLD-SPACEBAR HOTBOX ROUTER
    // =========================================================================
    window.addEventListener("keydown", (ev) => {
      const s = studio();
      if (!s || s.getAppView() !== "studio") return;
      if (isTextEditingElement(ev.target) || isTextEditingElement(document.activeElement)) return;

      const ctrl = ev.ctrlKey || ev.metaKey;
      const shift = ev.shiftKey;

      // Hold Spacebar -> Maya Hotbox
      if (ev.code === "Space" && !ctrl && !shift && !ev.altKey) {
        ev.preventDefault();
        if (!spacebarHeld) {
          spacebarHeld = true;
          showHotbox();
        }
        return;
      }

      if (ev.code === "Escape") {
        hideHotbox();
        hideShelfPopup();
        return;
      }

      if (ev.repeat) return;

      // Fixed universal shortcuts
      if (ctrl && ev.code === "KeyZ" && !shift) {
        ev.preventDefault();
        dispatch({ action: "undo" });
        return;
      }
      if ((ctrl && ev.code === "KeyY") || (ctrl && shift && ev.code === "KeyZ")) {
        ev.preventDefault();
        dispatch({ action: "redo" });
        return;
      }
      if (ctrl && ev.code === "KeyS") {
        ev.preventDefault();
        s.exportPortableFe3d();
        return;
      }
      if (ctrl && ev.code === "KeyA") {
        ev.preventDefault();
        dispatch({ action: "select_all" });
        return;
      }
      if (ev.code === "Delete" || ev.code === "Backspace") {
        ev.preventDefault();
        executeMayaCommand("smart_delete");
        return;
      }
      if (ev.code === "F8") {
        ev.preventDefault();
        const st = state();
        const curMode = (st && st.editor && st.editor.mode) || "object";
        dispatch({ action: "set_editor_mode", mode: curMode === "object" ? "vertex" : "object" });
        return;
      }
      if (ev.code === "F9")  { ev.preventDefault(); dispatch({ action: "set_editor_mode", mode: "vertex" }); return; }
      if (ev.code === "F10") { ev.preventDefault(); dispatch({ action: "set_editor_mode", mode: "edge" }); return; }
      if (ev.code === "F11") { ev.preventDefault(); dispatch({ action: "set_editor_mode", mode: "face" }); return; }
      if (ev.code === "F12") { ev.preventDefault(); executeMayaCommand("open_uv_editor"); return; }
      if (!ctrl && (ev.code === "Digit1" || ev.code === "Digit2" || ev.code === "Digit3")) {
        ev.preventDefault();
        dispatch({
          action: "set_viewport_display",
          mesh_display_smoothness: Number(ev.code.slice(-1)),
        });
        return;
      }
      if (!ctrl && ev.code === "KeyH") {
        ev.preventDefault();
        executeMayaCommand(shift ? "show_selection" : "hide_selection");
        return;
      }
      if (!ctrl && ev.code === "KeyX") {
        ev.preventDefault();
        const st = state();
        dispatch({ action: "set_snapping", grid: !(st && st.editor && st.editor.snapping && st.editor.snapping.grid) });
        return;
      }
      if (!ctrl && ev.code === "KeyC") {
        ev.preventDefault();
        const st = state();
        dispatch({ action: "set_snapping", curve: !(st && st.editor && st.editor.snapping && st.editor.snapping.curve) });
        return;
      }
      if (!ctrl && ev.code === "KeyV") {
        ev.preventDefault();
        const st = state();
        dispatch({ action: "set_snapping", point: !(st && st.editor && st.editor.snapping && st.editor.snapping.point) });
        return;
      }

      // Check customizable Maya hotkey map
      for (let i = 0; i < hotkeys.length; ++i) {
        const hk = hotkeys[i];
        if (hk.key === ev.code && Boolean(hk.ctrl) === ctrl && Boolean(hk.shift) === shift && !ev.altKey) {
          ev.preventDefault();
          executeHotkeyById(hk.id);
          return;
        }
      }
    });

    window.addEventListener("keyup", (ev) => {
      if (ev.code === "Space" && spacebarHeld) {
        hideHotbox();
      }
    });
    window.addEventListener("blur", hideHotbox);

    // Bind Left Dock Splitter resize
    const splitLeft = document.getElementById("splitter-left");
    const dockLeft = document.getElementById("dock-left");
    if (splitLeft && dockLeft) {
      splitLeft.addEventListener("pointerdown", (ev) => {
        if (ev.button !== 0 && ev.pointerType === "mouse") return;
        ev.preventDefault();
        splitLeft.setPointerCapture(ev.pointerId);
        const startX = ev.clientX;
        const startW = dockLeft.getBoundingClientRect().width || 220;
        function onMove(mev) {
          const nextW = Math.max(160, Math.min(400, Math.round(startW + (mev.clientX - startX))));
          dockLeft.style.width = nextW + "px";
        }
        function onUp(uev) {
          splitLeft.removeEventListener("pointermove", onMove);
          splitLeft.removeEventListener("pointerup", onUp);
          try {
            splitLeft.releasePointerCapture(uev.pointerId);
          } catch (_) {}
        }
        splitLeft.addEventListener("pointermove", onMove);
        splitLeft.addEventListener("pointerup", onUp);
      });
    }

    // Hook into C++20 Engine state sync
    const attachTimer = setInterval(() => {
      const s = studio();
      if (s && s.onSync) {
        clearInterval(attachTimer);
        if (!window.localStorage.getItem("fe3d.workspace.v2")) {
          dispatch({ action: "set_panel_visible", panel: "timeline", visible: true });
          if (s.setBottomEditor) s.setBottomEditor("timeline");
        }
        s.onSync((st) => {
          syncToolAndViewportBars(st);
          renderChannelBox(st);
          renderDisplayLayers(st);
          renderMayaTimeSlider(st);
        });
        const initial = s.getState();
        if (initial) {
          syncToolAndViewportBars(initial);
          renderChannelBox(initial);
          renderDisplayLayers(initial);
          renderMayaTimeSlider(initial);
        }
      }
    }, 30);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initMayaWorkspace);
  } else {
    initMayaWorkspace();
  }
})();
