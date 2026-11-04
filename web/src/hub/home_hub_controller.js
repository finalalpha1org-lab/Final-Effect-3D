// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 Desktop Home & Project Hub Controller

import {
  ConfirmDialogController,
  escapeHtml,
  HUB_ICONS,
} from "../ui/components.js";
import { CommandPaletteController } from "../shell/command_palette.js";
import { createProjectsStartView } from "./views/projects_start_view.js";
import { createTemplatesView } from "./views/templates_view.js";
import { createNewProjectView } from "./views/new_project_view.js";
import { createOpenProjectView } from "./views/open_project_view.js";
import { createRecoveryView } from "./views/recovery_view.js";
import { createPreferencesView } from "./views/preferences_view.js";
import { createAboutView } from "./views/about_view.js";

export const HUB_VIEWS = Object.freeze([
  { id: "projects", label: "Projects & Start", icon: "projects", shortcut: "Alt+1" },
  { id: "templates", label: "Templates", icon: "templates", shortcut: "Alt+2" },
  { id: "new_project", label: "New Project", icon: "plus", shortcut: "Alt+3" },
  { id: "open_project", label: "Open Project", icon: "folder", shortcut: "Alt+4" },
  { id: "recovery", label: "Recovery & Autosave", icon: "recovery", shortcut: "Alt+5" },
  { id: "preferences", label: "Preferences", icon: "settings", shortcut: "Alt+6" },
  { id: "about", label: "About & System Info", icon: "info", shortcut: "Alt+7" },
]);

export class HomeHubController {
  constructor({
    rootEl,
    fileInputEl,
    preferencesService,
    recentProjectsService,
    recoveryService,
    projectOperationsService,
    shortcutRegistry,
    getStudioBridge,
  }) {
    this.rootEl = rootEl;
    this.fileInputEl = fileInputEl;
    this.preferences = preferencesService;
    this.recents = recentProjectsService;
    this.recovery = recoveryService;
    this.projectOps = projectOperationsService;
    this.shortcuts = shortcutRegistry;
    this.getStudio = getStudioBridge;

    this.activeViewId = "projects";
    this.globalError = "";
    this.pendingOpenCallback = null;
    this.lastAutosaveRevision = null;
    this.autosaveTimerId = null;

    this._mountShell();

    const sharedCtx = {
      preferences: this.preferences,
      recents: this.recents,
      recovery: this.recovery,
      projectOps: this.projectOps,
      shortcuts: this.shortcuts,
      confirmDialog: this.confirmDialog,
      navigate: (viewId, params) => this.navigate(viewId, params),
      resumeStudio: () => this.resumeStudio(),
      triggerOpenFlow: (opts) => this.triggerOpenFlow(opts),
      quickCreateFromTemplate: (tmplId) => this.quickCreateFromTemplate(tmplId),
      reopenEntryWithGuard: (entry) => this.reopenEntryWithGuard(entry),
      confirmReplaceDirtySessionIfNeeded: () => this.confirmReplaceDirtySessionIfNeeded(),
      captureActiveSessionSnapshot: (reason) => this.captureActiveSessionSnapshot(reason),
      setGlobalError: (msg) => this.setGlobalError(msg),
    };

    this.views = {
      projects: createProjectsStartView(sharedCtx),
      templates: createTemplatesView(sharedCtx),
      new_project: createNewProjectView(sharedCtx),
      open_project: createOpenProjectView(sharedCtx),
      recovery: createRecoveryView(sharedCtx),
      preferences: createPreferencesView(sharedCtx),
      about: createAboutView(sharedCtx),
    };

    this._registerShortcuts();
    this._setupSubscriptions();
  }

