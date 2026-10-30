// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Connected Browser Studio Controller
// Bridges DOM controls directly to the C++20 WebAssembly EditorController & StudioSession.

(function () {
  "use strict";

  const canvas = document.getElementById("fe3d-canvas");
  const banner = document.getElementById("viewport-banner");

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

    outlinerList: document.getElementById("outliner-list"),
    metaProjectName: document.getElementById("meta-project-name"),
    metaAuthor: document.getElementById("meta-author"),
    metaSceneName: document.getElementById("meta-scene-name"),

    vpCameraPill: document.getElementById("vp-camera-pill"),
    vpSelectionPill: document.getElementById("vp-selection-pill"),

    inspectorIdBadge: document.getElementById("inspector-id-badge"),
    inspectorEmpty: document.getElementById("inspector-empty"),
    inspectorBody: document.getElementById("inspector-body"),
    inspName: document.getElementById("insp-name"),
    inspParent: document.getElementById("insp-parent"),
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

    statObjectsMeshes: document.getElementById("stat-objects-meshes"),
    statTriangles: document.getElementById("stat-triangles"),
    statMemory: document.getElementById("stat-memory"),
    statHistory: document.getElementById("stat-history"),

    statusDot: document.getElementById("status-dot"),
    statusMessage: document.getElementById("status-message"),
    statusRenderer: document.getElementById("status-renderer"),
  };

  let wasmApi = null;
  let currentState = null;
  let lastStateRaw = "";
  let lastOutlinerSignature = "";

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
      saveBundleToVfs: function (vfsPath) {
        return M.ccall("fe3d_save_bundle_to_vfs", "number", ["string"], [vfsPath]) === 1;
      },
      loadFileFromVfs: function (vfsPath) {
        const ok = M.ccall("fe3d_load_file_from_vfs", "number", ["string"], [vfsPath]) === 1;
        syncFromEngine(true);
        return ok;
      },
      fs: M.FS,
    };

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

  function formatFloat(val, digits) {
    const n = Number(val);
    if (!Number.isFinite(n)) return "0";
    const fixed = n.toFixed(digits);
    return Number(fixed).toString();
  }

  function setInputIfUnfocused(inputEl, newValue) {
    if (document.activeElement !== inputEl) {
      const strVal = String(newValue);
      if (inputEl.value !== strVal) {
        inputEl.value = strVal;
      }
    }
  }

  function rgbFloatToHex(rgb) {
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
      row.className = "outliner-row" +
        (obj.id === selectedId ? " is-selected" : "") +
        (!obj.visible ? " is-hidden-obj" : "");
      row.dataset.objectId = String(obj.id);
      row.dataset.testid = "outliner-item-" + obj.name;
      row.style.paddingLeft = (6 + obj.depth * 14) + "px";
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
    if (document.activeElement === els.inspParent) {
      return;
    }
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

    setInputIfUnfocused(els.posX, formatFloat(insp.position[0], 3));
    setInputIfUnfocused(els.posY, formatFloat(insp.position[1], 3));
    setInputIfUnfocused(els.posZ, formatFloat(insp.position[2], 3));

    setInputIfUnfocused(els.rotX, formatFloat(insp.rotation_deg[0], 2));
    setInputIfUnfocused(els.rotY, formatFloat(insp.rotation_deg[1], 2));
    setInputIfUnfocused(els.rotZ, formatFloat(insp.rotation_deg[2], 2));

    setInputIfUnfocused(els.sclX, formatFloat(insp.scale[0], 3));
    setInputIfUnfocused(els.sclY, formatFloat(insp.scale[1], 3));
    setInputIfUnfocused(els.sclZ, formatFloat(insp.scale[2], 3));

    // Mesh component card
    if (insp.mesh) {
      els.cardMesh.hidden = false;
      els.meshAssetName.textContent = insp.mesh.name + " (#" + insp.mesh.id + ")";
      els.meshGeoCounts.textContent = insp.mesh.vertex_count + " verts · " + insp.mesh.triangle_count + " tris";
      els.meshBounds.textContent =
        formatFloat(insp.mesh.bounds_size[0], 2) + " × " +
        formatFloat(insp.mesh.bounds_size[1], 2) + " × " +
        formatFloat(insp.mesh.bounds_size[2], 2);
    } else {
      els.cardMesh.hidden = true;
    }

    // Camera component card
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

    // Directional Light component card
    if (insp.light) {
      els.cardLight.hidden = false;
      setInputIfUnfocused(els.lightColor, rgbFloatToHex(insp.light.color));
      setInputIfUnfocused(els.lightIntensity, formatFloat(insp.light.intensity, 2));
    } else {
      els.cardLight.hidden = true;
    }
  }

  function syncFromEngine(force) {
    if (!wasmApi) return;
    const raw = wasmApi.getStateJson();
    if (!force && raw === lastStateRaw) {
      return;
    }
    lastStateRaw = raw;

    let state = null;
    try {
      state = JSON.parse(raw);
    } catch (err) {
      console.error("Failed to parse engine state JSON:", err);
      return;
    }
    currentState = state;

    els.engineBadge.textContent = "v" + state.project.engine_version + " · " + state.viewport.backend_name.split(" ")[0];
    setInputIfUnfocused(els.metaProjectName, state.project.project_name);
    setInputIfUnfocused(els.metaAuthor, state.project.author);
    setInputIfUnfocused(els.metaSceneName, state.project.scene_name);

    // Undo / Redo toolbar buttons
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

    // Rig animation play/pause button
    els.btnToggleRig.textContent = state.viewport.rig_playing ? "Pause Rig" : "Play Rig";

    // Active camera HUD pill
    const activeCamObj = state.objects.find((o) => o.id === state.viewport.active_camera_id);
    els.vpCameraPill.textContent =
      "Camera: " + (activeCamObj ? activeCamObj.name : "None") +
      " · Dist " + formatFloat(state.viewport.camera_distance, 1) + "m";

    renderOutliner(state);
    renderInspector(state);

    // Telemetry
    els.statObjectsMeshes.textContent = state.stats.object_count + " / " + state.stats.mesh_count;
    els.statTriangles.textContent = String(state.stats.total_triangles);
    els.statMemory.textContent =
      formatBytes(state.stats.cpu_mesh_bytes) + " / " + formatBytes(state.stats.gpu_mesh_bytes);
    els.statHistory.textContent =
      state.history.undo_count + " steps (" + formatBytes(state.history.memory_bytes) + ")";

    // Status bar
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
    if ([...pos, ...rot, ...scl].some((v) => !Number.isFinite(v))) {
      return;
    }
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
    } catch (_) {
      // Directory already exists in Emscripten MEMFS.
    }

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
      els.statusMessage.textContent = "Downloaded " + stem + ".fe3d (inline schema 1.0)";
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

    document.querySelectorAll("[data-preset]").forEach((btn) => {
      btn.addEventListener("click", () => {
        if (!wasmApi) return;
        wasmApi.dispatchAction({
          action: "add_preset",
          preset: btn.dataset.preset,
        });
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

    // Pressing Enter inside any text/number input commits and blurs cleanly
    document.querySelectorAll('input[type="text"], input[type="number"]').forEach((input) => {
      input.addEventListener("keydown", (ev) => {
        if (ev.key === "Enter") {
          ev.preventDefault();
          input.blur();
        }
      });
    });

    // Metadata inputs
    [els.metaProjectName, els.metaAuthor, els.metaSceneName].forEach((input) => {
      input.addEventListener("change", dispatchMetadataFromInputs);
    });

    // Inspector object header controls
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

    els.btnResetTransform.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      wasmApi.dispatchAction({
        action: "reset_transform",
        id: currentState.inspector.id,
      });
    });

    // Transform XYZ inputs: commit clean undoable step on change
    [
      els.posX, els.posY, els.posZ,
      els.rotX, els.rotY, els.rotZ,
      els.sclX, els.sclY, els.sclZ,
    ].forEach((input) => {
      input.addEventListener("change", () => dispatchTransformFromInputs(false));
    });

    // Camera component controls
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

    // Directional light controls
    els.lightColor.addEventListener("input", () => dispatchLightFromInputs(true));
    els.lightColor.addEventListener("change", () => dispatchLightFromInputs(false));
    els.lightIntensity.addEventListener("input", () => dispatchLightFromInputs(true));
    els.lightIntensity.addEventListener("change", () => dispatchLightFromInputs(false));

    // Sync immediately after canvas mouseup or window keyup
    window.addEventListener("mouseup", () => setTimeout(() => syncFromEngine(false), 20));
    window.addEventListener("keyup", () => setTimeout(() => syncFromEngine(false), 20));

    // Drag-and-drop .fe3d / .fe3dbin onto viewport
    const stage = document.getElementById("viewport-stage");
    stage.addEventListener("dragover", (ev) => {
      ev.preventDefault();
    });
    stage.addEventListener("drop", (ev) => {
      ev.preventDefault();
      if (ev.dataTransfer && ev.dataTransfer.files) {
        importSelectedFiles(ev.dataTransfer.files);
      }
    });
  }

  bindUiEvents();

  // Poll until Emscripten runtime + WebGL2 engine initialize, then maintain live UI sync.
  const bootTimer = setInterval(() => {
    if (tryBindWasmApi()) {
      clearInterval(bootTimer);
      setInterval(() => syncFromEngine(false), 75);
    }
  }, 40);
})();
