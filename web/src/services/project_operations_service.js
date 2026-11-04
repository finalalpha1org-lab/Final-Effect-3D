// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 Project Operations & Capability Service
// Connects Home Hub views directly to real C++20 EditorController templates, serializer,
// File System Access API (when supported) + <input type="file"> / download fallbacks.

export const C_PLUS_PLUS_TEMPLATES = Object.freeze([
  {
    id: "starter",
    title: "Starter Showcase",
    category: "General",
    summary:
      "Complete hierarchical showcase rig with HeroCube, AccentPyramid, MainCamera, SunLight, and 3 calibrated PBR materials.",
    objectCount: 5,
    meshCount: 2,
    materialCount: 3,
    diagramKind: "showcase",
  },
  {
    id: "minimal_cube",
    title: "Minimal Cube",
    category: "Modeling",
    summary:
      "Single editable 8-vertex polygon Cube centered at world origin with Studio Clay PBR material, MainCamera, and SunLight.",
    objectCount: 3,
    meshCount: 1,
    materialCount: 1,
    diagramKind: "cube",
  },
  {
    id: "empty_stage",
    title: "Empty Modeling Stage",
    category: "Modeling",
    summary:
      "Clean right-handed +Y origin stage with calibrated MainCamera, directional SunLight, and procedural distance grid.",
    objectCount: 2,
    meshCount: 0,
    materialCount: 0,
    diagramKind: "stage",
  },
  {
    id: "shader_ball",
    title: "Shader Ball Studio",
    category: "LookDev & Shading",
    summary:
      "High-tessellation UV ShaderSphere configured with Brushed Gold, Chrome Mirror, and Ceramic Matte PBR materials.",
    objectCount: 3,
    meshCount: 1,
    materialCount: 3,
    diagramKind: "sphere",
  },
]);

