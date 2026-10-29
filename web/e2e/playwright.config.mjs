// SPDX-License-Identifier: GPL-3.0-or-later
import { defineConfig } from "@playwright/test";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const distDir = path.resolve(__dirname, process.env.FE3D_WEB_DIST_DIR || "../../build/wasm-release");

export default defineConfig({
  testDir: ".",
  testMatch: "*.spec.mjs",
  timeout: 45_000,
  expect: {
    timeout: 10_000,
  },
  fullyParallel: false,
  workers: 1,
  use: {
    baseURL: "http://127.0.0.1:8765",
    viewport: { width: 1440, height: 860 },
    headless: true,
    launchOptions: {
      args: [
        "--use-gl=angle",
        "--use-angle=swiftshader",
        "--enable-webgl",
        "--ignore-gpu-blocklist",
      ],
    },
  },
  webServer: {
    command: `python3 -m http.server 8765 --bind 127.0.0.1 --directory "${distDir}"`,
    url: "http://127.0.0.1:8765/index.html",
    reuseExistingServer: !process.env.CI,
    timeout: 15_000,
  },
});
