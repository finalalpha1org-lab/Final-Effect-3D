// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 Desktop Command Palette (Ctrl+K / F3)

import { escapeHtml } from "../ui/components.js";

export class CommandPaletteController {
  constructor({ mountEl, getCommands }) {
    this.mountEl = mountEl;
    this.getCommands = getCommands;
    this.isOpen = false;
    this.query = "";
    this.selectedIndex = 0;
    this.filtered = [];
    this._buildDom();
  }

  _buildDom() {
    if (!this.mountEl) return;
    this.mountEl.innerHTML = `
      <div class="fe-dialog-card" role="dialog" aria-modal="true" aria-label="Final Effect 3D Command Palette">
        <div class="fe-dialog-head">
          <input
            type="text"
            class="fe-input"
            style="width: 100%;"
            data-testid="command-palette-input"
            placeholder="Type a command or jump to a Hub view (Projects, Templates, Preferences, System Info)…"
            autocomplete="off"
          />
          <button type="button" class="fe-btn fe-btn-sm fe-btn-ghost" data-testid="command-palette-close">Esc</button>
        </div>
        <div class="fe-palette-list" data-testid="command-palette-list" role="listbox"></div>
      </div>
    `;
    this.inputEl = this.mountEl.querySelector('[data-testid="command-palette-input"]');
    this.listEl = this.mountEl.querySelector('[data-testid="command-palette-list"]');
    const closeBtn = this.mountEl.querySelector('[data-testid="command-palette-close"]');

    this.mountEl.addEventListener("click", (ev) => {
      if (ev.target === this.mountEl) this.close();
    });
    if (closeBtn) closeBtn.addEventListener("click", () => this.close());

    if (this.inputEl) {
      this.inputEl.addEventListener("input", () => {
        this.query = this.inputEl.value;
        this.selectedIndex = 0;
        this._renderList();
      });
      this.inputEl.addEventListener("keydown", (ev) => {
        if (ev.key === "Escape") {
          ev.preventDefault();
          ev.stopPropagation();
          this.close();
        } else if (ev.key === "ArrowDown") {
          ev.preventDefault();
          if (this.filtered.length > 0) {
            this.selectedIndex = (this.selectedIndex + 1) % this.filtered.length;
            this._renderList();
          }
        } else if (ev.key === "ArrowUp") {
          ev.preventDefault();
          if (this.filtered.length > 0) {
            this.selectedIndex = (this.selectedIndex - 1 + this.filtered.length) % this.filtered.length;
            this._renderList();
          }
        } else if (ev.key === "Enter") {
          ev.preventDefault();
          const chosen = this.filtered[this.selectedIndex];
          if (chosen && typeof chosen.run === "function") {
            this.close();
            chosen.run();
          }
        }
      });
    }
  }

  open() {
    if (!this.mountEl) return;
    this.isOpen = true;
    this.query = "";
    this.selectedIndex = 0;
    this.mountEl.hidden = false;
    if (this.inputEl) {
      this.inputEl.value = "";
      this._renderList();
      this.inputEl.focus();
    }
  }

  close() {
    if (!this.mountEl) return;
    this.isOpen = false;
    this.mountEl.hidden = true;
  }

  toggle() {
    if (this.isOpen) this.close();
    else this.open();
  }

  _renderList() {
    if (!this.listEl) return;
    const all = typeof this.getCommands === "function" ? this.getCommands() : [];
    const needle = this.query.trim().toLowerCase();
    this.filtered = all.filter((cmd) => {
      if (!needle) return true;
      return (
        cmd.label.toLowerCase().includes(needle) ||
        (cmd.category && cmd.category.toLowerCase().includes(needle))
      );
    });

    if (this.filtered.length === 0) {
      this.listEl.innerHTML = `<div style="padding: 14px 12px; color: var(--fe-text-muted); font-size: 12px;">No matching commands found.</div>`;
      return;
    }

    this.listEl.innerHTML = this.filtered
      .map(
        (cmd, idx) => `
          <button
            type="button"
            class="fe-palette-item ${idx === this.selectedIndex ? "is-selected" : ""}"
            data-palette-idx="${idx}"
            role="option"
            aria-selected="${idx === this.selectedIndex ? "true" : "false"}"
          >
            <span>
              <strong style="color: var(--fe-accent-primary); font-weight: 600; margin-right: 6px;">${escapeHtml(cmd.category || "Hub")}</strong>
              ${escapeHtml(cmd.label)}
            </span>
            ${cmd.shortcut ? `<kbd class="fe-kbd">${escapeHtml(cmd.shortcut)}</kbd>` : ""}
          </button>
        `
      )
      .join("");

    this.listEl.querySelectorAll("[data-palette-idx]").forEach((btn) => {
      btn.addEventListener("click", () => {
        const idx = Number(btn.getAttribute("data-palette-idx"));
        const chosen = this.filtered[idx];
        if (chosen && typeof chosen.run === "function") {
          this.close();
          chosen.run();
        }
      });
    });
  }
}
