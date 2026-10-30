// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Stage 5 (v0.5.0) Real-Browser WebAssembly + WebGL 2.0 Multi-Device E2E Suite
// Validates Splash -> Home / 6-Studio Selector -> 3D Studio Project Launcher -> 12 Workspaces ->
// 3D Viewport, Gizmos, Mesh Modeling, PBR Materials, .fe3d/.fe3dbin Serialization, Touch Gestures,
// Mobile Portrait, Mobile Landscape, Tablet, Desktop, and iPhone Safari "Request Desktop Website".

import { test, expect } from "@playwright/test";
import fs from "node:fs/promises";
import path from "node:path";
import os from "node:os";

async function sampleViewportPixels(page) {
  return await page.evaluate(() => {
    const canvas = document.getElementById("fe3d-canvas");
    const gl = canvas.getContext("webgl2");
    if (!gl) {
      return { hasWebGL2: false };
    }
    const w = canvas.width;
    const h = canvas.height;
    const buf = new Uint8Array(w * h * 4);
    gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, buf);

    let nonClearPixels = 0;
    let hash = 2166136261;
    const clearR = 23;
    const clearG = 28;
    const clearB = 36;

    const stride = 13;
    for (let i = 0; i < w * h; i += stride) {
      const idx = i * 4;
      const r = buf[idx];
      const g = buf[idx + 1];
      const b = buf[idx + 2];
      const diff = Math.abs(r - clearR) + Math.abs(g - clearG) + Math.abs(b - clearB);
      if (diff > 18) {
        nonClearPixels++;
      }
      hash ^= r;
      hash = Math.imul(hash, 16777619);
      hash ^= g;
      hash = Math.imul(hash, 16777619);
      hash ^= b;
      hash = Math.imul(hash, 16777619);
    }

    return {
      hasWebGL2: true,
      width: w,
      height: h,
      nonClearPixels,
      hash: hash >>> 0,
      renderer: gl.getParameter(gl.RENDERER),
      version: gl.getParameter(gl.VERSION),
    };
  });
}

async function sampleMaterialPreviewCanvas(page) {
  return await page.evaluate(() => {
    const c = document.getElementById("material-preview-canvas");
    const ctx = c.getContext("2d");
    const w = c.width;
    const h = c.height;
    const data = ctx.getImageData(0, 0, w, h).data;

    const pixelAt = (x, y) => {
      const idx = (y * w + x) * 4;
      return [data[idx], data[idx + 1], data[idx + 2], data[idx + 3]];
    };

    const corner = pixelAt(2, 2);
    const center = pixelAt(Math.floor(w / 2), Math.floor(h / 2));

    let hash = 2166136261;
    let distinctFromCorner = 0;
    for (let i = 0; i < data.length; i += 4) {
      const r = data[i];
      const g = data[i + 1];
      const b = data[i + 2];
      if (Math.abs(r - corner[0]) + Math.abs(g - corner[1]) + Math.abs(b - corner[2]) > 20) {
        distinctFromCorner++;
      }
      hash ^= r;
      hash = Math.imul(hash, 16777619);
      hash ^= g;
      hash = Math.imul(hash, 16777619);
      hash ^= b;
      hash = Math.imul(hash, 16777619);
    }

    return {
      width: w,
      height: h,
      corner,
      center,
      distinctFromCorner,
      hash: hash >>> 0,
    };
  });
}

async function setControlValueAndCommit(locator, value) {
  await locator.evaluate((el, v) => {
    el.value = String(v);
    el.dispatchEvent(new Event("input", { bubbles: true }));
    el.dispatchEvent(new Event("change", { bubbles: true }));
  }, value);
}

async function assertNoHorizontalOverflow(page) {
  const dims = await page.evaluate(() => ({
    scrollW: document.documentElement.scrollWidth,
    clientW: document.documentElement.clientWidth,
    innerW: window.innerWidth,
  }));
  expect(dims.scrollW).toBeLessThanOrEqual(Math.max(dims.clientW, dims.innerW) + 1);
}

async function bootToHomeReady(page) {
  const pageErrors = [];
  page.on("pageerror", (err) => pageErrors.push(err.message));

  await page.goto("/index.html");
  await expect(page.getByTestId("splash-screen")).toBeHidden({ timeout: 25_000 });
  await expect(page.getByTestId("home-screen")).toBeVisible();
  await expect(page.getByTestId("home-engine-badge")).toContainText("v0.5.0");
  expect(pageErrors).toEqual([]);
  return pageErrors;
}

async function bootDirectStudioReady(page) {
  const pageErrors = [];
  page.on("pageerror", (err) => pageErrors.push(err.message));

  await page.goto("/index.html?view=studio");
  await expect(page.getByTestId("splash-screen")).toBeHidden({ timeout: 25_000 });
  await expect(page.getByTestId("studio-shell")).toBeVisible();
  await expect(page.getByTestId("engine-badge")).toContainText("v0.5.0");
  await expect(page.getByTestId("stat-objects-meshes")).toHaveText("6 / 3");
  expect(pageErrors).toEqual([]);
  return pageErrors;
}

