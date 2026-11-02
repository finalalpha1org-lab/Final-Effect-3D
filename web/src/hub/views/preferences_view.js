// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 View 6: Application Preferences
// Persisted strictly outside .fe3d scene documents. Desktop-only settings.

import { escapeHtml } from "../../ui/components.js";

export function createPreferencesView(ctx) {
  let savedFeedback = "";

  function render(mountEl) {
    const prefs = ctx.preferences.get();
    const shortcuts = ctx.shortcuts.listBindings();

    mountEl.innerHTML = `
      <div class="fe-hub-container" data-testid="hub-view-preferences">
        <div class="fe-view-header">
          <div>
            <div class="fe-view-eyebrow">APPLICATION CONFIGURATION</div>
            <h1 class="fe-view-title">Preferences</h1>
            <p class="fe-view-subtitle">
              Desktop application preferences persist independently of <code>.fe3d</code> project files. Opening external scenes never overwrites your interface scale, navigation profile, or autosave settings.
            </p>
          </div>
          <div class="fe-view-actions">
            <button type="button" class="fe-btn" data-testid="btn-reset-preferences">
              Reset to Defaults
            </button>
          </div>
        </div>

        <div
          class="fe-banner fe-banner-success"
          data-testid="preferences-saved-banner"
          ${savedFeedback ? "" : "hidden"}
        >
          <span>${escapeHtml(savedFeedback)}</span>
        </div>

        <section class="fe-panel" aria-label="Interface & Startup">
          <div class="fe-panel-head">
            <h2 class="fe-panel-title">Interface &amp; Startup</h2>
            <span class="fe-tag fe-tag-cyan">Obsidian Slate Theme</span>
          </div>

          <div class="fe-pref-row">
            <div>
              <div style="font-weight: 580;">Desktop UI Scale</div>
              <div class="fe-field-hint">Scales typography, controls, and Hub navigation density immediately.</div>
            </div>
            <select class="fe-select" data-testid="pref-ui-scale-select">
              <option value="85" ${prefs.uiScalePercent === 85 ? "selected" : ""}>85% (Compact High-DPI)</option>
              <option value="100" ${prefs.uiScalePercent === 100 ? "selected" : ""}>100% (Default Desktop)</option>
              <option value="115" ${prefs.uiScalePercent === 115 ? "selected" : ""}>115% (Comfortable)</option>
              <option value="125" ${prefs.uiScalePercent === 125 ? "selected" : ""}>125% (Large Display)</option>
            </select>
          </div>

          <div class="fe-pref-row">
            <div>
              <div style="font-weight: 580;">Default Startup Environment</div>
              <div class="fe-field-hint">Choose whether Final Effect 3D opens to the Project Hub or boots directly into 3D Studio.</div>
            </div>
            <select class="fe-select" data-testid="pref-startup-select">
              <option value="home_hub" ${prefs.startupEnvironment === "home_hub" ? "selected" : ""}>Project Hub (Home)</option>
              <option value="default_scene" ${prefs.startupEnvironment === "default_scene" ? "selected" : ""}>Direct 3D Studio (Starter Scene)</option>
            </select>
          </div>

          <div class="fe-pref-row">
            <div>
              <div style="font-weight: 580;">Show 3D Viewport Heads-Up Display (HUD)</div>
              <div class="fe-field-hint">Displays camera, selection, and mode pills inside the 3D Studio viewport.</div>
            </div>
            <input
              type="checkbox"
              data-testid="pref-viewport-hud-toggle"
              ${prefs.showViewportHud ? "checked" : ""}
              style="width: 17px; height: 17px; accent-color: var(--fe-accent-primary);"
            />
          </div>
        </section>

        <section class="fe-panel" aria-label="Navigation & New Project Defaults">
          <div class="fe-panel-head">
            <h2 class="fe-panel-title">Navigation &amp; Authoring Defaults</h2>
          </div>

          <div class="fe-pref-row">
            <div>
              <div style="font-weight: 580;">Viewport Navigation Profile</div>
              <div class="fe-field-hint">Selects preferred desktop mouse orbit/pan/dolly conventions for Studio sessions.</div>
            </div>
            <select class="fe-select" data-testid="pref-nav-profile-select">
              <option value="fe3d_hybrid" ${prefs.navigationProfile === "fe3d_hybrid" ? "selected" : ""}>Final Effect Hybrid (MMB + Alt-Mouse)</option>
              <option value="blender" ${prefs.navigationProfile === "blender" ? "selected" : ""}>Blender Compatible (MMB Orbit / Shift+MMB Pan)</option>
              <option value="maya" ${prefs.navigationProfile === "maya" ? "selected" : ""}>Industry Alt-Navigation (Alt+LMB / Alt+MMB / Alt+RMB)</option>
            </select>
          </div>

          <div class="fe-pref-row">
            <div>
              <div style="font-weight: 580;">Default Project Author</div>
              <div class="fe-field-hint">Pre-populated when creating new <code>.fe3d</code> projects.</div>
            </div>
            <input
              type="text"
              class="fe-input"
              style="width: 250px;"
              data-testid="pref-author-input"
              value="${escapeHtml(prefs.defaultAuthor)}"
              autocomplete="off"
            />
          </div>

          <div class="fe-pref-row">
            <div>
              <div style="font-weight: 580;">Default Linear Units</div>
              <div class="fe-field-hint">Initial unit scale preselected in New Project.</div>
            </div>
            <select class="fe-select" data-testid="pref-units-select">
              <option value="meters" ${prefs.defaultUnits === "meters" ? "selected" : ""}>Meters (m)</option>
              <option value="centimeters" ${prefs.defaultUnits === "centimeters" ? "selected" : ""}>Centimeters (cm)</option>
              <option value="millimeters" ${prefs.defaultUnits === "millimeters" ? "selected" : ""}>Millimeters (mm)</option>
            </select>
          </div>
        </section>

        <section class="fe-panel" aria-label="Autosave & Recent History">
          <div class="fe-panel-head">
            <h2 class="fe-panel-title">Autosave &amp; Recent History</h2>
          </div>

          <div class="fe-pref-row">
            <div>
              <div style="font-weight: 580;">Enable Periodic Recovery Autosave</div>
              <div class="fe-field-hint">Captures ring-buffer recovery snapshots only when unsaved Studio changes exist.</div>
            </div>
            <input
              type="checkbox"
              data-testid="pref-autosave-toggle"
              ${prefs.autosaveEnabled ? "checked" : ""}
              style="width: 17px; height: 17px; accent-color: var(--fe-accent-primary);"
            />
          </div>

          <div class="fe-pref-row">
            <div>
              <div style="font-weight: 580;">Autosave Interval</div>
              <div class="fe-field-hint">Minimum seconds between dirty-session recovery checkpoints (30–900s).</div>
            </div>
            <select class="fe-select" data-testid="pref-autosave-interval-select">
              <option value="60" ${prefs.autosaveIntervalSeconds === 60 ? "selected" : ""}>Every 1 minute (60s)</option>
              <option value="180" ${prefs.autosaveIntervalSeconds === 180 ? "selected" : ""}>Every 3 minutes (180s)</option>
              <option value="300" ${prefs.autosaveIntervalSeconds === 300 ? "selected" : ""}>Every 5 minutes (300s)</option>
              <option value="600" ${prefs.autosaveIntervalSeconds === 600 ? "selected" : ""}>Every 10 minutes (600s)</option>
            </select>
          </div>

          <div class="fe-pref-row">
            <div>
              <div style="font-weight: 580;">Recent Projects History Limit</div>
              <div class="fe-field-hint">Maximum unpinned projects retained in the Project Hub index.</div>
            </div>
            <select class="fe-select" data-testid="pref-recent-limit-select">
              <option value="10" ${prefs.recentProjectsLimit === 10 ? "selected" : ""}>10 Projects</option>
              <option value="20" ${prefs.recentProjectsLimit === 20 ? "selected" : ""}>20 Projects</option>
              <option value="35" ${prefs.recentProjectsLimit === 35 ? "selected" : ""}>35 Projects</option>
              <option value="50" ${prefs.recentProjectsLimit === 50 ? "selected" : ""}>50 Projects</option>
            </select>
          </div>
        </section>

        <section class="fe-panel" aria-label="Hub & Global Shortcuts">
          <div class="fe-panel-head">
            <h2 class="fe-panel-title">Desktop Hub &amp; Command Palette Shortcuts</h2>
            <span class="fe-tag">Input-Focus Guarded</span>
          </div>
          <div class="fe-kv-grid">
            ${shortcuts
              .slice(0, 6)
              .map(
                (sc) => `
                <div class="fe-kv-row">
                  <span class="fe-kv-key">${escapeHtml(sc.label)}</span>
                  <input
                    type="text"
                    class="fe-input"
                    style="width: 130px; height: 28px; font-family: var(--fe-font-mono); font-size: 11px; text-align: center;"
                    data-shortcut-id="${escapeHtml(sc.id)}"
                    value="${escapeHtml(sc.effectiveCombo)}"
                  />
                </div>
              `
              )
              .join("")}
          </div>
        </section>
      </div>
    `;

    bindPrefEvents(mountEl);
  }

  function showSaved(mountEl, msg) {
    savedFeedback = msg;
    ctx.preferences.applyToDocument();
    render(mountEl);
  }

  function bindPrefEvents(mountEl) {
    const scaleSel = mountEl.querySelector('[data-testid="pref-ui-scale-select"]');
    if (scaleSel) {
      scaleSel.addEventListener("change", () => {
        ctx.preferences.update({ uiScalePercent: Number(scaleSel.value) });
        showSaved(mountEl, `Saved UI scale (${scaleSel.value}%).`);
      });
    }

    const startupSel = mountEl.querySelector('[data-testid="pref-startup-select"]');
    if (startupSel) {
      startupSel.addEventListener("change", () => {
        ctx.preferences.update({ startupEnvironment: startupSel.value });
        showSaved(mountEl, "Updated startup environment preference.");
      });
    }

    const hudToggle = mountEl.querySelector('[data-testid="pref-viewport-hud-toggle"]');
    if (hudToggle) {
      hudToggle.addEventListener("change", () => {
        ctx.preferences.update({ showViewportHud: hudToggle.checked });
        showSaved(mountEl, "Updated 3D Viewport HUD visibility.");
      });
    }

    const navSel = mountEl.querySelector('[data-testid="pref-nav-profile-select"]');
    if (navSel) {
      navSel.addEventListener("change", () => {
        ctx.preferences.update({ navigationProfile: navSel.value });
        showSaved(mountEl, "Saved viewport navigation profile.");
      });
    }

    const authorInput = mountEl.querySelector('[data-testid="pref-author-input"]');
    if (authorInput) {
      authorInput.addEventListener("change", () => {
        ctx.preferences.update({ defaultAuthor: authorInput.value });
        showSaved(mountEl, "Saved default project author.");
      });
    }

    const unitsSel = mountEl.querySelector('[data-testid="pref-units-select"]');
    if (unitsSel) {
      unitsSel.addEventListener("change", () => {
        ctx.preferences.update({ defaultUnits: unitsSel.value });
        showSaved(mountEl, `Saved default linear units (${unitsSel.value}).`);
      });
    }

    const autosaveToggle = mountEl.querySelector('[data-testid="pref-autosave-toggle"]');
    if (autosaveToggle) {
      autosaveToggle.addEventListener("change", () => {
        ctx.preferences.update({ autosaveEnabled: autosaveToggle.checked });
        showSaved(mountEl, `Periodic recovery autosave ${autosaveToggle.checked ? "enabled" : "disabled"}.`);
      });
    }

    const intervalSel = mountEl.querySelector('[data-testid="pref-autosave-interval-select"]');
    if (intervalSel) {
      intervalSel.addEventListener("change", () => {
        ctx.preferences.update({ autosaveIntervalSeconds: Number(intervalSel.value) });
        showSaved(mountEl, `Saved autosave interval (${intervalSel.value}s).`);
      });
    }

    const limitSel = mountEl.querySelector('[data-testid="pref-recent-limit-select"]');
    if (limitSel) {
      limitSel.addEventListener("change", () => {
        const limit = Number(limitSel.value);
        ctx.preferences.update({ recentProjectsLimit: limit });
        ctx.recents.enforceLimit(limit);
        showSaved(mountEl, `Saved recent projects limit (${limit}).`);
      });
    }

    mountEl.querySelectorAll("[data-shortcut-id]").forEach((inp) => {
      inp.addEventListener("change", () => {
        const id = inp.getAttribute("data-shortcut-id");
        ctx.preferences.setKeymapOverride(id, inp.value);
        showSaved(mountEl, `Updated shortcut binding for '${id}'.`);
      });
    });

    const resetBtn = mountEl.querySelector('[data-testid="btn-reset-preferences"]');
    if (resetBtn) {
      resetBtn.addEventListener("click", () => {
        ctx.preferences.resetToDefaults();
        showSaved(mountEl, "Restored all application preferences to factory defaults.");
      });
    }
  }

  return { render };
}
