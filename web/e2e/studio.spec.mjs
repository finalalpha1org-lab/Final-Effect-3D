// SPDX-License-Identifier: GPL-3.0-or-later
// Real-browser Chromium WebAssembly + WebGL 2.0 interactive verification suite.

import { test, expect } from "@playwright/test";
import fs from "node:fs/promises";
import os from "node:os";
import path from "node:path";

async function waitForStudioReady(page) {
  await page.goto("/index.html");
  await expect(page.getByTestId("viewport-banner")).toBeHidden({ timeout: 20_000 });
  await expect(page.getByTestId("status-renderer")).toContainText("WebGL 2.0");
  await page.waitForFunction(() => Boolean(window.__FE3D_STUDIO__ && window.__FE3D_STUDIO__.getState()));
}

async function sampleWebGl2Framebuffer(page) {
  // Wait two animation frames so WebGL2 renders the latest scene state into the preserved drawing buffer.
  await page.evaluate(
    () =>
      new Promise((resolve) =>
        requestAnimationFrame(() => requestAnimationFrame(resolve))
      )
  );

  return page.evaluate(() => {
    const canvas = document.getElementById("fe3d-canvas");
    const gl = canvas.getContext("webgl2");
    if (!gl) {
      throw new Error("No active WebGL2RenderingContext on #fe3d-canvas");
    }
    const w = canvas.width;
    const h = canvas.height;
    const rgba = new Uint8Array(w * h * 4);
    gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, rgba);

    // Clear color is (0.09, 0.11, 0.14) -> ~(23, 28, 36).
    let nonBackgroundPixels = 0;
    let hash = 2166136261;
    // Sample every 4th pixel for fast deterministic FNV-1a digest + geometry coverage count.
    for (let i = 0; i < rgba.length; i += 16) {
      const r = rgba[i];
      const g = rgba[i + 1];
      const b = rgba[i + 2];
      if (Math.abs(r - 23) + Math.abs(g - 28) + Math.abs(b - 36) > 18) {
        nonBackgroundPixels++;
      }
      hash ^= r;
      hash = Math.imul(hash, 16777619) >>> 0;
      hash ^= g;
      hash = Math.imul(hash, 16777619) >>> 0;
      hash ^= b;
      hash = Math.imul(hash, 16777619) >>> 0;
    }

    // Also compute a focused digest around the center 80x80 region where HeroCube projects.
    let centerHash = 2166136261;
    const cx0 = Math.max(0, Math.floor(w / 2) - 40);
    const cy0 = Math.max(0, Math.floor(h / 2) - 40);
    for (let y = cy0; y < Math.min(h, cy0 + 80); y += 2) {
      for (let x = cx0; x < Math.min(w, cx0 + 80); x += 2) {
        const idx = (y * w + x) * 4;
        centerHash ^= rgba[idx];
        centerHash = Math.imul(centerHash, 16777619) >>> 0;
        centerHash ^= rgba[idx + 1];
        centerHash = Math.imul(centerHash, 16777619) >>> 0;
        centerHash ^= rgba[idx + 2];
        centerHash = Math.imul(centerHash, 16777619) >>> 0;
      }
    }

    return { width: w, height: h, nonBackgroundPixels, hash, centerHash };
  });
}

test("boots WebAssembly + WebGL 2.0, rasterizes 3D starter scene, and populates Outliner & Telemetry", async ({
  page,
}) => {
  const pageErrors = [];
  page.on("pageerror", (err) => pageErrors.push(err.message));

  await waitForStudioReady(page);
  expect(pageErrors).toEqual([]);

  await expect(page.getByTestId("engine-badge")).toContainText("v0.3.0");
  await expect(page.locator("#stat-objects-meshes")).toHaveText("6 / 3");

  for (const starterObj of [
    "GroundPlane",
    "RigPivot",
    "HeroCube",
    "AccentPyramid",
    "MainCamera",
    "SunLight",
  ]) {
    await expect(page.getByTestId(`outliner-item-${starterObj}`)).toBeVisible();
  }

  const fb = await sampleWebGl2Framebuffer(page);
  expect(fb.width).toBeGreaterThan(400);
  expect(fb.height).toBeGreaterThan(300);
  expect(fb.nonBackgroundPixels).toBeGreaterThan(500);
});

