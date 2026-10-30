// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Real-Browser Chromium WebAssembly + WebGL 2.0 E2E Suite (Stage 4 v0.4.0)

import { test, expect } from "@playwright/test";
import fs from "node:fs/promises";
import path from "node:path";
import os from "node:os";

/**
 * Sample non-background WebGL2 pixels directly from #fe3d-canvas to prove
 * real GPU/WebGL2 rasterization rather than a stubbed or empty canvas.
 */
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
    // Clear color is roughly (0.09, 0.11, 0.14) -> ~(23, 28, 36)
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

/**
 * Sample 2D RGBA pixels from #material-preview-canvas to prove the C++20
 * MaterialPreviewRenderer renders a real shaded 3D PBR sphere live.
 */
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

async function waitForStudioReady(page) {
  const pageErrors = [];
  page.on("pageerror", (err) => pageErrors.push(err.message));

  await page.goto("/index.html");
  await expect(page.getByTestId("viewport-banner")).toBeHidden({ timeout: 20_000 });
  await expect(page.getByTestId("engine-badge")).toContainText("v0.4.0");
  await expect(page.getByTestId("stat-objects-meshes")).toHaveText("6 / 3");
  expect(pageErrors).toEqual([]);
  return pageErrors;
}

test.describe("Final Effect 3D Stage 4 Connected Browser Studio", () => {
  test("boots WASM engine, renders starter scene in WebGL 2.0, picks 3D object, and orbits camera", async ({
    page,
  }) => {
    await waitForStudioReady(page);

    // Pause animated showcase rig so pixel comparisons are deterministic
    await page.getByTestId("btn-toggle-rig").click();
    await expect(page.getByTestId("btn-toggle-rig")).toHaveText("Play Rig");
    await page.waitForTimeout(50);

    // Verify all 6 starter objects appear in the Scene Outliner
    await expect(page.getByTestId("outliner-item-GroundPlane")).toBeVisible();
    await expect(page.getByTestId("outliner-item-RigPivot")).toBeVisible();
    await expect(page.getByTestId("outliner-item-HeroCube")).toBeVisible();
    await expect(page.getByTestId("outliner-item-AccentPyramid")).toBeVisible();
    await expect(page.getByTestId("outliner-item-MainCamera")).toBeVisible();
    await expect(page.getByTestId("outliner-item-SunLight")).toBeVisible();

    // Verify real WebGL 2.0 3D scene rasterization
    const initialPixels = await sampleViewportPixels(page);
    expect(initialPixels.hasWebGL2).toBe(true);
    expect(initialPixels.version).toContain("WebGL 2.0");
    expect(initialPixels.nonClearPixels).toBeGreaterThan(250);

    // Click center of the 3D viewport to raycast-pick HeroCube
    const canvas = page.getByTestId("viewport-canvas");
    const box = await canvas.boundingBox();
    expect(box).not.toBeNull();
    await page.mouse.click(box.x + box.width * 0.5, box.y + box.height * 0.5);

    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: HeroCube");
    await expect(page.getByTestId("insp-name")).toHaveValue("HeroCube");

    // Right-drag in viewport to orbit camera and verify pixels change
    const cx = box.x + box.width * 0.5;
    const cy = box.y + box.height * 0.5;
    await page.mouse.move(cx, cy);
    await page.mouse.down({ button: "right" });
    await page.mouse.move(cx + 120, cy + 45, { steps: 6 });
    await page.mouse.up({ button: "right" });

    await expect
      .poll(async () => (await sampleViewportPixels(page)).hash)
      .not.toBe(initialPixels.hash);
  });

  test("supports full scene authoring, undo/redo, focus guard, and cycle-safe parenting", async ({
    page,
  }) => {
    await waitForStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();

    // Add a Cube preset (reuses shared HeroCubeMesh: 6 -> 7 objects / 3 meshes)
    await page.getByTestId("btn-add-cube").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 3");
    await expect(page.getByTestId("insp-name")).toHaveValue("Cube");

    // Type viewport hotkeys (W, E, R, F, 1..4) inside Inspector Name input to verify focus guard
    const nameInput = page.getByTestId("insp-name");
    await nameInput.click({ clickCount: 3 });
    await page.keyboard.press("Backspace");
    await page.keyboard.type("Pillar_WERF1234");
    await page.keyboard.press("Enter");
    await expect(page.getByTestId("outliner-item-Pillar_WERF1234")).toBeVisible();
    await expect(page.getByTestId("vp-mode-pill")).toContainText("OBJECT · MOVE");

    // Duplicate Pillar_WERF1234 -> Pillar_WERF1234.001 (7 -> 8 objects, shares mesh asset)
    await page.getByTestId("btn-duplicate").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("8 / 3");
    await expect(page.getByTestId("outliner-item-Pillar_WERF1234.001")).toBeVisible();

    // Delete Pillar_WERF1234.001 (8 -> 7 objects)
    await page.getByTestId("btn-delete").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 3");

    // Undo Delete -> back to 8 objects; Undo Duplicate -> 7; Redo -> 8
    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("8 / 3");
    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 3");
    await page.getByTestId("btn-redo").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("8 / 3");

    // Cycle-safe parenting check: select RigPivot and attempt to parent it under its child HeroCube
    await page.getByTestId("outliner-item-RigPivot").click();
    await expect(page.getByTestId("insp-name")).toHaveValue("RigPivot");
    const heroId = await page.evaluate(
      () => window.__FE3D_STUDIO__.getState().objects.find((o) => o.name === "HeroCube").id
    );
    await page.getByTestId("insp-parent").selectOption(String(heroId));
    await expect(page.getByTestId("status-message")).toContainText("HierarchyCycle");
    await expect(page.getByTestId("insp-parent")).toHaveValue("0");
  });

  test("edits transforms, camera projection, and directional light with live WebGL2 output", async ({
    page,
  }) => {
    await waitForStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();

    await page.getByTestId("outliner-item-HeroCube").click();
    await page.waitForTimeout(50);
    const beforeTf = await sampleViewportPixels(page);

    // Move HeroCube on X and scale up
    await setControlValueAndCommit(page.getByTestId("tf-pos-x"), "1.25");
    await setControlValueAndCommit(page.getByTestId("tf-scl-y"), "1.8");
    await expect
      .poll(async () => (await sampleViewportPixels(page)).hash)
      .not.toBe(beforeTf.hash);
    const afterTf = await sampleViewportPixels(page);

    // Select MainCamera and switch to Orthographic projection
    await page.getByTestId("outliner-item-MainCamera").click();
    await expect(page.getByTestId("card-camera")).toBeVisible();
    await page.getByTestId("cam-projection").selectOption("orthographic");
    await expect
      .poll(async () => (await sampleViewportPixels(page)).hash)
      .not.toBe(afterTf.hash);
    const afterOrtho = await sampleViewportPixels(page);

    // Select SunLight and lower intensity
    await page.getByTestId("outliner-item-SunLight").click();
    await expect(page.getByTestId("card-light")).toBeVisible();
    await setControlValueAndCommit(page.getByTestId("light-intensity"), "0.2");
    await expect
      .poll(async () => (await sampleViewportPixels(page)).hash)
      .not.toBe(afterOrtho.hash);
  });

  test("exports inline .fe3d and binary .fe3dbin bundle, resets project, and reloads cleanly", async ({
    page,
  }) => {
    await waitForStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();

    // Customize project metadata and add a UV Sphere object (creates 4th mesh asset StudioSphereMesh)
    await setControlValueAndCommit(page.getByTestId("meta-project-name"), "Stage4BrowserRoundTrip");
    await page.getByTestId("btn-add-sphere").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 4");
    await setControlValueAndCommit(page.getByTestId("insp-name"), "KeystoneSphere");
    await expect(page.getByTestId("outliner-item-KeystoneSphere")).toBeVisible();

    const tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), "fe3d-e2e-"));

    // 1. Export inline .fe3d
    const inlineDownloadPromise = page.waitForEvent("download");
    await page.getByTestId("btn-export-fe3d").click();
    const inlineDownload = await inlineDownloadPromise;
    expect(inlineDownload.suggestedFilename()).toBe("Stage4BrowserRoundTrip.fe3d");
    const inlinePath = path.join(tmpDir, inlineDownload.suggestedFilename());
    await inlineDownload.saveAs(inlinePath);

    const inlineJson = JSON.parse(await fs.readFile(inlinePath, "utf-8"));
    expect(inlineJson.schema_version).toEqual({ major: 1, minor: 2 });
    expect(inlineJson.metadata.project_name).toBe("Stage4BrowserRoundTrip");

    // 2. Export binary bundle (.fe3d + .fe3dbin)
    const bundleDir = path.join(tmpDir, "bundle");
    await fs.mkdir(bundleDir);
    const bundleDownloads = [];
    const onDownload = (dl) => bundleDownloads.push(dl);
    page.on("download", onDownload);
    await page.getByTestId("btn-export-bundle").click();
    await expect.poll(() => bundleDownloads.length, { timeout: 10_000 }).toBe(2);
    page.off("download", onDownload);

    const savedBundlePaths = [];
    for (const dl of bundleDownloads) {
      const target = path.join(bundleDir, dl.suggestedFilename());
      await dl.saveAs(target);
      savedBundlePaths.push(target);
    }
    const binFile = savedBundlePaths.find((p) => p.endsWith(".fe3dbin"));
    expect(binFile).toBeTruthy();
    const binHeader = await fs.readFile(binFile);
    expect(binHeader.subarray(0, 7).toString("ascii")).toBe("FE3DBIN");

    // 3. Reset to fresh starter project (back to 6 objects / 3 meshes)
    await page.getByTestId("btn-new-project").click();
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("6 / 3");
    await expect(page.getByTestId("outliner-item-KeystoneSphere")).toHaveCount(0);

    // 4. Re-import binary bundle (.fe3d + .fe3dbin) via file input and verify restoration
    await page.locator("#file-open-input").setInputFiles(savedBundlePaths);
    await expect(page.getByTestId("stat-objects-meshes")).toHaveText("7 / 4");
    await expect(page.getByTestId("meta-project-name")).toHaveValue("Stage4BrowserRoundTrip");
    await expect(page.getByTestId("outliner-item-KeystoneSphere")).toBeVisible();

    await fs.rm(tmpDir, { recursive: true, force: true });
  });

  test("interactive 3D transform gizmo hover, drag, Inspector sync, and coalesced single-step undo", async ({
    page,
  }) => {
    await waitForStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();

    // Select HeroCube, reset transform to exact origin (0, 0, 0) under RigPivot, and frame camera on it
    await page.getByTestId("outliner-item-HeroCube").click();
    await page.getByTestId("btn-reset-transform").click();
    await page.getByTestId("btn-frame").click();
    await expect(page.getByTestId("tf-pos-y")).toHaveValue("0");
    await page.waitForTimeout(50);

    const stateBefore = await page.evaluate(() => window.__FE3D_STUDIO__.getState());
    const undoBeforeDrag = stateBefore.history.undo_count;

    // HeroCube is framed at viewport center; world +Y translate shaft projects straight upward
    // along x = centerX above centerY.
    const canvas = page.getByTestId("viewport-canvas");
    const box = await canvas.boundingBox();
    expect(box).not.toBeNull();
    const cx = box.x + box.width * 0.5;
    let handleY = box.y + box.height * 0.39;

    for (const frac of [0.39, 0.36, 0.42, 0.34]) {
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

    await expect
      .poll(async () => {
        const s = await page.evaluate(() => window.__FE3D_STUDIO__.getState());
        return s.editor.gizmo_hovered_handle;
      })
      .toBe("translate_y");
    await expect(page.getByTestId("vp-mode-pill")).toContainText("HOVER TRANSLATE_Y");

    // Drag the +Y gizmo handle upward in 8 incremental pointer steps
    await page.mouse.down({ button: "left" });
    await page.mouse.move(cx, handleY - 64, { steps: 8 });
    await page.mouse.up({ button: "left" });

    // Wait for rAF + UI sync to reflect the committed gizmo drag
    await expect
      .poll(async () => {
        const s = await page.evaluate(() => window.__FE3D_STUDIO__.getState());
        return s.history.undo_count;
      })
      .toBe(undoBeforeDrag + 1);

    // Selection must remain HeroCube (no accidental background/ground reselection)
    await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: HeroCube");

    await expect
      .poll(async () => Number(await page.getByTestId("tf-pos-y").inputValue()))
      .toBeGreaterThan(0.2);

    // Undo restores exact pre-drag Y = 0
    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("tf-pos-y")).toHaveValue("0");

    // Verify Rotate, Scale, and Local/World orientation switching
    await page.getByTestId("btn-gizmo-rotate").click();
    await expect(page.getByTestId("vp-mode-pill")).toContainText("OBJECT · ROTATE (WORLD)");
    await page.getByTestId("btn-gizmo-scale").click();
    await page.getByTestId("btn-gizmo-space").click();
    await expect(page.getByTestId("vp-mode-pill")).toContainText("OBJECT · SCALE (LOCAL)");
  });

  test("supports polygon mesh editing in Face/Vertex modes, extrusion, hole boundary selection, face fill, and undo/redo", async ({
    page,
  }) => {
    await waitForStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();

    // Select HeroCube and switch to Face Edit mode
    await page.getByTestId("outliner-item-HeroCube").click();
    await page.getByTestId("btn-frame").click();
    await page.getByTestId("btn-mode-face").click();
    await page.waitForTimeout(50);

    await expect(page.getByTestId("card-mesh-edit")).toBeVisible();
    await expect(page.getByTestId("mesh-edit-topology")).toHaveText("6 faces · 12 edges · 8 verts");
    const trisBefore = Number(await page.getByTestId("stat-triangles").textContent());

    // Raycast-pick a front-facing quad face on HeroCube in the 3D viewport
    const box = await page.getByTestId("viewport-canvas").boundingBox();
    await page.mouse.click(box.x + box.width * 0.5, box.y + box.height * 0.5);
    await expect(page.getByTestId("mesh-edit-selection-tag")).toHaveText("1 face");

    // Extrude selected quad face outward by 0.45m -> adds 4 side quads (6 -> 10 faces, +8 triangles)
    await setControlValueAndCommit(page.getByTestId("mesh-extrude-dist"), "0.45");
    await page.getByTestId("btn-mesh-extrude").click();
    await expect(page.getByTestId("mesh-edit-topology")).toHaveText("10 faces · 20 edges · 12 verts");
    const trisAfterExtrude = Number(await page.getByTestId("stat-triangles").textContent());
    expect(trisAfterExtrude).toBe(trisBefore + 8);

    // Delete the extruded cap face -> 9 faces with 4 open boundary edges
    await page.getByTestId("btn-mesh-delete").click();
    await expect(page.getByTestId("mesh-edit-topology")).toHaveText("9 faces · 20 edges · 12 verts");

    // Select open boundary loop -> switches to Vertex mode with 4 ordered loop vertices selected
    await page.getByTestId("btn-mesh-boundary").click();
    await expect(page.getByTestId("vp-mode-pill")).toContainText("VERTEX ·");
    await expect(page.getByTestId("mesh-edit-selection-tag")).toHaveText("4 vertexs");

    // Fill Face closes the open boundary loop back to a 10-face closed manifold mesh
    await page.getByTestId("btn-mesh-fill").click();
    await expect(page.getByTestId("mesh-edit-topology")).toHaveText("10 faces · 20 edges · 12 verts");

    // Undo Fill Face -> 9 faces; Undo Delete Face -> 10 faces; Undo Extrude -> 6 faces
    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("mesh-edit-topology")).toHaveText("9 faces · 20 edges · 12 verts");
    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("mesh-edit-topology")).toHaveText("10 faces · 20 edges · 12 verts");
    await page.getByTestId("btn-undo").click();
    await expect(page.getByTestId("mesh-edit-topology")).toHaveText("6 faces · 12 edges · 8 verts");
  });

  test("creates and edits PBR materials with live 3D sphere preview, workspace presets, and reload persistence", async ({
    page,
  }) => {
    await waitForStudioReady(page);
    await page.getByTestId("btn-toggle-rig").click();

    // Select HeroCube first so we can measure its initial viewport shading before assigning a new material
    await page.getByTestId("outliner-item-HeroCube").click();
    await page.waitForTimeout(50);
    const vpBeforeMat = await sampleViewportPixels(page);

    // 1. Verify initial 96x96 live 3D sphere preview is genuinely rendered (not blank/uniform)
    const previewInitial = await sampleMaterialPreviewCanvas(page);
    expect(previewInitial.width).toBe(96);
    expect(previewInitial.height).toBe(96);
    expect(previewInitial.distinctFromCorner).toBeGreaterThan(1500);

    // 2. Create a new PBR material, edit metallic-roughness + emissive, and verify live preview updates
    await page.getByTestId("btn-mat-new").click();
    await setControlValueAndCommit(page.getByTestId("mat-name"), "SolarGold");
    await setControlValueAndCommit(page.getByTestId("mat-base-color"), "#ffcc22");
    await setControlValueAndCommit(page.getByTestId("mat-metallic"), "0.95");
    await setControlValueAndCommit(page.getByTestId("mat-roughness"), "0.12");
    await setControlValueAndCommit(page.getByTestId("mat-emissive-color"), "#ff4400");
    await setControlValueAndCommit(page.getByTestId("mat-emissive-strength"), "0.85");

    const previewGold = await sampleMaterialPreviewCanvas(page);
    expect(previewGold.hash).not.toBe(previewInitial.hash);

    // 3. Assign SolarGold (active material) to HeroCube and verify live WebGL2 viewport shading changes
    await page.getByTestId("btn-mat-assign-obj").click();
    await expect
      .poll(async () => (await sampleViewportPixels(page)).hash)
      .not.toBe(vpBeforeMat.hash);

    // 4. Switch Workspace Preset to "Modeling" (hides bottom dock & Material Editor) and persist across reload
    await page.getByTestId("workspace-preset-select").selectOption("modeling");
    await expect(page.getByTestId("panel-material-editor")).toBeHidden();
    await expect(page.getByTestId("dock-bottom")).toBeHidden();

    // Reload browser page and verify workspace layout persisted via localStorage + WorkspaceManager
    await page.reload();
    await expect(page.getByTestId("viewport-banner")).toBeHidden({ timeout: 20_000 });
    await expect(page.getByTestId("panel-material-editor")).toBeHidden();
    await expect(page.getByTestId("dock-bottom")).toBeHidden();

    // Restore Default workspace preset so Material Editor is visible again
    await page.getByTestId("workspace-preset-select").selectOption("default");
    await expect(page.getByTestId("panel-material-editor")).toBeVisible();
  });
});
