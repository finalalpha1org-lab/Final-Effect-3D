// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 Desktop Shortcut Registry & Input Focus Guard
// Compatible with Stage 0 Decision A3 (Ctrl+K / F3 Command Palette, Space / Shift+Space
// reserved for Studio Context Action Menu / Play-Pause in Stage 2+, strict input guard).

export const DEFAULT_HUB_SHORTCUTS = Object.freeze([
  {
    id: "palette.open",
    label: "Open Command Palette",
    defaultCombo: "Ctrl+K",
    secondaryCombo: "F3",
    scope: "global",
  },
  {
    id: "hub.new_project",
    label: "New Project View",
    defaultCombo: "Ctrl+Alt+N",
    scope: "home",
  },
  {
    id: "hub.open_project",
    label: "Open Project View",
    defaultCombo: "Ctrl+Alt+O",
    scope: "home",
  },
  {
    id: "hub.preferences",
    label: "Application Preferences",
    defaultCombo: "Ctrl+,",
    scope: "home",
  },
  {
    id: "hub.view_projects",
    label: "Switch to Projects & Start",
    defaultCombo: "Alt+1",
    scope: "home",
  },
  {
    id: "hub.view_templates",
    label: "Switch to Templates",
    defaultCombo: "Alt+2",
    scope: "home",
  },
  {
    id: "hub.view_new",
    label: "Switch to New Project",
    defaultCombo: "Alt+3",
    scope: "home",
  },
  {
    id: "hub.view_open",
    label: "Switch to Open Project",
    defaultCombo: "Alt+4",
    scope: "home",
  },
  {
    id: "hub.view_recovery",
    label: "Switch to Recovery & Autosave",
    defaultCombo: "Alt+5",
    scope: "home",
  },
  {
    id: "hub.view_preferences",
    label: "Switch to Preferences",
    defaultCombo: "Alt+6",
    scope: "home",
  },
  {
    id: "hub.view_about",
    label: "Switch to About / System Info",
    defaultCombo: "Alt+7",
    scope: "home",
  },
]);

export function isEditableInputTarget(target) {
  if (!target || typeof target !== "object") return false;
  if (target.isContentEditable) return true;
  const tag = String(target.tagName || "").toUpperCase();
  if (tag === "TEXTAREA" || tag === "SELECT") return true;
  if (tag === "INPUT") {
    const type = String(target.type || "text").toLowerCase();
    return !["button", "submit", "reset", "checkbox", "radio"].includes(type);
  }
  return false;
}

export function normalizeEventCombo(ev) {
  if (!ev || !ev.key) return "";
  const parts = [];
  if (ev.ctrlKey || ev.metaKey) parts.push("Ctrl");
  if (ev.altKey) parts.push("Alt");
  if (ev.shiftKey) parts.push("Shift");

  let key = ev.key;
  if (key === " ") key = "Space";
  else if (key.length === 1) key = key.toUpperCase();

  if (["Control", "Shift", "Alt", "Meta"].includes(ev.key)) {
    return "";
  }
  parts.push(key);
  return parts.join("+");
}

export class ShortcutRegistry {
  constructor({ preferencesService, getAppView }) {
    this.preferences = preferencesService;
    this.getAppView = getAppView || (() => "home");
    this.handlers = new Map();
  }

  register(actionId, fn) {
    this.handlers.set(actionId, fn);
  }

  getEffectiveCombo(actionId) {
    const overrides = this.preferences ? this.preferences.get().keymapOverrides : {};
    if (overrides && overrides[actionId]) {
      return overrides[actionId];
    }
    const def = DEFAULT_HUB_SHORTCUTS.find((s) => s.id === actionId);
    return def ? def.defaultCombo : "";
  }

  listBindings() {
    return DEFAULT_HUB_SHORTCUTS.map((item) => ({
      ...item,
      effectiveCombo: this.getEffectiveCombo(item.id),
    }));
  }

  handleKeyDown(ev) {
    const target = ev?.target || null;
    const inEditable = isEditableInputTarget(target);
    const combo = normalizeEventCombo(ev);
    if (!combo) return false;

    // Strict input focus guard: never hijack Space, Shift+Space, F3, or unmodified keys inside text fields.
    if (inEditable) {
      if (combo !== "Ctrl+K" && combo !== "Escape") {
        return false;
      }
    }

    const appView = this.getAppView();
    for (const def of DEFAULT_HUB_SHORTCUTS) {
      if (def.scope === "home" && appView !== "home") {
        continue;
      }
      const primary = this.getEffectiveCombo(def.id).toLowerCase();
      const secondary = (def.secondaryCombo || "").toLowerCase();
      if (combo.toLowerCase() === primary || (secondary && combo.toLowerCase() === secondary)) {
        const handler = this.handlers.get(def.id);
        if (typeof handler === "function") {
          if (typeof ev.preventDefault === "function") ev.preventDefault();
          handler(ev);
          return true;
        }
      }
    }
    return false;
  }
}
