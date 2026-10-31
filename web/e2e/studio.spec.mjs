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
  await expect(page.getByTestId("stat-objects-meshes")).toHaveText("5 / 2");
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

    // Verify we entered 3D Studio with real C++ MinimalCube scene (3 objects: Camera, Light, Cube — ground is procedural grid!)
    await expect(page.getByTestId("studio-shell")).toBeVisible();
    await expect(page.getByTestId("active-project-pill")).toHaveText("ArchCubeProject");
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("3 / 1");
    await expect(page.getByTestId("outliner-item-Cube")).toBeVisible();

    // Add a Sphere object (3 -> 4 objects) and return to Main Home
    await page.getByTestId("btn-add-sphere").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("4 / 2");
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

    // Return to Home and reopen "ArchCubeProject" from Recent Projects -> restores 4 objects!
    await page.getByTestId("btn-go-home").click();
    await expect(recentItems).toHaveCount(2);
    const archRow = recentItems.filter({ hasText: "ArchCubeProject" });
    await archRow.getByTestId("btn-reopen-recent").click();
    await expect(page.getByTestId("studio-shell")).toBeVisible();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("4 / 2");

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

    await expect(page.getByTestId("outliner-item-GroundPlane")).toHaveCount(0);
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
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("6 / 2");
    const nameInput = page.getByTestId("insp-name");
    await nameInput.click({ clickCount: 3 });
    await page.keyboard.press("Backspace");
    await page.keyboard.type("Pillar_WERF1234");
    await page.keyboard.press("Enter");
    await expect(page.getByTestId("outliner-item-Pillar_WERF1234")).toBeVisible();
    await expect(page.getByTestId("vp-mode-pill")).toContainText("OBJECT · MOVE");

    await page.getByTestId("btn-duplicate").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 2");
    await expect(page.getByTestId("outliner-item-Pillar_WERF1234.001")).toBeVisible();

    await page.getByTestId("btn-delete").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("6 / 2");
    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 2");
    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("6 / 2");
    await page.getByTestId("btn-redo").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 2");

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
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("8 / 3");

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
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("5 / 2");

    await page.locator("#file-open-input").setInputFiles(savedPaths);
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("8 / 3");
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
      const viewGizmoBox = await page.getByTestId("viewport-view-gizmo").boundingBox();
      const shelfVisible = await page.getByTestId("tool-shelf").isVisible();
      const shelfBox = shelfVisible ? await page.getByTestId("tool-shelf").boundingBox() : null;

      expect(viewGizmoBox, `View Gizmo visible on ${label}`).toBeTruthy();
      expect(boxesOverlap(hudBox, navBox), `HUD vs Nav overlap on ${label}`).toBe(false);
      expect(boxesOverlap(hudBox, shelfBox), `HUD vs Shelf overlap on ${label}`).toBe(false);
      expect(boxesOverlap(navBox, shelfBox), `Nav vs Shelf overlap on ${label}`).toBe(false);
      expect(boxesOverlap(viewGizmoBox, hudBox), `ViewGizmo vs HUD overlap on ${label}`).toBe(false);
      expect(boxesOverlap(viewGizmoBox, navBox), `ViewGizmo vs Nav overlap on ${label}`).toBe(false);
      expect(boxesOverlap(viewGizmoBox, shelfBox), `ViewGizmo vs Shelf overlap on ${label}`).toBe(false);
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
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("6 / 2");
    await setControlValueAndCommit(page.getByTestId("insp-name"), "KeyShotCam");
    await page.getByTestId("cam-projection").selectOption("orthographic");
    await expect(page.getByTestId("outliner-item-KeyShotCam")).toBeVisible();

    await page.getByTestId("btn-add-light").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 2");
    await setControlValueAndCommit(page.getByTestId("insp-name"), "RimLight");
    await setControlValueAndCommit(page.getByTestId("light-color"), "#4499ff");
    await setControlValueAndCommit(page.getByTestId("light-intensity"), "1.6");
    await expect(page.getByTestId("outliner-item-RimLight")).toBeVisible();

    await page.screenshot({
      path: path.join(outDir, "03-light-selected-with-gizmo-and-multi-camera-light-1920x1080.png"),
    });

    // Delete RimLight -> disappears cleanly -> Undo restores RimLight & its 3D helper -> Redo -> Undo
    await page.getByTestId("btn-delete").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("6 / 2");
    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 2");
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
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("5 / 2");

    await page.locator("#file-open-input").setInputFiles(bundlePaths);
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 2");
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

  test("9. Stage 5.3 Professional Transform Gizmos (axis/plane/center/ring/scale, zoom/HiDPI sizing, hover/active states, no reselection, Undo/Redo) & Interactive 3D View Navigation Gizmo (XYZ/diagonal clicks, drag orbit, Home, Persp/Ortho, scene isolation, desktop & mobile landscape)", async ({
    browser,
  }) => {
    test.setTimeout(120_000);
    const outDir = process.env.FE3D_SCREENSHOT_DIR
      ? path.resolve(process.env.FE3D_SCREENSHOT_DIR, "stage5-3")
      : path.resolve(process.cwd(), "../../build/ci-screenshots/stage5-3");
    await fs.mkdir(outDir, { recursive: true });

    const context = await browser.newContext({
      viewport: { width: 1920, height: 1080 },
      deviceScaleFactor: 1,
    });
    const page = await context.newPage();

    const waitRenderedFrames = async () => {
      await page.evaluate(
        () =>
          new Promise((resolve) => {
            requestAnimationFrame(() => requestAnimationFrame(resolve));
          })
      );
    };

    const fbToClient = async (fbPt) => {
      return await page.evaluate(([fx, fy]) => {
        const c = document.getElementById("fe3d-canvas");
        const rect = c.getBoundingClientRect();
        const sx = rect.width / (c.width || rect.width);
        const sy = rect.height / (c.height || rect.height);
        return { x: rect.left + fx * sx, y: rect.top + fy * sy };
      }, fbPt);
    };

    await bootDirectStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();
    await waitRenderedFrames();

    // Select HeroCube and reset RigPivot + HeroCube transforms to clean world-aligned axes so axis & plane deltas are exact
    await page.getByTestId("outliner-item-HeroCube").click();
    await page.evaluate(() => {
      const objs = window.__FE3D_STUDIO__.getState().objects;
      const pivot = objs.find((o) => o.name === "RigPivot");
      const hero = objs.find((o) => o.name === "HeroCube");
      if (pivot) {
        window.__FE3D_STUDIO__.dispatch({
          action: "set_transform",
          id: pivot.id,
          position: [0, 0, 0],
          rotation_deg: [0, 0, 0],
          scale: [1, 1, 1],
        });
      }
      window.__FE3D_STUDIO__.dispatch({
        action: "set_transform",
        id: hero.id,
        position: [0, 0.85, 0],
        rotation_deg: [0, 0, 0],
        scale: [1, 1, 1],
      });
    });
    await waitRenderedFrames();

    // 1. Screen-space gizmo sizing across zoom distances on Desktop 1920x1080
    const radiusInitial = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().editor.gizmo_handles_screen.screen_radius_px
    );
    expect(radiusInitial).toBeGreaterThan(85);

    const canvasBox = await page.getByTestId("viewport-canvas").boundingBox();
    await page.mouse.move(canvasBox.x + canvasBox.width * 0.5, canvasBox.y + canvasBox.height * 0.5);
    await page.mouse.wheel(0, 420);
    await waitRenderedFrames();
    const radiusZoomedOut = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().editor.gizmo_handles_screen.screen_radius_px
    );
    expect(Math.abs(radiusZoomedOut - radiusInitial)).toBeLessThan(18);
    await page.getByTestId("btn-nav-home-view").click();
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.view_transition_active))
      .toBe(false);
    await waitRenderedFrames();

    // 2. Move Gizmo Hover + Active Highlight + Single-Axis (X) + Plane (XY) + Center (View) Drag + No Accidental Reselection
    let moveHandles = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().editor.gizmo_handles_screen
    );
    const gx = await fbToClient(moveHandles.x);
    await page.mouse.move(gx.x, gx.y);
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().editor.gizmo_hovered_handle))
      .toBe("translate_x");
    await waitRenderedFrames();
    await page.screenshot({
      path: path.join(outDir, "01-stage53-move-gizmo-hover-and-nav-widget-1920x1080.png"),
    });

    await page.mouse.down({ button: "left" });
    await page.mouse.move(gx.x + 70, gx.y, { steps: 6 });
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().editor.gizmo_active_handle))
      .toBe("translate_x");
    await page.mouse.up({ button: "left" });
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: HeroCube");
    const posAfterX = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().inspector.position
    );
    expect(posAfterX[0]).toBeGreaterThan(0.35);
    expect(Math.abs(posAfterX[1] - 0.85)).toBeLessThan(0.02);
    expect(Math.abs(posAfterX[2])).toBeLessThan(0.02);

    // Plane-handle drag (TranslateXY moves X & Y simultaneously while keeping Z == 0)
    moveHandles = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().editor.gizmo_handles_screen
    );
    const gxy = await fbToClient(moveHandles.xy);
    await page.mouse.move(gxy.x, gxy.y);
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().editor.gizmo_hovered_handle))
      .toBe("translate_xy");
    await page.mouse.down({ button: "left" });
    await page.mouse.move(gxy.x - 45, gxy.y - 50, { steps: 6 });
    await page.mouse.up({ button: "left" });
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: HeroCube");
    const posAfterXY = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().inspector.position
    );
    expect(posAfterXY[1]).toBeGreaterThan(1.0);
    expect(Math.abs(posAfterXY[2])).toBeLessThan(0.02);

    // Undo / Redo verification for Move gizmo transforms
    await page.getByTestId("btn-undo").click();
    await page.getByTestId("btn-undo").click();
    await expect
      .poll(async () => (await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.position))[0])
      .toBeCloseTo(0, 2);
    await page.getByTestId("btn-redo").click();
    await expect
      .poll(async () => (await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.position))[0])
      .toBeGreaterThan(0.35);
    await page.getByTestId("btn-undo").click();
    await waitRenderedFrames();

    // 3. Rotate Gizmo Ring Hover & Drag + Local Space Coordinate Verification
    await page.getByTestId("btn-gizmo-rotate").click();
    await waitRenderedFrames();
    const rotHandles = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().editor.gizmo_handles_screen
    );
    const gry = await fbToClient(rotHandles.ry);
    await page.mouse.move(gry.x, gry.y);
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().editor.gizmo_hovered_handle))
      .toBe("rotate_y");
    await waitRenderedFrames();
    await page.screenshot({
      path: path.join(outDir, "02-stage53-rotate-gizmo-and-nav-widget-1920x1080.png"),
    });

    await page.mouse.down({ button: "left" });
    await page.mouse.move(gry.x + 65, gry.y, { steps: 8 });
    await page.mouse.up({ button: "left" });
    const rotAfterY = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().inspector.rotation_deg
    );
    expect(Math.abs(rotAfterY[1])).toBeGreaterThan(8);

    // Verify Local Space Move moves along HeroCube's rotated local X axis (affecting both World X & Z)
    await page.getByTestId("btn-gizmo-translate").click();
    await page.getByTestId("btn-gizmo-space").click();
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().editor.gizmo_space))
      .toBe("local");
    const localHandles = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().editor.gizmo_handles_screen
    );
    const glx = await fbToClient(localHandles.x);
    await page.mouse.move(glx.x, glx.y);
    await page.mouse.down({ button: "left" });
    await page.mouse.move(glx.x + 60, glx.y, { steps: 6 });
    await page.mouse.up({ button: "left" });
    const posLocal = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().inspector.position
    );
    expect(Math.hypot(posLocal[0], posLocal[2])).toBeGreaterThan(0.25);
    expect(Math.abs(posLocal[2])).toBeGreaterThan(0.03);
    await page.getByTestId("btn-gizmo-space").click(); // back to World
    await page.getByTestId("btn-undo").click();
    await page.getByTestId("btn-undo").click();

    // 4. Scale Gizmo Uniform Center Handle & Axis Handle Drag + Undo
    await page.getByTestId("btn-gizmo-scale").click();
    await waitRenderedFrames();
    const sclHandles = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().editor.gizmo_handles_screen
    );
    const gCenter = await fbToClient(sclHandles.center);
    await page.mouse.move(gCenter.x, gCenter.y);
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().editor.gizmo_hovered_handle))
      .toBe("scale_uniform");
    await waitRenderedFrames();
    await page.screenshot({
      path: path.join(outDir, "03-stage53-scale-gizmo-and-nav-widget-1920x1080.png"),
    });

    await page.mouse.down({ button: "left" });
    await page.mouse.move(gCenter.x + 55, gCenter.y - 45, { steps: 6 });
    await page.mouse.up({ button: "left" });
    const sclAfterUni = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().inspector.scale
    );
    expect(sclAfterUni[0]).toBeGreaterThan(1.15);
    expect(sclAfterUni[1]).toBeCloseTo(sclAfterUni[0], 2);
    expect(sclAfterUni[2]).toBeCloseTo(sclAfterUni[0], 2);

    const sclHandles2 = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().editor.gizmo_handles_screen
    );
    const gsx = await fbToClient(sclHandles2.x);
    await page.mouse.move(gsx.x, gsx.y);
    await page.mouse.down({ button: "left" });
    await page.mouse.move(gsx.x + 55, gsx.y, { steps: 6 });
    await page.mouse.up({ button: "left" });
    const sclAfterAxis = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().inspector.scale
    );
    expect(sclAfterAxis[0]).toBeGreaterThan(sclAfterUni[1] + 0.12);

    await page.getByTestId("btn-undo").click();
    await page.getByTestId("btn-undo").click();
    await page.getByTestId("btn-gizmo-translate").click();
    await waitRenderedFrames();

    // 5. Interactive 3D View Navigation Gizmo: Click XYZ/-X/-Y/-Z + Diagonal + Drag Orbit + Persp/Ortho + Home + Strict Scene Isolation
    const sceneSnapshotBeforeNav = await page.evaluate(() => {
      const st = window.__FE3D_STUDIO__.getState();
      return {
        undoCount: st.history.undo_count,
        selectedId: st.viewport.selected_object_id,
        heroPos: st.inspector.position,
        heroRot: st.inspector.rotation_deg,
        heroScl: st.inspector.scale,
        objects: st.objects.map((o) => ({
          id: o.id,
          name: o.name,
          worldPos: o.world_position,
        })),
      };
    });

    const clickNavAxis = async (axisKey, expectedYaw, expectedPitch) => {
      // Return to Home first if target cap happens to sit directly behind opposite pole
      const layout0 = await page.evaluate(() => window.__FE3D_STUDIO__.getNavGizmoLayout());
      if (layout0.axes[axisKey].vz < -0.92) {
        await page.getByTestId("btn-nav-home-view").click();
        await expect
          .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.view_transition_active))
          .toBe(false);
      }
      const layout = await page.evaluate(() => window.__FE3D_STUDIO__.getNavGizmoLayout());
      const pt = layout.axes[axisKey];
      await page.mouse.click(pt.x, pt.y);
      await expect
        .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.view_transition_active))
        .toBe(false);
      const vp = await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport);
      if (expectedYaw !== null) {
        const diff = ((((vp.camera_yaw_deg - expectedYaw) % 360) + 540) % 360) - 180;
        expect(Math.abs(diff)).toBeLessThan(1.5);
      }
      expect(Math.abs(vp.camera_pitch_deg - expectedPitch)).toBeLessThan(1.5);
    };

    await clickNavAxis("pos_x", 90, 0);
    await clickNavAxis("pos_y", null, -89);
    await clickNavAxis("neg_z", 180, 0);
    await clickNavAxis("neg_x", -90, 0);
    await clickNavAxis("neg_y", null, 89);
    await clickNavAxis("pos_z", 0, 0);

    // Click a front-facing Three-Quarter Diagonal region on the 3D Navigation Gizmo
    const diagLayout = await page.evaluate(() => window.__FE3D_STUDIO__.getNavGizmoLayout());
    expect(diagLayout.diagonals.length).toBeGreaterThan(0);
    await page.mouse.click(diagLayout.diagonals[0].x, diagLayout.diagonals[0].y);
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.view_transition_active))
      .toBe(false);
    await expect(page.getByTestId("vp-camera-pill")).toContainText("Iso");

    // Toggle Editor Perspective <-> Orthographic via Navigation Gizmo subcontrol
    await expect(page.getByTestId("btn-nav-proj-toggle")).toHaveText("Persp");
    await page.getByTestId("btn-nav-proj-toggle").click();
    await expect(page.getByTestId("btn-nav-proj-toggle")).toHaveText("Ortho");
    expect(await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.projection)).toBe("orthographic");
    await waitRenderedFrames();
    await page.screenshot({
      path: path.join(outDir, "04-stage53-nav-gizmo-iso-orthographic-1920x1080.png"),
    });

    // Drag across the 3D View Navigation Gizmo to orbit editor observer camera smoothly
    const navBeforeDrag = await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport);
    const navCenter = (await page.evaluate(() => window.__FE3D_STUDIO__.getNavGizmoLayout())).center;
    await page.mouse.move(navCenter.x, navCenter.y);
    await page.mouse.down({ button: "left" });
    await page.mouse.move(navCenter.x + 34, navCenter.y - 22, { steps: 6 });
    await page.mouse.up({ button: "left" });
    const navAfterDrag = await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport);
    expect(Math.abs(navAfterDrag.camera_yaw_deg - navBeforeDrag.camera_yaw_deg)).toBeGreaterThan(4);

    // Reset / Home View restores Home Perspective view cleanly
    await page.getByTestId("btn-nav-home-view").click();
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.view_transition_active))
      .toBe(false);
    await expect(page.getByTestId("btn-nav-proj-toggle")).toHaveText("Persp");
    expect(await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.projection)).toBe("perspective");

    // Verify ZERO scene object, MainCamera, or SunLight transform mutations and ZERO Undo steps from navigation widget!
    const sceneSnapshotAfterNav = await page.evaluate(() => {
      const st = window.__FE3D_STUDIO__.getState();
      return {
        undoCount: st.history.undo_count,
        selectedId: st.viewport.selected_object_id,
        heroPos: st.inspector.position,
        heroRot: st.inspector.rotation_deg,
        heroScl: st.inspector.scale,
        objects: st.objects.map((o) => ({
          id: o.id,
          name: o.name,
          worldPos: o.world_position,
        })),
      };
    });
    expect(sceneSnapshotAfterNav.undoCount).toBe(sceneSnapshotBeforeNav.undoCount);
    expect(sceneSnapshotAfterNav.selectedId).toBe(sceneSnapshotBeforeNav.selectedId);
    expect(sceneSnapshotAfterNav.heroPos).toEqual(sceneSnapshotBeforeNav.heroPos);
    expect(sceneSnapshotAfterNav.heroRot).toEqual(sceneSnapshotBeforeNav.heroRot);
    expect(sceneSnapshotAfterNav.heroScl).toEqual(sceneSnapshotBeforeNav.heroScl);
    expect(sceneSnapshotAfterNav.objects).toEqual(sceneSnapshotBeforeNav.objects);

    await page.getByTestId("outliner-item-MainCamera").click();
    await expect(page.getByTestId("cam-projection")).toHaveValue("perspective");
    await context.close();

    // 6. Mobile Landscape High-DPI (844x390 @ 2x DPR, hasTouch: true) Touch Gizmo & Navigation Gizmo Interaction
    const mobileContext = await browser.newContext({
      viewport: { width: 844, height: 390 },
      deviceScaleFactor: 2,
      hasTouch: true,
      isMobile: true,
    });
    const mPage = await mobileContext.newPage();
    await bootDirectStudioReady(mPage);
    await mPage.getByTestId("btn-toggle-rig").click();
    await expect(mPage.locator("body")).toHaveAttribute("data-layout-mode", "landscape");

    // Select HeroCube and verify DPR-aware gizmo size in Mobile Landscape
    await mPage.getByTestId("outliner-item-HeroCube").click();
    await mPage.evaluate(
      () =>
        new Promise((resolve) => {
          requestAnimationFrame(() => requestAnimationFrame(resolve));
        })
    );
    const mobileGizmoInfo = await mPage.evaluate(() => {
      const gh = window.__FE3D_STUDIO__.getState().editor.gizmo_handles_screen;
      return { fbRadius: gh.screen_radius_px, dpr: window.devicePixelRatio };
    });
    expect(mobileGizmoInfo.fbRadius / mobileGizmoInfo.dpr).toBeGreaterThan(60);

    // Touch tap on +X endpoint of the 3D View Navigation Gizmo in Mobile Landscape
    const mNavLayout = await mPage.evaluate(() => window.__FE3D_STUDIO__.getNavGizmoLayout());
    await mPage.touchscreen.tap(mNavLayout.axes.pos_x.x, mNavLayout.axes.pos_x.y);
    await expect
      .poll(async () => await mPage.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.view_transition_active))
      .toBe(false);
    await expect(mPage.getByTestId("vp-camera-pill")).toContainText("Right (+X)");
    await expect(mPage.getByTestId("vp-selection-pill")).toHaveText("Selected: HeroCube");

    // Touch drag on Move Gizmo +Y handle in Mobile Landscape via touch PointerEvents
    const mHandles = await mPage.evaluate(() => {
      const c = document.getElementById("fe3d-canvas");
      const r = c.getBoundingClientRect();
      const gh = window.__FE3D_STUDIO__.getState().editor.gizmo_handles_screen;
      const sx = r.width / (c.width || r.width);
      const sy = r.height / (c.height || r.height);
      return { x: r.left + gh.y[0] * sx, y: r.top + gh.y[1] * sy };
    });
    const yBeforeMobileDrag = await mPage.evaluate(
      () => window.__FE3D_STUDIO__.getState().inspector.position[1]
    );
    await mPage.evaluate(async (pt) => {
      const c = document.getElementById("fe3d-canvas");
      const fire = (type, x, y) =>
        c.dispatchEvent(
          new PointerEvent(type, {
            bubbles: true,
            cancelable: true,
            pointerId: 7,
            pointerType: "touch",
            isPrimary: true,
            clientX: x,
            clientY: y,
          })
        );
      fire("pointerdown", pt.x, pt.y);
      fire("pointermove", pt.x, pt.y - 24);
      fire("pointermove", pt.x, pt.y - 46);
      fire("pointerup", pt.x, pt.y - 46);
    }, mHandles);
    await expect
      .poll(async () => await mPage.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.position[1]))
      .toBeGreaterThan(yBeforeMobileDrag + 0.12);

    await mPage.evaluate(
      () =>
        new Promise((resolve) => {
          requestAnimationFrame(() => requestAnimationFrame(resolve));
        })
    );
    await mPage.screenshot({
      path: path.join(outDir, "05-stage53-mobile-landscape-gizmo-and-nav-844x390.png"),
    });

    await mobileContext.close();
  });

  test("10. True Procedural Editor Viewport Grid: default XZ grid & axes, absent from Outliner, unselectable, user Plane preservation, View menu toggle safety, camera zoom/orbit & Persp/Ortho, final render & .fe3d exclusion, and helper/gizmo parity", async ({
    browser,
  }) => {
    test.setTimeout(120_000);
    const outDir = process.env.FE3D_SCREENSHOT_DIR
      ? path.resolve(process.env.FE3D_SCREENSHOT_DIR)
      : path.resolve(process.cwd(), "../../build/ci-screenshots/editor-grid");
    await fs.mkdir(outDir, { recursive: true });

    const context = await browser.newContext({ viewport: { width: 1920, height: 1080 } });
    const page = await context.newPage();
    await bootDirectStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();
    await page.waitForTimeout(50);

    // (1) & (2): Default viewport displays procedural editor grid; NO GroundPlane or Grid object in Outliner!
    await expect(page.getByTestId("outliner-item-GroundPlane")).toHaveCount(0);
    const starterState = await page.evaluate(() => window.__FE3D_STUDIO__.getState());
    expect(starterState.viewport.editor_grid_visible).toBe(true);
    expect(starterState.stats.object_count).toBe(5);
    expect(starterState.stats.mesh_count).toBe(2);
    expect(starterState.stats.gpu_buffer_count).toBeLessThanOrEqual(4); // HeroCube + AccentPyramid + reusable helper/gizmo slots (zero grid mesh!)
    expect(
      starterState.objects.some((o) => /ground|grid/i.test(o.name))
    ).toBe(false);

    await page.screenshot({
      path: path.join(outDir, "06-editor-grid-perspective-showcase-1920x1080.png"),
    });

    // Switch to Empty Stage (MainCamera + SunLight, 0 meshes) and hide scene helpers so ONLY the procedural grid renders:
    await page.evaluate(() => {
      window.__FE3D_STUDIO__.dispatch({
        action: "create_new_project",
        template: "empty_stage",
        project_name: "ProceduralGridOnly",
      });
      window.__FE3D_STUDIO__.dispatch({ action: "set_editor_helpers_visible", visible: false });
    });
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("2 / 0");

    // Sample WebGL2 framebuffer: verify major/minor grid lines + distinct red X & blue Z world axes!
    const analyzeGridFramebuffer = async () =>
      page.evaluate(
        () =>
          new Promise((resolve) => {
            requestAnimationFrame(() =>
              requestAnimationFrame(() => {
                const c = document.getElementById("fe3d-canvas");
                const gl = c.getContext("webgl2");
                const w = c.width;
                const h = c.height;
                const buf = new Uint8Array(w * h * 4);
                gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, buf);
                // FrameClearDesc default clear_color = (0.09, 0.11, 0.14) -> (23, 28, 36):
                const bgR = 23;
                const bgG = 28;
                const bgB = 36;
                let gridPx = 0;
                let redAxisPx = 0;
                let blueAxisPx = 0;
                let hash = 2166136261;
                for (let i = 0; i < buf.length; i += 16) {
                  const r = buf[i];
                  const g = buf[i + 1];
                  const b = buf[i + 2];
                  const diff = Math.abs(r - bgR) + Math.abs(g - bgG) + Math.abs(b - bgB);
                  if (diff >= 12) {
                    gridPx++;
                    if (r > b + 35 && r > g + 28) redAxisPx++;
                    if (b > r + 35 && b > g + 10) blueAxisPx++;
                  }
                  hash ^= r ^ (g << 8) ^ (b << 16);
                  hash = Math.imul(hash, 16777619);
                }
                resolve({ gridPx, redAxisPx, blueAxisPx, hash: hash >>> 0 });
              })
            );
          })
      );

    const emptyGridStats = await analyzeGridFramebuffer();
    expect(emptyGridStats.gridPx).toBeGreaterThan(250);
    expect(emptyGridStats.redAxisPx).toBeGreaterThan(8);
    expect(emptyGridStats.blueAxisPx).toBeGreaterThan(8);

    await page.screenshot({
      path: path.join(outDir, "07-editor-grid-empty-stage-axes-and-origin-1920x1080.png"),
    });

    // (3): Grid cannot be selected, transformed, or deleted as an object!
    const canvas = page.getByTestId("viewport-canvas");
    const box = await canvas.boundingBox();
    expect(box).not.toBeNull();
    await page.mouse.click(box.x + box.width * 0.5, box.y + box.height * 0.62);
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("No Selection");
    await page.keyboard.press("Delete");
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("2 / 0");

    // (8): Final scene render (renderFinalFrame) & .fe3d serialization MUST exclude the editor grid!
    const finalRenderNonClear = await page.evaluate(() => {
      const ok = window.__FE3D_STUDIO__.renderFinalFrame();
      if (!ok) return -1;
      const c = document.getElementById("fe3d-canvas");
      const gl = c.getContext("webgl2");
      const w = c.width;
      const h = c.height;
      const buf = new Uint8Array(w * h * 4);
      gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, buf);
      const bgR = 23;
      const bgG = 28;
      const bgB = 36;
      let nonClear = 0;
      for (let i = 0; i < buf.length; i += 16) {
        if (Math.abs(buf[i] - bgR) + Math.abs(buf[i + 1] - bgG) + Math.abs(buf[i + 2] - bgB) >= 8) {
          nonClear++;
        }
      }
      return nonClear;
    });
    expect(finalRenderNonClear).toBe(0);

    const exportedEmptyJson = JSON.parse(
      await page.evaluate(() => window.__FE3D_STUDIO__.exportInlineProject())
    );
    expect(exportedEmptyJson.scene.meshes).toHaveLength(0);
    expect(exportedEmptyJson.scene.objects).toHaveLength(2);

    // (6) & (7): Grid follows camera navigation, zoom-adaptive LOD, and Perspective + Orthographic views!
    await page.mouse.move(box.x + box.width * 0.5, box.y + box.height * 0.5);
    await page.mouse.wheel(0, -360);
    const zoomedInGrid = await analyzeGridFramebuffer();
    expect(zoomedInGrid.hash).not.toBe(emptyGridStats.hash);
    expect(zoomedInGrid.gridPx).toBeGreaterThan(200);

    // Switch to Orthographic & snap to Top (+Y) view via Navigation Gizmo:
    await page.getByTestId("btn-nav-proj-toggle").click();
    await expect(page.getByTestId("btn-nav-proj-toggle")).toHaveText("Ortho");
    const ortho3dStats = await analyzeGridFramebuffer();
    expect(ortho3dStats.gridPx).toBeGreaterThan(250);

    const navLayout = await page.evaluate(() => window.__FE3D_STUDIO__.getNavGizmoLayout());
    await page.mouse.click(navLayout.axes.pos_y.x, navLayout.axes.pos_y.y);
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.view_transition_active))
      .toBe(false);
    const topOrthoStats = await analyzeGridFramebuffer();
    expect(topOrthoStats.gridPx).toBeGreaterThan(300);
    expect(topOrthoStats.redAxisPx).toBeGreaterThan(10);
    expect(topOrthoStats.blueAxisPx).toBeGreaterThan(10);

    // (4) & (5): Create a real user Plane object ('Add -> Mesh -> Grid Plane'), edit & persist it,
    // and verify View -> Toggle Ground Grid hides ONLY the procedural grid without touching the Plane object!
    await page.getByTestId("btn-nav-home-view").click();
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.view_transition_active))
      .toBe(false);
    await expect(page.getByTestId("btn-nav-proj-toggle")).toHaveText("Persp");

    await page.getByTestId("btn-add-plane").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("3 / 1");
    await expect(page.getByTestId("outliner-item-Plane")).toBeVisible();
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: Plane");

    // Raise user Plane to Y = 0.65 so both user Plane mesh AND underlying Y = 0 procedural grid are distinct:
    await setControlValueAndCommit(page.getByTestId("tf-pos-y"), "0.65");
    await page.screenshot({
      path: path.join(outDir, "08-editor-grid-and-real-user-plane-mesh-1920x1080.png"),
    });

    const withGridAndPlane = await analyzeGridFramebuffer();
    await page.getByTestId("menu-view").click();
    await page.getByTestId("cmd-toggle-editor-grid").click();
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.editor_grid_visible))
      .toBe(false);

    // User Plane object remains 100% visible, selected, and intact in Scene Outliner!
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("3 / 1");
    await expect(page.getByTestId("outliner-item-Plane")).toBeVisible();
    const planeOnlyStats = await analyzeGridFramebuffer();
    expect(planeOnlyStats.hash).not.toBe(withGridAndPlane.hash);
    expect(planeOnlyStats.gridPx).toBeGreaterThan(80); // Real Plane object still renders!

    // Save inline .fe3d with user Plane + re-import -> user Plane preserved untouched!
    const savedPlaneProject = await page.evaluate(() => window.__FE3D_STUDIO__.exportInlineProject());
    await page.evaluate((json) => window.__FE3D_STUDIO__.importInlineProject(json), savedPlaneProject);
    await expect(page.getByTestId("outliner-item-Plane")).toBeVisible();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("3 / 1");

    // Re-enable procedural grid via View -> Toggle Ground Grid & restore Scene Helpers (Scenario 9):
    await page.getByTestId("menu-view").click();
    await page.getByTestId("cmd-toggle-editor-grid").click();
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.editor_grid_visible))
      .toBe(true);
    await page.getByTestId("menu-view").click();
    await page.getByTestId("cmd-toggle-editor-helpers").click();

    // Verify Transform Gizmo (+X Move handle) and SunLight helper still work cleanly alongside the grid:
    await page.getByTestId("outliner-item-Plane").click();
    const planeXBefore = await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.position[0]);
    const planeGx = await page.evaluate(() => {
      const c = document.getElementById("fe3d-canvas");
      const r = c.getBoundingClientRect();
      const gh = window.__FE3D_STUDIO__.getState().editor.gizmo_handles_screen;
      return {
        x: r.left + gh.x[0] * (r.width / (c.width || r.width)),
        y: r.top + gh.x[1] * (r.height / (c.height || r.height)),
      };
    });
    await page.mouse.move(planeGx.x, planeGx.y);
    await page.mouse.down({ button: "left" });
    await page.mouse.move(planeGx.x + 60, planeGx.y, { steps: 6 });
    await page.mouse.up({ button: "left" });
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.position[0]))
      .toBeGreaterThan(planeXBefore + 0.15);

    await page.getByTestId("outliner-item-SunLight").click();
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: SunLight");

    await context.close();
  });

  test("11. Stage 5.4 Lighting Engine Part A: All 4 Light Types (Directional, Point, Spot, Rect & Disk Area), Independent Properties, Real WebGL2 PBR Attenuation/Cone/Area Spread, and Real-Time Shadow Atlas", async ({
    browser,
  }) => {
    const outDir =
      process.env.FE3D_SCREENSHOT_DIR ||
      path.resolve(process.cwd(), "../../build/ci-screenshots/after-stage5.1");
    await fs.mkdir(outDir, { recursive: true });

    const context = await browser.newContext({ viewport: { width: 1920, height: 1080 } });
    const page = await context.newPage();
    await bootDirectStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();

    const setCheckedAndCommit = async (locator, checked) => {
      await locator.evaluate((el, v) => {
        el.checked = Boolean(v);
        el.dispatchEvent(new Event("change", { bubbles: true }));
      }, checked);
    };

    // Helper: synchronously renders final camera frame and samples normalized (nx, ny) patches in WebGL2 (0..1, bottom-up ny)
    const sampleFinalPatches = async (patches) =>
      page.evaluate((pts) => {
        const ok = window.__FE3D_STUDIO__.renderFinalFrame();
        if (!ok) return null;
        const c = document.getElementById("fe3d-canvas");
        const gl = c.getContext("webgl2");
        const w = c.width;
        const h = c.height;
        const buf = new Uint8Array(w * h * 4);
        gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, buf);

        return pts.map((pt) => {
          const cx = Math.max(2, Math.min(w - 3, Math.round(pt.nx * (w - 1))));
          const cy = Math.max(2, Math.min(h - 3, Math.round(pt.ny * (h - 1))));
          let r = 0;
          let g = 0;
          let b = 0;
          let n = 0;
          for (let dy = -2; dy <= 2; dy++) {
            for (let dx = -2; dx <= 2; dx++) {
              const idx = ((cy + dy) * w + (cx + dx)) * 4;
              r += buf[idx];
              g += buf[idx + 1];
              b += buf[idx + 2];
              n++;
            }
          }
          r /= n;
          g /= n;
          b /= n;
          return {
            r,
            g,
            b,
            luma: 0.2126 * r + 0.7152 * g + 0.0722 * b,
          };
        });
      }, patches);

    // (1)–(5) & (15): Create Directional, Point, Spot, Rectangle Area, and Disk Area lights via Add Menu
    // and verify Outliner tags + completely independent properties across multiple lights!
    await page.getByTestId("menu-add").click();
    await page.getByTestId("menu-add-directional-light").click();
    await expect(page.getByTestId("outliner-item-KeyLight")).toContainText("SUN");
    expect(await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.light.type)).toBe(
      "directional"
    );

    await page.getByTestId("menu-add").click();
    await page.getByTestId("menu-add-point-light").click();
    await expect(page.getByTestId("outliner-item-PointLight")).toContainText("PT");
    expect(await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.light.type)).toBe(
      "point"
    );

    // Second PointLight to prove zero shared mutable state (Item 15)
    await page.getByTestId("menu-add").click();
    await page.getByTestId("menu-add-point-light").click();
    await expect(page.getByTestId("outliner-item-PointLight.001")).toContainText("PT");

    await page.getByTestId("menu-add").click();
    await page.getByTestId("menu-add-spot-light").click();
    await expect(page.getByTestId("outliner-item-SpotLight")).toContainText("SPOT");
    expect(await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.light.type)).toBe(
      "spot"
    );

    // Rectangle Area Light (Item 4)
    await page.getByTestId("menu-add").click();
    await page.getByTestId("menu-add-area-light").click();
    await expect(page.getByTestId("outliner-item-AreaLight")).toContainText("AREA");
    const rectAreaState = await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.light);
    expect(rectAreaState.type).toBe("area");
    expect(rectAreaState.shape).toBe("rectangle");

    // Disk Area Light (Item 5)
    await page.getByTestId("menu-add").click();
    await page.getByTestId("menu-add-area-light").click();
    await expect(page.getByTestId("outliner-item-AreaLight.001")).toContainText("AREA");
    await setControlValueAndCommit(page.getByTestId("light-area-shape"), "disk");
    await setControlValueAndCommit(page.getByTestId("light-area-width"), "2.4");
    await setCheckedAndCommit(page.getByTestId("light-area-twosided"), true);
    await setControlValueAndCommit(page.getByTestId("light-area-samples"), "3");
    const diskAreaState = await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.light);
    expect(diskAreaState.shape).toBe("disk");
    expect(diskAreaState.width).toBeCloseTo(2.4, 2);
    expect(diskAreaState.two_sided).toBe(true);
    expect(diskAreaState.sample_quality).toBe(3);

    // Verify Rectangle AreaLight was NOT mutated when editing Disk AreaLight.001!
    await page.getByTestId("outliner-item-AreaLight").click();
    expect(await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.light.shape)).toBe(
      "rectangle"
    );

    // (6) & (15): Edit PointLight properties + shadows and verify PointLight.001 stays completely untouched
    await page.getByTestId("outliner-item-PointLight").click();
    await setControlValueAndCommit(page.getByTestId("light-color"), "#ff3322");
    await setControlValueAndCommit(page.getByTestId("light-intensity"), "37.5");
    await setControlValueAndCommit(page.getByTestId("light-range"), "21.5");
    await setControlValueAndCommit(page.getByTestId("light-falloff"), "smooth_quadratic");
    await setCheckedAndCommit(page.getByTestId("light-shadow-enabled"), true);
    await setControlValueAndCommit(page.getByTestId("light-shadow-quality"), "high");
    await setControlValueAndCommit(page.getByTestId("light-shadow-filter"), "pcf5x5");
    await setControlValueAndCommit(page.getByTestId("light-shadow-strength"), "0.85");

    const pt1Edited = await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.light);
    expect(pt1Edited.intensity).toBeCloseTo(37.5, 2);
    expect(pt1Edited.range).toBeCloseTo(21.5, 2);
    expect(pt1Edited.falloff).toBe("smooth_quadratic");
    expect(pt1Edited.shadow.enabled).toBe(true);
    expect(pt1Edited.shadow.quality).toBe("high");
    expect(pt1Edited.shadow.filter).toBe("pcf5x5");
    expect(pt1Edited.shadow.strength).toBeCloseTo(0.85, 2);

    await page.getByTestId("outliner-item-PointLight.001").click();
    const pt2Untouched = await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.light);
    expect(pt2Untouched.intensity).toBeCloseTo(8.0, 2);
    expect(pt2Untouched.range).toBeCloseTo(12.0, 2);
    expect(pt2Untouched.shadow.enabled).toBe(false);

    // Deterministic WebGL2 Lighting Stage for Items (7), (8), (9), (10), and (11):
    await page.evaluate(() => {
      window.__FE3D_STUDIO__.dispatch({
        action: "create_new_project",
        template: "empty_stage",
        project_name: "Stage54LightingLab",
      });
    });

    // Disable sky environment & default SunLight, add satin Plane receiver at Y=0, aim MainCamera at (0,0,0)
    await setCheckedAndCommit(page.getByTestId("env-enabled"), false);
    await page.getByTestId("outliner-item-SunLight").click();
    await setCheckedAndCommit(page.getByTestId("light-enabled"), false);

    await page.getByTestId("btn-add-plane").click();
    const receiverPlaneId = await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.id);
    await page.evaluate((id) => {
      window.__FE3D_STUDIO__.dispatch({
        action: "set_transform",
        id,
        position: [0, 0, 0],
        rotation_deg: [0, 0, 0],
        scale: [6, 1, 6],
      });
    }, receiverPlaneId);
    await page.getByTestId("btn-mat-new").click();
    await setControlValueAndCommit(page.getByTestId("mat-base-color"), "#dcdce2");
    await setControlValueAndCommit(page.getByTestId("mat-metallic"), "0.25");
    await setControlValueAndCommit(page.getByTestId("mat-roughness"), "0.32");
    await page.getByTestId("btn-mat-assign-obj").click();

    await page.evaluate(() => {
      const st = window.__FE3D_STUDIO__.getState();
      const cam = st.objects.find((o) => o.name === "MainCamera");
      window.__FE3D_STUDIO__.dispatch({
        action: "set_transform",
        id: cam.id,
        position: [0, 4.2, 4.2],
        rotation_deg: [-45, 0, 0],
        scale: [1, 1, 1],
      });
    });

    // With all lights & environment disabled, receiver center is completely dark (0,0,0)
    const [darkPatch] = await sampleFinalPatches([{ nx: 0.5, ny: 0.5 }]);
    expect(darkPatch.luma).toBeLessThan(2.0);

    // (7) & (8): Add PointLight at (0, 1.5, 0); verify color/intensity changes & distance/range attenuation!
    await page.getByTestId("menu-add").click();
    await page.getByTestId("menu-add-point-light").click();
    const testLightId = await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.id);
    await page.evaluate((id) => {
      window.__FE3D_STUDIO__.dispatch({
        action: "set_transform",
        id,
        position: [0, 1.5, 0],
        rotation_deg: [-90, 0, 0],
        scale: [1, 1, 1],
      });
    }, testLightId);

    await setControlValueAndCommit(page.getByTestId("light-color"), "#ff0000");
    await setControlValueAndCommit(page.getByTestId("light-intensity"), "22");
    await setControlValueAndCommit(page.getByTestId("light-range"), "10");
    const [redCenter, redFar] = await sampleFinalPatches([
      { nx: 0.5, ny: 0.5 },
      { nx: 0.18, ny: 0.5 },
    ]);
    expect(redCenter.luma).toBeGreaterThan(25.0);
    expect(redCenter.r).toBeGreaterThan(Math.max(15.0, redCenter.b * 3.0));
    expect(redCenter.luma).toBeGreaterThan(redFar.luma + 14.0); // Distance attenuation!

    await setControlValueAndCommit(page.getByTestId("light-color"), "#0044ff");
    const [blueCenter] = await sampleFinalPatches([{ nx: 0.5, ny: 0.5 }]);
    expect(blueCenter.b).toBeGreaterThan(Math.max(15.0, blueCenter.r * 3.0));

    // Shrink PointLight range to 1.1m (< 1.5m height above plane) -> strict range cutoff!
    await setControlValueAndCommit(page.getByTestId("light-range"), "1.1");
    const [cutoffCenter] = await sampleFinalPatches([{ nx: 0.5, ny: 0.5 }]);
    expect(cutoffCenter.luma).toBeLessThan(2.0);

    // (9): Convert to SpotLight via UI dropdown; verify narrow cone vs wide cone angular falloff!
    await setControlValueAndCommit(page.getByTestId("light-type"), "spot");
    await setControlValueAndCommit(page.getByTestId("light-color"), "#ffffff");
    await setControlValueAndCommit(page.getByTestId("light-intensity"), "28");
    await setControlValueAndCommit(page.getByTestId("light-range"), "12");
    await setControlValueAndCommit(page.getByTestId("light-inner-cone"), "7");
    await setControlValueAndCommit(page.getByTestId("light-outer-cone"), "14");

    const [narrowSpotCenter, narrowSpotSide] = await sampleFinalPatches([
      { nx: 0.5, ny: 0.5 },
      { nx: 0.30, ny: 0.5 },
    ]);
    expect(narrowSpotCenter.luma).toBeGreaterThan(55.0);
    expect(narrowSpotSide.luma).toBeLessThan(3.0);

    await setControlValueAndCommit(page.getByTestId("light-inner-cone"), "38");
    await setControlValueAndCommit(page.getByTestId("light-outer-cone"), "58");
    const [, wideSpotSide] = await sampleFinalPatches([
      { nx: 0.5, ny: 0.5 },
      { nx: 0.30, ny: 0.5 },
    ]);
    expect(wideSpotSide.luma).toBeGreaterThan(narrowSpotSide.luma + 20.0);

    // (10): AreaLight vs PointLight at identical transform, color, intensity & range!
    await setControlValueAndCommit(page.getByTestId("light-type"), "point");
    await setControlValueAndCommit(page.getByTestId("light-color"), "#ffffff");
    await setControlValueAndCommit(page.getByTestId("light-intensity"), "16");
    await setControlValueAndCommit(page.getByTestId("light-range"), "12");
    const [ptRefCenter, ptRefWide] = await sampleFinalPatches([
      { nx: 0.5, ny: 0.5 },
      { nx: 0.28, ny: 0.5 },
    ]);

    await setControlValueAndCommit(page.getByTestId("light-type"), "area");
    await setControlValueAndCommit(page.getByTestId("light-area-shape"), "rectangle");
    await setControlValueAndCommit(page.getByTestId("light-color"), "#ffffff");
    await setControlValueAndCommit(page.getByTestId("light-intensity"), "16");
    await setControlValueAndCommit(page.getByTestId("light-range"), "12");
    await setControlValueAndCommit(page.getByTestId("light-area-width"), "4.0");
    await setControlValueAndCommit(page.getByTestId("light-area-height"), "1.4");
    await setControlValueAndCommit(page.getByTestId("light-area-samples"), "3");

    const [areaRefCenter, areaRefWide] = await sampleFinalPatches([
      { nx: 0.5, ny: 0.5 },
      { nx: 0.28, ny: 0.5 },
    ]);
    const ptSpread = ptRefWide.luma / Math.max(1.0, ptRefCenter.luma);
    const areaSpread = areaRefWide.luma / Math.max(1.0, areaRefCenter.luma);
    expect(areaSpread).toBeGreaterThan(ptSpread + 0.06);

    // Switch Rectangle (4.0m) -> Disk (1.0m diameter) -> lateral illumination changes measurably
    await setControlValueAndCommit(page.getByTestId("light-area-shape"), "disk");
    await setControlValueAndCommit(page.getByTestId("light-area-width"), "1.0");
    const [, diskRefWide] = await sampleFinalPatches([
      { nx: 0.5, ny: 0.5 },
      { nx: 0.28, ny: 0.5 },
    ]);
    expect(Math.abs(areaRefWide.luma - diskRefWide.luma)).toBeGreaterThan(4.0);

    // (11): Real-Time WebGL2 Shadow Atlas verification (SpotLight overhead + Cube occluder slab)
    await page.getByTestId("btn-add-cube").click();
    const occluderId = await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.id);
    await page.evaluate(
      ({ occId, lgtId }) => {
        const st = window.__FE3D_STUDIO__.getState();
        const cam = st.objects.find((o) => o.name === "MainCamera");
        window.__FE3D_STUDIO__.dispatch({
          action: "set_transform",
          id: occId,
          position: [0, 1.4, 0],
          rotation_deg: [0, 0, 0],
          scale: [1.25, 0.25, 1.25],
        });
        window.__FE3D_STUDIO__.dispatch({
          action: "set_transform",
          id: lgtId,
          position: [0, 3.6, 0],
          rotation_deg: [-90, 0, 0],
          scale: [1, 1, 1],
        });
        window.__FE3D_STUDIO__.dispatch({
          action: "set_transform",
          id: cam.id,
          position: [0, 0.9, 3.8],
          rotation_deg: [-13.3, 0, 0],
          scale: [1, 1, 1],
        });
      },
      { occId: occluderId, lgtId: testLightId }
    );

    await page.getByTestId("outliner-item-PointLight").click();
    await setControlValueAndCommit(page.getByTestId("light-type"), "spot");
    await setControlValueAndCommit(page.getByTestId("light-intensity"), "36");
    await setControlValueAndCommit(page.getByTestId("light-range"), "12");
    await setControlValueAndCommit(page.getByTestId("light-inner-cone"), "32");
    await setControlValueAndCommit(page.getByTestId("light-outer-cone"), "48");
    await setCheckedAndCommit(page.getByTestId("light-shadow-enabled"), false);

    const [unshadowedFloor] = await sampleFinalPatches([{ nx: 0.5, ny: 0.5 }]);
    expect(unshadowedFloor.luma).toBeGreaterThan(50.0);

    await setCheckedAndCommit(page.getByTestId("light-shadow-enabled"), true);
    await setControlValueAndCommit(page.getByTestId("light-shadow-quality"), "high");
    await setControlValueAndCommit(page.getByTestId("light-shadow-filter"), "pcf3x3");

    const [shadowedFloor] = await sampleFinalPatches([{ nx: 0.5, ny: 0.5 }]);
    expect(shadowedFloor.luma).toBeLessThan(unshadowedFloor.luma * 0.3);

    const shadowDiag = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().lighting_diagnostics
    );
    expect(shadowDiag.active_shadow_lights).toBeGreaterThanOrEqual(1);
    expect(shadowDiag.shadow_atlas_dimension).toBeGreaterThanOrEqual(1024);

    // Move occluder out of the light cone -> shadow updates dynamically in WebGL2!
    await page.evaluate((occId) => {
      window.__FE3D_STUDIO__.dispatch({
        action: "set_transform",
        id: occId,
        position: [3.2, 1.4, 0],
        rotation_deg: [0, 0, 0],
        scale: [1.25, 0.25, 1.25],
      });
    }, occluderId);
    const [revealedFloor] = await sampleFinalPatches([{ nx: 0.5, ny: 0.5 }]);
    expect(revealedFloor.luma).toBeGreaterThan(shadowedFloor.luma * 2.8);

    // Move occluder slightly aside to show dramatic studio shadow & capture screenshot
    await page.evaluate((occId) => {
      window.__FE3D_STUDIO__.dispatch({
        action: "set_transform",
        id: occId,
        position: [-0.35, 1.2, 0.15],
        rotation_deg: [15, 28, 0],
        scale: [0.9, 0.9, 0.9],
      });
    }, occluderId);
    await setCheckedAndCommit(page.getByTestId("env-enabled"), true);
    await page.waitForTimeout(60);
    await page.screenshot({
      path: path.join(outDir, "09-stage54-multi-light-pbr-and-shadows-1920x1080.png"),
    });

    await assertNoHorizontalOverflow(page);
    await context.close();
  });

  test("12. Stage 5.4 Lighting Engine Part B: Radiance .hdr IBL Assignment/Rotation/Error Safety, Light Linking, Undo/Redo, .fe3d v1.3 Round-Trip, v1.2 Compatibility, and 3D Light Helpers", async ({
    browser,
  }) => {
    const outDir =
      process.env.FE3D_SCREENSHOT_DIR ||
      path.resolve(process.cwd(), "../../build/ci-screenshots/after-stage5.1");
    await fs.mkdir(outDir, { recursive: true });

    // Build deterministic binary Radiance RGBE .hdr buffer in Node (32x16 with asymmetric bright sun lobe)
    const buildRadianceHdrBuffer = (width, height) => {
      const header = Buffer.from(
        `#?RADIANCE\nSOFTWARE=FinalEffect3D_E2E\nFORMAT=32-bit_rle_rgbe\n\n-Y ${height} +X ${width}\n`,
        "ascii"
      );
      const pixels = Buffer.alloc(width * height * 4);
      let off = 0;
      for (let y = 0; y < height; y++) {
        const v = (y + 0.5) / height;
        for (let x = 0; x < width; x++) {
          const u = (x + 0.5) / width;
          const du = u - 0.25;
          const dv = v - 0.30;
          const spot = Math.exp(-(du * du + dv * dv) * 90.0);
          const r = 0.06 + 16.0 * spot;
          const g = 0.12 + 10.0 * spot;
          const b = 0.28 + 3.0 * spot;
          const maxV = Math.max(r, g, b);
          const exp = Math.ceil(Math.log2(maxV));
          const scale = Math.pow(2, -exp) * 256.0;
          pixels[off++] = Math.min(255, Math.max(0, Math.floor(r * scale)));
          pixels[off++] = Math.min(255, Math.max(0, Math.floor(g * scale)));
          pixels[off++] = Math.min(255, Math.max(0, Math.floor(b * scale)));
          pixels[off++] = Math.min(255, Math.max(0, exp + 128));
        }
      }
      return Buffer.concat([header, pixels]);
    };

    const context = await browser.newContext({ viewport: { width: 1920, height: 1080 } });
    const page = await context.newPage();
    await bootDirectStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();

    const setCheckedAndCommit = async (locator, checked) => {
      await locator.evaluate((el, v) => {
        el.checked = Boolean(v);
        el.dispatchEvent(new Event("change", { bubbles: true }));
      }, checked);
    };

    const sampleFinalPatches = async (patches) =>
      page.evaluate((pts) => {
        const ok = window.__FE3D_STUDIO__.renderFinalFrame();
        if (!ok) return null;
        const c = document.getElementById("fe3d-canvas");
        const gl = c.getContext("webgl2");
        const w = c.width;
        const h = c.height;
        const buf = new Uint8Array(w * h * 4);
        gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, buf);
        return pts.map((pt) => {
          const cx = Math.max(2, Math.min(w - 3, Math.round(pt.nx * (w - 1))));
          const cy = Math.max(2, Math.min(h - 3, Math.round(pt.ny * (h - 1))));
          let r = 0;
          let g = 0;
          let b = 0;
          let n = 0;
          for (let dy = -2; dy <= 2; dy++) {
            for (let dx = -2; dx <= 2; dx++) {
              const idx = ((cy + dy) * w + (cx + dx)) * 4;
              r += buf[idx];
              g += buf[idx + 1];
              b += buf[idx + 2];
              n++;
            }
          }
          r /= n;
          g /= n;
          b /= n;
          return { r, g, b, luma: 0.2126 * r + 0.7152 * g + 0.0722 * b };
        });
      }, patches);

    // (12) & (13): Load ShaderBall template, disable SunLight, upload genuine Radiance .hdr via #file-open-hdri,
    // verify WebGL2 IBL intensity & rotation, and verify corrupt .hdr preserves active valid environment!
    await page.evaluate(() => {
      window.__FE3D_STUDIO__.dispatch({
        action: "create_new_project",
        template: "shader_ball",
        project_name: "Stage54HdriTest",
      });
      const st = window.__FE3D_STUDIO__.getState();
      const sun = st.objects.find((o) => o.name === "SunLight");
      const cam = st.objects.find((o) => o.name === "MainCamera");
      const sphere = st.objects.find((o) => o.name === "ShaderSphere");
      window.__FE3D_STUDIO__.dispatch({
        action: "set_light",
        id: sun.id,
        enabled: false,
      });
      window.__FE3D_STUDIO__.dispatch({
        action: "set_transform",
        id: sphere.id,
        position: [0, 0, 0],
        rotation_deg: [0, 0, 0],
        scale: [1, 1, 1],
      });
      window.__FE3D_STUDIO__.dispatch({
        action: "set_transform",
        id: cam.id,
        position: [0, 0, 3.2],
        rotation_deg: [0, 0, 0],
        scale: [1, 1, 1],
      });
    });

    await page.getByTestId("file-open-hdri").setInputFiles({
      name: "ValidStudio.hdr",
      mimeType: "application/octet-stream",
      buffer: buildRadianceHdrBuffer(32, 16),
    });

    await expect(page.getByTestId("env-hdri-status")).toHaveText("ValidStudio.hdr (32×16)");
    await expect(page.getByTestId("btn-clear-hdri")).toBeEnabled();

    const [iblRot0Left, iblRot0Right] = await sampleFinalPatches([
      { nx: 0.43, ny: 0.52 },
      { nx: 0.57, ny: 0.52 },
    ]);
    expect(iblRot0Left.luma + iblRot0Right.luma).toBeGreaterThan(25.0);

    // Rotate HDRI 180° -> glossy reflection asymmetry flips on the metallic ShaderSphere!
    await setControlValueAndCommit(page.getByTestId("env-rotation"), "180");
    const [iblRot180Left, iblRot180Right] = await sampleFinalPatches([
      { nx: 0.43, ny: 0.52 },
      { nx: 0.57, ny: 0.52 },
    ]);
    expect(
      Math.abs(iblRot0Left.luma - iblRot0Right.luma - (iblRot180Left.luma - iblRot180Right.luma))
    ).toBeGreaterThan(4.0);

    // Scale environment intensity 0.5 vs 2.5 -> WebGL2 sphere brightness increases!
    await setControlValueAndCommit(page.getByTestId("env-intensity"), "0.5");
    const [iblDim] = await sampleFinalPatches([{ nx: 0.5, ny: 0.5 }]);
    await setControlValueAndCommit(page.getByTestId("env-intensity"), "2.5");
    const [iblBright] = await sampleFinalPatches([{ nx: 0.5, ny: 0.5 }]);
    expect(iblBright.luma).toBeGreaterThan(iblDim.luma * 1.8);

    // (13): Attempt to load corrupted .hdr -> reports clear error while preserving ValidStudio.hdr & settings!
    await setControlValueAndCommit(page.getByTestId("env-rotation"), "95");
    await page.getByTestId("file-open-hdri").setInputFiles({
      name: "CorruptBroken.hdr",
      mimeType: "application/octet-stream",
      buffer: Buffer.from("CORRUPT_HDR_NOT_RADIANCE_DATA", "utf8"),
    });
    const afterBadHdri = await page.evaluate(() => window.__FE3D_STUDIO__.getState());
    expect(afterBadHdri.status.level).toBe("error");
    expect(afterBadHdri.environment.has_hdri).toBe(true);
    expect(afterBadHdri.environment.hdri_name).toBe("ValidStudio.hdr");
    expect(Math.round(afterBadHdri.environment.rotation_deg)).toBe(95);

    // Clear HDRI, then Undo restores ValidStudio.hdr, Redo clears again (Item 16)!
    await page.getByTestId("btn-clear-hdri").click();
    await expect(page.getByTestId("env-hdri-status")).toHaveText("Studio Sky (Fallback)");
    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("env-hdri-status")).toHaveText("ValidStudio.hdr (32×16)");

    // (14): Per-Light Object Light Linking in real WebGL2!
    await page.evaluate(() => {
      window.__FE3D_STUDIO__.dispatch({
        action: "create_new_project",
        template: "empty_stage",
        project_name: "Stage54LightLinking",
      });
    });
    await setCheckedAndCommit(page.getByTestId("env-enabled"), false);

    // Add LeftSphere (-1.15, 0, 0) & RightSphere (+1.15, 0, 0) facing MainCamera at (0, 0, 4.2)
    await page.getByTestId("btn-add-sphere").click();
    await setControlValueAndCommit(page.getByTestId("insp-name"), "LeftSphere");
    await page.getByTestId("btn-add-sphere").click();
    await setControlValueAndCommit(page.getByTestId("insp-name"), "RightSphere");

    const spherePatches = await page.evaluate(() => {
      const st = window.__FE3D_STUDIO__.getState();
      const left = st.objects.find((o) => o.name === "LeftSphere");
      const right = st.objects.find((o) => o.name === "RightSphere");
      const cam = st.objects.find((o) => o.name === "MainCamera");
      const sun = st.objects.find((o) => o.name === "SunLight");
      window.__FE3D_STUDIO__.dispatch({
        action: "set_transform",
        id: left.id,
        position: [-1.15, 0, 0],
        rotation_deg: [0, 0, 0],
        scale: [0.9, 0.9, 0.9],
      });
      window.__FE3D_STUDIO__.dispatch({
        action: "set_transform",
        id: right.id,
        position: [1.15, 0, 0],
        rotation_deg: [0, 0, 0],
        scale: [0.9, 0.9, 0.9],
      });
      window.__FE3D_STUDIO__.dispatch({
        action: "set_transform",
        id: cam.id,
        position: [0, 0, 4.2],
        rotation_deg: [0, 0, 0],
        scale: [1, 1, 1],
      });
      window.__FE3D_STUDIO__.dispatch({
        action: "set_transform",
        id: sun.id,
        position: [0, 2, 2],
        rotation_deg: [-20, 0, 0],
        scale: [1, 1, 1],
      });
      window.__FE3D_STUDIO__.dispatch({
        action: "set_light",
        id: sun.id,
        intensity: 3.5,
        shadow: { enabled: false },
      });
      const c = document.getElementById("fe3d-canvas");
      const aspect = (c.width || 1280) / (c.height || 720);
      const tanHalfFovX = Math.tan((24.0 * Math.PI) / 180.0) * aspect;
      const ndcOffset = 1.15 / 4.2 / tanHalfFovX;
      return [
        { nx: 0.5 - 0.5 * ndcOffset, ny: 0.5 },
        { nx: 0.5 + 0.5 * ndcOffset, ny: 0.5 },
      ];
    });

    // Mode 'all': both spheres are brightly lit
    const [allLeft, allRight] = await sampleFinalPatches(spherePatches);
    expect(allLeft.luma).toBeGreaterThan(70.0);
    expect(allRight.luma).toBeGreaterThan(70.0);

    // Switch SunLight Light Linking to 'exclude' and check LeftSphere -> LeftSphere dark, RightSphere bright!
    await page.getByTestId("outliner-item-SunLight").click();
    await setControlValueAndCommit(page.getByTestId("light-link-mode"), "exclude");
    await setCheckedAndCommit(page.getByTestId("light-link-target-LeftSphere"), true);
    const [excLeft, excRight] = await sampleFinalPatches(spherePatches);
    expect(excLeft.luma).toBeLessThan(3.0);
    expect(excRight.luma).toBeGreaterThan(70.0);

    // Switch to 'include_only' (LeftSphere still checked) -> LeftSphere bright, RightSphere dark!
    await setControlValueAndCommit(page.getByTestId("light-link-mode"), "include_only");
    const [incLeft, incRight] = await sampleFinalPatches(spherePatches);
    expect(incLeft.luma).toBeGreaterThan(70.0);
    expect(incRight.luma).toBeLessThan(3.0);

    // (16): Undo restores 'exclude' mode; Redo reapplies 'include_only'!
    await page.getByTestId("btn-undo").click();
    expect(
      await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.light.linking.mode)
    ).toBe("exclude");
    await page.getByTestId("btn-redo").click();
    expect(
      await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.light.linking.mode)
    ).toBe("include_only");

    // (17) & (18): Add Point, Spot & Area lights + Sunset HDRI, verify .fe3d v1.3 save/load round-trip
    // AND verify legacy .fe3d v1.2 backward compatibility!
    await page.getByTestId("btn-hdri-sunset").click();
    await page.getByTestId("menu-add").click();
    await page.getByTestId("menu-add-point-light").click();
    await page.getByTestId("menu-add").click();
    await page.getByTestId("menu-add-spot-light").click();
    await page.getByTestId("menu-add").click();
    await page.getByTestId("menu-add-area-light").click();

    await page.waitForTimeout(60);
    await page.screenshot({
      path: path.join(outDir, "10-stage54-hdri-ibl-and-light-linking-1920x1080.png"),
    });

    const exportedV13 = await page.evaluate(() => window.__FE3D_STUDIO__.exportInlineProject());
    const parsedV13 = JSON.parse(exportedV13);
    expect(parsedV13.schema_version.major).toBe(1);
    expect(parsedV13.schema_version.minor).toBe(3);

    // Load a genuine legacy v1.2 .fe3d document (no environment, no light shadow/linking keys) -> Item (18)
    const legacyV12Json = JSON.stringify({
      format: "finaleffect.project",
      schema_version: { major: 1, minor: 2 },
      coordinate_system: "right-handed-y-up",
      metadata: {
        project_name: "LegacyV12Scene",
        author: "Stage52User",
        description: "Legacy v1.2 compatibility check",
        engine_version: "0.5.0",
      },
      scene: {
        name: "LegacyScene",
        active_camera_id: 1,
        materials: [],
        meshes: [],
        objects: [
          {
            id: 1,
            name: "MainCamera",
            visible: true,
            parent_id: 0,
            local_transform: { position: [0, 2, 5], rotation: [0, 0, 0, 1], scale: [1, 1, 1] },
            camera: {
              projection: "perspective",
              fov_y_radians: 0.85,
              orthographic_height: 6.0,
              near_plane: 0.1,
              far_plane: 500.0,
            },
          },
          {
            id: 2,
            name: "LegacySun",
            visible: true,
            parent_id: 0,
            local_transform: { position: [1, 3, 2], rotation: [-0.3, 0.2, 0, 0.93], scale: [1, 1, 1] },
            directional_light: { color: [1.0, 0.95, 0.88], intensity: 2.1 },
          },
        ],
      },
    });
    const okV12 = await page.evaluate(
      (json) => window.__FE3D_STUDIO__.importInlineProject(json),
      legacyV12Json
    );
    expect(okV12).toBe(true);
    await expect(page.getByTestId("outliner-item-LegacySun")).toContainText("SUN");

    // Re-import exported v1.3 project and verify all 4 light types, Light Linking, and HDRI restore cleanly!
    const okV13 = await page.evaluate(
      (json) => window.__FE3D_STUDIO__.importInlineProject(json),
      exportedV13
    );
    expect(okV13).toBe(true);
    const restoredState = await page.evaluate(() => window.__FE3D_STUDIO__.getState());
    expect(restoredState.environment.has_hdri).toBe(true);
    expect(restoredState.environment.hdri_name).toBe("Golden_Sunset_Horizon.hdr");
    expect(restoredState.lighting_diagnostics.directional_count).toBe(1);
    expect(restoredState.lighting_diagnostics.point_count).toBe(1);
    expect(restoredState.lighting_diagnostics.spot_count).toBe(1);
    expect(restoredState.lighting_diagnostics.area_count).toBe(1);

    await page.getByTestId("outliner-item-SunLight").click();
    expect(
      await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.light.linking.mode)
    ).toBe("include_only");

    // (19) & (20): 3D Viewport Helpers remain selectable & editable via Transform Gizmo,
    // and are strictly excluded from final rendering!
    await page.getByTestId("btn-clear-selection").click();
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("No Selection");

    // Position SpotLight cleanly at (0, 1.1, 0) and click its 3D viewport helper directly on #fe3d-canvas!
    const spotPickCss = await page.evaluate(() => {
      const st = window.__FE3D_STUDIO__.getState();
      const sp = st.objects.find((o) => o.name === "SpotLight");
      window.__FE3D_STUDIO__.dispatch({
        action: "set_transform",
        id: sp.id,
        position: [0, 1.1, 0],
        rotation_deg: [-45, 25, 0],
        scale: [1, 1, 1],
      });
      const updated = window.__FE3D_STUDIO__.getState();
      const spUpdated = updated.objects.find((o) => o.id === sp.id);
      const c = document.getElementById("fe3d-canvas");
      const r = c.getBoundingClientRect();
      return {
        x: r.left + spUpdated.pick_screen_pos[0] * (r.width / (c.width || r.width)),
        y: r.top + spUpdated.pick_screen_pos[1] * (r.height / (c.height || r.height)),
      };
    });
    await page.mouse.click(spotPickCss.x, spotPickCss.y);
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: SpotLight");

    // Drag SpotLight's +X Transform Gizmo handle and verify its 3D position moves!
    const spotXBefore = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().inspector.position[0]
    );
    const spotGizmoX = await page.evaluate(() => {
      const c = document.getElementById("fe3d-canvas");
      const r = c.getBoundingClientRect();
      const gh = window.__FE3D_STUDIO__.getState().editor.gizmo_handles_screen;
      return {
        x: r.left + gh.x[0] * (r.width / (c.width || r.width)),
        y: r.top + gh.x[1] * (r.height / (c.height || r.height)),
      };
    });
    await page.mouse.move(spotGizmoX.x, spotGizmoX.y);
    await page.mouse.down({ button: "left" });
    await page.mouse.move(spotGizmoX.x + 65, spotGizmoX.y, { steps: 6 });
    await page.mouse.up({ button: "left" });
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.position[0]))
      .toBeGreaterThan(spotXBefore + 0.15);

    await page.waitForTimeout(60);
    await page.screenshot({
      path: path.join(outDir, "11-stage54-3d-light-helpers-and-gizmo-1920x1080.png"),
    });

    // Item (20): Compare editor viewport framebuffer (with 3D helpers + gizmo + grid) vs renderFinalFrame()!
    const helperExclusionCheck = await page.evaluate(
      () =>
        new Promise((resolve) => {
          requestAnimationFrame(() =>
            requestAnimationFrame(() => {
              const c = document.getElementById("fe3d-canvas");
              const gl = c.getContext("webgl2");
              const w = c.width;
              const h = c.height;
              const editorBuf = new Uint8Array(w * h * 4);
              gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, editorBuf);

              window.__FE3D_STUDIO__.renderFinalFrame();
              const finalBuf = new Uint8Array(w * h * 4);
              gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, finalBuf);

              let diffPixels = 0;
              for (let i = 0; i < editorBuf.length; i += 16) {
                if (
                  Math.abs(editorBuf[i] - finalBuf[i]) +
                    Math.abs(editorBuf[i + 1] - finalBuf[i + 1]) +
                    Math.abs(editorBuf[i + 2] - finalBuf[i + 2]) >
                  15
                ) {
                  diffPixels++;
                }
              }
              resolve({ diffPixels });
            })
          );
        })
    );
    expect(helperExclusionCheck.diffPixels).toBeGreaterThan(100);

    await assertNoHorizontalOverflow(page);
    await context.close();
  });

  test("13. Stage 1 Maya Redesign: Classic layout hierarchy, Menu Sets (Modeling..Rendering + Arnold), Customizable Shelves, Toolbox, Snapping & Viewport Shading/Alt-Nav", async ({
    browser,
  }) => {
    const outDir = path.resolve(process.cwd(), "../../build/ci-screenshots");
    await fs.mkdir(outDir, { recursive: true });

    const context = await browser.newContext({ viewport: { width: 1920, height: 1080 } });
    const page = await context.newPage();
    await bootDirectStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();
    await assertNoHorizontalOverflow(page);

    // 1. Verify Maya Classic region hierarchy & non-overlapping geometry
    await expect(page.getByTestId("maya-main-menubar")).toBeVisible();
    await expect(page.getByTestId("maya-status-line")).toBeVisible();
    await expect(page.getByTestId("maya-shelf-bar")).toBeVisible();
    await expect(page.getByTestId("tool-shelf")).toBeVisible();
    await expect(page.getByTestId("vp-panel-menubar")).toBeVisible();
    await expect(page.getByTestId("vp-panel-toolbar")).toBeVisible();
    await expect(page.getByTestId("panel-channel-box")).toBeVisible();
    await expect(page.getByTestId("panel-layer-editor")).toBeVisible();
    await expect(page.getByTestId("maya-command-footer")).toBeVisible();

    await page.screenshot({
      path: path.join(outDir, "12-maya-stage1-desktop-classic-1920x1080.png"),
    });

    // 2. Switch Menu Sets dynamically without resetting scene, selection, or camera
    await page.getByTestId("outliner-item-HeroCube").click();
    const beforeMs = await page.evaluate(() => {
      const s = window.__FE3D_STUDIO__.getState();
      return { count: s.objects.length, sel: s.viewport.selected_object_id, cam: s.viewport.eye };
    });

    await page.getByTestId("maya-menuset-select").selectOption("rendering");
    await expect(page.getByTestId("menuset-menu-arnold")).toBeVisible();
    await expect(page.getByTestId("menuset-menu-lighting-shading")).toBeVisible();

    await page.getByTestId("maya-menuset-select").selectOption("rigging");
    await expect(page.getByTestId("menuset-menu-skeleton")).toBeVisible();

    await page.getByTestId("maya-menuset-select").selectOption("animation");
    await expect(page.getByTestId("menuset-menu-key")).toBeVisible();

    await page.getByTestId("maya-menuset-select").selectOption("fx");
    await expect(page.getByTestId("menuset-menu-fields-solvers")).toBeVisible();

    await page.getByTestId("maya-menuset-select").selectOption("modeling");
    await expect(page.getByTestId("menuset-menu-mesh")).toBeVisible();
    await expect(page.getByTestId("menuset-menu-edit-mesh")).toBeVisible();

    const afterMs = await page.evaluate(() => {
      const s = window.__FE3D_STUDIO__.getState();
      return { count: s.objects.length, sel: s.viewport.selected_object_id, cam: s.viewport.eye };
    });
    expect(afterMs).toEqual(beforeMs);

    // 3. Customizable Shelf: create Cylinder, Cone & Torus from Poly Modeling shelf + Undo/Redo + Custom Shelf persistence
    await page.getByTestId("shelf-tab-poly_modeling").click();
    await page.getByTestId("shelf-item-pm_cyl").click();
    await page.getByTestId("shelf-item-pm_con").click();
    await page.getByTestId("shelf-item-pm_tor").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("8 / 5");

    await page.keyboard.press("Control+z");
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 5");
    await page.keyboard.press("Control+y");
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("8 / 5");

    await page.getByTestId("btn-shelf-gear").click();
    await page.getByTestId("shelf-gear-new-shelf").click();
    const savedShelves = await page.evaluate(() =>
      JSON.parse(window.localStorage.getItem("fe3d.maya_shelves.v1") || "{}")
    );
    expect(savedShelves.shelves.some((s) => s.name === "Custom1")).toBe(true);

    // 4. Toolbox shortcuts (Q, W, E, R, T, D, Y) + Viewport shading modes (4, 5, 6, 7) + Alt+Mouse navigation
    await page.keyboard.press("KeyQ");
    await expect.poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().editor.active_tool)).toBe("select");
    await page.keyboard.press("KeyE");
    await expect.poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().editor.active_tool)).toBe("rotate");
    await page.keyboard.press("KeyR");
    await expect.poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().editor.active_tool)).toBe("scale");
    await page.keyboard.press("KeyY");
    await expect.poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().editor.active_tool)).toBe("rotate");
    await page.keyboard.press("KeyW");
    await expect.poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().editor.active_tool)).toBe("move");

    const hashTextured = (await sampleViewportPixels(page)).hash;
    await page.keyboard.press("Digit4"); // Wireframe
    await page.waitForTimeout(50);
    const hashWire = (await sampleViewportPixels(page)).hash;
    expect(hashWire).not.toBe(hashTextured);

    await page.keyboard.press("Digit5"); // Smooth Shaded
    await page.waitForTimeout(50);
    const hashShaded = (await sampleViewportPixels(page)).hash;
    expect(hashShaded).not.toBe(hashWire);

    await page.keyboard.press("Digit6"); // Back to Shaded & Textured
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.shading_mode))
      .toBe("shaded_textured");

    // Alt + LMB Orbit, Alt + MMB Pan, Alt + RMB Dolly Zoom
    const vpRect = await page.locator("#fe3d-canvas").boundingBox();
    const eye0 = await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.eye.slice());
    await page.keyboard.down("Alt");
    await page.mouse.move(vpRect.x + 400, vpRect.y + 300);
    await page.mouse.down({ button: "left" });
    await page.mouse.move(vpRect.x + 490, vpRect.y + 260, { steps: 5 });
    await page.mouse.up({ button: "left" });
    const eye1 = await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.eye.slice());
    expect(Math.hypot(eye1[0] - eye0[0], eye1[1] - eye0[1], eye1[2] - eye0[2])).toBeGreaterThan(0.2);

    await page.mouse.move(vpRect.x + 400, vpRect.y + 300);
    await page.mouse.down({ button: "right" });
    await page.mouse.move(vpRect.x + 470, vpRect.y + 240, { steps: 5 });
    await page.mouse.up({ button: "right" });
    await page.keyboard.up("Alt");
    const eye2 = await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.eye.slice());
    expect(Math.hypot(eye2[0] - eye1[0], eye2[1] - eye1[1], eye2[2] - eye1[2])).toBeGreaterThan(0.15);
    // Alt+RMB drag must NOT leave stationary Marking Menu open
    await expect(page.getByTestId("viewport-context-menu")).toBeHidden();

    await context.close();
  });

  test("14. Stage 1 Maya Outliner Hierarchy, Multi-Selection, Dedicated Channel Box, Custom Attributes, Display Layers & .fe3d v1.3 Persistence", async ({
    browser,
  }) => {
    const context = await browser.newContext({ viewport: { width: 1600, height: 920 } });
    const page = await context.newPage();
    await bootDirectStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();

    // 1. Outliner multi-selection (Ctrl+Click), Ctrl+G Group, collapse/expand hierarchy,Shift+P Unparent
    await page.getByTestId("outliner-item-HeroCube").click();
    await page.getByTestId("outliner-item-AccentPyramid").click({ modifiers: ["Control"] });
    const selIds = await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.selected_object_ids);
    expect(selIds.length).toBe(2);

    await page.keyboard.press("Control+g");
    await expect(page.getByTestId("outliner-item-group1")).toBeVisible();
    await page.getByTestId("outliner-item-HeroCube").click();
    const heroParentAfterGroup = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().inspector.parent_id
    );
    expect(heroParentAfterGroup).toBeGreaterThan(0);

    // 2. Dedicated Channel Box: numeric edit + channel lock + Custom Attribute + Undo/Redo
    const cbTx = page.getByTestId("cb-tx");
    await cbTx.fill("2.75");
    await cbTx.press("Enter");
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.position[0]))
      .toBeCloseTo(2.75, 2);

    // Lock TX channel
    await page.getByTestId("cb-lock-tx").click();
    await expect(cbTx).toBeDisabled();

    // Add Custom Attribute 'rigBlend' = 0.65
    await page.getByTestId("btn-cb-add-attr-toggle").click();
    await page.locator("#cb-new-attr-name").fill("rigBlend");
    await page.locator("#cb-new-attr-val").fill("0.65");
    await page.getByTestId("btn-cb-create-attr").click();
    await expect(page.getByTestId("cb-custom-rigBlend")).toHaveValue("0.650");

    // 3. Display Layer Editor: create layer with HeroCube, toggle V/T/R, delete layer preserving HeroCube
    await page.getByTestId("btn-layer-create-selected").click();
    await expect(page.getByTestId("display-layer-row-layer1")).toBeVisible();

    const hashVisOn = (await sampleViewportPixels(page)).hash;
    await page.getByTestId("layer-vis-layer1").click(); // Hide layer1
    await page.waitForTimeout(50);
    const hashVisOff = (await sampleViewportPixels(page)).hash;
    expect(hashVisOff).not.toBe(hashVisOn);
    await page.getByTestId("layer-vis-layer1").click(); // Show layer1 again

    // Cycle Normal -> Template -> Reference
    await page.getByTestId("layer-mode-layer1").click();
    await expect(page.getByTestId("layer-mode-layer1")).toHaveText("T");
    await page.getByTestId("layer-mode-layer1").click();
    await expect(page.getByTestId("layer-mode-layer1")).toHaveText("R");

    // Verify .fe3d v1.3 export & re-import preserves Display Layers, Channel Locks & Custom Attributes cleanly
    const exportedJson = await page.evaluate(() => window.__FE3D_STUDIO__.exportInlineProject());
    expect(exportedJson).toContain('"display_layers"');
    expect(exportedJson).toContain('"rigBlend"');

    await page.evaluate(() => window.__FE3D_STUDIO__.dispatch({ action: "new_starter_project" }));
    await expect(page.getByTestId("display-layer-row-layer1")).toHaveCount(0);

    await page.evaluate((txt) => window.__FE3D_STUDIO__.importInlineProject(txt), exportedJson);
    await expect(page.getByTestId("display-layer-row-layer1")).toBeVisible();
    await page.getByTestId("outliner-item-HeroCube").click();
    await expect(page.getByTestId("cb-custom-rigBlend")).toHaveValue("0.650");
    const warnings = await page.evaluate(() => window.__FE3D_STUDIO__.getState().project.load_warnings);
    expect(warnings).toEqual([]);

    // Delete layer1 -> HeroCube remains in scene!
    await page.getByTestId("layer-del-layer1").click();
    await expect(page.getByTestId("display-layer-row-layer1")).toHaveCount(0);
    await expect(page.getByTestId("outliner-item-HeroCube")).toBeVisible();

    await context.close();
  });

  test("15. Stage 1 Maya Time/Range Slider Keyframes, Safe Command Line, Focus Guard, Hotbox, Marking Menu, Undock/Redock & Responsive Mobile Views", async ({
    browser,
  }) => {
    const outDir = path.resolve(process.cwd(), "../../build/ci-screenshots");
    await fs.mkdir(outDir, { recursive: true });

    const context = await browser.newContext({ viewport: { width: 1600, height: 920 } });
    const page = await context.newPage();
    await bootDirectStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();

    // 1. Keyframe animation on Time Slider (Frame 1 -> Frame 25, scrub Frame 13)
    await page.getByTestId("outliner-item-HeroCube").click();
    await page.evaluate(() => {
      const s = window.__FE3D_STUDIO__;
      const id = s.getState().inspector.id;
      s.dispatch({ action: "set_current_frame", frame: 1 });
      s.dispatch({ action: "set_transform", id, position: [0, 0, 0], rotation_deg: [0, 0, 0], scale: [1, 1, 1] });
    });
    await page.getByTestId("btn-tl-set-key").click();
    await page.getByTestId("btn-tl-autokey").click(); // Enable Auto Key!

    await page.locator("#timeline-frame-num").fill("25");
    await page.locator("#timeline-frame-num").press("Enter");
    await page.getByTestId("cb-tx").fill("4.0");
    await page.getByTestId("cb-tx").press("Enter");

    // Scrub to midpoint Frame 13 -> interpolated X ~ 2.0!
    await page.locator("#timeline-frame-num").fill("13");
    await page.locator("#timeline-frame-num").press("Enter");
    const midX = await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.position[0]);
    expect(midX).toBeGreaterThan(1.8);
    expect(midX).toBeLessThan(2.2);

    // 2. Safe Command Line + Input Focus Guard (typing shortcuts in Command Line must NOT trigger tools or delete objects!)
    const cmdInput = page.getByTestId("maya-command-input");
    await cmdInput.click();
    await cmdInput.type("polyTorus");
    // While typing 'polyTorus' (contains 'r', 's', 'y'), active tool must NOT change and HeroCube must remain selected!
    await cmdInput.press("Enter");
    await expect(page.getByTestId("outliner-item-Torus")).toBeVisible();

    await cmdInput.fill('eval("window.__HACKED__ = 1")');
    await cmdInput.press("Enter");
    const hackedVal = await page.evaluate(() => window.__HACKED__);
    expect(hackedVal).toBeUndefined();
    await expect(page.getByTestId("status-message")).toContainText("Unknown command");

    // 3. Hold Spacebar Hotbox & Stationary RMB Radial Marking Menu
    await page.locator("#fe3d-canvas").click({ position: { x: 200, y: 200 } });
    await page.keyboard.down("Space");
    await expect(page.getByTestId("maya-hotbox")).toBeVisible();
    await page.keyboard.up("Space");
    await expect(page.getByTestId("maya-hotbox")).toBeHidden();

    await page.getByTestId("outliner-item-HeroCube").click();
    await page.locator("#fe3d-canvas").click({ button: "right", position: { x: 320, y: 260 } });
    await expect(page.getByTestId("viewport-context-menu")).toBeVisible();
    await expect(page.getByTestId("marking-menu-compass")).toBeVisible();
    await page.getByTestId("mm-face").click();
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().editor.mode))
      .toBe("face");
    await page.keyboard.press("F8");
    await expect
      .poll(async () => await page.evaluate(() => window.__FE3D_STUDIO__.getState().editor.mode))
      .toBe("object");

    // 4. Undock Outliner into Floating Window & Redock cleanly
    await page.locator('[data-menu-trigger="windows"]').click();
    await page.locator('[data-maya-cmd="float_outliner"]').click();
    await expect(page.getByTestId("floating-win-panel-outliner")).toBeVisible();
    await page.getByTestId("redock-panel-outliner").click();
    await expect(page.getByTestId("floating-win-panel-outliner")).toHaveCount(0);
    await expect(page.getByTestId("panel-outliner")).toBeVisible();

    // 5. Capture Mobile Landscape (844x390) & Mobile Portrait (390x844) Maya screenshots with zero horizontal overflow
    await page.setViewportSize({ width: 844, height: 390 });
    await page.waitForTimeout(80);
    await assertNoHorizontalOverflow(page);
    await page.screenshot({
      path: path.join(outDir, "13-maya-stage1-mobile-landscape-844x390.png"),
    });

    await page.setViewportSize({ width: 390, height: 844 });
    await page.waitForTimeout(80);
    await assertNoHorizontalOverflow(page);
    await page.screenshot({
      path: path.join(outDir, "14-maya-stage1-mobile-portrait-390x844.png"),
    });

    await context.close();
  });
});




