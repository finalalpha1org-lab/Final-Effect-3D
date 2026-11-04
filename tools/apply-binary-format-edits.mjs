// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D - one-shot editor for the two large web client files touched by the
// .fe3d / .fe3a binary project container change:
//   web/editor.js            (binary save/export/open flows, base64 bridges, container magic check)
//   web/e2e/studio.spec.mjs  (binary container assertions instead of JSON text assertions)
//
// It applies exact, idempotent text replacements: every replacement must match exactly once
// (or already be applied), otherwise the run fails loudly. This file and its workflow are
// removed again in a follow-up commit once the edits have landed.
import { readFileSync, writeFileSync } from "node:fs";

const EDITS = [
  {
    "file": "web/editor.js",
    "old": "\n  async function snapshotActiveProjectToRecent(sourceLabel) {\n    if (!wasmApi || !currentState) return;\n    const inlineJson = wasmApi.exportInlineProject();\n    if (!inlineJson) return;\n    const thumb = captureViewportThumbnail();\n    const rec = await REG.recordRecentProject(\n      {",
    "new": "\n  async function snapshotActiveProjectToRecent(sourceLabel) {\n    if (!wasmApi || !currentState) return;\n    const projectBytes = wasmApi.exportProjectBytes();\n    if (!projectBytes || projectBytes.length === 0) return;\n    const thumb = captureViewportThumbnail();\n    const rec = await REG.recordRecentProject(\n      {"
  },
  {
    "file": "web/editor.js",
    "old": "        sourceLabel: sourceLabel || \"3D Studio Session\",\n        thumbnailDataUrl: thumb,\n      },\n      inlineJson\n    );\n    activeProjectRecordId = rec.id;\n  }",
    "new": "        sourceLabel: sourceLabel || \"3D Studio Session\",\n        thumbnailDataUrl: thumb,\n      },\n      projectBytes\n    );\n    activeProjectRecordId = rec.id;\n  }"
  },
  {
    "file": "web/editor.js",
    "old": "      wasmApi.fs.mkdir(\"/fe3d_import\");\n    } catch (_) {}\n    const vfsPath = \"/fe3d_import/recent_reopen.fe3d\";\n    wasmApi.fs.writeFile(vfsPath, payload);\n    const ok = wasmApi.loadFileFromVfs(vfsPath);\n    if (!ok) {\n      const errMsg =",
    "new": "      wasmApi.fs.mkdir(\"/fe3d_import\");\n    } catch (_) {}\n    const vfsPath = \"/fe3d_import/recent_reopen.fe3d\";\n    const payloadBytes =\n      typeof payload === \"string\" ? new TextEncoder().encode(payload) : payload;\n    wasmApi.fs.writeFile(vfsPath, payloadBytes);\n    const ok = wasmApi.loadFileFromVfs(vfsPath);\n    if (!ok) {\n      const errMsg ="
  },
  {
    "file": "web/editor.js",
    "old": "          M._free(ptr);\n        }\n      },\n      exportInlineProject: function () {\n        return M.ccall(\"fe3d_export_inline_project\", \"string\", [], []);\n      },\n      exportWorkspaceJson: function () {\n        return M.ccall(\"fe3d_export_workspace_json\", \"string\", [], []);\n      },\n      saveBundleToVfs: function (vfsPath) {\n        return M.ccall(\"fe3d_save_bundle_to_vfs\", \"number\", [\"string\"], [vfsPath]) === 1;\n      },\n      loadFileFromVfs: function (vfsPath) {\n        const ok = M.ccall(\"fe3d_load_file_from_vfs\", \"number\", [\"string\"], [vfsPath]) === 1;\n        syncFromEngine(true);",
    "new": "          M._free(ptr);\n        }\n      },\n      exportProjectBytes: function () {\n        const ptr = M._fe3d_export_inline_project_ptr();\n        const size = M._fe3d_export_inline_project_size();\n        if (!ptr || size <= 0) return null;\n        return new Uint8Array(M.HEAPU8.buffer, ptr, size).slice();\n      },\n      exportWorkspaceBytes: function () {\n        const ptr = M._fe3d_export_workspace_ptr();\n        const size = M._fe3d_export_workspace_size();\n        if (!ptr || size <= 0) return null;\n        return new Uint8Array(M.HEAPU8.buffer, ptr, size).slice();\n      },\n      saveProjectToVfs: function (vfsPath) {\n        return M.ccall(\"fe3d_save_bundle_to_vfs\", \"number\", [\"string\"], [vfsPath]) === 1;\n      },\n      importWorkspaceFromVfs: function (vfsPath) {\n        return M.ccall(\"fe3d_import_workspace_from_vfs\", \"number\", [\"string\"], [vfsPath]) === 1;\n      },\n      loadFileFromVfs: function (vfsPath) {\n        const ok = M.ccall(\"fe3d_load_file_from_vfs\", \"number\", [\"string\"], [vfsPath]) === 1;\n        syncFromEngine(true);"
  },
  {
    "file": "web/editor.js",
    "old": "      getNavGizmoLayout: getNavGizmoLayout,\n      renderFinalFrame: wasmApi.renderFinalFrame,\n      loadHdriBytes: wasmApi.loadHdriBytes,\n      exportInlineProject: wasmApi.exportInlineProject,\n      importInlineProject: function (jsonText) {\n        return wasmApi.dispatchAction({ action: \"load_inline_json\", json_text: jsonText });\n      },\n      onSync: function (fn) {\n        if (typeof fn === \"function\") mayaSyncListeners.push(fn);",
    "new": "      getNavGizmoLayout: getNavGizmoLayout,\n      renderFinalFrame: wasmApi.renderFinalFrame,\n      loadHdriBytes: wasmApi.loadHdriBytes,\n      exportProjectBytes: wasmApi.exportProjectBytes,\n      exportWorkspaceBytes: wasmApi.exportWorkspaceBytes,\n      importProjectBytes: function (bytesInput) {\n        const bytes =\n          bytesInput instanceof Uint8Array ? bytesInput : new Uint8Array(bytesInput || []);\n        if (bytes.length === 0) return false;\n        try {\n          wasmApi.fs.mkdir(\"/fe3d_import\");\n        } catch (_) {}\n        const vfsPath = \"/fe3d_import/imported_project.fe3d\";\n        wasmApi.fs.writeFile(vfsPath, bytes);\n        return wasmApi.loadFileFromVfs(vfsPath);\n      },\n      // Base64 bridges so automation (Playwright) can move whole project containers as strings.\n      exportProjectBase64: function () {\n        const bytes = wasmApi.exportProjectBytes();\n        return bytes && bytes.length > 0 ? bytesToBase64(bytes) : null;\n      },\n      importProjectBase64: function (base64Text) {\n        const bytes = base64ToBytes(base64Text);\n        if (!bytes || bytes.length === 0) return false;\n        try {\n          wasmApi.fs.mkdir(\"/fe3d_import\");\n        } catch (_) {}\n        const vfsPath = \"/fe3d_import/base64_project.fe3d\";\n        wasmApi.fs.writeFile(vfsPath, bytes);\n        return wasmApi.loadFileFromVfs(vfsPath);\n      },\n      // Reads the container header so tests can assert the binary format directly.\n      projectContainerInfo: function () {\n        const bytes = wasmApi.exportProjectBytes();\n        if (!bytes || bytes.length < 32) return null;\n        return {\n          magic_ok: hasFe3dContainerMagic(bytes),\n          major: bytes[8] | (bytes[9] << 8),\n          minor: bytes[10] | (bytes[11] << 8),\n          size: bytes.length,\n        };\n      },\n      onSync: function (fn) {\n        if (typeof fn === \"function\") mayaSyncListeners.push(fn);"
  },
  {
    "file": "web/editor.js",
    "old": "  function persistWorkspaceToStorage() {\n    if (!wasmApi) return;\n    try {\n      const wsJson = wasmApi.exportWorkspaceJson();\n      if (wsJson) {\n        window.localStorage.setItem(WORKSPACE_STORAGE_KEY, wsJson);\n      }\n    } catch (_) {}\n  }",
    "new": "  function persistWorkspaceToStorage() {\n    if (!wasmApi) return;\n    try {\n      const wsBytes = wasmApi.exportWorkspaceBytes();\n      if (wsBytes && wsBytes.length > 0) {\n        window.localStorage.setItem(WORKSPACE_STORAGE_KEY, bytesToBase64(wsBytes));\n      }\n    } catch (_) {}\n  }"
  },
  {
    "file": "web/editor.js",
    "old": "    if (!wasmApi) return;\n    try {\n      const saved = window.localStorage.getItem(WORKSPACE_STORAGE_KEY);\n      if (saved && saved.trim().startsWith(\"{\")) {\n        const parsed = JSON.parse(saved);\n        const wsObj = parsed.workspace || parsed;\n        syncWorkspaceTabFromLayoutName(wsObj.layout_name);\n        wasmApi.dispatchAction({\n          action: \"import_workspace_json\",\n          json_text: saved,\n        });\n        applyDockRatiosCss(wsObj);\n      }\n    } catch (_) {}\n  }",
    "new": "    if (!wasmApi) return;\n    try {\n      const saved = window.localStorage.getItem(WORKSPACE_STORAGE_KEY);\n      if (!saved) return;\n      const layoutBytes = base64ToBytes(saved);\n      if (!layoutBytes || layoutBytes.length === 0) return;\n      if (!restoreWorkspaceBytes(layoutBytes)) return;\n      const wsState = currentState && currentState.workspace ? currentState.workspace : null;\n      if (wsState) {\n        syncWorkspaceTabFromLayoutName(wsState.layout_name);\n        applyDockRatiosCss(wsState);\n      }\n    } catch (_) {}\n  }"
  },
  {
    "file": "web/editor.js",
    "old": "    return cleaned || \"project\";\n  }\n\n  function triggerDownload(filename, content, mimeType) {\n    const blob = new Blob([content], { type: mimeType });\n    const url = URL.createObjectURL(blob);",
    "new": "    return cleaned || \"project\";\n  }\n\n  function bytesToBase64(bytes) {\n    let binary = \"\";\n    const chunk = 0x8000;\n    for (let i = 0; i < bytes.length; i += chunk) {\n      binary += String.fromCharCode.apply(null, bytes.subarray(i, i + chunk));\n    }\n    return window.btoa(binary);\n  }\n\n  function base64ToBytes(text) {\n    try {\n      const binary = window.atob(String(text || \"\"));\n      const out = new Uint8Array(binary.length);\n      for (let i = 0; i < binary.length; i++) out[i] = binary.charCodeAt(i);\n      return out;\n    } catch (_) {\n      return null;\n    }\n  }\n\n  // Restores a workspace layout container through the virtual file system so the C++ workspace\n  // manager validates and applies it exactly like a saved layout file.\n  function restoreWorkspaceBytes(bytes, vfsPath) {\n    if (!wasmApi || !bytes || bytes.length === 0) return false;\n    try {\n      wasmApi.fs.mkdir(\"/fe3d_state\");\n    } catch (_) {}\n    const path = vfsPath || \"/fe3d_state/workspace_layout.fe3d\";\n    wasmApi.fs.writeFile(path, bytes);\n    const ok = wasmApi.importWorkspaceFromVfs(path);\n    if (ok) syncFromEngine(true);\n    return ok;\n  }\n\n  function triggerDownload(filename, content, mimeType) {\n    const blob = new Blob([content], { type: mimeType });\n    const url = URL.createObjectURL(blob);"
  },
  {
    "file": "web/editor.js",
    "old": "      const buf = new Uint8Array(await file.arrayBuffer());\n      const vfsPath = \"/fe3d_import/\" + file.name;\n      wasmApi.fs.writeFile(vfsPath, buf);\n      if (lower.endsWith(\".fe3d\") || lower.endsWith(\".json\")) {\n        fe3dTargetPath = vfsPath;\n      }\n    }",
    "new": "      const buf = new Uint8Array(await file.arrayBuffer());\n      const vfsPath = \"/fe3d_import/\" + file.name;\n      wasmApi.fs.writeFile(vfsPath, buf);\n      if (lower.endsWith(\".fe3d\") || lower.endsWith(\".fe3a\")) {\n        fe3dTargetPath = vfsPath;\n      }\n    }"
  },
  {
    "file": "web/editor.js",
    "old": "    }\n\n    if (!fe3dTargetPath) {\n      const msg = \"Please include a .fe3d project file when opening a project.\";\n      els.statusDot.classList.add(\"is-error\");\n      els.statusMessage.textContent = msg;\n      if (document.body.dataset.appView === \"home\" && els.launcherErrorBanner) {",
    "new": "    }\n\n    if (!fe3dTargetPath) {\n      const msg = \"Please include a .fe3d / .fe3a project file when opening a project.\";\n      els.statusDot.classList.add(\"is-error\");\n      els.statusMessage.textContent = msg;\n      if (document.body.dataset.appView === \"home\" && els.launcherErrorBanner) {"
  },
  {
    "file": "web/editor.js",
    "old": "      const errText =\n        currentState && currentState.status\n          ? currentState.status.message\n          : \"Invalid or corrupted .fe3d project file.\";\n      if (document.body.dataset.appView === \"home\" && els.launcherErrorBanner) {\n        els.launcherErrorBanner.hidden = false;\n        els.launcherErrorBanner.textContent = \"Failed to open project: \" + errText;",
    "new": "      const errText =\n        currentState && currentState.status\n          ? currentState.status.message\n          : \"Invalid or corrupted .fe3d / .fe3a binary project container.\";\n      if (document.body.dataset.appView === \"home\" && els.launcherErrorBanner) {\n        els.launcherErrorBanner.hidden = false;\n        els.launcherErrorBanner.textContent = \"Failed to open project: \" + errText;"
  },
  {
    "file": "web/editor.js",
    "old": "    setTimeout(() => snapshotActiveProjectToRecent(\"Opened .fe3d File\"), 80);\n  }\n\n  function exportPortableFe3d() {\n    if (!wasmApi || !currentState) return;\n    const jsonText = wasmApi.exportInlineProject();\n    if (!jsonText) return;\n    const stem = sanitizeFilenameStem(currentState.project.project_name);\n    triggerDownload(stem + \".fe3d\", jsonText, \"application/json\");\n    snapshotActiveProjectToRecent(\"Saved .fe3d\");\n    els.statusDot.classList.remove(\"is-error\");\n    els.statusMessage.textContent = \"Saved \" + stem + \".fe3d (inline schema 1.3)\";\n  }\n\n  function exportBinaryBundle() {\n    if (!wasmApi || !currentState) return;\n    try {",
    "new": "    setTimeout(() => snapshotActiveProjectToRecent(\"Opened .fe3d File\"), 80);\n  }\n\n  // Final Effect 3D project files are the app's own binary container: the same bytes are used for\n  // both extensions, so \".fe3d\" (scene save) and \".fe3a\" (portable export) are interchangeable.\n  const FE3D_CONTAINER_MIME = \"application/x-finaleffect-project\";\n\n  function hasFe3dContainerMagic(bytes) {\n    return (\n      !!bytes &&\n      bytes.length >= 32 &&\n      bytes[0] === 0x46 &&\n      bytes[1] === 0x45 &&\n      bytes[2] === 0x33 &&\n      bytes[3] === 0x44 &&\n      bytes[4] === 0x50 &&\n      bytes[5] === 0x52 &&\n      bytes[6] === 0x4a &&\n      bytes[7] === 0x1a\n    );\n  }\n\n  // Portable export: identical container bytes, saved with the .fe3a extension.\n  function exportPortableFe3d() {\n    if (!wasmApi || !currentState) return;\n    const bytes = wasmApi.exportProjectBytes();\n    if (!bytes || bytes.length === 0) {\n      syncFromEngine(true);\n      return;\n    }\n    if (!hasFe3dContainerMagic(bytes)) {\n      els.statusDot.classList.add(\"is-error\");\n      els.statusMessage.textContent = \"Export aborted: encoder did not produce a valid .fe3a container.\";\n      return;\n    }\n    const stem = sanitizeFilenameStem(currentState.project.project_name);\n    triggerDownload(stem + \".fe3a\", bytes, FE3D_CONTAINER_MIME);\n    snapshotActiveProjectToRecent(\"Exported .fe3a\");\n    els.statusDot.classList.remove(\"is-error\");\n    els.statusMessage.textContent =\n      \"Exported \" + stem + \".fe3a (binary container, \" + bytes.length + \" bytes)\";\n  }\n\n  // Scene save: writes the container through the virtual file system and verifies the bytes on\n  // disk before offering the .fe3d download.\n  function exportBinaryBundle() {\n    if (!wasmApi || !currentState) return;\n    try {"
  },
  {
    "file": "web/editor.js",
    "old": "    } catch (_) {}\n    const stem = sanitizeFilenameStem(currentState.project.project_name);\n    const fe3dPath = \"/fe3d_export/\" + stem + \".fe3d\";\n    const binPath = \"/fe3d_export/\" + stem + \".fe3dbin\";\n    if (!wasmApi.saveBundleToVfs(fe3dPath)) {\n      syncFromEngine(true);\n      return;\n    }\n    const fe3dBytes = wasmApi.fs.readFile(fe3dPath);\n    const binBytes = wasmApi.fs.readFile(binPath);\n    triggerDownload(stem + \".fe3d\", fe3dBytes, \"application/json\");\n    triggerDownload(stem + \".fe3dbin\", binBytes, \"application/octet-stream\");\n    snapshotActiveProjectToRecent(\"Exported Bundle\");\n  }\n\n  // =========================================================================",
    "new": "    } catch (_) {}\n    const stem = sanitizeFilenameStem(currentState.project.project_name);\n    const fe3dPath = \"/fe3d_export/\" + stem + \".fe3d\";\n    if (!wasmApi.saveProjectToVfs(fe3dPath)) {\n      syncFromEngine(true);\n      els.statusDot.classList.add(\"is-error\");\n      els.statusMessage.textContent = \"Save failed: the project container could not be written.\";\n      return;\n    }\n    const fe3dBytes = wasmApi.fs.readFile(fe3dPath);\n    if (!hasFe3dContainerMagic(fe3dBytes)) {\n      els.statusDot.classList.add(\"is-error\");\n      els.statusMessage.textContent = \"Save aborted: on-disk project bytes are not a valid container.\";\n      return;\n    }\n    triggerDownload(stem + \".fe3d\", fe3dBytes, FE3D_CONTAINER_MIME);\n    snapshotActiveProjectToRecent(\"Saved .fe3d\");\n    els.statusDot.classList.remove(\"is-error\");\n    els.statusMessage.textContent =\n      \"Saved \" + stem + \".fe3d (binary container, \" + fe3dBytes.length + \" bytes)\";\n  }\n\n  // ========================================================================="
  },
  {
    "file": "web/editor.js",
    "old": "\n    function saveNamedCustomWorkspace() {\n      if (!wasmApi) return;\n      const wsJson = wasmApi.exportWorkspaceJson();\n      if (wsJson && REG.saveCustomWorkspaceLayout(wsJson)) {\n        persistWorkspaceToStorage();\n        els.statusDot.classList.remove(\"is-error\");\n        els.statusMessage.textContent = \"Saved custom workspace layout (scene data untouched)\";",
    "new": "\n    function saveNamedCustomWorkspace() {\n      if (!wasmApi) return;\n      const wsBytes = wasmApi.exportWorkspaceBytes();\n      if (wsBytes && wsBytes.length > 0 && REG.saveCustomWorkspaceLayout(bytesToBase64(wsBytes))) {\n        persistWorkspaceToStorage();\n        els.statusDot.classList.remove(\"is-error\");\n        els.statusMessage.textContent = \"Saved custom workspace layout (scene data untouched)\";"
  },
  {
    "file": "web/editor.js",
    "old": "        return;\n      }\n      try {\n        const parsed = JSON.parse(saved);\n        const wsObj = parsed.workspace || parsed;\n        isRightDockCollapsed = false;\n        syncWorkspaceTabFromLayoutName(wsObj.layout_name);\n        wasmApi.dispatchAction({ action: \"import_workspace_json\", json_text: saved });\n        applyDockRatiosCss(wsObj);\n        persistWorkspaceToStorage();\n        els.statusDot.classList.remove(\"is-error\");\n        els.statusMessage.textContent = \"Restored saved custom workspace layout\";",
    "new": "        return;\n      }\n      try {\n        const layoutBytes = base64ToBytes(saved);\n        if (!restoreWorkspaceBytes(layoutBytes, \"/fe3d_state/custom_workspace_layout.fe3d\")) {\n          els.statusDot.classList.add(\"is-error\");\n          els.statusMessage.textContent = \"Saved custom workspace layout could not be restored.\";\n          return;\n        }\n        const wsObj = currentState && currentState.workspace ? currentState.workspace : null;\n        isRightDockCollapsed = false;\n        if (wsObj) {\n          syncWorkspaceTabFromLayoutName(wsObj.layout_name);\n          applyDockRatiosCss(wsObj);\n        }\n        persistWorkspaceToStorage();\n        els.statusDot.classList.remove(\"is-error\");\n        els.statusMessage.textContent = \"Restored saved custom workspace layout\";"
  },
  {
    "file": "web/e2e/studio.spec.mjs",
    "old": "// SPDX-License-Identifier: GPL-3.0-or-later\n// Final Effect 3D — Stage 5 (v0.5.0) Real-Browser WebAssembly + WebGL 2.0 Multi-Device E2E Suite\n// Validates Splash -> Home / 6-Studio Selector -> 3D Studio Project Launcher -> 12 Workspaces ->\n// 3D Viewport, Gizmos, Mesh Modeling, PBR Materials, .fe3d/.fe3dbin Serialization, Touch Gestures,\n// Mobile Portrait, Mobile Landscape, Tablet, Desktop, and iPhone Safari \"Request Desktop Website\".\n\nimport { test, expect } from \"@playwright/test\";",
    "new": "// SPDX-License-Identifier: GPL-3.0-or-later\n// Final Effect 3D — Stage 5 (v0.5.0) Real-Browser WebAssembly + WebGL 2.0 Multi-Device E2E Suite\n// Validates Splash -> Home / 6-Studio Selector -> 3D Studio Project Launcher -> 12 Workspaces ->\n// 3D Viewport, Gizmos, Mesh Modeling, PBR Materials, .fe3d/.fe3a binary container serialization, Touch Gestures,\n// Mobile Portrait, Mobile Landscape, Tablet, Desktop, and iPhone Safari \"Request Desktop Website\".\n\nimport { test, expect } from \"@playwright/test\";"
  },
  {
    "file": "web/e2e/studio.spec.mjs",
    "old": "    await page.getByTestId(\"ws-tab-layout\").click();\n  });\n\n  test(\"4. Core 3D Studio authoring: WebGL2 rasterization, raycast pick, RMB orbit, focus guard, cycle-safe parenting, camera/light live shading, and .fe3d + .fe3dbin round-trip\", async ({\n    page,\n  }) => {\n    await bootDirectStudioReady(page);",
    "new": "    await page.getByTestId(\"ws-tab-layout\").click();\n  });\n\n  test(\"4. Core 3D Studio authoring: WebGL2 rasterization, raycast pick, RMB orbit, focus guard, cycle-safe parenting, camera/light live shading, and .fe3d binary container round-trip\", async ({\n    page,\n  }) => {\n    await bootDirectStudioReady(page);"
  },
  {
    "file": "web/e2e/studio.spec.mjs",
    "old": "      .poll(async () => (await sampleViewportPixels(page)).hash)\n      .not.toBe(afterCam.hash);\n\n    // Export .fe3d + .fe3dbin bundle, reset via File menu, and reload bundle\n    await setControlValueAndCommit(page.getByTestId(\"meta-project-name\"), \"Stage5BundleTrip\");\n    await page.getByTestId(\"btn-add-sphere\").click();\n    await expect(page.getByTestId(\"stat-objects-meshes\")).toHaveText(\"8 / 3\");",
    "new": "      .poll(async () => (await sampleViewportPixels(page)).hash)\n      .not.toBe(afterCam.hash);\n\n    // Export the self-contained .fe3d binary container, reset via File menu, and reload it\n    await setControlValueAndCommit(page.getByTestId(\"meta-project-name\"), \"Stage5BundleTrip\");\n    await page.getByTestId(\"btn-add-sphere\").click();\n    await expect(page.getByTestId(\"stat-objects-meshes\")).toHaveText(\"8 / 3\");"
  },
  {
    "file": "web/e2e/studio.spec.mjs",
    "old": "    const onDl = (dl) => bundleDownloads.push(dl);\n    page.on(\"download\", onDl);\n    await page.getByTestId(\"btn-export-bundle\").click();\n    await expect.poll(() => bundleDownloads.length, { timeout: 10_000 }).toBe(2);\n    page.off(\"download\", onDl);\n\n    const savedPaths = [];",
    "new": "    const onDl = (dl) => bundleDownloads.push(dl);\n    page.on(\"download\", onDl);\n    await page.getByTestId(\"btn-export-bundle\").click();\n    await expect.poll(() => bundleDownloads.length, { timeout: 10_000 }).toBe(1);\n    page.off(\"download\", onDl);\n\n    const savedPaths = [];"
  },
  {
    "file": "web/e2e/studio.spec.mjs",
    "old": "    await context.close();\n  });\n\n  test(\"8. Stage 5.2 Camera & Directional Light viewport visibility, viewport picking, Move/Rotate/Scale gizmo transforms, multi-camera & multi-light independence, Undo/Redo, .fe3d/.fe3dbin persistence, and viewport screenshots\", async ({\n    browser,\n  }) => {\n    const outDir = path.resolve(",
    "new": "    await context.close();\n  });\n\n  test(\"8. Stage 5.2 Camera & Directional Light viewport visibility, viewport picking, Move/Rotate/Scale gizmo transforms, multi-camera & multi-light independence, Undo/Redo, .fe3d binary container persistence, and viewport screenshots\", async ({\n    browser,\n  }) => {\n    const outDir = path.resolve("
  },
  {
    "file": "web/e2e/studio.spec.mjs",
    "old": "      )\n      .not.toBe(heroPatchDark.hash);\n\n    // 5, 7, & 8. Multiple independent Cameras & Lights + Rename + Delete + Undo/Redo + .fe3d/.fe3dbin persistence without duplication\n    await page.getByTestId(\"btn-add-camera\").click();\n    await expect(page.getByTestId(\"stat-objects-meshes\")).toHaveText(\"6 / 2\");\n    await setControlValueAndCommit(page.getByTestId(\"insp-name\"), \"KeyShotCam\");",
    "new": "      )\n      .not.toBe(heroPatchDark.hash);\n\n    // 5, 7, & 8. Multiple independent Cameras & Lights + Rename + Delete + Undo/Redo + .fe3d binary container persistence without duplication\n    await page.getByTestId(\"btn-add-camera\").click();\n    await expect(page.getByTestId(\"stat-objects-meshes\")).toHaveText(\"6 / 2\");\n    await setControlValueAndCommit(page.getByTestId(\"insp-name\"), \"KeyShotCam\");"
  },
  {
    "file": "web/e2e/studio.spec.mjs",
    "old": "    await expect(page.getByTestId(\"stat-objects-meshes\")).toHaveText(\"7 / 2\");\n    await expect(page.getByTestId(\"outliner-item-RimLight\")).toBeVisible();\n\n    // Save/Export .fe3d + .fe3dbin bundle, reset to New Project, and reopen -> ZERO duplicate cameras/lights!\n    const tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), \"fe3d-s52-\"));\n    const downloads = [];\n    const onDl = (dl) => downloads.push(dl);\n    page.on(\"download\", onDl);\n    await page.getByTestId(\"btn-export-bundle\").click();\n    await expect.poll(() => downloads.length, { timeout: 10_000 }).toBe(2);\n    page.off(\"download\", onDl);\n\n    const bundlePaths = [];",
    "new": "    await expect(page.getByTestId(\"stat-objects-meshes\")).toHaveText(\"7 / 2\");\n    await expect(page.getByTestId(\"outliner-item-RimLight\")).toBeVisible();\n\n    // Save the .fe3d binary container, reset to New Project, and reopen -> ZERO duplicate cameras/lights!\n    const tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), \"fe3d-s52-\"));\n    const downloads = [];\n    const onDl = (dl) => downloads.push(dl);\n    page.on(\"download\", onDl);\n    await page.getByTestId(\"btn-export-bundle\").click();\n    await expect.poll(() => downloads.length, { timeout: 10_000 }).toBe(1);\n    page.off(\"download\", onDl);\n\n    const bundlePaths = [];"
  },
  {
    "file": "web/e2e/studio.spec.mjs",
    "old": "    });\n    expect(finalRenderNonClear).toBe(0);\n\n    const exportedEmptyJson = JSON.parse(\n      await page.evaluate(() => window.__FE3D_STUDIO__.exportInlineProject())\n    );\n    expect(exportedEmptyJson.scene.meshes).toHaveLength(0);\n    expect(exportedEmptyJson.scene.objects).toHaveLength(2);\n\n    // (6) & (7): Grid follows camera navigation, zoom-adaptive LOD, and Perspective + Orthographic views!\n    await page.mouse.move(box.x + box.width * 0.5, box.y + box.height * 0.5);",
    "new": "    });\n    expect(finalRenderNonClear).toBe(0);\n\n    const exportedEmptyState = await page.evaluate(() => window.__FE3D_STUDIO__.getState());\n    expect(exportedEmptyState.stats.mesh_count).toBe(0);\n    expect(exportedEmptyState.stats.object_count).toBe(2);\n\n    // (6) & (7): Grid follows camera navigation, zoom-adaptive LOD, and Perspective + Orthographic views!\n    await page.mouse.move(box.x + box.width * 0.5, box.y + box.height * 0.5);"
  },
  {
    "file": "web/e2e/studio.spec.mjs",
    "old": "    expect(planeOnlyStats.hash).not.toBe(withGridAndPlane.hash);\n    expect(planeOnlyStats.gridPx).toBeGreaterThan(80); // Real Plane object still renders!\n\n    // Save inline .fe3d with user Plane + re-import -> user Plane preserved untouched!\n    const savedPlaneProject = await page.evaluate(() => window.__FE3D_STUDIO__.exportInlineProject());\n    await page.evaluate((json) => window.__FE3D_STUDIO__.importInlineProject(json), savedPlaneProject);\n    await expect(page.getByTestId(\"outliner-item-Plane\")).toBeVisible();\n    await expect(page.getByTestId(\"stat-objects-meshes\")).toHaveText(\"3 / 1\");\n",
    "new": "    expect(planeOnlyStats.hash).not.toBe(withGridAndPlane.hash);\n    expect(planeOnlyStats.gridPx).toBeGreaterThan(80); // Real Plane object still renders!\n\n    // Save the binary .fe3d container with the user Plane + re-import -> Plane preserved untouched!\n    const savedPlaneProject = await page.evaluate(() => window.__FE3D_STUDIO__.exportProjectBase64());\n    await page.evaluate((b64) => window.__FE3D_STUDIO__.importProjectBase64(b64), savedPlaneProject);\n    await expect(page.getByTestId(\"outliner-item-Plane\")).toBeVisible();\n    await expect(page.getByTestId(\"stat-objects-meshes\")).toHaveText(\"3 / 1\");\n"
  },
  {
    "file": "web/e2e/studio.spec.mjs",
    "old": "      path: path.join(outDir, \"10-stage54-hdri-ibl-and-light-linking-1920x1080.png\"),\n    });\n\n    const exportedV13 = await page.evaluate(() => window.__FE3D_STUDIO__.exportInlineProject());\n    const parsedV13 = JSON.parse(exportedV13);\n    expect(parsedV13.schema_version.major).toBe(1);\n    expect(parsedV13.schema_version.minor).toBe(3);\n\n    // Load a genuine legacy v1.2 .fe3d document (no environment, no light shadow/linking keys) -> Item (18)\n    const legacyV12Json = JSON.stringify({\n      format: \"finaleffect.project\",\n      schema_version: { major: 1, minor: 2 },",
    "new": "      path: path.join(outDir, \"10-stage54-hdri-ibl-and-light-linking-1920x1080.png\"),\n    });\n\n    const exportedV13 = await page.evaluate(() => window.__FE3D_STUDIO__.exportProjectBase64());\n    const containerV13 = await page.evaluate(() => window.__FE3D_STUDIO__.projectContainerInfo());\n    expect(containerV13.magic_ok).toBe(true);\n    expect(containerV13.major).toBe(1);\n    expect(containerV13.minor).toBe(3);\n\n    // Legacy JSON project text is intentionally no longer loadable (binary-only format) -> Item (18)\n    const legacyV12Json = JSON.stringify({\n      format: \"finaleffect.project\",\n      schema_version: { major: 1, minor: 2 },"
  },
  {
    "file": "web/e2e/studio.spec.mjs",
    "old": "        ],\n      },\n    });\n    const okV12 = await page.evaluate(\n      (json) => window.__FE3D_STUDIO__.importInlineProject(json),\n      legacyV12Json\n    );\n    expect(okV12).toBe(true);\n    await expect(page.getByTestId(\"outliner-item-LegacySun\")).toContainText(\"SUN\");\n\n    // Re-import exported v1.3 project and verify all 4 light types, Light Linking, and HDRI restore cleanly!\n    const okV13 = await page.evaluate(\n      (json) => window.__FE3D_STUDIO__.importInlineProject(json),\n      exportedV13\n    );\n    expect(okV13).toBe(true);",
    "new": "        ],\n      },\n    });\n    const legacyAccepted = await page.evaluate(\n      (json) => window.__FE3D_STUDIO__.importProjectBase64(window.btoa(json)),\n      legacyV12Json\n    );\n    expect(legacyAccepted).toBe(false);\n\n    // Re-import the exported binary v1.3 container and verify all 4 light types, Light Linking and\n    // HDRI restore cleanly!\n    const okV13 = await page.evaluate(\n      (b64) => window.__FE3D_STUDIO__.importProjectBase64(b64),\n      exportedV13\n    );\n    expect(okV13).toBe(true);"
  },
  {
    "file": "web/e2e/studio.spec.mjs",
    "old": "    await page.getByTestId(\"layer-mode-layer1\").click();\n    await expect(page.getByTestId(\"layer-mode-layer1\")).toHaveText(\"R\");\n\n    // Verify .fe3d v1.3 export & re-import preserves Display Layers, Channel Locks & Custom Attributes cleanly\n    const exportedJson = await page.evaluate(() => window.__FE3D_STUDIO__.exportInlineProject());\n    expect(exportedJson).toContain('\"display_layers\"');\n    expect(exportedJson).toContain('\"rigBlend\"');\n\n    await page.evaluate(() => window.__FE3D_STUDIO__.dispatch({ action: \"new_starter_project\" }));\n    await expect(page.getByTestId(\"display-layer-row-layer1\")).toHaveCount(0);\n\n    await page.evaluate((txt) => window.__FE3D_STUDIO__.importInlineProject(txt), exportedJson);\n    await expect(page.getByTestId(\"display-layer-row-layer1\")).toBeVisible();\n    await page.getByTestId(\"outliner-item-HeroCube\").click();\n    await expect(page.getByTestId(\"cb-custom-rigBlend\")).toHaveValue(\"0.650\");",
    "new": "    await page.getByTestId(\"layer-mode-layer1\").click();\n    await expect(page.getByTestId(\"layer-mode-layer1\")).toHaveText(\"R\");\n\n    // Verify the binary .fe3d container export & re-import preserves Display Layers, Channel Locks\n    // & Custom Attributes cleanly\n    const exportedContainer = await page.evaluate(() => window.__FE3D_STUDIO__.exportProjectBase64());\n    expect(typeof exportedContainer).toBe(\"string\");\n    expect(exportedContainer.length).toBeGreaterThan(256);\n    const containerInfo = await page.evaluate(() => window.__FE3D_STUDIO__.projectContainerInfo());\n    expect(containerInfo.magic_ok).toBe(true);\n    expect(containerInfo.minor).toBe(3);\n\n    await page.evaluate(() => window.__FE3D_STUDIO__.dispatch({ action: \"new_starter_project\" }));\n    await expect(page.getByTestId(\"display-layer-row-layer1\")).toHaveCount(0);\n\n    await page.evaluate((b64) => window.__FE3D_STUDIO__.importProjectBase64(b64), exportedContainer);\n    await expect(page.getByTestId(\"display-layer-row-layer1\")).toBeVisible();\n    await page.getByTestId(\"outliner-item-HeroCube\").click();\n    await expect(page.getByTestId(\"cb-custom-rigBlend\")).toHaveValue(\"0.650\");"
  }
];

let changed = 0;
let skipped = 0;
for (const edit of EDITS) {
  const text = readFileSync(edit.file, "utf8");
  if (text.includes(edit.new)) {
    skipped += 1;
    continue;
  }
  const occurrences = text.split(edit.old).length - 1;
  if (occurrences !== 1) {
    console.error("edit for " + edit.file + " matched " + occurrences + " times (expected 1)");
    process.exit(1);
  }
  writeFileSync(edit.file, text.replace(edit.old, edit.new));
  changed += 1;
}
console.log("applied " + changed + " edit(s), already present " + skipped);
