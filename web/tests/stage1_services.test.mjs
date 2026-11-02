// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 Modular Services & Shortcut Registry Unit Tests (Node --test)

import test from "node:test";
import assert from "node:assert/strict";

import {
  PreferencesService,
  DEFAULT_PREFERENCES,
} from "../src/services/preferences_service.js";
import { RecentProjectsService } from "../src/services/recent_projects_service.js";
import {
  RecoveryService,
  MAX_RECOVERY_RING_SLOTS,
} from "../src/services/recovery_service.js";
import {
  ProjectOperationsService,
  C_PLUS_PLUS_TEMPLATES,
  buildProjectSavePath,
} from "../src/services/project_operations_service.js";
import {
  ShortcutRegistry,
  isEditableInputTarget,
} from "../src/shell/shortcut_registry.js";

class MemoryStorage {
  constructor() {
    this.map = new Map();
  }
  getItem(key) {
    return this.map.has(key) ? this.map.get(key) : null;
  }
  setItem(key, val) {
    this.map.set(key, String(val));
  }
  removeItem(key) {
    this.map.delete(key);
  }
}

test("PreferencesService persists independently across restarts and clamps invalid values", () => {
  const storage = new MemoryStorage();
  const prefs1 = new PreferencesService({ storage, legacyRegistry: null });

  assert.equal(prefs1.get().uiScalePercent, DEFAULT_PREFERENCES.uiScalePercent);
  assert.equal(prefs1.get().navigationProfile, "fe3d_hybrid");

  prefs1.update({
    uiScalePercent: 118, // snaps to 115
    startupEnvironment: "default_scene",
    defaultAuthor: "  Pipeline TD  ",
    navigationProfile: "blender",
    recentProjectsLimit: 999, // clamps to 50
    autosaveEnabled: false,
    autosaveIntervalSeconds: 5, // clamps to 30
  });
  prefs1.setKeymapOverride("palette.open", "Ctrl+Shift+P");

  // Simulate full browser restart with fresh service instance on same storage
  const prefs2 = new PreferencesService({ storage, legacyRegistry: null });
  const snap = prefs2.get();
  assert.equal(snap.uiScalePercent, 115);
  assert.equal(snap.startupEnvironment, "default_scene");
  assert.equal(snap.defaultAuthor, "Pipeline TD");
  assert.equal(snap.navigationProfile, "blender");
  assert.equal(snap.recentProjectsLimit, 50);
  assert.equal(snap.autosaveEnabled, false);
  assert.equal(snap.autosaveIntervalSeconds, 30);
  assert.equal(snap.keymapOverrides["palette.open"], "Ctrl+Shift+P");

  prefs2.resetToDefaults();
  assert.equal(prefs2.get().uiScalePercent, 100);
  assert.deepEqual(prefs2.get().keymapOverrides, {});
});

test("RecentProjectsService enforces thumbnail honesty, search, sort, pin, and limits", async () => {
  const storage = new MemoryStorage();
  let clock = 1000;
  const recents = new RecentProjectsService({
    storage,
    idbFactory: null,
    nowFn: () => (clock += 100),
  });

  assert.equal(recents.count(), 0);
  assert.deepEqual(recents.query(), []);

  // Fake non-data thumbnail strings are rejected; only genuine base64 data URLs are kept.
  await recents.recordProject(
    {
      name: "Zeta Mech",
      author: "ArtistA",
      filePath: "/workspace/projects/Zeta_Mech.fe3d",
      objectCount: 5,
      materialCount: 3,
      thumbnailDataUrl: "https://example.com/fake-placeholder.png",
    },
    '{"format":"finaleffect.fe3d"}'
  );

  const genuineThumb = "data:image/jpeg;base64," + "A".repeat(80);
  const alphaEntry = await recents.recordProject(
    {
      name: "Alpha Stage",
      author: "ArtistB",
      filePath: "/workspace/projects/Alpha_Stage.fe3d",
      objectCount: 2,
      materialCount: 0,
      thumbnailDataUrl: genuineThumb,
    },
    '{"format":"finaleffect.fe3d","scene":{}}'
  );

  const all = recents.query();
  assert.equal(all.length, 2);
  assert.equal(all[0].name, "Alpha Stage");
  assert.equal(all[0].thumbnailDataUrl, genuineThumb);
  assert.equal(all[1].name, "Zeta Mech");
  assert.equal(all[1].thumbnailDataUrl, null);

  // Pinning Zeta Mech promotes it above newer unpinned entries
  recents.togglePin(all[1].id);
  const pinnedOrdered = recents.query({ sort: "recent" });
  assert.equal(pinnedOrdered[0].name, "Zeta Mech");
  assert.equal(pinnedOrdered[0].pinned, true);

  // Search by path or author
  const searchRes = recents.query({ search: "artistb" });
  assert.equal(searchRes.length, 1);
  assert.equal(searchRes[0].id, alphaEntry.id);

  // Remove entry cleanly
  await recents.removeProject(alphaEntry.id);
  assert.equal(recents.count(), 1);
});

