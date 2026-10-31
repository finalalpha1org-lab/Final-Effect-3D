// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Unified Creative Application & 3D Studio Controller (Stage 5 v0.5.0)
// Bridges Splash -> Home / 6-Studio Launcher -> Blender-Inspired 3D Studio directly to
// the C++20 WebAssembly Engine, EditorController, StudioSession, and Serializer.

(function () {
  "use strict";

  const REG = window.FE3D_REGISTRY;
  const WORKSPACE_STORAGE_KEY = "fe3d.workspace.v2";

  const canvas = document.getElementById("fe3d-canvas");
  const banner = document.getElementById("viewport-banner");
  const previewCanvas = document.getElementById("material-preview-canvas");
  const previewCtx = previewCanvas ? previewCanvas.getContext("2d") : null;

  function hydrateStaticIcons(root) {
    if (!REG || !REG.ICONS) return;
    const scope = root || document;
    const splashLogo = document.getElementById("splash-logo-slot");
    if (splashLogo && !splashLogo.innerHTML) {
      splashLogo.innerHTML = REG.ICONS.logo;
    }
    scope.querySelectorAll("[data-icon]").forEach((el) => {
      const key = el.getAttribute("data-icon");
      if (REG.ICONS[key]) {
        el.innerHTML = REG.ICONS[key];
      }
    });
  }
  hydrateStaticIcons(document);

  // Real Splash Screen Telemetry State (driven strictly by actual WASM/WebGL2 milestones).
  const splashEls = {
    screen: document.getElementById("app-splash"),
    statusText: document.getElementById("splash-status-text"),
    pct: document.getElementById("splash-pct"),
    progressbar: document.getElementById("splash-progressbar"),
    barFill: document.getElementById("splash-bar-fill"),
    milestones: document.getElementById("splash-milestones"),
    spinner: document.getElementById("splash-spinner"),
    errorBox: document.getElementById("splash-error-box"),
    errorDetail: document.getElementById("splash-error-detail"),
  };

  function setSplashProgress(stageIndex, pctValue, statusMsg) {
    const clamped = Math.max(0, Math.min(100, Math.round(pctValue)));
    if (splashEls.statusText && statusMsg) splashEls.statusText.textContent = statusMsg;
    if (splashEls.pct) splashEls.pct.textContent = clamped + "%";
    if (splashEls.progressbar) splashEls.progressbar.setAttribute("aria-valuenow", String(clamped));
    if (splashEls.barFill) splashEls.barFill.style.width = clamped + "%";
    if (splashEls.milestones) {
      splashEls.milestones.querySelectorAll("li[data-stage]").forEach((li) => {
        const s = Number(li.getAttribute("data-stage"));
        li.classList.toggle("is-done", s < stageIndex || clamped >= 100);
        li.classList.toggle("is-active", s === stageIndex && clamped < 100);
      });
    }
  }

  function showSplashFatalError(detailMsg) {
    if (splashEls.spinner) splashEls.spinner.hidden = true;
    if (splashEls.errorBox) splashEls.errorBox.hidden = false;
    if (splashEls.errorDetail) {
      splashEls.errorDetail.textContent =
        detailMsg || "WebAssembly or WebGL 2.0 context failed to initialize.";
    }
    if (splashEls.statusText) {
      splashEls.statusText.textContent = "Initialization halted.";
    }
  }

  // Configure Emscripten Module BEFORE finaleffect_web.js executes.
  window.Module = {
    canvas: canvas,
    __fe3dRuntimeReady: false,
    onRuntimeInitialized: function () {
      window.Module.__fe3dRuntimeReady = true;
    },
    print: function (text) {
      console.log("[FE3D]", text);
    },
    printErr: function (text) {
      console.warn("[FE3D]", text);
    },
    setStatus: function (text) {
      if (!text) return;
      const m = text.match(/\((\d+(?:\.\d+)?)\/(\d+)\)/);
      if (m) {
        const loaded = parseFloat(m[1]);
        const total = Math.max(1, parseFloat(m[2]));
        const dlPct = 8 + (loaded / total) * 24;
        setSplashProgress(1, dlPct, text);
      } else {
        setSplashProgress(1, 20, text);
      }
    },
    onAbort: function (reason) {
      showSplashFatalError("WebAssembly runtime aborted: " + String(reason || "Unknown abort"));
    },
  };

  window.addEventListener("error", (ev) => {
    if (
      document.body.dataset.appView === "splash" &&
      ev &&
      ev.filename &&
      ev.filename.indexOf("finaleffect_web") !== -1
    ) {
      showSplashFatalError("Failed to load finaleffect_web.js / .wasm asset.");
    }
  });

  const els = {
    appSplash: document.getElementById("app-splash"),
    appHome: document.getElementById("app-home"),
    appStudio: document.getElementById("app-studio-3d"),

    homeEngineBadge: document.getElementById("home-engine-badge"),
    btnHomeResumeStudio: document.getElementById("btn-home-resume-studio"),
    btnOpenSettings: document.getElementById("btn-open-settings"),
    studioSelectorList: document.getElementById("studio-selector-list"),

    studioLauncherPanel: document.getElementById("studio-launcher-panel"),
    launcherStudioBadge: document.getElementById("launcher-studio-badge"),
    launcherStudioTitle: document.getElementById("launcher-studio-title"),
    launcherStudioDesc: document.getElementById("launcher-studio-desc"),
    launcherErrorBanner: document.getElementById("launcher-error-banner"),
    launcherProjectName: document.getElementById("launcher-project-name"),
    launcherProjectAuthor: document.getElementById("launcher-project-author"),
    launcherTemplateGrid: document.getElementById("launcher-template-grid"),
    btnLauncherCreate: document.getElementById("btn-launcher-create"),
    btnLauncherOpen: document.getElementById("btn-launcher-open"),
    recentCountTag: document.getElementById("recent-count-tag"),
    recentProjectsList: document.getElementById("recent-projects-list"),

    studioFuturePanel: document.getElementById("studio-future-panel"),
    futureStudioBadge: document.getElementById("future-studio-badge"),
    futureStudioTitle: document.getElementById("future-studio-title"),
    futureStudioTagline: document.getElementById("future-studio-tagline"),
    futureStudioDesc: document.getElementById("future-studio-desc"),
    futureStudioCaps: document.getElementById("future-studio-caps"),
    btnFutureBack3d: document.getElementById("btn-future-back-3d"),
    btnFutureJumpShading: document.getElementById("btn-future-jump-shading"),

    btnGoHome: document.getElementById("btn-go-home"),
    workspaceTabs: document.getElementById("workspace-tabs"),
    btnWsScrollLeft: document.getElementById("btn-ws-scroll-left"),
    btnWsScrollRight: document.getElementById("btn-ws-scroll-right"),
    workspacePresetSelect: document.getElementById("workspace-preset-select"),
    activeProjectPill: document.getElementById("active-project-pill"),
    engineBadge: document.getElementById("engine-version-badge"),

    btnNew: document.getElementById("btn-new-project"),
    btnOpen: document.getElementById("btn-open-project"),
    fileOpenInput: document.getElementById("file-open-input"),
    btnExportFe3d: document.getElementById("btn-export-fe3d"),
    btnExportBundle: document.getElementById("btn-export-bundle"),
    btnStudioSettings: document.getElementById("btn-studio-settings"),
    btnUndo: document.getElementById("btn-undo"),
    btnRedo: document.getElementById("btn-redo"),
    btnDuplicate: document.getElementById("btn-duplicate"),
    btnDelete: document.getElementById("btn-delete"),
    btnFrame: document.getElementById("btn-frame"),
    btnToggleRig: document.getElementById("btn-toggle-rig"),
    btnClearSelection: document.getElementById("btn-clear-selection"),

    btnModeObject: document.getElementById("btn-mode-object"),
    btnModeVertex: document.getElementById("btn-mode-vertex"),
    btnModeEdge: document.getElementById("btn-mode-edge"),
    btnModeFace: document.getElementById("btn-mode-face"),
    ctxMeshSubbar: document.getElementById("ctx-mesh-subbar"),
    topbarSubbar: document.getElementById("topbar-subbar"),
    btnLandscapeInspectorToggle: document.getElementById("btn-landscape-inspector-toggle"),

    toolShelf: document.getElementById("tool-shelf"),
    btnToolSelect: document.getElementById("btn-tool-select"),
    btnGizmoTranslate: document.getElementById("btn-gizmo-translate"),
    btnGizmoRotate: document.getElementById("btn-gizmo-rotate"),
    btnGizmoScale: document.getElementById("btn-gizmo-scale"),
    btnGizmoSpace: document.getElementById("btn-gizmo-space"),
    btnCollapseShelf: document.getElementById("btn-collapse-shelf"),
    btnVpRestoreShelf: document.getElementById("btn-vp-restore-shelf"),
    btnVpRestoreRight: document.getElementById("btn-vp-restore-right"),

    studioWorkspace: document.getElementById("studio-workspace"),
    centerColumn: document.getElementById("center-column"),
    viewportStage: document.getElementById("viewport-stage"),
    wfBanner: document.getElementById("workspace-foundation-banner"),
    wfBannerTitle: document.getElementById("wf-banner-title"),
    wfBannerDetail: document.getElementById("wf-banner-detail"),
    viewportHudTop: document.getElementById("viewport-hud-top"),
    viewportHudBottom: document.getElementById("viewport-hud-bottom"),
    vpCameraPill: document.getElementById("vp-camera-pill"),
    vpSelectionPill: document.getElementById("vp-selection-pill"),
    vpModePill: document.getElementById("vp-mode-pill"),
    viewportViewGizmo: document.getElementById("viewport-view-gizmo"),
    viewNavGizmoCanvas: document.getElementById("view-nav-gizmo-canvas"),
    viewGizmoHoverTip: document.getElementById("view-gizmo-hover-tip"),
    btnNavHomeView: document.getElementById("btn-nav-home-view"),
    btnNavProjToggle: document.getElementById("btn-nav-proj-toggle"),
    btnTouchMultiSelect: document.getElementById("btn-touch-multiselect"),
    viewportContextMenu: document.getElementById("viewport-context-menu"),
    ctxMenuTitle: document.getElementById("ctx-menu-title"),
    ctxCmdDuplicate: document.getElementById("ctx-cmd-duplicate"),
    ctxCmdResetTf: document.getElementById("ctx-cmd-reset-tf"),
    ctxMeshSep: document.getElementById("ctx-mesh-sep"),
    ctxCmdExtrude: document.getElementById("ctx-cmd-extrude"),
    ctxCmdBoundary: document.getElementById("ctx-cmd-boundary"),
    ctxCmdFill: document.getElementById("ctx-cmd-fill"),
    ctxCmdDelete: document.getElementById("ctx-cmd-delete"),

    splitterBottom: document.getElementById("splitter-bottom"),
    splitterRight: document.getElementById("splitter-right"),
    splitterOutliner: document.getElementById("splitter-outliner"),

    dockBottom: document.getElementById("dock-bottom"),
    bottomEditorSelect: document.getElementById("bottom-editor-select"),
    tabTimeline: document.getElementById("tab-timeline"),
    tabAssetBrowser: document.getElementById("tab-asset-browser"),
    tabConsole: document.getElementById("tab-console"),
    btnCollapseBottom: document.getElementById("btn-collapse-bottom"),
    paneTimeline: document.getElementById("pane-timeline"),
    paneAssetBrowser: document.getElementById("pane-asset-browser"),
    paneConsole: document.getElementById("pane-console"),
    paneFoundationEditor: document.getElementById("pane-foundation-editor"),
    fePaneTitle: document.getElementById("fe-pane-title"),
    fePaneDesc: document.getElementById("fe-pane-desc"),
    fePaneBinding: document.getElementById("fe-pane-binding"),
    btnTimelinePlay: document.getElementById("btn-timeline-play"),
    lblTimelinePlay: document.getElementById("lbl-timeline-play"),
    timelineFrameNum: document.getElementById("timeline-frame-num"),
    timelineScrubber: document.getElementById("timeline-scrubber"),
    assetBrowserList: document.getElementById("asset-browser-list"),
    consoleLogList: document.getElementById("console-log-list"),

    dockRight: document.getElementById("dock-right"),
    panelOutliner: document.getElementById("panel-outliner"),
    btnCollapseRight: document.getElementById("btn-collapse-right"),
    outlinerFilter: document.getElementById("outliner-filter"),
    outlinerList: document.getElementById("outliner-list"),

    propertiesShell: document.getElementById("properties-shell"),
    inspectorTabs: document.getElementById("inspector-tabs"),
    propsActiveHeading: document.getElementById("props-active-heading"),
    panelProperties: document.getElementById("panel-properties"),
    panelMaterialEditor: document.getElementById("panel-material-editor"),

    inspectorIdBadge: document.getElementById("inspector-id-badge"),
    inspectorEmpty: document.getElementById("inspector-empty"),
    inspectorBody: document.getElementById("inspector-body"),
    inspName: document.getElementById("insp-name"),
    inspParent: document.getElementById("insp-parent"),
    inspMaterialRow: document.getElementById("insp-material-row"),
    inspMaterial: document.getElementById("insp-material"),
    inspVisible: document.getElementById("insp-visible"),
    btnResetTransform: document.getElementById("btn-reset-transform"),

    posX: document.getElementById("tf-pos-x"),
    posY: document.getElementById("tf-pos-y"),
    posZ: document.getElementById("tf-pos-z"),
    rotX: document.getElementById("tf-rot-x"),
    rotY: document.getElementById("tf-rot-y"),
    rotZ: document.getElementById("tf-rot-z"),
    sclX: document.getElementById("tf-scl-x"),
    sclY: document.getElementById("tf-scl-y"),
    sclZ: document.getElementById("tf-scl-z"),

    cardMeshEdit: document.getElementById("card-mesh-edit"),
    meshEditSelectionTag: document.getElementById("mesh-edit-selection-tag"),
    meshEditTopology: document.getElementById("mesh-edit-topology"),
    meshExtrudeDist: document.getElementById("mesh-extrude-dist"),
    btnMeshExtrude: document.getElementById("btn-mesh-extrude"),
    btnMeshBoundary: document.getElementById("btn-mesh-boundary"),
    btnMeshFill: document.getElementById("btn-mesh-fill"),
    btnMeshAssignMat: document.getElementById("btn-mesh-assign-mat"),
    btnMeshDelete: document.getElementById("btn-mesh-delete"),

    cardMesh: document.getElementById("card-mesh"),
    meshAssetName: document.getElementById("mesh-asset-name"),
    meshGeoCounts: document.getElementById("mesh-geo-counts"),
    meshBounds: document.getElementById("mesh-bounds"),

    cardCamera: document.getElementById("card-camera"),
    cardLight: document.getElementById("card-light"),
    cardDataEmpty: document.getElementById("card-data-empty"),
    btnToggleCameraView: document.getElementById("btn-toggle-camera-view"),
    btnSetActiveCamera: document.getElementById("btn-set-active-camera"),
    camProjection: document.getElementById("cam-projection"),
    camFov: document.getElementById("cam-fov"),
    camOrtho: document.getElementById("cam-ortho"),
    camNear: document.getElementById("cam-near"),
    camFar: document.getElementById("cam-far"),
    lightCardTitle: document.getElementById("light-card-title"),
    lightEnabled: document.getElementById("light-enabled"),
    lightType: document.getElementById("light-type"),
    lightColor: document.getElementById("light-color"),
    lightIntensity: document.getElementById("light-intensity"),
    lightShapeBlock: document.getElementById("light-shape-block"),
    lightRowAreaShape: document.getElementById("light-row-area-shape"),
    lightAreaShape: document.getElementById("light-area-shape"),
    lightRowAreaWidth: document.getElementById("light-row-area-width"),
    lblLightAreaWidth: document.getElementById("lbl-light-area-width"),
    lightAreaWidth: document.getElementById("light-area-width"),
    lightRowAreaHeight: document.getElementById("light-row-area-height"),
    lightAreaHeight: document.getElementById("light-area-height"),
    lightRowAreaTwoSided: document.getElementById("light-row-area-twosided"),
    lightAreaTwoSided: document.getElementById("light-area-twosided"),
    lightRowAreaSamples: document.getElementById("light-row-area-samples"),
    lightAreaSamples: document.getElementById("light-area-samples"),
    lightRowRange: document.getElementById("light-row-range"),
    lightRange: document.getElementById("light-range"),
    lightRowFalloff: document.getElementById("light-row-falloff"),
    lightFalloff: document.getElementById("light-falloff"),
    lightRowAttenuation: document.getElementById("light-row-attenuation"),
    lightAttenuation: document.getElementById("light-attenuation"),
    lightRowSourceRadius: document.getElementById("light-row-source-radius"),
    lightSourceRadius: document.getElementById("light-source-radius"),
    lightRowSpotCones: document.getElementById("light-row-spot-cones"),
    lightInnerCone: document.getElementById("light-inner-cone"),
    lightOuterCone: document.getElementById("light-outer-cone"),
    lightRowConeFalloff: document.getElementById("light-row-cone-falloff"),
    lightConeFalloff: document.getElementById("light-cone-falloff"),
    lightShadowEnabled: document.getElementById("light-shadow-enabled"),
    lightShadowQuality: document.getElementById("light-shadow-quality"),
    lightShadowFilter: document.getElementById("light-shadow-filter"),
    lightShadowStrength: document.getElementById("light-shadow-strength"),
    lightShadowBias: document.getElementById("light-shadow-bias"),
    lightShadowNormalBias: document.getElementById("light-shadow-normal-bias"),
    lightShadowSoftness: document.getElementById("light-shadow-softness"),
    lightLinkMode: document.getElementById("light-link-mode"),
    lightLinkListWrap: document.getElementById("light-link-list-wrap"),
    lightLinkDescendants: document.getElementById("light-link-descendants"),
    lightLinkTargets: document.getElementById("light-link-targets"),

    fileOpenHdri: document.getElementById("file-open-hdri"),
    envEnabled: document.getElementById("env-enabled"),
    envHdriStatus: document.getElementById("env-hdri-status"),
    btnLoadHdri: document.getElementById("btn-load-hdri"),
    btnClearHdri: document.getElementById("btn-clear-hdri"),
    envIntensity: document.getElementById("env-intensity"),
    valEnvIntensity: document.getElementById("val-env-intensity"),
    envRotation: document.getElementById("env-rotation"),
    valEnvRotation: document.getElementById("val-env-rotation"),
    envDiffuseScale: document.getElementById("env-diffuse-scale"),
    valEnvDiffuse: document.getElementById("val-env-diffuse"),
    envSpecularScale: document.getElementById("env-specular-scale"),
    valEnvSpecular: document.getElementById("val-env-specular"),

    btnMatNew: document.getElementById("btn-mat-new"),
    btnMatDup: document.getElementById("btn-mat-dup"),
    btnMatDel: document.getElementById("btn-mat-del"),
    matSelect: document.getElementById("mat-select"),
    matName: document.getElementById("mat-name"),
    btnMatAssignObj: document.getElementById("btn-mat-assign-obj"),
    matBaseColor: document.getElementById("mat-base-color"),
    matMetallic: document.getElementById("mat-metallic"),
    valMatMetallic: document.getElementById("val-mat-metallic"),
    matRoughness: document.getElementById("mat-roughness"),
    valMatRoughness: document.getElementById("val-mat-roughness"),
    matAo: document.getElementById("mat-ao"),
    valMatAo: document.getElementById("val-mat-ao"),
    matEmissiveColor: document.getElementById("mat-emissive-color"),
    matEmissiveStrength: document.getElementById("mat-emissive-strength"),
    matOpacity: document.getElementById("mat-opacity"),
    valMatOpacity: document.getElementById("val-mat-opacity"),

    metaProjectName: document.getElementById("meta-project-name"),
    metaAuthor: document.getElementById("meta-author"),
    metaSceneName: document.getElementById("meta-scene-name"),

    statObjectsMeshes: document.getElementById("stat-objects-meshes"),
    statMaterialsUpdates: document.getElementById("stat-materials-updates"),
    statLightsShadows: document.getElementById("stat-lights-shadows"),
    statTriangles: document.getElementById("stat-triangles"),
    statMemory: document.getElementById("stat-memory"),
    statLightingGpu: document.getElementById("stat-lighting-gpu"),
    statLightingWarning: document.getElementById("stat-lighting-warning"),
    statHistory: document.getElementById("stat-history"),

    mobilePortraitNav: document.getElementById("mobile-portrait-nav"),
    statusDot: document.getElementById("status-dot"),
    statusMessage: document.getElementById("status-message"),
    statusLayoutMode: document.getElementById("status-layout-mode"),
    statusRenderer: document.getElementById("status-renderer"),

    settingsModal: document.getElementById("settings-modal"),
    btnCloseSettings: document.getElementById("btn-close-settings"),
    prefLayoutMode: document.getElementById("pref-layout-mode"),
    prefUiDensity: document.getElementById("pref-ui-density"),
    prefDefaultAuthor: document.getElementById("pref-default-author"),
    prefShowHud: document.getElementById("pref-show-hud"),
    btnPrefSaveLayout: document.getElementById("btn-pref-save-layout"),
    btnPrefRestoreLayout: document.getElementById("btn-pref-restore-layout"),
  };

  let wasmApi = null;
  let currentState = null;
  let lastRevision = -1;
  let lastPreviewGen = -1;
  let lastOutlinerSignature = "";
  let lastAssetsSignature = "";
  let lastConsoleSignature = "";
  let lastInspParentSig = "";
  let lastInspMatSig = "";
  let lastMatSelectSig = "";
  let lastLightLinkSig = "";
  let lastAutoInspObjId = 0;

  let appPrefs = REG.loadPreferences();
  let selectedStudioId = "3d_studio";
  let selectedTemplateId = "starter";
  let hasEnteredStudioOnce = false;
  let activeProjectRecordId = null;

  let activeWorkspaceId = "layout";
  let activeInspTab = "all";
  let activeBottomEditor = "timeline";
  let activeMobileDrawer = "viewport";
  let outlinerFilterQuery = "";
  let isRightDockCollapsed = false;

  let touchNavMode = "orbit";
  let touchMultiSelect = false;

  function closeViewportContextMenu() {
    if (els.viewportContextMenu) {
      els.viewportContextMenu.hidden = true;
    }
  }

  function updateWorkspaceTabsOverflowUi() {
    const strip = els.workspaceTabs;
    if (!strip || !els.btnWsScrollLeft || !els.btnWsScrollRight) return;
    const maxScroll = Math.max(0, strip.scrollWidth - strip.clientWidth);
    const hasOverflow = maxScroll > 4;
    els.btnWsScrollLeft.hidden = !hasOverflow;
    els.btnWsScrollRight.hidden = !hasOverflow;
    if (hasOverflow) {
      els.btnWsScrollLeft.disabled = strip.scrollLeft <= 2;
      els.btnWsScrollRight.disabled = strip.scrollLeft >= maxScroll - 2;
    }
  }

  // =========================================================================
  // RESPONSIVE LAYOUT & PREFERENCES ENGINE
  // =========================================================================

  function computeEffectiveLayoutMode() {
    if (appPrefs.layoutMode && appPrefs.layoutMode !== "auto") {
      return appPrefs.layoutMode;
    }
    const w = window.innerWidth || document.documentElement.clientWidth || 1280;
    const h = window.innerHeight || document.documentElement.clientHeight || 720;
    if (w >= 960) {
      return "desktop";
    }
    return w > h ? "landscape" : "portrait";
  }

  function applyPreferencesAndResponsiveMode() {
    const mode = computeEffectiveLayoutMode();
    document.body.dataset.layoutMode = mode;
    document.body.dataset.uiDensity = appPrefs.uiDensity || "comfortable";
    if (!document.body.dataset.mpDrawer) {
      document.body.dataset.mpDrawer = activeMobileDrawer;
    }

    closeViewportContextMenu();

    // Preserve central 3D Viewport priority when Desktop window dimensions shrink
    if (mode === "desktop") {
      const w = window.innerWidth || 1280;
      const h = window.innerHeight || 720;
      const inlineRight = parseFloat(document.documentElement.style.getPropertyValue("--right-dock-w"));
      if (Number.isFinite(inlineRight) && inlineRight > 0) {
        const maxRight = Math.max(240, Math.min(520, Math.floor(w * 0.42)));
        if (inlineRight > maxRight) {
          document.documentElement.style.setProperty("--right-dock-w", maxRight + "px");
        }
      }
      const inlineBottom = parseFloat(document.documentElement.style.getPropertyValue("--bottom-dock-h"));
      if (Number.isFinite(inlineBottom) && inlineBottom > 0) {
        const maxBottom = Math.max(84, Math.min(360, Math.floor((h - 90) * 0.46)));
        if (inlineBottom > maxBottom) {
          document.documentElement.style.setProperty("--bottom-dock-h", maxBottom + "px");
        }
      }
    }

    if (els.btnLandscapeInspectorToggle) {
      els.btnLandscapeInspectorToggle.classList.toggle(
        "is-active",
        mode === "landscape" && !document.body.classList.contains("is-landscape-drawer-closed")
      );
    }

    if (els.statusLayoutMode) {
      els.statusLayoutMode.textContent =
        mode === "desktop"
          ? "Desktop"
          : mode === "landscape"
          ? "Landscape Pro"
          : "Portrait Mobile";
    }

    const hudHidden = appPrefs.showViewportHud === false;
    if (els.viewportHudTop) els.viewportHudTop.hidden = hudHidden;
    if (els.viewportHudBottom) els.viewportHudBottom.hidden = hudHidden;

    if (els.prefLayoutMode) els.prefLayoutMode.value = appPrefs.layoutMode || "auto";
    if (els.prefUiDensity) els.prefUiDensity.value = appPrefs.uiDensity || "comfortable";
    if (els.prefDefaultAuthor) els.prefDefaultAuthor.value = appPrefs.defaultAuthor || "Final Effect 3D";
    if (els.prefShowHud) els.prefShowHud.checked = appPrefs.showViewportHud !== false;

    if (currentState) {
      renderWorkspaceAndBottomDock(currentState);
    }
    requestAnimationFrame(updateWorkspaceTabsOverflowUi);
  }

  window.addEventListener("resize", () => {
    applyPreferencesAndResponsiveMode();
  });
  window.addEventListener("orientationchange", () => {
    setTimeout(applyPreferencesAndResponsiveMode, 50);
  });
  applyPreferencesAndResponsiveMode();

  // =========================================================================
  // THREE-LEVEL VIEW NAVIGATION (Splash -> Home / Launcher -> 3D Studio)
  // =========================================================================

  function setAppView(view) {
    document.body.dataset.appView = view;
    closeViewportContextMenu();
    if (els.appSplash) els.appSplash.hidden = view !== "splash";
    if (els.appHome) els.appHome.hidden = view !== "home";
    if (view === "studio") {
      hasEnteredStudioOnce = true;
      if (els.btnHomeResumeStudio) els.btnHomeResumeStudio.hidden = false;
      syncFromEngine(true);
      requestAnimationFrame(updateWorkspaceTabsOverflowUi);
    } else if (view === "home") {
      renderRecentProjectsList();
      if (els.btnHomeResumeStudio) {
        els.btnHomeResumeStudio.hidden = !hasEnteredStudioOnce;
      }
    }
  }

  function captureViewportThumbnail() {
    try {
      if (!canvas || canvas.width < 8 || canvas.height < 8) return null;
      const thumbCanvas = document.createElement("canvas");
      thumbCanvas.width = 160;
      thumbCanvas.height = 100;
      const ctx2d = thumbCanvas.getContext("2d");
      if (!ctx2d) return null;
      ctx2d.drawImage(canvas, 0, 0, thumbCanvas.width, thumbCanvas.height);
      return thumbCanvas.toDataURL("image/jpeg", 0.72);
    } catch (_) {
      return null;
    }
  }

  async function snapshotActiveProjectToRecent(sourceLabel) {
    if (!wasmApi || !currentState) return;
    const inlineJson = wasmApi.exportInlineProject();
    if (!inlineJson) return;
    const thumb = captureViewportThumbnail();
    const rec = await REG.recordRecentProject(
      {
        id: activeProjectRecordId,
        name: currentState.project.project_name || "Untitled Project",
        author: currentState.project.author || appPrefs.defaultAuthor || "Final Effect 3D",
        studioId: "3d_studio",
        sceneName: currentState.project.scene_name || "Scene",
        objectCount: currentState.stats ? currentState.stats.object_count : 0,
        materialCount: currentState.stats ? currentState.stats.material_count : 0,
        sourceLabel: sourceLabel || "3D Studio Session",
        thumbnailDataUrl: thumb,
      },
      inlineJson
    );
    activeProjectRecordId = rec.id;
  }

  // =========================================================================
  // LEVEL 2: STUDIO SELECTOR & PROJECT LAUNCHER
  // =========================================================================

  function renderStudioSelector() {
    if (!els.studioSelectorList) return;
    els.studioSelectorList.innerHTML = "";
    REG.STUDIOS.forEach((st) => {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.className =
        "studio-nav-card" +
        (st.id === selectedStudioId ? " is-selected" : "") +
        (!st.operational ? " is-planned-studio" : "");
      btn.dataset.testid = "studio-card-" + st.id;
      btn.innerHTML =
        '<span class="studio-nav-ic">' +
        (REG.ICONS[st.icon] || REG.ICONS.cube) +
        "</span>" +
        '<span class="studio-nav-meta">' +
        '<span class="studio-nav-title">' +
        escapeHtml(st.name) +
        "</span>" +
        '<span class="studio-nav-state">' +
        (st.operational ? "Operational" : "Planned") +
        "</span>" +
        "</span>";
      btn.addEventListener("click", () => {
        selectedStudioId = st.id;
        renderStudioSelector();
        renderSelectedStudioHomePanel();
      });
      els.studioSelectorList.appendChild(btn);
    });
  }

  function renderSelectedStudioHomePanel() {
    const st = REG.STUDIOS.find((item) => item.id === selectedStudioId) || REG.STUDIOS[0];
    if (els.launcherErrorBanner) els.launcherErrorBanner.hidden = true;

    if (st.operational) {
      els.studioLauncherPanel.hidden = false;
      els.studioFuturePanel.hidden = true;
      els.launcherStudioBadge.textContent = st.badge;
      els.launcherStudioTitle.textContent = st.name;
      els.launcherStudioDesc.textContent = st.description;

      els.launcherTemplateGrid.innerHTML = "";
      (st.templates || []).forEach((tmpl) => {
        const card = document.createElement("button");
        card.type = "button";
        card.className = "template-card" + (tmpl.id === selectedTemplateId ? " is-selected" : "");
        card.dataset.testid = "template-card-" + tmpl.id;
        card.innerHTML =
          '<div class="template-card-head"><span class="btn-ic">' +
          (REG.ICONS[tmpl.icon] || REG.ICONS.cube) +
          "</span><span>" +
          escapeHtml(tmpl.name) +
          "</span></div>" +
          '<div class="template-card-sub">' +
          escapeHtml(tmpl.subtitle) +
          "</div>";
        card.addEventListener("click", () => {
          selectedTemplateId = tmpl.id;
          renderSelectedStudioHomePanel();
        });
        els.launcherTemplateGrid.appendChild(card);
      });
      renderRecentProjectsList();
    } else {
      els.studioLauncherPanel.hidden = true;
      els.studioFuturePanel.hidden = false;
      els.futureStudioBadge.textContent = st.badge;
      els.futureStudioTitle.textContent = st.name;
      els.futureStudioTagline.textContent = st.tagline;
      els.futureStudioDesc.textContent = st.description;
      els.futureStudioCaps.innerHTML = "";
      (st.plannedCapabilities || []).forEach((cap) => {
        const li = document.createElement("li");
        li.textContent = cap;
        els.futureStudioCaps.appendChild(li);
      });
      if (els.btnFutureJumpShading) {
        els.btnFutureJumpShading.hidden = !st.jumpTo3dWorkspace;
      }
    }
  }

  function formatRelativeTimestamp(ts) {
    if (!ts) return "";
    const diffSec = Math.max(0, Math.round((Date.now() - ts) / 1000));
    if (diffSec < 60) return "Just now";
    if (diffSec < 3600) return Math.floor(diffSec / 60) + "m ago";
    if (diffSec < 86400) return Math.floor(diffSec / 3600) + "h ago";
    return new Date(ts).toLocaleDateString();
  }

  function renderRecentProjectsList() {
    if (!els.recentProjectsList) return;
    const recents = REG.listRecentProjects();
    if (els.recentCountTag) {
      els.recentCountTag.textContent = recents.length + " saved";
    }
    els.recentProjectsList.innerHTML = "";
    if (recents.length === 0) {
      const empty = document.createElement("div");
      empty.className = "recent-empty";
      empty.dataset.testid = "recent-empty-state";
      empty.textContent =
        "No recent projects recorded yet. Create a new project or open an existing .fe3d file to populate real history.";
      els.recentProjectsList.appendChild(empty);
      return;
    }

    recents.forEach((item) => {
      const row = document.createElement("div");
      row.className = "recent-item";
      row.dataset.testid = "recent-project-item";

      const thumbHtml = item.thumbnailDataUrl
        ? '<img src="' + item.thumbnailDataUrl + '" alt="" />'
        : '<span class="btn-ic">' + REG.ICONS.cube + "</span>";

      row.innerHTML =
        '<div class="recent-thumb">' +
        thumbHtml +
        "</div>" +
        '<div class="recent-info">' +
        '<div class="recent-name">' +
        escapeHtml(item.name) +
        "</div>" +
        '<div class="recent-sub">' +
        escapeHtml(
          "3D Studio · " +
            item.objectCount +
            " obj · " +
            item.materialCount +
            " mat · " +
            formatRelativeTimestamp(item.updatedAt)
        ) +
        "</div>" +
        "</div>" +
        '<div class="recent-actions">' +
        '<button type="button" class="pro-btn" data-reopen-id="' +
        escapeHtml(item.id) +
        '" data-testid="btn-reopen-recent">Open</button>' +
        '<button type="button" class="ghost-btn" data-remove-id="' +
        escapeHtml(item.id) +
        '" data-testid="btn-remove-recent" title="Remove from Recent Projects list (does not delete local files)">Remove</button>' +
        "</div>";

      row.querySelector("[data-reopen-id]").addEventListener("click", async () => {
        await reopenRecentProjectById(item);
      });
      row.querySelector("[data-remove-id]").addEventListener("click", async () => {
        if (activeProjectRecordId === item.id) activeProjectRecordId = null;
        await REG.removeRecentProject(item.id);
        renderRecentProjectsList();
      });

      els.recentProjectsList.appendChild(row);
    });
  }

  async function reopenRecentProjectById(item) {
    if (!wasmApi) return;
    if (els.launcherErrorBanner) els.launcherErrorBanner.hidden = true;
    const payload = await REG.loadProjectPayload(item.id);
    if (!payload) {
      if (els.launcherErrorBanner) {
        els.launcherErrorBanner.hidden = false;
        els.launcherErrorBanner.textContent =
          "Cached project data for '" +
          item.name +
          "' is no longer available in browser storage. Open the .fe3d file from disk or remove this recent entry.";
      }
      return;
    }
    try {
      wasmApi.fs.mkdir("/fe3d_import");
    } catch (_) {}
    const vfsPath = "/fe3d_import/recent_reopen.fe3d";
    wasmApi.fs.writeFile(vfsPath, payload);
    const ok = wasmApi.loadFileFromVfs(vfsPath);
    if (!ok) {
      const errMsg =
        currentState && currentState.status ? currentState.status.message : "Corrupted project data";
      if (els.launcherErrorBanner) {
        els.launcherErrorBanner.hidden = false;
        els.launcherErrorBanner.textContent = "Failed to reopen project: " + errMsg;
      }
      return;
    }
    activeProjectRecordId = item.id;
    setAppView("studio");
  }

  // =========================================================================
  // LEVEL 3: 12 WORKSPACE TABS & INSPECTOR CATEGORY RAIL
  // =========================================================================

  function renderWorkspaceTabBar() {
    if (!els.workspaceTabs) return;
    if (els.workspaceTabs.children.length !== REG.WORKSPACES.length) {
      els.workspaceTabs.innerHTML = "";
      const frag = document.createDocumentFragment();
      REG.WORKSPACES.forEach((ws) => {
        const btn = document.createElement("button");
        btn.type = "button";
        const isOp = !ws.foundationNotice;
        btn.className = "ws-tab" + (isOp ? " is-operational" : "");
        btn.setAttribute("role", "tab");
        btn.dataset.workspaceTab = ws.id;
        btn.dataset.testid = "ws-tab-" + ws.id;
        btn.innerHTML = '<span class="ws-tab-dot"></span><span>' + escapeHtml(ws.label) + "</span>";
        btn.addEventListener("click", () => activateWorkspaceById(ws.id, true));
        frag.appendChild(btn);
      });
      els.workspaceTabs.appendChild(frag);
    }

    let activeBtn = null;
    Array.from(els.workspaceTabs.children).forEach((btn) => {
      const isAct = btn.dataset.workspaceTab === activeWorkspaceId;
      btn.classList.toggle("is-active", isAct);
      btn.setAttribute("aria-selected", isAct ? "true" : "false");
      if (isAct) activeBtn = btn;
    });

    if (activeBtn && typeof activeBtn.scrollIntoView === "function") {
      try {
        activeBtn.scrollIntoView({ block: "nearest", inline: "nearest" });
      } catch (_) {}
    }
    requestAnimationFrame(updateWorkspaceTabsOverflowUi);
  }

  function activateWorkspaceById(wsId, persistFlag) {
    const norm = wsId === "default" ? "layout" : wsId === "animation_prep" ? "animation" : wsId;
    const ws = REG.WORKSPACES.find((item) => item.id === norm) || REG.WORKSPACES[0];
    activeWorkspaceId = ws.id;
    if (els.workspacePresetSelect) {
      els.workspacePresetSelect.value = ws.id;
    }
    activeBottomEditor = ws.bottomEditor || "timeline";
    if (els.bottomEditorSelect) {
      els.bottomEditorSelect.value = activeBottomEditor;
    }
    if (ws.id === "layout") {
      setInspectorCategory("all");
    } else if (ws.inspectorTab) {
      setInspectorCategory(ws.inspectorTab);
    }

    if (els.wfBanner) {
      if (ws.foundationNotice) {
        els.wfBanner.hidden = false;
        els.wfBannerTitle.textContent = ws.foundationNotice.title;
        els.wfBannerDetail.textContent = ws.foundationNotice.detail;
      } else {
        els.wfBanner.hidden = true;
      }
    }

    if (els.toolShelf) {
      els.toolShelf.hidden = ws.showToolShelf === false;
      if (els.btnVpRestoreShelf) {
        els.btnVpRestoreShelf.hidden = !els.toolShelf.hidden;
      }
    }

    renderWorkspaceTabBar();
    if (wasmApi) {
      wasmApi.dispatchAction({ action: "apply_workspace_preset", preset: ws.preset });
    }
    if (persistFlag) {
      persistWorkspaceToStorage();
    }
  }

  const INSP_HEADINGS = {
    object: "Object & Transform",
    modifiers: "Modeling & Modifiers",
    material: "Material Properties",
    data: "Camera & Light Data",
    render: "Project & Telemetry",
    all: "Inspector (All Sections)",
  };

  function setInspectorCategory(cat) {
    activeInspTab = cat || "all";
    if (els.propsActiveHeading) {
      els.propsActiveHeading.textContent = INSP_HEADINGS[activeInspTab] || "Properties";
    }
    document.querySelectorAll("[data-insp-tab]").forEach((btn) => {
      btn.classList.toggle("is-active", btn.dataset.inspTab === activeInspTab);
    });
    document.querySelectorAll("[data-insp-section]").forEach((sec) => {
      const key = sec.dataset.inspSection;
      sec.hidden = activeInspTab !== "all" && key !== activeInspTab;
    });
    if (els.panelProperties) {
      els.panelProperties.hidden =
        activeInspTab === "material" || activeInspTab === "render";
    }
  }

  // =========================================================================
  // WASM ENGINE BINDING & SPLASH BOOT LOOP
  // =========================================================================

  function tryBindWasmApi() {
    const M = window.Module;
    if (
      !M ||
      (!M.__fe3dRuntimeReady && !M.calledRun) ||
      typeof M._fe3d_is_ready !== "function" ||
      typeof M.ccall !== "function"
    ) {
      return false;
    }

    if (typeof M._fe3d_get_boot_stage === "function") {
      const stage = M._fe3d_get_boot_stage();
      const msg =
        typeof M._fe3d_get_boot_message === "function"
          ? M.ccall("fe3d_get_boot_message", "string", [], [])
          : "";
      if (stage === 1) setSplashProgress(2, 45, msg);
      else if (stage === 2) setSplashProgress(3, 72, msg);
      else if (stage === 3) setSplashProgress(4, 90, msg);
    }

    const ready = M._fe3d_is_ready();
    if (ready === 0) {
      return false;
    }
    if (ready < 0) {
      const errMsg = M.ccall("fe3d_get_init_error", "string", [], []);
      showSplashFatalError(errMsg || "Unsupported WebGL 2.0 graphics context");
      if (els.statusDot) els.statusDot.classList.add("is-error");
      if (els.statusMessage) els.statusMessage.textContent = errMsg;
      return true;
    }

    wasmApi = {
      getRevision: function () {
        return typeof M._fe3d_state_revision === "function" ? M._fe3d_state_revision() : -1;
      },
      getStateJson: function () {
        return M.ccall("fe3d_get_state_json", "string", [], []);
      },
      dispatchAction: function (payloadObj) {
        const ptr = M.stringToNewUTF8(JSON.stringify(payloadObj));
        try {
          const ok = M._fe3d_dispatch_action(ptr);
          syncFromEngine(true);
          return ok === 1;
        } finally {
          M._free(ptr);
        }
      },
      exportInlineProject: function () {
        return M.ccall("fe3d_export_inline_project", "string", [], []);
      },
      exportWorkspaceJson: function () {
        return M.ccall("fe3d_export_workspace_json", "string", [], []);
      },
      saveBundleToVfs: function (vfsPath) {
        return M.ccall("fe3d_save_bundle_to_vfs", "number", ["string"], [vfsPath]) === 1;
      },
      loadFileFromVfs: function (vfsPath) {
        const ok = M.ccall("fe3d_load_file_from_vfs", "number", ["string"], [vfsPath]) === 1;
        syncFromEngine(true);
        return ok;
      },
      loadHdriBytes: function (bytesInput, sourceLabel) {
        if (typeof M._fe3d_load_hdri_bytes !== "function") return false;
        const bytes =
          bytesInput instanceof Uint8Array
            ? bytesInput
            : new Uint8Array(bytesInput || []);
        const labelStr = String(sourceLabel || "Uploaded.hdr");
        const labelUtf8 = new TextEncoder().encode(labelStr + "\0");
        const dataPtr = M._malloc(Math.max(1, bytes.length));
        const labelPtr = M._malloc(labelUtf8.length);
        if (!dataPtr || !labelPtr) {
          if (dataPtr) M._free(dataPtr);
          if (labelPtr) M._free(labelPtr);
          return false;
        }
        try {
          if (bytes.length > 0) M.HEAPU8.set(bytes, dataPtr);
          M.HEAPU8.set(labelUtf8, labelPtr);
          const ok = M._fe3d_load_hdri_bytes(dataPtr, bytes.length, labelPtr) === 1;
          syncFromEngine(true);
          return ok;
        } finally {
          M._free(dataPtr);
          M._free(labelPtr);
        }
      },
      hitTestGizmo: function (cssX, cssY) {
        return typeof M._fe3d_hit_test_gizmo === "function"
          ? M._fe3d_hit_test_gizmo(cssX, cssY) === 1
          : false;
      },
      touchTap: function (cssX, cssY, additive) {
        return typeof M._fe3d_touch_tap === "function"
          ? M._fe3d_touch_tap(cssX, cssY, additive ? 1 : 0) === 1
          : false;
      },
      pointerEvent: function (kind, button, cssX, cssY, shift, ctrl, alt, meta) {
        if (typeof M._fe3d_pointer_event === "function") {
          M._fe3d_pointer_event(
            kind,
            button,
            cssX,
            cssY,
            shift ? 1 : 0,
            ctrl ? 1 : 0,
            alt ? 1 : 0,
            meta ? 1 : 0
          );
        }
      },
      cameraOrbit: function (dxCss, dyCss) {
        if (typeof M._fe3d_camera_orbit === "function") {
          M._fe3d_camera_orbit(dxCss, dyCss);
        }
      },
      editorCameraOrbit: function (dxCss, dyCss) {
        if (typeof M._fe3d_editor_camera_orbit === "function") {
          M._fe3d_editor_camera_orbit(dxCss, dyCss);
        } else {
          this.dispatchAction({ action: "orbit_editor_camera", dx: dxCss, dy: dyCss });
        }
      },
      cameraPan: function (dxCss, dyCss) {
        if (typeof M._fe3d_camera_pan === "function") {
          M._fe3d_camera_pan(dxCss, dyCss);
        }
      },
      cameraZoom: function (steps) {
        if (typeof M._fe3d_camera_zoom === "function") {
          M._fe3d_camera_zoom(steps);
        }
      },
      renderFinalFrame: function () {
        if (typeof M._fe3d_render_final_frame === "function") {
          return M._fe3d_render_final_frame() !== 0;
        }
        return false;
      },
      blitMaterialPreview: function () {
        if (!previewCtx || typeof M._fe3d_material_preview_ptr !== "function") return;
        const gen = M._fe3d_material_preview_generation();
        if (gen === lastPreviewGen) return;
        const ptr = M._fe3d_material_preview_ptr();
        const w = M._fe3d_material_preview_width();
        const h = M._fe3d_material_preview_height();
        if (!ptr || w <= 0 || h <= 0 || !M.HEAPU8) return;
        if (previewCanvas.width !== w || previewCanvas.height !== h) {
          previewCanvas.width = w;
          previewCanvas.height = h;
        }
        const byteLen = w * h * 4;
        const pixels = new Uint8ClampedArray(byteLen);
        pixels.set(M.HEAPU8.subarray(ptr, ptr + byteLen));
        const imgData = new ImageData(pixels, w, h);
        previewCtx.putImageData(imgData, 0, 0);
        lastPreviewGen = gen;
      },
      fs: M.FS,
    };

    setSplashProgress(4, 100, "Final Effect 3D v0.5.0 Ready");
    if (banner) banner.hidden = true;

    window.__FE3D_STUDIO__ = {
      getState: function () {
        syncFromEngine(true);
        return currentState;
      },
      dispatch: wasmApi.dispatchAction,
      setView: setAppView,
      getAppView: function () {
        return document.body.dataset.appView;
      },
      getNavGizmoLayout: getNavGizmoLayout,
      renderFinalFrame: wasmApi.renderFinalFrame,
      loadHdriBytes: wasmApi.loadHdriBytes,
      exportInlineProject: wasmApi.exportInlineProject,
      importInlineProject: function (jsonText) {
        return wasmApi.dispatchAction({ action: "load_inline_json", json_text: jsonText });
      },
    };

    restoreWorkspaceFromStorage();
    syncFromEngine(true);

    const params = new URLSearchParams(window.location.search || "");
    if (params.get("view") === "studio") {
      setAppView("studio");
    } else {
      setAppView("home");
    }
    return true;
  }

  function persistWorkspaceToStorage() {
    if (!wasmApi) return;
    try {
      const wsJson = wasmApi.exportWorkspaceJson();
      if (wsJson) {
        window.localStorage.setItem(WORKSPACE_STORAGE_KEY, wsJson);
      }
    } catch (_) {}
  }

  function syncWorkspaceTabFromLayoutName(layoutName) {
    if (!layoutName || !REG || !REG.WORKSPACES) return;
    const norm = String(layoutName).trim().toLowerCase();
    const match = REG.WORKSPACES.find(
      (w) => w.label.toLowerCase() === norm || w.id.toLowerCase() === norm
    );
    if (match) {
      activeWorkspaceId = match.id;
      activeBottomEditor = match.bottomEditor || "timeline";
      if (els.workspacePresetSelect) els.workspacePresetSelect.value = match.id;
      if (match.id === "layout") setInspectorCategory("all");
      else if (match.inspectorTab) setInspectorCategory(match.inspectorTab);
      renderWorkspaceTabBar();
    }
  }

  function restoreWorkspaceFromStorage() {
    if (!wasmApi) return;
    try {
      const saved = window.localStorage.getItem(WORKSPACE_STORAGE_KEY);
      if (saved && saved.trim().startsWith("{")) {
        const parsed = JSON.parse(saved);
        const wsObj = parsed.workspace || parsed;
        syncWorkspaceTabFromLayoutName(wsObj.layout_name);
        wasmApi.dispatchAction({
          action: "import_workspace_json",
          json_text: saved,
        });
        applyDockRatiosCss(wsObj);
      }
    } catch (_) {}
  }

  function applyDockRatiosCss(wsObj) {
    if (!wsObj) return;
    const totalW = window.innerWidth || 1280;
    const totalH = window.innerHeight || 720;
    if (typeof wsObj.right_dock_width_ratio === "number" && totalW >= 960) {
      const pxW = Math.round(Math.max(240, Math.min(520, wsObj.right_dock_width_ratio * totalW)));
      document.documentElement.style.setProperty("--right-dock-w", pxW + "px");
    }
    if (typeof wsObj.bottom_dock_height_ratio === "number" && totalH >= 520) {
      const pxH = Math.round(Math.max(84, Math.min(340, wsObj.bottom_dock_height_ratio * totalH)));
      document.documentElement.style.setProperty("--bottom-dock-h", pxH + "px");
    }
  }

  // =========================================================================
  // FORMATTING & COLOR HELPERS
  // =========================================================================

  function formatFloat(val, digits) {
    const n = Number(val);
    if (!Number.isFinite(n)) return "0";
    return Number(n.toFixed(digits)).toString();
  }

  function formatBytes(bytes) {
    const b = Number(bytes) || 0;
    if (b < 1024) return b + " B";
    if (b < 1024 * 1024) return (b / 1024).toFixed(1) + " KB";
    return (b / (1024 * 1024)).toFixed(2) + " MB";
  }

  function isFocused(el) {
    return document.activeElement === el;
  }

  function setInputValueIfUnfocused(el, nextValue) {
    if (el && !isFocused(el)) {
      const strVal = String(nextValue);
      if (el.value !== strVal) {
        el.value = strVal;
      }
    }
  }

  function rgbFloatToHex(rgb) {
    if (!Array.isArray(rgb) || rgb.length < 3) return "#ffffff";
    const toHex = (c) => {
      const byte = Math.max(0, Math.min(255, Math.round(Number(c) * 255)));
      return byte.toString(16).padStart(2, "0");
    };
    return "#" + toHex(rgb[0]) + toHex(rgb[1]) + toHex(rgb[2]);
  }

  function hexToRgbFloat(hex) {
    const clean = (hex || "#ffffff").replace("#", "");
    if (clean.length !== 6) return [1, 1, 1];
    return [
      parseInt(clean.slice(0, 2), 16) / 255,
      parseInt(clean.slice(2, 4), 16) / 255,
      parseInt(clean.slice(4, 6), 16) / 255,
    ];
  }

  function sanitizeFilenameStem(raw) {
    const cleaned = (raw || "project")
      .trim()
      .replace(/[^a-zA-Z0-9._-]+/g, "_")
      .replace(/^_+|_+$/g, "");
    return cleaned || "project";
  }

  function triggerDownload(filename, content, mimeType) {
    const blob = new Blob([content], { type: mimeType });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1500);
  }

  function escapeHtml(str) {
    return String(str || "")
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  // =========================================================================
  // ENGINE -> UI SYNCHRONIZATION
  // =========================================================================

  function syncFromEngine(force) {
    if (!wasmApi) return;
    const rev = wasmApi.getRevision();
    if (!force && rev >= 0 && rev === lastRevision) {
      wasmApi.blitMaterialPreview();
      return;
    }
    lastRevision = rev;

    let state = null;
    try {
      state = JSON.parse(wasmApi.getStateJson());
    } catch (err) {
      console.error("[FE3D] State JSON parse error:", err);
      return;
    }
    currentState = state;

    const engVer = state.project ? state.project.engine_version : "0.5.0";
    const backendName = state.viewport ? state.viewport.backend_name : "WebGL 2.0";
    const deviceName = state.viewport ? state.viewport.device_name : "";

    if (els.engineBadge) {
      els.engineBadge.textContent = "v" + engVer + " · " + backendName.split(" ")[0];
    }
    if (els.homeEngineBadge) {
      els.homeEngineBadge.textContent = "v" + engVer + " · " + backendName + " Ready";
    }
    if (els.statusRenderer) {
      els.statusRenderer.textContent = deviceName
        ? backendName + " · " + deviceName
        : backendName;
    }
    if (els.activeProjectPill) {
      els.activeProjectPill.textContent = state.project.project_name || "Untitled";
    }

    els.btnUndo.disabled = !state.history.can_undo;
    els.btnRedo.disabled = !state.history.can_redo;
    els.btnUndo.title = state.history.can_undo
      ? "Undo " + state.history.next_undo + " (Ctrl+Z)"
      : "Nothing to undo";
    els.btnRedo.title = state.history.can_redo
      ? "Redo " + state.history.next_redo + " (Ctrl+Y)"
      : "Nothing to redo";

    const insp = state.inspector;
    const hasSel = insp !== null && typeof insp === "object";
    els.btnDuplicate.disabled = !hasSel;
    els.btnDelete.disabled = !hasSel || (insp && insp.id === state.viewport.active_camera_id);

    const ed = state.editor || {
      mode: "object",
      gizmo_mode: "translate",
      gizmo_space: "world",
      gizmo_active_handle: "none",
      gizmo_hovered_handle: "none",
      editable_mesh: null,
    };
    const canMeshEdit = Boolean(ed.editable_mesh && ed.editable_mesh.editable);
    els.btnModeVertex.disabled = !canMeshEdit;
    els.btnModeEdge.disabled = !canMeshEdit;
    els.btnModeFace.disabled = !canMeshEdit;

    [els.btnModeObject, els.btnModeVertex, els.btnModeEdge, els.btnModeFace].forEach((b) => {
      if (b) b.classList.toggle("is-active", b.dataset.editMode === ed.mode);
    });

    if (els.ctxMeshSubbar) {
      els.ctxMeshSubbar.hidden = ed.mode === "object";
    }

    if (ed.mode !== "object" && activeInspTab !== "all" && activeInspTab !== "modifiers") {
      setInspectorCategory("modifiers");
    } else if (insp && insp.id !== lastAutoInspObjId) {
      lastAutoInspObjId = insp.id;
      if (
        (insp.camera || insp.light) &&
        !insp.mesh &&
        activeInspTab !== "all" &&
        activeInspTab !== "data"
      ) {
        setInspectorCategory("all");
      }
    } else if (!insp) {
      lastAutoInspObjId = 0;
    }

    [els.btnGizmoTranslate, els.btnGizmoRotate, els.btnGizmoScale].forEach((b) => {
      if (b) b.classList.toggle("is-active", b.dataset.gizmoMode === ed.gizmo_mode);
    });
    if (els.btnGizmoSpace) {
      els.btnGizmoSpace.textContent = ed.gizmo_space === "local" ? "Local" : "World";
    }

    const playing = Boolean(state.viewport.rig_playing);
    if (els.btnToggleRig) {
      els.btnToggleRig.textContent = playing ? "Pause Rig" : "Play Rig";
      els.btnToggleRig.classList.toggle("is-active", playing);
    }
    if (els.lblTimelinePlay) {
      els.lblTimelinePlay.textContent = playing ? "Pause Rig" : "Play Rig";
    }

    setInputValueIfUnfocused(els.metaProjectName, state.project.project_name);
    setInputValueIfUnfocused(els.metaAuthor, state.project.author);
    setInputValueIfUnfocused(els.metaSceneName, state.project.scene_name);

    const activeCamObj = state.objects.find((o) => o.id === state.viewport.active_camera_id);
    const lensSuffix = state.viewport.camera_view_active
      ? " [Lens]"
      : state.viewport.view_label
      ? " · " + state.viewport.view_label
      : "";
    els.vpCameraPill.textContent =
      "Camera: " +
      (activeCamObj ? activeCamObj.name : "None") +
      lensSuffix +
      " · Dist " +
      formatFloat(state.viewport.camera_distance, 1) +
      "m";
    els.vpSelectionPill.textContent = hasSel ? "Selected: " + insp.name : "No Selection";

    const toolLabel = ed.gizmo_mode === "translate" ? "MOVE" : ed.gizmo_mode.toUpperCase();
    const handleHint =
      ed.gizmo_active_handle && ed.gizmo_active_handle !== "none"
        ? " · DRAG " + ed.gizmo_active_handle.toUpperCase()
        : ed.gizmo_hovered_handle && ed.gizmo_hovered_handle !== "none"
        ? " · HOVER " + ed.gizmo_hovered_handle.toUpperCase()
        : "";
    els.vpModePill.textContent =
      ed.mode.toUpperCase() + " · " + toolLabel + " (" + ed.gizmo_space.toUpperCase() + ")" + handleHint;

    renderViewNavGizmo(state);
    if (state.viewport.view_transition_active) {
      scheduleViewTransitionSync();
    }

    renderWorkspaceAndBottomDock(state);
    renderOutliner(state);
    renderInspector(state, ed);
    renderEnvironmentCard(state);
    renderMaterialEditor(state);

    els.statObjectsMeshes.textContent =
      state.stats.object_count + " / " + state.stats.mesh_count;
    els.statMaterialsUpdates.textContent =
      (state.stats.material_count || 0) + " / " + (state.stats.gpu_buffer_updates || 0);
    const ld = state.lighting_diagnostics || {};
    if (els.statLightsShadows) {
      els.statLightsShadows.textContent =
        (ld.active_lights ?? ld.active_realtime_lights ?? 0) +
        " / " +
        (ld.active_shadow_lights ?? 0);
    }
    els.statTriangles.textContent = String(state.stats.total_triangles);
    els.statMemory.textContent =
      formatBytes(state.stats.cpu_mesh_bytes) + " / " + formatBytes(state.stats.gpu_mesh_bytes);
    if (els.statLightingGpu) {
      els.statLightingGpu.textContent =
        formatBytes(ld.shadow_gpu_bytes || 0) +
        " / " +
        formatBytes(ld.ibl_gpu_bytes ?? ld.environment_gpu_bytes ?? 0);
    }
    if (els.statLightingWarning) {
      const warnMsg = ld.warning_message || "";
      els.statLightingWarning.hidden = !warnMsg;
      els.statLightingWarning.textContent = warnMsg;
    }
    els.statHistory.textContent =
      state.history.undo_count + " steps (" + formatBytes(state.history.memory_bytes) + ")";

    if (state.status && state.status.message) {
      els.statusDot.classList.toggle("is-error", state.status.level === "error");
      els.statusMessage.textContent = state.status.message;
    }

    wasmApi.blitMaterialPreview();
  }

  const FOUNDATION_EDITOR_COPY = {
    shader_foundation: {
      title: "Shader Node Graph (Architectural Foundation)",
      desc: "Node-based procedural shader DAG compilation is scheduled for a future engine stage. Direct Metallic-Roughness PBR parameters & live 3D sphere preview are active in the Material Inspector.",
    },
    uv_foundation: {
      title: "2D UV Editor (Architectural Foundation)",
      desc: "EditableMesh preserves per-corner UV attributes in memory and .fe3d serialization. Interactive 2D UV island unwrapping & packing tools are planned for a future stage.",
    },
    geonodes_foundation: {
      title: "Geometry Nodes Graph (Architectural Foundation)",
      desc: "Procedural geometry modifier graphs are scheduled for a future engine stage. Direct undoable polygon extrusion, hole selection & boundary fill are active today.",
    },
    compositor_foundation: {
      title: "Render Layer Compositor (Architectural Foundation)",
      desc: "Post-processing node chains (glare, tone-mapping curves, AOV passes) are planned for a future stage.",
    },
    vse_foundation: {
      title: "Video Sequencer (Architectural Foundation)",
      desc: "Multi-track timeline clip editing is planned for a future stage. Showcase rig transport is connected to the live 3D scene.",
    },
  };

  function renderWorkspaceAndBottomDock(state) {
    const ws = state.workspace;
    const isFoundationEditor = Boolean(FOUNDATION_EDITOR_COPY[activeBottomEditor]);
    const layoutMode = document.body.dataset.layoutMode || "desktop";
    const isPortrait = layoutMode === "portrait";

    if (ws) {
      const panelVis = {};
      (ws.panels || []).forEach((p) => {
        panelVis[p.key] = Boolean(p.visible);
      });
      document.querySelectorAll("[data-panel-toggle]").forEach((btn) => {
        const k = btn.dataset.panelToggle;
        btn.classList.toggle("is-on", Boolean(panelVis[k]));
      });

      if (!isPortrait) {
        if (els.panelOutliner) {
          els.panelOutliner.hidden = panelVis.scene_outliner === false;
        }
        if (els.propertiesShell) {
          const anyProps =
            panelVis.properties !== false || panelVis.material_editor !== false;
          els.propertiesShell.hidden = !anyProps;
        }
        if (els.splitterOutliner) {
          els.splitterOutliner.hidden = Boolean(
            (els.panelOutliner && els.panelOutliner.hidden) ||
              (els.propertiesShell && els.propertiesShell.hidden)
          );
        }
        if (activeInspTab === "all") {
          if (els.panelProperties) {
            els.panelProperties.hidden = panelVis.properties === false;
          }
          if (els.panelMaterialEditor) {
            els.panelMaterialEditor.hidden = panelVis.material_editor === false;
          }
        }
        if (els.dockRight) {
          const allRightOff =
            panelVis.scene_outliner === false &&
            panelVis.properties === false &&
            panelVis.material_editor === false;
          if (layoutMode === "desktop") {
            els.dockRight.hidden = isRightDockCollapsed || allRightOff;
            if (els.splitterRight) els.splitterRight.hidden = els.dockRight.hidden;
            if (els.btnVpRestoreRight) els.btnVpRestoreRight.hidden = !els.dockRight.hidden;
          } else {
            els.dockRight.hidden = allRightOff;
            if (els.btnVpRestoreRight) els.btnVpRestoreRight.hidden = true;
          }
        }
        if (els.dockBottom) {
          const bottomKeys = ["asset_browser", "timeline", "console"];
          const visibleBottom = bottomKeys.filter((k) => Boolean(panelVis[k]));
          const shouldHideBottom = visibleBottom.length === 0 && !isFoundationEditor;
          els.dockBottom.hidden = shouldHideBottom;
          const isBottomCollapsed = els.dockBottom.classList.contains("is-collapsed");
          if (els.splitterBottom) {
            els.splitterBottom.hidden = shouldHideBottom || isBottomCollapsed;
          }
          if (
            !isFoundationEditor &&
            visibleBottom.length > 0 &&
            !visibleBottom.includes(activeBottomEditor)
          ) {
            activeBottomEditor = visibleBottom[0];
          }
        }
      } else {
        // In Mobile Portrait mode, panel visibility inside drawers is controlled cleanly by data-mp-drawer
        if (els.panelOutliner) els.panelOutliner.hidden = false;
        if (els.propertiesShell) els.propertiesShell.hidden = false;
        if (els.dockBottom) els.dockBottom.hidden = false;
        if (els.dockRight) els.dockRight.hidden = false;
        if (els.btnVpRestoreRight) els.btnVpRestoreRight.hidden = true;
      }
    }

    const menuToggleShelf = document.querySelector(
      '[data-menu-cmd="toggle_toolshelf"], [data-menu-cmd="toggle_tool_shelf"]'
    );
    if (menuToggleShelf && els.toolShelf) {
      menuToggleShelf.classList.toggle("is-on", !els.toolShelf.hidden);
    }
    const menuToggleRight = document.querySelector('[data-menu-cmd="toggle_right_dock"]');
    if (menuToggleRight && els.dockRight) {
      const rightOpen =
        layoutMode === "landscape"
          ? !document.body.classList.contains("is-landscape-drawer-closed")
          : !els.dockRight.hidden;
      menuToggleRight.classList.toggle("is-on", rightOpen);
    }
    const menuToggleGrid = document.querySelector('[data-menu-cmd="toggle_editor_grid"]');
    if (menuToggleGrid && state.viewport) {
      menuToggleGrid.classList.toggle("is-on", Boolean(state.viewport.editor_grid_visible));
    }
    const menuToggleHelpers = document.querySelector('[data-menu-cmd="toggle_editor_helpers"]');
    if (menuToggleHelpers && state.viewport) {
      menuToggleHelpers.classList.toggle("is-on", Boolean(state.viewport.editor_helpers_visible));
    }

    [els.tabTimeline, els.tabAssetBrowser, els.tabConsole].forEach((tab) => {
      if (tab) tab.classList.toggle("is-active", tab.dataset.bottomTab === activeBottomEditor);
    });
    if (els.bottomEditorSelect && els.bottomEditorSelect.value !== activeBottomEditor) {
      els.bottomEditorSelect.value = activeBottomEditor;
    }

    els.paneTimeline.hidden = activeBottomEditor !== "timeline";
    els.paneAssetBrowser.hidden = activeBottomEditor !== "asset_browser";
    els.paneConsole.hidden = activeBottomEditor !== "console";
    els.paneFoundationEditor.hidden = !isFoundationEditor;

    if (isFoundationEditor) {
      const info = FOUNDATION_EDITOR_COPY[activeBottomEditor];
      els.fePaneTitle.textContent = info.title;
      els.fePaneDesc.textContent = info.desc;
      const selName = state.inspector ? state.inspector.name : "None";
      const activeMat = (state.materials || []).find((m) => m.id === state.active_material_id);
      els.fePaneBinding.textContent =
        "Connected Shared Scene: '" +
        state.project.scene_name +
        "' · Active Object: " +
        selName +
        " · Active Material: " +
        (activeMat ? activeMat.name : "None");
    }

    const assetsSig = JSON.stringify([state.materials, state.active_material_id]);
    if (assetsSig !== lastAssetsSignature && els.assetBrowserList) {
      lastAssetsSignature = assetsSig;
      els.assetBrowserList.innerHTML = "";

      [
        { preset: "cube", label: "Primitive: Cube", icon: "cube" },
        { preset: "sphere", label: "Primitive: UV Sphere", icon: "cube" },
        { preset: "pyramid", label: "Primitive: Pyramid", icon: "cube" },
        { preset: "point_light", label: "Light: Point", icon: "light" },
        { preset: "spot_light", label: "Light: Spot", icon: "light" },
        { preset: "area_light", label: "Light: Area", icon: "light" },
      ].forEach((pr) => {
        const chip = document.createElement("button");
        chip.type = "button";
        chip.className = "asset-chip";
        chip.innerHTML =
          '<span class="btn-ic">' + REG.ICONS[pr.icon] + "</span><span>" + pr.label + "</span>";
        chip.addEventListener("click", () => {
          if (wasmApi) wasmApi.dispatchAction({ action: "add_preset", preset: pr.preset });
        });
        els.assetBrowserList.appendChild(chip);
      });

      (state.materials || []).forEach((mat) => {
        const chip = document.createElement("button");
        chip.type = "button";
        chip.className = "asset-chip";
        chip.dataset.testid = "asset-mat-" + mat.name;
        const sw = document.createElement("span");
        sw.className = "asset-swatch";
        sw.style.backgroundColor = rgbFloatToHex(mat.base_color);
        const lbl = document.createElement("span");
        lbl.textContent = mat.name;
        chip.appendChild(sw);
        chip.appendChild(lbl);
        chip.addEventListener("click", () => {
          if (wasmApi) wasmApi.dispatchAction({ action: "select_material", id: mat.id });
        });
        els.assetBrowserList.appendChild(chip);
      });
    }

    const consoleSig = JSON.stringify(state.console || []);
    if (consoleSig !== lastConsoleSignature && els.consoleLogList) {
      lastConsoleSignature = consoleSig;
      els.consoleLogList.innerHTML = "";
      (state.console || [])
        .slice(-24)
        .reverse()
        .forEach((entry) => {
          const line = document.createElement("div");
          line.className = "console-line" + (entry.level === "error" ? " is-error" : "");
          line.textContent = "#" + entry.seq + " " + entry.message;
          els.consoleLogList.appendChild(line);
        });
    }
  }

  function renderOutliner(state) {
    const selectedId = state.viewport ? state.viewport.selected_object_id : 0;
    const q = outlinerFilterQuery.trim().toLowerCase();
    const sig = JSON.stringify([selectedId, state.objects, q]);
    if (sig === lastOutlinerSignature) return;
    lastOutlinerSignature = sig;

    els.outlinerList.innerHTML = "";
    state.objects.forEach((obj) => {
      if (q && obj.name.toLowerCase().indexOf(q) === -1) return;
      const isSel = obj.id === selectedId;

      const row = document.createElement("div");
      row.className =
        "outliner-row" +
        (isSel ? " is-selected" : "") +
        (!obj.visible ? " is-hidden-obj" : "");
      row.setAttribute("role", "treeitem");
      row.setAttribute("aria-selected", isSel ? "true" : "false");
      row.dataset.testid = "outliner-item-" + obj.name;
      row.style.paddingLeft = 6 + obj.depth * 13 + "px";

      const iconWrap = document.createElement("span");
      iconWrap.className = "btn-ic";
      if (obj.has_camera) iconWrap.innerHTML = REG.ICONS.camera;
      else if (obj.has_light) iconWrap.innerHTML = REG.ICONS.light;
      else if (obj.has_mesh) iconWrap.innerHTML = REG.ICONS.cube;
      else iconWrap.innerHTML = REG.ICONS.pivot;

      const nameSpan = document.createElement("span");
      nameSpan.className = "outliner-name";
      nameSpan.textContent = obj.name;

      const tagSpan = document.createElement("span");
      tagSpan.className = "outliner-tag";
      const lt = obj.light_type || "directional";
      const lightTag =
        lt === "point"
          ? "PT"
          : lt === "spot"
          ? "SPOT"
          : lt === "area"
          ? "AREA"
          : "SUN";
      tagSpan.textContent = obj.has_camera
        ? "CAM"
        : obj.has_light
        ? lightTag
        : obj.has_mesh
        ? "MESH"
        : "PIVOT";

      const eyeBtn = document.createElement("button");
      eyeBtn.type = "button";
      eyeBtn.className = "outliner-eye-btn";
      eyeBtn.dataset.testid = "toggle-vis-" + obj.name;
      eyeBtn.title = obj.visible ? "Hide in Viewport" : "Show in Viewport";
      eyeBtn.innerHTML =
        '<span class="btn-ic">' + (obj.visible ? REG.ICONS.eye : REG.ICONS.eyeOff) + "</span>";
      eyeBtn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        if (wasmApi) {
          wasmApi.dispatchAction({
            action: "set_visibility",
            id: obj.id,
            visible: !obj.visible,
          });
        }
      });

      row.appendChild(iconWrap);
      row.appendChild(nameSpan);
      row.appendChild(tagSpan);
      row.appendChild(eyeBtn);

      row.addEventListener("click", () => {
        if (wasmApi) wasmApi.dispatchAction({ action: "select_object", id: obj.id });
      });
      els.outlinerList.appendChild(row);
    });
  }

  function renderInspector(state, ed) {
    const insp = state.inspector;
    if (!insp) {
      els.inspectorIdBadge.textContent = "None";
      els.inspectorEmpty.hidden = false;
      els.inspectorBody.hidden = true;
      return;
    }

    els.inspectorEmpty.hidden = true;
    els.inspectorBody.hidden = false;
    els.inspectorIdBadge.textContent = "#" + insp.id;

    setInputValueIfUnfocused(els.inspName, insp.name);
    if (!isFocused(els.inspVisible)) {
      els.inspVisible.checked = Boolean(insp.visible);
    }

    if (!isFocused(els.inspParent)) {
      const parentSig =
        insp.id +
        ":" +
        state.objects
          .filter((candidate) => candidate.id !== insp.id)
          .map((candidate) => candidate.id + ":" + candidate.name)
          .join("|");
      if (parentSig !== lastInspParentSig) {
        lastInspParentSig = parentSig;
        els.inspParent.innerHTML = "";
        const rootOpt = document.createElement("option");
        rootOpt.value = "0";
        rootOpt.textContent = "(Scene Root)";
        els.inspParent.appendChild(rootOpt);
        state.objects.forEach((candidate) => {
          if (candidate.id === insp.id) return;
          const opt = document.createElement("option");
          opt.value = String(candidate.id);
          opt.textContent = candidate.name + " (#" + candidate.id + ")";
          els.inspParent.appendChild(opt);
        });
      }
      els.inspParent.value = String(insp.parent_id || 0);
    }

    if (insp.mesh) {
      els.inspMaterialRow.hidden = false;
      if (!isFocused(els.inspMaterial)) {
        const matSig = (state.materials || []).map((m) => m.id + ":" + m.name).join("|");
        if (matSig !== lastInspMatSig) {
          lastInspMatSig = matSig;
          els.inspMaterial.innerHTML = "";
          const noneOpt = document.createElement("option");
          noneOpt.value = "0";
          noneOpt.textContent = "(Default Vertex Color)";
          els.inspMaterial.appendChild(noneOpt);
          (state.materials || []).forEach((mat) => {
            const opt = document.createElement("option");
            opt.value = String(mat.id);
            opt.textContent = mat.name + " (#" + mat.id + ")";
            els.inspMaterial.appendChild(opt);
          });
        }
        els.inspMaterial.value = String(insp.material_id || 0);
      }
    } else {
      els.inspMaterialRow.hidden = true;
    }

    setInputValueIfUnfocused(els.posX, formatFloat(insp.position[0], 3));
    setInputValueIfUnfocused(els.posY, formatFloat(insp.position[1], 3));
    setInputValueIfUnfocused(els.posZ, formatFloat(insp.position[2], 3));

    setInputValueIfUnfocused(els.rotX, formatFloat(insp.rotation_deg[0], 2));
    setInputValueIfUnfocused(els.rotY, formatFloat(insp.rotation_deg[1], 2));
    setInputValueIfUnfocused(els.rotZ, formatFloat(insp.rotation_deg[2], 2));

    setInputValueIfUnfocused(els.sclX, formatFloat(insp.scale[0], 3));
    setInputValueIfUnfocused(els.sclY, formatFloat(insp.scale[1], 3));
    setInputValueIfUnfocused(els.sclZ, formatFloat(insp.scale[2], 3));

    if (ed && ed.mode !== "object" && ed.editable_mesh && ed.editable_mesh.editable) {
      els.cardMeshEdit.hidden = false;
      const selCount =
        ed.mode === "vertex"
          ? ed.mesh_selection.vertices.length
          : ed.mode === "edge"
          ? ed.mesh_selection.edges.length
          : ed.mesh_selection.faces.length;
      els.meshEditSelectionTag.textContent =
        selCount + " " + ed.mode + (selCount === 1 ? "" : "s");
      els.meshEditTopology.textContent =
        ed.editable_mesh.face_count +
        " faces · " +
        ed.editable_mesh.edge_count +
        " edges · " +
        ed.editable_mesh.vertex_count +
        " verts";
      els.btnMeshExtrude.disabled = ed.mode !== "face" || selCount === 0;
      els.btnMeshBoundary.disabled = ed.editable_mesh.open_boundary_edges < 3;
      els.btnMeshFill.disabled = ed.mode !== "vertex" || selCount < 3;
      els.btnMeshAssignMat.disabled =
        ed.mode !== "face" || selCount === 0 || !state.active_material_id;
      els.btnMeshDelete.disabled = selCount === 0;
    } else {
      els.cardMeshEdit.hidden = true;
    }

    if (insp.mesh) {
      els.cardMesh.hidden = false;
      els.meshAssetName.textContent = insp.mesh.name + " (#" + insp.mesh.id + ")";
      els.meshGeoCounts.textContent =
        insp.mesh.vertex_count +
        " verts · " +
        insp.mesh.triangle_count +
        " tris (" +
        insp.mesh.face_count +
        " faces)";
      els.meshBounds.textContent =
        formatFloat(insp.mesh.bounds_size[0], 2) +
        " × " +
        formatFloat(insp.mesh.bounds_size[1], 2) +
        " × " +
        formatFloat(insp.mesh.bounds_size[2], 2);
    } else {
      els.cardMesh.hidden = true;
    }

    if (insp.camera) {
      els.cardCamera.hidden = false;
      els.btnSetActiveCamera.textContent = insp.camera.is_active ? "Active" : "Set Active";
      els.btnSetActiveCamera.disabled = Boolean(insp.camera.is_active);
      if (els.btnToggleCameraView) {
        const viewingThisLens = Boolean(
          insp.camera.is_active && state.viewport && state.viewport.camera_view_active
        );
        els.btnToggleCameraView.textContent = viewingThisLens ? "Exit Lens" : "View Lens";
        els.btnToggleCameraView.classList.toggle("is-active", viewingThisLens);
      }
      if (!isFocused(els.camProjection)) els.camProjection.value = insp.camera.projection;
      setInputValueIfUnfocused(els.camFov, formatFloat(insp.camera.fov_deg, 1));
      setInputValueIfUnfocused(els.camOrtho, formatFloat(insp.camera.ortho_height, 2));
      setInputValueIfUnfocused(els.camNear, formatFloat(insp.camera.near_plane, 3));
      setInputValueIfUnfocused(els.camFar, formatFloat(insp.camera.far_plane, 1));
    } else {
      els.cardCamera.hidden = true;
    }

    if (insp.light) {
      els.cardLight.hidden = false;
      const lt = insp.light.type || "directional";
      if (els.lightCardTitle) {
        els.lightCardTitle.textContent =
          lt === "point"
            ? "Point Light"
            : lt === "spot"
            ? "Spot Light"
            : lt === "area"
            ? "Area Light"
            : "Directional Light";
      }
      if (els.lightEnabled && !isFocused(els.lightEnabled)) {
        els.lightEnabled.checked = insp.light.enabled !== false;
      }
      if (els.lightType && !isFocused(els.lightType)) {
        els.lightType.value = lt;
      }
      if (!isFocused(els.lightColor)) els.lightColor.value = rgbFloatToHex(insp.light.color);
      setInputValueIfUnfocused(els.lightIntensity, formatFloat(insp.light.intensity, 2));

      const isPoint = lt === "point";
      const isSpot = lt === "spot";
      const isArea = lt === "area";
      const isRanged = isPoint || isSpot || isArea;
      if (els.lightShapeBlock) els.lightShapeBlock.hidden = !isRanged;
      if (els.lightRowRange) els.lightRowRange.hidden = !isRanged;
      if (els.lightRowFalloff) els.lightRowFalloff.hidden = !(isPoint || isSpot);
      if (els.lightRowAttenuation) els.lightRowAttenuation.hidden = !(isPoint || isSpot);
      if (els.lightRowSourceRadius) els.lightRowSourceRadius.hidden = !isPoint;
      if (els.lightRowSpotCones) els.lightRowSpotCones.hidden = !isSpot;
      if (els.lightRowConeFalloff) els.lightRowConeFalloff.hidden = !isSpot;
      if (els.lightRowAreaShape) els.lightRowAreaShape.hidden = !isArea;
      if (els.lightRowAreaWidth) els.lightRowAreaWidth.hidden = !isArea;
      if (els.lightRowAreaTwoSided) els.lightRowAreaTwoSided.hidden = !isArea;
      if (els.lightRowAreaSamples) els.lightRowAreaSamples.hidden = !isArea;

      if (isRanged && insp.light.range !== undefined) {
        setInputValueIfUnfocused(els.lightRange, formatFloat(insp.light.range, 2));
      }
      if (isPoint || isSpot) {
        if (els.lightFalloff && !isFocused(els.lightFalloff) && insp.light.falloff) {
          els.lightFalloff.value = insp.light.falloff;
        }
        if (els.lightAttenuation && insp.light.attenuation !== undefined) {
          setInputValueIfUnfocused(els.lightAttenuation, formatFloat(insp.light.attenuation, 2));
        }
      }
      if (isPoint && els.lightSourceRadius && insp.light.source_radius !== undefined) {
        setInputValueIfUnfocused(els.lightSourceRadius, formatFloat(insp.light.source_radius, 3));
      }
      if (isSpot) {
        if (els.lightInnerCone && insp.light.inner_cone_deg !== undefined) {
          setInputValueIfUnfocused(els.lightInnerCone, formatFloat(insp.light.inner_cone_deg, 1));
        }
        if (els.lightOuterCone && insp.light.outer_cone_deg !== undefined) {
          setInputValueIfUnfocused(els.lightOuterCone, formatFloat(insp.light.outer_cone_deg, 1));
        }
        if (els.lightConeFalloff && insp.light.cone_falloff !== undefined) {
          setInputValueIfUnfocused(els.lightConeFalloff, formatFloat(insp.light.cone_falloff, 2));
        }
      }
      if (isArea) {
        const areaShape = insp.light.shape || "rectangle";
        if (els.lightAreaShape && !isFocused(els.lightAreaShape)) {
          els.lightAreaShape.value = areaShape;
        }
        if (els.lblLightAreaWidth) {
          els.lblLightAreaWidth.textContent = areaShape === "disk" ? "Diameter" : "Width";
        }
        if (els.lightRowAreaHeight) {
          els.lightRowAreaHeight.hidden = areaShape === "disk";
        }
        if (els.lightAreaWidth && insp.light.width !== undefined) {
          setInputValueIfUnfocused(els.lightAreaWidth, formatFloat(insp.light.width, 2));
        }
        if (els.lightAreaHeight && insp.light.height !== undefined) {
          setInputValueIfUnfocused(els.lightAreaHeight, formatFloat(insp.light.height, 2));
        }
        if (els.lightAreaTwoSided && !isFocused(els.lightAreaTwoSided)) {
          els.lightAreaTwoSided.checked = Boolean(insp.light.two_sided);
        }
        if (els.lightAreaSamples && !isFocused(els.lightAreaSamples) && insp.light.sample_quality) {
          els.lightAreaSamples.value = String(insp.light.sample_quality);
        }
      }

      const sh = insp.light.shadow || {};
      if (els.lightShadowEnabled && !isFocused(els.lightShadowEnabled)) {
        els.lightShadowEnabled.checked = Boolean(sh.enabled);
      }
      if (els.lightShadowQuality && !isFocused(els.lightShadowQuality) && sh.quality) {
        els.lightShadowQuality.value = sh.quality;
      }
      if (els.lightShadowFilter && !isFocused(els.lightShadowFilter) && sh.filter) {
        els.lightShadowFilter.value = sh.filter;
      }
      if (els.lightShadowStrength && sh.strength !== undefined) {
        setInputValueIfUnfocused(els.lightShadowStrength, formatFloat(sh.strength, 2));
      }
      if (els.lightShadowBias && sh.bias !== undefined) {
        setInputValueIfUnfocused(els.lightShadowBias, formatFloat(sh.bias, 4));
      }
      if (els.lightShadowNormalBias && sh.normal_bias !== undefined) {
        setInputValueIfUnfocused(els.lightShadowNormalBias, formatFloat(sh.normal_bias, 3));
      }
      if (els.lightShadowSoftness && sh.softness !== undefined) {
        setInputValueIfUnfocused(els.lightShadowSoftness, formatFloat(sh.softness, 2));
      }

      const ll =
        insp.light.linking ||
        insp.light_linking || { mode: "all", include_descendants: true, object_ids: [] };
      const linkMode = ll.mode || "all";
      if (els.lightLinkMode && !isFocused(els.lightLinkMode)) {
        els.lightLinkMode.value = linkMode;
      }
      if (els.lightLinkListWrap) {
        els.lightLinkListWrap.hidden = linkMode === "all";
      }
      if (els.lightLinkDescendants && !isFocused(els.lightLinkDescendants)) {
        els.lightLinkDescendants.checked = ll.include_descendants !== false;
      }
      if (els.lightLinkTargets && linkMode !== "all") {
        const eligibleObjs = (state.objects || []).filter(
          (o) => o.id !== insp.id && (o.has_mesh || (!o.has_camera && !o.has_light))
        );
        const rawLinkedIds = ll.object_ids || ll.linked_objects || [];
        const linkedSet = new Set(rawLinkedIds);
        const linkSig = JSON.stringify([
          insp.id,
          linkMode,
          rawLinkedIds,
          eligibleObjs.map((o) => [o.id, o.name]),
        ]);
        if (linkSig !== lastLightLinkSig) {
          lastLightLinkSig = linkSig;
          els.lightLinkTargets.innerHTML = "";
          eligibleObjs.forEach((o) => {
            const lbl = document.createElement("label");
            lbl.className = "light-link-item";
            const cb = document.createElement("input");
            cb.type = "checkbox";
            cb.dataset.objId = String(o.id);
            cb.dataset.testid = "light-link-target-" + o.name;
            cb.checked = linkedSet.has(o.id);
            cb.addEventListener("change", () => {
              if (!wasmApi || !currentState || !currentState.inspector) return;
              wasmApi.dispatchAction({
                action: "toggle_light_link_target",
                id: currentState.inspector.id,
                target_id: o.id,
                linked: Boolean(cb.checked),
              });
            });
            const sp = document.createElement("span");
            sp.textContent = o.name;
            lbl.appendChild(cb);
            lbl.appendChild(sp);
            els.lightLinkTargets.appendChild(lbl);
          });
        }
      }
    } else {
      els.cardLight.hidden = true;
    }

    if (els.cardDataEmpty) {
      els.cardDataEmpty.hidden = Boolean(insp.camera || insp.light);
    }
  }

  function renderEnvironmentCard(state) {
    const env = state.environment;
    if (!env || !els.envEnabled) return;
    if (!isFocused(els.envEnabled)) {
      els.envEnabled.checked = env.enabled !== false;
    }
    if (els.envHdriStatus) {
      if (env.has_hdri) {
        const hName = env.hdri_name || (env.hdri && env.hdri.name) || env.source_path || "HDRI";
        const hW = env.hdri_width || (env.hdri && env.hdri.width) || 0;
        const hH = env.hdri_height || (env.hdri && env.hdri.height) || 0;
        els.envHdriStatus.textContent = hW > 0 ? hName + " (" + hW + "×" + hH + ")" : hName;
      } else {
        els.envHdriStatus.textContent = "Studio Sky (Fallback)";
      }
    }
    if (els.btnClearHdri) {
      els.btnClearHdri.disabled = !env.has_hdri;
    }
    if (els.envIntensity) {
      setInputValueIfUnfocused(els.envIntensity, Number(env.intensity).toFixed(2));
      if (els.valEnvIntensity) els.valEnvIntensity.textContent = Number(env.intensity).toFixed(2);
    }
    if (els.envRotation) {
      setInputValueIfUnfocused(els.envRotation, String(Math.round(Number(env.rotation_deg || 0))));
      if (els.valEnvRotation) {
        els.valEnvRotation.textContent = Math.round(Number(env.rotation_deg || 0)) + "°";
      }
    }
    if (els.envDiffuseScale) {
      setInputValueIfUnfocused(els.envDiffuseScale, Number(env.diffuse_ibl_scale).toFixed(2));
      if (els.valEnvDiffuse) {
        els.valEnvDiffuse.textContent = Number(env.diffuse_ibl_scale).toFixed(2);
      }
    }
    if (els.envSpecularScale) {
      setInputValueIfUnfocused(els.envSpecularScale, Number(env.specular_ibl_scale).toFixed(2));
      if (els.valEnvSpecular) {
        els.valEnvSpecular.textContent = Number(env.specular_ibl_scale).toFixed(2);
      }
    }
  }

  function renderMaterialEditor(state) {
    const mats = state.materials || [];
    const activeId = state.active_material_id || 0;
    const activeMat = mats.find((m) => m.id === activeId) || mats[0] || null;

    if (!isFocused(els.matSelect)) {
      const matSelSig = mats.map((m) => m.id + ":" + m.name).join("|");
      if (matSelSig !== lastMatSelectSig) {
        lastMatSelectSig = matSelSig;
        els.matSelect.innerHTML = "";
        mats.forEach((m) => {
          const opt = document.createElement("option");
          opt.value = String(m.id);
          opt.textContent = m.name + " (#" + m.id + ")";
          els.matSelect.appendChild(opt);
        });
      }
      if (activeMat) els.matSelect.value = String(activeMat.id);
    }

    const hasMat = Boolean(activeMat);
    els.btnMatDup.disabled = !hasMat;
    els.btnMatDel.disabled = !hasMat;
    els.btnMatAssignObj.disabled = !hasMat || !state.inspector || !state.inspector.mesh;

    if (!activeMat) return;
    setInputValueIfUnfocused(els.matName, activeMat.name);
    if (!isFocused(els.matBaseColor)) {
      els.matBaseColor.value = rgbFloatToHex(activeMat.base_color);
    }
    setInputValueIfUnfocused(els.matMetallic, activeMat.metallic.toFixed(2));
    els.valMatMetallic.textContent = Number(activeMat.metallic).toFixed(2);

    setInputValueIfUnfocused(els.matRoughness, activeMat.roughness.toFixed(2));
    els.valMatRoughness.textContent = Number(activeMat.roughness).toFixed(2);

    setInputValueIfUnfocused(els.matAo, activeMat.ambient_occlusion.toFixed(2));
    els.valMatAo.textContent = Number(activeMat.ambient_occlusion).toFixed(2);

    if (!isFocused(els.matEmissiveColor)) {
      els.matEmissiveColor.value = rgbFloatToHex(activeMat.emissive_color);
    }
    setInputValueIfUnfocused(
      els.matEmissiveStrength,
      formatFloat(activeMat.emissive_strength, 2)
    );
    setInputValueIfUnfocused(els.matOpacity, activeMat.opacity.toFixed(2));
    els.valMatOpacity.textContent = Number(activeMat.opacity).toFixed(2);
  }

  // =========================================================================
  // DISPATCH HELPERS & FILE SERIALIZATION
  // =========================================================================

  function dispatchTransformFromInputs(coalesce) {
    if (!wasmApi || !currentState || !currentState.inspector) return;
    const id = currentState.inspector.id;
    const pos = [Number(els.posX.value), Number(els.posY.value), Number(els.posZ.value)];
    const rot = [Number(els.rotX.value), Number(els.rotY.value), Number(els.rotZ.value)];
    const scl = [Number(els.sclX.value), Number(els.sclY.value), Number(els.sclZ.value)];
    if ([...pos, ...rot, ...scl].some((v) => !Number.isFinite(v))) return;
    wasmApi.dispatchAction({
      action: "set_transform",
      id: id,
      position: pos,
      rotation_deg: rot,
      scale: scl,
      coalesce: Boolean(coalesce),
    });
  }

  function dispatchCameraFromInputs() {
    if (!wasmApi || !currentState || !currentState.inspector || !currentState.inspector.camera) return;
    wasmApi.dispatchAction({
      action: "set_camera",
      id: currentState.inspector.id,
      projection: els.camProjection.value,
      fov_deg: Number(els.camFov.value),
      ortho_height: Number(els.camOrtho.value),
      near_plane: Number(els.camNear.value),
      far_plane: Number(els.camFar.value),
    });
  }

  function dispatchLightFromInputs(coalesce) {
    if (!wasmApi || !currentState || !currentState.inspector || !currentState.inspector.light) return;
    const lt = currentState.inspector.light.type || (els.lightType ? els.lightType.value : "directional");
    const payload = {
      action: "set_light",
      id: currentState.inspector.id,
      type: lt,
      enabled: els.lightEnabled ? Boolean(els.lightEnabled.checked) : true,
      color: hexToRgbFloat(els.lightColor.value),
      intensity: Number(els.lightIntensity.value),
      shadow: {
        enabled: els.lightShadowEnabled ? Boolean(els.lightShadowEnabled.checked) : false,
        quality: els.lightShadowQuality ? els.lightShadowQuality.value : "medium",
        filter: els.lightShadowFilter ? els.lightShadowFilter.value : "pcf3x3",
        strength: els.lightShadowStrength ? Number(els.lightShadowStrength.value) : 1.0,
        bias: els.lightShadowBias ? Number(els.lightShadowBias.value) : 0.0025,
        normal_bias: els.lightShadowNormalBias ? Number(els.lightShadowNormalBias.value) : 0.02,
        softness: els.lightShadowSoftness ? Number(els.lightShadowSoftness.value) : 1.0,
      },
      coalesce: Boolean(coalesce),
    };
    if (lt === "point" || lt === "spot" || lt === "area") {
      if (els.lightRange) payload.range = Number(els.lightRange.value);
    }
    if (lt === "point" || lt === "spot") {
      if (els.lightFalloff) payload.falloff = els.lightFalloff.value;
      if (els.lightAttenuation) payload.attenuation = Number(els.lightAttenuation.value);
    }
    if (lt === "point" && els.lightSourceRadius) {
      payload.source_radius = Number(els.lightSourceRadius.value);
    }
    if (lt === "spot") {
      if (els.lightInnerCone) payload.inner_cone_deg = Number(els.lightInnerCone.value);
      if (els.lightOuterCone) payload.outer_cone_deg = Number(els.lightOuterCone.value);
      if (els.lightConeFalloff) payload.cone_falloff = Number(els.lightConeFalloff.value);
    }
    if (lt === "area") {
      if (els.lightAreaShape) payload.shape = els.lightAreaShape.value;
      if (els.lightAreaWidth) payload.width = Number(els.lightAreaWidth.value);
      if (els.lightAreaHeight) payload.height = Number(els.lightAreaHeight.value);
      if (els.lightAreaTwoSided) payload.two_sided = Boolean(els.lightAreaTwoSided.checked);
      if (els.lightAreaSamples) payload.sample_quality = Number(els.lightAreaSamples.value);
    }
    wasmApi.dispatchAction(payload);
  }

  function dispatchLightLinkingFromInputs() {
    if (!wasmApi || !currentState || !currentState.inspector || !currentState.inspector.light) return;
    const mode = els.lightLinkMode ? els.lightLinkMode.value : "all";
    const includeDesc = els.lightLinkDescendants ? Boolean(els.lightLinkDescendants.checked) : true;
    const prevLinking =
      currentState.inspector.light.linking || currentState.inspector.light_linking || {};
    let linkedIds = prevLinking.object_ids || prevLinking.linked_objects || [];
    if (mode === "all") {
      linkedIds = [];
    } else if (els.lightLinkTargets && els.lightLinkTargets.children.length > 0 && !els.lightLinkListWrap.hidden) {
      const checkedInputs = els.lightLinkTargets.querySelectorAll('input[type="checkbox"]:checked');
      linkedIds = Array.from(checkedInputs)
        .map((cb) => Number(cb.dataset.objId))
        .filter((v) => Number.isFinite(v) && v > 0);
    }
    wasmApi.dispatchAction({
      action: "set_light_linking",
      id: currentState.inspector.id,
      mode: mode,
      include_descendants: includeDesc,
      object_ids: linkedIds,
    });
  }

  function dispatchEnvironmentFromInputs(coalesce) {
    if (!wasmApi || !els.envEnabled) return;
    const intensity = Number(els.envIntensity ? els.envIntensity.value : 1);
    const rotation = Number(els.envRotation ? els.envRotation.value : 0);
    const diffScale = Number(els.envDiffuseScale ? els.envDiffuseScale.value : 1);
    const specScale = Number(els.envSpecularScale ? els.envSpecularScale.value : 1);
    if (els.valEnvIntensity) els.valEnvIntensity.textContent = intensity.toFixed(2);
    if (els.valEnvRotation) els.valEnvRotation.textContent = Math.round(rotation) + "°";
    if (els.valEnvDiffuse) els.valEnvDiffuse.textContent = diffScale.toFixed(2);
    if (els.valEnvSpecular) els.valEnvSpecular.textContent = specScale.toFixed(2);

    wasmApi.dispatchAction({
      action: "set_environment",
      enabled: Boolean(els.envEnabled.checked),
      intensity: intensity,
      rotation_deg: rotation,
      diffuse_ibl_scale: diffScale,
      specular_ibl_scale: specScale,
      coalesce: Boolean(coalesce),
    });
  }

  async function importHdriFile(file) {
    if (!wasmApi || !file) return false;
    const buf = new Uint8Array(await file.arrayBuffer());
    return wasmApi.loadHdriBytes(buf, file.name);
  }

  function dispatchMaterialFromInputs(coalesce) {
    if (!wasmApi || !currentState || !currentState.active_material_id) return;
    els.valMatMetallic.textContent = Number(els.matMetallic.value).toFixed(2);
    els.valMatRoughness.textContent = Number(els.matRoughness.value).toFixed(2);
    els.valMatAo.textContent = Number(els.matAo.value).toFixed(2);
    els.valMatOpacity.textContent = Number(els.matOpacity.value).toFixed(2);

    wasmApi.dispatchAction({
      action: "update_material",
      id: currentState.active_material_id,
      base_color: hexToRgbFloat(els.matBaseColor.value),
      metallic: Number(els.matMetallic.value),
      roughness: Number(els.matRoughness.value),
      ambient_occlusion: Number(els.matAo.value),
      emissive_color: hexToRgbFloat(els.matEmissiveColor.value),
      emissive_strength: Number(els.matEmissiveStrength.value),
      opacity: Number(els.matOpacity.value),
      coalesce: Boolean(coalesce),
    });
  }

  function dispatchMetadataFromInputs() {
    if (!wasmApi) return;
    wasmApi.dispatchAction({
      action: "set_metadata",
      project_name: els.metaProjectName.value.trim(),
      author: els.metaAuthor.value.trim(),
      scene_name: els.metaSceneName.value.trim(),
    });
  }

  async function importSelectedFiles(fileList) {
    if (!wasmApi || !fileList || fileList.length === 0) return;
    if (els.launcherErrorBanner) els.launcherErrorBanner.hidden = true;

    const files = Array.from(fileList);
    try {
      wasmApi.fs.mkdir("/fe3d_import");
    } catch (_) {}

    let fe3dTargetPath = null;
    let hdriCandidateFile = null;
    for (const file of files) {
      const lower = file.name.toLowerCase();
      if ((lower.endsWith(".hdr") || lower.endsWith(".pfm")) && !hdriCandidateFile) {
        hdriCandidateFile = file;
      }
      const buf = new Uint8Array(await file.arrayBuffer());
      const vfsPath = "/fe3d_import/" + file.name;
      wasmApi.fs.writeFile(vfsPath, buf);
      if (lower.endsWith(".fe3d") || lower.endsWith(".json")) {
        fe3dTargetPath = vfsPath;
      }
    }

    if (!fe3dTargetPath && hdriCandidateFile) {
      await importHdriFile(hdriCandidateFile);
      return;
    }

    if (!fe3dTargetPath) {
      const msg = "Please include a .fe3d project file when opening a project.";
      els.statusDot.classList.add("is-error");
      els.statusMessage.textContent = msg;
      if (document.body.dataset.appView === "home" && els.launcherErrorBanner) {
        els.launcherErrorBanner.hidden = false;
        els.launcherErrorBanner.textContent = msg;
      }
      return;
    }

    const ok = wasmApi.loadFileFromVfs(fe3dTargetPath);
    if (!ok) {
      const errText =
        currentState && currentState.status
          ? currentState.status.message
          : "Invalid or corrupted .fe3d project file.";
      if (document.body.dataset.appView === "home" && els.launcherErrorBanner) {
        els.launcherErrorBanner.hidden = false;
        els.launcherErrorBanner.textContent = "Failed to open project: " + errText;
      }
      return;
    }

    activeProjectRecordId = null;
    setAppView("studio");
    setTimeout(() => snapshotActiveProjectToRecent("Opened .fe3d File"), 80);
  }

  function exportPortableFe3d() {
    if (!wasmApi || !currentState) return;
    const jsonText = wasmApi.exportInlineProject();
    if (!jsonText) return;
    const stem = sanitizeFilenameStem(currentState.project.project_name);
    triggerDownload(stem + ".fe3d", jsonText, "application/json");
    snapshotActiveProjectToRecent("Saved .fe3d");
    els.statusDot.classList.remove("is-error");
    els.statusMessage.textContent = "Saved " + stem + ".fe3d (inline schema 1.3)";
  }

  function exportBinaryBundle() {
    if (!wasmApi || !currentState) return;
    try {
      wasmApi.fs.mkdir("/fe3d_export");
    } catch (_) {}
    const stem = sanitizeFilenameStem(currentState.project.project_name);
    const fe3dPath = "/fe3d_export/" + stem + ".fe3d";
    const binPath = "/fe3d_export/" + stem + ".fe3dbin";
    if (!wasmApi.saveBundleToVfs(fe3dPath)) {
      syncFromEngine(true);
      return;
    }
    const fe3dBytes = wasmApi.fs.readFile(fe3dPath);
    const binBytes = wasmApi.fs.readFile(binPath);
    triggerDownload(stem + ".fe3d", fe3dBytes, "application/json");
    triggerDownload(stem + ".fe3dbin", binBytes, "application/octet-stream");
    snapshotActiveProjectToRecent("Exported Bundle");
  }

  // =========================================================================
  // UNIFIED TOUCH / STYLUS / MULTI-TOUCH VIEWPORT GESTURE CONTROLLER
  // =========================================================================

  function bindViewportTouchAndPointer() {
    if (!canvas) return;
    const activeTouches = new Map();
    const touchState = {
      mode: "idle",
      movedBeyondTap: false,
      lastPinchDist: 0,
      lastMidX: 0,
      lastMidY: 0,
    };

    function canvasRelativeCss(ev) {
      const rect = canvas.getBoundingClientRect();
      return {
        x: ev.clientX - rect.left,
        y: ev.clientY - rect.top,
      };
    }

    canvas.addEventListener("pointerdown", (ev) => {
      if (ev.pointerType === "mouse") return;
      if (!wasmApi) return;

      ev.preventDefault();
      try {
        canvas.setPointerCapture(ev.pointerId);
      } catch (_) {}

      const pt = canvasRelativeCss(ev);
      activeTouches.set(ev.pointerId, {
        startX: pt.x,
        startY: pt.y,
        x: pt.x,
        y: pt.y,
      });

      if (activeTouches.size === 1) {
        if (wasmApi.hitTestGizmo(pt.x, pt.y)) {
          touchState.mode = "gizmo";
          touchState.movedBeyondTap = true;
          wasmApi.pointerEvent(1, 0, pt.x, pt.y, touchMultiSelect, false, false, false);
        } else {
          touchState.mode = "single";
          touchState.movedBeyondTap = false;
        }
      } else if (activeTouches.size >= 2 && touchState.mode !== "gizmo") {
        touchState.mode = "pinch";
        touchState.movedBeyondTap = true;
        const pts = Array.from(activeTouches.values());
        touchState.lastPinchDist = Math.hypot(pts[1].x - pts[0].x, pts[1].y - pts[0].y);
        touchState.lastMidX = (pts[0].x + pts[1].x) * 0.5;
        touchState.lastMidY = (pts[0].y + pts[1].y) * 0.5;
      }
    });

    canvas.addEventListener("pointermove", (ev) => {
      if (ev.pointerType === "mouse") return;
      if (!wasmApi || !activeTouches.has(ev.pointerId)) return;
      ev.preventDefault();

      const rec = activeTouches.get(ev.pointerId);
      const pt = canvasRelativeCss(ev);
      const dx = pt.x - rec.x;
      const dy = pt.y - rec.y;
      rec.x = pt.x;
      rec.y = pt.y;

      if (touchState.mode === "gizmo") {
        wasmApi.pointerEvent(0, 0, pt.x, pt.y, touchMultiSelect, false, false, false);
        return;
      }

      if (activeTouches.size === 1 && touchState.mode === "single") {
        const totalDist = Math.hypot(pt.x - rec.startX, pt.y - rec.startY);
        if (!touchState.movedBeyondTap && totalDist < 8) {
          return;
        }
        touchState.movedBeyondTap = true;
        if (touchNavMode === "pan") {
          wasmApi.cameraPan(dx, dy);
        } else if (touchNavMode === "zoom") {
          wasmApi.cameraZoom(-dy * 0.04);
        } else {
          wasmApi.cameraOrbit(dx, dy);
        }
      } else if (activeTouches.size >= 2) {
        touchState.mode = "pinch";
        touchState.movedBeyondTap = true;
        const pts = Array.from(activeTouches.values());
        const dist = Math.hypot(pts[1].x - pts[0].x, pts[1].y - pts[0].y);
        const midX = (pts[0].x + pts[1].x) * 0.5;
        const midY = (pts[0].y + pts[1].y) * 0.5;

        const midDx = midX - touchState.lastMidX;
        const midDy = midY - touchState.lastMidY;
        const pinchDelta = dist - touchState.lastPinchDist;

        if (Math.abs(midDx) > 0.2 || Math.abs(midDy) > 0.2) {
          wasmApi.cameraPan(midDx, midDy);
        }
        if (Math.abs(pinchDelta) > 0.5) {
          wasmApi.cameraZoom(pinchDelta * 0.035);
        }

        touchState.lastPinchDist = dist;
        touchState.lastMidX = midX;
        touchState.lastMidY = midY;
      }
    });

    function finishTouch(ev, canceled) {
      if (ev.pointerType === "mouse") return;
      if (!activeTouches.has(ev.pointerId)) return;
      const rec = activeTouches.get(ev.pointerId);
      activeTouches.delete(ev.pointerId);

      if (touchState.mode === "gizmo" && activeTouches.size === 0) {
        wasmApi.pointerEvent(2, 0, rec.x, rec.y, touchMultiSelect, false, false, false);
        touchState.mode = "idle";
        setTimeout(() => syncFromEngine(true), 20);
        return;
      }

      if (
        !canceled &&
        touchState.mode === "single" &&
        !touchState.movedBeyondTap &&
        activeTouches.size === 0
      ) {
        wasmApi.touchTap(rec.x, rec.y, touchMultiSelect);
        setTimeout(() => syncFromEngine(true), 20);
      }

      if (activeTouches.size === 0) {
        touchState.mode = "idle";
        touchState.movedBeyondTap = false;
        setTimeout(() => syncFromEngine(false), 20);
      }
    }

    canvas.addEventListener("pointerup", (ev) => finishTouch(ev, false));
    canvas.addEventListener("pointercancel", (ev) => finishTouch(ev, true));
  }

  // =========================================================================
  // INTERACTIVE 3D XYZ VIEW NAVIGATION GIZMO (Editor Observer Camera Only)
  // =========================================================================

  const VIEW_GIZMO_AXES = [
    { id: "pos_x", preset: "pos_x", dir: [1, 0, 0], label: "X", tip: "Right (+X)", color: "#fa454f", fillHi: "#ff8c92", positive: true },
    { id: "neg_x", preset: "neg_x", dir: [-1, 0, 0], label: "-X", tip: "Left (-X)", color: "#fa454f", fillHi: "#ff8c92", positive: false },
    { id: "pos_y", preset: "pos_y", dir: [0, 1, 0], label: "Y", tip: "Top (+Y)", color: "#3fe068", fillHi: "#94f7ad", positive: true },
    { id: "neg_y", preset: "neg_y", dir: [0, -1, 0], label: "-Y", tip: "Bottom (-Y)", color: "#3fe068", fillHi: "#94f7ad", positive: false },
    { id: "pos_z", preset: "pos_z", dir: [0, 0, 1], label: "Z", tip: "Front (+Z)", color: "#47a3ff", fillHi: "#9dcfff", positive: true },
    { id: "neg_z", preset: "neg_z", dir: [0, 0, -1], label: "-Z", tip: "Back (-Z)", color: "#47a3ff", fillHi: "#9dcfff", positive: false },
  ];

  const INV_SQRT3 = 0.577350269;
  const VIEW_GIZMO_DIAGONALS = [
    { id: "diag_px_py_pz", preset: "diag_px_py_pz", dir: [INV_SQRT3, INV_SQRT3, INV_SQRT3], tip: "3/4 (+X+Y+Z)" },
    { id: "diag_nx_py_pz", preset: "diag_nx_py_pz", dir: [-INV_SQRT3, INV_SQRT3, INV_SQRT3], tip: "3/4 (-X+Y+Z)" },
    { id: "diag_px_py_nz", preset: "diag_px_py_nz", dir: [INV_SQRT3, INV_SQRT3, -INV_SQRT3], tip: "3/4 (+X+Y-Z)" },
    { id: "diag_nx_py_nz", preset: "diag_nx_py_nz", dir: [-INV_SQRT3, INV_SQRT3, -INV_SQRT3], tip: "3/4 (-X+Y-Z)" },
    { id: "diag_px_ny_pz", preset: "diag_px_ny_pz", dir: [INV_SQRT3, -INV_SQRT3, INV_SQRT3], tip: "3/4 (+X-Y+Z)" },
    { id: "diag_nx_ny_pz", preset: "diag_nx_ny_pz", dir: [-INV_SQRT3, -INV_SQRT3, INV_SQRT3], tip: "3/4 (-X-Y+Z)" },
    { id: "diag_px_ny_nz", preset: "diag_px_ny_nz", dir: [INV_SQRT3, -INV_SQRT3, -INV_SQRT3], tip: "3/4 (+X-Y-Z)" },
    { id: "diag_nx_ny_nz", preset: "diag_nx_ny_nz", dir: [-INV_SQRT3, -INV_SQRT3, -INV_SQRT3], tip: "3/4 (-X-Y-Z)" },
  ];

  const viewGizmoUiState = {
    hoveredId: null,
    hoverTip: "",
    pointerInside: false,
    dragging: false,
    rafPending: false,
    orbitRafPending: false,
  };

  function scheduleViewTransitionSync() {
    if (viewGizmoUiState.rafPending) return;
    viewGizmoUiState.rafPending = true;
    requestAnimationFrame(() => {
      viewGizmoUiState.rafPending = false;
      syncFromEngine(false);
    });
  }

  function computeViewNavGizmoGeometry(state, cssSize) {
    const vp = (state && state.viewport) || {};
    const camRight = Array.isArray(vp.camera_right) && vp.camera_right.length === 3 ? vp.camera_right : [1, 0, 0];
    const camUp = Array.isArray(vp.camera_up) && vp.camera_up.length === 3 ? vp.camera_up : [0, 1, 0];
    const camFwd = Array.isArray(vp.camera_forward) && vp.camera_forward.length === 3 ? vp.camera_forward : [0, 0, -1];

    const size = Math.max(64, cssSize || 94);
    const cx = size * 0.5;
    const cy = size * 0.5;
    const sphereR = size * 0.445;
    const axisR = size * 0.355;
    const diagR = size * 0.225;

    function projectDir(d, radius) {
      const vx = d[0] * camRight[0] + d[1] * camRight[1] + d[2] * camRight[2];
      const vy = d[0] * camUp[0] + d[1] * camUp[1] + d[2] * camUp[2];
      const vz = -(d[0] * camFwd[0] + d[1] * camFwd[1] + d[2] * camFwd[2]);
      return {
        x: cx + vx * radius,
        y: cy - vy * radius,
        vx: vx,
        vy: vy,
        vz: vz,
      };
    }

    const axes = VIEW_GIZMO_AXES.map((a) => {
      const p = projectDir(a.dir, axisR);
      return {
        id: a.id,
        kind: "axis",
        preset: a.preset,
        label: a.label,
        tip: a.tip,
        color: a.color,
        fillHi: a.fillHi,
        positive: a.positive,
        x: p.x,
        y: p.y,
        vz: p.vz,
      };
    });

    const diagonals = VIEW_GIZMO_DIAGONALS.map((d) => {
      const p = projectDir(d.dir, diagR);
      return {
        id: d.id,
        kind: "diagonal",
        preset: d.preset,
        tip: d.tip,
        x: p.x,
        y: p.y,
        vz: p.vz,
      };
    });

    return {
      size: size,
      cx: cx,
      cy: cy,
      sphereR: sphereR,
      axisR: axisR,
      diagR: diagR,
      axes: axes,
      diagonals: diagonals,
      center: {
        id: "center",
        kind: "center",
        preset: "home",
        tip: "Home View (Drag to Orbit)",
        x: cx,
        y: cy,
      },
    };
  }

  function hitTestViewNavGizmo(localX, localY, isTouch, geom) {
    if (!geom) return null;
    const capHitR = isTouch ? 13.5 : 10.5;
    const frontFirstAxes = geom.axes.slice().sort((a, b) => b.vz - a.vz);
    let bestAxis = null;
    let bestAxisScore = Infinity;
    for (let i = 0; i < frontFirstAxes.length; ++i) {
      const a = frontFirstAxes[i];
      const dist = Math.hypot(localX - a.x, localY - a.y);
      if (dist <= capHitR) {
        const depthBias = a.vz < -0.25 ? 2.2 : 0;
        const score = dist + depthBias;
        if (score < bestAxisScore) {
          bestAxisScore = score;
          bestAxis = a;
        }
      }
    }
    if (bestAxis) return bestAxis;

    const diagHitR = isTouch ? 10.0 : 7.2;
    let bestDiag = null;
    let bestDiagDist = diagHitR;
    for (let i = 0; i < geom.diagonals.length; ++i) {
      const d = geom.diagonals[i];
      if (d.vz <= 0.12) continue;
      const dist = Math.hypot(localX - d.x, localY - d.y);
      if (dist <= bestDiagDist) {
        bestDiagDist = dist;
        bestDiag = d;
      }
    }
    if (bestDiag) return bestDiag;

    const centerDist = Math.hypot(localX - geom.cx, localY - geom.cy);
    if (centerDist <= (isTouch ? 10.0 : 7.8)) {
      return geom.center;
    }

    if (centerDist <= geom.sphereR + 4) {
      return { id: "orbit_sphere", kind: "sphere", preset: null, tip: "Drag to Orbit" };
    }
    return null;
  }

  function getNavGizmoLayout() {
    if (!els.viewNavGizmoCanvas) return null;
    syncFromEngine(true);
    const rect = els.viewNavGizmoCanvas.getBoundingClientRect();
    const cssSize = rect.width > 16 ? rect.width : 94;
    const geom = computeViewNavGizmoGeometry(currentState, cssSize);
    const axesClient = {};
    geom.axes.forEach((a) => {
      axesClient[a.id] = {
        x: rect.left + a.x,
        y: rect.top + a.y,
        localX: a.x,
        localY: a.y,
        vz: a.vz,
        label: a.label,
      };
    });
    const frontDiagonals = geom.diagonals
      .filter((d) => d.vz > 0.12)
      .sort((a, b) => b.vz - a.vz)
      .map((d) => ({
        id: d.id,
        preset: d.preset,
        x: rect.left + d.x,
        y: rect.top + d.y,
        localX: d.x,
        localY: d.y,
        vz: d.vz,
      }));
    return {
      rect: { x: rect.left, y: rect.top, width: rect.width, height: rect.height },
      center: { x: rect.left + geom.cx, y: rect.top + geom.cy },
      axes: axesClient,
      diagonals: frontDiagonals,
    };
  }

  function renderViewNavGizmo(state) {
    const cvs = els.viewNavGizmoCanvas;
    if (!cvs) return;
    const ctx = cvs.getContext("2d");
    if (!ctx) return;

    const vp = (state && state.viewport) || {};
    if (els.btnNavProjToggle) {
      const isOrtho = vp.projection === "orthographic";
      els.btnNavProjToggle.textContent = isOrtho ? "Ortho" : "Persp";
      els.btnNavProjToggle.classList.toggle("is-ortho", isOrtho);
    }

    const rect = cvs.getBoundingClientRect();
    const cssSize = Math.round(rect.width > 24 ? rect.width : 94);
    const dpr = Math.max(1, Math.min(3, window.devicePixelRatio || 1));
    const pxSize = Math.round(cssSize * dpr);
    if (cvs.width !== pxSize || cvs.height !== pxSize) {
      cvs.width = pxSize;
      cvs.height = pxSize;
    }

    const geom = computeViewNavGizmoGeometry(state, cssSize);
    ctx.save();
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, cssSize, cssSize);

    // 1. Central orientation sphere backdrop
    const activeSphere = viewGizmoUiState.pointerInside || viewGizmoUiState.dragging;
    const grad = ctx.createRadialGradient(
      geom.cx - geom.sphereR * 0.28,
      geom.cy - geom.sphereR * 0.28,
      geom.sphereR * 0.1,
      geom.cx,
      geom.cy,
      geom.sphereR
    );
    if (activeSphere) {
      grad.addColorStop(0, "rgba(54, 68, 92, 0.84)");
      grad.addColorStop(1, "rgba(18, 23, 32, 0.78)");
    } else {
      grad.addColorStop(0, "rgba(34, 42, 56, 0.62)");
      grad.addColorStop(1, "rgba(15, 19, 26, 0.52)");
    }
    ctx.beginPath();
    ctx.arc(geom.cx, geom.cy, geom.sphereR, 0, Math.PI * 2);
    ctx.fillStyle = grad;
    ctx.fill();
    ctx.lineWidth = viewGizmoUiState.dragging ? 1.6 : 1.0;
    ctx.strokeStyle = viewGizmoUiState.dragging
      ? "rgba(96, 168, 255, 0.88)"
      : activeSphere
      ? "rgba(155, 188, 238, 0.42)"
      : "rgba(255, 255, 255, 0.15)";
    ctx.stroke();

    // 2. Front-hemisphere three-quarter diagonal orientation regions
    for (let i = 0; i < geom.diagonals.length; ++i) {
      const d = geom.diagonals[i];
      if (d.vz <= 0.12) continue;
      const isHov = viewGizmoUiState.hoveredId === d.id;
      const r = isHov ? 4.8 : 2.5;
      ctx.beginPath();
      ctx.arc(d.x, d.y, r, 0, Math.PI * 2);
      ctx.fillStyle = isHov ? "#ffe27a" : "rgba(198, 214, 238, 0.34)";
      ctx.fill();
      if (isHov) {
        ctx.lineWidth = 1.3;
        ctx.strokeStyle = "#ffffff";
        ctx.stroke();
      }
    }

    // 3. Depth-sorted back-to-front XYZ stems & positive/negative endpoints
    const sortedAxes = geom.axes.slice().sort((a, b) => a.vz - b.vz);
    const scaleFactor = cssSize / 94;
    const posCapR = 8.4 * scaleFactor;
    const negCapR = 7.0 * scaleFactor;

    for (let i = 0; i < sortedAxes.length; ++i) {
      const a = sortedAxes[i];
      const isHov = viewGizmoUiState.hoveredId === a.id;
      const depthAlpha = a.vz < -0.15 ? 0.55 : 0.96;

      // Axis stem from center hub to cap edge
      ctx.save();
      ctx.globalAlpha = a.positive ? depthAlpha : depthAlpha * 0.45;
      ctx.beginPath();
      ctx.moveTo(geom.cx, geom.cy);
      ctx.lineTo(a.x, a.y);
      ctx.lineWidth = (a.positive ? 2.3 : 1.3) * scaleFactor;
      ctx.strokeStyle = isHov ? "#ffe484" : a.color;
      ctx.stroke();
      ctx.restore();

      const capR = (a.positive ? posCapR : negCapR) * (isHov ? 1.18 : 1.0);
      ctx.save();
      ctx.globalAlpha = isHov ? 1.0 : depthAlpha;
      ctx.beginPath();
      ctx.arc(a.x, a.y, capR, 0, Math.PI * 2);

      if (a.positive) {
        const capGrad = ctx.createRadialGradient(
          a.x - capR * 0.3,
          a.y - capR * 0.3,
          capR * 0.1,
          a.x,
          a.y,
          capR
        );
        capGrad.addColorStop(0, isHov ? "#fff5b8" : a.fillHi);
        capGrad.addColorStop(1, a.color);
        ctx.fillStyle = capGrad;
        ctx.fill();
        if (isHov) {
          ctx.lineWidth = 1.6;
          ctx.strokeStyle = "#ffffff";
          ctx.stroke();
        }
        ctx.fillStyle = "#0b0e14";
        ctx.font = "700 " + Math.round(9.8 * scaleFactor) + "px Inter, system-ui, sans-serif";
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        ctx.fillText(a.label, a.x, a.y + 0.4);
      } else {
        ctx.fillStyle = isHov ? "rgba(42, 52, 68, 0.96)" : "rgba(17, 21, 29, 0.90)";
        ctx.fill();
        ctx.lineWidth = (isHov ? 2.1 : 1.8) * scaleFactor;
        ctx.strokeStyle = isHov ? "#ffe484" : a.color;
        ctx.stroke();
        ctx.fillStyle = isHov ? "#fff5c0" : "#dbe6f6";
        ctx.font = "700 " + Math.round(7.6 * scaleFactor) + "px Inter, system-ui, sans-serif";
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        ctx.fillText(a.label, a.x, a.y + 0.3);
      }
      ctx.restore();
    }

    // 4. Central orientation control hub
    const centerHov = viewGizmoUiState.hoveredId === "center";
    ctx.beginPath();
    ctx.arc(geom.cx, geom.cy, (centerHov ? 4.8 : 3.4) * scaleFactor, 0, Math.PI * 2);
    ctx.fillStyle = centerHov ? "#ffe27a" : "#e5edf8";
    ctx.fill();
    ctx.lineWidth = 1.2;
    ctx.strokeStyle = "rgba(12, 16, 22, 0.85)";
    ctx.stroke();

    ctx.restore();
  }

  function bindViewNavGizmo() {
    const pod = els.viewportViewGizmo;
    const cvs = els.viewNavGizmoCanvas;
    if (!pod || !cvs) return;

    // Strict event isolation: prevent navigation widget interactions from ever leaking to #fe3d-canvas
    [
      "mousedown",
      "mouseup",
      "click",
      "dblclick",
      "contextmenu",
      "touchstart",
      "touchmove",
      "touchend",
      "wheel",
    ].forEach((evtName) => {
      pod.addEventListener(evtName, (ev) => {
        ev.stopPropagation();
        if (evtName === "contextmenu") ev.preventDefault();
      });
    });

    function localCoords(ev) {
      const rect = cvs.getBoundingClientRect();
      return {
        x: ev.clientX - rect.left,
        y: ev.clientY - rect.top,
        size: rect.width > 24 ? rect.width : 94,
      };
    }

    function updateHoverState(ev) {
      if (viewGizmoUiState.dragging) return;
      const pt = localCoords(ev);
      const isTouch = ev.pointerType === "touch";
      const geom = computeViewNavGizmoGeometry(currentState, pt.size);
      const hit = hitTestViewNavGizmo(pt.x, pt.y, isTouch, geom);
      const nextId = hit ? hit.id : null;
      const nextTip = hit && hit.tip ? hit.tip : "";
      const actionable = Boolean(hit && hit.preset);
      pod.classList.toggle("is-hover-target", actionable);
      if (els.viewGizmoHoverTip) {
        if (nextTip && !isTouch) {
          els.viewGizmoHoverTip.textContent = nextTip;
          els.viewGizmoHoverTip.hidden = false;
        } else {
          els.viewGizmoHoverTip.hidden = true;
        }
      }
      if (nextId !== viewGizmoUiState.hoveredId || !viewGizmoUiState.pointerInside) {
        viewGizmoUiState.hoveredId = nextId;
        viewGizmoUiState.hoverTip = nextTip;
        viewGizmoUiState.pointerInside = true;
        renderViewNavGizmo(currentState);
      }
    }

    let activePtrId = null;
    let startX = 0;
    let startY = 0;
    let lastX = 0;
    let lastY = 0;
    let pressHit = null;
    let movedToOrbit = false;

    cvs.addEventListener("pointerenter", (ev) => {
      ev.stopPropagation();
      updateHoverState(ev);
    });

    cvs.addEventListener("pointermove", (ev) => {
      ev.stopPropagation();
      if (activePtrId === ev.pointerId) {
        ev.preventDefault();
        const dx = ev.clientX - lastX;
        const dy = ev.clientY - lastY;
        lastX = ev.clientX;
        lastY = ev.clientY;
        const travel = Math.hypot(ev.clientX - startX, ev.clientY - startY);
        const dragSlop = ev.pointerType === "touch" ? 7 : 5;
        if (!movedToOrbit && travel >= dragSlop) {
          movedToOrbit = true;
          viewGizmoUiState.dragging = true;
          pod.classList.add("is-dragging");
          if (els.viewGizmoHoverTip) els.viewGizmoHoverTip.hidden = true;
        }
        if (movedToOrbit && wasmApi) {
          wasmApi.editorCameraOrbit(dx * 1.45, dy * 1.45);
          if (!viewGizmoUiState.orbitRafPending) {
            viewGizmoUiState.orbitRafPending = true;
            requestAnimationFrame(() => {
              viewGizmoUiState.orbitRafPending = false;
              syncFromEngine(false);
            });
          }
        }
        return;
      }
      updateHoverState(ev);
    });

    cvs.addEventListener("pointerleave", (ev) => {
      ev.stopPropagation();
      if (activePtrId !== null) return;
      viewGizmoUiState.hoveredId = null;
      viewGizmoUiState.pointerInside = false;
      pod.classList.remove("is-hover-target");
      if (els.viewGizmoHoverTip) els.viewGizmoHoverTip.hidden = true;
      renderViewNavGizmo(currentState);
    });

    cvs.addEventListener("pointerdown", (ev) => {
      ev.stopPropagation();
      if (ev.pointerType === "mouse" && ev.button !== 0) return;
      ev.preventDefault();
      activePtrId = ev.pointerId;
      startX = ev.clientX;
      startY = ev.clientY;
      lastX = ev.clientX;
      lastY = ev.clientY;
      movedToOrbit = false;
      const pt = localCoords(ev);
      const geom = computeViewNavGizmoGeometry(currentState, pt.size);
      pressHit = hitTestViewNavGizmo(pt.x, pt.y, ev.pointerType === "touch", geom);
      try {
        cvs.setPointerCapture(ev.pointerId);
      } catch (_) {}
    });

    function finishPointer(ev, canceled) {
      ev.stopPropagation();
      if (activePtrId !== ev.pointerId) return;
      try {
        cvs.releasePointerCapture(ev.pointerId);
      } catch (_) {}
      activePtrId = null;
      const wasOrbit = movedToOrbit;
      const targetHit = pressHit;
      pressHit = null;
      movedToOrbit = false;
      viewGizmoUiState.dragging = false;
      pod.classList.remove("is-dragging");

      if (!canceled && !wasOrbit && targetHit && targetHit.preset && wasmApi) {
        wasmApi.dispatchAction({
          action: "set_viewport_view",
          view: targetHit.preset,
          smooth: true,
        });
        scheduleViewTransitionSync();
      } else {
        syncFromEngine(false);
      }

      if (ev.pointerType === "touch") {
        viewGizmoUiState.hoveredId = null;
        viewGizmoUiState.pointerInside = false;
      }
      renderViewNavGizmo(currentState);
    }

    cvs.addEventListener("pointerup", (ev) => finishPointer(ev, false));
    cvs.addEventListener("pointercancel", (ev) => finishPointer(ev, true));

    if (els.btnNavHomeView) {
      els.btnNavHomeView.addEventListener("pointerdown", (ev) => ev.stopPropagation());
      els.btnNavHomeView.addEventListener("click", (ev) => {
        ev.stopPropagation();
        if (!wasmApi) return;
        wasmApi.dispatchAction({ action: "reset_viewport_view", smooth: true });
        scheduleViewTransitionSync();
      });
    }

    if (els.btnNavProjToggle) {
      els.btnNavProjToggle.addEventListener("pointerdown", (ev) => ev.stopPropagation());
      els.btnNavProjToggle.addEventListener("click", (ev) => {
        ev.stopPropagation();
        if (!wasmApi) return;
        wasmApi.dispatchAction({ action: "toggle_viewport_projection" });
      });
    }

    window.addEventListener("resize", () => {
      renderViewNavGizmo(currentState);
      setTimeout(() => syncFromEngine(false), 30);
    });
  }

  // =========================================================================
  // DRAGGABLE WORKSPACE DOCK SPLITTERS (Right Dock, Bottom Dock, Outliner/Props)
  // =========================================================================

  function bindDockSplitters() {
    if (els.splitterRight) {
      els.splitterRight.addEventListener("pointerdown", (ev) => {
        if (ev.button !== 0 && ev.pointerType === "mouse") return;
        ev.preventDefault();
        document.body.classList.add("is-resizing-col");
        els.splitterRight.classList.add("is-dragging");
        els.splitterRight.setPointerCapture(ev.pointerId);
        const startX = ev.clientX;
        const startW = els.dockRight.getBoundingClientRect().width;

        function onMove(mev) {
          const maxRight = Math.max(240, Math.min(520, Math.floor((window.innerWidth || 1280) * 0.42)));
          const nextW = Math.round(Math.max(230, Math.min(maxRight, startW - (mev.clientX - startX))));
          document.documentElement.style.setProperty("--right-dock-w", nextW + "px");
        }
        function onUp(uev) {
          document.body.classList.remove("is-resizing-col");
          els.splitterRight.classList.remove("is-dragging");
          els.splitterRight.removeEventListener("pointermove", onMove);
          els.splitterRight.removeEventListener("pointerup", onUp);
          els.splitterRight.removeEventListener("pointercancel", onUp);
          try {
            els.splitterRight.releasePointerCapture(uev.pointerId);
          } catch (_) {}
          if (wasmApi) {
            const ratio = els.dockRight.getBoundingClientRect().width / Math.max(1, window.innerWidth);
            wasmApi.dispatchAction({ action: "set_dock_ratios", right: ratio });
            persistWorkspaceToStorage();
          }
        }
        els.splitterRight.addEventListener("pointermove", onMove);
        els.splitterRight.addEventListener("pointerup", onUp);
        els.splitterRight.addEventListener("pointercancel", onUp);
      });
    }

    if (els.splitterBottom) {
      els.splitterBottom.addEventListener("pointerdown", (ev) => {
        if (ev.button !== 0 && ev.pointerType === "mouse") return;
        ev.preventDefault();
        els.dockBottom.classList.remove("is-collapsed");
        if (els.btnCollapseBottom) {
          els.btnCollapseBottom.textContent = "−";
          els.btnCollapseBottom.setAttribute("aria-expanded", "true");
        }
        document.body.classList.add("is-resizing-row");
        els.splitterBottom.classList.add("is-dragging");
        els.splitterBottom.setPointerCapture(ev.pointerId);
        const startY = ev.clientY;
        const startH = els.dockBottom.getBoundingClientRect().height;

        function onMove(mev) {
          const maxBottom = Math.max(84, Math.min(360, Math.floor(((window.innerHeight || 720) - 90) * 0.46)));
          const nextH = Math.round(Math.max(76, Math.min(maxBottom, startH - (mev.clientY - startY))));
          document.documentElement.style.setProperty("--bottom-dock-h", nextH + "px");
        }
        function onUp(uev) {
          document.body.classList.remove("is-resizing-row");
          els.splitterBottom.classList.remove("is-dragging");
          els.splitterBottom.removeEventListener("pointermove", onMove);
          els.splitterBottom.removeEventListener("pointerup", onUp);
          els.splitterBottom.removeEventListener("pointercancel", onUp);
          try {
            els.splitterBottom.releasePointerCapture(uev.pointerId);
          } catch (_) {}
          if (wasmApi) {
            const ratio = els.dockBottom.getBoundingClientRect().height / Math.max(1, window.innerHeight);
            wasmApi.dispatchAction({ action: "set_dock_ratios", bottom: ratio });
            persistWorkspaceToStorage();
          }
        }
        els.splitterBottom.addEventListener("pointermove", onMove);
        els.splitterBottom.addEventListener("pointerup", onUp);
        els.splitterBottom.addEventListener("pointercancel", onUp);
      });
    }

    if (els.splitterOutliner) {
      els.splitterOutliner.addEventListener("pointerdown", (ev) => {
        if (ev.button !== 0 && ev.pointerType === "mouse") return;
        ev.preventDefault();
        document.body.classList.add("is-resizing-row");
        els.splitterOutliner.classList.add("is-dragging");
        els.splitterOutliner.setPointerCapture(ev.pointerId);
        const startY = ev.clientY;
        const dockRect = els.dockRight.getBoundingClientRect();
        const startH = els.panelOutliner.getBoundingClientRect().height;

        function onMove(mev) {
          const totalH = Math.max(220, dockRect.height);
          const nextPx = startH + (mev.clientY - startY);
          const nextPct = Math.max(20, Math.min(62, (nextPx / totalH) * 100));
          document.documentElement.style.setProperty("--outliner-h-pct", nextPct.toFixed(1) + "%");
        }
        function onUp(uev) {
          document.body.classList.remove("is-resizing-row");
          els.splitterOutliner.classList.remove("is-dragging");
          els.splitterOutliner.removeEventListener("pointermove", onMove);
          els.splitterOutliner.removeEventListener("pointerup", onUp);
          els.splitterOutliner.removeEventListener("pointercancel", onUp);
          try {
            els.splitterOutliner.releasePointerCapture(uev.pointerId);
          } catch (_) {}
        }
        els.splitterOutliner.addEventListener("pointermove", onMove);
        els.splitterOutliner.addEventListener("pointerup", onUp);
        els.splitterOutliner.addEventListener("pointercancel", onUp);
      });
    }
  }

  // =========================================================================
  // VIEWPORT CONTEXT MENU (Stationary Right-Click, Clamped Inside Viewport)
  // =========================================================================

  function openViewportContextMenu(clientX, clientY) {
    if (!els.viewportContextMenu || !els.viewportStage) return;
    closeAllDropdownMenus();

    const ed = currentState && currentState.editor ? currentState.editor : null;
    const insp = currentState && currentState.inspector ? currentState.inspector : null;
    const isMeshMode = Boolean(ed && ed.mode !== "object" && ed.editable_mesh && ed.editable_mesh.editable);

    if (els.ctxMenuTitle) {
      els.ctxMenuTitle.textContent = isMeshMode
        ? ed.mode.charAt(0).toUpperCase() + ed.mode.slice(1) + " Context Menu"
        : insp
        ? "Object Context · " + insp.name
        : "Viewport Context Menu";
    }
    if (els.ctxCmdDuplicate) els.ctxCmdDuplicate.disabled = !insp || isMeshMode;
    if (els.ctxCmdResetTf) els.ctxCmdResetTf.disabled = !insp;
    if (els.ctxCmdDelete) els.ctxCmdDelete.disabled = !insp && !isMeshMode;

    if (els.ctxMeshSep) els.ctxMeshSep.hidden = !isMeshMode;
    if (els.ctxCmdExtrude) {
      els.ctxCmdExtrude.hidden = !isMeshMode;
      els.ctxCmdExtrude.disabled =
        !isMeshMode || ed.mode !== "face" || (ed.mesh_selection.faces || []).length === 0;
    }
    if (els.ctxCmdBoundary) {
      els.ctxCmdBoundary.hidden = !isMeshMode;
      els.ctxCmdBoundary.disabled = !isMeshMode || ed.editable_mesh.open_boundary_edges < 3;
    }
    if (els.ctxCmdFill) {
      els.ctxCmdFill.hidden = !isMeshMode;
      els.ctxCmdFill.disabled =
        !isMeshMode || ed.mode !== "vertex" || (ed.mesh_selection.vertices || []).length < 3;
    }

    const stageRect = els.viewportStage.getBoundingClientRect();
    els.viewportContextMenu.style.left = "6px";
    els.viewportContextMenu.style.top = "6px";
    els.viewportContextMenu.hidden = false;

    const menuRect = els.viewportContextMenu.getBoundingClientRect();
    const relX = clientX - stageRect.left;
    const relY = clientY - stageRect.top;
    const maxX = Math.max(6, stageRect.width - menuRect.width - 6);
    const maxY = Math.max(6, stageRect.height - menuRect.height - 6);
    const clampedX = Math.round(Math.max(6, Math.min(maxX, relX)));
    const clampedY = Math.round(Math.max(6, Math.min(maxY, relY)));

    els.viewportContextMenu.style.left = clampedX + "px";
    els.viewportContextMenu.style.top = clampedY + "px";
  }

  // =========================================================================
  // MENUS, MODALS & STUDIO UI EVENT BINDINGS
  // =========================================================================

  function closeAllDropdownMenus() {
    document.querySelectorAll(".menu-item.is-open").forEach((item) => {
      item.classList.remove("is-open");
      const panel = item.querySelector(".menu-dropdown");
      if (panel) {
        panel.hidden = true;
        panel.style.left = "";
      }
    });
  }

  function bindUiEvents() {
    renderStudioSelector();
    renderSelectedStudioHomePanel();
    renderWorkspaceTabBar();
    setInspectorCategory("all");

    if (els.launcherProjectAuthor) {
      els.launcherProjectAuthor.value = appPrefs.defaultAuthor || "Final Effect 3D";
    }

    els.btnLauncherCreate.addEventListener("click", () => {
      if (!wasmApi) return;
      const projName = (els.launcherProjectName.value || "").trim() || "Untitled Studio Project";
      const projAuthor =
        (els.launcherProjectAuthor.value || "").trim() ||
        appPrefs.defaultAuthor ||
        "Final Effect 3D";
      activeProjectRecordId = null;
      wasmApi.dispatchAction({
        action: "create_new_project",
        template: selectedTemplateId,
        project_name: projName,
        author: projAuthor,
      });
      activateWorkspaceById("layout", false);
      setAppView("studio");
      requestAnimationFrame(() => {
        requestAnimationFrame(() => {
          snapshotActiveProjectToRecent("Created New Project");
        });
      });
    });

    els.btnLauncherOpen.addEventListener("click", () => {
      els.fileOpenInput.value = "";
      els.fileOpenInput.click();
    });

    els.btnHomeResumeStudio.addEventListener("click", () => {
      setAppView("studio");
    });

    els.btnFutureBack3d.addEventListener("click", () => {
      selectedStudioId = "3d_studio";
      renderStudioSelector();
      renderSelectedStudioHomePanel();
    });

    if (els.btnFutureJumpShading) {
      els.btnFutureJumpShading.addEventListener("click", () => {
        selectedStudioId = "3d_studio";
        renderStudioSelector();
        renderSelectedStudioHomePanel();
        setAppView("studio");
        activateWorkspaceById("shading", false);
      });
    }

    els.btnGoHome.addEventListener("click", async () => {
      closeAllDropdownMenus();
      await snapshotActiveProjectToRecent("Active Session");
      setAppView("home");
    });

    document.querySelectorAll("[data-menu-trigger]").forEach((trigger) => {
      trigger.addEventListener("click", (ev) => {
        ev.stopPropagation();
        closeViewportContextMenu();
        const item = trigger.closest(".menu-item");
        const panel = item.querySelector(".menu-dropdown");
        const wasOpen = item.classList.contains("is-open");
        closeAllDropdownMenus();
        if (!wasOpen) {
          item.classList.add("is-open");
          panel.style.left = "0px";
          panel.hidden = false;
          const rect = panel.getBoundingClientRect();
          const winW = window.innerWidth || 1280;
          if (rect.right > winW - 6) {
            const shift = Math.round(rect.right - (winW - 6));
            panel.style.left = -shift + "px";
          }
        }
      });
    });

    document.addEventListener("click", (ev) => {
      closeAllDropdownMenus();
      if (
        els.viewportContextMenu &&
        !els.viewportContextMenu.hidden &&
        !els.viewportContextMenu.contains(ev.target)
      ) {
        closeViewportContextMenu();
      }
    });

    window.addEventListener(
      "keydown",
      (ev) => {
        if (ev.key === "Escape") {
          const hadMenuOpen = Boolean(document.querySelector(".menu-item.is-open"));
          const hadCtxOpen = Boolean(els.viewportContextMenu && !els.viewportContextMenu.hidden);
          const hadModalOpen = Boolean(els.settingsModal && !els.settingsModal.hidden);
          if (hadMenuOpen || hadCtxOpen || hadModalOpen) {
            closeAllDropdownMenus();
            closeViewportContextMenu();
            if (hadModalOpen) {
              els.settingsModal.hidden = true;
            }
            ev.preventDefault();
            ev.stopImmediatePropagation();
          }
        }
      },
      true
    );

    function toggleRightDockVisibility() {
      const mode = document.body.dataset.layoutMode || "desktop";
      if (mode === "landscape") {
        const closed = document.body.classList.toggle("is-landscape-drawer-closed");
        if (els.btnLandscapeInspectorToggle) {
          els.btnLandscapeInspectorToggle.classList.toggle("is-active", !closed);
        }
      } else {
        isRightDockCollapsed = !isRightDockCollapsed;
      }
      if (currentState) renderWorkspaceAndBottomDock(currentState);
    }

    function setBottomDockCollapsed(collapsed) {
      if (!els.dockBottom) return;
      els.dockBottom.classList.toggle("is-collapsed", collapsed);
      if (els.btnCollapseBottom) {
        els.btnCollapseBottom.textContent = collapsed ? "+" : "−";
        els.btnCollapseBottom.setAttribute("aria-expanded", collapsed ? "false" : "true");
        els.btnCollapseBottom.title = collapsed
          ? "Expand Bottom Editor Area"
          : "Collapse Bottom Editor Area";
      }
      if (currentState) renderWorkspaceAndBottomDock(currentState);
    }

    function saveNamedCustomWorkspace() {
      if (!wasmApi) return;
      const wsJson = wasmApi.exportWorkspaceJson();
      if (wsJson && REG.saveCustomWorkspaceLayout(wsJson)) {
        persistWorkspaceToStorage();
        els.statusDot.classList.remove("is-error");
        els.statusMessage.textContent = "Saved custom workspace layout (scene data untouched)";
      }
    }

    function restoreNamedCustomWorkspace() {
      if (!wasmApi) return;
      const saved = REG.loadCustomWorkspaceLayout();
      if (!saved) {
        els.statusMessage.textContent = "No custom workspace layout saved yet.";
        return;
      }
      try {
        const parsed = JSON.parse(saved);
        const wsObj = parsed.workspace || parsed;
        isRightDockCollapsed = false;
        syncWorkspaceTabFromLayoutName(wsObj.layout_name);
        wasmApi.dispatchAction({ action: "import_workspace_json", json_text: saved });
        applyDockRatiosCss(wsObj);
        persistWorkspaceToStorage();
        els.statusDot.classList.remove("is-error");
        els.statusMessage.textContent = "Restored saved custom workspace layout";
      } catch (_) {}
    }

    document.querySelectorAll("[data-menu-cmd]").forEach((btn) => {
      btn.addEventListener("click", async () => {
        const cmd = btn.dataset.menuCmd;
        closeAllDropdownMenus();
        if (!wasmApi) return;

        if (cmd === "new_showcase") {
          activeProjectRecordId = null;
          wasmApi.dispatchAction({ action: "new_starter_project" });
        } else if (cmd === "new_cube") {
          activeProjectRecordId = null;
          wasmApi.dispatchAction({
            action: "create_new_project",
            template: "minimal_cube",
            project_name: "General Cube Scene",
          });
        } else if (cmd === "new_empty") {
          activeProjectRecordId = null;
          wasmApi.dispatchAction({
            action: "create_new_project",
            template: "empty_stage",
            project_name: "Empty Stage",
          });
        } else if (cmd === "open_file") {
          els.fileOpenInput.value = "";
          els.fileOpenInput.click();
        } else if (cmd === "save_fe3d") {
          exportPortableFe3d();
        } else if (cmd === "export_bundle") {
          exportBinaryBundle();
        } else if (cmd === "go_home") {
          await snapshotActiveProjectToRecent("Active Session");
          setAppView("home");
        } else if (cmd === "undo") {
          wasmApi.dispatchAction({ action: "undo" });
        } else if (cmd === "redo") {
          wasmApi.dispatchAction({ action: "redo" });
        } else if (cmd === "duplicate") {
          wasmApi.dispatchAction({ action: "duplicate_selected" });
        } else if (cmd === "delete_obj") {
          wasmApi.dispatchAction({ action: "delete_selected" });
        } else if (cmd === "deselect") {
          wasmApi.dispatchAction({ action: "select_object", id: 0 });
        } else if (cmd === "preferences") {
          els.settingsModal.hidden = false;
        } else if (cmd === "frame_selection") {
          wasmApi.dispatchAction({ action: "frame_selection" });
        } else if (cmd === "toggle_tool_shelf" || cmd === "toggle_toolshelf") {
          els.toolShelf.hidden = !els.toolShelf.hidden;
          els.btnVpRestoreShelf.hidden = !els.toolShelf.hidden;
          if (currentState) renderWorkspaceAndBottomDock(currentState);
        } else if (cmd === "toggle_right_dock") {
          toggleRightDockVisibility();
        } else if (cmd === "toggle_bottom_dock") {
          setBottomDockCollapsed(!els.dockBottom.classList.contains("is-collapsed"));
        } else if (cmd === "toggle_hud") {
          appPrefs.showViewportHud = !appPrefs.showViewportHud;
          REG.savePreferences(appPrefs);
          applyPreferencesAndResponsiveMode();
        } else if (cmd === "toggle_camera_view") {
          wasmApi.dispatchAction({ action: "toggle_camera_view" });
        } else if (cmd === "toggle_editor_helpers") {
          wasmApi.dispatchAction({ action: "toggle_editor_helpers" });
        } else if (cmd === "toggle_editor_grid") {
          wasmApi.dispatchAction({ action: "toggle_editor_grid" });
        } else if (cmd === "save_custom_layout") {
          saveNamedCustomWorkspace();
        } else if (cmd === "restore_custom_layout") {
          restoreNamedCustomWorkspace();
        } else if (cmd === "reset_layout") {
          isRightDockCollapsed = false;
          setBottomDockCollapsed(false);
          document.documentElement.style.removeProperty("--right-dock-w");
          document.documentElement.style.removeProperty("--bottom-dock-h");
          document.documentElement.style.removeProperty("--outliner-h-pct");
          activateWorkspaceById(activeWorkspaceId, true);
        }
      });
    });

    els.btnOpen.addEventListener("click", () => {
      els.fileOpenInput.value = "";
      els.fileOpenInput.click();
    });

    els.fileOpenInput.addEventListener("change", () => {
      importSelectedFiles(els.fileOpenInput.files);
    });

    els.btnExportFe3d.addEventListener("click", exportPortableFe3d);
    els.btnExportBundle.addEventListener("click", exportBinaryBundle);

    els.btnUndo.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "undo" });
    });

    els.btnRedo.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "redo" });
    });

    [els.btnOpenSettings, els.btnStudioSettings].forEach((b) => {
      if (b) {
        b.addEventListener("click", () => {
          applyPreferencesAndResponsiveMode();
          els.settingsModal.hidden = false;
        });
      }
    });

    els.btnCloseSettings.addEventListener("click", () => {
      els.settingsModal.hidden = true;
    });

    els.prefLayoutMode.addEventListener("change", () => {
      appPrefs.layoutMode = els.prefLayoutMode.value;
      REG.savePreferences(appPrefs);
      applyPreferencesAndResponsiveMode();
    });

    els.prefUiDensity.addEventListener("change", () => {
      appPrefs.uiDensity = els.prefUiDensity.value;
      REG.savePreferences(appPrefs);
      applyPreferencesAndResponsiveMode();
    });

    els.prefDefaultAuthor.addEventListener("change", () => {
      appPrefs.defaultAuthor = els.prefDefaultAuthor.value.trim() || "Final Effect 3D";
      REG.savePreferences(appPrefs);
    });

    els.prefShowHud.addEventListener("change", () => {
      appPrefs.showViewportHud = els.prefShowHud.checked;
      REG.savePreferences(appPrefs);
      applyPreferencesAndResponsiveMode();
    });

    els.btnPrefSaveLayout.addEventListener("click", saveNamedCustomWorkspace);
    els.btnPrefRestoreLayout.addEventListener("click", restoreNamedCustomWorkspace);

    document.querySelectorAll("[data-edit-mode]").forEach((btn) => {
      btn.addEventListener("click", () => {
        if (!wasmApi) return;
        wasmApi.dispatchAction({ action: "set_editor_mode", mode: btn.dataset.editMode });
      });
    });

    document.querySelectorAll("[data-gizmo-mode]").forEach((btn) => {
      btn.addEventListener("click", () => {
        if (!wasmApi) return;
        wasmApi.dispatchAction({ action: "set_gizmo_mode", mode: btn.dataset.gizmoMode });
      });
    });

    if (els.btnToolSelect) {
      els.btnToolSelect.addEventListener("click", () => {
        canvas.focus();
      });
    }

    els.btnGizmoSpace.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.editor) return;
      const nextSpace = currentState.editor.gizmo_space === "local" ? "world" : "local";
      wasmApi.dispatchAction({ action: "set_gizmo_space", space: nextSpace });
    });

    els.btnCollapseShelf.addEventListener("click", () => {
      els.toolShelf.hidden = true;
      els.btnVpRestoreShelf.hidden = false;
      if (currentState) renderWorkspaceAndBottomDock(currentState);
    });
    els.btnVpRestoreShelf.addEventListener("click", () => {
      els.toolShelf.hidden = false;
      els.btnVpRestoreShelf.hidden = true;
      if (currentState) renderWorkspaceAndBottomDock(currentState);
    });

    if (els.btnCollapseRight) {
      els.btnCollapseRight.addEventListener("click", () => {
        isRightDockCollapsed = true;
        if (currentState) renderWorkspaceAndBottomDock(currentState);
      });
    }
    if (els.btnVpRestoreRight) {
      els.btnVpRestoreRight.addEventListener("click", () => {
        isRightDockCollapsed = false;
        if (currentState) renderWorkspaceAndBottomDock(currentState);
      });
    }

    if (els.btnLandscapeInspectorToggle) {
      els.btnLandscapeInspectorToggle.addEventListener("click", () => {
        const closed = document.body.classList.toggle("is-landscape-drawer-closed");
        els.btnLandscapeInspectorToggle.classList.toggle("is-active", !closed);
        if (currentState) renderWorkspaceAndBottomDock(currentState);
      });
    }

    if (els.workspaceTabs) {
      els.workspaceTabs.addEventListener("scroll", updateWorkspaceTabsOverflowUi, { passive: true });
      els.workspaceTabs.addEventListener(
        "wheel",
        (ev) => {
          if (Math.abs(ev.deltaY) > Math.abs(ev.deltaX) && els.workspaceTabs.scrollWidth > els.workspaceTabs.clientWidth) {
            ev.preventDefault();
            els.workspaceTabs.scrollLeft += ev.deltaY;
          }
        },
        { passive: false }
      );
    }
    if (els.btnWsScrollLeft) {
      els.btnWsScrollLeft.addEventListener("click", () => {
        if (els.workspaceTabs) {
          els.workspaceTabs.scrollBy({ left: -180, behavior: "smooth" });
        }
      });
    }
    if (els.btnWsScrollRight) {
      els.btnWsScrollRight.addEventListener("click", () => {
        if (els.workspaceTabs) {
          els.workspaceTabs.scrollBy({ left: 180, behavior: "smooth" });
        }
      });
    }
    if (els.topbarSubbar) {
      els.topbarSubbar.addEventListener(
        "wheel",
        (ev) => {
          if (Math.abs(ev.deltaY) > Math.abs(ev.deltaX) && els.topbarSubbar.scrollWidth > els.topbarSubbar.clientWidth) {
            ev.preventDefault();
            els.topbarSubbar.scrollLeft += ev.deltaY;
          }
        },
        { passive: false }
      );
    }

    // Stationary Right-Click inside 3D Viewport opens clamped context menu (without interfering with RMB drag orbit)
    let rmbDownPos = null;
    canvas.addEventListener("pointerdown", (ev) => {
      closeViewportContextMenu();
      closeAllDropdownMenus();
      if (ev.pointerType === "mouse" && ev.button === 2) {
        rmbDownPos = { x: ev.clientX, y: ev.clientY };
      } else {
        rmbDownPos = null;
      }
    });
    canvas.addEventListener("pointerup", (ev) => {
      if (ev.pointerType === "mouse" && ev.button === 2 && rmbDownPos) {
        const dist = Math.hypot(ev.clientX - rmbDownPos.x, ev.clientY - rmbDownPos.y);
        rmbDownPos = null;
        if (dist < 5) {
          openViewportContextMenu(ev.clientX, ev.clientY);
        }
      }
    });

    document.querySelectorAll("[data-ctx-cmd]").forEach((btn) => {
      btn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        const cmd = btn.dataset.ctxCmd;
        closeViewportContextMenu();
        if (!wasmApi) return;
        if (cmd === "frame") {
          wasmApi.dispatchAction({ action: "frame_selection" });
        } else if (cmd === "duplicate") {
          wasmApi.dispatchAction({ action: "duplicate_selected" });
        } else if (cmd === "reset_transform") {
          if (currentState && currentState.inspector) {
            wasmApi.dispatchAction({
              action: "set_transform",
              id: currentState.inspector.id,
              position: [0, 0, 0],
              rotation_deg: [0, 0, 0],
              scale: [1, 1, 1],
            });
          }
        } else if (cmd === "extrude") {
          const dist = Number(els.meshExtrudeDist ? els.meshExtrudeDist.value : 0.35);
          wasmApi.dispatchAction({
            action: "extrude_selected_faces",
            distance: Number.isFinite(dist) ? dist : 0.35,
          });
        } else if (cmd === "select_boundary") {
          wasmApi.dispatchAction({ action: "select_boundary_loop" });
        } else if (cmd === "fill_hole") {
          wasmApi.dispatchAction({ action: "fill_selected_hole" });
        } else if (cmd === "delete") {
          const ed = currentState && currentState.editor ? currentState.editor : null;
          if (ed && ed.mode !== "object") {
            wasmApi.dispatchAction({ action: "delete_selected_elements" });
          } else {
            wasmApi.dispatchAction({ action: "delete_selected" });
          }
        } else if (cmd === "deselect") {
          const ed = currentState && currentState.editor ? currentState.editor : null;
          if (ed && ed.mode !== "object") {
            wasmApi.dispatchAction({ action: "clear_mesh_selection" });
          } else {
            wasmApi.dispatchAction({ action: "select_object", id: 0 });
          }
        }
      });
    });

    document.querySelectorAll("[data-touch-nav]").forEach((btn) => {
      btn.addEventListener("click", () => {
        touchNavMode = btn.dataset.touchNav;
        document.querySelectorAll("[data-touch-nav]").forEach((b) => {
          b.classList.toggle("is-active", b.dataset.touchNav === touchNavMode);
        });
      });
    });

    if (els.btnTouchMultiSelect) {
      els.btnTouchMultiSelect.addEventListener("click", () => {
        touchMultiSelect = !touchMultiSelect;
        els.btnTouchMultiSelect.classList.toggle("is-active", touchMultiSelect);
      });
    }

    function activatePortraitDrawer(drawerId) {
      activeMobileDrawer = drawerId;
      document.body.dataset.mpDrawer = activeMobileDrawer;
      document.querySelectorAll("[data-mp-drawer]").forEach((b) => {
        b.classList.toggle("is-active", b.dataset.mpDrawer === activeMobileDrawer);
      });
      if (activeMobileDrawer === "inspector") {
        setInspectorCategory("all");
      } else if (activeMobileDrawer === "materials") {
        setInspectorCategory("material");
      } else if (activeMobileDrawer === "assets") {
        activeBottomEditor = "asset_browser";
        if (currentState) renderWorkspaceAndBottomDock(currentState);
      } else if (activeMobileDrawer === "timeline") {
        activeBottomEditor = "timeline";
        if (currentState) renderWorkspaceAndBottomDock(currentState);
      } else if (activeMobileDrawer === "console") {
        activeBottomEditor = "console";
        if (currentState) renderWorkspaceAndBottomDock(currentState);
      }
    }

    document.querySelectorAll("[data-mp-drawer]").forEach((btn) => {
      btn.addEventListener("click", () => {
        activatePortraitDrawer(btn.dataset.mpDrawer);
      });
    });

    document.querySelectorAll("[data-mp-close-drawer]").forEach((btn) => {
      btn.addEventListener("click", () => {
        activatePortraitDrawer("viewport");
      });
    });

    els.workspacePresetSelect.addEventListener("change", () => {
      activateWorkspaceById(els.workspacePresetSelect.value, true);
    });

    document.querySelectorAll("[data-panel-toggle]").forEach((btn) => {
      btn.addEventListener("click", () => {
        closeAllDropdownMenus();
        if (!wasmApi || !currentState || !currentState.workspace) return;
        const key = btn.dataset.panelToggle;
        const p = currentState.workspace.panels.find((item) => item.key === key);
        const nextVis = p ? !p.visible : true;
        if (nextVis && (key === "scene_outliner" || key === "properties" || key === "material_editor")) {
          isRightDockCollapsed = false;
        }
        wasmApi.dispatchAction({ action: "set_panel_visible", panel: key, visible: nextVis });
        persistWorkspaceToStorage();
      });
    });

    els.bottomEditorSelect.addEventListener("change", () => {
      activeBottomEditor = els.bottomEditorSelect.value;
      setBottomDockCollapsed(false);
    });

    document.querySelectorAll("[data-bottom-tab]").forEach((tab) => {
      tab.addEventListener("click", () => {
        activeBottomEditor = tab.dataset.bottomTab;
        setBottomDockCollapsed(false);
      });
    });

    els.btnCollapseBottom.addEventListener("click", () => {
      setBottomDockCollapsed(!els.dockBottom.classList.contains("is-collapsed"));
    });

    els.btnTimelinePlay.addEventListener("click", () => {
      if (!wasmApi || !currentState) return;
      wasmApi.dispatchAction({
        action: "set_rig_playing",
        playing: !currentState.viewport.rig_playing,
      });
    });

    if (els.timelineScrubber && els.timelineFrameNum) {
      els.timelineScrubber.addEventListener("input", () => {
        els.timelineFrameNum.value = els.timelineScrubber.value;
      });
      els.timelineFrameNum.addEventListener("change", () => {
        const f = Math.max(1, Math.min(240, Number(els.timelineFrameNum.value) || 1));
        els.timelineFrameNum.value = String(f);
        els.timelineScrubber.value = String(f);
      });
    }

    document.querySelectorAll("[data-insp-tab]").forEach((btn) => {
      btn.addEventListener("click", () => {
        setInspectorCategory(btn.dataset.inspTab);
      });
    });

    if (els.outlinerFilter) {
      els.outlinerFilter.addEventListener("input", () => {
        outlinerFilterQuery = els.outlinerFilter.value || "";
        if (currentState) renderOutliner(currentState);
      });
    }

    document.querySelectorAll("[data-preset]").forEach((btn) => {
      btn.addEventListener("click", () => {
        closeAllDropdownMenus();
        if (!wasmApi) return;
        wasmApi.dispatchAction({ action: "add_preset", preset: btn.dataset.preset });
      });
    });

    els.btnDuplicate.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "duplicate_selected" });
    });

    els.btnDelete.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "delete_selected" });
    });

    els.btnFrame.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "frame_selection" });
    });

    els.btnToggleRig.addEventListener("click", () => {
      if (!wasmApi || !currentState) return;
      wasmApi.dispatchAction({
        action: "set_rig_playing",
        playing: !currentState.viewport.rig_playing,
      });
    });

    els.btnClearSelection.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "select_object", id: 0 });
    });

    function triggerExtrude() {
      if (!wasmApi) return;
      const dist = Number(els.meshExtrudeDist.value);
      wasmApi.dispatchAction({
        action: "extrude_selected_faces",
        distance: Number.isFinite(dist) ? dist : 0.35,
      });
    }

    els.btnMeshExtrude.addEventListener("click", triggerExtrude);
    els.btnMeshBoundary.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "select_boundary_loop" });
    });
    els.btnMeshFill.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "fill_selected_face" });
    });
    els.btnMeshAssignMat.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.active_material_id) return;
      wasmApi.dispatchAction({
        action: "assign_face_material",
        material_id: currentState.active_material_id,
      });
    });
    els.btnMeshDelete.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "delete_mesh_selection" });
    });

    document.querySelectorAll("[data-mesh-quick]").forEach((btn) => {
      btn.addEventListener("click", () => {
        const op = btn.dataset.meshQuick;
        if (op === "extrude") triggerExtrude();
        else if (op === "boundary" && wasmApi)
          wasmApi.dispatchAction({ action: "select_boundary_loop" });
        else if (op === "fill" && wasmApi)
          wasmApi.dispatchAction({ action: "fill_selected_face" });
        else if (op === "delete" && wasmApi)
          wasmApi.dispatchAction({ action: "delete_mesh_selection" });
      });
    });

    els.matSelect.addEventListener("change", () => {
      if (!wasmApi) return;
      const mid = Number(els.matSelect.value) || 0;
      if (mid > 0) wasmApi.dispatchAction({ action: "select_material", id: mid });
    });

    els.btnMatNew.addEventListener("click", () => {
      if (wasmApi) wasmApi.dispatchAction({ action: "create_material", name: "PBR Material" });
    });

    els.btnMatDup.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.active_material_id) return;
      wasmApi.dispatchAction({ action: "duplicate_material", id: currentState.active_material_id });
    });

    els.btnMatDel.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.active_material_id) return;
      wasmApi.dispatchAction({ action: "delete_material", id: currentState.active_material_id });
    });

    els.matName.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.active_material_id) return;
      wasmApi.dispatchAction({
        action: "rename_material",
        id: currentState.active_material_id,
        name: els.matName.value.trim(),
      });
    });

    els.btnMatAssignObj.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.inspector || !currentState.active_material_id)
        return;
      wasmApi.dispatchAction({
        action: "assign_object_material",
        object_id: currentState.inspector.id,
        material_id: currentState.active_material_id,
      });
    });

    [
      els.matBaseColor,
      els.matMetallic,
      els.matRoughness,
      els.matAo,
      els.matEmissiveColor,
      els.matEmissiveStrength,
      els.matOpacity,
    ].forEach((el) => {
      el.addEventListener("input", () => dispatchMaterialFromInputs(true));
      el.addEventListener("change", () => dispatchMaterialFromInputs(false));
    });

    document.querySelectorAll('input[type="text"], input[type="number"]').forEach((input) => {
      input.addEventListener("keydown", (ev) => {
        if (ev.key === "Enter") {
          ev.preventDefault();
          input.blur();
        }
      });
    });

    [els.metaProjectName, els.metaAuthor, els.metaSceneName].forEach((input) => {
      input.addEventListener("change", dispatchMetadataFromInputs);
    });

    els.inspName.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      wasmApi.dispatchAction({
        action: "rename_object",
        id: currentState.inspector.id,
        name: els.inspName.value.trim(),
      });
    });

    els.inspVisible.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      wasmApi.dispatchAction({
        action: "set_visibility",
        id: currentState.inspector.id,
        visible: els.inspVisible.checked,
      });
    });

    els.inspParent.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      const newParentId = Number(els.inspParent.value) || 0;
      const ok = wasmApi.dispatchAction({
        action: "reparent_object",
        child_id: currentState.inspector.id,
        parent_id: newParentId,
      });
      if (!ok && currentState.inspector) {
        els.inspParent.value = String(currentState.inspector.parent_id || 0);
      }
    });

    els.inspMaterial.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      const mid = Number(els.inspMaterial.value) || 0;
      wasmApi.dispatchAction({
        action: "assign_object_material",
        object_id: currentState.inspector.id,
        material_id: mid,
      });
    });

    els.btnResetTransform.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      wasmApi.dispatchAction({
        action: "reset_transform",
        id: currentState.inspector.id,
      });
    });

    [
      els.posX,
      els.posY,
      els.posZ,
      els.rotX,
      els.rotY,
      els.rotZ,
      els.sclX,
      els.sclY,
      els.sclZ,
    ].forEach((input) => {
      input.addEventListener("change", () => dispatchTransformFromInputs(false));
    });

    [els.camProjection, els.camFov, els.camOrtho, els.camNear, els.camFar].forEach((el) => {
      el.addEventListener("change", dispatchCameraFromInputs);
    });

    if (els.btnToggleCameraView) {
      els.btnToggleCameraView.addEventListener("click", () => {
        if (!wasmApi || !currentState || !currentState.inspector || !currentState.inspector.camera) {
          return;
        }
        const insp = currentState.inspector;
        const vp = currentState.viewport || {};
        if (insp.camera.is_active && vp.camera_view_active) {
          wasmApi.dispatchAction({ action: "set_camera_view", active: false });
        } else {
          if (!insp.camera.is_active) {
            wasmApi.dispatchAction({ action: "set_active_camera", id: insp.id });
          }
          wasmApi.dispatchAction({ action: "set_camera_view", active: true });
        }
      });
    }

    els.btnSetActiveCamera.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      wasmApi.dispatchAction({
        action: "set_active_camera",
        id: currentState.inspector.id,
      });
    });

    if (els.lightType) {
      els.lightType.addEventListener("change", () => {
        if (!wasmApi || !currentState || !currentState.inspector || !currentState.inspector.light) {
          return;
        }
        wasmApi.dispatchAction({
          action: "change_light_type",
          id: currentState.inspector.id,
          light_type: els.lightType.value,
        });
      });
    }

    [
      els.lightEnabled,
      els.lightAreaShape,
      els.lightAreaTwoSided,
      els.lightAreaSamples,
      els.lightFalloff,
      els.lightShadowEnabled,
      els.lightShadowQuality,
      els.lightShadowFilter,
    ].forEach((el) => {
      if (el) el.addEventListener("change", () => dispatchLightFromInputs(false));
    });

    [
      els.lightColor,
      els.lightIntensity,
      els.lightRange,
      els.lightAttenuation,
      els.lightSourceRadius,
      els.lightInnerCone,
      els.lightOuterCone,
      els.lightConeFalloff,
      els.lightAreaWidth,
      els.lightAreaHeight,
      els.lightShadowStrength,
      els.lightShadowBias,
      els.lightShadowNormalBias,
      els.lightShadowSoftness,
    ].forEach((el) => {
      if (!el) return;
      el.addEventListener("input", () => dispatchLightFromInputs(true));
      el.addEventListener("change", () => dispatchLightFromInputs(false));
    });

    if (els.lightLinkMode) {
      els.lightLinkMode.addEventListener("change", () => dispatchLightLinkingFromInputs());
    }
    if (els.lightLinkDescendants) {
      els.lightLinkDescendants.addEventListener("change", () => dispatchLightLinkingFromInputs());
    }

    if (els.envEnabled) {
      els.envEnabled.addEventListener("change", () => dispatchEnvironmentFromInputs(false));
    }
    [els.envIntensity, els.envRotation, els.envDiffuseScale, els.envSpecularScale].forEach((el) => {
      if (!el) return;
      el.addEventListener("input", () => dispatchEnvironmentFromInputs(true));
      el.addEventListener("change", () => dispatchEnvironmentFromInputs(false));
    });
    if (els.btnLoadHdri && els.fileOpenHdri) {
      els.btnLoadHdri.addEventListener("click", () => {
        els.fileOpenHdri.value = "";
        els.fileOpenHdri.click();
      });
      els.fileOpenHdri.addEventListener("change", async () => {
        if (els.fileOpenHdri.files && els.fileOpenHdri.files.length > 0) {
          await importHdriFile(els.fileOpenHdri.files[0]);
        }
      });
    }
    if (els.btnClearHdri) {
      els.btnClearHdri.addEventListener("click", () => {
        if (wasmApi) wasmApi.dispatchAction({ action: "clear_hdri" });
      });
    }
    document.querySelectorAll("[data-hdri-preset]").forEach((btn) => {
      btn.addEventListener("click", () => {
        if (wasmApi) {
          wasmApi.dispatchAction({
            action: "load_hdri_preset",
            preset: btn.dataset.hdriPreset,
          });
        }
      });
    });

    window.addEventListener("mouseup", () => setTimeout(() => syncFromEngine(false), 20));
    window.addEventListener("keyup", () => setTimeout(() => syncFromEngine(false), 20));

    if (els.viewportStage) {
      els.viewportStage.addEventListener("dragover", (ev) => ev.preventDefault());
      els.viewportStage.addEventListener("drop", (ev) => {
        ev.preventDefault();
        if (ev.dataTransfer && ev.dataTransfer.files) {
          importSelectedFiles(ev.dataTransfer.files);
        }
      });
    }

    bindViewportTouchAndPointer();
    bindViewNavGizmo();
    bindDockSplitters();
  }

  bindUiEvents();

  const bootTimer = setInterval(() => {
    if (tryBindWasmApi()) {
      clearInterval(bootTimer);
      setInterval(() => syncFromEngine(false), 60);
    }
  }, 35);
})();
