// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 View 4: Open Project (Desktop File Picker + Drag/Drop + Honest Error Handling)

import {
  escapeHtml,
  formatByteSize,
  formatRelativeTime,
  HUB_ICONS,
} from "../../ui/components.js";

export function createOpenProjectView(ctx) {
  let lastError = "";

  function setError(msg) {
    lastError = String(msg || "");
  }

  function render(mountEl) {
    const fsCaps = ctx.projectOps.detectFileSystemCapabilities();
    const recents = ctx.recents.query({ sort: "recent" }).slice(0, 8);

    mountEl.innerHTML = `
      <div class="fe-hub-container" data-testid="hub-view-open-project">
        <div class="fe-view-header">
          <div>
            <div class="fe-view-eyebrow">OPEN EXISTING DOCUMENT</div>
            <h1 class="fe-view-title">Open Project</h1>
            <p class="fe-view-subtitle">
              Open a Final Effect 3D <code>.fe3d</code> project file (and optional companion <code>.fe3dbin</code> binary mesh archive) from local storage. Invalid files report clear diagnostics without corrupting your active session.
            </p>
          </div>
          <div class="fe-view-actions">
            <button type="button" class="fe-btn fe-btn-primary" data-testid="btn-open-view-browse">
              ${HUB_ICONS.folder}
              <span>Browse Local Files…</span>
            </button>
          </div>
        </div>

        <div
          class="fe-banner fe-banner-error"
          data-testid="open-project-error"
          ${lastError ? "" : "hidden"}
        >
          <span>${escapeHtml(lastError)}</span>
        </div>

        <div class="fe-dropzone" data-testid="open-project-dropzone">
          <div style="width: 42px; height: 42px; border-radius: 8px; background: var(--fe-accent-primary-muted); color: var(--fe-accent-primary); display: flex; align-items: center; justify-content: center;">
            ${HUB_ICONS.folder}
          </div>
          <div style="font-size: 15px; font-weight: 630; color: var(--fe-text-primary);">
            Drop <code>.fe3d</code> (+ optional <code>.fe3dbin</code>) files here or browse disk
          </div>
          <div style="max-width: 560px; font-size: 12.5px; color: var(--fe-text-secondary);">
            Supports inline JSON <code>.fe3d</code> schema v1.0–v1.3 and deduplicated binary bundle archives (select both <code>.fe3d</code> and <code>.fe3dbin</code> together when opening a bundle).
          </div>
          <div style="display: flex; gap: 10px; margin-top: 6px;">
            <button type="button" class="fe-btn fe-btn-primary" data-testid="btn-dropzone-browse">
              Select .fe3d Project File…
            </button>
            ${
              fsCaps.openPicker
                ? `<span class="fe-tag fe-tag-cyan">File System Access API Active</span>`
                : `<span class="fe-tag">Standard Browser File Input Fallback Active</span>`
            }
          </div>
        </div>

        <section class="fe-panel" aria-label="Quick Reopen Recent">
          <div class="fe-panel-head">
            <h2 class="fe-panel-title">Quick Reopen Recent Projects</h2>
            <span class="fe-tag">${recents.length} shown</span>
          </div>
          ${
            recents.length === 0
              ? `<p style="margin: 0; color: var(--fe-text-muted); font-size: 12.5px;">No recent projects have been opened in this browser profile yet.</p>`
              : `<div class="fe-recent-list">
                  ${recents
                    .map(
                      (item) => `
                      <div class="fe-recent-card" data-testid="open-view-recent-item">
                        <div class="fe-recent-main">
                          <div class="fe-recent-meta">
                            <div class="fe-recent-title-row">
                              <span class="fe-recent-title">${escapeHtml(item.name)}</span>
                              ${
                                item.available
                                  ? `<span class="fe-tag fe-tag-emerald">Cached</span>`
                                  : `<span class="fe-tag fe-tag-amber">Disk Required</span>`
                              }
                            </div>
                            <div class="fe-recent-path">${escapeHtml(item.filePath)}</div>
                            <div class="fe-recent-stats">${formatByteSize(item.byteSize)} &middot; ${formatRelativeTime(
                        item.updatedAt
                      )}</div>
                          </div>
                        </div>
                        <div class="fe-recent-actions">
                          <button type="button" class="fe-btn fe-btn-sm fe-btn-primary" data-open-recent-id="${escapeHtml(
                            item.id
                          )}">Reopen</button>
                        </div>
                      </div>
                    `
                    )
                    .join("")}
                </div>`
          }
        </section>
      </div>
    `;

    const handleBrowse = async () => {
      lastError = "";
      await ctx.triggerOpenFlow({
        onError: (errMsg) => {
          lastError = errMsg;
          render(mountEl);
        },
      });
    };

    const browseTop = mountEl.querySelector('[data-testid="btn-open-view-browse"]');
    const browseDrop = mountEl.querySelector('[data-testid="btn-dropzone-browse"]');
    if (browseTop) browseTop.addEventListener("click", handleBrowse);
    if (browseDrop) browseDrop.addEventListener("click", handleBrowse);

    const dropzone = mountEl.querySelector('[data-testid="open-project-dropzone"]');
    if (dropzone) {
      dropzone.addEventListener("dragover", (ev) => {
        ev.preventDefault();
        dropzone.classList.add("is-dragover");
      });
      dropzone.addEventListener("dragleave", () => {
        dropzone.classList.remove("is-dragover");
      });
      dropzone.addEventListener("drop", async (ev) => {
        ev.preventDefault();
        dropzone.classList.remove("is-dragover");
        if (!ev.dataTransfer || !ev.dataTransfer.files || ev.dataTransfer.files.length === 0) {
          return;
        }
        const allowed = await ctx.confirmReplaceDirtySessionIfNeeded();
        if (!allowed) return;
        const res = await ctx.projectOps.openFilesList(ev.dataTransfer.files);
        if (!res.ok && !res.cancelled) {
          lastError = res.error || "Failed to open dropped file.";
          render(mountEl);
        }
      });
    }

    mountEl.querySelectorAll("[data-open-recent-id]").forEach((btn) => {
      btn.addEventListener("click", async () => {
        const id = btn.getAttribute("data-open-recent-id");
        const entry = ctx.recents.findById(id);
        if (!entry) return;
        const res = await ctx.reopenEntryWithGuard(entry);
        if (res && !res.ok) {
          lastError = res.error || "Failed to reopen project.";
          render(mountEl);
        }
      });
    });
  }

  return { render, setError };
}