test("RecoveryService distinguishes Unavailable vs Empty, rotates 3-slot ring, and detects unclean exit", async () => {
  const storage = new MemoryStorage();

  // 1. Unavailable state (never fabricates records)
  const unavailable = new RecoveryService({
    storage,
    idbFactory: null,
    forceUnavailable: true,
  });
  const unavailStatus = await unavailable.initialize();
  assert.equal(unavailStatus.storageState, "unavailable");
  assert.equal(unavailStatus.snapshots.length, 0);

  // 2. Available & genuinely empty state
  let now = 5000;
  const svc = new RecoveryService({
    storage,
    idbFactory: null,
    useMemoryBackend: true,
    nowFn: () => (now += 1000),
  });
  const emptyStatus = await svc.initialize();
  assert.equal(emptyStatus.storageState, "available");
  assert.equal(emptyStatus.hadUncleanPreviousSession, false);
  assert.equal(emptyStatus.snapshots.length, 0);

  // 3. Capture 4 snapshots -> ring buffer stays bounded at MAX_RECOVERY_RING_SLOTS (3)
  for (let i = 1; i <= 4; i++) {
    const res = await svc.captureSnapshot({
      projectName: `Scene Checkpoint ${i}`,
      originPath: `/workspace/scene_${i}.fe3d`,
      jsonText: JSON.stringify({ checkpoint: i }),
      objectCount: i + 1,
    });
    assert.equal(res.ok, true);
  }
  assert.equal(svc.getStatus().snapshots.length, MAX_RECOVERY_RING_SLOTS);
  const latest = await svc.getSnapshotPayload("latest");
  assert.equal(latest.projectName, "Scene Checkpoint 4");

  // 4. Simulate crash (no markCleanShutdown) -> next session detects unclean exit
  const afterCrash = new RecoveryService({
    storage,
    idbFactory: null,
    useMemoryBackend: true,
  });
  // Share memory snapshots map to simulate persistent IndexedDB across crash
  afterCrash.memorySnapshots = new Map(svc.memorySnapshots);
  const crashStatus = await afterCrash.initialize();
  assert.equal(crashStatus.hadUncleanPreviousSession, true);
  assert.equal(crashStatus.snapshots[0].uncleanSession, true);

  afterCrash.markCleanShutdown();
  const cleanBoot = new RecoveryService({
    storage,
    idbFactory: null,
    useMemoryBackend: true,
  });
  const cleanStatus = await cleanBoot.initialize();
  assert.equal(cleanStatus.hadUncleanPreviousSession, false);
});