test("canvas 3D raycast picking highlights selected mesh and RMB drag orbits camera", async ({
  page,
}) => {
  await waitForStudioReady(page);

  // Pause showcase rig rotation so pixel diffs isolate selection highlight & camera orbit.
  await page.getByTestId("btn-toggle-rig").click();
  await expect(page.getByTestId("btn-toggle-rig")).toHaveText("Play Rig");

  const beforePick = await sampleWebGl2Framebuffer(page);

  // Real mouse click in the center of the 3D WebGL2 canvas -> raycastScene hits HeroCube.
  const canvas = page.getByTestId("fe3d-canvas");
  const box = await canvas.boundingBox();
  expect(box).not.toBeNull();
  await page.mouse.click(box.x + box.width * 0.5, box.y + box.height * 0.5);

  await expect(page.getByTestId("vp-selection-pill")).toHaveText("Selected: HeroCube");
  await expect(page.getByTestId("insp-name")).toHaveValue("HeroCube");
  await expect(page.getByTestId("card-mesh")).toBeVisible();
  await expect(page.locator("#mesh-asset-name")).toContainText("HeroCubeMesh");

  const afterPick = await sampleWebGl2Framebuffer(page);
  expect(afterPick.centerHash).not.toBe(beforePick.centerHash);

  // Real Right-Mouse-Button drag across canvas orbits active camera.
  const initialYaw = await page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.camera_yaw_deg);
  const startX = box.x + box.width * 0.5;
  const startY = box.y + box.height * 0.5;
  await page.mouse.move(startX, startY);
  await page.mouse.down({ button: "right" });
  await page.mouse.move(startX + 110, startY - 45, { steps: 6 });
  await page.mouse.up({ button: "right" });

  await expect
    .poll(async () => page.evaluate(() => window.__FE3D_STUDIO__.getState().viewport.camera_yaw_deg))
    .not.toBeCloseTo(initialYaw, 1);

  const afterOrbit = await sampleWebGl2Framebuffer(page);
  expect(afterOrbit.hash).not.toBe(afterPick.hash);
});

test("authoring controls: add preset, input focus guard, transform edit, cycle guard, delete, and undo/redo", async ({
  page,
}) => {
  await waitForStudioReady(page);
  await page.getByTestId("btn-toggle-rig").click();

  // Add Pyramid preset -> reuses shared AccentPyramidMesh (7 objects / 3 meshes).
  await page.getByTestId("btn-add-pyramid").click();
  await expect(page.locator("#stat-objects-meshes")).toHaveText("7 / 3");
  await expect(page.getByTestId("insp-name")).toHaveValue("Pyramid");

  // Type keys that are also viewport hotkeys ('F', 'W', 'S', 'D', 'Z') via real keystrokes inside Inspector Name input.
  const nameInput = page.getByTestId("insp-name");
  await nameInput.click({ clickCount: 3 });
  await page.keyboard.press("Backspace");
  await page.keyboard.type("Spire_FWDZ_01");
  await page.keyboard.press("Enter");
  await expect(page.getByTestId("outliner-item-Spire_FWDZ_01")).toBeVisible();

  // Edit local transform Position X and Scale Y via Inspector inputs.
  const posX = page.getByTestId("tf-pos-x");
  await posX.fill("2.25");
  await posX.dispatchEvent("change");

  const sclY = page.getByTestId("tf-scl-y");
  await sclY.fill("1.75");
  await sclY.dispatchEvent("change");

  const spirePosX = await page.evaluate(() => window.__FE3D_STUDIO__.getState().inspector.position[0]);
  expect(spirePosX).toBeCloseTo(2.25, 3);

  // Verify hierarchy cycle guard: RigPivot cannot be reparented under its child HeroCube.
  await page.getByTestId("outliner-item-RigPivot").click();
  await expect(page.getByTestId("insp-name")).toHaveValue("RigPivot");
  const heroId = await page.evaluate(
    () => window.__FE3D_STUDIO__.getState().objects.find((o) => o.name === "HeroCube").id
  );
  await page.getByTestId("insp-parent").selectOption(String(heroId));
  await expect(page.getByTestId("status-message")).toContainText("HierarchyCycle");
  await expect(page.getByTestId("insp-parent")).toHaveValue("0");

  // Reselect Spire_FWDZ_01, delete it, then Undo & Redo through CommandStack.
  await page.getByTestId("outliner-item-Spire_FWDZ_01").click();
  await page.getByTestId("btn-delete").click();
  await expect(page.locator("#stat-objects-meshes")).toHaveText("6 / 3");
  await expect(page.getByTestId("outliner-item-Spire_FWDZ_01")).toHaveCount(0);

  // Undo Delete -> restores Spire_FWDZ_01 with position.x = 2.25!
  await page.getByTestId("btn-undo").click();
  await expect(page.locator("#stat-objects-meshes")).toHaveText("7 / 3");
  await expect(page.getByTestId("outliner-item-Spire_FWDZ_01")).toBeVisible();

  await page.getByTestId("outliner-item-Spire_FWDZ_01").click();
  await expect(page.getByTestId("tf-pos-x")).toHaveValue("2.25");

  // Redo Delete -> removes Spire_FWDZ_01 again.
  await page.getByTestId("btn-redo").click();
  await expect(page.locator("#stat-objects-meshes")).toHaveText("6 / 3");
});

