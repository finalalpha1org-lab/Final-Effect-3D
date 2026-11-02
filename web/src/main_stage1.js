// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 Modular Bootstrap Entrypoint
// Initializes Application Preferences, Recent Projects, Recovery & Autosave,
// Shortcut Registry, and the 7-View Desktop Home & Project Hub.

import { PreferencesService } from "./services/preferences_service.js";
import { RecentProjectsService } from "./services/recent_projects_service.js";
import { RecoveryService } from "./services/recovery_service.js";
import { ProjectOperationsService } from "./services/project_operations_service.js";
import { ShortcutRegistry } from "./shell/shortcut_registry.js";
import { HomeHubController } from "./hub/home_hub_controller.js";

async function bootstrapStage1Hub() {
  const rootEl = document.getElementById("app-home");
  const fileInputEl = document.getElementById("file-open-input");
  if (!rootEl) return null;

  const preferencesService = new PreferencesService();
  preferencesService.applyToDocument(document);

  const recentProjectsService = new RecentProjectsService();
  recentProjectsService.enforceLimit(preferencesService.get().recentProjectsLimit);

  const recoveryService = new RecoveryService();
  await recoveryService.initialize();

  const getStudioBridge = () => window.__FE3D_STUDIO__ || null;

  const projectOperationsService = new ProjectOperationsService({
    preferencesService,
    recentProjectsService,
    recoveryService,
    getStudioBridge,
  });

  const shortcutRegistry = new ShortcutRegistry({
    preferencesService,
    getAppView: () => document.body?.dataset?.appView || "home",
  });

  const controller = new HomeHubController({
    rootEl,
    fileInputEl,
    preferencesService,
    recentProjectsService,
    recoveryService,
    projectOperationsService,
    shortcutRegistry,
    getStudioBridge,
  });

  const params = new URLSearchParams(window.location.search || "");
  const initialHubView = params.get("hub");
  controller.navigate(initialHubView || "projects");

  const api = {
    controller,
    preferences: preferencesService,
    recents: recentProjectsService,
    recovery: recoveryService,
    projectOps: projectOperationsService,
    shortcuts: shortcutRegistry,
    onStudioBootReady() {
      const studio = getStudioBridge();
      if (!studio) return;
      const urlView = new URLSearchParams(window.location.search || "").get("view");
      if (urlView === "studio") {
        projectOperationsService.hasEnteredStudioSession = true;
        studio.setView("studio");
        return;
      }
      if (urlView !== "home" && preferencesService.get().startupEnvironment === "default_scene") {
        projectOperationsService.hasEnteredStudioSession = true;
        studio.setView("studio");
        return;
      }
      controller.onShowHome();
    },
    onShowHome() {
      controller.onShowHome();
    },
    async onLeaveStudioForHome() {
      projectOperationsService.hasEnteredStudioSession = true;
      await projectOperationsService.snapshotActiveSessionToRecent();
    },
    async onStudioSaved(filePath = "") {
      const studio = getStudioBridge();
      if (studio) {
        studio.dispatch({ action: "mark_document_saved", file_path: filePath });
      }
      projectOperationsService.hasEnteredStudioSession = true;
      await projectOperationsService.snapshotActiveSessionToRecent();
    },
    async handleFileInputFiles(files) {
      return controller.handleFileInputFiles(files);
    },
  };

  window.__FE3D_STAGE1_HUB__ = api;

  // If Wasm finished booting before the ES module resolved, synchronize immediately.
  if (window.__FE3D_STUDIO__) {
    api.onStudioBootReady();
  }

  return api;
}

if (typeof document !== "undefined") {
  void bootstrapStage1Hub();
}
