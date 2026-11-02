// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 Shared UI Components, Icons & Confirmation Dialog

export function escapeHtml(str) {
  return String(str ?? "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

export function formatByteSize(bytes) {
  const n = Number(bytes);
  if (!Number.isFinite(n) || n <= 0) return "—";
  if (n < 1024) return `${Math.round(n)} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(2)} MB`;
}

export function formatRelativeTime(epochMs, nowMs = Date.now()) {
  const ts = Number(epochMs);
  if (!Number.isFinite(ts) || ts <= 0) return "Unknown time";
  const diffSec = Math.max(0, Math.round((nowMs - ts) / 1000));
  if (diffSec < 45) return "Just now";
  if (diffSec < 3600) return `${Math.floor(diffSec / 60)}m ago`;
  if (diffSec < 86400) return `${Math.floor(diffSec / 3600)}h ago`;
  return new Date(ts).toLocaleDateString(undefined, {
    year: "numeric",
    month: "short",
    day: "numeric",
  });
}

export const HUB_ICONS = Object.freeze({
  logo: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><polygon points="12 2 21 7 21 17 12 22 3 17 3 7"/><polyline points="3 7 12 12 21 7"/><line x1="12" y1="22" x2="12" y2="12"/></svg>`,
  projects: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><rect x="3" y="3" width="8" height="8" rx="1.5"/><rect x="13" y="3" width="8" height="8" rx="1.5"/><rect x="3" y="13" width="8" height="8" rx="1.5"/><rect x="13" y="13" width="8" height="8" rx="1.5"/></svg>`,
  templates: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><polygon points="12 3 20 7.5 20 16.5 12 21 4 16.5 4 7.5"/><line x1="12" y1="12" x2="20" y2="7.5"/><line x1="12" y1="12" x2="12" y2="21"/><line x1="12" y1="12" x2="4" y2="7.5"/></svg>`,
  plus: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>`,
  folder: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V7z"/></svg>`,
  recovery: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><polyline points="1 4 1 10 7 10"/><path d="M3.51 15a9 9 0 1 0 2.13-9.36L1 10"/><polyline points="12 7 12 12 15 14"/></svg>`,
  settings: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09a1.65 1.65 0 0 0-1-1.51 1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09a1.65 1.65 0 0 0 1.51-1 1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33h.08a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82v.08a1.65 1.65 0 0 0 1.51 1H21a2 2 0 1 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>`,
  info: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><circle cx="12" cy="12" r="9"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/></svg>`,
  search: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><circle cx="11" cy="11" r="7"/><line x1="20" y1="20" x2="16.2" y2="16.2"/></svg>`,
  pin: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><path d="M12 17v5"/><path d="M8 3h8l-1.5 6 3.5 3v2H6v-2l3.5-3L8 3z"/></svg>`,
});

export function renderTemplateWireframe(diagramKind) {
  switch (diagramKind) {
    case "cube":
      return `<svg viewBox="0 0 160 84" fill="none" stroke="currentColor" stroke-width="1.5">
        <line x1="18" y1="68" x2="142" y2="68" stroke="#2c3340"/>
        <polygon points="80,14 114,30 114,60 80,74 46,60 46,30" fill="rgba(39,182,217,0.10)"/>
        <polyline points="46,30 80,44 114,30"/>
        <line x1="80" y1="44" x2="80" y2="74"/>
      </svg>`;
    case "stage":
      return `<svg viewBox="0 0 160 84" fill="none" stroke="currentColor" stroke-width="1.3">
        <polygon points="24,58 80,36 136,58 80,76" stroke="#3a4354" fill="rgba(39,182,217,0.05)"/>
        <line x1="52" y1="47" x2="108" y2="67" stroke="#2c3340"/>
        <line x1="108" y1="47" x2="52" y2="67" stroke="#2c3340"/>
        <line x1="80" y1="56" x2="80" y2="20" stroke="#27b6d9" stroke-width="1.7"/>
        <circle cx="80" cy="56" r="2.5" fill="#f29d38" stroke="none"/>
      </svg>`;
    case "sphere":
      return `<svg viewBox="0 0 160 84" fill="none" stroke="currentColor" stroke-width="1.4">
        <circle cx="80" cy="42" r="27" fill="rgba(39,182,217,0.10)"/>
        <ellipse cx="80" cy="42" rx="27" ry="10" stroke="rgba(39,182,217,0.65)"/>
        <ellipse cx="80" cy="42" rx="11" ry="27" stroke="rgba(39,182,217,0.65)"/>
        <circle cx="71" cy="32" r="3" fill="#f29d38" stroke="none"/>
      </svg>`;
    case "showcase":
    default:
      return `<svg viewBox="0 0 160 84" fill="none" stroke="currentColor" stroke-width="1.4">
        <line x1="16" y1="68" x2="144" y2="68" stroke="#2c3340"/>
        <polygon points="62,22 88,34 88,58 62,68 36,58 36,34" fill="rgba(39,182,217,0.10)"/>
        <polyline points="36,34 62,45 88,34"/>
        <line x1="62" y1="45" x2="62" y2="68"/>
        <polygon points="114,24 134,62 94,62" stroke="#f29d38" fill="rgba(242,157,56,0.10)"/>
        <line x1="114" y1="24" x2="114" y2="62" stroke="#f29d38"/>
      </svg>`;
  }
}

export class ConfirmDialogController {
  constructor(mountEl) {
    this.mountEl = mountEl;
    this.pendingResolve = null;
  }

  confirm({
    title = "Confirm Action",
    message = "",
    detail = "",
    confirmLabel = "Confirm",
    cancelLabel = "Cancel",
    tone = "warning",
    testIdPrefix = "confirm-dialog",
  } = {}) {
    if (!this.mountEl) return Promise.resolve(false);
    if (this.pendingResolve) {
      this.pendingResolve(false);
      this.pendingResolve = null;
    }

    const btnClass =
      tone === "danger"
        ? "fe-btn fe-btn-danger"
        : tone === "warning"
        ? "fe-btn fe-btn-amber"
        : "fe-btn fe-btn-primary";

    this.mountEl.innerHTML = `
      <div class="fe-dialog-card" role="alertdialog" aria-modal="true" data-testid="${escapeHtml(testIdPrefix)}">
        <div class="fe-dialog-head">
          <h3 class="fe-dialog-title">${escapeHtml(title)}</h3>
        </div>
        <div class="fe-dialog-body">
          <p style="margin: 0; color: var(--fe-text-primary);">${escapeHtml(message)}</p>
          ${
            detail
              ? `<div style="padding: 8px 10px; border-radius: 6px; background: var(--fe-bg-input); border: 1px solid var(--fe-border-subtle); font-family: var(--fe-font-mono); font-size: 11.5px; color: var(--fe-text-secondary);">${escapeHtml(
                  detail
                )}</div>`
              : ""
          }
        </div>
        <div class="fe-dialog-actions">
          <button type="button" class="fe-btn" data-testid="${escapeHtml(testIdPrefix)}-cancel">${escapeHtml(
      cancelLabel
    )}</button>
          <button type="button" class="${btnClass}" data-testid="${escapeHtml(
      testIdPrefix
    )}-accept">${escapeHtml(confirmLabel)}</button>
        </div>
      </div>
    `;
    this.mountEl.hidden = false;

    return new Promise((resolve) => {
      this.pendingResolve = resolve;
      const finish = (val) => {
        if (!this.pendingResolve) return;
        const r = this.pendingResolve;
        this.pendingResolve = null;
        this.mountEl.hidden = true;
        r(val);
      };

      const cancelBtn = this.mountEl.querySelector(`[data-testid="${testIdPrefix}-cancel"]`);
      const acceptBtn = this.mountEl.querySelector(`[data-testid="${testIdPrefix}-accept"]`);
      if (cancelBtn) cancelBtn.addEventListener("click", () => finish(false));
      if (acceptBtn) {
        acceptBtn.addEventListener("click", () => finish(true));
        acceptBtn.focus();
      }
    });
  }
}