  _mountShell() {
    if (!this.rootEl) return;
    this.rootEl.classList.add("fe-hub-shell");
    this.rootEl.innerHTML = `
      <header class="fe-hub-topbar">
        <div class="fe-hub-brand">
          <span class="fe-hub-logo" aria-hidden="true">${HUB_ICONS.logo}</span>
          <div class="fe-hub-brand-copy">
            <span class="fe-hub-brand-title">Final Effect 3D</span>
            <span class="fe-hub-brand-badge" id="home-engine-badge" data-testid="home-engine-badge">
              v0.5.0 &middot; WebAssembly + WebGL 2.0
            </span>
          </div>
        </div>

        <div class="fe-hub-topbar-center">
          <button
            type="button"
            class="fe-hub-cmd-trigger"
            data-testid="btn-open-command-palette"
            title="Open Command Palette (Ctrl+K or F3)"
          >
            <span style="display: inline-flex; align-items: center; gap: 7px;">
              ${HUB_ICONS.search}
              <span>Search Hub commands, templates &amp; views…</span>
            </span>
            <kbd class="fe-kbd">Ctrl+K</kbd>
          </button>
        </div>

        <div class="fe-hub-topbar-actions">
          <span class="fe-session-pill" data-testid="hub-session-pill" hidden></span>
          <button
            type="button"
            class="fe-btn fe-btn-primary"
            id="btn-home-resume-studio"
            data-testid="btn-home-resume-studio"
            hidden
          >
            Return to Active 3D Studio
          </button>
          <button
            type="button"
            class="fe-btn"
            id="btn-open-settings"
            data-testid="btn-open-settings"
          >
            ${HUB_ICONS.settings}
            <span>Preferences</span>
          </button>
        </div>
      </header>

      <div class="fe-hub-workspace">
        <aside class="fe-hub-rail" aria-label="Project Hub Navigation">
          <div class="fe-hub-rail-group" id="hub-rail-nav" data-testid="hub-rail-nav">
            <div class="fe-hub-rail-heading">Project Hub</div>
            <div id="hub-rail-items"></div>
          </div>

          <div class="fe-hub-rail-card">
            <span class="fe-hub-rail-card-title">Desktop Architecture</span>
            <span class="fe-hub-rail-card-sub">
              Shared C++20 <code>finaleffect::scene</code>, Cook-Torrance PBR &amp; <code>.fe3d</code> v1.3 serializer.
            </span>
          </div>
        </aside>

        <main class="fe-hub-stage">
          <div style="max-width: 1220px; margin: 0 auto 14px;" id="hub-global-banner-wrap" ${
            this.globalError ? "" : "hidden"
          }>
            <div class="fe-banner fe-banner-error" id="launcher-error-banner" data-testid="launcher-error-banner">
              <span id="hub-global-banner-text">${escapeHtml(this.globalError)}</span>
              <button type="button" class="fe-btn fe-btn-sm fe-btn-ghost" data-testid="btn-dismiss-hub-error">Dismiss</button>
            </div>
          </div>
          <div id="hub-view-mount" data-testid="hub-view-mount"></div>
        </main>
      </div>

      <footer class="fe-hub-footer" data-testid="hub-footer">
        <div class="fe-hub-footer-group">
          <span id="hub-footer-fs">File I/O: Initializing…</span>
          <span>&middot;</span>
          <span id="hub-footer-recovery">Recovery: Initializing…</span>
        </div>
        <div class="fe-hub-footer-group">
          <span id="hub-footer-nav">Nav: FE3D Hybrid</span>
          <span>&middot;</span>
          <span>GNU GPL v3.0-or-later</span>
        </div>
      </footer>

      <div class="fe-overlay-backdrop" id="hub-palette-backdrop" data-testid="command-palette-backdrop" hidden></div>
      <div class="fe-overlay-backdrop" id="hub-confirm-backdrop" data-testid="confirm-dialog-backdrop" hidden></div>
    `;

    this.railItemsEl = this.rootEl.querySelector("#hub-rail-items");
    this.viewMountEl = this.rootEl.querySelector("#hub-view-mount");
    this.resumeBtnEl = this.rootEl.querySelector("#btn-home-resume-studio");
    this.sessionPillEl = this.rootEl.querySelector('[data-testid="hub-session-pill"]');
    this.bannerWrapEl = this.rootEl.querySelector("#hub-global-banner-wrap");
    this.bannerTextEl = this.rootEl.querySelector("#hub-global-banner-text");

    this.confirmDialog = new ConfirmDialogController(
      this.rootEl.querySelector("#hub-confirm-backdrop")
    );
    this.palette = new CommandPaletteController({
      mountEl: this.rootEl.querySelector("#hub-palette-backdrop"),
      getCommands: () => this._buildPaletteCommands(),
    });

    const cmdBtn = this.rootEl.querySelector('[data-testid="btn-open-command-palette"]');
    if (cmdBtn) cmdBtn.addEventListener("click", () => this.palette.open());

    const prefsBtn = this.rootEl.querySelector("#btn-open-settings");
    if (prefsBtn) prefsBtn.addEventListener("click", () => this.navigate("preferences"));

    if (this.resumeBtnEl) {
      this.resumeBtnEl.addEventListener("click", () => this.resumeStudio());
    }

    const dismissBtn = this.rootEl.querySelector('[data-testid="btn-dismiss-hub-error"]');
    if (dismissBtn) {
      dismissBtn.addEventListener("click", () => this.setGlobalError(""));
    }
  }