test.describe("Final Effect 3D Stage 5 Complete Creative Studio Suite", () => {
  test("1. Splash screen gates on real WASM/WebGL2 readiness, opens Main Home, and presents honest 6-Studio architecture", async ({
    page,
  }) => {
    await bootToHomeReady(page);
    await assertNoHorizontalOverflow(page);

    // All 6 Studios are present in the Studio Selector
    const studioIds = [
      "3d_studio",
      "landscaping_studio",
      "character_studio",
      "studio_2d",
      "sculpting_studio",
      "shade_studio",
    ];
    for (const id of studioIds) {
      await expect(page.getByTestId(`studio-card-${id}`)).toBeVisible();
    }

    // 3D Studio shows operational Project Launcher by default
    await expect(page.getByTestId("studio-launcher-panel")).toBeVisible();
    await expect(page.getByTestId("studio-future-panel")).toBeHidden();

    // Every planned studio shows honest architectural status with zero fake launch controls
    for (const plannedId of studioIds.slice(1)) {
      await page.getByTestId(`studio-card-${plannedId}`).click();
      await expect(page.getByTestId("studio-launcher-panel")).toBeHidden();
      await expect(page.getByTestId("studio-future-panel")).toBeVisible();
      await expect(page.locator("#future-studio-badge")).toContainText("PLANNED");
    }

    // Shade Studio provides an honest shortcut into operational 3D Studio Shading workspace
    await page.getByTestId("btn-future-jump-shading").click();
    await expect(page.getByTestId("home-screen")).toBeHidden();
    await expect(page.getByTestId("studio-shell")).toBeVisible();
    await expect(page.getByTestId("ws-tab-shading")).toHaveClass(/is-active/);
  });

  test("2. 3D Studio Project Launcher creates real templates, records & reopens Recent Projects with thumbnails, removes entries cleanly, and rejects corrupted files", async ({
    page,
  }) => {
    await bootToHomeReady(page);

    // Recent projects begins genuinely empty (no fake hardcoded entries)
    await expect(page.getByTestId("recent-empty-state")).toBeVisible();

    // Create a Minimal Cube project via the Launcher
    await page.getByTestId("template-card-minimal_cube").click();
    await setControlValueAndCommit(page.getByTestId("launcher-project-name"), "ArchCubeProject");
    await setControlValueAndCommit(page.getByTestId("launcher-project-author"), "Stage5Artist");
    await page.getByTestId("btn-launcher-create").click();

    // Verify we entered 3D Studio with real C++ MinimalCube scene (4 objects: Camera, Light, Ground, Cube)
    await expect(page.getByTestId("studio-shell")).toBeVisible();
    await expect(page.getByTestId("active-project-pill")).toHaveText("ArchCubeProject");
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("4 / 2");
    await expect(page.getByTestId("outliner-item-Cube")).toBeVisible();

    // Add a Sphere object (4 -> 5 objects) and return to Main Home
    await page.getByTestId("btn-add-sphere").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("5 / 3");
    await page.getByTestId("btn-go-home").click();

    await expect(page.getByTestId("home-screen")).toBeVisible();
    const recentItems = page.getByTestId("recent-project-item");
    await expect(recentItems).toHaveCount(1);
    await expect(recentItems.first()).toContainText("ArchCubeProject");
    // Verify real captured WebGL2 data URL thumbnail
    const thumbSrc = await recentItems.first().locator("img").getAttribute("src");
    expect(thumbSrc).toMatch(/^data:image\/jpeg;base64,/);

    // Create an Empty Stage project (2 objects: MainCamera + SunLight)
    await page.getByTestId("template-card-empty_stage").click();
    await setControlValueAndCommit(page.getByTestId("launcher-project-name"), "CleanStage");
    await page.getByTestId("btn-launcher-create").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("2 / 0");

    // Return to Home and reopen "ArchCubeProject" from Recent Projects -> restores 5 objects!
    await page.getByTestId("btn-go-home").click();
    await expect(recentItems).toHaveCount(2);
    const archRow = recentItems.filter({ hasText: "ArchCubeProject" });
    await archRow.getByTestId("btn-reopen-recent").click();
    await expect(page.getByTestId("studio-shell")).toBeVisible();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("5 / 3");

    // Return to Home, test corrupted .fe3d file handling in Launcher
    await page.getByTestId("btn-go-home").click();
    const tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), "fe3d-corrupt-"));
    const badPath = path.join(tmpDir, "broken.fe3d");
    await fs.writeFile(badPath, "{invalid-corrupted-fe3d-payload", "utf-8");
    await page.locator("#file-open-input").setInputFiles([badPath]);
    await expect(page.getByTestId("launcher-error-banner")).toBeVisible();
    await expect(page.getByTestId("home-screen")).toBeVisible();

    // Remove "CleanStage" from Recent Projects without affecting actual disk files
    const cleanRow = recentItems.filter({ hasText: "CleanStage" });
    await cleanRow.getByTestId("btn-remove-recent").click();
    await expect(recentItems).toHaveCount(1);
    expect(await fs.stat(badPath)).toBeTruthy();

    await fs.rm(tmpDir, { recursive: true, force: true });
  });

  test("3. All 12 Blender-inspired workspace tabs share the exact same live scene, selection, materials & Undo stack, and workspace layout customization persists independently of scene data", async ({
    page,
  }) => {
    await bootDirectStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();

    // Author an object + material in Layout workspace
    await page.getByTestId("btn-add-sphere").click();
    await setControlValueAndCommit(page.getByTestId("insp-name"), "SharedWorkspaceOrb");
    await page.getByTestId("btn-mat-new").click();
    await setControlValueAndCommit(page.getByTestId("mat-name"), "SharedCyanMat");

    const baseline = await page.evaluate(() => {
      const s = window.__FE3D_STUDIO__.getState();
      return {
        selectedId: s.viewport.selected_object_id,
        objCount: s.stats.object_count,
        matCount: s.stats.material_count,
        undoCount: s.history.undo_count,
      };
    });

    const operationalTabs = ["layout", "modeling", "shading", "animation", "rendering"];
    const foundationTabs = [
      "sculpting",
      "compositing",
      "vfx",
      "geometry_nodes",
      "uv_editing",
      "animation_2d",
      "video_editing",
    ];

    for (const tabId of [...operationalTabs, ...foundationTabs]) {
      await page.getByTestId(`ws-tab-${tabId}`).click();
      await expect(page.getByTestId(`ws-tab-${tabId}`)).toHaveClass(/is-active/);

      if (foundationTabs.includes(tabId)) {
        await expect(page.getByTestId("workspace-foundation-banner")).toBeVisible();
      } else {
        await expect(page.getByTestId("workspace-foundation-banner")).toBeHidden();
      }

      const snap = await page.evaluate(() => {
        const s = window.__FE3D_STUDIO__.getState();
        return {
          selectedId: s.viewport.selected_object_id,
          objCount: s.stats.object_count,
          matCount: s.stats.material_count,
          undoCount: s.history.undo_count,
          hasOrb: s.objects.some((o) => o.name === "SharedWorkspaceOrb"),
          hasMat: s.materials.some((m) => m.name === "SharedCyanMat"),
        };
      });
      expect(snap.selectedId).toBe(baseline.selectedId);
      expect(snap.objCount).toBe(baseline.objCount);
      expect(snap.matCount).toBe(baseline.matCount);
      expect(snap.undoCount).toBe(baseline.undoCount);
      expect(snap.hasOrb).toBe(true);
      expect(snap.hasMat).toBe(true);
    }

    // Switch back to Modeling (hides bottom dock), drag right splitter, and save custom workspace layout
    await page.getByTestId("ws-tab-modeling").click();
    await expect(page.getByTestId("dock-bottom")).toBeHidden();

    const splitter = page.getByTestId("splitter-right");
    const sbox = await splitter.boundingBox();
    expect(sbox).not.toBeNull();
    await page.mouse.move(sbox.x + 2, sbox.y + sbox.height * 0.5);
    await page.mouse.down();
    await page.mouse.move(sbox.x - 45, sbox.y + sbox.height * 0.5, { steps: 4 });
    await page.mouse.up();

    // Save custom layout via Preferences modal and verify scene objects & Undo stack were NOT mutated
    await page.getByTestId("btn-studio-settings").click();
    await page.getByTestId("btn-pref-save-layout").click();
    await page.getByTestId("btn-close-settings").click();

    // Switch to Layout, then restore saved custom layout -> returns to Modeling
    await page.getByTestId("ws-tab-layout").click();
    await expect(page.getByTestId("dock-bottom")).toBeVisible();
    await page.getByTestId("btn-studio-settings").click();
    await page.getByTestId("btn-pref-restore-layout").click();
    await page.getByTestId("btn-close-settings").click();
    await expect(page.getByTestId("ws-tab-modeling")).toHaveClass(/is-active/);
    await expect(page.getByTestId("dock-bottom")).toBeHidden();

    // Reload browser and confirm persisted workspace layout reloads cleanly
    await page.goto("/index.html?view=studio");
    await expect(page.getByTestId("splash-screen")).toBeHidden({ timeout: 25_000 });
    await expect(page.getByTestId("ws-tab-modeling")).toHaveClass(/is-active/);
    await expect(page.getByTestId("dock-bottom")).toBeHidden();
    await page.getByTestId("ws-tab-layout").click();
  });

  test("4. Core 3D Studio authoring: WebGL2 rasterization, raycast pick, RMB orbit, focus guard, cycle-safe parenting, camera/light live shading, and .fe3d + .fe3dbin round-trip", async ({
    page,
  }) => {
    await bootDirectStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();
    await page.waitForTimeout(50);

    await expect(page.getByTestId("outliner-item-GroundPlane")).toBeVisible();
    await expect(page.getByTestId("outliner-item-HeroCube")).toBeVisible();

    const initialPixels = await sampleViewportPixels(page);
    expect(initialPixels.hasWebGL2).toBe(true);
    expect(initialPixels.version).toContain("WebGL 2.0");
    expect(initialPixels.nonClearPixels).toBeGreaterThan(250);

    // LMB pick HeroCube in 3D Viewport
    const canvas = page.getByTestId("viewport-canvas");
    const box = await canvas.boundingBox();
    expect(box).not.toBeNull();
    await page.mouse.click(box.x + box.width * 0.5, box.y + box.height * 0.5);
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: HeroCube");

    // RMB drag camera orbit changes WebGL2 framebuffer pixels
    const cx = box.x + box.width * 0.5;
    const cy = box.y + box.height * 0.5;
    await page.mouse.move(cx, cy);
    await page.mouse.down({ button: "right" });
    await page.mouse.move(cx + 110, cy + 40, { steps: 6 });
    await page.mouse.up({ button: "right" });
    await expect
      .poll(async () => (await sampleViewportPixels(page)).hash)
      .not.toBe(initialPixels.hash);

    // Add Cube, rename with viewport hotkey characters (focus guard), Duplicate, Delete, Undo/Redo
    await page.getByTestId("btn-add-cube").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 3");
    const nameInput = page.getByTestId("insp-name");
    await nameInput.click({ clickCount: 3 });
    await page.keyboard.press("Backspace");
    await page.keyboard.type("Pillar_WERF1234");
    await page.keyboard.press("Enter");
    await expect(page.getByTestId("outliner-item-Pillar_WERF1234")).toBeVisible();
    await expect(page.getByTestId("vp-mode-pill")).toContainText("OBJECT · MOVE");

    await page.getByTestId("btn-duplicate").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("8 / 3");
    await expect(page.getByTestId("outliner-item-Pillar_WERF1234.001")).toBeVisible();

    await page.getByTestId("btn-delete").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 3");
    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("8 / 3");
    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 3");
    await page.getByTestId("btn-redo").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("8 / 3");

    // Cycle-safe parenting rejection
    await page.getByTestId("outliner-item-RigPivot").click();
    const heroId = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().objects.find((o) => o.name === "HeroCube").id
    );
    await page.getByTestId("insp-parent").selectOption(String(heroId));
    await expect(page.getByTestId("status-message")).toContainText("HierarchyCycle");

    // Camera orthographic & Light intensity live WebGL2 output
    const beforeCam = await sampleViewportPixels(page);
    await page.getByTestId("outliner-item-MainCamera").click();
    await page.getByTestId("cam-projection").selectOption("orthographic");
    await expect
      .poll(async () => (await sampleViewportPixels(page)).hash)
      .not.toBe(beforeCam.hash);
    const afterCam = await sampleViewportPixels(page);

    await page.getByTestId("outliner-item-SunLight").click();
    await setControlValueAndCommit(page.getByTestId("light-intensity"), "0.25");
    await expect
      .poll(async () => (await sampleViewportPixels(page)).hash)
      .not.toBe(afterCam.hash);

    // Export .fe3d + .fe3dbin bundle, reset via File menu, and reload bundle
    await setControlValueAndCommit(page.getByTestId("meta-project-name"), "Stage5BundleTrip");
    await page.getByTestId("btn-add-sphere").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("9 / 4");

    const tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), "fe3d-s5-"));
    const bundleDownloads = [];
    const onDl = (dl) => bundleDownloads.push(dl);
    page.on("download", onDl);
    await page.getByTestId("btn-export-bundle").click();
    await expect.poll(() => bundleDownloads.length, { timeout: 10_000 }).toBe(2);
    page.off("download", onDl);

    const savedPaths = [];
    for (const dl of bundleDownloads) {
      const p = path.join(tmpDir, dl.suggestedFilename());
      await dl.saveAs(p);
      savedPaths.push(p);
    }

    await page.getByTestId("menu-file").click();
    await page.getByTestId("btn-new-project").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("6 / 3");

    await page.locator("#file-open-input").setInputFiles(savedPaths);
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("9 / 4");
    await expect(page.getByTestId("meta-project-name")).toHaveValue("Stage5BundleTrip");

    await fs.rm(tmpDir, { recursive: true, force: true });
  });

  test("5. Interactive 3D Transform Gizmos, Face/Vertex polygon extrusion & boundary fill, and live 96x96 PBR sphere preview", async ({
    page,
  }) => {
    await bootDirectStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();

    // 1. 3D Move Gizmo +Y hover, drag, Inspector sync, and single-step coalesced Undo
    await page.getByTestId("outliner-item-HeroCube").click();
    await page.getByTestId("btn-reset-transform").click();
    await page.getByTestId("btn-frame").click();
    await expect(page.getByTestId("tf-pos-y")).toHaveValue("0");
    await page.waitForTimeout(50);

    const undoBeforeDrag = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().history.undo_count
    );

    const box = await page.getByTestId("viewport-canvas").boundingBox();
    expect(box).not.toBeNull();
    const cx = box.x + box.width * 0.5;
    let handleY = box.y + box.height * 0.39;

    for (const frac of [0.39, 0.36, 0.42, 0.34, 0.31]) {
      const candidateY = box.y + box.height * frac;
      await page.mouse.move(cx, candidateY);
      await page.waitForTimeout(45);
      const hovered = await page.evaluate(
        () => window.__FE3D_STUDIO__.getState().editor.gizmo_hovered_handle
      );
      if (hovered === "translate_y") {
        handleY = candidateY;
        break;
      }
    }

    await expect(page.getByTestId("vp-mode-pill")).toContainText("HOVER TRANSLATE_Y");
    await page.mouse.down({ button: "left" });
    await page.mouse.move(cx, handleY - 60, { steps: 8 });
    await page.mouse.up({ button: "left" });

    await expect
      .poll(async () => (await page.evaluate(() => window.__FE3D_STUDIO__.getState().history.undo_count)))
      .toBe(undoBeforeDrag + 1);
    await expect
      .poll(async () => Number(await page.getByTestId("tf-pos-y").inputValue()))
      .toBeGreaterThan(0.2);

    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("tf-pos-y")).toHaveValue("0");

    // 2. Polygon Mesh Modeling: Face pick -> Extrude -> Delete cap -> Select Hole -> Fill Face -> Undo
    await page.getByTestId("btn-mode-face").click();
    await page.waitForTimeout(50);
    await expect(page.getByTestId("card-mesh-edit")).toBeVisible();
    await expect(page.getByTestId("mesh-edit-topology")).toHaveText("6 faces · 12 edges · 8 verts");

    await page.mouse.click(box.x + box.width * 0.5, box.y + box.height * 0.5);
    await expect(page.getByTestId("mesh-edit-selection-tag")).toHaveText("1 face");

    await setControlValueAndCommit(page.getByTestId("mesh-extrude-dist"), "0.45");
    await page.getByTestId("btn-mesh-extrude").click();
    await expect(page.getByTestId("mesh-edit-topology")).toHaveText("10 faces · 20 edges · 12 verts");

    await page.getByTestId("btn-mesh-delete").click();
    await expect(page.getByTestId("mesh-edit-topology")).toHaveText("9 faces · 20 edges · 12 verts");

    await page.getByTestId("btn-mesh-boundary").click();
    await expect(page.getByTestId("mesh-edit-selection-tag")).toHaveText("4 vertexs");

    await page.getByTestId("btn-mesh-fill").click();
    await expect(page.getByTestId("mesh-edit-topology")).toHaveText("10 faces · 20 edges · 12 verts");

    await page.getByTestId("btn-undo").click();
    await page.getByTestId("btn-undo").click();
    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("mesh-edit-topology")).toHaveText("6 faces · 12 edges · 8 verts");
    await page.getByTestId("btn-mode-object").click();

    // 3. PBR Material Live 96x96 Sphere Preview & Object Assignment
    const vpBeforeMat = await sampleViewportPixels(page);
    const previewInitial = await sampleMaterialPreviewCanvas(page);
    expect(previewInitial.width).toBe(96);
    expect(previewInitial.distinctFromCorner).toBeGreaterThan(1500);

    await page.getByTestId("btn-mat-new").click();
    await setControlValueAndCommit(page.getByTestId("mat-name"), "SolarGold");
    await setControlValueAndCommit(page.getByTestId("mat-base-color"), "#ffcc22");
    await setControlValueAndCommit(page.getByTestId("mat-metallic"), "0.95");
    await setControlValueAndCommit(page.getByTestId("mat-roughness"), "0.12");

    const previewGold = await sampleMaterialPreviewCanvas(page);
    expect(previewGold.hash).not.toBe(previewInitial.hash);

    await page.getByTestId("btn-mat-assign-obj").click();
    await expect
      .poll(async () => (await sampleViewportPixels(page)).hash)
      .not.toBe(vpBeforeMat.hash);
  });

  test("6. Multi-device responsive & touch matrix: iPhone Portrait, iPhone Landscape, iPhone Safari 'Request Desktop Website', Android, Tablet, and Desktop 1366x768 & 1920x1080", async ({
    browser,
  }) => {
    const context = await browser.newContext({
      viewport: { width: 390, height: 844 },
      hasTouch: true,
      isMobile: true,
      deviceScaleFactor: 2,
    });
    const page = await context.newPage();

    // A. iPhone Safari Portrait (390x844) — Splash -> Home -> 3D Studio Portrait Drawer + Touch Gestures
    await bootToHomeReady(page);
    await expect(page.locator("body")).toHaveAttribute("data-layout-mode", "portrait");
    await assertNoHorizontalOverflow(page);

    await page.getByTestId("btn-launcher-create").click();
    await expect(page.getByTestId("studio-shell")).toBeVisible();
    await expect(page.getByTestId("mobile-portrait-nav")).toBeVisible();
    await assertNoHorizontalOverflow(page);

    // Pause rig and clear initial selection so touch tap raycast is unambiguously proven
    await page.getByTestId("btn-toggle-rig").click();
    await page.evaluate(() => window.__FE3D_STUDIO__.dispatch({ action: "select_object", id: 0 }));
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("No Selection");
    await page.waitForTimeout(60);

    // Single-finger touch TAP on #fe3d-canvas center selects HeroCube without orbiting camera
    await page.evaluate(() => {
      const c = document.getElementById("fe3d-canvas");
      const r = c.getBoundingClientRect();
      const cx = r.left + r.width * 0.5;
      const cy = r.top + r.height * 0.5;
      c.dispatchEvent(
        new PointerEvent("pointerdown", {
          pointerId: 11,
          pointerType: "touch",
          clientX: cx,
          clientY: cy,
          bubbles: true,
        })
      );
      c.dispatchEvent(
        new PointerEvent("pointerup", {
          pointerId: 11,
          pointerType: "touch",
          clientX: cx,
          clientY: cy,
          bubbles: true,
        })
      );
    });
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: HeroCube");

    // 1-finger touch DRAG on empty viewport orbits camera while preserving HeroCube selection (zero conflict!)
    const hashBeforeTouchOrbit = (await sampleViewportPixels(page)).hash;
    await page.evaluate(() => {
      const c = document.getElementById("fe3d-canvas");
      const r = c.getBoundingClientRect();
      const sx = r.left + r.width * 0.18;
      const sy = r.top + r.height * 0.24;
      c.dispatchEvent(
        new PointerEvent("pointerdown", {
          pointerId: 12,
          pointerType: "touch",
          clientX: sx,
          clientY: sy,
          bubbles: true,
        })
      );
      for (let i = 1; i <= 5; i++) {
        c.dispatchEvent(
          new PointerEvent("pointermove", {
            pointerId: 12,
            pointerType: "touch",
            clientX: sx + i * 16,
            clientY: sy + i * 8,
            bubbles: true,
          })
        );
      }
      c.dispatchEvent(
        new PointerEvent("pointerup", {
          pointerId: 12,
          pointerType: "touch",
          clientX: sx + 80,
          clientY: sy + 40,
          bubbles: true,
        })
      );
    });
    await expect
      .poll(async () => (await sampleViewportPixels(page)).hash)
      .not.toBe(hashBeforeTouchOrbit);
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: HeroCube");

    // 2-finger pinch-zoom gesture adjusts camera distance cleanly without losing selection
    const distBeforePinch = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().viewport.camera_distance
    );
    await page.evaluate(() => {
      const c = document.getElementById("fe3d-canvas");
      const r = c.getBoundingClientRect();
      const cy = r.top + r.height * 0.5;
      c.dispatchEvent(
        new PointerEvent("pointerdown", {
          pointerId: 21,
          pointerType: "touch",
          clientX: r.left + 120,
          clientY: cy,
          bubbles: true,
        })
      );
      c.dispatchEvent(
        new PointerEvent("pointerdown", {
          pointerId: 22,
          pointerType: "touch",
          clientX: r.left + 220,
          clientY: cy,
          bubbles: true,
        })
      );
      c.dispatchEvent(
        new PointerEvent("pointermove", {
          pointerId: 21,
          pointerType: "touch",
          clientX: r.left + 70,
          clientY: cy,
          bubbles: true,
        })
      );
      c.dispatchEvent(
        new PointerEvent("pointermove", {
          pointerId: 22,
          pointerType: "touch",
          clientX: r.left + 270,
          clientY: cy,
          bubbles: true,
        })
      );
      c.dispatchEvent(
        new PointerEvent("pointerup", {
          pointerId: 21,
          pointerType: "touch",
          clientX: r.left + 70,
          clientY: cy,
          bubbles: true,
        })
      );
      c.dispatchEvent(
        new PointerEvent("pointerup", {
          pointerId: 22,
          pointerType: "touch",
          clientX: r.left + 270,
          clientY: cy,
          bubbles: true,
        })
      );
    });
    const distAfterPinch = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().viewport.camera_distance
    );
    expect(distAfterPinch).not.toBe(distBeforePinch);
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: HeroCube");

    // Navigate all Mobile Portrait secondary drawers while preserving selection & live 3D Viewport!
    for (const drawer of ["outliner", "inspector", "materials", "assets", "timeline", "console", "viewport"]) {
      await page.getByTestId(`mp-tab-${drawer}`).click();
      await expect(page.getByTestId("viewport-canvas")).toBeVisible();
      await assertNoHorizontalOverflow(page);
    }
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: HeroCube");

    // B. iPhone Safari Landscape (844x390) — Preferred Mobile Landscape Pro Mode
    await page.setViewportSize({ width: 844, height: 390 });
    await expect(page.locator("body")).toHaveAttribute("data-layout-mode", "landscape");
    await expect(page.getByTestId("mobile-portrait-nav")).toBeHidden();
    await expect(page.getByTestId("dock-right")).toBeVisible();
    await assertNoHorizontalOverflow(page);

    // Toggle Landscape Panels drawer to maximize 3D Viewport
    await page.getByTestId("btn-landscape-inspector-toggle").click();
    await expect(page.getByTestId("dock-right")).toBeHidden();
    await page.getByTestId("btn-landscape-inspector-toggle").click();
    await expect(page.getByTestId("dock-right")).toBeVisible();

    // C. iPhone Safari with "Request Desktop Website" (980x1660 Portrait & 980x430 Landscape, hasTouch: true)
    // Must switch into genuine Desktop multi-panel workspace without losing HeroCube selection or touch support!
    for (const safariDesktopVp of [
      { width: 980, height: 1660 },
      { width: 980, height: 430 },
    ]) {
      await page.setViewportSize(safariDesktopVp);
      await expect(page.locator("body")).toHaveAttribute("data-layout-mode", "desktop");
      await expect(page.getByTestId("mobile-portrait-nav")).toBeHidden();
      await expect(page.getByTestId("tool-shelf")).toBeVisible();
      await expect(page.getByTestId("dock-right")).toBeVisible();
      await expect(page.getByTestId("dock-bottom")).toBeVisible();
      await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: HeroCube");
      await assertNoHorizontalOverflow(page);
    }

    // D. Android Mobile (412x915 Portrait & 915x412 Landscape)
    await page.setViewportSize({ width: 412, height: 915 });
    await expect(page.locator("body")).toHaveAttribute("data-layout-mode", "portrait");
    await assertNoHorizontalOverflow(page);

    await page.setViewportSize({ width: 915, height: 412 });
    await expect(page.locator("body")).toHaveAttribute("data-layout-mode", "landscape");
    await assertNoHorizontalOverflow(page);

    // E. Tablet (768x1024 Portrait & 1024x768 Landscape)
    await page.setViewportSize({ width: 768, height: 1024 });
    await expect(page.locator("body")).toHaveAttribute("data-layout-mode", "portrait");
    await assertNoHorizontalOverflow(page);

    await page.setViewportSize({ width: 1024, height: 768 });
    await expect(page.locator("body")).toHaveAttribute("data-layout-mode", "desktop");
    await assertNoHorizontalOverflow(page);

    // F. Desktop (1366x768 & 1920x1080) — Verify full desktop non-overlapping panel geometry
    for (const desktopVp of [
      { width: 1366, height: 768 },
      { width: 1920, height: 1080 },
    ]) {
      await page.setViewportSize(desktopVp);
      await expect(page.locator("body")).toHaveAttribute("data-layout-mode", "desktop");
      await assertNoHorizontalOverflow(page);

      const shelfBox = await page.getByTestId("tool-shelf").boundingBox();
      const vpBox = await page.getByTestId("viewport-stage").boundingBox();
      const rightBox = await page.getByTestId("dock-right").boundingBox();
      const bottomBox = await page.getByTestId("dock-bottom").boundingBox();

      expect(shelfBox.x + shelfBox.width).toBeLessThanOrEqual(vpBox.x + 2);
      expect(vpBox.x + vpBox.width).toBeLessThanOrEqual(rightBox.x + 6);
      expect(vpBox.y + vpBox.height).toBeLessThanOrEqual(bottomBox.y + 6);
      expect(vpBox.width).toBeGreaterThan(desktopVp.width * 0.55);
    }

    await context.close();
  });

  test("7. Stage 5.1 UI Polish: non-destructive workspace tabs & overflow controls, persistent dock collapse/restore, 3-axis splitters, clamped menus & RMB context menu, zero HUD overlap across all viewports, and screenshot capture", async ({
    browser,
  }) => {
    const outDir =
      process.env.FE3D_SCREENSHOT_DIR ||
      path.resolve(process.cwd(), "../../build/ci-screenshots/after-stage5.1");
    await fs.mkdir(outDir, { recursive: true });

    const context = await browser.newContext({
      viewport: { width: 1920, height: 1080 },
      hasTouch: true,
    });
    const page = await context.newPage();

    const boxesOverlap = (a, b) => {
      if (!a || !b) return false;
      return (
        a.x + 1 < b.x + b.width &&
        a.x + a.width - 1 > b.x &&
        a.y + 1 < b.y + b.height &&
        a.y + a.height - 1 > b.y
      );
    };

    const assertZeroOverlayCollisions = async (label) => {
      const hudBox = await page.getByTestId("viewport-hud-top").boundingBox();
      const navBox = await page.locator(".viewport-nav-cluster").boundingBox();
      const shelfVisible = await page.getByTestId("tool-shelf").isVisible();
      const shelfBox = shelfVisible ? await page.getByTestId("tool-shelf").boundingBox() : null;

      expect(boxesOverlap(hudBox, navBox), `HUD vs Nav overlap on ${label}`).toBe(false);
      expect(boxesOverlap(hudBox, shelfBox), `HUD vs Shelf overlap on ${label}`).toBe(false);
      expect(boxesOverlap(navBox, shelfBox), `Nav vs Shelf overlap on ${label}`).toBe(false);
    };

    // 0. Capture Home / Project Launcher screenshot at 1920x1080
    await bootToHomeReady(page);
    await assertNoHorizontalOverflow(page);
    await page.screenshot({ path: path.join(outDir, "01-home-desktop-1920x1080.png") });

    // Enter 3D Studio directly from Launcher
    await page.getByTestId("btn-launcher-create").click();
    await expect(page.getByTestId("studio-shell")).toBeVisible();
    await page.getByTestId("btn-toggle-rig").click();
    await assertZeroOverlayCollisions("Desktop 1920x1080");
    await page.screenshot({ path: path.join(outDir, "02-studio-desktop-1920x1080.png") });

    // 1. Desktop 1366x768: Workspace Tab overflow chevrons + zero DOM reconstruction on tab/transform updates
    await page.setViewportSize({ width: 1366, height: 768 });
    await assertNoHorizontalOverflow(page);
    await assertZeroOverlayCollisions("Desktop 1366x768");

    await page.evaluate(() => {
      window.__fe3dWsTabRef = document.querySelector('[data-testid="ws-tab-layout"]');
      window.__fe3dParentOptRef = document.querySelector("#insp-parent option");
    });

    await expect(page.getByTestId("btn-ws-scroll-right")).toBeVisible();
    await page.getByTestId("btn-ws-scroll-right").click();
    await expect
      .poll(async () => await page.locator("#workspace-tabs").evaluate((el) => el.scrollLeft))
      .toBeGreaterThan(20);

    await page.getByTestId("ws-tab-video_editing").click();
    await expect(page.getByTestId("ws-tab-video_editing")).toHaveClass(/is-active/);
    await page.getByTestId("ws-tab-layout").click();
    await expect(page.getByTestId("ws-tab-layout")).toHaveClass(/is-active/);

    // Verify 12 workspace tab buttons were updated in-place without DOM teardown
    const sameWsNode = await page.evaluate(
      () => document.querySelector('[data-testid="ws-tab-layout"]') === window.__fe3dWsTabRef
    );
    expect(sameWsNode).toBe(true);

    // Select HeroCube and modify X position -> verify <select id="insp-parent"> <option> nodes are NOT torn down
    await page.getByTestId("outliner-item-HeroCube").click();
    await page.evaluate(() => {
      window.__fe3dParentOptRef = document.querySelector("#insp-parent option");
    });
    await setControlValueAndCommit(page.getByTestId("tf-pos-x"), "0.75");
    const sameSelectOptionNode = await page.evaluate(
      () => document.querySelector("#insp-parent option") === window.__fe3dParentOptRef
    );
    expect(sameSelectOptionNode).toBe(true);

    // 2. Collapse & reopen all 3 Desktop Dock Regions cleanly (including regression check on toggle_right_dock)
    // Left Tool Shelf
    await page.getByTestId("btn-collapse-shelf").click();
    await expect(page.getByTestId("tool-shelf")).toBeHidden();
    await expect(page.getByTestId("btn-vp-restore-shelf")).toBeVisible();
    await page.getByTestId("btn-vp-restore-shelf").click();
    await expect(page.getByTestId("tool-shelf")).toBeVisible();
    await expect(page.getByTestId("btn-vp-restore-shelf")).toBeHidden();

    // Right Outliner/Properties Dock via View menu -> must stay collapsed across engine state revisions!
    await page.locator('[data-menu-trigger="view"]').click();
    await page.locator('[data-menu-cmd="toggle_right_dock"]').click();
    await expect(page.getByTestId("dock-right")).toBeHidden();
    await expect(page.getByTestId("splitter-right")).toBeHidden();
    await expect(page.getByTestId("btn-vp-restore-right")).toBeVisible();

    // Trigger an engine state revision (Undo) while Right Dock is collapsed -> must NOT overwrite hidden state!
    await page.getByTestId("btn-undo").click();
    await page.waitForTimeout(80);
    await expect(page.getByTestId("dock-right")).toBeHidden();

    // Restore Right Dock via viewport restore button, then collapse via Outliner header button and restore again
    await page.getByTestId("btn-vp-restore-right").click();
    await expect(page.getByTestId("dock-right")).toBeVisible();
    await page.getByTestId("btn-collapse-right").click();
    await expect(page.getByTestId("dock-right")).toBeHidden();
    await page.getByTestId("btn-vp-restore-right").click();
    await expect(page.getByTestId("dock-right")).toBeVisible();

    // Bottom Dock collapse & expand
    await page.locator("#btn-collapse-bottom").click();
    await expect(page.getByTestId("dock-bottom")).toHaveClass(/is-collapsed/);
    await expect(page.getByTestId("splitter-bottom")).toBeHidden();
    await page.locator("#btn-collapse-bottom").click();
    await expect(page.getByTestId("dock-bottom")).not.toHaveClass(/is-collapsed/);
    await expect(page.getByTestId("splitter-bottom")).toBeVisible();

    // 3. Smooth Predictable Splitters: Outliner/Properties vertical splitter + Viewport-priority clamping
    const outlinerBefore = await page.getByTestId("panel-outliner").boundingBox();
    const outSplitBox = await page.getByTestId("splitter-outliner").boundingBox();
    await page.mouse.move(
      outSplitBox.x + outSplitBox.width / 2,
      outSplitBox.y + outSplitBox.height / 2
    );
    await page.mouse.down();
    await page.mouse.move(
      outSplitBox.x + outSplitBox.width / 2,
      outSplitBox.y + outSplitBox.height / 2 + 45,
      { steps: 5 }
    );
    await page.mouse.up();
    const outlinerAfter = await page.getByTestId("panel-outliner").boundingBox();
    expect(outlinerAfter.height).toBeGreaterThan(outlinerBefore.height + 25);

    // Extreme-left drag on Right Dock splitter must clamp so central 3D Viewport retains >50% width priority
    const rightSplitBox = await page.getByTestId("splitter-right").boundingBox();
    await page.mouse.move(
      rightSplitBox.x + rightSplitBox.width / 2,
      rightSplitBox.y + rightSplitBox.height / 2
    );
    await page.mouse.down();
    await page.mouse.move(100, rightSplitBox.y + rightSplitBox.height / 2, { steps: 6 });
    await page.mouse.up();
    const vpAfterExtremeDrag = await page.getByTestId("viewport-stage").boundingBox();
    expect(vpAfterExtremeDrag.width).toBeGreaterThan(1366 * 0.5);

    // Reset workspace layout to crisp defaults
    await page.locator('[data-menu-trigger="workspace"]').click();
    await page.locator('[data-menu-cmd="reset_layout"]').click();

    // 4. Stationary RMB Viewport Context Menu (clamped at extreme bottom-right edge) vs RMB Orbit Drag
    const stageRect = await page.getByTestId("viewport-stage").boundingBox();
    await page.mouse.click(
      stageRect.x + stageRect.width - 6,
      stageRect.y + stageRect.height - 6,
      { button: "right" }
    );
    const ctxMenu = page.getByTestId("viewport-context-menu");
    await expect(ctxMenu).toBeVisible();
    const ctxBox = await ctxMenu.boundingBox();
    expect(ctxBox.x).toBeGreaterThanOrEqual(stageRect.x);
    expect(ctxBox.y).toBeGreaterThanOrEqual(stageRect.y);
    expect(ctxBox.x + ctxBox.width).toBeLessThanOrEqual(stageRect.x + stageRect.width + 1);
    expect(ctxBox.y + ctxBox.height).toBeLessThanOrEqual(stageRect.y + stageRect.height + 1);

    await page.screenshot({
      path: path.join(outDir, "03-studio-desktop-1366x768-tab-overflow-and-ctx.png"),
    });

    // Close context menu via Escape
    await page.keyboard.press("Escape");
    await expect(ctxMenu).toBeHidden();

    // Verify RMB drag orbits camera without opening context menu
    await page.mouse.move(stageRect.x + 220, stageRect.y + 200);
    await page.mouse.down({ button: "right" });
    await page.mouse.move(stageRect.x + 290, stageRect.y + 230, { steps: 5 });
    await page.mouse.up({ button: "right" });
    await expect(ctxMenu).toBeHidden();

    // 5. Multi-Viewport & Orientation Matrix Sweep (preserving WebGL2 canvas & scene state)
    await page.getByTestId("outliner-item-HeroCube").click();
    await page.evaluate(() => {
      window.__fe3dCanvasIdentity = document.getElementById("fe3d-canvas");
    });

    // iPhone Safari "Request Desktop Website" (980x430) + clamped Workspace dropdown check
    await page.setViewportSize({ width: 980, height: 430 });
    await expect(page.locator("body")).toHaveAttribute("data-layout-mode", "desktop");
    await assertNoHorizontalOverflow(page);
    await assertZeroOverlayCollisions("Safari Request Desktop Website 980x430");
    await page.locator('[data-menu-trigger="workspace"]').click();
    const wsDropdown = page.locator('.menu-item.is-open .menu-dropdown');
    await expect(wsDropdown).toBeVisible();
    const wsBox = await wsDropdown.boundingBox();
    expect(wsBox.x + wsBox.width).toBeLessThanOrEqual(980);
    expect(wsBox.y + wsBox.height).toBeLessThanOrEqual(430);
    await page.keyboard.press("Escape");
    await page.screenshot({ path: path.join(outDir, "04-studio-safari-desktop-980x430.png") });

    // Mobile Landscape Pro (844x390)
    await page.setViewportSize({ width: 844, height: 390 });
    await expect(page.locator("body")).toHaveAttribute("data-layout-mode", "landscape");
    await assertNoHorizontalOverflow(page);
    await assertZeroOverlayCollisions("Mobile Landscape 844x390");
    await page.screenshot({ path: path.join(outDir, "05-studio-mobile-landscape-844x390.png") });

    // Mobile Portrait (390x844) — Viewport-first + Inspector drawer + Done quick-return
    await page.setViewportSize({ width: 390, height: 844 });
    await expect(page.locator("body")).toHaveAttribute("data-layout-mode", "portrait");
    await assertNoHorizontalOverflow(page);
    await assertZeroOverlayCollisions("Mobile Portrait 390x844 (Viewport)");
    await page.screenshot({
      path: path.join(outDir, "06-studio-mobile-portrait-390x844-viewport.png"),
    });

    await page.getByTestId("mp-tab-inspector").click();
    await expect(page.locator("body")).toHaveAttribute("data-mp-drawer", "inspector");
    await assertNoHorizontalOverflow(page);
    await assertZeroOverlayCollisions("Mobile Portrait 390x844 (Inspector Drawer)");
    await page.screenshot({
      path: path.join(outDir, "07-studio-mobile-portrait-390x844-inspector.png"),
    });

    // Quick-return "Done" button inside Portrait drawer returns cleanly to full Viewport
    await page.locator("#panel-properties [data-mp-close-drawer]").click();
    await expect(page.locator("body")).toHaveAttribute("data-mp-drawer", "viewport");

    // Android Compact Portrait (360x800) & Tablet (1024x768)
    await page.setViewportSize({ width: 360, height: 800 });
    await assertNoHorizontalOverflow(page);
    await assertZeroOverlayCollisions("Android Portrait 360x800");

    await page.setViewportSize({ width: 1024, height: 768 });
    await assertNoHorizontalOverflow(page);
    await assertZeroOverlayCollisions("Tablet 1024x768");

    // Verify WebGL2 context & canvas element survived every orientation and layout change intact!
    const preserved = await page.evaluate(() => {
      const c = document.getElementById("fe3d-canvas");
      const gl = c.getContext("webgl2");
      return {
        sameCanvas: c === window.__fe3dCanvasIdentity,
        contextAlive: Boolean(gl && !gl.isContextLost()),
      };
    });
    expect(preserved).toEqual({ sameCanvas: true, contextAlive: true });
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: HeroCube");

    await context.close();
  });

  test("8. Stage 5.2 Camera & Directional Light viewport visibility, viewport picking, Move/Rotate/Scale gizmo transforms, multi-camera & multi-light independence, Undo/Redo, .fe3d/.fe3dbin persistence, and viewport screenshots", async ({
    browser,
  }) => {
    const outDir = path.resolve(
      process.cwd(),
      "../../build/ci-screenshots/stage5.2-camera-light"
    );
    await fs.mkdir(outDir, { recursive: true });

    const context = await browser.newContext({
      viewport: { width: 1920, height: 1080 },
      hasTouch: true,
    });
    const page = await context.newPage();

    const samplePatchAtFb = async (fbX, fbY, radius = 14) => {
      return await page.evaluate(
        ({ fx, fy, r }) => {
          const canvas = document.getElementById("fe3d-canvas");
          const gl = canvas.getContext("webgl2");
          if (!gl) return { hasWebGL2: false, nonClear: 0, hash: 0 };
          const w = canvas.width;
          const h = canvas.height;
          const cx = Math.max(r, Math.min(w - r - 1, Math.round(fx)));
          const cy = Math.max(r, Math.min(h - r - 1, h - 1 - Math.round(fy)));
          const side = r * 2 + 1;
          const buf = new Uint8Array(side * side * 4);
          gl.readPixels(cx - r, cy - r, side, side, gl.RGBA, gl.UNSIGNED_BYTE, buf);

          const cornerBuf = new Uint8Array(4);
          gl.readPixels(2, h - 3, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, cornerBuf);
          const bgR = cornerBuf[0];
          const bgG = cornerBuf[1];
          const bgB = cornerBuf[2];

          let nonClear = 0;
          let hash = 2166136261;
          for (let i = 0; i < buf.length; i += 4) {
            const pr = buf[i];
            const pg = buf[i + 1];
            const pb = buf[i + 2];
            if (Math.abs(pr - bgR) + Math.abs(pg - bgG) + Math.abs(pb - bgB) > 22) {
              nonClear++;
            }
            hash ^= pr;
            hash = Math.imul(hash, 16777619);
            hash ^= pg;
            hash = Math.imul(hash, 16777619);
            hash ^= pb;
            hash = Math.imul(hash, 16777619);
          }
          return { hasWebGL2: true, nonClear, hash: hash >>> 0 };
        },
        { fx: fbX, fy: fbY, r: radius }
      );
    };

    const fbToClient = async (fbPos) => {
      return await page.evaluate(([fx, fy]) => {
        const canvas = document.getElementById("fe3d-canvas");
        const rect = canvas.getBoundingClientRect();
        return {
          x: rect.left + (fx / canvas.width) * rect.width,
          y: rect.top + (fy / canvas.height) * rect.height,
        };
      }, fbPos);
    };

    const waitRenderedFrames = async () => {
      await page.evaluate(
        () => new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve)))
      );
    };

    // 1 & 2. Default starter scene opens with MainCamera & SunLight immediately visible in the 3D WebGL2 Viewport
    await bootDirectStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();
    await page.getByTestId("btn-clear-selection").click();
    await waitRenderedFrames();

    const initialObjs = await page.evaluate(() => {
      const s = window.__FE3D_STUDIO__.getState();
      return {
        cameraViewActive: s.viewport.camera_view_active,
        helpersVisible: s.viewport.editor_helpers_visible,
        mainCam: s.objects.find((o) => o.name === "MainCamera"),
        sunLight: s.objects.find((o) => o.name === "SunLight"),
        heroCube: s.objects.find((o) => o.name === "HeroCube"),
      };
    });
    expect(initialObjs.cameraViewActive).toBe(false);
    expect(initialObjs.helpersVisible).toBe(true);
    expect(initialObjs.mainCam?.pick_screen_pos).toBeTruthy();
    expect(initialObjs.sunLight?.pick_screen_pos).toBeTruthy();

    const mainCamPatchUnselected = await samplePatchAtFb(
      initialObjs.mainCam.pick_screen_pos[0],
      initialObjs.mainCam.pick_screen_pos[1],
      16
    );
    const sunLightPatchUnselected = await samplePatchAtFb(
      initialObjs.sunLight.pick_screen_pos[0],
      initialObjs.sunLight.pick_screen_pos[1],
      16
    );
    expect(mainCamPatchUnselected.nonClear).toBeGreaterThan(25);
    expect(sunLightPatchUnselected.nonClear).toBeGreaterThan(25);

    await page.screenshot({
      path: path.join(outDir, "01-default-scene-camera-and-light-visible-1920x1080.png"),
    });

    // 9. Toggle Scene Helpers off via View menu -> helpers cleanly disappear from rendered output, then re-enable pixel-identically
    await page.locator('[data-menu-trigger="view"]').click();
    await page.getByTestId("cmd-toggle-editor-helpers").click();
    await waitRenderedFrames();

    await expect
      .poll(
        async () =>
          (
            await samplePatchAtFb(
              initialObjs.mainCam.pick_screen_pos[0],
              initialObjs.mainCam.pick_screen_pos[1],
              16
            )
          ).hash
      )
      .not.toBe(mainCamPatchUnselected.hash);
    await expect
      .poll(
        async () =>
          (
            await samplePatchAtFb(
              initialObjs.sunLight.pick_screen_pos[0],
              initialObjs.sunLight.pick_screen_pos[1],
              16
            )
          ).hash
      )
      .not.toBe(sunLightPatchUnselected.hash);

    await page.locator('[data-menu-trigger="view"]').click();
    await page.getByTestId("cmd-toggle-editor-helpers").click();
    await waitRenderedFrames();

    await expect
      .poll(
        async () =>
          (
            await samplePatchAtFb(
              initialObjs.mainCam.pick_screen_pos[0],
              initialObjs.mainCam.pick_screen_pos[1],
              16
            )
          ).hash
      )
      .toBe(mainCamPatchUnselected.hash);
    await expect
      .poll(
        async () =>
          (
            await samplePatchAtFb(
              initialObjs.sunLight.pick_screen_pos[0],
              initialObjs.sunLight.pick_screen_pos[1],
              16
            )
          ).hash
      )
      .toBe(sunLightPatchUnselected.hash);

    // Normal mesh picking (HeroCube inside MainCamera's frustum) is NEVER blocked by camera frustum helpers
    const heroClient = await fbToClient(initialObjs.heroCube.pick_screen_pos);
    await page.mouse.click(heroClient.x, heroClient.y);
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: HeroCube");

    // 3 & 6. Click MainCamera directly in the 3D Viewport -> selects, highlights, syncs Outliner & Camera Inspector, and exposes Gizmos
    const camClient = await fbToClient(initialObjs.mainCam.pick_screen_pos);
    await page.mouse.click(camClient.x, camClient.y);
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: MainCamera");
    await expect(page.getByTestId("outliner-item-MainCamera")).toHaveClass(/is-selected/);
    await expect(page.locator("#card-camera")).toBeVisible();
    await waitRenderedFrames();

    await expect
      .poll(
        async () =>
          (
            await samplePatchAtFb(
              initialObjs.mainCam.pick_screen_pos[0],
              initialObjs.mainCam.pick_screen_pos[1],
              16
            )
          ).hash
      )
      .not.toBe(mainCamPatchUnselected.hash);

    await page.screenshot({
      path: path.join(outDir, "02-camera-selected-with-gizmo-and-inspector-1920x1080.png"),
    });

    // Drag Move Gizmo (+Y handle) directly on MainCamera in the 3D Viewport
    const camYBefore = Number(await page.getByTestId("tf-pos-y").inputValue());
    const camGizmoHandles = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().editor.gizmo_handles_screen
    );
    expect(camGizmoHandles?.y).toBeTruthy();
    const gyClient = await fbToClient(camGizmoHandles.y);
    await page.mouse.move(gyClient.x, gyClient.y);
    await page.mouse.down({ button: "left" });
    await page.mouse.move(gyClient.x, gyClient.y - 48, { steps: 6 });
    await page.mouse.up({ button: "left" });

    await expect
      .poll(async () => Number(await page.getByTestId("tf-pos-y").inputValue()))
      .toBeGreaterThan(camYBefore + 0.08);

    // Verify Rotate & Scale gizmo modes also activate cleanly on MainCamera
    await page.getByTestId("btn-gizmo-rotate").click();
    await expect(page.getByTestId("vp-mode-pill")).toContainText("ROTATE");
    await page.getByTestId("btn-gizmo-scale").click();
    await expect(page.getByTestId("vp-mode-pill")).toContainText("SCALE");
    await page.getByTestId("btn-gizmo-translate").click();

    // Dynamic Camera Frustum update on FOV change + Active Camera Lens View toggle
    const camScreenAfterDrag = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().objects.find((o) => o.name === "MainCamera").pick_screen_pos
    );
    const frustumPatchBeforeFov = await samplePatchAtFb(
      camScreenAfterDrag[0] - 50,
      camScreenAfterDrag[1] + 20,
      55
    );
    await setControlValueAndCommit(page.getByTestId("cam-fov"), "82");
    await expect
      .poll(
        async () =>
          (await samplePatchAtFb(camScreenAfterDrag[0] - 50, camScreenAfterDrag[1] + 20, 55)).hash
      )
      .not.toBe(frustumPatchBeforeFov.hash);

    await page.getByTestId("btn-toggle-camera-view").click();
    await expect(page.getByTestId("vp-camera-pill")).toContainText("Camera: MainCamera [Lens]");
    await waitRenderedFrames();
    await page.screenshot({
      path: path.join(outDir, "04-active-camera-lens-view-1920x1080.png"),
    });
    await page.getByTestId("btn-toggle-camera-view").click();
    await expect(page.getByTestId("vp-camera-pill")).not.toContainText("[Lens]");
    await waitRenderedFrames();

    // 4. Click SunLight directly in the 3D Viewport -> selects, highlights, stays visible at intensity 0, and updates real PBR lighting
    const sunScreenNow = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().objects.find((o) => o.name === "SunLight").pick_screen_pos
    );
    const sunClient = await fbToClient(sunScreenNow);
    await page.mouse.click(sunClient.x, sunClient.y);
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: SunLight");
    await expect(page.getByTestId("outliner-item-SunLight")).toHaveClass(/is-selected/);
    await expect(page.locator("#card-light")).toBeVisible();

    // Even at intensity 0, SunLight helper remains clearly visible in the 3D viewport!
    await setControlValueAndCommit(page.getByTestId("light-intensity"), "0");
    await waitRenderedFrames();
    const sunZeroWithHelpers = await samplePatchAtFb(sunScreenNow[0], sunScreenNow[1], 18);
    await page.evaluate(() =>
      window.__FE3D_STUDIO__.dispatch({ action: "set_editor_helpers_visible", visible: false })
    );
    await waitRenderedFrames();
    await expect
      .poll(async () => (await samplePatchAtFb(sunScreenNow[0], sunScreenNow[1], 18)).hash)
      .not.toBe(sunZeroWithHelpers.hash);
    await page.evaluate(() =>
      window.__FE3D_STUDIO__.dispatch({ action: "set_editor_helpers_visible", visible: true })
    );
    await waitRenderedFrames();

    // Restoring intensity & rotating SunLight updates actual PBR illumination on HeroCube
    const heroPatchDark = await samplePatchAtFb(
      initialObjs.heroCube.pick_screen_pos[0],
      initialObjs.heroCube.pick_screen_pos[1],
      18
    );
    await setControlValueAndCommit(page.getByTestId("light-intensity"), "1.9");
    await setControlValueAndCommit(page.getByTestId("tf-rot-y"), "-25");
    await waitRenderedFrames();
    await expect
      .poll(
        async () =>
          (
            await samplePatchAtFb(
              initialObjs.heroCube.pick_screen_pos[0],
              initialObjs.heroCube.pick_screen_pos[1],
              18
            )
          ).hash
      )
      .not.toBe(heroPatchDark.hash);

    // 5, 7, & 8. Multiple independent Cameras & Lights + Rename + Delete + Undo/Redo + .fe3d/.fe3dbin persistence without duplication
    await page.getByTestId("btn-add-camera").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 3");
    await setControlValueAndCommit(page.getByTestId("insp-name"), "KeyShotCam");
    await page.getByTestId("cam-projection").selectOption("orthographic");
    await expect(page.getByTestId("outliner-item-KeyShotCam")).toBeVisible();

    await page.getByTestId("btn-add-light").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("8 / 3");
    await setControlValueAndCommit(page.getByTestId("insp-name"), "RimLight");
    await setControlValueAndCommit(page.getByTestId("light-color"), "#4499ff");
    await setControlValueAndCommit(page.getByTestId("light-intensity"), "1.6");
    await expect(page.getByTestId("outliner-item-RimLight")).toBeVisible();

    await page.screenshot({
      path: path.join(outDir, "03-light-selected-with-gizmo-and-multi-camera-light-1920x1080.png"),
    });

    // Delete RimLight -> disappears cleanly -> Undo restores RimLight & its 3D helper -> Redo -> Undo
    await page.getByTestId("btn-delete").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 3");
    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("8 / 3");
    await expect(page.getByTestId("outliner-item-RimLight")).toBeVisible();

    // Save/Export .fe3d + .fe3dbin bundle, reset to New Project, and reopen -> ZERO duplicate cameras/lights!
    const tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), "fe3d-s52-"));
    const downloads = [];
    const onDl = (dl) => downloads.push(dl);
    page.on("download", onDl);
    await page.getByTestId("btn-export-bundle").click();
    await expect.poll(() => downloads.length, { timeout: 10_000 }).toBe(2);
    page.off("download", onDl);

    const bundlePaths = [];
    for (const dl of downloads) {
      const p = path.join(tmpDir, dl.suggestedFilename());
      await dl.saveAs(p);
      bundlePaths.push(p);
    }

    await page.getByTestId("menu-file").click();
    await page.getByTestId("btn-new-project").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("6 / 3");

    await page.locator("#file-open-input").setInputFiles(bundlePaths);
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("8 / 3");
    await expect(page.getByTestId("outliner-item-MainCamera")).toBeVisible();
    await expect(page.getByTestId("outliner-item-KeyShotCam")).toBeVisible();
    await expect(page.getByTestId("outliner-item-SunLight")).toBeVisible();
    await expect(page.getByTestId("outliner-item-RimLight")).toBeVisible();

    // Confirm MainCamera remained Perspective while KeyShotCam stayed Orthographic independently
    await page.getByTestId("outliner-item-MainCamera").click();
    await expect(page.getByTestId("cam-projection")).toHaveValue("perspective");
    await page.getByTestId("outliner-item-KeyShotCam").click();
    await expect(page.getByTestId("cam-projection")).toHaveValue("orthographic");

    await fs.rm(tmpDir, { recursive: true, force: true });
    await context.close();
  });
});