export function sanitizeFileStem(name) {
  const cleaned = String(name || "")
    .trim()
    .replace(/[<>:"/\\|?*\x00-\x1F]+/g, "_")
    .replace(/\s+/g, "_");
  return cleaned || "Untitled_Project";
}

export function buildProjectSavePath(saveDirectory, projectName, explicitFileName = "") {
  const dir = String(saveDirectory || "/workspace/projects")
    .trim()
    .replace(/\\/g, "/")
    .replace(/\/+$/, "") || "/workspace/projects";
  const rawFile = String(explicitFileName || "").trim();
  let fileName = rawFile || `${sanitizeFileStem(projectName)}.fe3d`;
  if (!fileName.toLowerCase().endsWith(".fe3d")) {
    fileName += ".fe3d";
  }
  return `${dir}/${fileName}`;
}

export class ProjectOperationsService {
  constructor({ preferencesService, recentProjectsService, recoveryService, getStudioBridge }) {
    this.preferences = preferencesService;
    this.recents = recentProjectsService;
    this.recovery = recoveryService;
    this.getStudio = getStudioBridge || (() => (typeof window !== "undefined" ? window.__FE3D_STUDIO__ : null));
    this.activeFileHandle = null;
    this.hasEnteredStudioSession = false;
  }

  getTemplates() {
    return C_PLUS_PLUS_TEMPLATES;
  }

  getActiveSessionSummary() {
    const studio = this.getStudio();
    if (!studio || typeof studio.getState !== "function") {
      return {
        ready: false,
        hasActiveSession: false,
        projectName: "",
        filePath: "",
        isDirty: false,
        objectCount: 0,
      };
    }
    const st = studio.getState();
    const proj = st?.project || {};
    return {
      ready: true,
      hasActiveSession: Boolean(this.hasEnteredStudioSession),
      projectName: proj.project_name || "Untitled 3D Project",
      author: proj.author || "Final Effect 3D",
      filePath: proj.file_path || "",
      units: proj.units || "meters",
      isDirty: Boolean(proj.is_dirty),
      objectCount: st?.stats?.object_count ?? 0,
      materialCount: st?.stats?.material_count ?? 0,
    };
  }

  detectFileSystemCapabilities() {
    const win = typeof window !== "undefined" ? window : {};
    return {
      openPicker: typeof win.showOpenFilePicker === "function",
      savePicker: typeof win.showSaveFilePicker === "function",
      directoryPicker: typeof win.showDirectoryPicker === "function",
      secureContext: Boolean(win.isSecureContext),
    };
  }

  async validateNewProject({
    projectName,
    templateId = "starter",
    saveDirectory = "/workspace/projects",
    fileName = "",
    directoryHandle = null,
  } = {}) {
    const cleanName = String(projectName || "").trim();
    if (!cleanName) {
      return {
        ok: false,
        error: "Project name is required. Enter a descriptive title before creating the project.",
      };
    }
    if (cleanName.length > 120) {
      return {
        ok: false,
        error: "Project name must be 120 characters or fewer.",
      };
    }
    const tmpl = C_PLUS_PLUS_TEMPLATES.find((t) => t.id === templateId);
    if (!tmpl) {
      return {
        ok: false,
        error: `Unknown project template '${templateId}'.`,
      };
    }

    const normalizedPath = buildProjectSavePath(saveDirectory, cleanName, fileName);
    const leafName = normalizedPath.split("/").pop();

    if (/[<>:"|?*\x00-\x1F]/.test(leafName)) {
      return {
        ok: false,
        error: "Project file name contains unsupported filesystem characters.",
      };
    }

    let conflictExists = Boolean(this.recents.findByPath(normalizedPath));
    if (!conflictExists && directoryHandle && typeof directoryHandle.getFileHandle === "function") {
      try {
        await directoryHandle.getFileHandle(leafName, { create: false });
        conflictExists = true;
      } catch (_) {
        // NotFoundError means clean non-conflicting target.
      }
    }

    return {
      ok: true,
      cleanName,
      template: tmpl,
      normalizedPath,
      leafName,
      requiresOverwriteConfirmation: conflictExists,
    };
  }

  captureGenuineViewportThumbnail() {
    if (typeof document === "undefined") return null;
    try {
      const canvas = document.getElementById("fe3d-canvas");
      if (!canvas || canvas.width < 16 || canvas.height < 16) return null;
      const thumb = document.createElement("canvas");
      thumb.width = 176;
      thumb.height = 110;
      const ctx = thumb.getContext("2d");
      if (!ctx) return null;
      ctx.drawImage(canvas, 0, 0, thumb.width, thumb.height);
      return thumb.toDataURL("image/jpeg", 0.76);
    } catch (_) {
      return null;
    }
  }

  async createNewProject(params = {}, { allowOverwrite = false } = {}) {
    const validation = await this.validateNewProject(params);
    if (!validation.ok) {
      return validation;
    }
    if (validation.requiresOverwriteConfirmation && !allowOverwrite) {
      return {
        ok: false,
        requiresOverwriteConfirmation: true,
        conflictPath: validation.normalizedPath,
        error: `A project already exists at '${validation.normalizedPath}'. Confirm overwrite or choose another name/location.`,
      };
    }

    const studio = this.getStudio();
    if (!studio || typeof studio.dispatch !== "function") {
      return {
        ok: false,
        error: "C++20 WebAssembly Studio controller is still initializing. Wait for ready status and try again.",
      };
    }

    const prefs = this.preferences.get();
    const author = String(params.author ?? prefs.defaultAuthor).trim() || prefs.defaultAuthor;
    const description = String(params.description || "").trim();
    const units = String(params.units || prefs.defaultUnits || "meters");
    const fps = Number(params.fps || prefs.defaultFps || 24);

    const ok = studio.dispatch({
      action: "create_new_project",
      template: validation.template.id,
      project_name: validation.cleanName,
      author,
      description,
      units,
      fps,
      file_path: validation.normalizedPath,
    });

    if (!ok) {
      const st = studio.getState();
      return {
        ok: false,
        error: st?.status?.message || "Engine failed to initialize the requested template.",
      };
    }

    let fileHandle = params.fileHandle || null;
    if (!fileHandle && params.directoryHandle && typeof params.directoryHandle.getFileHandle === "function") {
      try {
        fileHandle = await params.directoryHandle.getFileHandle(validation.leafName, { create: true });
      } catch (err) {
        return {
          ok: false,
          error: `Could not create '${validation.leafName}' in selected folder: ${err?.message || String(err)}`,
        };
      }
    }

    const exportedJson =
      typeof studio.exportInlineProject === "function" ? studio.exportInlineProject() : "";

    if (fileHandle && typeof fileHandle.createWritable === "function" && exportedJson) {
      try {
        const writable = await fileHandle.createWritable();
        await writable.write(exportedJson);
        await writable.close();
      } catch (err) {
        return {
          ok: false,
          error: `Failed writing project file to disk: ${err?.message || String(err)}`,
        };
      }
    }

    this.activeFileHandle = fileHandle;
    this.hasEnteredStudioSession = true;
    if (typeof studio.activateWorkspaceById === "function") {
      studio.activateWorkspaceById("layout", false);
    }
    if (typeof studio.setView === "function") {
      studio.setView("studio");
    }

    const st = studio.getState();
    const entry = await this.recents.recordProject(
      {
        name: validation.cleanName,
        author,
        sceneName: st?.project?.scene_name || "Scene",
        filePath: validation.normalizedPath,
        byteSize: exportedJson ? exportedJson.length : 0,
        objectCount: st?.stats?.object_count ?? validation.template.objectCount,
        meshCount: validation.template.meshCount,
        materialCount: st?.stats?.material_count ?? validation.template.materialCount,
        fileHandle,
        storageKind: fileHandle ? "native_handle" : "browser_workspace",
        thumbnailDataUrl: null,
      },
      exportedJson,
      prefs.recentProjectsLimit
    );

    // Capture a genuine viewport thumbnail only after WebGL2 renders real frames in Studio.
    if (typeof requestAnimationFrame === "function") {
      requestAnimationFrame(() => {
        requestAnimationFrame(async () => {
          const thumb = this.captureGenuineViewportThumbnail();
          if (thumb) {
            await this.recents.recordProject(
              {
                ...entry,
                fileHandle,
                thumbnailDataUrl: thumb,
              },
              exportedJson,
              this.preferences.get().recentProjectsLimit
            );
          }
        });
      });
    }

    return { ok: true, entry };
  }

  async openViaNativePicker() {
    const caps = this.detectFileSystemCapabilities();
    if (!caps.openPicker) {
      return { ok: false, fallbackRequired: true };
    }
    try {
      const handles = await window.showOpenFilePicker({
        multiple: true,
        types: [
          {
            description: "Final Effect 3D Project (.fe3d, .fe3dbin)",
            accept: {
              "application/json": [".fe3d", ".json"],
              "application/octet-stream": [".fe3dbin"],
            },
          },
        ],
      });
      if (!handles || handles.length === 0) {
        return { ok: false, cancelled: true };
      }
      const files = [];
      let primaryHandle = null;
      for (const h of handles) {
        const f = await h.getFile();
        files.push(f);
        if (f.name.toLowerCase().endsWith(".fe3d") || f.name.toLowerCase().endsWith(".json")) {
          primaryHandle = h;
        }
      }
      return this.openFilesList(files, { fileHandle: primaryHandle });
    } catch (err) {
      if (err && err.name === "AbortError") {
        return { ok: false, cancelled: true };
      }
      return {
        ok: false,
        error: `Could not open file picker: ${err?.message || String(err)}`,
      };
    }
  }

  async openFilesList(fileList, options = {}) {
    const files = Array.from(fileList || []);
    if (files.length === 0) {
      return { ok: false, cancelled: true };
    }

    const studio = this.getStudio();
    if (!studio || !studio.wasm || !studio.wasm.fs) {
      return {
        ok: false,
        error: "C++20 WebAssembly Studio runtime is not ready to load files yet.",
      };
    }

    let primaryFile = null;
    for (const f of files) {
      const lower = f.name.toLowerCase();
      if (lower.endsWith(".fe3d") || lower.endsWith(".json")) {
        primaryFile = f;
        break;
      }
    }

    if (!primaryFile) {
      const unsupportedNames = files.map((f) => f.name).join(", ");
      return {
        ok: false,
        error: `Unsupported selection (${unsupportedNames}). Select a valid Final Effect 3D '.fe3d' project file (plus optional companion '.fe3dbin' archive).`,
      };
    }

    try {
      studio.wasm.fs.mkdir("/fe3d_import");
    } catch (_) {}

    let primaryText = "";
    try {
      for (const f of files) {
        const bytes = new Uint8Array(await f.arrayBuffer());
        studio.wasm.fs.writeFile(`/fe3d_import/${f.name}`, bytes);
        if (f === primaryFile) {
          primaryText = new TextDecoder("utf-8", { fatal: false }).decode(bytes);
        }
      }
    } catch (err) {
      return {
        ok: false,
        error: `Failed reading selected file '${primaryFile.name}': ${err?.message || String(err)}`,
      };
    }

    const targetVfsPath = `/fe3d_import/${primaryFile.name}`;
    const loadedOk = studio.wasm.loadFileFromVfs(targetVfsPath);
    if (!loadedOk) {
      const st = studio.getState();
      const engineDetail = st?.status?.message || "Invalid or corrupted .fe3d schema.";
      return {
        ok: false,
        error: `Failed to open '${primaryFile.name}': ${engineDetail}`,
      };
    }

    const displayPath = options.displayPath || `/local/${primaryFile.name}`;
    studio.dispatch({ action: "mark_document_saved", file_path: displayPath });

    this.activeFileHandle = options.fileHandle || null;
    this.hasEnteredStudioSession = true;
    if (typeof studio.setView === "function") {
      studio.setView("studio");
    }

    const st = studio.getState();
    const inlineJson =
      typeof studio.exportInlineProject === "function" ? studio.exportInlineProject() : primaryText;

    const entry = await this.recents.recordProject(
      {
        name: st?.project?.project_name || primaryFile.name.replace(/\.fe3d$/i, ""),
        author: st?.project?.author || this.preferences.get().defaultAuthor,
        sceneName: st?.project?.scene_name || "Scene",
        filePath: displayPath,
        byteSize: primaryFile.size || inlineJson.length,
        objectCount: st?.stats?.object_count ?? 0,
        materialCount: st?.stats?.material_count ?? 0,
        fileHandle: this.activeFileHandle,
        storageKind: this.activeFileHandle ? "native_handle" : "imported_file",
      },
      inlineJson,
      this.preferences.get().recentProjectsLimit
    );

    if (typeof requestAnimationFrame === "function") {
      requestAnimationFrame(() => {
        requestAnimationFrame(async () => {
          const thumb = this.captureGenuineViewportThumbnail();
          if (thumb) {
            await this.recents.recordProject(
              { ...entry, fileHandle: this.activeFileHandle, thumbnailDataUrl: thumb },
              inlineJson,
              this.preferences.get().recentProjectsLimit
            );
          }
        });
      });
    }

    return { ok: true, entry };
  }

  async reopenRecentEntry(entry) {
    if (!entry) {
      return { ok: false, error: "Invalid recent project entry." };
    }
    const studio = this.getStudio();
    if (!studio || !studio.wasm) {
      return { ok: false, error: "C++20 WebAssembly Studio runtime is not ready." };
    }

    let jsonText = null;
    const handle = this.recents.getFileHandle(entry.id);
    if (handle && typeof handle.getFile === "function") {
      try {
        const f = await handle.getFile();
        jsonText = await f.text();
      } catch (_) {
        jsonText = null;
      }
    }

    if (!jsonText) {
      jsonText = await this.recents.loadPayload(entry.id);
    }

    if (!jsonText) {
      this.recents.markAvailability(entry.id, false);
      return {
        ok: false,
        error: `Project data for '${entry.name}' (${entry.filePath}) is no longer cached in browser storage. Use Open Project to locate the '.fe3d' file on disk or remove this entry.`,
      };
    }

    const ok = studio.dispatch({
      action: "load_inline_json",
      json_text: jsonText,
      file_path: entry.filePath,
    });

    if (!ok) {
      const st = studio.getState();
      return {
        ok: false,
        error: `Failed to reopen '${entry.name}': ${st?.status?.message || "Corrupted project data."}`,
      };
    }

    this.activeFileHandle = handle || null;
    this.hasEnteredStudioSession = true;
    if (typeof studio.setView === "function") {
      studio.setView("studio");
    }

    const st = studio.getState();
    await this.recents.recordProject(
      {
        ...entry,
        objectCount: st?.stats?.object_count ?? entry.objectCount,
        materialCount: st?.stats?.material_count ?? entry.materialCount,
        fileHandle: this.activeFileHandle,
      },
      jsonText,
      this.preferences.get().recentProjectsLimit
    );

    return { ok: true, entry };
  }

  async recoverAutosaveSnapshot(snapshotId) {
    const record = await this.recovery.getSnapshotPayload(snapshotId);
    if (!record || !record.jsonText) {
      return {
        ok: false,
        error: "Selected recovery snapshot is no longer available in storage.",
      };
    }
    const studio = this.getStudio();
    if (!studio || typeof studio.dispatch !== "function") {
      return {
        ok: false,
        error: "C++20 WebAssembly Studio runtime is not ready.",
      };
    }

    const recoveredPath = record.originPath || `/recovered/${sanitizeFileStem(record.projectName)}.fe3d`;
    const ok = studio.dispatch({
      action: "load_inline_json",
      json_text: record.jsonText,
      file_path: recoveredPath,
    });
    if (!ok) {
      const st = studio.getState();
      return {
        ok: false,
        error: `Recovery snapshot could not be deserialized: ${st?.status?.message || "Corrupted snapshot"}`,
      };
    }

    this.hasEnteredStudioSession = true;
    if (typeof studio.setView === "function") {
      studio.setView("studio");
    }

    const st = studio.getState();
    await this.recents.recordProject(
      {
        name: st?.project?.project_name || record.projectName,
        author: st?.project?.author || record.author,
        sceneName: st?.project?.scene_name || "Scene",
        filePath: recoveredPath,
        byteSize: record.jsonText.length,
        objectCount: st?.stats?.object_count ?? record.objectCount,
        materialCount: st?.stats?.material_count ?? 0,
        storageKind: "recovered_snapshot",
      },
      record.jsonText,
      this.preferences.get().recentProjectsLimit
    );

    return { ok: true, record };
  }

  async snapshotActiveSessionToRecent() {
    const studio = this.getStudio();
    if (!studio || !this.hasEnteredStudioSession) return null;
    const st = studio.getState();
    const inlineJson =
      typeof studio.exportInlineProject === "function" ? studio.exportInlineProject() : "";
    if (!inlineJson || !st?.project) return null;

    const thumb = this.captureGenuineViewportThumbnail();
    const filePath =
      st.project.file_path ||
      buildProjectSavePath("/workspace/projects", st.project.project_name || "Untitled Project");

    return this.recents.recordProject(
      {
        name: st.project.project_name || "Untitled Project",
        author: st.project.author || this.preferences.get().defaultAuthor,
        sceneName: st.project.scene_name || "Scene",
        filePath,
        byteSize: inlineJson.length,
        objectCount: st?.stats?.object_count ?? 0,
        materialCount: st?.stats?.material_count ?? 0,
        fileHandle: this.activeFileHandle,
        thumbnailDataUrl: thumb,
      },
      inlineJson,
      this.preferences.get().recentProjectsLimit
    );
  }

  querySystemCapabilities() {
    const fsCaps = this.detectFileSystemCapabilities();
    const recStatus = this.recovery.getStatus();
    let cppInfo = {};

    try {
      if (
        typeof window !== "undefined" &&
        window.Module &&
        typeof window.Module._fe3d_get_system_info_json === "function" &&
        typeof window.Module.UTF8ToString === "function"
      ) {
        const ptr = window.Module._fe3d_get_system_info_json();
        if (ptr) {
          cppInfo = JSON.parse(window.Module.UTF8ToString(ptr));
        }
      }
    } catch (_) {}

    let glVendor = "Unavailable";
    let glRenderer = cppInfo.graphics_device || "WebGL 2.0 Context";
    let glVersion = "WebGL 2.0 (OpenGL ES 3.0)";
    let glShadingLang = "WebGL GLSL ES 3.00";
    let maxTextureSize = "N/A";
    let maxSamples = "N/A";

    if (typeof document !== "undefined") {
      try {
        const canvas = document.getElementById("fe3d-canvas");
        const gl = canvas ? canvas.getContext("webgl2") : null;
        if (gl) {
          glVersion = gl.getParameter(gl.VERSION) || glVersion;
          glShadingLang = gl.getParameter(gl.SHADING_LANGUAGE_VERSION) || glShadingLang;
          maxTextureSize = `${gl.getParameter(gl.MAX_TEXTURE_SIZE)} px`;
          maxSamples = `${gl.getParameter(gl.MAX_SAMPLES)}x MSAA`;
          const dbg = gl.getExtension("WEBGL_debug_renderer_info");
          if (dbg) {
            glVendor = gl.getParameter(dbg.UNMASKED_VENDOR_WEBGL) || glVendor;
            glRenderer = gl.getParameter(dbg.UNMASKED_RENDERER_WEBGL) || glRenderer;
          } else {
            glVendor = gl.getParameter(gl.VENDOR) || glVendor;
            glRenderer = gl.getParameter(gl.RENDERER) || glRenderer;
          }
        }
      } catch (_) {}
    }

    const nav = typeof navigator !== "undefined" ? navigator : {};
    return {
      applicationName: cppInfo.application_name || "Final Effect 3D",
      engineVersion: cppInfo.engine_version || "0.5.0",
      hostTarget: "Desktop Web (Emscripten WebAssembly + WebGL 2.0)",
      nativeTarget: "Desktop Native (C++20 + OpenGL 3.3 Core)",
      browserPlatform: nav.platform || nav.userAgentData?.platform || cppInfo.platform_name || "Desktop",
      hardwareConcurrency: nav.hardwareConcurrency ? `${nav.hardwareConcurrency} logical cores` : "Not reported",
      graphicsBackend: cppInfo.graphics_backend || "WebGL 2.0",
      glVersion,
      glShadingLang,
      glVendor,
      glRenderer,
      maxTextureSize,
      maxSamples,
      coordinateSystem: cppInfo.coordinate_system || "right-handed-y-up",
      projectFormat: cppInfo.project_format || ".fe3d JSON (v1.0-v1.3) + .fe3dbin Binary Archive (v1.0)",
      license: cppInfo.license || "GNU GPL v3.0-or-later",
      fileSystemAccess: fsCaps.openPicker && fsCaps.savePicker
        ? "Supported (Native File System Access API)"
        : "Browser File Picker + Download Fallback (.fe3d / .fe3dbin)",
      directoryPickerSupport: fsCaps.directoryPicker
        ? "Supported (showDirectoryPicker)"
        : "Virtual Workspace Path + Browser Download",
      recoveryStorage:
        recStatus.storageState === "available"
          ? `Available (IndexedDB ${recStatus.maxRingSlots}-slot ring buffer)`
          : `Unavailable (${recStatus.unavailableReason || "Storage restricted"})`,
    };
  }
}
