// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 View 3: New Project (Validated Name, Save Path & Overwrite Guard)

import { buildProjectSavePath } from "../../services/project_operations_service.js";
import { escapeHtml, HUB_ICONS } from "../../ui/components.js";

export function createNewProjectView(ctx) {
  const prefs = ctx.preferences.get();
  const form = {
    projectName: "Untitled Studio Project",
    templateId: "starter",
    saveDirectory: "/workspace/projects",
    fileName: "",
    author: prefs.defaultAuthor || "Final Effect 3D",
    units: prefs.defaultUnits || "meters",
    fps: prefs.defaultFps || 24,
    description: "",
    directoryHandle: null,
    errorMessage: "",
  };

  function applyNavigationParams(params = {}) {
    if (params.templateId) {
      form.templateId = params.templateId;
    }
    const latestPrefs = ctx.preferences.get();
    if (!form.author) {
      form.author = latestPrefs.defaultAuthor;
    }
  }

  function render(mountEl, navParams = null) {
    if (navParams) {
      applyNavigationParams(navParams);
    }

    const templates = ctx.projectOps.getTemplates();
    const fsCaps = ctx.projectOps.detectFileSystemCapabilities();
    const resolvedPath = buildProjectSavePath(form.saveDirectory, form.projectName, form.fileName);
    const existingConflict = Boolean(ctx.recents.findByPath(resolvedPath));

    mountEl.innerHTML = `
      <div class="fe-hub-container" data-testid="hub-view-new-project">
        <div class="fe-view-header">
          <div>
            <div class="fe-view-eyebrow">NEW .FE3D DOCUMENT</div>
            <h1 class="fe-view-title">Create New Project</h1>
            <p class="fe-view-subtitle">
              Configure project identity, target save location, template preset, and scene defaults before initializing the C++20 document.
            </p>
          </div>
          <div class="fe-view-actions">
            <button type="button" class="fe-btn" data-testid="btn-new-cancel">Back to Projects</button>
            <button type="button" class="fe-btn fe-btn-primary" data-testid="btn-new-project-submit">
              ${HUB_ICONS.plus}
              <span>Create Project &amp; Open Studio</span>
            </button>
          </div>
        </div>

        <div
          class="fe-banner fe-banner-error"
          data-testid="new-project-error"
          ${form.errorMessage ? "" : "hidden"}
        >
          <span>${escapeHtml(form.errorMessage)}</span>
        </div>

        <div
          class="fe-banner fe-banner-warning"
          data-testid="new-project-conflict-notice"
          ${existingConflict ? "" : "hidden"}
        >
          <span>
            A project is already recorded at <code>${escapeHtml(resolvedPath)}</code>. Submitting will ask for explicit overwrite confirmation before replacing it.
          </span>
        </div>

        <section class="fe-panel">
          <div class="fe-panel-head">
            <h2 class="fe-panel-title">Project Identity &amp; Save Location</h2>
            <span class="fe-tag">
              ${
                fsCaps.directoryPicker
                  ? "Native Folder Picker Available"
                  : "Workspace Path + Download Portable .fe3d"
              }
            </span>
          </div>

          <div class="fe-form-grid">
            <label class="fe-field">
              <span class="fe-field-label">Project Name *</span>
              <input
                type="text"
                class="fe-input"
                data-testid="new-project-name"
                value="${escapeHtml(form.projectName)}"
                placeholder="e.g. Hero Prop LookDev"
                autocomplete="off"
              />
              <span class="fe-field-hint">Stored in <code>metadata.project_name</code> inside the <code>.fe3d</code> file.</span>
            </label>

            <label class="fe-field">
              <span class="fe-field-label">Starting C++ Template *</span>
              <select class="fe-select" data-testid="new-project-template">
                ${templates
                  .map(
                    (t) =>
                      `<option value="${escapeHtml(t.id)}" ${
                        t.id === form.templateId ? "selected" : ""
                      }>${escapeHtml(t.title)} (${t.objectCount} obj, ${t.materialCount} mat)</option>`
                  )
                  .join("")}
              </select>
              <span class="fe-field-hint">Selects the initial scene hierarchy, camera, lights, and PBR materials.</span>
            </label>

            <div class="fe-field">
              <span class="fe-field-label">Save Directory *</span>
              <div style="display: flex; gap: 8px;">
                <input
                  type="text"
                  class="fe-input"
                  style="flex: 1;"
                  data-testid="new-project-directory"
                  value="${escapeHtml(form.saveDirectory)}"
                  autocomplete="off"
                />
                ${
                  fsCaps.directoryPicker
                    ? `<button type="button" class="fe-btn" data-testid="btn-pick-save-directory">Browse Folder…</button>`
                    : ""
                }
              </div>
              <span class="fe-field-hint">
                Target path: <code data-testid="new-project-resolved-path">${escapeHtml(resolvedPath)}</code>
              </span>
            </div>

            <label class="fe-field">
              <span class="fe-field-label">Optional Explicit File Name (.fe3d)</span>
              <input
                type="text"
                class="fe-input"
                data-testid="new-project-filename"
                value="${escapeHtml(form.fileName)}"
                placeholder="Auto-derived from Project Name when blank"
                autocomplete="off"
              />
              <span class="fe-field-hint">Automatically appends <code>.fe3d</code> if omitted.</span>
            </label>
          </div>
        </section>

        <section class="fe-panel">
          <div class="fe-panel-head">
            <h2 class="fe-panel-title">Scene Metadata &amp; Unit Defaults</h2>
            <span class="fe-tag">Right-Handed +Y</span>
          </div>

          <div class="fe-form-grid">
            <label class="fe-field">
              <span class="fe-field-label">Author</span>
              <input
                type="text"
                class="fe-input"
                data-testid="new-project-author"
                value="${escapeHtml(form.author)}"
                autocomplete="off"
              />
            </label>

            <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 12px;">
              <label class="fe-field">
                <span class="fe-field-label">Linear Units</span>
                <select class="fe-select" data-testid="new-project-units">
                  <option value="meters" ${form.units === "meters" ? "selected" : ""}>Meters (m)</option>
                  <option value="centimeters" ${form.units === "centimeters" ? "selected" : ""}>Centimeters (cm)</option>
                  <option value="millimeters" ${form.units === "millimeters" ? "selected" : ""}>Millimeters (mm)</option>
                </select>
              </label>

              <label class="fe-field">
                <span class="fe-field-label">Timeline Frame Rate</span>
                <select class="fe-select" data-testid="new-project-fps">
                  <option value="24" ${Number(form.fps) === 24 ? "selected" : ""}>24 fps (Film)</option>
                  <option value="30" ${Number(form.fps) === 30 ? "selected" : ""}>30 fps (Broadcast)</option>
                  <option value="60" ${Number(form.fps) === 60 ? "selected" : ""}>60 fps (Realtime)</option>
                </select>
              </label>
            </div>

            <label class="fe-field" style="grid-column: 1 / -1;">
              <span class="fe-field-label">Project Notes / Description (Optional)</span>
              <textarea
                class="fe-textarea"
                data-testid="new-project-description"
                placeholder="Optional production notes stored inside metadata.description…"
              >${escapeHtml(form.description)}</textarea>
            </label>
          </div>
        </section>
      </div>
    `;

    bindFormEvents(mountEl);
  }

  function updateLivePathPreview(mountEl) {
    const resolved = buildProjectSavePath(form.saveDirectory, form.projectName, form.fileName);
    const pathCode = mountEl.querySelector('[data-testid="new-project-resolved-path"]');
    if (pathCode) pathCode.textContent = resolved;
    const conflictBanner = mountEl.querySelector('[data-testid="new-project-conflict-notice"]');
    if (conflictBanner) {
      const hasConflict = Boolean(ctx.recents.findByPath(resolved));
      conflictBanner.hidden = !hasConflict;
      if (hasConflict) {
        conflictBanner.innerHTML = `<span>A project is already recorded at <code>${escapeHtml(
          resolved
        )}</code>. Submitting will ask for explicit overwrite confirmation before replacing it.</span>`;
      }
    }
  }

  function bindFormEvents(mountEl) {
    const nameInput = mountEl.querySelector('[data-testid="new-project-name"]');
    const tmplSelect = mountEl.querySelector('[data-testid="new-project-template"]');
    const dirInput = mountEl.querySelector('[data-testid="new-project-directory"]');
    const fileInput = mountEl.querySelector('[data-testid="new-project-filename"]');
    const authorInput = mountEl.querySelector('[data-testid="new-project-author"]');
    const unitsSelect = mountEl.querySelector('[data-testid="new-project-units"]');
    const fpsSelect = mountEl.querySelector('[data-testid="new-project-fps"]');
    const descInput = mountEl.querySelector('[data-testid="new-project-description"]');

    if (nameInput) {
      nameInput.addEventListener("input", () => {
        form.projectName = nameInput.value;
        form.errorMessage = "";
        const errBanner = mountEl.querySelector('[data-testid="new-project-error"]');
        if (errBanner) errBanner.hidden = true;
        updateLivePathPreview(mountEl);
      });
    }
    if (tmplSelect) {
      tmplSelect.addEventListener("change", () => {
        form.templateId = tmplSelect.value;
      });
    }
    if (dirInput) {
      dirInput.addEventListener("input", () => {
        form.saveDirectory = dirInput.value;
        form.directoryHandle = null;
        updateLivePathPreview(mountEl);
      });
    }
    if (fileInput) {
      fileInput.addEventListener("input", () => {
        form.fileName = fileInput.value;
        updateLivePathPreview(mountEl);
      });
    }
    if (authorInput) {
      authorInput.addEventListener("input", () => {
        form.author = authorInput.value;
      });
    }
    if (unitsSelect) {
      unitsSelect.addEventListener("change", () => {
        form.units = unitsSelect.value;
      });
    }
    if (fpsSelect) {
      fpsSelect.addEventListener("change", () => {
        form.fps = Number(fpsSelect.value) || 24;
      });
    }
    if (descInput) {
      descInput.addEventListener("input", () => {
        form.description = descInput.value;
      });
    }

    const pickDirBtn = mountEl.querySelector('[data-testid="btn-pick-save-directory"]');
    if (pickDirBtn) {
      pickDirBtn.addEventListener("click", async () => {
        try {
          const handle = await window.showDirectoryPicker({ mode: "readwrite" });
          if (handle) {
            form.directoryHandle = handle;
            form.saveDirectory = `/${handle.name}`;
            render(mountEl);
          }
        } catch (err) {
          if (!err || err.name !== "AbortError") {
            form.errorMessage = `Folder selection failed: ${err?.message || String(err)}`;
            render(mountEl);
          }
        }
      });
    }

    const cancelBtn = mountEl.querySelector('[data-testid="btn-new-cancel"]');
    if (cancelBtn) {
      cancelBtn.addEventListener("click", () => ctx.navigate("projects"));
    }

    const submitBtn = mountEl.querySelector('[data-testid="btn-new-project-submit"]');
    if (submitBtn) {
      submitBtn.addEventListener("click", async () => {
        form.errorMessage = "";
        const canProceedSession = await ctx.confirmReplaceDirtySessionIfNeeded();
        if (!canProceedSession) return;

        let res = await ctx.projectOps.createNewProject(form, { allowOverwrite: false });
        if (!res.ok && res.requiresOverwriteConfirmation) {
          const confirmed = await ctx.confirmDialog.confirm({
            title: "Overwrite Existing Project?",
            message:
              "A project already exists at the selected save path. Do you want to overwrite it with this new project?",
            detail: res.conflictPath,
            confirmLabel: "Overwrite Project",
            cancelLabel: "Keep Existing",
            tone: "warning",
            testIdPrefix: "overwrite-confirm-dialog",
          });
          if (!confirmed) {
            return;
          }
          res = await ctx.projectOps.createNewProject(form, { allowOverwrite: true });
        }

        if (!res.ok) {
          form.errorMessage = res.error || "Failed to create project.";
          render(mountEl);
        }
      });
    }
  }

  return { render };
}
