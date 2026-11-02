// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 Application Preferences Service
// Persisted strictly outside .fe3d scene/project documents.

export const PREFERENCES_STORAGE_KEY = "fe3d.stage1.preferences.v1";

export const ALLOWED_UI_SCALES = Object.freeze([85, 100, 115, 125]);
export const ALLOWED_STARTUP_ENVIRONMENTS = Object.freeze(["home_hub", "default_scene"]);
export const ALLOWED_NAVIGATION_PROFILES = Object.freeze(["fe3d_hybrid", "blender", "maya"]);
export const ALLOWED_UNITS = Object.freeze(["meters", "centimeters", "millimeters"]);

export const DEFAULT_PREFERENCES = Object.freeze({
  format: "finaleffect.preferences",
  version: 1,
  uiScalePercent: 100,
  startupEnvironment: "home_hub",
  defaultAuthor: "Final Effect 3D",
  defaultUnits: "meters",
  defaultFps: 24,
  navigationProfile: "fe3d_hybrid",
  recentProjectsLimit: 20,
  autosaveEnabled: true,
  autosaveIntervalSeconds: 180,
  showViewportHud: true,
  keymapOverrides: Object.freeze({}),
});

export function snapUiScale(raw) {
  const num = Number(raw);
  if (!Number.isFinite(num)) return 100;
  let best = 100;
  let bestDiff = Infinity;
  for (const candidate of ALLOWED_UI_SCALES) {
    const diff = Math.abs(num - candidate);
    if (diff < bestDiff) {
      bestDiff = diff;
      best = candidate;
    }
  }
  return best;
}

export function sanitizePreferences(input = {}) {
  const src = input && typeof input === "object" ? input : {};
  const startupEnvironment = ALLOWED_STARTUP_ENVIRONMENTS.includes(src.startupEnvironment)
    ? src.startupEnvironment
    : DEFAULT_PREFERENCES.startupEnvironment;
  const navigationProfile = ALLOWED_NAVIGATION_PROFILES.includes(src.navigationProfile)
    ? src.navigationProfile
    : DEFAULT_PREFERENCES.navigationProfile;
  const defaultUnits = ALLOWED_UNITS.includes(src.defaultUnits)
    ? src.defaultUnits
    : DEFAULT_PREFERENCES.defaultUnits;

  const rawAuthor = typeof src.defaultAuthor === "string" ? src.defaultAuthor.trim() : "";
  const defaultAuthor = (rawAuthor || DEFAULT_PREFERENCES.defaultAuthor).slice(0, 128);

  const rawLimit = Number(src.recentProjectsLimit);
  const recentProjectsLimit = Number.isFinite(rawLimit)
    ? Math.max(5, Math.min(50, Math.round(rawLimit)))
    : DEFAULT_PREFERENCES.recentProjectsLimit;

  const rawInterval = Number(src.autosaveIntervalSeconds);
  const autosaveIntervalSeconds = Number.isFinite(rawInterval)
    ? Math.max(30, Math.min(900, Math.round(rawInterval)))
    : DEFAULT_PREFERENCES.autosaveIntervalSeconds;

  const rawFps = Number(src.defaultFps);
  const defaultFps = Number.isFinite(rawFps)
    ? Math.max(1, Math.min(240, Math.round(rawFps)))
    : DEFAULT_PREFERENCES.defaultFps;

  const keymapOverrides = {};
  if (src.keymapOverrides && typeof src.keymapOverrides === "object") {
    for (const [k, v] of Object.entries(src.keymapOverrides)) {
      const actionId = String(k || "").trim();
      const combo = typeof v === "string" ? v.trim() : "";
      if (actionId && combo) {
        keymapOverrides[actionId] = combo;
      }
    }
  }

  return {
    format: "finaleffect.preferences",
    version: 1,
    uiScalePercent: snapUiScale(src.uiScalePercent ?? DEFAULT_PREFERENCES.uiScalePercent),
    startupEnvironment,
    defaultAuthor,
    defaultUnits,
    defaultFps,
    navigationProfile,
    recentProjectsLimit,
    autosaveEnabled:
      typeof src.autosaveEnabled === "boolean"
        ? src.autosaveEnabled
        : DEFAULT_PREFERENCES.autosaveEnabled,
    autosaveIntervalSeconds,
    showViewportHud:
      typeof src.showViewportHud === "boolean"
        ? src.showViewportHud
        : DEFAULT_PREFERENCES.showViewportHud,
    keymapOverrides,
  };
}

