// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 View 2: Templates (4 Verified C++20 ProjectTemplate Presets)

import { escapeHtml, renderTemplateWireframe } from "../../ui/components.js";

export function createTemplatesView(ctx) {
  let categoryFilter = "All";

  function render(mountEl) {
    const allTemplates = ctx.projectOps.getTemplates();
    const categories = ["All", ...new Set(allTemplates.map((t) => t.category))];
    const visible = allTemplates.filter(
      (t) => categoryFilter === "All" || t.category === categoryFilter
    );

    mountEl.innerHTML = `
      <div class="fe-hub-container" data-testid="hub-view-templates">
        <div class="fe-view-header">
          <div>
            <div class="fe-view-eyebrow">VERIFIED C++20 ENGINE PRESETS</div>
            <h1 class="fe-view-title">Project Templates</h1>
            <p class="fe-view-subtitle">
              Every preset maps directly to a compiled <code>finaleffect::app::ProjectTemplate</code> initializer in C++20 and produces a valid, non-corrupted <code>.fe3d</code> v1.3 scene document.
            </p>
          </div>
          <div class="fe-view-actions">
            <select class="fe-select" data-testid="templates-category-select" aria-label="Filter templates by category">
              ${categories
                .map(
                  (cat) =>
                    `<option value="${escapeHtml(cat)}" ${
                      cat === categoryFilter ? "selected" : ""
                    }>Category: ${escapeHtml(cat)}</option>`
                )
                .join("")}
            </select>
          </div>
        </div>

        <div class="fe-template-grid" data-testid="hub-templates-grid">
          ${visible
            .map(
              (t) => `
              <article class="fe-template-card" data-testid="template-card-${escapeHtml(t.id)}">
                <div style="display: flex; flex-direction: column; gap: 11px;">
                  <div class="fe-template-diagram" aria-hidden="true">
                    ${renderTemplateWireframe(t.diagramKind)}
                  </div>
                  <div style="display: flex; align-items: center; justify-content: space-between; gap: 8px;">
                    <h2 style="margin: 0; font-size: 15px; font-weight: 640;">${escapeHtml(t.title)}</h2>
                    <span class="fe-tag fe-tag-cyan">${escapeHtml(t.category)}</span>
                  </div>
                  <p style="margin: 0; font-size: 12.5px; color: var(--fe-text-secondary);">
                    ${escapeHtml(t.summary)}
                  </p>
                  <div class="fe-template-specs">
                    <span class="fe-tag">${t.objectCount} Objects</span>
                    <span class="fe-tag">${t.meshCount} Editable Meshes</span>
                    <span class="fe-tag">${t.materialCount} PBR Materials</span>
                    <span class="fe-tag">Right-Handed +Y</span>
                  </div>
                </div>

                <div style="display: flex; align-items: center; justify-content: flex-end; gap: 8px; padding-top: 6px; border-top: 1px solid var(--fe-border-subtle);">
                  <button
                    type="button"
                    class="fe-btn fe-btn-sm"
                    data-configure-template="${escapeHtml(t.id)}"
                    data-testid="btn-template-configure-${escapeHtml(t.id)}"
                  >
                    Configure Name &amp; Path…
                  </button>
                  <button
                    type="button"
                    class="fe-btn fe-btn-sm fe-btn-primary"
                    data-quick-template="${escapeHtml(t.id)}"
                    data-testid="btn-template-quick-${escapeHtml(t.id)}"
                  >
                    Create &amp; Enter Studio
                  </button>
                </div>
              </article>
            `
            )
            .join("")}
        </div>
      </div>
    `;

    const catSelect = mountEl.querySelector('[data-testid="templates-category-select"]');
    if (catSelect) {
      catSelect.addEventListener("change", () => {
        categoryFilter = catSelect.value;
        render(mountEl);
      });
    }

    mountEl.querySelectorAll("[data-configure-template]").forEach((btn) => {
      btn.addEventListener("click", () => {
        const tmplId = btn.getAttribute("data-configure-template");
        ctx.navigate("new_project", { templateId: tmplId });
      });
    });

    mountEl.querySelectorAll("[data-quick-template]").forEach((btn) => {
      btn.addEventListener("click", async () => {
        const tmplId = btn.getAttribute("data-quick-template");
        await ctx.quickCreateFromTemplate(tmplId);
      });
    });
  }

  return { render };
}