test("project round-trip: downloads inline .fe3d and exports + reloads .fe3d/.fe3dbin binary bundle", async ({
  page,
}) => {
  await waitForStudioReady(page);
  await page.getByTestId("btn-toggle-rig").click();

  // Customize project metadata and spawn a directional light.
  const projInput = page.getByTestId("meta-project-name");
  await projInput.fill("Browser Bundle Roundtrip");
  await projInput.dispatchEvent("change");

  const sceneInput = page.getByTestId("meta-scene-name");
  await sceneInput.fill("AuthoredWebScene");
  await sceneInput.dispatchEvent("change");

  await page.getByTestId("btn-add-light").click();
  await expect(page.locator("#stat-objects-meshes")).toHaveText("7 / 3");
  const lightIntensity = page.getByTestId("light-intensity");
  await lightIntensity.fill("2.4");
  await lightIntensity.dispatchEvent("change");

  // 1. Verify inline .fe3d download.
  const inlineDownloadPromise = page.waitForEvent("download");
  await page.getByTestId("btn-export-fe3d").click();
  const inlineDownload = await inlineDownloadPromise;
  const inlineTmpPath = await inlineDownload.path();
  const inlineJson = JSON.parse(await fs.readFile(inlineTmpPath, "utf8"));
  expect(inlineJson.format).toBe("finaleffect.project");
  expect(inlineJson.schema_version).toEqual({ major: 1, minor: 1 });
  expect(inlineJson.metadata.project_name).toBe("Browser Bundle Roundtrip");

  // 2. Verify binary bundle (.fe3d schema 1.1 + .fe3dbin v1.0) export and re-import!
  const tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), "fe3d-web-bundle-"));
  const downloads = [];
  const onDownload = (dl) => downloads.push(dl);
  page.on("download", onDownload);

  await page.getByTestId("btn-export-bundle").click();
  await expect.poll(() => downloads.length).toBe(2);
  page.off("download", onDownload);

  const savedPaths = [];
  for (const dl of downloads) {
    const dest = path.join(tmpDir, dl.suggestedFilename());
    await dl.saveAs(dest);
    savedPaths.push(dest);
  }

  const binFile = savedPaths.find((p) => p.endsWith(".fe3dbin"));
  expect(binFile).toBeTruthy();
  const binBytes = await fs.readFile(binFile);
  expect(binBytes.subarray(0, 7).toString("ascii")).toBe("FE3DBIN");

  // Reset to clean starter project (6 objects), then re-open the exported .fe3d + .fe3dbin bundle!
  await page.getByTestId("btn-new-project").click();
  await expect(page.locator("#stat-objects-meshes")).toHaveText("6 / 3");
  await expect(page.getByTestId("meta-project-name")).toHaveValue("Final Effect 3D Starter Project");

  await page.getByTestId("file-open-input").setInputFiles(savedPaths);
  await expect(page.getByTestId("meta-project-name")).toHaveValue("Browser Bundle Roundtrip");
  await expect(page.getByTestId("meta-scene-name")).toHaveValue("AuthoredWebScene");
  await expect(page.locator("#stat-objects-meshes")).toHaveText("7 / 3");

  await fs.rm(tmpDir, { recursive: true, force: true });
});