export class PreferencesService {
  constructor(options = {}) {
    this.storageKey = options.storageKey || PREFERENCES_STORAGE_KEY;
    this.storage =
      options.storage !== undefined
        ? options.storage
        : typeof window !== "undefined"
        ? window.localStorage
        : null;
    this.legacyRegistry =
      options.legacyRegistry !== undefined
        ? options.legacyRegistry
        : typeof window !== "undefined"
        ? window.FE3D_REGISTRY
        : null;
    this.listeners = new Set();
    this.state = this._readInitialState();
  }

  _readInitialState() {
    try {
      if (this.storage) {
        const raw = this.storage.getItem(this.storageKey);
        if (raw) {
          return sanitizePreferences(JSON.parse(raw));
        }
      }
      if (this.legacyRegistry && typeof this.legacyRegistry.loadPreferences === "function") {
        const legacy = this.legacyRegistry.loadPreferences();
        if (legacy && typeof legacy === "object") {
          return sanitizePreferences({
            defaultAuthor: legacy.defaultAuthor,
            showViewportHud: legacy.showViewportHud,
          });
        }
      }
    } catch (_) {
      // Corrupted storage falls back cleanly to defaults.
    }
    return sanitizePreferences(DEFAULT_PREFERENCES);
  }

  get() {
    return {
      ...this.state,
      keymapOverrides: { ...this.state.keymapOverrides },
    };
  }

  update(patch = {}) {
    this.state = sanitizePreferences({
      ...this.state,
      ...patch,
      keymapOverrides:
        patch && patch.keymapOverrides !== undefined
          ? patch.keymapOverrides
          : this.state.keymapOverrides,
    });
    this._persist();
    this._notify();
    return this.get();
  }

  setKeymapOverride(actionId, combo) {
    const nextMap = { ...this.state.keymapOverrides };
    if (!combo || !String(combo).trim()) {
      delete nextMap[actionId];
    } else {
      nextMap[actionId] = String(combo).trim();
    }
    return this.update({ keymapOverrides: nextMap });
  }

  resetToDefaults() {
    this.state = sanitizePreferences(DEFAULT_PREFERENCES);
    this._persist();
    this._notify();
    return this.get();
  }

  applyToDocument(doc) {
    const targetDoc = doc || (typeof document !== "undefined" ? document : null);
    if (!targetDoc || !targetDoc.documentElement) return;
    const scaleRatio = (this.state.uiScalePercent / 100).toFixed(2);
    targetDoc.documentElement.style.setProperty("--fe-ui-scale", scaleRatio);
    if (targetDoc.body) {
      targetDoc.body.dataset.feNavProfile = this.state.navigationProfile;
      targetDoc.body.dataset.feUiScale = String(this.state.uiScalePercent);
    }
    // Keep Studio's viewport HUD visibility and default author synchronized without touching .fe3d data.
    const hudTop = targetDoc.getElementById("viewport-hud-top");
    const hudBottom = targetDoc.getElementById("viewport-hud-bottom");
    if (hudTop) hudTop.hidden = !this.state.showViewportHud;
    if (hudBottom) hudBottom.hidden = !this.state.showViewportHud;
  }

  subscribe(listener) {
    if (typeof listener !== "function") return () => {};
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  _persist() {
    try {
      if (this.storage) {
        this.storage.setItem(this.storageKey, JSON.stringify(this.state));
      }
      if (this.legacyRegistry && typeof this.legacyRegistry.savePreferences === "function") {
        const prev =
          typeof this.legacyRegistry.loadPreferences === "function"
            ? this.legacyRegistry.loadPreferences()
            : {};
        this.legacyRegistry.savePreferences({
          ...prev,
          layoutMode: "desktop",
          defaultAuthor: this.state.defaultAuthor,
          showViewportHud: this.state.showViewportHud,
        });
      }
    } catch (_) {}
  }

  _notify() {
    const snapshot = this.get();
    for (const fn of this.listeners) {
      fn(snapshot);
    }
  }
}
