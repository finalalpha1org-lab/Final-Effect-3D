// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 1 Desktop Home & Project Hub End-to-End Verification Suite
// Runs in real Headless Chromium with Emscripten WebAssembly + WebGL 2.0.

import { test, expect } from "@playwright/test";
import fs from "node:fs/promises";
import path from "node:path";
import os from "node:os";

const SCREENSHOT_DIR = path.resolve(process.cwd(), "../../build/ci-screenshots");

async function bootStage1Home(page) {
  const pageErrors = [];
  page.on("pageerror", (err) => pageErrors.push(err.message));
  await page.setViewportSize({ width: 1920, height: 1080 });
  await page.goto("/index.html");
  await expect(page.getByTestId("splash-screen")).toBeHidden({ timeout: 25_000 });
  await expect(page.getByTestId("home-screen")).toBeVisible();
  await expect(page.getByTestId("home-engine-badge")).toContainText("v0.5.0");
  expect(pageErrors).toEqual([]);
  return pageErrors;
}

test.describe("Final Effect 3D Stage 1 — Desktop Home, Project Hub & Preferences", () => {
  test("1. All 7 Home Hub views render real data, support Command Palette & input focus guards, and capture screenshots", async ({
    page,
  }) => {
    await fs.mkdir(SCREENSHOT_DIR, { recursive: true });
    await bootStage1Home(page);

    // View 1: Projects & Start (genuinely empty on clean profile)
    await expect(page.getByTestId("hub-view-projects")).toBeVisible();
    await expect(page.getByTestId("recent-empty-state")).toBeVisible();
    await page.screenshot({
      path: path.join(SCREENSHOT_DIR, "stage1-01-projects-start.png"),
    });

    // Strict input focus guard: typing Space and Shift+Space inside search input inserts characters cleanly
    const searchInput = page.getByTestId("recent-search-input");
    await searchInput.focus();
    await page.keyboard.type("Hero ");
    await page.keyboard.down("Shift");
    await page.keyboard.press("Space");
    await page.keyboard.up("Shift");
    await page.keyboard.type("Rig");
    await expect(searchInput).toHaveValue("Hero  Rig");
    await searchInput.fill("");

    // View 2: Templates (4 real C++ presets, zero coming-soon placeholders)
    await page.getByTestId("hub-nav-templates").click();
    await expect(page.getByTestId("hub-view-templates")).toBeVisible();
    for (const id of ["starter", "minimal_cube", "empty_stage", "shader_ball"]) {
      await expect(page.getByTestId(`template-card-${id}`)).toBeVisible();
    }
    await page.screenshot({
      path: path.join(SCREENSHOT_DIR, "stage1-02-templates.png"),
    });

    // View 3: New Project
    await page.getByTestId("hub-nav-new_project").click();
    await expect(page.getByTestId("hub-view-new-project")).toBeVisible();
    await page.screenshot({
      path: path.join(SCREENSHOT_DIR, "stage1-03-new-project.png"),
    });

    // View 4: Open Project
    await page.getByTestId("hub-nav-open_project").click();
    await expect(page.getByTestId("hub-view-open-project")).toBeVisible();
    await page.screenshot({
      path: path.join(SCREENSHOT_DIR, "stage1-04-open-project.png"),
    });

    // View 5: Recovery & Autosave (Available & genuinely empty)
    await page.getByTestId("hub-nav-recovery").click();
    await expect(page.getByTestId("hub-view-recovery")).toBeVisible();
    await expect(page.getByTestId("recovery-empty-state")).toBeVisible();
    await page.screenshot({
      path: path.join(SCREENSHOT_DIR, "stage1-05-recovery-autosave.png"),
    });

    // View 6: Preferences
    await page.getByTestId("hub-nav-preferences").click();
    await expect(page.getByTestId("hub-view-preferences")).toBeVisible();
    await page.screenshot({
      path: path.join(SCREENSHOT_DIR, "stage1-06-preferences.png"),
    });

    // View 7: About / System Information (real C++20 + WebGL2 telemetry)
    await page.getByTestId("hub-nav-about").click();
    await expect(page.getByTestId("hub-view-about")).toBeVisible();
    await expect(page.getByTestId("about-app-name")).toHaveText("Final Effect 3D");
    await expect(page.getByTestId("about-version-tag")).toHaveText("v0.5.0");
    await expect(page.getByTestId("about-gl-version")).toContainText("WebGL 2.0");
    await page.screenshot({
      path: path.join(SCREENSHOT_DIR, "stage1-07-about-system-info.png"),
    });

    // Command Palette (Ctrl+K) navigates cleanly back to Projects & Start
    await page.keyboard.press("Control+k");
    await expect(page.getByTestId("command-palette-input")).toBeVisible();
    await page.getByTestId("command-palette-input").fill("Projects & Start");
    await page.keyboard.press("Enter");
    await expect(page.getByTestId("hub-view-projects")).toBeVisible();
  });

  test("2. All 4 C++ templates initialize valid scenes, New Project validates & protects against silent overwrite, and Open Project preserves active session on error", async ({
    page,
  }) => {
    await bootStage1Home(page);

    // 1. Validate empty project name rejection
    await page.getByTestId("hub-nav-new_project").click();
    await page.getByTestId("new-project-name").fill("   ");
    await page.getByTestId("btn-new-project-submit").click();
    await expect(page.getByTestId("new-project-error")).toBeVisible();

    // 2. Create Shader Ball Studio project at /workspace/projects/LookDev_Orb.fe3d
    await page.getByTestId("new-project-name").fill("LookDev Orb");
    await page.getByTestId("new-project-template").selectOption("shader_ball");
    await page.getByTestId("new-project-units").selectOption("centimeters");
    await page.getByTestId("btn-new-project-submit").click();

    await expect(page.getByTestId("studio-shell")).toBeVisible();
    await expect(page.getByTestId("active-project-pill")).toHaveText("LookDev Orb");
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("3 / 1");

    // Return to Home and attempt to create another project at the SAME path -> Overwrite Confirmation!
    await page.getByTestId("btn-go-home").click();
    await page.getByTestId("hub-nav-new_project").click();
    await page.getByTestId("new-project-name").fill("LookDev Orb");
    await page.getByTestId("new-project-template").selectOption("empty_stage");
    await page.getByTestId("btn-new-project-submit").click();

    await expect(page.getByTestId("overwrite-confirm-dialog")).toBeVisible();
    // Cancel overwrite -> stays on New Project and keeps original 3-object Shader Ball scene intact!
    await page.getByTestId("overwrite-confirm-dialog-cancel").click();
    await page.getByTestId("btn-home-resume-studio").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("3 / 1");

    // Return to Home, confirm overwrite -> replaces with 2-object Empty Stage!
    await page.getByTestId("btn-go-home").click();
    await page.getByTestId("hub-nav-new_project").click();
    await page.getByTestId("new-project-name").fill("LookDev Orb");
    await page.getByTestId("new-project-template").selectOption("empty_stage");
    await page.getByTestId("btn-new-project-submit").click();
    await page.getByTestId("overwrite-confirm-dialog-accept").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("2 / 0");

    // Return to Home -> Open Project with unsupported .txt file reports clear error & preserves active Studio session
    await page.getByTestId("btn-go-home").click();
    await page.getByTestId("hub-nav-open_project").click();
    const tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), "fe3d-stage1-open-"));
    const unsupportedFile = path.join(tmpDir, "notes.txt");
    await fs.writeFile(unsupportedFile, "not a project", "utf-8");
    await page.locator("#file-open-input").setInputFiles([unsupportedFile]);
    await expect(page.getByTestId("open-project-error")).toBeVisible();
    await page.getByTestId("btn-home-resume-studio").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("2 / 0");
    await fs.rm(tmpDir, { recursive: true, force: true });
  });

  test("3. Recovery & Autosave captures real .fe3d checkpoints, restores scenes accurately, and distinguishes Unavailable storage from Empty list", async ({
    browser,
  }) => {
    // Part A: Available storage -> capture & recover real .fe3d checkpoint
    const ctxAvailable = await browser.newContext({ viewport: { width: 1920, height: 1080 } });
    const pageA = await ctxAvailable.newPage();
    await bootStage1Home(pageA);

    // Create Minimal Cube (3 objects), add a Sphere in Studio (4 objects), return to Home -> capture Recovery Checkpoint
    await pageA.getByTestId("hub-nav-templates").click();
    await pageA.getByTestId("btn-template-quick-minimal_cube").click();
    await expect(pageA.getByTestId("stat-objects-meshes")).toHaveText("3 / 1");
    await pageA.getByTestId("btn-add-sphere").click();
    await expect(pageA.getByTestId("stat-objects-meshes")).toHaveText("4 / 2");

    await pageA.getByTestId("btn-go-home").click();
    await pageA.getByTestId("hub-nav-recovery").click();
    await pageA.getByTestId("btn-capture-manual-snapshot").click();
    await expect(pageA.getByTestId("recovery-snapshot-item")).toHaveCount(1);

    // Switch to Empty Stage (2 objects), then recover the 4-object checkpoint from Recovery view!
    await pageA.getByTestId("hub-nav-templates").click();
    await pageA.getByTestId("btn-template-quick-empty_stage").click();
    await pageA.getByTestId("dirty-session-dialog-accept").click();
    await expect(pageA.getByTestId("stat-objects-meshes")).toHaveText("2 / 0");

    await pageA.getByTestId("btn-go-home").click();
    await pageA.getByTestId("hub-nav-recovery").click();
    await pageA.getByTestId("btn-recover-snapshot").first().click();
    await expect(pageA.getByTestId("studio-shell")).toBeVisible();
    await expect(pageA.getByTestId("stat-objects-meshes")).toHaveText("4 / 2");
    await ctxAvailable.close();

    // Part B: Restricted/Unavailable storage -> explicitly displays Unavailable capability panel, NOT Empty list
    const ctxUnavailable = await browser.newContext({ viewport: { width: 1920, height: 1080 } });
    await ctxUnavailable.addInitScript(() => {
      window.__FE3D_FORCE_RECOVERY_UNAVAILABLE__ = true;
    });
    const pageB = await ctxUnavailable.newPage();
    await bootStage1Home(pageB);
    await pageB.getByTestId("hub-nav-recovery").click();
    await expect(pageB.getByTestId("recovery-unavailable-state")).toBeVisible();
    await expect(pageB.getByTestId("recovery-empty-state")).toHaveCount(0);
    await ctxUnavailable.close();
  });

  test("4. Application Preferences persist across browser reloads and remain isolated from .fe3d project loads", async ({
    page,
  }) => {
    await bootStage1Home(page);

    await page.getByTestId("hub-nav-preferences").click();
    await page.getByTestId("pref-ui-scale-select").selectOption("115");
    await page.getByTestId("pref-nav-profile-select").selectOption("blender");
    await page.getByTestId("pref-author-input").fill("Lead LookDev TD");
    await page.getByTestId("pref-author-input").dispatchEvent("change");
    await expect(page.locator("body")).toHaveAttribute("data-fe-ui-scale", "115");
    await expect(page.locator("body")).toHaveAttribute("data-fe-nav-profile", "blender");

    // Reload page -> verify preferences persist across restarts
    await page.reload();
    await expect(page.getByTestId("splash-screen")).toBeHidden({ timeout: 25_000 });
    await expect(page.locator("body")).toHaveAttribute("data-fe-ui-scale", "115");
    await expect(page.locator("body")).toHaveAttribute("data-fe-nav-profile", "blender");

    // Load a different template project -> verify preferences remain untouched
    await page.getByTestId("hub-nav-templates").click();
    await page.getByTestId("btn-template-quick-shader_ball").click();
    await expect(page.getByTestId("studio-shell")).toBeVisible();
    await page.getByTestId("btn-go-home").click();
    await page.getByTestId("hub-nav-preferences").click();
    await expect(page.getByTestId("pref-ui-scale-select")).toHaveValue("115");
    await expect(page.getByTestId("pref-nav-profile-select")).toHaveValue("blender");
    await expect(page.getByTestId("pref-author-input")).toHaveValue("Lead LookDev TD");
  });

  test("5. Multi-resolution (1920x1080, 2560x1440, 1366x768) layout, clipping, overflow, WCAG readability, navigation & UI scaling audit", async ({
    page,
  }) => {
    await bootStage1Home(page);

    const viewPairs = [
      ["projects", "hub-view-projects"],
      ["templates", "hub-view-templates"],
      ["new_project", "hub-view-new-project"],
      ["open_project", "hub-view-open-project"],
      ["recovery", "hub-view-recovery"],
      ["preferences", "hub-view-preferences"],
      ["about", "hub-view-about"],
    ];

    const resolutions = [
      { width: 1920, height: 1080, label: "1920x1080" },
      { width: 2560, height: 1440, label: "2560x1440" },
      { width: 1366, height: 768, label: "1366x768" },
    ];

    // Verify WCAG AA contrast ratios (>= 4.5:1) of theme text tokens against dark slate surfaces
    const contrastAudit = await page.evaluate(() => {
      function parseHexOrRgb(str) {
        const s = str.trim();
        if (s.startsWith("#")) {
          const hex = s.slice(1);
          return [
            parseInt(hex.slice(0, 2), 16),
            parseInt(hex.slice(2, 4), 16),
            parseInt(hex.slice(4, 6), 16),
          ];
        }
        const m = s.match(/(\d+)\s*,\s*(\d+)\s*,\s*(\d+)/);
        return m ? [Number(m[1]), Number(m[2]), Number(m[3])] : [0, 0, 0];
      }
      function relLum([r, g, b]) {
        const lin = [r, g, b].map((v) => {
          const c = v / 255;
          return c <= 0.03928 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
        });
        return 0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2];
      }
      function ratio(fg, bg) {
        const l1 = relLum(parseHexOrRgb(fg));
        const l2 = relLum(parseHexOrRgb(bg));
        return (Math.max(l1, l2) + 0.05) / (Math.min(l1, l2) + 0.05);
      }
      const cs = getComputedStyle(document.documentElement);
      const bgElevated = cs.getPropertyValue("--fe-bg-elevated");
      const bgPanel = cs.getPropertyValue("--fe-bg-panel");
      return {
        primaryOnPanel: ratio(cs.getPropertyValue("--fe-text-primary"), bgPanel),
        secondaryOnElevated: ratio(cs.getPropertyValue("--fe-text-secondary"), bgElevated),
        mutedOnElevated: ratio(cs.getPropertyValue("--fe-text-muted"), bgElevated),
      };
    });
    expect(contrastAudit.primaryOnPanel).toBeGreaterThanOrEqual(12.0);
    expect(contrastAudit.secondaryOnElevated).toBeGreaterThanOrEqual(7.0);
    expect(contrastAudit.mutedOnElevated).toBeGreaterThanOrEqual(4.5);

    for (const res of resolutions) {
      await page.setViewportSize({ width: res.width, height: res.height });
      for (const [navId, viewTestId] of viewPairs) {
        await page.getByTestId(`hub-nav-${navId}`).click();
        await expect(page.getByTestId(viewTestId)).toBeVisible();

        const audit = await page.evaluate((vpW) => {
          const issues = [];
          const docOverflow = document.documentElement.scrollWidth - window.innerWidth;
          if (docOverflow > 1) issues.push(`docOverflow=${docOverflow}`);

          const checkEls = document.querySelectorAll(
            ".fe-hub-shell, .fe-hub-topbar, .fe-hub-stage, .fe-panel, .fe-kv-row, .fe-pref-row"
          );
          for (const el of checkEls) {
            if (el.offsetParent === null && !el.classList.contains("fe-hub-shell")) continue;
            if (el.scrollWidth > el.clientWidth + 2) {
              issues.push(`horizOverflow:${el.className}:${el.scrollWidth}>${el.clientWidth}`);
            }
          }

          const controls = document.querySelectorAll(
            ".fe-hub-shell .fe-btn, .fe-hub-shell .fe-input, .fe-hub-shell .fe-select, .fe-hub-shell .fe-hub-nav-btn"
          );
          for (const ctrl of controls) {
            if (ctrl.hidden || ctrl.offsetParent === null) continue;
            const r = ctrl.getBoundingClientRect();
            const fsPx = parseFloat(getComputedStyle(ctrl).fontSize || "12");
            if (r.width < 20 || r.height < 20) {
              issues.push(`tinyControl:${ctrl.dataset.testid || ctrl.className}:${r.width}x${r.height}`);
            }
            if (r.left < -1 || r.right > vpW + 1) {
              issues.push(`clippedHoriz:${ctrl.dataset.testid || ctrl.className}:${r.left}..${r.right}`);
            }
            if (fsPx < 9.5) {
              issues.push(`unreadableFont:${ctrl.dataset.testid || ctrl.className}:${fsPx}px`);
            }
          }
          return issues;
        }, res.width);

        expect(audit, `Layout audit at ${res.label} on view ${navId}`).toEqual([]);
      }
    }

    // Verify proportional UI scaling (85%, 100%, 115%, 125%) at 1366x768 (tightest desktop resolution)
    await page.setViewportSize({ width: 1366, height: 768 });
    const measuredTopbarHeights = [];
    const measuredShellFonts = [];
    for (const scale of ["85", "100", "115", "125"]) {
      await page.getByTestId("hub-nav-preferences").click();
      await page.getByTestId("pref-ui-scale-select").selectOption(scale);
      const metrics = await page.evaluate(() => {
        const shell = document.querySelector(".fe-hub-shell");
        const topbar = document.querySelector(".fe-hub-topbar");
        return {
          fontPx: parseFloat(getComputedStyle(shell).fontSize),
          topbarH: topbar.getBoundingClientRect().height,
          docOverflow: document.documentElement.scrollWidth - window.innerWidth,
        };
      });
      expect(metrics.docOverflow).toBeLessThanOrEqual(1);
      measuredShellFonts.push(metrics.fontPx);
      measuredTopbarHeights.push(metrics.topbarH);

      // Also check About view (long GPU/WebGL strings) at 1366x768 under this scale
      await page.getByTestId("hub-nav-about").click();
      const aboutOverflow = await page.evaluate(() => {
        let maxDiff = 0;
        for (const row of document.querySelectorAll(".fe-kv-row")) {
          maxDiff = Math.max(maxDiff, row.scrollWidth - row.clientWidth);
        }
        return maxDiff;
      });
      expect(aboutOverflow, `About kv-row overflow at scale ${scale}% on 1366x768`).toBeLessThanOrEqual(2);
    }

    expect(measuredShellFonts[0]).toBeLessThan(measuredShellFonts[1]);
    expect(measuredShellFonts[1]).toBeLessThan(measuredShellFonts[2]);
    expect(measuredShellFonts[2]).toBeLessThan(measuredShellFonts[3]);
    expect(measuredTopbarHeights[1]).toBeLessThan(measuredTopbarHeights[2]);
    expect(measuredTopbarHeights[2]).toBeLessThan(measuredTopbarHeights[3]);
  });

  test("6. Desktop Web file picker fallbacks (non-FSA browser + AbortError) and total browser storage lock resilience", async ({
    browser,
  }) => {
    // Part A: Simulate Firefox/Safari Desktop (no showOpenFilePicker / showSaveFilePicker / showDirectoryPicker)
    const ctxFallback = await browser.newContext({ viewport: { width: 1920, height: 1080 } });
    await ctxFallback.addInitScript(() => {
      delete window.showOpenFilePicker;
      delete window.showSaveFilePicker;
      delete window.showDirectoryPicker;
    });
    const pageFallback = await ctxFallback.newPage();
    await bootStage1Home(pageFallback);

    await pageFallback.getByTestId("hub-nav-open_project").click();
    await expect(pageFallback.getByTestId("hub-view-open-project")).toContainText(
      "Standard Browser File Input Fallback Active"
    );

    await pageFallback.getByTestId("hub-nav-new_project").click();
    await expect(pageFallback.getByTestId("hub-view-new-project")).toContainText(
      "Workspace Path + Download Portable .fe3d"
    );

    // Export a valid .fe3d from Starter Showcase, then open it via #file-open-input fallback!
    await pageFallback.getByTestId("hub-nav-projects").click();
    await pageFallback.getByTestId("btn-launcher-create").click();
    await expect(pageFallback.getByTestId("studio-shell")).toBeVisible();
    const exportedFe3d = await pageFallback.evaluate(() =>
      window.__FE3D_STUDIO__.exportInlineProject()
    );
    expect(exportedFe3d).toContain('"format"');

    const tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), "fe3d-fallback-open-"));
    const savedFe3dPath = path.join(tmpDir, "FallbackRoundtrip.fe3d");
    await fs.writeFile(savedFe3dPath, exportedFe3d, "utf-8");

    await pageFallback.getByTestId("btn-go-home").click();
    await pageFallback.getByTestId("hub-nav-open_project").click();
    await pageFallback.locator("#file-open-input").setInputFiles([savedFe3dPath]);
    await expect(pageFallback.getByTestId("studio-shell")).toBeVisible();
    await expect(pageFallback.getByTestId("stat-objects-meshes")).toHaveText("5 / 2");
    await fs.rm(tmpDir, { recursive: true, force: true });
    await ctxFallback.close();

    // Part B: Simulate native showOpenFilePicker throwing AbortError (user cancelled dialog) -> zero error banner
    const ctxAbort = await browser.newContext({ viewport: { width: 1920, height: 1080 } });
    await ctxAbort.addInitScript(() => {
      window.showOpenFilePicker = async () => {
        throw new DOMException("The user aborted a request.", "AbortError");
      };
    });
    const pageAbort = await ctxAbort.newPage();
    await bootStage1Home(pageAbort);
    await pageAbort.getByTestId("hub-nav-open_project").click();
    await pageAbort.getByTestId("btn-open-view-browse").click();
    await expect(pageAbort.getByTestId("open-project-error")).toBeHidden();
    await ctxAbort.close();

    // Part C: Simulate locked browser where both localStorage & indexedDB getters throw SecurityError
    const ctxLocked = await browser.newContext({ viewport: { width: 1920, height: 1080 } });
    await ctxLocked.addInitScript(() => {
      Object.defineProperty(window, "localStorage", {
        configurable: true,
        get() {
          throw new DOMException("Storage disabled by policy", "SecurityError");
        },
      });
      Object.defineProperty(window, "indexedDB", {
        configurable: true,
        get() {
          throw new DOMException("IndexedDB disabled by policy", "SecurityError");
        },
      });
    });
    const pageLocked = await ctxLocked.newPage();
    await bootStage1Home(pageLocked);
    await pageLocked.getByTestId("hub-nav-recovery").click();
    await expect(pageLocked.getByTestId("recovery-unavailable-state")).toBeVisible();
    await pageLocked.getByTestId("hub-nav-preferences").click();
    await pageLocked.getByTestId("pref-ui-scale-select").selectOption("115");
    await expect(pageLocked.locator("body")).toHaveAttribute("data-fe-ui-scale", "115");
    await ctxLocked.close();
  });
});
