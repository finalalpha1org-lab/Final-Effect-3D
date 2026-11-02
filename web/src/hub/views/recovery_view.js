// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 View 5: Recovery & Autosave
// Explicitly distinguishes Unavailable storage capability from an Empty recovery list.
// Zero fabricated snapshots or fake success states.

import {
  escapeHtml,
  formatByteSize,
  formatRelativeTime,
} from "../../ui/components.js";

export function createRecoveryView(ctx) {
  let bannerMsg = "";
  let bannerTone = "error";

  function render(mountEl) {
    const status = ctx.recovery.getStatus();
    const prefs = ctx.preferences.get();
    const session = ctx.projectOps.getActiveSessionSummary();

    mountEl.innerHTML = `
      <div class="fe-hub-container" data-testid="hub-view-recovery">
        <div class="fe-view-header">
          <div>
            <div class="fe-view-eyebrow">CRASH PROTECTION &amp; RING BUFFER</div>
            <h1 class="fe-view-title">Recovery &amp; Autosave</h1>
            <p class="fe-view-subtitle">
              Real <code>.fe3d</code> recovery snapshots captured in a bounded ${status.maxRingSlots}-slot IndexedDB ring buffer when unsaved Studio edits occur.
            </p>
          </div>
          <div class="fe-view-actions">
            ${
              status.storageState === "available" && session.ready
                ? `<button type="button" class="fe-btn" data-testid="btn-capture-manual-snapshot">
                    Capture Current Session Snapshot
                  </button>`
                : ""
            }
            ${
              status.storageState === "available" && status.snapshots.length > 0
                ? `<button type="button" class="fe-btn fe-btn-danger" data-testid="btn-discard-all-snapshots">
                    Discard All Snapshots
                  </button>`
                : ""
            }
          </div>
        </div>

        <div
          class="fe-banner ${bannerTone === "success" ? "fe-banner-success" : "fe-banner-error"}"
          data-testid="recovery-action-banner"
          ${bannerMsg ? "" : "hidden"}
        >
          <span>${escapeHtml(bannerMsg)}</span>
        </div>

        ${
          status.hadUncleanPreviousSession && status.snapshots.length > 0
            ? `<div class="fe-banner fe-banner-warning" data-testid="recovery-unclean-banner">
                <span>
                  Previous session exited without a clean shutdown marker. ${status.snapshots.length} genuine recovery snapshot(s) can be restored below.
                </span>
              </div>`
            : ""
        }

        <section class="fe-panel" aria-label="Autosave Status">
          <div class="fe-panel-head">
            <h2 class="fe-panel-title">Autosave Subsystem Status</h2>
            <span class="fe-tag ${
              status.storageState === "available" ? "fe-tag-emerald" : "fe-tag-amber"
            }" data-testid="recovery-storage-badge">
              ${status.storageState === "available" ? "Storage Available" : "Storage Unavailable"}
            </span>
          </div>
          <div class="fe-kv-grid">
            <div class="fe-kv-row">
              <span class="fe-kv-key">Periodic Autosave</span>
              <span class="fe-kv-val">${prefs.autosaveEnabled ? "Enabled" : "Disabled in Preferences"}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Autosave Interval</span>
              <span class="fe-kv-val">${prefs.autosaveIntervalSeconds}s (dirty-session gated)</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Ring Buffer Occupancy</span>
              <span class="fe-kv-val">${status.snapshots.length} / ${status.maxRingSlots} slots</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Previous Session Exit</span>
              <span class="fe-kv-val">${
                status.hadUncleanPreviousSession ? "Unclean termination detected" : "Clean"
              }</span>
            </div>
          </div>
        </section>

        ${renderRecoveryBody(status)}
      </div>
    `;

    bindRecoveryEvents(mountEl);
  }

  function renderRecoveryBody(status) {
    if (status.storageState !== "available") {
      return `
        <div class="fe-empty-state" data-testid="recovery-unavailable-state">
          <span class="fe-tag fe-tag-amber">CAPABILITY UNAVAILABLE</span>
          <h3 class="fe-empty-title">Persistent Recovery Storage Is Unavailable</h3>
          <p class="fe-empty-desc">
            ${escapeHtml(
              status.unavailableReason ||
                "Browser IndexedDB storage is disabled or restricted in this session."
            )}
            Use explicit File &rarr; Save Scene (<code>.fe3d</code>) inside 3D Studio to export work directly to disk.
          </p>
        </div>
      `;
    }

    if (status.snapshots.length === 0) {
      return `
        <div class="fe-empty-state" data-testid="recovery-empty-state">
          <span class="fe-tag fe-tag-emerald">SUBSYSTEM READY &middot; 0 SNAPSHOTS</span>
          <h3 class="fe-empty-title">No Recoverable Snapshots Recorded</h3>
          <p class="fe-empty-desc">
            Recovery storage is active and healthy, and there are currently zero unsaved crash or autosave snapshots waiting for recovery.
          </p>
        </div>
      `;
    }

    return `
      <section class="fe-panel" aria-label="Recoverable Snapshots">
        <div class="fe-panel-head">
          <h2 class="fe-panel-title">Recoverable .fe3d Snapshots</h2>
          <span class="fe-tag">${status.snapshots.length} available</span>
        </div>
        <div class="fe-recent-list" data-testid="recovery-snapshots-list">
          ${status.snapshots
            .map(
              (snap) => `
              <article class="fe-recent-card" data-testid="recovery-snapshot-item" data-snapshot-id="${escapeHtml(
                snap.id
              )}">
                <div class="fe-recent-main">
                  <div class="fe-recent-meta">
                    <div class="fe-recent-title-row">
                      <span class="fe-recent-title">${escapeHtml(snap.projectName)}</span>
                      <span class="fe-tag">Slot ${snap.slotIndex + 1}</span>
                      ${
                        snap.uncleanSession
                          ? `<span class="fe-tag fe-tag-amber">Unclean Session</span>`
                          : `<span class="fe-tag fe-tag-cyan">${escapeHtml(snap.reason)}</span>`
                      }
                    </div>
                    <div class="fe-recent-path">${escapeHtml(
                      snap.originPath || "(Unsaved workspace document)"
                    )}</div>
                    <div class="fe-recent-stats">
                      ${snap.objectCount} obj &middot; ${formatByteSize(
                snap.byteSize
              )} &middot; Captured ${formatRelativeTime(snap.capturedAt)}
                    </div>
                  </div>
                </div>
                <div class="fe-recent-actions">
                  <button
                    type="button"
                    class="fe-btn fe-btn-sm fe-btn-primary"
                    data-recover-id="${escapeHtml(snap.id)}"
                    data-testid="btn-recover-snapshot"
                  >
                    Recover into Studio
                  </button>
                  <button
                    type="button"
                    class="fe-btn fe-btn-sm"
                    data-export-snap-id="${escapeHtml(snap.id)}"
                    data-testid="btn-download-snapshot"
                  >
                    Download .fe3d
                  </button>
                  <button
                    type="button"
                    class="fe-btn fe-btn-sm fe-btn-ghost"
                    data-discard-id="${escapeHtml(snap.id)}"
                    data-testid="btn-discard-snapshot"
                  >
                    Discard
                  </button>
                </div>
              </article>
            `
            )
            .join("")}
        </div>
      </section>
    `;
  }

  function bindRecoveryEvents(mountEl) {
    const captureBtn = mountEl.querySelector('[data-testid="btn-capture-manual-snapshot"]');
    if (captureBtn) {
      captureBtn.addEventListener("click", async () => {
        const res = await ctx.captureActiveSessionSnapshot("Manual Recovery Checkpoint");
        if (res.ok) {
          bannerTone = "success";
          bannerMsg = `Captured recovery snapshot for '${res.snapshot.projectName}' (Slot ${
            res.snapshot.slotIndex + 1
          }).`;
        } else {
          bannerTone = "error";
          bannerMsg = res.error || "Could not capture snapshot.";
        }
        render(mountEl);
      });
    }

    const discardAllBtn = mountEl.querySelector('[data-testid="btn-discard-all-snapshots"]');
    if (discardAllBtn) {
      discardAllBtn.addEventListener("click", async () => {
        const confirmed = await ctx.confirmDialog.confirm({
          title: "Discard All Recovery Snapshots?",
          message:
            "This permanently removes all autosaved recovery checkpoints from browser storage.",
          confirmLabel: "Discard All",
          cancelLabel: "Cancel",
          tone: "danger",
          testIdPrefix: "discard-recovery-dialog",
        });
        if (!confirmed) return;
        await ctx.recovery.discardAll();
        bannerMsg = "";
        render(mountEl);
      });
    }

    mountEl.querySelectorAll("[data-recover-id]").forEach((btn) => {
      btn.addEventListener("click", async () => {
        const id = btn.getAttribute("data-recover-id");
        const allowed = await ctx.confirmReplaceDirtySessionIfNeeded();
        if (!allowed) return;
        const res = await ctx.projectOps.recoverAutosaveSnapshot(id);
        if (!res.ok) {
          bannerTone = "error";
          bannerMsg = res.error || "Failed to recover snapshot.";
          render(mountEl);
        }
      });
    });

    mountEl.querySelectorAll("[data-export-snap-id]").forEach((btn) => {
      btn.addEventListener("click", async () => {
        const id = btn.getAttribute("data-export-snap-id");
        const rec = await ctx.recovery.getSnapshotPayload(id);
        if (!rec || !rec.jsonText) return;
        const blob = new Blob([rec.jsonText], { type: "application/json" });
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = `${rec.projectName.replace(/[^a-zA-Z0-9._-]+/g, "_")}_recovered.fe3d`;
        document.body.appendChild(a);
        a.click();
        a.remove();
        setTimeout(() => URL.revokeObjectURL(url), 500);
      });
    });

    mountEl.querySelectorAll("[data-discard-id]").forEach((btn) => {
      btn.addEventListener("click", async () => {
        const id = btn.getAttribute("data-discard-id");
        await ctx.recovery.discardSnapshot(id);
        bannerMsg = "";
        render(mountEl);
      });
    });
  }

  return { render };
}
