// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 View 1: Projects & Start

import {
  escapeHtml,
  formatByteSize,
  formatRelativeTime,
  HUB_ICONS,
} from "../../ui/components.js";

export function createProjectsStartView(ctx) {
  const uiState = {
    search: "",
    sort: "recent",
    filter: "all",
    layout: "list",
  };

  function render(mountEl) {
    const items = ctx.recents.query({
      search: uiState.search,
      sort: uiState.sort,
      filter: uiState.filter,
    });
    const totalCount = ctx.recents.count();
    const session = ctx.projectOps.getActiveSessionSummary();

    mountEl.innerHTML = `
      <div class="fe-hub-container" data-testid="hub-view-projects">
        <div class="fe-view-header">
          <div>
            <div class="fe-view-eyebrow">STAGE 1 PROJECT HUB</div>
            <h1 class="fe-view-title">Projects &amp; Start</h1>
            <p class="fe-view-subtitle">
              Create calibrated 3D scenes from C++20 templates, open portable <code>.fe3d</code> documents, or resume recent desktop work.
            </p>
          </div>
          <div class="fe-view-actions">
            ${
              session.hasActiveSession
                ? `<button type="button" class="fe-btn fe-btn-amber" data-testid="btn-projects-resume-session">
                    Resume '${escapeHtml(session.projectName)}'${session.isDirty ? " • Unsaved" : ""}
                  </button>`
                : ""
            }
            <button type="button" class="fe-btn" data-testid="btn-projects-open-dialog">
              ${HUB_ICONS.folder}
              <span>Open .fe3d…</span>
            </button>
            <button type="button" class="fe-btn fe-btn-primary" data-testid="btn-projects-go-new">
              ${HUB_ICONS.plus}
              <span>New Project…</span>
            </button>
          </div>
        </div>

        <div class="fe-quickstart-grid">
          <button type="button" class="fe-quick-card" data-testid="btn-launcher-create">
            <div class="fe-quick-card-head">
              <span class="fe-quick-card-title">Quick Starter Showcase</span>
              <span class="fe-tag fe-tag-cyan">Instant</span>
            </div>
            <span class="fe-quick-card-desc">
              Initialize the C++20 Starter Showcase rig (HeroCube, AccentPyramid, MainCamera, SunLight) and enter 3D Studio.
            </span>
          </button>

          <button type="button" class="fe-quick-card" data-testid="btn-quick-configure-new">
            <div class="fe-quick-card-head">
              <span class="fe-quick-card-title">Configure New Project</span>
              <span class="fe-tag">Validated</span>
            </div>
            <span class="fe-quick-card-desc">
              Set project name, save path, scene units, frame rate, and overwrite protection before initializing.
            </span>
          </button>

          <button type="button" class="fe-quick-card" data-testid="btn-quick-browse-templates">
            <div class="fe-quick-card-head">
              <span class="fe-quick-card-title">Explore 4 C++ Templates</span>
              <span class="fe-tag">Presets</span>
            </div>
            <span class="fe-quick-card-desc">
              Inspect Starter Showcase, Minimal Cube, Empty Modeling Stage, and Shader Ball Studio presets.
            </span>
          </button>
        </div>

        <section class="fe-panel" aria-label="Recent Projects">
          <div class="fe-panel-head">
            <div style="display: flex; align-items: center; gap: 8px;">
              <h2 class="fe-panel-title">Recent Projects</h2>
              <span class="fe-tag" data-testid="recent-count-tag">${totalCount} recorded</span>
            </div>
            <div class="fe-toolbar-right">
              ${
                totalCount > 0
                  ? `<button type="button" class="fe-btn fe-btn-sm fe-btn-ghost" data-testid="btn-clear-unpinned">
                      Clear Unpinned
                    </button>`
                  : ""
              }
            </div>
          </div>

          <div class="fe-toolbar" style="margin-bottom: 14px;">
            <div class="fe-toolbar-left">
              <input
                type="search"
                class="fe-input fe-search-input"
                data-testid="recent-search-input"
                placeholder="Search by project name, file path, scene, or author…"
                value="${escapeHtml(uiState.search)}"
                autocomplete="off"
              />
              <select class="fe-select" data-testid="recent-filter-select" aria-label="Filter recent projects">
                <option value="all" ${uiState.filter === "all" ? "selected" : ""}>All Projects</option>
                <option value="pinned" ${uiState.filter === "pinned" ? "selected" : ""}>Pinned Only</option>
                <option value="available" ${uiState.filter === "available" ? "selected" : ""}>Available Cache</option>
              </select>
            </div>

            <div class="fe-toolbar-right">
              <select class="fe-select" data-testid="recent-sort-select" aria-label="Sort recent projects">
                <option value="recent" ${uiState.sort === "recent" ? "selected" : ""}>Sort: Last Opened</option>
                <option value="name" ${uiState.sort === "name" ? "selected" : ""}>Sort: Name (A–Z)</option>
                <option value="size" ${uiState.sort === "size" ? "selected" : ""}>Sort: File Size</option>
              </select>
              <button
                type="button"
                class="fe-btn fe-btn-sm"
                data-testid="btn-toggle-recent-layout"
                title="Switch between list and card grid"
              >
                ${uiState.layout === "list" ? "Grid View" : "List View"}
              </button>
            </div>
          </div>

          <div
            class="${uiState.layout === "grid" ? "fe-recent-grid" : "fe-recent-list"}"
            id="recent-projects-list"
            data-testid="recent-projects-list"
          >
            ${renderRecentItemsHtml(items, totalCount)}
          </div>
        </section>
      </div>
    `;

    bindEvents(mountEl, items);
  }

  function renderRecentItemsHtml(items, totalCount) {
    if (totalCount === 0) {
      return `
        <div class="fe-empty-state" data-testid="recent-empty-state" style="grid-column: 1 / -1;">
          <h3 class="fe-empty-title">No Recent Projects Yet</h3>
          <p class="fe-empty-desc">
            Projects you create, open, or save in Final Effect 3D appear here with their real <code>.fe3d</code> path, scene telemetry, and captured viewport preview when available.
          </p>
          <div style="display: flex; gap: 8px; margin-top: 4px;">
            <button type="button" class="fe-btn fe-btn-primary" data-testid="btn-empty-new-project">Create New Project</button>
            <button type="button" class="fe-btn" data-testid="btn-empty-open-project">Open .fe3d File…</button>
          </div>
        </div>
      `;
    }

    if (items.length === 0) {
      return `
        <div class="fe-empty-state" data-testid="recent-no-matches" style="grid-column: 1 / -1;">
          <h3 class="fe-empty-title">No Matching Projects</h3>
          <p class="fe-empty-desc">No recorded projects matched your current search or filter criteria.</p>
        </div>
      `;
    }

    return items
      .map((item) => {
        const thumbMarkup = item.thumbnailDataUrl
          ? `<img src="${escapeHtml(item.thumbnailDataUrl)}" alt="${escapeHtml(item.name)} viewport preview" />`
          : `<span>.FE3D</span><span style="opacity:0.65;">NO PREVIEW</span>`;
        const statusBadge = item.available
          ? `<span class="fe-tag fe-tag-emerald">Cached</span>`
          : `<span class="fe-tag fe-tag-amber" title="Browser payload cache missing; locate file on disk">Disk Only</span>`;
        const pinBadge = item.pinned ? `<span class="fe-tag fe-tag-cyan">Pinned</span>` : "";

        return `
          <article class="fe-recent-card" data-testid="recent-project-item" data-project-id="${escapeHtml(item.id)}">
            <div class="fe-recent-main">
              <div class="fe-recent-thumb">${thumbMarkup}</div>
              <div class="fe-recent-meta">
                <div class="fe-recent-title-row">
                  <span class="fe-recent-title">${escapeHtml(item.name)}</span>
                  ${pinBadge}
                  ${statusBadge}
                </div>
                <div class="fe-recent-path" title="${escapeHtml(item.filePath)}">${escapeHtml(item.filePath)}</div>
                <div class="fe-recent-stats">
                  ${escapeHtml(item.formatLabel)} &middot; ${item.objectCount} obj &middot; ${item.materialCount} mat &middot; ${formatByteSize(item.byteSize)} &middot; ${formatRelativeTime(item.updatedAt)}
                </div>
              </div>
            </div>
            <div class="fe-recent-actions">
              <button type="button" class="fe-btn fe-btn-sm fe-btn-primary" data-action="reopen" data-id="${escapeHtml(item.id)}" data-testid="btn-reopen-recent">Open</button>
              <button type="button" class="fe-btn fe-btn-sm" data-action="pin" data-id="${escapeHtml(item.id)}" data-testid="btn-pin-recent" title="${item.pinned ? "Unpin project" : "Pin project to top"}">
                ${item.pinned ? "Unpin" : "Pin"}
              </button>
              <button type="button" class="fe-btn fe-btn-sm fe-btn-ghost" data-action="remove" data-id="${escapeHtml(item.id)}" data-testid="btn-remove-recent" title="Remove from Recent Projects list (does not delete files on disk)">
                Remove
              </button>
            </div>
          </article>
        `;
      })
      .join("");
  }

  function bindEvents(mountEl, items) {
    const bindClick = (sel, fn) => {
      const el = mountEl.querySelector(sel);
      if (el) el.addEventListener("click", fn);
    };

    bindClick('[data-testid="btn-projects-go-new"]', () => ctx.navigate("new_project"));
    bindClick('[data-testid="btn-quick-configure-new"]', () => ctx.navigate("new_project"));
    bindClick('[data-testid="btn-empty-new-project"]', () => ctx.navigate("new_project"));
    bindClick('[data-testid="btn-quick-browse-templates"]', () => ctx.navigate("templates"));
    bindClick('[data-testid="btn-projects-open-dialog"]', () => ctx.triggerOpenFlow());
    bindClick('[data-testid="btn-empty-open-project"]', () => ctx.triggerOpenFlow());
    bindClick('[data-testid="btn-projects-resume-session"]', () => ctx.resumeStudio());

    bindClick('[data-testid="btn-launcher-create"]', async () => {
      await ctx.quickCreateFromTemplate("starter");
    });

    bindClick('[data-testid="btn-clear-unpinned"]', async () => {
      await ctx.recents.clearUnpinned();
      render(mountEl);
    });

    bindClick('[data-testid="btn-toggle-recent-layout"]', () => {
      uiState.layout = uiState.layout === "list" ? "grid" : "list";
      render(mountEl);
    });

    const searchInput = mountEl.querySelector('[data-testid="recent-search-input"]');
    if (searchInput) {
      searchInput.addEventListener("input", () => {
        uiState.search = searchInput.value;
        const pos = searchInput.selectionStart ?? searchInput.value.length;
        render(mountEl);
        const nextInput = mountEl.querySelector('[data-testid="recent-search-input"]');
        if (nextInput) {
          nextInput.focus();
          try {
            nextInput.setSelectionRange(pos, pos);
          } catch (_) {}
        }
      });
    }

    const filterSel = mountEl.querySelector('[data-testid="recent-filter-select"]');
    if (filterSel) {
      filterSel.addEventListener("change", () => {
        uiState.filter = filterSel.value;
        render(mountEl);
      });
    }

    const sortSel = mountEl.querySelector('[data-testid="recent-sort-select"]');
    if (sortSel) {
      sortSel.addEventListener("change", () => {
        uiState.sort = sortSel.value;
        render(mountEl);
      });
    }

    mountEl.querySelectorAll("[data-action]").forEach((btn) => {
      btn.addEventListener("click", async () => {
        const action = btn.getAttribute("data-action");
        const id = btn.getAttribute("data-id");
        const entry = items.find((e) => e.id === id) || ctx.recents.findById(id);
        if (!entry) return;

        if (action === "reopen") {
          await ctx.reopenEntryWithGuard(entry);
        } else if (action === "pin") {
          ctx.recents.togglePin(entry.id);
          render(mountEl);
        } else if (action === "remove") {
          await ctx.recents.removeProject(entry.id);
          render(mountEl);
        }
      });
    });
  }

  return { render };
}
