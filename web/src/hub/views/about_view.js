// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 View 7: About / System Information
// Displays real C++20 engine version, platform, WebGL2/GPU telemetry & honest capability status.

import { escapeHtml } from "../../ui/components.js";

export function createAboutView(ctx) {
  let copiedFeedback = false;

  function render(mountEl) {
    const info = ctx.projectOps.querySystemCapabilities();

    mountEl.innerHTML = `
      <div class="fe-hub-container" data-testid="hub-view-about">
        <div class="fe-view-header">
          <div>
            <div class="fe-view-eyebrow">DIAGNOSTICS &amp; RUNTIME TELEMETRY</div>
            <h1 class="fe-view-title">About &amp; System Information</h1>
            <p class="fe-view-subtitle">
              Real runtime, graphics device, serializer, and desktop storage capability report queried directly from the C++20 engine and WebGL 2.0 context.
            </p>
          </div>
          <div class="fe-view-actions">
            <button type="button" class="fe-btn fe-btn-primary" data-testid="btn-copy-diagnostics">
              ${copiedFeedback ? "Copied Diagnostics JSON" : "Copy Diagnostic Report"}
            </button>
          </div>
        </div>

        <section class="fe-panel" aria-label="Application Identity">
          <div class="fe-panel-head">
            <h2 class="fe-panel-title">Application &amp; Engine Build</h2>
            <span class="fe-tag fe-tag-cyan" data-testid="about-version-tag">v${escapeHtml(
              info.engineVersion
            )}</span>
          </div>
          <div class="fe-kv-grid">
            <div class="fe-kv-row">
              <span class="fe-kv-key">Application Name</span>
              <span class="fe-kv-val" data-testid="about-app-name">${escapeHtml(info.applicationName)}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">C++20 Core Engine Version</span>
              <span class="fe-kv-val">${escapeHtml(info.engineVersion)}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Active Desktop Runtime</span>
              <span class="fe-kv-val">${escapeHtml(info.hostTarget)}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Native Companion Target</span>
              <span class="fe-kv-val">${escapeHtml(info.nativeTarget)}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Host OS / Platform</span>
              <span class="fe-kv-val">${escapeHtml(info.browserPlatform)}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Logical CPU Threads</span>
              <span class="fe-kv-val">${escapeHtml(info.hardwareConcurrency)}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Coordinate System</span>
              <span class="fe-kv-val">${escapeHtml(info.coordinateSystem)}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Open-Source License</span>
              <span class="fe-kv-val">${escapeHtml(info.license)}</span>
            </div>
          </div>
        </section>

        <section class="fe-panel" aria-label="Graphics & GPU Capabilities">
          <div class="fe-panel-head">
            <h2 class="fe-panel-title">Graphics API &amp; GPU Device</h2>
            <span class="fe-tag fe-tag-emerald">${escapeHtml(info.graphicsBackend)}</span>
          </div>
          <div class="fe-kv-grid">
            <div class="fe-kv-row">
              <span class="fe-kv-key">Graphics API Version</span>
              <span class="fe-kv-val" data-testid="about-gl-version">${escapeHtml(info.glVersion)}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Shading Language</span>
              <span class="fe-kv-val">${escapeHtml(info.glShadingLang)}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">GPU Vendor</span>
              <span class="fe-kv-val">${escapeHtml(info.glVendor)}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">GPU Renderer</span>
              <span class="fe-kv-val" data-testid="about-gl-renderer">${escapeHtml(info.glRenderer)}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Max 2D Texture Dimension</span>
              <span class="fe-kv-val">${escapeHtml(info.maxTextureSize)}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Max Framebuffer Samples</span>
              <span class="fe-kv-val">${escapeHtml(info.maxSamples)}</span>
            </div>
          </div>
        </section>

        <section class="fe-panel" aria-label="File I/O & Persistence Capabilities">
          <div class="fe-panel-head">
            <h2 class="fe-panel-title">Project Format &amp; Persistence Capabilities</h2>
          </div>
          <div class="fe-kv-grid">
            <div class="fe-kv-row">
              <span class="fe-kv-key">Supported Project Formats</span>
              <span class="fe-kv-val">${escapeHtml(info.projectFormat)}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Desktop File Access Mode</span>
              <span class="fe-kv-val" data-testid="about-fs-capability">${escapeHtml(
                info.fileSystemAccess
              )}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Save Folder Selection</span>
              <span class="fe-kv-val">${escapeHtml(info.directoryPickerSupport)}</span>
            </div>
            <div class="fe-kv-row">
              <span class="fe-kv-key">Recovery &amp; Autosave Storage</span>
              <span class="fe-kv-val" data-testid="about-recovery-capability">${escapeHtml(
                info.recoveryStorage
              )}</span>
            </div>
          </div>
        </section>
      </div>
    `;

    const copyBtn = mountEl.querySelector('[data-testid="btn-copy-diagnostics"]');
    if (copyBtn) {
      copyBtn.addEventListener("click", async () => {
        const text = JSON.stringify(info, null, 2);
        try {
          if (navigator.clipboard && typeof navigator.clipboard.writeText === "function") {
            await navigator.clipboard.writeText(text);
          }
        } catch (_) {}
        copiedFeedback = true;
        render(mountEl);
        setTimeout(() => {
          copiedFeedback = false;
        }, 2500);
      });
    }
  }

  return { render };
}
