// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Connected Browser Studio Controller (Stage 4 v0.4.0)
// Bridges DOM controls directly to the C++20 WebAssembly EditorController & StudioSession.

(function () {
  "use strict";

  const WORKSPACE_STORAGE_KEY = "fe3d.workspace.v1";
  const canvas = document.getElementById("fe3d-canvas");
  const banner = document.getElementById("viewport-banner");
  const previewCanvas = document.getElementById("material-preview-canvas");
  const previewCtx = previewCanvas ? previewCanvas.getContext("2d") : null;

  // Configure Emscripten Module object BEFORE finaleffect_web.js executes.
  window.Module = {
    canvas: canvas,
    print: function (text) {
      console.log("[FE3D]", text);
    },
    printErr: function (text) {
      console.warn("[FE3D]", text);
    },
  };

  const els = {
    engineBadge: document.getElementById("engine-version-badge"),
    studioWorkspace: document.getElementById("studio-workspace"),
    dockLeft: document.getElementById("dock-left"),
    dockRight: document.getElementById("dock-right"),
    dockBottom: document.getElementById("dock-bottom"),
    panelOutliner: document.getElementById("panel-outliner"),
    panelProperties: document.getElementById("panel-properties"),
    panelMaterialEditor: document.getElementById("panel-material-editor"),

    btnNew: document.getElementById("btn-new-project"),
    btnOpen: document.getElementById("btn-open-project"),
    fileOpenInput: document.getElementById("file-open-input"),
    btnExportFe3d: document.getElementById("btn-export-fe3d"),
    btnExportBundle: document.getElementById("btn-export-bundle"),
    btnUndo: document.getElementById("btn-undo"),
    btnRedo: document.getElementById("btn-redo"),
    btnDuplicate: document.getElementById("btn-duplicate"),
    btnDelete: document.getElementById("btn-delete"),
    btnFrame: document.getElementById("btn-frame"),
    btnToggleRig: document.getElementById("btn-toggle-rig"),
    btnClearSelection: document.getElementById("btn-clear-selection"),

    btnModeObject: document.getElementById("btn-mode-object"),
    btnModeVertex: document.getElementById("btn-mode-vertex"),
    btnModeEdge: document.getElementById("btn-mode-edge"),
    btnModeFace: document.getElementById("btn-mode-face"),

    btnGizmoTranslate: document.getElementById("btn-gizmo-translate"),
    btnGizmoRotate: document.getElementById("btn-gizmo-rotate"),
    btnGizmoScale: document.getElementById("btn-gizmo-scale"),
    btnGizmoSpace: document.getElementById("btn-gizmo-space"),
    workspacePresetSelect: document.getElementById("workspace-preset-select"),

    outlinerList: document.getElementById("outliner-list"),
    metaProjectName: document.getElementById("meta-project-name"),
    metaAuthor: document.getElementById("meta-author"),
    metaSceneName: document.getElementById("meta-scene-name"),

    vpCameraPill: document.getElementById("vp-camera-pill"),
    vpSelectionPill: document.getElementById("vp-selection-pill"),
    vpModePill: document.getElementById("vp-mode-pill"),

    tabAssetBrowser: document.getElementById("tab-asset-browser"),
    tabTimeline: document.getElementById("tab-timeline"),
    tabConsole: document.getElementById("tab-console"),
    paneAssetBrowser: document.getElementById("pane-asset-browser"),
    paneTimeline: document.getElementById("pane-timeline"),
    paneConsole: document.getElementById("pane-console"),
    assetBrowserList: document.getElementById("asset-browser-list"),
    btnTimelinePlay: document.getElementById("btn-timeline-play"),
    consoleLogList: document.getElementById("console-log-list"),

    inspectorIdBadge: document.getElementById("inspector-id-badge"),
    inspectorEmpty: document.getElementById("inspector-empty"),
    inspectorBody: document.getElementById("inspector-body"),
    inspName: document.getElementById("insp-name"),
    inspParent: document.getElementById("insp-parent"),
    inspMaterialRow: document.getElementById("insp-material-row"),
    inspMaterial: document.getElementById("insp-material"),
    inspVisible: document.getElementById("insp-visible"),
    btnResetTransform: document.getElementById("btn-reset-transform"),

    posX: document.getElementById("tf-pos-x"),
    posY: document.getElementById("tf-pos-y"),
    posZ: document.getElementById("tf-pos-z"),
    rotX: document.getElementById("tf-rot-x"),
    rotY: document.getElementById("tf-rot-y"),
    rotZ: document.getElementById("tf-rot-z"),
    sclX: document.getElementById("tf-scl-x"),
    sclY: document.getElementById("tf-scl-y"),
    sclZ: document.getElementById("tf-scl-z"),

    cardMeshEdit: document.getElementById("card-mesh-edit"),
    meshEditSelectionTag: document.getElementById("mesh-edit-selection-tag"),
    meshEditTopology: document.getElementById("mesh-edit-topology"),
    meshExtrudeDist: document.getElementById("mesh-extrude-dist"),
    btnMeshExtrude: document.getElementById("btn-mesh-extrude"),
    btnMeshBoundary: document.getElementById("btn-mesh-boundary"),
    btnMeshFill: document.getElementById("btn-mesh-fill"),
    btnMeshAssignMat: document.getElementById("btn-mesh-assign-mat"),
    btnMeshDelete: document.getElementById("btn-mesh-delete"),

    cardMesh: document.getElementById("card-mesh"),
    meshAssetName: document.getElementById("mesh-asset-name"),
    meshGeoCounts: document.getElementById("mesh-geo-counts"),
    meshBounds: document.getElementById("mesh-bounds"),

    cardCamera: document.getElementById("card-camera"),
    btnSetActiveCamera: document.getElementById("btn-set-active-camera"),
    camProjection: document.getElementById("cam-projection"),
    camFov: document.getElementById("cam-fov"),
    camOrtho: document.getElementById("cam-ortho"),
    camNear: document.getElementById("cam-near"),
    camFar: document.getElementById("cam-far"),

    cardLight: document.getElementById("card-light"),
    lightColor: document.getElementById("light-color"),
    lightIntensity: document.getElementById("light-intensity"),

    btnMatNew: document.getElementById("btn-mat-new"),
    btnMatDup: document.getElementById("btn-mat-dup"),
    btnMatDel: document.getElementById("btn-mat-del"),
    matSelect: document.getElementById("mat-select"),
    matName: document.getElementById("mat-name"),
    btnMatAssignObj: document.getElementById("btn-mat-assign-obj"),
    matBaseColor: document.getElementById("mat-base-color"),
    matMetallic: document.getElementById("mat-metallic"),
    valMatMetallic: document.getElementById("val-mat-metallic"),
    matRoughness: document.getElementById("mat-roughness"),
    valMatRoughness: document.getElementById("val-mat-roughness"),
    matAo: document.getElementById("mat-ao"),
    valMatAo: document.getElementById("val-mat-ao"),
    matEmissiveColor: document.getElementById("mat-emissive-color"),
    matEmissiveStrength: document.getElementById("mat-emissive-strength"),
    matOpacity: document.getElementById("mat-opacity"),
    valMatOpacity: document.getElementById("val-mat-opacity"),

    statObjectsMeshes: document.getElementById("stat-objects-meshes"),
    statMaterialsUpdates: document.getElementById("stat-materials-updates"),
    statTriangles: document.getElementById("stat-triangles"),
    statMemory: document.getElementById("stat-memory"),
    statHistory: document.getElementById("stat-history"),

    statusDot: document.getElementById("status-dot"),
    statusMessage: document.getElementById("status-message"),
    statusRenderer: document.getElementById("status-renderer"),
  };

  let wasmApi = null;
  let currentState = null;
  let lastRevision = -1;
  let lastPreviewGen = -1;
  let lastOutlinerSignature = "";
  let lastAssetsSignature = "";
  let lastConsoleSignature = "";
  let activeBottomTab = "asset_browser";

  function tryBindWasmApi() {
    const M = window.Module;
    if (!M || typeof M._fe3d_is_ready !== "function" || typeof M.ccall !== "function") {
      return false;
    }
    const ready = M._fe3d_is_ready();
    if (ready === 0) {
      return false;
    }
    if (ready < 0) {
      const errMsg = M.ccall("fe3d_get_init_error", "string", [], []);
      banner.hidden = false;
      banner.classList.add("is-error");
      banner.textContent = "WebGL 2.0 initialization failed: " + (errMsg || "Unsupported graphics context");
      els.statusDot.classList.add("is-error");
      els.statusMessage.textContent = banner.textContent;
      return true;
    }

    wasmApi = {
      getRevision: function () {
        return typeof M._fe3d_state_revision === "function" ? M._fe3d_state_revision() : -1;
      },
      getStateJson: function () {
        return M.ccall("fe3d_get_state_json", "string", [], []);
      },
      dispatchAction: function (payloadObj) {
        const ok = M.ccall("fe3d_dispatch_action", "number", ["string"], [JSON.stringify(payloadObj)]);
        syncFromEngine(true);
        return ok === 1;
      },
      exportInlineProject: function () {
        return M.ccall("fe3d_export_inline_project", "string", [], []);
      },
      exportWorkspaceJson: function () {
        return M.ccall("fe3d_export_workspace_json", "string", [], []);
      },
      saveBundleToVfs: function (vfsPath) {
        return M.ccall("fe3d_save_bundle_to_vfs", "number", ["string"], [vfsPath]) === 1;
      },
      loadFileFromVfs: function (vfsPath) {
        const ok = M.ccall("fe3d_load_file_from_vfs", "number", ["string"], [vfsPath]) === 1;
        syncFromEngine(true);
        return ok;
      },
      blitMaterialPreview: function () {
        if (!previewCtx || typeof M._fe3d_material_preview_ptr !== "function") return;
        const gen = M._fe3d_material_preview_generation();
        if (gen === lastPreviewGen) return;
        const ptr = M._fe3d_material_preview_ptr();
        const w = M._fe3d_material_preview_width();
        const h = M._fe3d_material_preview_height();
        if (!ptr || w <= 0 || h <= 0 || !M.HEAPU8) return;
        if (previewCanvas.width !== w || previewCanvas.height !== h) {
          previewCanvas.width = w;
          previewCanvas.height = h;
        }
        const byteLen = w * h * 4;
        const slice = new Uint8ClampedArray(M.HEAPU8.buffer, ptr, byteLen);
        const imgData = new ImageData(new Uint8ClampedArray(slice), w, h);
        previewCtx.putImageData(imgData, 0, 0);
        lastPreviewGen = gen;
      },
      fs: M.FS,
    };

    // Restore persisted workspace layout from localStorage if available.
    try {
      const savedLayout = window.localStorage.getItem(WORKSPACE_STORAGE_KEY);
      if (savedLayout && savedLayout.length > 10) {
        M.ccall(
          "fe3d_dispatch_action",
          "number",
          ["string"],
          [JSON.stringify({ action: "import_workspace_json", json_text: savedLayout })]
        );
      }
    } catch (_) {}

    banner.hidden = true;
    window.__FE3D_STUDIO__ = {
      getState: function () {
        syncFromEngine(true);
        return currentState;
      },
      dispatch: wasmApi.dispatchAction,
    };
    syncFromEngine(true);
    return true;
  }

  function persistWorkspaceToStorage() {
    if (!wasmApi) return;
    try {
      const json = wasmApi.exportWorkspaceJson();
      if (json) {
        window.localStorage.setItem(WORKSPACE_STORAGE_KEY, json);
      }
    } catch (_) {}
  }

  function formatFloat(val, digits) {
    const n = Number(val);
    if (!Number.isFinite(n)) return "0";
    return Number(n.toFixed(digits)).toString();
  }

  function setInputIfUnfocused(inputEl, newValue) {
    if (inputEl && document.activeElement !== inputEl) {
      const strVal = String(newValue);
      if (inputEl.value !== strVal) {
        inputEl.value = strVal;
      }
    }
  }

  function rgbFloatToHex(rgb) {
    if (!Array.isArray(rgb) || rgb.length < 3) return "#ffffff";
    const toByte = (c) => {
      const clamped = Math.max(0, Math.min(1, Number(c) || 0));
      return Math.round(clamped * 255).toString(16).padStart(2, "0");
    };
    return "#" + toByte(rgb[0]) + toByte(rgb[1]) + toByte(rgb[2]);
  }

  function hexToRgbFloat(hex) {
    const clean = (hex || "#ffffff").replace("#", "");
    if (clean.length !== 6) return [1.0, 1.0, 1.0];
    return [
      parseInt(clean.slice(0, 2), 16) / 255.0,
      parseInt(clean.slice(2, 4), 16) / 255.0,
      parseInt(clean.slice(4, 6), 16) / 255.0,
    ];
  }

  function formatBytes(bytes) {
    const b = Number(bytes) || 0;
    if (b < 1024) return b + " B";
    return (b / 1024).toFixed(1) + " KB";
  }

  function objectBadgeText(obj) {
    if (obj.has_camera) return "CAM";
    if (obj.has_light) return "LGT";
    if (obj.has_mesh) return "MSH";
    return "PIV";
  }

  function renderOutliner(state) {
    const selectedId = state.viewport.selected_object_id;
    const sig = JSON.stringify([selectedId, state.objects]);
    if (sig === lastOutlinerSignature) {
      return;
    }
    lastOutlinerSignature = sig;

    const frag = document.createDocumentFragment();
    for (const obj of state.objects) {
      const row = document.createElement("div");
      row.className =
        "outliner-row" +
        (obj.id === selectedId ? " is-selected" : "") +
        (!obj.visible ? " is-hidden-obj" : "");
      row.dataset.objectId = String(obj.id);
      row.dataset.testid = "outliner-item-" + obj.name;
      row.style.paddingLeft = 6 + obj.depth * 14 + "px";
      row.setAttribute("role", "treeitem");
      row.setAttribute("aria-selected", obj.id === selectedId ? "true" : "false");

      const main = document.createElement("div");
      main.className = "outliner-main";

      const icon = document.createElement("span");
      icon.className = "obj-icon";
      icon.textContent = objectBadgeText(obj);
      main.appendChild(icon);

      const nameSpan = document.createElement("span");
      nameSpan.className = "obj-name";
      nameSpan.textContent = obj.name;
      main.appendChild(nameSpan);

      if (obj.is_active_camera) {
        const activeTag = document.createElement("span");
        activeTag.className = "obj-cam-tag";
        activeTag.textContent = "ACTIVE";
        main.appendChild(activeTag);
      }

      const visBtn = document.createElement("button");
      visBtn.type = "button";
      visBtn.className = "vis-btn";
      visBtn.dataset.testid = "toggle-vis-" + obj.name;
      visBtn.title = obj.visible ? "Hide object" : "Show object";
      visBtn.textContent = obj.visible ? "Eye" : "Off";
      visBtn.addEventListener("click", function (ev) {
        ev.stopPropagation();
        wasmApi.dispatchAction({
          action: "set_visibility",
          id: obj.id,
          visible: !obj.visible,
        });
      });

      row.addEventListener("click", function () {
        wasmApi.dispatchAction({
          action: "select_object",
          id: obj.id,
        });
      });

      row.appendChild(main);
      row.appendChild(visBtn);
      frag.appendChild(row);
    }

    els.outlinerList.replaceChildren(frag);
  }

  function renderParentSelect(state, inspector) {
    if (document.activeElement === els.inspParent) return;
    els.inspParent.innerHTML = "";
    const rootOpt = document.createElement("option");
    rootOpt.value = "0";
    rootOpt.textContent = "<Scene Root>";
    els.inspParent.appendChild(rootOpt);

    for (const obj of state.objects) {
      if (obj.id === inspector.id) continue;
      const opt = document.createElement("option");
      opt.value = String(obj.id);
      opt.textContent = obj.name + " (#" + obj.id + ")";
      els.inspParent.appendChild(opt);
    }
    els.inspParent.value = String(inspector.parent_id || 0);
  }

  function renderObjectMaterialSelect(state, inspector) {
    if (document.activeElement === els.inspMaterial) return;
    els.inspMaterial.innerHTML = "";
    const noneOpt = document.createElement("option");
    noneOpt.value = "0";
    noneOpt.textContent = "<Default Vertex Color>";
    els.inspMaterial.appendChild(noneOpt);

    for (const mat of state.materials || []) {
      const opt = document.createElement("option");
      opt.value = String(mat.id);
      opt.textContent = mat.name + " (#" + mat.id + ")";
      els.inspMaterial.appendChild(opt);
    }
    els.inspMaterial.value = String(inspector.material_id || 0);
  }

  function renderInspector(state) {
    const insp = state.inspector;
    const hasSelection = insp !== null && typeof insp === "object";

    els.btnDuplicate.disabled = !hasSelection;
    els.btnDelete.disabled = !hasSelection || (insp && insp.id === state.viewport.active_camera_id);

    if (!hasSelection) {
      els.inspectorIdBadge.textContent = "None";
      els.inspectorEmpty.hidden = false;
      els.inspectorBody.hidden = true;
      els.vpSelectionPill.textContent = "No Selection";
      return;
    }

    els.inspectorEmpty.hidden = true;
    els.inspectorBody.hidden = false;
    els.inspectorIdBadge.textContent = "ID #" + insp.id;
    els.vpSelectionPill.textContent = "Selected: " + insp.name;

    setInputIfUnfocused(els.inspName, insp.name);
    if (document.activeElement !== els.inspVisible) {
      els.inspVisible.checked = Boolean(insp.visible);
    }
    renderParentSelect(state, insp);

    if (insp.mesh) {
      els.inspMaterialRow.hidden = false;
      renderObjectMaterialSelect(state, insp);
    } else {
      els.inspMaterialRow.hidden = true;
    }

    setInputIfUnfocused(els.posX, formatFloat(insp.position[0], 3));
    setInputIfUnfocused(els.posY, formatFloat(insp.position[1], 3));
    setInputIfUnfocused(els.posZ, formatFloat(insp.position[2], 3));

    setInputIfUnfocused(els.rotX, formatFloat(insp.rotation_deg[0], 2));
    setInputIfUnfocused(els.rotY, formatFloat(insp.rotation_deg[1], 2));
    setInputIfUnfocused(els.rotZ, formatFloat(insp.rotation_deg[2], 2));

    setInputIfUnfocused(els.sclX, formatFloat(insp.scale[0], 3));
    setInputIfUnfocused(els.sclY, formatFloat(insp.scale[1], 3));
    setInputIfUnfocused(els.sclZ, formatFloat(insp.scale[2], 3));

    // Mesh Edit modeling card when in Vertex / Edge / Face mode
    const ed = state.editor;
    if (ed && ed.mode !== "object" && ed.editable_mesh && ed.editable_mesh.editable) {
      els.cardMeshEdit.hidden = false;
      const selCount =
        ed.mode === "vertex"
          ? ed.mesh_selection.vertices.length
          : ed.mode === "edge"
          ? ed.mesh_selection.edges.length
          : ed.mesh_selection.faces.length;
      els.meshEditSelectionTag.textContent = selCount + " " + ed.mode + (selCount === 1 ? "" : "s");
      els.meshEditTopology.textContent =
        ed.editable_mesh.face_count +
        " faces · " +
        ed.editable_mesh.edge_count +
        " edges · " +
        ed.editable_mesh.vertex_count +
        " verts";
      els.btnMeshExtrude.disabled = ed.mode !== "face" || selCount === 0;
      els.btnMeshBoundary.disabled = ed.editable_mesh.open_boundary_edges < 3;
      els.btnMeshFill.disabled = ed.mode !== "vertex" || selCount < 3;
      els.btnMeshAssignMat.disabled = ed.mode !== "face" || selCount === 0 || !state.active_material_id;
      els.btnMeshDelete.disabled = selCount === 0;
    } else {
      els.cardMeshEdit.hidden = true;
    }

    if (insp.mesh) {
      els.cardMesh.hidden = false;
      els.meshAssetName.textContent = insp.mesh.name + " (#" + insp.mesh.id + ")";
      els.meshGeoCounts.textContent =
        insp.mesh.vertex_count + " verts · " + insp.mesh.triangle_count + " tris (" + insp.mesh.face_count + " faces)";
      els.meshBounds.textContent =
        formatFloat(insp.mesh.bounds_size[0], 2) +
        " × " +
        formatFloat(insp.mesh.bounds_size[1], 2) +
        " × " +
        formatFloat(insp.mesh.bounds_size[2], 2);
    } else {
      els.cardMesh.hidden = true;
    }

    if (insp.camera) {
      els.cardCamera.hidden = false;
      els.btnSetActiveCamera.disabled = Boolean(insp.camera.is_active);
      els.btnSetActiveCamera.textContent = insp.camera.is_active ? "Active" : "Set Active";
      setInputIfUnfocused(els.camProjection, insp.camera.projection);
      setInputIfUnfocused(els.camFov, formatFloat(insp.camera.fov_deg, 1));
      setInputIfUnfocused(els.camOrtho, formatFloat(insp.camera.ortho_height, 2));
      setInputIfUnfocused(els.camNear, formatFloat(insp.camera.near_plane, 3));
      setInputIfUnfocused(els.camFar, formatFloat(insp.camera.far_plane, 1));
    } else {
      els.cardCamera.hidden = true;
    }

    if (insp.light) {
      els.cardLight.hidden = false;
      setInputIfUnfocused(els.lightColor, rgbFloatToHex(insp.light.color));
      setInputIfUnfocused(els.lightIntensity, formatFloat(insp.light.intensity, 2));
    } else {
      els.cardLight.hidden = true;
    }
  }

  function renderMaterialEditor(state) {
    const mats = state.materials || [];
    const activeId = state.active_material_id || 0;

    if (document.activeElement !== els.matSelect) {
      els.matSelect.innerHTML = "";
      for (const m of mats) {
        const opt = document.createElement("option");
        opt.value = String(m.id);
        opt.textContent = m.name + " (#" + m.id + ")";
        els.matSelect.appendChild(opt);
      }
      if (activeId > 0) {
        els.matSelect.value = String(activeId);
      }
    }

    const activeMat = mats.find((m) => m.id === activeId) || null;
    els.btnMatDup.disabled = !activeMat;
    els.btnMatDel.disabled = !activeMat;
    els.btnMatAssignObj.disabled = !activeMat || !state.inspector || !state.inspector.mesh;

    if (activeMat) {
      setInputIfUnfocused(els.matName, activeMat.name);
      setInputIfUnfocused(els.matBaseColor, rgbFloatToHex(activeMat.base_color));
      setInputIfUnfocused(els.matMetallic, activeMat.metallic.toFixed(2));
      els.valMatMetallic.textContent = activeMat.metallic.toFixed(2);
      setInputIfUnfocused(els.matRoughness, activeMat.roughness.toFixed(2));
      els.valMatRoughness.textContent = activeMat.roughness.toFixed(2);
      setInputIfUnfocused(els.matAo, activeMat.ambient_occlusion.toFixed(2));
      els.valMatAo.textContent = activeMat.ambient_occlusion.toFixed(2);
      setInputIfUnfocused(els.matEmissiveColor, rgbFloatToHex(activeMat.emissive_color));
      setInputIfUnfocused(els.matEmissiveStrength, formatFloat(activeMat.emissive_strength, 2));
      setInputIfUnfocused(els.matOpacity, activeMat.opacity.toFixed(2));
      els.valMatOpacity.textContent = activeMat.opacity.toFixed(2);
    }

    wasmApi.blitMaterialPreview();
  }

  function renderModeAndGizmoToolbar(state) {
    const ed = state.editor;
    if (!ed) return;
    const canEditMesh = Boolean(ed.editable_mesh && ed.editable_mesh.editable);
    els.btnModeVertex.disabled = !canEditMesh;
    els.btnModeEdge.disabled = !canEditMesh;
    els.btnModeFace.disabled = !canEditMesh;

    els.btnModeObject.classList.toggle("is-active", ed.mode === "object");
    els.btnModeVertex.classList.toggle("is-active", ed.mode === "vertex");
    els.btnModeEdge.classList.toggle("is-active", ed.mode === "edge");
    els.btnModeFace.classList.toggle("is-active", ed.mode === "face");

    els.btnGizmoTranslate.classList.toggle("is-active", ed.gizmo_mode === "translate");
    els.btnGizmoRotate.classList.toggle("is-active", ed.gizmo_mode === "rotate");
    els.btnGizmoScale.classList.toggle("is-active", ed.gizmo_mode === "scale");
    els.btnGizmoSpace.textContent = ed.gizmo_space === "local" ? "Local" : "World";

    const toolLabel = ed.gizmo_mode === "translate" ? "MOVE" : ed.gizmo_mode.toUpperCase();
    const handleHint =
      ed.gizmo_active_handle !== "none"
        ? " · DRAG " + ed.gizmo_active_handle.toUpperCase()
        : ed.gizmo_hovered_handle !== "none"
        ? " · HOVER " + ed.gizmo_hovered_handle.toUpperCase()
        : "";
    els.vpModePill.textContent =
      ed.mode.toUpperCase() + " · " + toolLabel + " (" + ed.gizmo_space.toUpperCase() + ")" + handleHint;
  }

  function renderWorkspaceAndBottomDock(state) {
    const ws = state.workspace;
    if (!ws || !Array.isArray(ws.panels)) return;

    const vis = {};
    for (const p of ws.panels) {
      vis[p.key] = Boolean(p.visible);
    }

    document.querySelectorAll("[data-panel-toggle]").forEach((btn) => {
      const k = btn.dataset.panelToggle;
      btn.classList.toggle("is-on", Boolean(vis[k]));
    });

    if (document.activeElement !== els.workspacePresetSelect) {
      const ln = (ws.layout_name || "").toLowerCase();
      if (ln.includes("modeling")) els.workspacePresetSelect.value = "modeling";
      else if (ln.includes("shading")) els.workspacePresetSelect.value = "shading";
      else if (ln.includes("animation")) els.workspacePresetSelect.value = "animation_prep";
      else els.workspacePresetSelect.value = "default";
    }

    const showLeft = Boolean(vis.scene_outliner);
    const showProps = Boolean(vis.properties);
    const showMat = Boolean(vis.material_editor);
    const showRight = showProps || showMat;

    els.dockLeft.hidden = !showLeft;
    els.panelProperties.hidden = !showProps;
    els.panelMaterialEditor.hidden = !showMat;
    els.dockRight.hidden = !showRight;

    els.studioWorkspace.classList.toggle("hide-left", !showLeft);
    els.studioWorkspace.classList.toggle("hide-right", !showRight);

    const bottomKeys = ["asset_browser", "timeline", "console"];
    const visibleBottom = bottomKeys.filter((k) => Boolean(vis[k]));
    els.dockBottom.hidden = visibleBottom.length === 0;

    els.tabAssetBrowser.hidden = !vis.asset_browser;
    els.tabTimeline.hidden = !vis.timeline;
    els.tabConsole.hidden = !vis.console;

    if (visibleBottom.length > 0 && !visibleBottom.includes(activeBottomTab)) {
      activeBottomTab = visibleBottom[0];
    }
    els.tabAssetBrowser.classList.toggle("is-active", activeBottomTab === "asset_browser");
    els.tabTimeline.classList.toggle("is-active", activeBottomTab === "timeline");
    els.tabConsole.classList.toggle("is-active", activeBottomTab === "console");

    els.paneAssetBrowser.hidden = activeBottomTab !== "asset_browser";
    els.paneTimeline.hidden = activeBottomTab !== "timeline";
    els.paneConsole.hidden = activeBottomTab !== "console";

    // Asset Browser cards (PBR Materials + Quick Primitives)
    const assetsSig = JSON.stringify([state.materials, state.active_material_id]);
    if (assetsSig !== lastAssetsSignature) {
      lastAssetsSignature = assetsSig;
      const frag = document.createDocumentFragment();
      for (const m of state.materials || []) {
        const card = document.createElement("button");
        card.type = "button";
        card.className = "asset-card";
        card.dataset.testid = "asset-mat-" + m.name;
        const sw = document.createElement("span");
        sw.className = "asset-swatch";
        sw.style.background = rgbFloatToHex(m.base_color);
        card.appendChild(sw);
        const lbl = document.createElement("span");
        lbl.textContent = m.name;
        card.appendChild(lbl);
        card.addEventListener("click", () => {
          wasmApi.dispatchAction({ action: "select_material", id: m.id });
        });
        frag.appendChild(card);
      }
      els.assetBrowserList.replaceChildren(frag);
    }

    // Console log lines
    const consoleSig = JSON.stringify(state.console || []);
    if (consoleSig !== lastConsoleSignature) {
      lastConsoleSignature = consoleSig;
      const frag = document.createDocumentFragment();
      const lines = (state.console || []).slice(-18);
      for (const item of lines) {
        const div = document.createElement("div");
        div.className = "console-line" + (item.level === "error" ? " is-error" : "");
        div.textContent = "#" + item.seq + " " + item.message;
        frag.appendChild(div);
      }
      els.consoleLogList.replaceChildren(frag);
    }
  }

  function syncFromEngine(force) {
    if (!wasmApi) return;
    const rev = wasmApi.getRevision();
    if (!force && rev >= 0 && rev === lastRevision) {
      return;
    }
    lastRevision = rev;

    const raw = wasmApi.getStateJson();
    let state = null;
    try {
      state = JSON.parse(raw);
    } catch (err) {
      console.error("Failed to parse engine state JSON:", err);
      return;
    }
    currentState = state;

    els.engineBadge.textContent =
      "v" + state.project.engine_version + " · " + state.viewport.backend_name.split(" ")[0];
    setInputIfUnfocused(els.metaProjectName, state.project.project_name);
    setInputIfUnfocused(els.metaAuthor, state.project.author);
    setInputIfUnfocused(els.metaSceneName, state.project.scene_name);

    els.btnUndo.disabled = !state.history.can_undo;
    els.btnUndo.textContent = state.history.can_undo
      ? "Undo (" + state.history.undo_count + ")"
      : "Undo";
    els.btnUndo.title = state.history.can_undo
      ? "Undo " + state.history.next_undo + " (Ctrl+Z)"
      : "Nothing to undo";

    els.btnRedo.disabled = !state.history.can_redo;
    els.btnRedo.textContent = state.history.can_redo
      ? "Redo (" + state.history.redo_count + ")"
      : "Redo";
    els.btnRedo.title = state.history.can_redo
      ? "Redo " + state.history.next_redo + " (Ctrl+Y)"
      : "Nothing to redo";

    els.btnToggleRig.textContent = state.viewport.rig_playing ? "Pause Rig" : "Play Rig";

    const activeCamObj = state.objects.find((o) => o.id === state.viewport.active_camera_id);
    els.vpCameraPill.textContent =
      "Camera: " +
      (activeCamObj ? activeCamObj.name : "None") +
      " · Dist " +
      formatFloat(state.viewport.camera_distance, 1) +
      "m";

    renderModeAndGizmoToolbar(state);
    renderOutliner(state);
    renderInspector(state);
    renderMaterialEditor(state);
    renderWorkspaceAndBottomDock(state);

    els.statObjectsMeshes.textContent = state.stats.object_count + " / " + state.stats.mesh_count;
    els.statMaterialsUpdates.textContent =
      (state.stats.material_count || 0) + " / " + (state.stats.gpu_buffer_updates || 0);
    els.statTriangles.textContent = String(state.stats.total_triangles);
    els.statMemory.textContent =
      formatBytes(state.stats.cpu_mesh_bytes) + " / " + formatBytes(state.stats.gpu_mesh_bytes);
    els.statHistory.textContent =
      state.history.undo_count + " steps (" + formatBytes(state.history.memory_bytes) + ")";

    const isErr = state.status.level === "error";
    els.statusDot.classList.toggle("is-error", isErr);
    els.statusMessage.textContent = state.status.message;
    els.statusRenderer.textContent = state.viewport.backend_name + " · " + state.viewport.device_name;
  }

  function triggerDownload(filename, blobPart, mimeType) {
    const blob = new Blob([blobPart], { type: mimeType });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 2000);
  }

  function sanitizeFilenameStem(name) {
    const cleaned = (name || "project").trim().replace(/[^a-zA-Z0-9._-]+/g, "_");
    return cleaned.length > 0 ? cleaned : "project";
  }

  function dispatchTransformFromInputs(coalesce) {
    if (!wasmApi || !currentState || !currentState.inspector) return;
    const pos = [Number(els.posX.value), Number(els.posY.value), Number(els.posZ.value)];
    const rot = [Number(els.rotX.value), Number(els.rotY.value), Number(els.rotZ.value)];
    const scl = [Number(els.sclX.value), Number(els.sclY.value), Number(els.sclZ.value)];
    if ([...pos, ...rot, ...scl].some((v) => !Number.isFinite(v))) return;
    wasmApi.dispatchAction({
      action: "set_transform",
      id: currentState.inspector.id,
      position: pos,
      rotation_deg: rot,
      scale: scl,
      coalesce: Boolean(coalesce),
    });
  }

  function dispatchCameraFromInputs() {
    if (!wasmApi || !currentState || !currentState.inspector || !currentState.inspector.camera) return;
    wasmApi.dispatchAction({
      action: "set_camera",
      id: currentState.inspector.id,
      projection: els.camProjection.value,
      fov_deg: Number(els.camFov.value),
      ortho_height: Number(els.camOrtho.value),
      near_plane: Number(els.camNear.value),
      far_plane: Number(els.camFar.value),
    });
  }

  function dispatchLightFromInputs(coalesce) {
    if (!wasmApi || !currentState || !currentState.inspector || !currentState.inspector.light) return;
    wasmApi.dispatchAction({
      action: "set_light",
      id: currentState.inspector.id,
      color: hexToRgbFloat(els.lightColor.value),
      intensity: Number(els.lightIntensity.value),
      coalesce: Boolean(coalesce),
    });
  }

  function dispatchMaterialFromInputs(coalesce) {
    if (!wasmApi || !currentState || !currentState.active_material_id) return;
    wasmApi.dispatchAction({
      action: "update_material",
      id: currentState.active_material_id,
      base_color: hexToRgbFloat(els.matBaseColor.value),
      metallic: Number(els.matMetallic.value),
      roughness: Number(els.matRoughness.value),
      ambient_occlusion: Number(els.matAo.value),
      emissive_color: hexToRgbFloat(els.matEmissiveColor.value),
      emissive_strength: Number(els.matEmissiveStrength.value),
      opacity: Number(els.matOpacity.value),
      coalesce: Boolean(coalesce),
    });
  }

  function dispatchMetadataFromInputs() {
    if (!wasmApi) return;
    wasmApi.dispatchAction({
      action: "set_metadata",
      project_name: els.metaProjectName.value.trim(),
      author: els.metaAuthor.value.trim(),
      scene_name: els.metaSceneName.value.trim(),
    });
  }

  async function importSelectedFiles(fileList) {
    if (!wasmApi || !fileList || fileList.length === 0) return;
    const files = Array.from(fileList);
    try {
      wasmApi.fs.mkdir("/fe3d_import");
    } catch (_) {}

    let fe3dTargetPath = null;
    for (const file of files) {
      const buf = new Uint8Array(await file.arrayBuffer());
      const vfsPath = "/fe3d_import/" + file.name;
      wasmApi.fs.writeFile(vfsPath, buf);
      if (file.name.toLowerCase().endsWith(".fe3d") || file.name.toLowerCase().endsWith(".json")) {
        fe3dTargetPath = vfsPath;
      }
    }

    if (!fe3dTargetPath) {
      els.statusDot.classList.add("is-error");
      els.statusMessage.textContent = "Please include a .fe3d project file when opening a project.";
      return;
    }
    wasmApi.loadFileFromVfs(fe3dTargetPath);
  }

  function bindUiEvents() {
    els.btnNew.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "new_starter_project" });
    });

    els.btnOpen.addEventListener("click", () => {
      els.fileOpenInput.value = "";
      els.fileOpenInput.click();
    });

    els.fileOpenInput.addEventListener("change", () => {
      importSelectedFiles(els.fileOpenInput.files);
    });

    els.btnExportFe3d.addEventListener("click", () => {
      if (!wasmApi || !currentState) return;
      const jsonText = wasmApi.exportInlineProject();
      if (!jsonText) return;
      const stem = sanitizeFilenameStem(currentState.project.project_name);
      triggerDownload(stem + ".fe3d", jsonText, "application/json");
      els.statusDot.classList.remove("is-error");
      els.statusMessage.textContent = "Downloaded " + stem + ".fe3d (inline schema 1.2)";
    });

    els.btnExportBundle.addEventListener("click", () => {
      if (!wasmApi || !currentState) return;
      try {
        wasmApi.fs.mkdir("/fe3d_export");
      } catch (_) {}
      const stem = sanitizeFilenameStem(currentState.project.project_name);
      const fe3dPath = "/fe3d_export/" + stem + ".fe3d";
      const binPath = "/fe3d_export/" + stem + ".fe3dbin";
      if (!wasmApi.saveBundleToVfs(fe3dPath)) {
        syncFromEngine(true);
        return;
      }
      const fe3dBytes = wasmApi.fs.readFile(fe3dPath);
      const binBytes = wasmApi.fs.readFile(binPath);
      triggerDownload(stem + ".fe3d", fe3dBytes, "application/json");
      triggerDownload(stem + ".fe3dbin", binBytes, "application/octet-stream");
    });

    els.btnUndo.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "undo" });
    });

    els.btnRedo.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "redo" });
    });

    // Mode & Gizmo buttons
    document.querySelectorAll("[data-edit-mode]").forEach((btn) => {
      btn.addEventListener("click", () => {
        if (!wasmApi) return;
        wasmApi.dispatchAction({ action: "set_editor_mode", mode: btn.dataset.editMode });
      });
    });

    document.querySelectorAll("[data-gizmo-mode]").forEach((btn) => {
      btn.addEventListener("click", () => {
        if (!wasmApi) return;
        wasmApi.dispatchAction({ action: "set_gizmo_mode", mode: btn.dataset.gizmoMode });
      });
    });

    els.btnGizmoSpace.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.editor) return;
      const nextSpace = currentState.editor.gizmo_space === "local" ? "world" : "local";
      wasmApi.dispatchAction({ action: "set_gizmo_space", space: nextSpace });
    });

    // Workspace presets & panel visibility chips
    els.workspacePresetSelect.addEventListener("change", () => {
      if (!wasmApi) return;
      wasmApi.dispatchAction({
        action: "apply_workspace_preset",
        preset: els.workspacePresetSelect.value,
      });
      persistWorkspaceToStorage();
    });

    document.querySelectorAll("[data-panel-toggle]").forEach((btn) => {
      btn.addEventListener("click", () => {
        if (!wasmApi || !currentState || !currentState.workspace) return;
        const key = btn.dataset.panelToggle;
        const p = currentState.workspace.panels.find((item) => item.key === key);
        const nextVis = p ? !p.visible : true;
        wasmApi.dispatchAction({ action: "set_panel_visible", panel: key, visible: nextVis });
        persistWorkspaceToStorage();
      });
    });

    document.querySelectorAll("[data-bottom-tab]").forEach((tab) => {
      tab.addEventListener("click", () => {
        activeBottomTab = tab.dataset.bottomTab;
        if (currentState) renderWorkspaceAndBottomDock(currentState);
      });
    });

    els.btnTimelinePlay.addEventListener("click", () => {
      if (!wasmApi || !currentState) return;
      wasmApi.dispatchAction({
        action: "set_rig_playing",
        playing: !currentState.viewport.rig_playing,
      });
    });

    document.querySelectorAll("[data-preset]").forEach((btn) => {
      btn.addEventListener("click", () => {
        if (!wasmApi) return;
        wasmApi.dispatchAction({ action: "add_preset", preset: btn.dataset.preset });
      });
    });

    els.btnDuplicate.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "duplicate_selected" });
    });

    els.btnDelete.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "delete_selected" });
    });

    els.btnFrame.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "frame_selection" });
    });

    els.btnToggleRig.addEventListener("click", () => {
      if (!wasmApi || !currentState) return;
      wasmApi.dispatchAction({
        action: "set_rig_playing",
        playing: !currentState.viewport.rig_playing,
      });
    });

    els.btnClearSelection.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "select_object", id: 0 });
    });

    // Mesh Edit buttons
    els.btnMeshExtrude.addEventListener("click", () => {
      if (!wasmApi) return;
      const dist = Number(els.meshExtrudeDist.value);
      wasmApi.dispatchAction({
        action: "extrude_selected_faces",
        distance: Number.isFinite(dist) ? dist : 0.35,
      });
    });

    els.btnMeshBoundary.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "select_boundary_loop" });
    });

    els.btnMeshFill.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "fill_selected_face" });
    });

    els.btnMeshAssignMat.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.active_material_id) return;
      wasmApi.dispatchAction({
        action: "assign_face_material",
        material_id: currentState.active_material_id,
      });
    });

    els.btnMeshDelete.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "delete_mesh_selection" });
    });

    // PBR Material Editor controls
    els.matSelect.addEventListener("change", () => {
      if (!wasmApi) return;
      const mid = Number(els.matSelect.value) || 0;
      if (mid > 0) wasmApi.dispatchAction({ action: "select_material", id: mid });
    });

    els.btnMatNew.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "create_material", name: "PBR Material" });
    });

    els.btnMatDup.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.active_material_id) return;
      wasmApi.dispatchAction({ action: "duplicate_material", id: currentState.active_material_id });
    });

    els.btnMatDel.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.active_material_id) return;
      wasmApi.dispatchAction({ action: "delete_material", id: currentState.active_material_id });
    });

    els.matName.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.active_material_id) return;
      wasmApi.dispatchAction({
        action: "rename_material",
        id: currentState.active_material_id,
        name: els.matName.value.trim(),
      });
    });

    els.btnMatAssignObj.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.inspector || !currentState.active_material_id) return;
      wasmApi.dispatchAction({
        action: "assign_object_material",
        object_id: currentState.inspector.id,
        material_id: currentState.active_material_id,
      });
    });

    [
      els.matBaseColor,
      els.matMetallic,
      els.matRoughness,
      els.matAo,
      els.matEmissiveColor,
      els.matEmissiveStrength,
      els.matOpacity,
    ].forEach((el) => {
      el.addEventListener("input", () => dispatchMaterialFromInputs(true));
      el.addEventListener("change", () => dispatchMaterialFromInputs(false));
    });

    // Pressing Enter inside any text/number input commits and blurs cleanly
    document.querySelectorAll('input[type="text"], input[type="number"]').forEach((input) => {
      input.addEventListener("keydown", (ev) => {
        if (ev.key === "Enter") {
          ev.preventDefault();
          input.blur();
        }
      });
    });

    [els.metaProjectName, els.metaAuthor, els.metaSceneName].forEach((input) => {
      input.addEventListener("change", dispatchMetadataFromInputs);
    });

    els.inspName.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      wasmApi.dispatchAction({
        action: "rename_object",
        id: currentState.inspector.id,
        name: els.inspName.value.trim(),
      });
    });

    els.inspVisible.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      wasmApi.dispatchAction({
        action: "set_visibility",
        id: currentState.inspector.id,
        visible: els.inspVisible.checked,
      });
    });

    els.inspParent.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      const newParentId = Number(els.inspParent.value) || 0;
      const ok = wasmApi.dispatchAction({
        action: "reparent_object",
        child_id: currentState.inspector.id,
        parent_id: newParentId,
      });
      if (!ok && currentState.inspector) {
        els.inspParent.value = String(currentState.inspector.parent_id || 0);
      }
    });

    els.inspMaterial.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      const mid = Number(els.inspMaterial.value) || 0;
      wasmApi.dispatchAction({
        action: "assign_object_material",
        object_id: currentState.inspector.id,
        material_id: mid,
      });
    });

    els.btnResetTransform.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      wasmApi.dispatchAction({
        action: "reset_transform",
        id: currentState.inspector.id,
      });
    });

    [
      els.posX, els.posY, els.posZ,
      els.rotX, els.rotY, els.rotZ,
      els.sclX, els.sclY, els.sclZ,
    ].forEach((input) => {
      input.addEventListener("change", () => dispatchTransformFromInputs(false));
    });

    [els.camProjection, els.camFov, els.camOrtho, els.camNear, els.camFar].forEach((el) => {
      el.addEventListener("change", dispatchCameraFromInputs);
    });

    els.btnSetActiveCamera.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      wasmApi.dispatchAction({
        action: "set_active_camera",
        id: currentState.inspector.id,
      });
    });

    els.lightColor.addEventListener("input", () => dispatchLightFromInputs(true));
    els.lightColor.addEventListener("change", () => dispatchLightFromInputs(false));
    els.lightIntensity.addEventListener("input", () => dispatchLightFromInputs(true));
    els.lightIntensity.addEventListener("change", () => dispatchLightFromInputs(false));

    window.addEventListener("mouseup", () => setTimeout(() => syncFromEngine(false), 20));
    window.addEventListener("keyup", () => setTimeout(() => syncFromEngine(false), 20));

    const stage = document.getElementById("viewport-stage");
    stage.addEventListener("dragover", (ev) => ev.preventDefault());
    stage.addEventListener("drop", (ev) => {
      ev.preventDefault();
      if (ev.dataTransfer && ev.dataTransfer.files) {
        importSelectedFiles(ev.dataTransfer.files);
      }
    });
  }

  bindUiEvents();

  const bootTimer = setInterval(() => {
    if (tryBindWasmApi()) {
      clearInterval(bootTimer);
      setInterval(() => syncFromEngine(false), 60);
    }
  }, 40);
})();