test("ProjectOperationsService validates inputs, enforces overwrite confirmation, and maps 4 C++ templates", async () => {
  assert.equal(C_PLUS_PLUS_TEMPLATES.length, 4);
  assert.equal(
    buildProjectSavePath("/workspace/projects/", "Hero Shader", ""),
    "/workspace/projects/Hero_Shader.fe3d"
  );

  const storage = new MemoryStorage();
  const preferencesService = new PreferencesService({ storage, legacyRegistry: null });
  const recentProjectsService = new RecentProjectsService({ storage, idbFactory: null });
  const recoveryService = new RecoveryService({ storage, useMemoryBackend: true });
  await recoveryService.initialize();

  let dispatchedActions = [];
  const fakeStudio = {
    dispatch(cmd) {
      dispatchedActions.push(cmd);
      return true;
    },
    getState() {
      return {
        revision: 2,
        project: {
          project_name: "Hero Shader",
          author: "Final Effect 3D",
          scene_name: "Scene",
          file_path: "/workspace/projects/Hero_Shader.fe3d",
          is_dirty: false,
        },
        stats: { object_count: 3, material_count: 3 },
      };
    },
    exportInlineProject() {
      return '{"format":"finaleffect.fe3d","version":"1.3"}';
    },
    activateWorkspaceById() {},
    setView() {},
  };

  const ops = new ProjectOperationsService({
    preferencesService,
    recentProjectsService,
    recoveryService,
    getStudioBridge: () => fakeStudio,
  });

  // Empty name rejected
  const bad = await ops.createNewProject({ projectName: "   ", templateId: "shader_ball" });
  assert.equal(bad.ok, false);

  // First creation succeeds
  const ok1 = await ops.createNewProject(
    {
      projectName: "Hero Shader",
      templateId: "shader_ball",
      saveDirectory: "/workspace/projects",
    },
    { allowOverwrite: false }
  );
  assert.equal(ok1.ok, true);
  assert.equal(dispatchedActions[0].template, "shader_ball");

  // Duplicate path without allowOverwrite requires explicit confirmation
  const conflict = await ops.createNewProject(
    {
      projectName: "Hero Shader",
      templateId: "minimal_cube",
      saveDirectory: "/workspace/projects",
    },
    { allowOverwrite: false }
  );
  assert.equal(conflict.ok, false);
  assert.equal(conflict.requiresOverwriteConfirmation, true);

  // Confirmed overwrite succeeds
  const ok2 = await ops.createNewProject(
    {
      projectName: "Hero Shader",
      templateId: "minimal_cube",
      saveDirectory: "/workspace/projects",
    },
    { allowOverwrite: true }
  );
  assert.equal(ok2.ok, true);
});

test("ShortcutRegistry guards Space, Shift+Space, F3, and Alt shortcuts when text inputs are focused", () => {
  const storage = new MemoryStorage();
  const preferencesService = new PreferencesService({ storage, legacyRegistry: null });
  const registry = new ShortcutRegistry({
    preferencesService,
    getAppView: () => "home",
  });

  let paletteCount = 0;
  let navTemplatesCount = 0;
  registry.register("palette.open", () => {
    paletteCount++;
  });
  registry.register("hub.view_templates", () => {
    navTemplatesCount++;
  });

  const textInputTarget = { tagName: "INPUT", type: "text" };
  assert.equal(isEditableInputTarget(textInputTarget), true);

  // F3 and Alt+2 inside a text input MUST NOT fire shortcuts
  assert.equal(
    registry.handleKeyDown({ key: "F3", target: textInputTarget }),
    false
  );
  assert.equal(
    registry.handleKeyDown({ key: "2", altKey: true, target: textInputTarget }),
    false
  );
  // Space / Shift+Space inside text input MUST NOT be intercepted
  assert.equal(
    registry.handleKeyDown({ key: " ", shiftKey: true, target: textInputTarget }),
    false
  );
  assert.equal(paletteCount, 0);
  assert.equal(navTemplatesCount, 0);

  // Outside editable input, F3 and Alt+2 fire cleanly
  const divTarget = { tagName: "DIV" };
  assert.equal(registry.handleKeyDown({ key: "F3", target: divTarget }), true);
  assert.equal(
    registry.handleKeyDown({ key: "2", altKey: true, target: divTarget }),
    true
  );
  assert.equal(paletteCount, 1);
  assert.equal(navTemplatesCount, 1);
});
