// SPDX-License-Identifier: GPL-3.0-or-later
// Captures Stage 5 (v0.5.0) baseline screenshots against the compiled WASM bundle
// so CI artifacts include genuine side-by-side Before (Stage 5) & After (Stage 5.1) PNGs.

import { chromium } from "@playwright/test";
import http from "node:http";
import fs from "node:fs/promises";
import path from "node:path";

const distDir = path.resolve(process.cwd(), process.env.FE3D_BASELINE_DIST_DIR || "../../build/ci-wasm-before");
const outDir = path.resolve(process.cwd(), process.env.FE3D_BASELINE_OUT_DIR || "../../build/ci-screenshots/before-stage5");

const MIME = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "application/javascript; charset=utf-8",
  ".wasm": "application/wasm",
};

const server = http.createServer(async (req, res) => {
  try {
    const cleanUrl = (req.url || "/").split("?")[0];
    const rel = cleanUrl === "/" ? "index.html" : cleanUrl.replace(/^\/+/, "");
    const fp = path.join(distDir, rel);
    const data = await fs.readFile(fp);
    res.writeHead(200, { "Content-Type": MIME[path.extname(fp)] || "application/octet-stream" });
    res.end(data);
  } catch (_) {
    res.writeHead(404);
    res.end("Not found");
  }
});

await fs.mkdir(outDir, { recursive: true });
await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
const { port } = server.address();
const baseUrl = `http://127.0.0.1:${port}`;

const browser = await chromium.launch({
  args: [
    "--use-gl=angle",
    "--use-angle=swiftshader",
    "--enable-unsafe-swiftshader",
    "--enable-webgl",
    "--ignore-gpu-blocklist",
  ],
});

const pageLogs = [];
try {
  const ctx = await browser.newContext({ viewport: { width: 1920, height: 1080 }, hasTouch: true });
  ctx.setDefaultTimeout(90_000);
  const page = await ctx.newPage();
  page.on("console", (msg) => {
    if (msg.type() === "error" || msg.type() === "warning") {
      pageLogs.push(`[console.${msg.type()}] ${msg.text()}`);
    }
  });
  page.on("pageerror", (err) => {
    pageLogs.push(`[pageerror] ${err && err.stack ? err.stack : String(err)}`);
  });
  page.on("crash", () => {
    pageLogs.push("[crash] Chromium page crashed");
  });

  await page.goto(`${baseUrl}/index.html`);
  await page.getByTestId("splash-screen").waitFor({ state: "hidden", timeout: 25_000 });
  await page.screenshot({ path: path.join(outDir, "01-home-desktop-1920x1080.png") });

  await page.getByTestId("btn-launcher-create").click();
  await page.getByTestId("studio-shell").waitFor({ state: "visible", timeout: 10_000 });
  await page.getByTestId("btn-toggle-rig").click();
  await page.screenshot({ path: path.join(outDir, "02-studio-desktop-1920x1080.png") });

  await page.setViewportSize({ width: 1366, height: 768 });
  await page.waitForTimeout(100);
  await page.screenshot({ path: path.join(outDir, "03-studio-desktop-1366x768.png") });

  await page.setViewportSize({ width: 980, height: 430 });
  await page.waitForTimeout(100);
  await page.screenshot({ path: path.join(outDir, "04-studio-safari-desktop-980x430.png") });

  await page.setViewportSize({ width: 844, height: 390 });
  await page.waitForTimeout(100);
  await page.screenshot({ path: path.join(outDir, "05-studio-mobile-landscape-844x390.png") });

  await page.setViewportSize({ width: 390, height: 844 });
  await page.waitForTimeout(100);
  await page.screenshot({ path: path.join(outDir, "06-studio-mobile-portrait-390x844-viewport.png") });

  await page.getByTestId("mp-tab-inspector").click();
  await page.waitForTimeout(100);
  await page.screenshot({ path: path.join(outDir, "07-studio-mobile-portrait-390x844-inspector.png") });

  await ctx.close();
} catch (err) {
  const details = `${err && err.stack ? err.stack : String(err)}\nLOGS:\n${pageLogs.slice(-20).join("\n")}`;
  console.log(`::error title=capture-baseline::${details.replace(/%/g, "%25").replace(/\r/g, "%0D").replace(/\n/g, "%0A")}`);
  throw err;
} finally {
  await browser.close();
  server.close();
}