  _registerShortcuts() {
    this.shortcuts.register("palette.open", () => {
      if (document.body.dataset.appView === "home") {
        this.palette.toggle();
      }
    });
    this.shortcuts.register("hub.new_project", () => this.navigate("new_project"));
    this.shortcuts.register("hub.open_project", () => this.navigate("open_project"));
    this.shortcuts.register("hub.preferences", () => this.navigate("preferences"));
    this.shortcuts.register("hub.view_projects", () => this.navigate("projects"));
    this.shortcuts.register("hub.view_templates", () => this.navigate("templates"));
    this.shortcuts.register("hub.view_new", () => this.navigate("new_project"));
    this.shortcuts.register("hub.view_open", () => this.navigate("open_project"));
    this.shortcuts.register("hub.view_recovery", () => this.navigate("recovery"));
    this.shortcuts.register("hub.view_preferences", () => this.navigate("preferences"));
    this.shortcuts.register("hub.view_about", () => this.navigate("about"));

    window.addEventListener("keydown", (ev) => {
      this.shortcuts.handleKeyDown(ev);
    });
  }

  _setupSubscriptions() {
    this.preferences.subscribe(() => {
      this.preferences.applyToDocument();
      this._renderChrome();
      this._restartAutosaveTimer();
    });

    this.recovery.subscribe(() => {
      this._renderChrome();
      if (this.activeViewId === "recovery" && !this.rootEl.hidden) {
        this.views.recovery.render(this.viewMountEl);
      }
    });

    this.recents.subscribe(() => {
      if (this.activeViewId === "projects" && !this.rootEl.hidden) {
        this.views.projects.render(this.viewMountEl);
      }
    });

    window.addEventListener("pagehide", () => {
      const session = this.projectOps.getActiveSessionSummary();
      if (!session.isDirty) {
        this.recovery.markCleanShutdown();
      }
    });
  }

  _restartAutosaveTimer() {
    if (this.autosaveTimerId !== null) {
      clearInterval(this.autosaveTimerId);
      this.autosaveTimerId = null;
    }
    const prefs = this.preferences.get();
    if (!prefs.autosaveEnabled) return;
    const intervalMs = Math.max(30, prefs.autosaveIntervalSeconds) * 1000;
    this.autosaveTimerId = setInterval(() => {
      void this._maybeRunPeriodicAutosave();
    }, intervalMs);
  }

  async _maybeRunPeriodicAutosave() {
    const prefs = this.preferences.get();
    if (!prefs.autosaveEnabled || this.recovery.storageState !== "available") return;
    const studio = this.getStudio();
    if (!studio || !this.projectOps.hasEnteredStudioSession) return;
    const st = studio.getState();
    if (!st?.project?.is_dirty) return;
    if (st.revision === this.lastAutosaveRevision) return;

    const res = await this.captureActiveSessionSnapshot("Periodic Autosave");
    if (res.ok) {
      this.lastAutosaveRevision = st.revision;
    }
  }

  async captureActiveSessionSnapshot(reason = "Periodic Autosave") {
    const studio = this.getStudio();
    if (!studio || typeof studio.exportInlineProject !== "function") {
      return { ok: false, error: "Studio engine is not ready." };
    }
    const st = studio.getState();
    const jsonText = studio.exportInlineProject();
    return this.recovery.captureSnapshot({
      projectName: st?.project?.project_name || "Untitled Project",
      author: st?.project?.author || this.preferences.get().defaultAuthor,
      originPath: st?.project?.file_path || "",
      jsonText,
      objectCount: st?.stats?.object_count ?? 0,
      reason,
    });
  }

  _buildPaletteCommands() {
    const cmds = HUB_VIEWS.map((v) => ({
      category: "Navigate",
      label: v.label,
      shortcut: v.shortcut,
      run: () => this.navigate(v.id),
    }));

    for (const tmpl of this.projectOps.getTemplates()) {
      cmds.push({
        category: "Template",
        label: `Create ${tmpl.title} Project`,
        shortcut: "",
        run: () => void this.quickCreateFromTemplate(tmpl.id),
      });
    }

    cmds.push({
      category: "File",
      label: "Browse & Open Local .fe3d Project…",
      shortcut: "Ctrl+Alt+O",
      run: () => void this.triggerOpenFlow(),
    });

    if (this.projectOps.hasEnteredStudioSession) {
      cmds.push({
        category: "Session",
        label: "Return to Active 3D Studio",
        shortcut: "",
        run: () => this.resumeStudio(),
      });
    }
    return cmds;
  }

  setGlobalError(msg) {
    this.globalError = String(msg || "");
    if (this.bannerWrapEl && this.bannerTextEl) {
      this.bannerWrapEl.hidden = !this.globalError;
      this.bannerTextEl.textContent = this.globalError;
    }
  }

  navigate(viewId, params = null) {
    const targetId = this.views[viewId] ? viewId : "projects";
    this.activeViewId = targetId;
    this.setGlobalError("");
    this._renderChrome();
    this.views[targetId].render(this.viewMountEl, params);
  }

  onShowHome() {
    this._renderChrome();
    this.views[this.activeViewId].render(this.viewMountEl);
  }

  resumeStudio() {
    const studio = this.getStudio();
    if (studio && typeof studio.setView === "function") {
      this.projectOps.hasEnteredStudioSession = true;
      studio.setView("studio");
    }
  }

  async confirmReplaceDirtySessionIfNeeded() {
    const session = this.projectOps.getActiveSessionSummary();
    if (!session.hasActiveSession || !session.isDirty) {
      return true;
    }
    return this.confirmDialog.confirm({
      title: "Discard Unsaved Studio Changes?",
      message: `Active project '${session.projectName}' has unsaved changes in 3D Studio. Opening or creating another project will replace the current scene.`,
      detail: session.filePath || "Unsaved workspace scene",
      confirmLabel: "Replace Active Scene",
      cancelLabel: "Keep Working",
      tone: "warning",
      testIdPrefix: "dirty-session-dialog",
    });
  }

  async quickCreateFromTemplate(templateId) {
    const allowed = await this.confirmReplaceDirtySessionIfNeeded();
    if (!allowed) return;
    const tmpl =
      this.projectOps.getTemplates().find((t) => t.id === templateId) ||
      this.projectOps.getTemplates()[0];
    const prefs = this.preferences.get();

    // Generate a unique non-conflicting default path for instant template launches.
    let index = 1;
    let candidateName = tmpl.title;
    while (
      this.recents.findByPath(
        `/workspace/projects/${candidateName.replace(/\s+/g, "_")}.fe3d`
      )
    ) {
      index += 1;
      candidateName = `${tmpl.title} ${index}`;
    }

    const res = await this.projectOps.createNewProject(
      {
        projectName: candidateName,
        templateId: tmpl.id,
        saveDirectory: "/workspace/projects",
        author: prefs.defaultAuthor,
        units: prefs.defaultUnits,
        fps: prefs.defaultFps,
      },
      { allowOverwrite: false }
    );
    if (!res.ok) {
      this.setGlobalError(res.error || "Failed to initialize template project.");
    }
  }

  async reopenEntryWithGuard(entry) {
    const allowed = await this.confirmReplaceDirtySessionIfNeeded();
    if (!allowed) return { ok: false, cancelled: true };
    const res = await this.projectOps.reopenRecentEntry(entry);
    if (!res.ok) {
      this.setGlobalError(res.error);
    }
    return res;
  }

  async triggerOpenFlow(opts = {}) {
    const allowed = await this.confirmReplaceDirtySessionIfNeeded();
    if (!allowed) return;

    const fsCaps = this.projectOps.detectFileSystemCapabilities();
    const forceInputFallback = Boolean(
      typeof window !== "undefined" && window.__FE3D_PREFER_FILE_INPUT_FALLBACK__
    );

    if (fsCaps.openPicker && !forceInputFallback) {
      const res = await this.projectOps.openViaNativePicker();
      if (!res.ok && !res.cancelled) {
        const msg = res.error || "Failed to open project file.";
        this.setGlobalError(msg);
        if (typeof opts.onError === "function") opts.onError(msg);
      }
      return;
    }

    if (this.fileInputEl) {
      this.pendingOpenCallback = opts.onError || null;
      this.fileInputEl.value = "";
      this.fileInputEl.click();
    }
  }

  async handleFileInputFiles(fileList) {
    const res = await this.projectOps.openFilesList(fileList);
    if (!res.ok && !res.cancelled) {
      const msg = res.error || "Failed to open selected project file.";
      this.setGlobalError(msg);
      if (this.activeViewId === "open_project") {
        this.views.open_project.setError(msg);
        this.views.open_project.render(this.viewMountEl);
      }
      if (typeof this.pendingOpenCallback === "function") {
        this.pendingOpenCallback(msg);
      }
    }
    this.pendingOpenCallback = null;
    return res;
  }

  _renderChrome() {
    if (!this.rootEl) return;
    const recStatus = this.recovery.getStatus();
    const session = this.projectOps.getActiveSessionSummary();
    const prefs = this.preferences.get();
    const fsCaps = this.projectOps.detectFileSystemCapabilities();

    if (this.railItemsEl) {
      this.railItemsEl.innerHTML = HUB_VIEWS.map((item) => {
        const isActive = item.id === this.activeViewId;
        let badgeHtml = "";
        if (item.id === "projects" && this.recents.count() > 0) {
          badgeHtml = `<span class="fe-hub-nav-badge">${this.recents.count()}</span>`;
        } else if (item.id === "recovery") {
          if (recStatus.storageState !== "available") {
            badgeHtml = `<span class="fe-hub-nav-badge is-amber">Off</span>`;
          } else if (recStatus.snapshots.length > 0) {
            badgeHtml = `<span class="fe-hub-nav-badge ${
              recStatus.hadUncleanPreviousSession ? "is-amber" : ""
            }">${recStatus.snapshots.length}</span>`;
          }
        }
        return `
          <button
            type="button"
            class="fe-hub-nav-btn ${isActive ? "is-active" : ""}"
            data-hub-view="${escapeHtml(item.id)}"
            data-testid="hub-nav-${escapeHtml(item.id)}"
            aria-current="${isActive ? "page" : "false"}"
          >
            <span class="fe-hub-nav-btn-left">
              <span class="fe-hub-nav-ic">${HUB_ICONS[item.icon] || ""}</span>
              <span>${escapeHtml(item.label)}</span>
            </span>
            ${badgeHtml}
          </button>
        `;
      }).join("");

      this.railItemsEl.querySelectorAll("[data-hub-view]").forEach((btn) => {
        btn.addEventListener("click", () => {
          this.navigate(btn.getAttribute("data-hub-view"));
        });
      });
    }

    if (this.resumeBtnEl) {
      this.resumeBtnEl.hidden = !session.hasActiveSession;
    }

    if (this.sessionPillEl) {
      if (session.hasActiveSession) {
        this.sessionPillEl.hidden = false;
        this.sessionPillEl.classList.toggle("is-dirty", session.isDirty);
        this.sessionPillEl.innerHTML = `
          <span class="fe-session-pill-dot"></span>
          <span>${escapeHtml(session.projectName)}${
          session.isDirty ? " • Unsaved" : " • Saved"
        }</span>
        `;
      } else {
        this.sessionPillEl.hidden = true;
      }
    }

    const fsEl = this.rootEl.querySelector("#hub-footer-fs");
    const recEl = this.rootEl.querySelector("#hub-footer-recovery");
    const navEl = this.rootEl.querySelector("#hub-footer-nav");
    if (fsEl) {
      fsEl.textContent = fsCaps.openPicker
        ? "File I/O: File System Access API"
        : "File I/O: Browser Picker + Download Fallback";
    }
    if (recEl) {
      recEl.textContent =
        recStatus.storageState === "available"
          ? `Recovery: IndexedDB Ready (${recStatus.snapshots.length}/${recStatus.maxRingSlots})`
          : "Recovery: Unavailable";
    }
    if (navEl) {
      const labels = {
        fe3d_hybrid: "FE3D Hybrid",
        blender: "Blender",
        maya: "Alt-Navigation",
      };
      navEl.textContent = `Nav: ${labels[prefs.navigationProfile] || prefs.navigationProfile}`;
    }

    this._restartAutosaveTimer();
  }
}
