// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Maya-Classic 3D Studio Controller (Stage 1 UI Redesign, v0.5.0)
// Drives the real C++20 WebAssembly StudioSession / EditorController JSON bridge.
// All scene mutation flows through fe3d_dispatch_action JSON commands. No scene data is
// duplicated in JavaScript: every panel renders strictly from the engine state snapshot.

(function () {
  "use strict";

  const REG = window.FE3D_REGISTRY;
  const WORKSPACE_STORAGE_KEY = "fe3d.workspace.v2";

  const $ = (id) => document.getElementById(id);

  const canvas = $("fe3d-canvas");
  const banner = $("engine-banner");
  const previewCanvas = $("material-preview-canvas");
  const previewCtx = previewCanvas ? previewCanvas.getContext("2d") : null;

  function hydrateStaticIcons(root) {
    if (!REG || !REG.ICONS) return;
    const scope = root || document;
    const splashLogo = $("splash-logo-slot");
    if (splashLogo && !splashLogo.innerHTML) splashLogo.innerHTML = REG.ICONS.logo;
    scope.querySelectorAll("[data-icon]").forEach((el) => {
      const key = el.getAttribute("data-icon");
      if (REG.ICONS[key]) el.innerHTML = REG.ICONS[key];
    });
  }
  hydrateStaticIcons(document);

  // =========================================================================
  // SPLASH SCREEN TELEMETRY
  // =========================================================================
  const splashEls = {
    statusText: $("splash-status-text"),
    pct: $("splash-pct"),
    progressbar: $("splash-progressbar"),
    barFill: $("splash-bar-fill"),
    milestones: $("splash-milestones"),
    spinner: $("splash-spinner"),
    errorBox: $("splash-error-box"),
    errorDetail: $("splash-error-detail"),
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
    if (splashEls.statusText) splashEls.statusText.textContent = "Initialization halted.";
  }

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
        setSplashProgress(1, 8 + (loaded / total) * 24, text);
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

  // =========================================================================
  // DOM ELEMENT REGISTRY
  // =========================================================================
  const els = {
    appSplash: $("app-splash"),
    appHome: $("app-home"),
    appStudio: $("studio-shell"),

    homeEngineBadge: $("home-engine-badge"),
    btnHomeResumeStudio: $("btn-home-resume-studio"),
    btnOpenSettings: $("btn-open-settings"),
    studioSelectorList: $("studio-selector-list"),

    studioLauncherPanel: $("studio-launcher-panel"),
    launcherStudioBadge: $("launcher-studio-badge"),
    launcherStudioTitle: $("launcher-studio-title"),
    launcherStudioDesc: $("launcher-studio-desc"),
    launcherErrorBanner: $("launcher-error-banner"),
    launcherProjectName: $("launcher-project-name"),
    launcherProjectAuthor: $("launcher-project-author"),
    launcherTemplateGrid: $("launcher-template-grid"),
    btnLauncherCreate: $("btn-launcher-create"),
    btnLauncherOpen: $("btn-launcher-open"),
    recentCountTag: $("recent-count-tag"),
    recentProjectsList: $("recent-projects-list"),

    studioFuturePanel: $("studio-future-panel"),
    futureStudioBadge: $("future-studio-badge"),
    futureStudioTitle: $("future-studio-title"),
    futureStudioTagline: $("future-studio-tagline"),
    futureStudioDesc: $("future-studio-desc"),
    futureStudioCaps: $("future-studio-caps"),
    btnFutureBack3d: $("btn-future-back-3d"),
    btnFutureJumpShading: $("btn-future-jump-shading"),

    btnGoHome: $("btn-go-home"),
    mayaMenuSetSelect: $("maya-menuset-select"),
    mayaDynamicMenus: $("maya-dynamic-menus"),
    workspaceTabs: $("workspace-tabs"),
    btnWsScrollLeft: $("btn-ws-scroll-left"),
    btnWsScrollRight: $("btn-ws-scroll-right"),
    workspacePresetSelect: $("workspace-preset-select"),
    activeProjectPill: $("active-project-pill"),
    engineBadge: $("engine-badge"),

    fileOpenInput: $("file-open-input"),
    btnOpen: $("btn-open"),
    btnExportFe3d: $("btn-export-fe3d"),
    btnExportBundle: $("btn-export-bundle"),
    btnUndo: $("btn-undo"),
    btnRedo: $("btn-redo"),
    btnStudioSettings: $("btn-studio-settings"),
    btnDuplicate: $("btn-duplicate"),
    btnDelete: $("btn-delete"),
    btnFrame: $("btn-frame"),
    btnToggleRig: $("btn-toggle-rig"),
    btnClearSelection: $("btn-clear-selection"),
    btnRenderFrame: $("btn-render-frame"),

    btnModeObject: $("btn-mode-object"),
    btnModeVertex: $("btn-mode-vertex"),
    btnModeEdge: $("btn-mode-edge"),
    btnModeFace: $("btn-mode-face"),
    btnModeUv: $("btn-mode-uv"),
    ctxMeshSubbar: $("ctx-mesh-subbar"),
    topbarSubbar: $("topbar-subbar"),
    mayaHelpText: $("maya-help-text"),
    btnLandscapeInspectorToggle: $("btn-landscape-inspector-toggle"),
    commandSearchInput: $("status-command-search"),
    commandSearchResults: $("command-search-results"),

    maskMeshes: $("mask-meshes"),
    maskLights: $("mask-lights"),
    maskCameras: $("mask-cameras"),
    maskHelpers: $("mask-helpers"),
    btnStatusTemplate: $("btn-status-template"),
    btnStatusReference: $("btn-status-reference"),
    btnStatusCb: $("btn-status-cb"),
    btnStatusAe: $("btn-status-ae"),
    btnToggleToolSettings: $("btn-toggle-tool-settings"),

    mayaShelfBar: $("maya-shelf-bar"),
    mayaShelfTabs: $("maya-shelf-tabs"),
    mayaShelfItems: $("maya-shelf-items"),
    btnShelfGear: $("btn-shelf-gear"),
    shelfGearMenu: $("shelf-gear-menu"),
    btnShelfAddItem: $("btn-shelf-add-item"),
    shelfItemCtxMenu: $("shelf-item-ctx-menu"),

    toolShelf: $("tool-shelf"),
    btnToolSelect: $("btn-tool-select"),
    btnToolComponent: $("btn-tool-component"),
    btnToolMulticomp: $("btn-tool-multicomp"),
    btnToolUniversal: $("btn-tool-universal"),
    btnToolShowManip: $("btn-tool-show-manip"),
    btnToolPivot: $("btn-tool-pivot"),
    btnToolLast: $("btn-tool-last"),
    btnGizmoTranslate: $("btn-gizmo-translate"),
    btnGizmoRotate: $("btn-gizmo-rotate"),
    btnGizmoScale: $("btn-gizmo-scale"),
    btnGizmoSpace: $("btn-gizmo-space"),
    lblGizmoSpace: $("lbl-gizmo-space"),
    btnCollapseShelf: $("btn-collapse-shelf"),
    btnVpRestoreShelf: $("btn-vp-restore-shelf"),
    btnVpRestoreRight: $("btn-vp-restore-right"),

    dockLeft: $("dock-left"),
    splitterLeft: $("splitter-left"),
    panelToolSettings: $("panel-tool-settings"),
    toolSettingsTitle: $("tool-settings-title"),
    tsCoordSpace: $("ts-coord-space"),
    tsSnapStep: $("ts-snap-step"),
    btnTsCenterPivot: $("btn-ts-center-pivot"),
    btnTsResetPivot: $("btn-ts-reset-pivot"),
    btnTsFreeze: $("btn-ts-freeze"),
    btnTsResetTool: $("btn-ts-reset-tool"),

    studioWorkspace: $("studio-workspace"),
    centerColumn: $("center-column"),
    viewportStage: $("viewport-stage"),
    wfBanner: $("workspace-foundation-banner"),
    wfBannerTitle: $("wf-banner-title"),
    wfBannerDetail: $("wf-banner-detail"),
    viewportHudTop: $("viewport-hud-top"),
    viewportHudBottom: $("viewport-hud-bottom"),
    vpCameraPill: $("vp-camera-pill"),
    vpSelectionPill: $("vp-selection-pill"),
    vpModePill: $("vp-mode-pill"),
    vpPanelCamSelect: $("vp-panel-cam-select"),
    viewportViewGizmo: $("viewport-view-gizmo"),
    viewNavGizmoCanvas: $("view-nav-gizmo-canvas"),
    viewGizmoHoverTip: $("view-gizmo-hover-tip"),
    btnNavHomeView: $("btn-nav-home-view"),
    btnNavProjToggle: $("btn-nav-proj-toggle"),
    btnTouchMultiSelect: $("btn-touch-multiselect"),
    viewportResGate: $("viewport-res-gate"),
    btnVpResGate: $("btn-vp-gate"),
    mayaHotbox: $("maya-hotbox"),

    btnVpWireframe: $("btn-vp-wireframe"),
    btnVpShaded: $("btn-vp-shaded"),
    btnVpTextured: $("btn-vp-textured"),
    btnVpWos: $("btn-vp-wos"),
    btnVpLights: $("btn-vp-lights"),
    btnVpXray: $("btn-vp-xray"),
    btnVpIsolate: $("btn-vp-isolate"),
    btnVpGrid: $("btn-vp-grid"),

    viewportContextMenu: $("viewport-context-menu"),
    ctxMenuTitle: $("ctx-menu-title"),
    ctxCmdDuplicate: $("ctx-cmd-duplicate"),
    ctxCmdResetTf: $("ctx-cmd-reset-tf"),
    ctxMeshSep: $("ctx-mesh-sep"),
    ctxCmdExtrude: $("ctx-cmd-extrude"),
    ctxCmdBoundary: $("ctx-cmd-boundary"),
    ctxCmdFill: $("ctx-cmd-fill"),
    ctxCmdDelete: $("ctx-cmd-delete"),

    splitterBottom: $("splitter-bottom"),
    splitterRight: $("splitter-right"),
    splitterOutliner: $("splitter-outliner"),
    splitterRightStack: $("splitter-right-stack"),

    dockBottom: $("dock-bottom"),
    bottomEditorSelect: $("bottom-editor-select"),
    tabTimeline: $("tab-timeline"),
    tabAssetBrowser: $("tab-asset-browser"),
    tabConsole: $("tab-console"),
    btnCollapseBottom: $("btn-collapse-bottom"),
    paneTimeline: $("pane-timeline"),
    paneAssetBrowser: $("pane-asset-browser"),
    paneConsole: $("pane-console"),
    paneFoundationEditor: $("pane-foundation-editor"),
    fePaneTitle: $("fe-pane-title"),
    fePaneDesc: $("fe-pane-desc"),
    fePaneBinding: $("fe-pane-binding"),
    assetBrowserList: $("asset-browser-list"),
    consoleLogList: $("console-log-list"),

    tlScrubber: $("timeline-scrubber"),
    tlFrameNum: $("timeline-frame-num"),
    tlKeyTicks: $("maya-keyframe-ticks"),
    tlSceneStart: $("tl-scene-start"),
    tlPlayStart: $("tl-play-start"),
    tlPlayEnd: $("tl-play-end"),
    tlSceneEnd: $("tl-scene-end"),
    tlRangeFill: $("tl-range-fill"),
    btnTlStart: $("btn-tl-start"),
    btnTlPrevKey: $("btn-tl-prev-key"),
    btnTlPrevFrame: $("btn-tl-prev-frame"),
    btnTimelinePlay: $("btn-timeline-play"),
    icTimelinePlay: $("ic-timeline-play"),
    lblTimelinePlay: $("lbl-timeline-play"),
    btnTlNextFrame: $("btn-tl-next-frame"),
    btnTlNextKey: $("btn-tl-next-key"),
    btnTlEnd: $("btn-tl-end"),
    btnTlSetKey: $("btn-tl-set-key"),
    btnTlDelKey: $("btn-tl-del-key"),
    btnTlAutokey: $("btn-tl-autokey"),
    tlFpsSelect: $("tl-fps-select"),
    tlLoopSelect: $("tl-loop-select"),

    dockRight: $("dock-right"),
    rightDockStack: $("cb-layer-stack"),
    panelOutliner: $("panel-outliner"),
    btnCollapseRight: $("btn-collapse-right"),
    outlinerFilter: $("outliner-filter"),
    outlinerList: $("outliner-list"),
    outlinerRootDrop: $("outliner-root-drop"),
    btnOutlinerGroup: $("btn-outliner-group"),
    btnOutlinerUnparent: $("btn-outliner-unparent"),

    cbNodeTitle: $("cb-node-title"),
    cbEmpty: $("cb-empty"),
    cbTable: $("cb-table"),
    cbTx: $("cb-tx"),
    cbTy: $("cb-ty"),
    cbTz: $("cb-tz"),
    cbRx: $("cb-rx"),
    cbRy: $("cb-ry"),
    cbRz: $("cb-rz"),
    cbSx: $("cb-sx"),
    cbSy: $("cb-sy"),
    cbSz: $("cb-sz"),
    cbVis: $("cb-vis"),
    cbShapes: $("cb-shapes-summary"),
    cbCustomList: $("cb-custom-list"),
    btnCbKey: $("btn-cb-key"),
    btnCbFreeze: $("btn-cb-freeze"),
    btnCbAddAttr: $("btn-cb-add-attr"),

    layerList: $("layer-editor-list"),
    btnLayerCreateSelected: $("btn-layer-create-selected"),
    btnLayerCreateEmpty: $("btn-layer-create-empty"),
    btnLayerAssign: $("btn-layer-assign"),
    btnLayerDelete: $("btn-layer-delete"),

    propertiesShell: $("properties-shell"),
    inspectorTabs: $("inspector-tabs"),
    aeNodeTabs: $("ae-node-tabs"),
    propsActiveHeading: $("props-active-heading"),
    panelProperties: $("panel-properties"),
    panelMaterialEditor: $("panel-material-editor"),
    panelEnvironment: $("card-environment"),

    inspectorIdBadge: $("inspector-id-badge"),
    inspectorEmpty: $("inspector-empty"),
    inspectorBody: $("inspector-body"),
    inspName: $("insp-name"),
    inspParent: $("insp-parent"),
    inspMaterialRow: $("insp-material-row"),
    inspMaterial: $("insp-material"),
    inspVisible: $("insp-visible"),
    btnResetTransform: $("btn-reset-transform"),

    posX: $("tf-pos-x"),
    posY: $("tf-pos-y"),
    posZ: $("tf-pos-z"),
    rotX: $("tf-rot-x"),
    rotY: $("tf-rot-y"),
    rotZ: $("tf-rot-z"),
    sclX: $("tf-scl-x"),
    sclY: $("tf-scl-y"),
    sclZ: $("tf-scl-z"),

    cardMeshEdit: $("card-mesh-edit"),
    meshEditSelectionTag: $("mesh-edit-selection-tag"),
    meshEditTopology: $("mesh-edit-topology"),
    meshExtrudeDist: $("mesh-extrude-dist"),
    btnMeshExtrude: $("btn-mesh-extrude"),
    btnMeshBoundary: $("btn-mesh-boundary"),
    btnMeshFill: $("btn-mesh-fill"),
    btnMeshAssignMat: $("btn-mesh-assign-mat"),
    btnMeshDelete: $("btn-mesh-delete"),

    cardMesh: $("card-mesh"),
    meshAssetName: $("mesh-asset-name"),
    meshGeoCounts: $("mesh-geo-counts"),
    meshBounds: $("mesh-bounds"),

    cardCamera: $("card-camera"),
    cardLight: $("card-light"),
    cardDataEmpty: $("card-data-empty"),
    btnToggleCameraView: $("btn-toggle-camera-view"),
    btnSetActiveCamera: $("btn-set-active-camera"),
    camProjection: $("cam-projection"),
    camFov: $("cam-fov"),
    camOrtho: $("cam-ortho"),
    camNear: $("cam-near"),
    camFar: $("cam-far"),
    lightCardTitle: $("light-card-title"),
    lightEnabled: $("light-enabled"),
    lightType: $("light-type"),
    lightColor: $("light-color"),
    lightIntensity: $("light-intensity"),
    lightShapeBlock: $("light-shape-block"),
    lightRowAreaShape: $("light-row-area-shape"),
    lightAreaShape: $("light-area-shape"),
    lightRowAreaWidth: $("light-row-area-width"),
    lblLightAreaWidth: $("lbl-light-area-width"),
    lightAreaWidth: $("light-area-width"),
    lightRowAreaHeight: $("light-row-area-height"),
    lightAreaHeight: $("light-area-height"),
    lightRowAreaTwoSided: $("light-row-area-twosided"),
    lightAreaTwoSided: $("light-area-two-sided") || $("light-area-twosided"),
    lightRowAreaSamples: $("light-row-area-samples"),
    lightAreaSamples: $("light-area-samples"),
    lightRowRange: $("light-row-range"),
    lightRange: $("light-range"),
    lightRowFalloff: $("light-row-falloff"),
    lightFalloff: $("light-falloff"),
    lightRowAttenuation: $("light-row-attenuation"),
    lightAttenuation: $("light-attenuation"),
    lightRowSourceRadius: $("light-row-source-radius"),
    lightSourceRadius: $("light-source-radius"),
    lightRowSpotCones: $("light-row-spot-cones"),
    lightInnerCone: $("light-inner-cone"),
    lightOuterCone: $("light-outer-cone"),
    lightRowConeFalloff: $("light-row-cone-falloff"),
    lightConeFalloff: $("light-cone-falloff"),
    lightShadowEnabled: $("light-shadow-enabled"),
    lightShadowQuality: $("light-shadow-quality"),
    lightShadowFilter: $("light-shadow-filter"),
    lightShadowStrength: $("light-shadow-strength"),
    lightShadowBias: $("light-shadow-bias"),
    lightShadowNormalBias: $("light-shadow-normal-bias"),
    lightShadowSoftness: $("light-shadow-softness"),
    lightLinkMode: $("light-link-mode"),
    lightLinkListWrap: $("light-link-list-wrap"),
    lightLinkDescendants: $("light-link-descendants"),
    lightLinkTargets: $("light-link-targets"),

    fileOpenHdri: $("file-open-hdri"),
    envEnabled: $("env-enabled"),
    envHdriStatus: $("env-hdri-status"),
    btnLoadHdri: $("btn-load-hdri"),
    btnClearHdri: $("btn-clear-hdri"),
    envIntensity: $("env-intensity"),
    valEnvIntensity: $("val-env-intensity"),
    envRotation: $("env-rotation"),
    valEnvRotation: $("val-env-rotation"),
    envDiffuseScale: $("env-diffuse-scale"),
    valEnvDiffuse: $("val-env-diffuse"),
    envSpecularScale: $("env-specular-scale"),
    valEnvSpecular: $("val-env-specular"),

    btnMatNew: $("btn-mat-new"),
    btnMatDup: $("btn-mat-dup"),
    btnMatDel: $("btn-mat-del"),
    matSelect: $("mat-select"),
    matName: $("mat-name"),
    btnMatAssignObj: $("btn-mat-assign-obj"),
    matBaseColor: $("mat-base-color"),
    matMetallic: $("mat-metallic"),
    valMatMetallic: $("val-mat-metallic"),
    matRoughness: $("mat-roughness"),
    valMatRoughness: $("val-mat-roughness"),
    matAo: $("mat-ao"),
    valMatAo: $("val-mat-ao"),
    matEmissiveColor: $("mat-emissive-color"),
    matEmissiveStrength: $("mat-emissive-strength"),
    matOpacity: $("mat-opacity"),
    valMatOpacity: $("val-mat-opacity"),

    metaProjectName: $("meta-project-name"),
    metaAuthor: $("meta-author"),
    metaSceneName: $("meta-scene-name"),

    statObjectsMeshes: $("stat-objects-meshes"),
    statMaterialsUpdates: $("stat-materials-updates"),
    statLightsShadows: $("stat-lights-shadows"),
    statTriangles: $("stat-triangles"),
    statMemory: $("stat-memory"),
    statLightingGpu: $("stat-lighting-gpu"),
    statLightingWarning: $("stat-lighting-warning"),
    statHistory: $("stat-history"),

    mobilePortraitNav: $("mobile-portrait-nav"),
    statusDot: $("status-dot"),
    statusMessage: $("status-message"),
    statusLayoutMode: $("status-layout-mode"),
    statusRenderer: $("status-renderer"),

    mayaCommandInput: $("maya-command-input"),
    btnCommandExec: $("btn-command-exec"),
    mayaCommandFeedback: $("maya-command-feedback"),
    btnOpenScriptLog: $("btn-open-script-log"),

    settingsModal: $("settings-modal"),
    btnCloseSettings: $("btn-close-settings"),
    prefLayoutMode: $("pref-layout-mode"),
    prefUiDensity: $("pref-ui-density"),
    prefDefaultAuthor: $("pref-default-author"),
    prefShowHud: $("pref-show-hud"),
    btnPrefSaveLayout: $("btn-pref-save-layout"),
    btnPrefRestoreLayout: $("btn-pref-restore-layout"),
    btnPrefOpenHotkeys: $("btn-pref-open-hotkeys"),

    hotkeyModal: $("hotkey-modal"),
    btnCloseHotkeys: $("btn-close-hotkeys"),
    hotkeyEditorList: $("hotkey-editor-list"),
    btnSaveHotkeys: $("btn-save-hotkeys"),
    btnResetHotkeys: $("btn-reset-hotkeys"),

    shelfModal: $("shelf-modal"),
    shelfModalTitle: $("shelf-modal-title"),
    shelfModalBody: $("shelf-modal-body"),
    btnCloseShelfModal: $("btn-close-shelf-modal"),

    floatingHost: $("floating-panel-host"),
  };

  // =========================================================================
  // RUNTIME STATE
  // =========================================================================
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
  let lastLayerSig = "";
  let lastCbSig = "";
  let lastAeNodeSig = "";
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
  let activeAeNodeId = "";
  let outlinerFilterQuery = "";
  let outlinerCollapsed = new Set();
  let isRightDockCollapsed = false;
  let isLeftDockCollapsed = false;
  let isToolSettingsCollapsed = false;
  let rightDockMode = "stacked";

  let touchNavMode = "orbit";
  let touchMultiSelect = false;

  let shelfData = REG.loadShelves();
  let activeShelfId = shelfData.length > 0 ? shelfData[0].id : "general";
  let shelfCtxTarget = null;
  let hotkeys = REG.loadHotkeys();
  let overriddenDomCodes = new Set();

  let hotboxOpen = false;
  let spaceHeld = false;

  const JS_HANDLED_ACTIONS = new Set([
    "duplicate_selected",
    "group_selected",
    "parent_selected",
    "unparent_selected",
    "save_fe3d",
    "timeline_toggle_play",
    "render_final_frame",
    "toggle_res_gate",
    "toggle_hotbox",
  ]);

  // =========================================================================
  // GENERIC HELPERS
  // =========================================================================
  function escapeHtml(str) {
    return String(str === undefined || str === null ? "" : str)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

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
      if (el.value !== strVal) el.value = strVal;
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

  function isTextEntryTarget(target) {
    if (!target) return false;
    const tag = (target.tagName || "").toUpperCase();
    return (
      tag === "INPUT" ||
      tag === "TEXTAREA" ||
      tag === "SELECT" ||
      target.isContentEditable === true
    );
  }

  function dispatch(payload) {
    if (!wasmApi) return false;
    return wasmApi.dispatchAction(payload);
  }

  function setStatusMessage(message, isError) {
    if (els.statusDot) els.statusDot.classList.toggle("is-error", Boolean(isError));
    if (els.statusMessage) els.statusMessage.textContent = message;
    if (els.mayaCommandFeedback && isError !== undefined) {
      els.mayaCommandFeedback.textContent = message;
      els.mayaCommandFeedback.classList.toggle("is-error", Boolean(isError));
    }
  }

  function setHelpLine(text) {
    if (els.mayaHelpText) els.mayaHelpText.textContent = text;
  }

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

  function closeShelfMenus() {
    if (els.shelfGearMenu) els.shelfGearMenu.hidden = true;
    if (els.shelfItemCtxMenu) els.shelfItemCtxMenu.hidden = true;
    const ctx = $("shelf-item-ctx-menu");
    if (ctx) ctx.hidden = true;
    if (els.btnShelfGear) els.btnShelfGear.classList.remove("is-on");
  }

  function closeCommandSearch() {
    if (els.commandSearchResults) els.commandSearchResults.hidden = true;
  }

  function closeViewportContextMenu() {
    if (els.viewportContextMenu) els.viewportContextMenu.hidden = true;
  }

  function closeHotbox() {
    hotboxOpen = false;
    if (els.mayaHotbox) els.mayaHotbox.hidden = true;
  }

  // =========================================================================
  // RESPONSIVE LAYOUT
  // =========================================================================
  function computeEffectiveLayoutMode() {
    if (appPrefs.layoutMode && appPrefs.layoutMode !== "auto") return appPrefs.layoutMode;
    const w = window.innerWidth || document.documentElement.clientWidth || 1280;
    const h = window.innerHeight || document.documentElement.clientHeight || 720;
    if (w >= 960) return "desktop";
    return w > h ? "landscape" : "portrait";
  }

  function relocateOutlinerForLayout(mode) {
    const outlinerSlot = $("slot-outliner-left");
    const responsiveSlot = $("slot-outliner-responsive");
    const panel = els.panelOutliner;
    if (!panel || !outlinerSlot || !responsiveSlot) return;
    const target = mode === "desktop" ? outlinerSlot : responsiveSlot;
    if (panel.parentElement !== target) target.appendChild(panel);
  }

  function applyPreferencesAndResponsiveMode() {
    const mode = computeEffectiveLayoutMode();
    document.body.dataset.layoutMode = mode;
    document.body.dataset.uiDensity = appPrefs.uiDensity || "comfortable";
    if (!document.body.dataset.mpDrawer) document.body.dataset.mpDrawer = activeMobileDrawer;

    closeViewportContextMenu();

    if (mode === "desktop") {
      const w = window.innerWidth || 1280;
      const h = window.innerHeight || 720;
      const inlineRight = parseFloat(document.documentElement.style.getPropertyValue("--right-dock-w"));
      if (Number.isFinite(inlineRight) && inlineRight > 0) {
        const maxRight = Math.max(230, Math.min(520, Math.floor(w * 0.42)));
        if (inlineRight > maxRight) {
          document.documentElement.style.setProperty("--right-dock-w", maxRight + "px");
        }
      }
      const inlineBottom = parseFloat(document.documentElement.style.getPropertyValue("--bottom-dock-h"));
      if (Number.isFinite(inlineBottom) && inlineBottom > 0) {
        const maxBottom = Math.max(70, Math.min(360, Math.floor((h - 90) * 0.46)));
        if (inlineBottom > maxBottom) {
          document.documentElement.style.setProperty("--bottom-dock-h", maxBottom + "px");
        }
      }
    }

    relocateOutlinerForLayout(mode);

    if (els.btnLandscapeInspectorToggle) {
      els.btnLandscapeInspectorToggle.classList.toggle(
        "is-active",
        mode === "landscape" && !document.body.classList.contains("is-landscape-drawer-closed")
      );
    }

    if (els.statusLayoutMode) {
      els.statusLayoutMode.textContent =
        mode === "desktop" ? "Desktop" : mode === "landscape" ? "Landscape Pro" : "Portrait Mobile";
    }

    const hudHidden = appPrefs.showViewportHud === false;
    if (els.viewportHudTop) els.viewportHudTop.hidden = hudHidden;
    if (els.viewportHudBottom) els.viewportHudBottom.hidden = hudHidden;

    if (els.prefLayoutMode) els.prefLayoutMode.value = appPrefs.layoutMode || "auto";
    if (els.prefUiDensity) els.prefUiDensity.value = appPrefs.uiDensity || "comfortable";
    if (els.prefDefaultAuthor) els.prefDefaultAuthor.value = appPrefs.defaultAuthor || "Final Effect 3D";
    if (els.prefShowHud) els.prefShowHud.checked = appPrefs.showViewportHud !== false;

    if (currentState) renderWorkspaceAndBottomDock(currentState);
    requestAnimationFrame(updateWorkspaceTabsOverflowUi);
  }

  window.addEventListener("resize", () => applyPreferencesAndResponsiveMode());
  window.addEventListener("orientationchange", () => setTimeout(applyPreferencesAndResponsiveMode, 50));
  applyPreferencesAndResponsiveMode();

  // =========================================================================
  // VIEW NAVIGATION (Splash -> Home -> Studio)
  // =========================================================================
  function setAppView(view) {
    document.body.dataset.appView = view;
    closeViewportContextMenu();
    closeHotbox();
    if (els.appSplash) els.appSplash.hidden = view !== "splash";
    if (els.appHome) els.appHome.hidden = view !== "home";
    if (view === "studio") {
      hasEnteredStudioOnce = true;
      if (els.btnHomeResumeStudio) els.btnHomeResumeStudio.hidden = false;
      syncFromEngine(true);
      requestAnimationFrame(updateWorkspaceTabsOverflowUi);
    } else if (view === "home") {
      renderRecentProjectsList();
      if (els.btnHomeResumeStudio) els.btnHomeResumeStudio.hidden = !hasEnteredStudioOnce;
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
  // HOME / STUDIO SELECTOR / PROJECT LAUNCHER
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
      if (els.btnFutureJumpShading) els.btnFutureJumpShading.hidden = !st.jumpTo3dWorkspace;
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
    if (els.recentCountTag) els.recentCountTag.textContent = recents.length + " saved";
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
      row.querySelector("[data-reopen-id]").addEventListener("click", () => reopenRecentProjectById(item));
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
  // WORKSPACE TABS
  // =========================================================================
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

  function renderWorkspaceTabBar() {
    if (!els.workspaceTabs) return;
    if (els.workspaceTabs.children.length !== REG.WORKSPACES.length) {
      els.workspaceTabs.innerHTML = "";
      const frag = document.createDocumentFragment();
      REG.WORKSPACES.forEach((ws) => {
        const btn = document.createElement("button");
        btn.type = "button";
        btn.className = "ws-tab" + (ws.foundationNotice ? "" : " is-operational");
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

  function applyMenuSet(menuSetId, dispatchEngine) {
    if (els.mayaMenuSetSelect && els.mayaMenuSetSelect.value !== menuSetId) {
      els.mayaMenuSetSelect.value = menuSetId;
    }
    renderDynamicMenuSet(menuSetId);
    if (dispatchEngine) dispatch({ action: "set_menu_set", menu_set: menuSetId });
  }

  function activateWorkspaceById(wsId, persistFlag) {
    const norm = wsId === "default" ? "layout" : wsId === "animation_prep" ? "animation" : wsId;
    const ws = REG.WORKSPACES.find((item) => item.id === norm) || REG.WORKSPACES[0];
    activeWorkspaceId = ws.id;
    if (els.workspacePresetSelect) els.workspacePresetSelect.value = ws.id;

    if (ws.menuSet) applyMenuSet(ws.menuSet, true);

    activeBottomEditor = ws.bottomEditor || "timeline";
    if (els.bottomEditorSelect) els.bottomEditorSelect.value = activeBottomEditor;
    if (ws.id === "layout") setInspectorCategory("all");
    else if (ws.inspectorTab) setInspectorCategory(ws.inspectorTab);

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
      if (els.btnVpRestoreShelf) els.btnVpRestoreShelf.hidden = !els.toolShelf.hidden;
    }

    renderWorkspaceTabBar();
    dispatch({ action: "apply_workspace_preset", preset: ws.preset });
    if (persistFlag) persistWorkspaceToStorage();
  }

  const INSP_HEADINGS = {
    object: "Transform Attributes",
    modifiers: "Shape & Component Modeling",
    material: "Material Shading",
    data: "Shape & Environment",
    render: "Project & Diagnostics",
    all: "Attribute Editor (All Tabs)",
  };

  function setInspectorCategory(cat) {
    activeInspTab = cat || "all";
    if (els.propsActiveHeading) {
      els.propsActiveHeading.textContent = INSP_HEADINGS[activeInspTab] || "Attribute Editor";
    }
    document.querySelectorAll("[data-insp-tab]").forEach((btn) => {
      btn.classList.toggle("is-active", btn.dataset.inspTab === activeInspTab);
    });
    document.querySelectorAll("[data-insp-section]").forEach((sec) => {
      const key = sec.dataset.inspSection;
      const visible = activeInspTab === "all" || key === activeInspTab;
      sec.hidden = !visible;
    });
    applyAeNodeFilter();
  }

  function applyAeNodeFilter() {
    const node = activeAeNodeId;
    const blocks = [
      { key: "transform", el: $("panel-properties") },
      { key: "material", el: els.panelMaterialEditor },
      { key: "environment", el: els.panelEnvironment },
      { key: "diagnostics", el: $("card-project-telemetry") },
    ];
    blocks.forEach((b) => {
      if (!b.el) return;
      if (!node) {
        b.el.hidden = false;
        return;
      }
      const keep =
        (node === "transform" && b.key === "transform") ||
        (node === "shape" && (b.key === "transform" || b.key === "environment")) ||
        (node === "material" && b.key === "material") ||
        (node === "environment" && b.key === "environment") ||
        (node === "diagnostics" && b.key === "diagnostics");
      b.el.hidden = !keep;
    });
  }

  // =========================================================================
  // MENU SETS (Dynamic Maya Workflow Menus)
  // =========================================================================
  const MENU_SET_DEFS = {
    modeling: [
      {
        slug: "mesh",
        label: "Mesh",
        items: [
          { sep: "Polygon Primitives" },
          { cmd: "preset_cube", label: "Create Polygon Cube" },
          { cmd: "preset_sphere", label: "Create Polygon Sphere" },
          { cmd: "preset_cylinder", label: "Create Polygon Cylinder" },
          { cmd: "preset_cone", label: "Create Polygon Cone" },
          { cmd: "preset_torus", label: "Create Polygon Torus" },
          { cmd: "preset_plane", label: "Create Polygon Plane" },
          { cmd: "preset_pyramid", label: "Create Polygon Pyramid" },
        ],
      },
      {
        slug: "edit_mesh",
        label: "Edit Mesh",
        items: [
          { cmd: "extrude_faces", label: "Extrude", kbd: "" },
          { cmd: "select_boundary", label: "Select Boundary Loop" },
          { cmd: "fill_hole", label: "Fill Hole" },
          { sep: "" },
          { cmd: "select_all_components", label: "Select All Components" },
          { cmd: "mode_vertex", label: "Vertex Mode", kbd: "F9" },
          { cmd: "mode_edge", label: "Edge Mode", kbd: "F10" },
          { cmd: "mode_face", label: "Face Mode", kbd: "F11" },
          { sep: "" },
          { cmd: "delete_mesh_selection", label: "Delete Selected Components" },
        ],
      },
      {
        slug: "mesh_display",
        label: "Mesh Display",
        items: [
          { cmd: "normals_soften", label: "Soften Edge Normals" },
          { cmd: "normals_harden", label: "Harden Edge Normals" },
          { cmd: "normals_reverse", label: "Reverse Normals" },
          { sep: "" },
          { cmd: "smooth_1", label: "Display Level 1 (Cage)", kbd: "1" },
          { cmd: "smooth_2", label: "Display Level 2 (Cage + Smooth)", kbd: "2" },
          { cmd: "smooth_3", label: "Display Level 3 (Smooth)", kbd: "3" },
          { sep: "" },
          { cmd: "toggle_wire_on_shaded", label: "Wireframe on Shaded" },
        ],
      },
      {
        slug: "uv",
        label: "UV",
        items: [
          { cmd: "mode_uv", label: "UV Component Selection", kbd: "F12" },
          { cmd: "shading_textured", label: "Textured Display", kbd: "6" },
          { cmd: "open_uv_foundation", label: "UV Editor (Foundation Status)…" },
        ],
      },
    ],
    rigging: [
      {
        slug: "rig_grouping",
        label: "Grouping",
        items: [
          { cmd: "group_selected", label: "Group", kbd: "Ctrl+G" },
          { cmd: "parent_selected", label: "Parent", kbd: "P" },
          { cmd: "unparent_selected", label: "Unparent", kbd: "Shift+P" },
          { cmd: "preset_pivot", label: "Create Locator / Group Pivot" },
        ],
      },
      {
        slug: "rig_pivot",
        label: "Pivot",
        items: [
          { cmd: "tool_pivot", label: "Edit Pivot Tool", kbd: "D" },
          { cmd: "center_pivot", label: "Center Pivot" },
          { cmd: "reset_pivot", label: "Reset Pivot" },
          { cmd: "freeze_transforms", label: "Freeze Transformations" },
        ],
      },
      {
        slug: "rig_node",
        label: "Node",
        items: [
          { cmd: "toggle_lock_selected", label: "Lock / Unlock Selected Node" },
          { cmd: "frame_selection", label: "Frame Selection", kbd: "F" },
          { cmd: "preset_camera", label: "Create Camera", kbd: "" },
        ],
      },
    ],
    animation: [
      {
        slug: "anim_keys",
        label: "Keyframes",
        items: [
          { cmd: "set_keyframe", label: "Set Keyframe", kbd: "S" },
          { cmd: "delete_keyframe", label: "Delete Keyframe at Current Time" },
          { cmd: "toggle_autokey", label: "Toggle Auto Keyframe" },
        ],
      },
      {
        slug: "anim_playback",
        label: "Playback",
        items: [
          { cmd: "timeline_toggle_play", label: "Play / Pause Playback", kbd: "Alt+V" },
          { cmd: "timeline_start", label: "Go to Playback Start" },
          { cmd: "timeline_end", label: "Go to Playback End" },
        ],
      },
      {
        slug: "anim_rig",
        label: "Showcase Rig",
        items: [{ cmd: "toggle_rig", label: "Play / Pause Showcase Turntable Rig" }],
      },
    ],
    fx: [
      {
        slug: "fx_emitters",
        label: "Emitters",
        items: [
          { cmd: "preset_sphere", label: "Create Emitter Sphere" },
          { cmd: "preset_point_light", label: "Create Glow Point Light" },
          { cmd: "preset_area_light", label: "Create Glow Area Light" },
        ],
      },
      {
        slug: "fx_view",
        label: "Viewport",
        items: [
          { cmd: "toggle_xray", label: "X-Ray Shading" },
          { cmd: "toggle_isolate", label: "Isolate Selected", kbd: "Ctrl+1" },
          { cmd: "frame_all", label: "Frame All", kbd: "A" },
          { cmd: "toggle_rig", label: "Play / Pause Turntable Simulation" },
        ],
      },
    ],
    rendering: [
      {
        slug: "render_lights",
        label: "Lights",
        items: [
          { cmd: "preset_directional_light", label: "Create Directional Light" },
          { cmd: "preset_point_light", label: "Create Point Light" },
          { cmd: "preset_spot_light", label: "Create Spot Light" },
          { cmd: "preset_area_light", label: "Create Area Light" },
          { sep: "" },
          { cmd: "shading_lights", label: "Use All Lights", kbd: "7" },
          { cmd: "lighting_default", label: "Use Default Lighting" },
        ],
      },
      {
        slug: "render_env",
        label: "Environment",
        items: [
          { cmd: "hdri_softbox", label: "Assign Studio Softbox HDRI" },
          { cmd: "hdri_sunset", label: "Assign Golden Sunset HDRI" },
          { cmd: "hdri_courtyard", label: "Assign Overcast Courtyard HDRI" },
          { cmd: "clear_hdri", label: "Clear HDRI (Studio Sky)" },
        ],
      },
      {
        slug: "render_cameras",
        label: "Cameras",
        items: [
          { cmd: "preset_camera", label: "Create Camera" },
          { cmd: "select_active_camera", label: "Select Active Camera" },
          { cmd: "toggle_camera_view", label: "Look Through Active Camera" },
        ],
      },
      {
        slug: "render_render",
        label: "Render",
        items: [
          { cmd: "render_final_frame", label: "Render Current Frame" },
          { cmd: "toggle_res_gate", label: "Resolution Gate" },
        ],
      },
    ],
  };

  const COMMAND_LABELS = {};

  function buildCommandLabelIndex() {
    Object.values(MENU_SET_DEFS).forEach((menus) => {
      menus.forEach((menu) => {
        menu.items.forEach((it) => {
          if (it && it.cmd) COMMAND_LABELS[it.cmd] = it.label;
        });
      });
    });
    document.querySelectorAll("[data-menu-cmd]").forEach((btn) => {
      const cmd = btn.dataset.menuCmd;
      if (!COMMAND_LABELS[cmd]) {
        const span = btn.querySelector("span");
        COMMAND_LABELS[cmd] = span ? span.textContent : cmd;
      }
    });
    shelfData.forEach((sh) =>
      sh.items.forEach((it) => {
        if (it.cmd && !COMMAND_LABELS[it.cmd]) COMMAND_LABELS[it.cmd] = it.label || it.code;
      })
    );
  }

  function updateMenuAvailability(state) {
    if (!state) return;
    const vp = state.viewport || {};
    const insp = state.inspector;
    const map = {
      undo: Boolean(state.history && state.history.can_undo),
      redo: Boolean(state.history && state.history.can_redo),
      duplicate_selected: Boolean(insp),
      duplicate: Boolean(insp),
      delete_obj: Boolean(insp) && (!insp || insp.id !== vp.active_camera_id),
      reset_transform: Boolean(insp),
      reset_transform_selected: Boolean(insp),
      freeze_transforms: Boolean(insp),
      center_pivot: Boolean(insp),
      reset_pivot: Boolean(insp),
      toggle_lock_selected: Boolean(insp),
      parent_selected: Boolean(insp),
      unparent_selected: Boolean(insp),
      select_hierarchy: Boolean(insp),
      mat_assign: Boolean(insp) && Boolean(state.active_material_id),
      mat_duplicate: Boolean(state.active_material_id),
      clear_hdri: Boolean(state.environment && state.environment.has_hdri),
      toggle_camera_view: Boolean(vp.active_camera_id),
      select_active_camera: Boolean(vp.active_camera_id),
    };
    document.querySelectorAll("[data-menu-cmd]").forEach((btn) => {
      const cmd = btn.dataset.menuCmd;
      if (!(cmd in map)) return;
      btn.disabled = map[cmd] === false;
    });
  }

  function renderDynamicMenuSet(menuSetId) {
    if (!els.mayaDynamicMenus) return;
    const menus = MENU_SET_DEFS[menuSetId] || MENU_SET_DEFS.modeling;
    els.mayaDynamicMenus.innerHTML = "";
    menus.forEach((menu) => {
      const item = document.createElement("div");
      item.className = "menu-item";
      const trigger = document.createElement("button");
      trigger.type = "button";
      trigger.className = "menu-trigger";
      trigger.dataset.menuTrigger = "ms_" + menu.slug;
      trigger.dataset.testid = "menu-" + menu.slug;
      trigger.textContent = menu.label;
      const panel = document.createElement("div");
      panel.className = "menu-dropdown";
      panel.hidden = true;

      menu.items.forEach((entry) => {
        if (entry.sep !== undefined) {
          if (entry.sep) {
            const lbl = document.createElement("div");
            lbl.className = "menu-section-lbl";
            lbl.textContent = entry.sep;
            panel.appendChild(lbl);
          } else {
            const sep = document.createElement("div");
            sep.className = "menu-sep";
            panel.appendChild(sep);
          }
          return;
        }
        const btn = document.createElement("button");
        btn.type = "button";
        btn.dataset.menuCmd = entry.cmd;
        btn.dataset.testid = "ms-cmd-" + entry.cmd;
        btn.innerHTML =
          "<span>" + escapeHtml(entry.label) + "</span>" +
          (entry.kbd ? "<kbd>" + escapeHtml(entry.kbd) + "</kbd>" : "");
        btn.addEventListener("click", () => {
          closeAllDropdownMenus();
          executeEditorCommand(entry.cmd);
        });
        panel.appendChild(btn);
      });

      item.appendChild(trigger);
      item.appendChild(panel);
      els.mayaDynamicMenus.appendChild(item);
    });
    bindMenuTriggers(els.mayaDynamicMenus);
    updateMenuAvailability(currentState);
  }

  function bindMenuTriggers(scope) {
    (scope || document).querySelectorAll("[data-menu-trigger]").forEach((trigger) => {
      if (trigger.dataset.boundMenuTrigger === "1") return;
      trigger.dataset.boundMenuTrigger = "1";
      trigger.addEventListener("click", (ev) => {
        ev.stopPropagation();
        closeViewportContextMenu();
        closeShelfMenus();
        closeCommandSearch();
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
          const winH = window.innerHeight || 720;
          if (rect.right > winW - 6) panel.style.left = -Math.round(rect.right - (winW - 6)) + "px";
          if (rect.bottom > winH - 6) {
            panel.scrollTop = 0;
            panel.style.maxHeight = Math.max(90, winH - rect.top - 10) + "px";
          }
        }
      });
    });
  }

  // =========================================================================
  // SHELF SYSTEM
  // =========================================================================
  function currentShelf() {
    return shelfData.find((s) => s.id === activeShelfId) || shelfData[0];
  }

  function persistShelves() {
    REG.saveShelves(shelfData);
  }

  function renderShelf() {
    if (!els.mayaShelfTabs || !els.mayaShelfItems) return;
    els.mayaShelfTabs.innerHTML = "";
    shelfData.forEach((sh) => {
      const tab = document.createElement("button");
      tab.type = "button";
      tab.className = "maya-shelf-tab" + (sh.id === activeShelfId ? " is-active" : "");
      tab.dataset.testid = "shelf-tab-" + sh.id;
      tab.textContent = sh.name;
      tab.addEventListener("click", () => {
        activeShelfId = sh.id;
        renderShelf();
      });
      els.mayaShelfTabs.appendChild(tab);
    });

    const sh = currentShelf();
    els.mayaShelfItems.innerHTML = "";
    if (!sh) return;
    sh.items.forEach((it) => {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.className = "maya-shelf-item";
      btn.dataset.testid = "shelf-item-" + it.id;
      btn.dataset.cmd = it.cmd;
      btn.title = it.label || it.code;
      btn.setAttribute("data-help", it.label || it.code);
      btn.innerHTML =
        '<span class="btn-ic">' +
        (REG.ICONS[it.icon] || REG.ICONS.cube) +
        "</span>" +
        '<span class="maya-shelf-code">' +
        escapeHtml(it.code) +
        "</span>";
      btn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        closeShelfMenus();
        executeEditorCommand(it.cmd);
      });
      btn.addEventListener("contextmenu", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        openShelfItemContextMenu(it, ev.clientX, ev.clientY);
      });
      btn.addEventListener("pointerenter", () => setHelpLine(it.label || it.code));
      els.mayaShelfItems.appendChild(btn);
    });
  }

  function openShelfItemContextMenu(item, clientX, clientY) {
    const menu = $("shelf-item-ctx-menu");
    if (!menu) return;
    shelfCtxTarget = item;
    menu.hidden = false;
    const rect = menu.getBoundingClientRect();
    const winW = window.innerWidth || 1280;
    const winH = window.innerHeight || 720;
    menu.style.left = Math.max(4, Math.min(winW - rect.width - 4, clientX)) + "px";
    menu.style.top = Math.max(4, Math.min(winH - rect.height - 4, clientY)) + "px";
  }

  function showShelfModal(title, bodyBuilder) {
    if (!els.shelfModal) return;
    els.shelfModalTitle.textContent = title;
    els.shelfModalBody.innerHTML = "";
    bodyBuilder(els.shelfModalBody);
    els.shelfModal.hidden = false;
  }

  function closeShelfModal() {
    if (els.shelfModal) els.shelfModal.hidden = true;
  }

  function shelfPrompt(title, label, defaultValue, onSubmit) {
    showShelfModal(title, (body) => {
      const field = document.createElement("label");
      field.className = "pro-field";
      field.innerHTML = "<span>" + escapeHtml(label) + "</span>";
      const input = document.createElement("input");
      input.type = "text";
      input.value = defaultValue || "";
      input.autocomplete = "off";
      input.setAttribute("data-testid", "shelf-modal-input");
      field.appendChild(input);
      body.appendChild(field);

      const actions = document.createElement("div");
      actions.className = "modal-actions-row";
      const ok = document.createElement("button");
      ok.type = "button";
      ok.className = "pro-btn pro-btn-primary";
      ok.textContent = "OK";
      ok.setAttribute("data-testid", "shelf-modal-ok");
      ok.addEventListener("click", () => {
        onSubmit(input.value.trim());
        closeShelfModal();
      });
      const cancel = document.createElement("button");
      cancel.type = "button";
      cancel.className = "pro-btn";
      cancel.textContent = "Cancel";
      cancel.addEventListener("click", closeShelfModal);
      actions.appendChild(ok);
      actions.appendChild(cancel);
      body.appendChild(actions);
      input.addEventListener("keydown", (ev) => {
        if (ev.key === "Enter") {
          ev.preventDefault();
          onSubmit(input.value.trim());
          closeShelfModal();
        }
      });
      setTimeout(() => input.focus(), 20);
    });
  }

  function shelfAddToolDialog() {
    showShelfModal("Add Command to Shelf", (body) => {
      const field = document.createElement("label");
      field.className = "pro-field";
      field.innerHTML = "<span>Command</span>";
      const select = document.createElement("select");
      select.setAttribute("data-testid", "shelf-tool-select");
      Object.keys(COMMAND_LABELS)
        .sort()
        .forEach((cmd) => {
          const opt = document.createElement("option");
          opt.value = cmd;
          opt.textContent = COMMAND_LABELS[cmd] + " (" + cmd + ")";
          select.appendChild(opt);
        });
      field.appendChild(select);
      body.appendChild(field);

      const actions = document.createElement("div");
      actions.className = "modal-actions-row";
      const ok = document.createElement("button");
      ok.type = "button";
      ok.className = "pro-btn pro-btn-primary";
      ok.textContent = "Add to Shelf";
      ok.setAttribute("data-testid", "shelf-add-confirm");
      ok.addEventListener("click", () => {
        const cmd = select.value;
        const sh = currentShelf();
        if (sh && cmd) {
          sh.items.push({
            id: "s_" + Date.now().toString(36),
            cmd: cmd,
            code: (COMMAND_LABELS[cmd] || cmd).slice(0, 5),
            label: COMMAND_LABELS[cmd] || cmd,
            icon: "cube",
          });
          persistShelves();
          renderShelf();
          setStatusMessage("Added '" + (COMMAND_LABELS[cmd] || cmd) + "' to shelf '" + sh.name + "'", false);
        }
        closeShelfModal();
      });
      const cancel = document.createElement("button");
      cancel.type = "button";
      cancel.className = "pro-btn";
      cancel.textContent = "Cancel";
      cancel.addEventListener("click", closeShelfModal);
      actions.appendChild(ok);
      actions.appendChild(cancel);
      body.appendChild(actions);
    });
  }

  function moveShelfItem(dir) {
    const sh = currentShelf();
    if (!sh || !shelfCtxTarget) return;
    const idx = sh.items.findIndex((i) => i.id === shelfCtxTarget.id);
    if (idx < 0) return;
    const next = idx + dir;
    if (next < 0 || next >= sh.items.length) return;
    const tmp = sh.items[idx];
    sh.items[idx] = sh.items[next];
    sh.items[next] = tmp;
    persistShelves();
    renderShelf();
  }

  function removeShelfItem() {
    const sh = currentShelf();
    if (!sh || !shelfCtxTarget) return;
    sh.items = sh.items.filter((i) => i.id !== shelfCtxTarget.id);
    persistShelves();
    renderShelf();
    setStatusMessage("Removed shelf command", false);
  }

  // =========================================================================
  // COMMAND EXECUTION MAP (single funnel for menus, shelves, search, hotkeys)
  // =========================================================================
  const PRESET_ALIASES = {
    preset_cube: "cube",
    preset_sphere: "sphere",
    preset_cylinder: "cylinder",
    preset_cone: "cone",
    preset_torus: "torus",
    preset_plane: "plane",
    preset_pyramid: "pyramid",
    preset_pivot: "pivot",
    preset_camera: "camera",
    preset_directional_light: "directional_light",
    preset_point_light: "point_light",
    preset_spot_light: "spot_light",
    preset_area_light: "area_light",
  };

  const TOOL_ALIASES = {
    tool_select: "select",
    tool_move: "move",
    tool_rotate: "rotate",
    tool_scale: "scale",
    tool_pivot: "pivot",
    tool_show_manip: "show_manipulator",
    tool_universal: "universal",
  };

  const VIEW_ALIASES = {
    vp_persp: { view: "home", projection: "perspective" },
    vp_home: { view: "home", projection: "perspective" },
    vp_ortho_top: { view: "pos_y", projection: "orthographic" },
    vp_ortho_bottom: { view: "neg_y", projection: "orthographic" },
    vp_ortho_front: { view: "pos_z", projection: "orthographic" },
    vp_ortho_back: { view: "neg_z", projection: "orthographic" },
    vp_ortho_right: { view: "pos_x", projection: "orthographic" },
    vp_ortho_left: { view: "neg_x", projection: "orthographic" },
  };

  function currentMeshMode() {
    const ed = currentState && currentState.editor ? currentState.editor : null;
    return ed ? ed.mode : "object";
  }

  function executeEditorCommand(cmd) {
    if (!cmd || !wasmApi) return;
    closeAllDropdownMenus();
    closeShelfMenus();
    closeCommandSearch();

    if (PRESET_ALIASES[cmd]) {
      dispatch({ action: "add_preset", preset: PRESET_ALIASES[cmd] });
      return;
    }
    if (TOOL_ALIASES[cmd]) {
      dispatch({ action: "set_active_tool", tool: TOOL_ALIASES[cmd] });
      return;
    }
    if (VIEW_ALIASES[cmd]) {
      const v = VIEW_ALIASES[cmd];
      dispatch({ action: "set_viewport_projection", projection: v.projection });
      dispatch({ action: "set_viewport_view", view: v.view, smooth: true });
      return;
    }

    switch (cmd) {
      case "undo":
        dispatch({ action: "undo" });
        return;
      case "redo":
        dispatch({ action: "redo" });
        return;
      case "duplicate_selected":
        dispatch({ action: "duplicate_selected" });
        return;
      case "delete_obj":
        dispatch({ action: "delete_selected" });
        return;
      case "group_selected":
        dispatch({ action: "group_selected" });
        return;
      case "parent_selected":
        dispatch({ action: "parent_selected" });
        return;
      case "unparent_selected":
        dispatch({ action: "unparent_selected" });
        return;
      case "freeze_transforms":
        dispatch({ action: "freeze_transforms" });
        return;
      case "center_pivot":
        dispatch({ action: "center_pivot" });
        return;
      case "reset_pivot":
        dispatch({ action: "reset_pivot" });
        return;
      case "toggle_lock_selected": {
        const insp = currentState && currentState.inspector;
        if (!insp) return;
        dispatch({ action: "set_object_lock", id: insp.id, locked: !insp.locked });
        return;
      }
      case "mode_object":
        dispatch({ action: "set_editor_mode", mode: "object" });
        return;
      case "mode_vertex":
        dispatch({ action: "set_editor_mode", mode: "vertex" });
        return;
      case "mode_edge":
        dispatch({ action: "set_editor_mode", mode: "edge" });
        return;
      case "mode_face":
        dispatch({ action: "set_editor_mode", mode: "face" });
        return;
      case "mode_uv": {
        const masks = (currentState && currentState.editor && currentState.editor.selection_masks) || {};
        dispatch({ action: "set_selection_masks", uv_mode: !masks.uv_mode });
        dispatch({ action: "set_editor_mode", mode: "vertex" });
        return;
      }
      case "select_all_components":
        dispatch({ action: "select_all_components" });
        return;
      case "extrude_faces": {
        const dist = Number(els.meshExtrudeDist && els.meshExtrudeDist.value);
        dispatch({
          action: "extrude_selected_faces",
          distance: Number.isFinite(dist) ? dist : 0.35,
        });
        return;
      }
      case "select_boundary":
        dispatch({ action: "select_boundary_loop" });
        return;
      case "fill_hole":
        dispatch({ action: "fill_selected_face" });
        return;
      case "delete_mesh_selection": {
        if (currentMeshMode() !== "object") dispatch({ action: "delete_mesh_selection" });
        else dispatch({ action: "delete_selected" });
        return;
      }
      case "normals_soften":
        dispatch({ action: "soften_normals" });
        return;
      case "normals_harden":
        dispatch({ action: "harden_normals" });
        return;
      case "normals_reverse":
        dispatch({ action: "reverse_normals" });
        return;
      case "smooth_1":
        dispatch({ action: "set_viewport_display", display_smoothness: 1 });
        return;
      case "smooth_2":
        dispatch({ action: "set_viewport_display", display_smoothness: 2 });
        return;
      case "smooth_3":
        dispatch({ action: "set_viewport_display", display_smoothness: 3 });
        return;
      case "shading_wireframe":
        dispatch({ action: "set_viewport_display", shading_mode: "wireframe" });
        return;
      case "shading_shaded":
        dispatch({ action: "set_viewport_display", shading_mode: "shaded" });
        return;
      case "shading_textured":
        dispatch({ action: "set_viewport_display", shading_mode: "textured" });
        return;
      case "shading_lights":
        dispatch({ action: "set_viewport_display", default_lighting: false });
        return;
      case "lighting_default":
        dispatch({ action: "set_viewport_display", default_lighting: true });
        return;
      case "toggle_wire_on_shaded": {
        const vp = (currentState && currentState.viewport) || {};
        dispatch({ action: "set_viewport_display", wireframe_on_shaded: !vp.wireframe_on_shaded });
        return;
      }
      case "toggle_xray": {
        const vp = (currentState && currentState.viewport) || {};
        dispatch({ action: "set_viewport_display", xray_enabled: !vp.xray_enabled });
        return;
      }
      case "toggle_isolate":
        dispatch({ action: "set_viewport_display", isolate_select: !((currentState && currentState.viewport && currentState.viewport.isolate_select) || false) });
        return;
      case "toggle_res_gate":
        toggleResolutionGate();
        return;
      case "toggle_editor_grid":
        dispatch({ action: "toggle_editor_grid" });
        return;
      case "toggle_editor_helpers":
        dispatch({ action: "toggle_editor_helpers" });
        return;
      case "frame_selection":
        dispatch({ action: "frame_selection" });
        return;
      case "frame_all":
        dispatch({ action: "frame_all" });
        return;
      case "toggle_camera_view":
        dispatch({ action: "toggle_camera_view" });
        return;
      case "select_active_camera": {
        const vp = (currentState && currentState.viewport) || {};
        if (vp.active_camera_id) dispatch({ action: "select_object", id: vp.active_camera_id });
        return;
      }
      case "select_all":
        dispatch({ action: "select_all_objects" });
        return;
      case "invert_selection":
        dispatch({ action: "invert_selection" });
        return;
      case "select_hierarchy":
        dispatch({ action: "select_hierarchy" });
        return;
      case "deselect":
        dispatch({ action: "select_object", id: 0 });
        return;
      case "mat_create":
        dispatch({ action: "create_material", name: "PBR Material" });
        return;
      case "mat_duplicate": {
        const mid = currentState && currentState.active_material_id;
        if (mid) dispatch({ action: "duplicate_material", id: mid });
        return;
      }
      case "mat_assign": {
        const mid = currentState && currentState.active_material_id;
        const insp = currentState && currentState.inspector;
        if (mid && insp) dispatch({ action: "assign_object_material", object_id: insp.id, material_id: mid });
        return;
      }
      case "hdri_softbox":
        dispatch({ action: "load_hdri_preset", preset: "studio_softbox" });
        return;
      case "hdri_sunset":
        dispatch({ action: "load_hdri_preset", preset: "warm_sunset" });
        return;
      case "hdri_courtyard":
        dispatch({ action: "load_hdri_preset", preset: "cool_courtyard" });
        return;
      case "clear_hdri":
        dispatch({ action: "clear_hdri" });
        return;
      case "set_keyframe":
        dispatch({ action: "timeline_set_key" });
        return;
      case "delete_keyframe":
        dispatch({ action: "timeline_delete_key" });
        return;
      case "toggle_autokey": {
        const tl = (currentState && currentState.timeline) || {};
        dispatch({ action: "timeline_set_auto_key", enabled: !tl.auto_key });
        return;
      }
      case "timeline_toggle_play": {
        const tl = (currentState && currentState.timeline) || {};
        dispatch({ action: "timeline_set_playing", playing: !tl.playing });
        return;
      }
      case "timeline_start":
        dispatch({ action: "timeline_step", delta: -1000000 });
        dispatch({ action: "timeline_set_frame", frame: (currentState && currentState.timeline && currentState.timeline.scene_start) || 1 });
        return;
      case "timeline_end":
        dispatch({ action: "timeline_set_frame", frame: (currentState && currentState.timeline && currentState.timeline.playback_end) || 120 });
        return;
      case "toggle_rig": {
        const vp = (currentState && currentState.viewport) || {};
        dispatch({ action: "set_rig_playing", playing: !vp.rig_playing });
        return;
      }
      case "render_final_frame":
        wasmApi.renderFinalFrame();
        setStatusMessage("Rendered final camera frame (editor helpers/grid excluded)", false);
        return;
      case "toggle_hud": {
        appPrefs.showViewportHud = !appPrefs.showViewportHud;
        REG.savePreferences(appPrefs);
        applyPreferencesAndResponsiveMode();
        return;
      }
      case "toggle_hotbox":
        toggleHotbox();
        return;
      case "step_prev_frame":
        dispatch({ action: "timeline_step", delta: -1 });
        return;
      case "step_next_frame":
        dispatch({ action: "timeline_step", delta: 1 });
        return;
      case "jump_prev_key":
        dispatch({ action: "timeline_jump_key", forward: false });
        return;
      case "jump_next_key":
        dispatch({ action: "timeline_jump_key", forward: true });
        return;
      case "save_fe3d":
        exportPortableFe3d();
        return;
      case "open_file":
        if (els.fileOpenInput) {
          els.fileOpenInput.value = "";
          els.fileOpenInput.click();
        }
        return;
      case "export_bundle":
        exportBinaryBundle();
        return;
      case "load_hdri_file":
        if (els.fileOpenHdri) {
          els.fileOpenHdri.value = "";
          els.fileOpenHdri.click();
        }
        return;
      case "new_showcase":
        activeProjectRecordId = null;
        dispatch({ action: "new_starter_project" });
        return;
      case "new_cube":
        activeProjectRecordId = null;
        dispatch({ action: "create_new_project", template: "minimal_cube", project_name: "General Cube Scene" });
        return;
      case "new_empty":
        activeProjectRecordId = null;
        dispatch({ action: "create_new_project", template: "empty_stage", project_name: "Empty Stage" });
        return;
      case "go_home":
        snapshotActiveProjectToRecent("Active Session").then(() => setAppView("home"));
        return;
      case "preferences":
        applyPreferencesAndResponsiveMode();
        if (els.settingsModal) els.settingsModal.hidden = false;
        return;
      case "open_hotkey_editor":
        openHotkeyEditor();
        return;
      case "cmd_help_summary":
        dispatch({ action: "execute_command_line", command: "help" });
        openScriptLog();
        return;
      case "focus_channel_box":
        setRightDockMode("cb_only");
        return;
      case "focus_layer_editor":
        setRightDockMode("cb_only");
        return;
      case "open_uv_foundation":
        activeBottomEditor = "uv_foundation";
        setBottomDockCollapsed(false);
        return;
      case "restore_default_shelves":
        shelfData = REG.resetDefaultShelves();
        activeShelfId = shelfData[0].id;
        renderShelf();
        setStatusMessage("Restored default Maya shelves", false);
        return;
      case "save_custom_layout":
        saveNamedCustomWorkspace();
        return;
      case "restore_custom_layout":
        restoreNamedCustomWorkspace();
        return;
      case "reset_layout":
        resetWorkspaceLayout();
        return;
      case "toggle_shelf_bar":
        if (els.mayaShelfBar) els.mayaShelfBar.hidden = !els.mayaShelfBar.hidden;
        return;
      case "toggle_tool_shelf":
        toggleToolShelf();
        return;
      case "toggle_left_dock":
        isLeftDockCollapsed = !isLeftDockCollapsed;
        if (currentState) renderWorkspaceAndBottomDock(currentState);
        return;
      case "toggle_right_dock":
        toggleRightDockVisibility();
        return;
      case "toggle_bottom_dock":
        setBottomDockCollapsed(!els.dockBottom.classList.contains("is-collapsed"));
        return;
      case "toggle_command_bar": {
        const bar = $("maya-command-bar");
        if (bar) bar.hidden = !bar.hidden;
        return;
      }
      case "reset_transform_selected":
      case "reset_transform": {
        const insp = currentState && currentState.inspector;
        if (insp) dispatch({ action: "reset_transform", id: insp.id });
        return;
      }
      case "rename_selected_prompt": {
        const insp = currentState && currentState.inspector;
        if (!insp) return;
        shelfPrompt("Rename Node", "Node Name", insp.name, (value) => {
          if (value) dispatch({ action: "rename_object", id: insp.id, name: value });
        });
        return;
      }
      case "focus_channel_box_key":
        dispatch({ action: "timeline_set_key" });
        return;
      case "menuset_modeling":
        applyMenuSet("modeling", true);
        return;
      case "menuset_rigging":
        applyMenuSet("rigging", true);
        return;
      case "menuset_animation":
        applyMenuSet("animation", true);
        return;
      case "menuset_fx":
        applyMenuSet("fx", true);
        return;
      case "menuset_rendering":
        applyMenuSet("rendering", true);
        return;
      case "ws_classic":
        activateWorkspaceById("layout", true);
        return;
      case "ws_modeling":
        activateWorkspaceById("modeling", true);
        return;
      case "ws_shading":
        activateWorkspaceById("shading", true);
        return;
      case "ws_animation":
        activateWorkspaceById("animation", true);
        return;
      case "ws_rendering":
        activateWorkspaceById("rendering", true);
        return;
      default:
        setStatusMessage("Command not available in this build: " + cmd, true);
        return;
    }
  }

  // =========================================================================
  // DISPATCH HELPERS FOR INSPECTOR INPUTS
  // =========================================================================
  function dispatchTransformFromInputs(coalesce) {
    if (!wasmApi || !currentState || !currentState.inspector) return;
    const id = currentState.inspector.id;
    const pos = [Number(els.posX.value), Number(els.posY.value), Number(els.posZ.value)];
    const rot = [Number(els.rotX.value), Number(els.rotY.value), Number(els.rotZ.value)];
    const scl = [Number(els.sclX.value), Number(els.sclY.value), Number(els.sclZ.value)];
    if ([...pos, ...rot, ...scl].some((v) => !Number.isFinite(v))) return;
    dispatch({
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
    dispatch({
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
    if (lt === "point" || lt === "spot" || lt === "area") payload.range = Number(els.lightRange.value);
    if (lt === "point" || lt === "spot") {
      payload.falloff = els.lightFalloff.value;
      payload.attenuation = Number(els.lightAttenuation.value);
    }
    if (lt === "point") payload.source_radius = Number(els.lightSourceRadius.value);
    if (lt === "spot") {
      payload.inner_cone_deg = Number(els.lightInnerCone.value);
      payload.outer_cone_deg = Number(els.lightOuterCone.value);
      payload.cone_falloff = Number(els.lightConeFalloff.value);
    }
    if (lt === "area") {
      payload.shape = els.lightAreaShape.value;
      payload.width = Number(els.lightAreaWidth.value);
      payload.height = Number(els.lightAreaHeight.value);
      payload.two_sided = Boolean(els.lightAreaTwoSided && els.lightAreaTwoSided.checked);
      payload.sample_quality = Number(els.lightAreaSamples.value);
    }
    dispatch(payload);
  }

  function dispatchLightLinkingFromInputs() {
    if (!wasmApi || !currentState || !currentState.inspector || !currentState.inspector.light) return;
    const mode = els.lightLinkMode ? els.lightLinkMode.value : "all";
    const includeDesc = els.lightLinkDescendants ? Boolean(els.lightLinkDescendants.checked) : true;
    const prevLinking = currentState.inspector.light.linking || {};
    let linkedIds = prevLinking.object_ids || [];
    if (mode === "all") {
      linkedIds = [];
    } else if (els.lightLinkTargets && els.lightLinkTargets.children.length > 0) {
      linkedIds = Array.from(els.lightLinkTargets.querySelectorAll('input[type="checkbox"]:checked'))
        .map((cb) => Number(cb.dataset.objId))
        .filter((v) => Number.isFinite(v) && v > 0);
    }
    dispatch({
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
    dispatch({
      action: "set_environment",
      enabled: Boolean(els.envEnabled.checked),
      intensity: intensity,
      rotation_deg: rotation,
      diffuse_ibl_scale: diffScale,
      specular_ibl_scale: specScale,
      coalesce: Boolean(coalesce),
    });
  }

  function dispatchMaterialFromInputs(coalesce) {
    if (!wasmApi || !currentState || !currentState.active_material_id) return;
    els.valMatMetallic.textContent = Number(els.matMetallic.value).toFixed(2);
    els.valMatRoughness.textContent = Number(els.matRoughness.value).toFixed(2);
    els.valMatAo.textContent = Number(els.matAo.value).toFixed(2);
    els.valMatOpacity.textContent = Number(els.matOpacity.value).toFixed(2);
    dispatch({
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
    dispatch({
      action: "set_metadata",
      project_name: els.metaProjectName.value.trim(),
      author: els.metaAuthor.value.trim(),
      scene_name: els.metaSceneName.value.trim(),
    });
  }

  async function importHdriFile(file) {
    if (!wasmApi || !file) return false;
    const buf = new Uint8Array(await file.arrayBuffer());
    return wasmApi.loadHdriBytes(buf, file.name);
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
      if ((lower.endsWith(".hdr") || lower.endsWith(".pfm")) && !hdriCandidateFile) hdriCandidateFile = file;
      const buf = new Uint8Array(await file.arrayBuffer());
      const vfsPath = "/fe3d_import/" + file.name;
      wasmApi.fs.writeFile(vfsPath, buf);
      if (lower.endsWith(".fe3d") || lower.endsWith(".json")) fe3dTargetPath = vfsPath;
    }

    if (!fe3dTargetPath && hdriCandidateFile) {
      await importHdriFile(hdriCandidateFile);
      return;
    }

    if (!fe3dTargetPath) {
      const msg = "Please include a .fe3d project file when opening a project.";
      setStatusMessage(msg, true);
      if (document.body.dataset.appView === "home" && els.launcherErrorBanner) {
        els.launcherErrorBanner.hidden = false;
        els.launcherErrorBanner.textContent = msg;
      }
      return;
    }

    const ok = wasmApi.loadFileFromVfs(fe3dTargetPath);
    if (!ok) {
      const errText =
        currentState && currentState.status ? currentState.status.message : "Invalid or corrupted .fe3d project file.";
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
    setStatusMessage("Saved " + stem + ".fe3d (inline schema 1.3)", false);
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

    if (els.engineBadge) els.engineBadge.textContent = "v" + engVer + " · " + backendName.split(" ")[0];
    if (els.homeEngineBadge) els.homeEngineBadge.textContent = "v" + engVer + " · " + backendName + " Ready";
    if (els.statusRenderer) {
      els.statusRenderer.textContent = deviceName ? backendName + " · " + deviceName : backendName;
    }
    if (els.activeProjectPill) els.activeProjectPill.textContent = state.project.project_name || "Untitled";

    els.btnUndo.disabled = !state.history.can_undo;
    els.btnRedo.disabled = !state.history.can_redo;
    els.btnUndo.title = state.history.can_undo ? "Undo " + state.history.next_undo + " (Ctrl+Z)" : "Nothing to undo";
    els.btnRedo.title = state.history.can_redo ? "Redo " + state.history.next_redo + " (Ctrl+Y)" : "Nothing to redo";

    const insp = state.inspector;
    const hasSel = insp !== null && typeof insp === "object";
    els.btnDuplicate.disabled = !hasSel;
    els.btnDelete.disabled = !hasSel || (insp && insp.id === state.viewport.active_camera_id);

    const ed =
      state.editor || {
        mode: "object",
        menu_set: "modeling",
        active_tool: "move",
        gizmo_mode: "translate",
        gizmo_space: "world",
        gizmo_active_handle: "none",
        gizmo_hovered_handle: "none",
        editable_mesh: null,
        selection_masks: {},
        snap: {},
      };

    // Menu set & toolbox sync
    if (ed.menu_set && els.mayaMenuSetSelect && els.mayaMenuSetSelect.value !== ed.menu_set) {
      els.mayaMenuSetSelect.value = ed.menu_set;
      renderDynamicMenuSet(ed.menu_set);
    }
    document.querySelectorAll("[data-maya-tool]").forEach((b) => {
      b.classList.toggle("is-active", b.dataset.mayaTool === ed.active_tool);
    });
    [els.btnGizmoTranslate, els.btnGizmoRotate, els.btnGizmoScale].forEach((b) => {
      if (b) b.classList.toggle("is-active", b.dataset.gizmoMode === ed.gizmo_mode);
    });
    if (els.lblGizmoSpace) els.lblGizmoSpace.textContent = ed.gizmo_space === "local" ? "Local" : "World";
    if (els.tsCoordSpace && !isFocused(els.tsCoordSpace)) els.tsCoordSpace.value = ed.gizmo_space;

    // Selection masks & snapping
    const masks = ed.selection_masks || {};
    [els.maskMeshes, els.maskLights, els.maskCameras, els.maskHelpers].forEach((b) => {
      if (!b) return;
      b.classList.toggle("is-active", Boolean(masks[b.dataset.selMask]));
    });
    if (els.btnModeUv) els.btnModeUv.classList.toggle("is-active", Boolean(masks.uv_mode));
    const snap = ed.snap || {};
    document.querySelectorAll("[data-snap-toggle]").forEach((b) => {
      b.classList.toggle("is-active", Boolean(snap[b.dataset.snapToggle]));
    });
    const dlState = state.display_layers || {};
    const activeLayer = (dlState.layers || []).find((l) => l.id === (dlState.active_layer_id || 0));
    const activeLayerMode = activeLayer ? activeLayer.mode : "normal";
    if (els.btnStatusTemplate) els.btnStatusTemplate.classList.toggle("is-active", activeLayerMode === "template");
    if (els.btnStatusReference) els.btnStatusReference.classList.toggle("is-active", activeLayerMode === "reference");

    // Component modes
    const canMeshEdit = Boolean(ed.editable_mesh && ed.editable_mesh.editable);
    els.btnModeVertex.disabled = !canMeshEdit;
    els.btnModeEdge.disabled = !canMeshEdit;
    els.btnModeFace.disabled = !canMeshEdit;
    [els.btnModeObject, els.btnModeVertex, els.btnModeEdge, els.btnModeFace].forEach((b) => {
      if (b) b.classList.toggle("is-active", b.dataset.editMode === ed.mode);
    });
    if (els.ctxMeshSubbar) els.ctxMeshSubbar.hidden = ed.mode === "object";

    if (ed.mode !== "object" && activeInspTab !== "all" && activeInspTab !== "modifiers") {
      setInspectorCategory("modifiers");
    } else if (insp && insp.id !== lastAutoInspObjId) {
      lastAutoInspObjId = insp.id;
      if ((insp.camera || insp.light) && !insp.mesh && activeInspTab !== "all" && activeInspTab !== "data") {
        setInspectorCategory("all");
      }
    } else if (!insp) {
      lastAutoInspObjId = 0;
    }

    // Viewport shading state
    const vp = state.viewport || {};
    const shading = vp.shading_mode || "textured";
    if (els.btnVpWireframe) els.btnVpWireframe.classList.toggle("is-active", shading === "wireframe");
    if (els.btnVpShaded) els.btnVpShaded.classList.toggle("is-active", shading === "shaded");
    if (els.btnVpTextured) els.btnVpTextured.classList.toggle("is-active", shading === "textured");
    if (els.btnVpWos) els.btnVpWos.classList.toggle("is-active", Boolean(vp.wireframe_on_shaded));
    if (els.btnVpLights) els.btnVpLights.classList.toggle("is-active", !vp.default_lighting);
    if (els.btnVpXray) els.btnVpXray.classList.toggle("is-active", Boolean(vp.xray_enabled));
    if (els.btnVpIsolate) els.btnVpIsolate.classList.toggle("is-active", Boolean(vp.isolate_select));
    if (els.btnVpGrid) els.btnVpGrid.classList.toggle("is-active", Boolean(vp.editor_grid_visible));

    // Rig transport labels
    const playing = Boolean(vp.rig_playing);
    if (els.btnToggleRig) els.btnToggleRig.textContent = playing ? "Pause Rig" : "Play Rig";

    const tl = state.timeline || {};
    const tlPlaying = Boolean(tl.playing);
    if (els.lblTimelinePlay) els.lblTimelinePlay.textContent = tlPlaying ? "Pause" : "Play";
    if (els.icTimelinePlay) els.icTimelinePlay.innerHTML = tlPlaying ? REG.ICONS.pause : REG.ICONS.play;
    if (els.btnTlAutokey) els.btnTlAutokey.classList.toggle("is-active", Boolean(tl.auto_key));

    setInputValueIfUnfocused(els.metaProjectName, state.project.project_name);
    setInputValueIfUnfocused(els.metaAuthor, state.project.author);
    setInputValueIfUnfocused(els.metaSceneName, state.project.scene_name);

    const activeCamObj = (state.objects || []).find((o) => o.id === vp.active_camera_id);
    const lensSuffix = vp.camera_view_active ? " [Lens]" : vp.view_label ? " · " + vp.view_label : "";
    if (els.vpCameraPill) {
      els.vpCameraPill.textContent =
        "Camera: " +
        (activeCamObj ? activeCamObj.name : "None") +
        lensSuffix +
        " · Dist " +
        formatFloat(vp.camera_distance, 1) +
        "m";
    }
    if (els.vpSelectionPill) {
      els.vpSelectionPill.textContent = hasSel ? "Selected: " + insp.name : "No Selection";
    }

    const toolLabel = ed.gizmo_mode === "translate" ? "MOVE" : String(ed.gizmo_mode).toUpperCase();
    const handleHint =
      ed.gizmo_active_handle && ed.gizmo_active_handle !== "none"
        ? " · DRAG " + String(ed.gizmo_active_handle).toUpperCase()
        : ed.gizmo_hovered_handle && ed.gizmo_hovered_handle !== "none"
        ? " · HOVER " + String(ed.gizmo_hovered_handle).toUpperCase()
        : "";
    if (els.vpModePill) {
      els.vpModePill.textContent =
        String(ed.mode).toUpperCase() + " · " + toolLabel + " (" + String(ed.gizmo_space).toUpperCase() + ")" + handleHint;
    }

    renderViewNavGizmo(state);
    if (vp.view_transition_active) scheduleViewTransitionSync();

    renderWorkspaceAndBottomDock(state);
    renderOutliner(state);
    renderInspector(state, ed);
    renderEnvironmentCard(state);
    renderMaterialEditor(state);
    renderChannelBox(state);
    renderLayerEditor(state);
    renderTimeline(state, tl);
    updateMenuAvailability(state);

    if (els.statObjectsMeshes) els.statObjectsMeshes.textContent = state.stats.object_count + " / " + state.stats.mesh_count;
    if (els.statMaterialsUpdates) {
      els.statMaterialsUpdates.textContent =
        (state.stats.material_count || 0) + " / " + (state.stats.gpu_buffer_updates || 0);
    }
    const ld = state.lighting_diagnostics || {};
    if (els.statLightsShadows) {
      els.statLightsShadows.textContent =
        (ld.active_lights !== undefined ? ld.active_lights : ld.active_realtime_lights || 0) +
        " / " +
        (ld.active_shadow_lights || 0);
    }
    if (els.statTriangles) els.statTriangles.textContent = String(state.stats.total_triangles);
    if (els.statMemory) els.statMemory.textContent = formatBytes(state.stats.cpu_mesh_bytes) + " / " + formatBytes(state.stats.gpu_mesh_bytes);
    if (els.statLightingGpu) {
      els.statLightingGpu.textContent =
        formatBytes(ld.shadow_gpu_bytes || 0) + " / " + formatBytes(ld.ibl_gpu_bytes || 0);
    }
    if (els.statLightingWarning) {
      const warnMsg = ld.warning_message || "";
      els.statLightingWarning.hidden = !warnMsg;
      els.statLightingWarning.textContent = warnMsg;
    }
    if (els.statHistory) {
      els.statHistory.textContent = state.history.undo_count + " steps (" + formatBytes(state.history.memory_bytes) + ")";
    }

    if (state.status && state.status.message) {
      els.statusDot.classList.toggle("is-error", state.status.level === "error");
      els.statusMessage.textContent = state.status.message;
      if (els.mayaCommandFeedback) {
        els.mayaCommandFeedback.textContent = state.status.message;
        els.mayaCommandFeedback.classList.toggle("is-error", state.status.level === "error");
      }
    }

    wasmApi.blitMaterialPreview();
  }

  // =========================================================================
  // WORKSPACE / DOCKS
  // =========================================================================
  const FOUNDATION_EDITOR_COPY = {
    shader_foundation: {
      title: "Shader Node Graph (Architectural Foundation)",
      desc: "Node-based procedural shader DAG compilation is scheduled for a future engine stage. Direct Metallic-Roughness PBR parameters & live 3D sphere preview are active in the Attribute Editor.",
    },
    uv_foundation: {
      title: "2D UV Editor (Architectural Foundation)",
      desc: "EditableMesh preserves per-corner UV attributes in memory and .fe3d serialization. Interactive 2D UV island unwrapping & packing are planned for a future stage.",
    },
    geonodes_foundation: {
      title: "Geometry Nodes Graph (Architectural Foundation)",
      desc: "Procedural geometry modifier graphs are scheduled for a future engine stage. Direct undoable polygon extrusion, boundary selection & fill are active today.",
    },
    compositor_foundation: {
      title: "Render Layer Compositor (Architectural Foundation)",
      desc: "Post-processing node chains (glare, tone-mapping curves, AOV passes) are planned for a future stage.",
    },
    vse_foundation: {
      title: "Video Sequencer (Architectural Foundation)",
      desc: "Multi-track timeline clip editing is planned for a future stage. The Maya Time Slider is connected to the live C++20 animation engine.",
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
        btn.classList.toggle("is-on", Boolean(panelVis[btn.dataset.panelToggle]));
      });

      if (!isPortrait) {
        if (els.panelOutliner) els.panelOutliner.hidden = panelVis.scene_outliner === false;
        if (els.propertiesShell) {
          const anyProps = panelVis.properties !== false || panelVis.material_editor !== false;
          els.propertiesShell.hidden = !anyProps;
        }
        if (els.splitterOutliner) {
          els.splitterOutliner.hidden = Boolean(
            (els.panelOutliner && els.panelOutliner.hidden) || (els.propertiesShell && els.propertiesShell.hidden)
          );
        }
        if (activeInspTab === "all") {
          if (els.panelProperties) els.panelProperties.hidden = panelVis.properties === false;
          if (els.panelMaterialEditor) els.panelMaterialEditor.hidden = panelVis.material_editor === false;
        }

        if (els.dockLeft) {
          const leftOff = isLeftDockCollapsed || panelVis.scene_outliner === false;
          els.dockLeft.hidden = leftOff;
          if (els.splitterLeft) els.splitterLeft.hidden = leftOff;
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
          if (els.splitterBottom) els.splitterBottom.hidden = shouldHideBottom || isBottomCollapsed;
          if (!isFoundationEditor && visibleBottom.length > 0 && !visibleBottom.includes(activeBottomEditor)) {
            activeBottomEditor = visibleBottom[0];
          }
        }
      } else {
        if (els.panelOutliner) els.panelOutliner.hidden = false;
        if (els.propertiesShell) els.propertiesShell.hidden = false;
        if (els.dockBottom) els.dockBottom.hidden = false;
        if (els.dockRight) els.dockRight.hidden = false;
        if (els.btnVpRestoreRight) els.btnVpRestoreRight.hidden = true;
      }

      const menuToggleGrid = document.querySelector('[data-menu-cmd="toggle_editor_grid"]');
      if (menuToggleGrid && state.viewport) menuToggleGrid.classList.toggle("is-on", Boolean(state.viewport.editor_grid_visible));
      const menuToggleHelpers = document.querySelector('[data-menu-cmd="toggle_editor_helpers"]');
      if (menuToggleHelpers && state.viewport) {
        menuToggleHelpers.classList.toggle("is-on", Boolean(state.viewport.editor_helpers_visible));
      }
      const menuToggleHud = document.querySelector('[data-menu-cmd="toggle_hud"]');
      if (menuToggleHud) menuToggleHud.classList.toggle("is-on", appPrefs.showViewportHud !== false);
      const menuToggleShelfBar = document.querySelector('[data-menu-cmd="toggle_shelf_bar"]');
      if (menuToggleShelfBar && els.mayaShelfBar) menuToggleShelfBar.classList.toggle("is-on", !els.mayaShelfBar.hidden);
    }

    [els.tabTimeline, els.tabAssetBrowser, els.tabConsole].forEach((tab) => {
      if (tab) tab.classList.toggle("is-active", tab.dataset.bottomTab === activeBottomEditor);
    });
    if (els.bottomEditorSelect && els.bottomEditorSelect.value !== activeBottomEditor) {
      els.bottomEditorSelect.value = activeBottomEditor;
    }
    if (els.paneTimeline) els.paneTimeline.hidden = activeBottomEditor !== "timeline";
    if (els.paneAssetBrowser) els.paneAssetBrowser.hidden = activeBottomEditor !== "asset_browser";
    if (els.paneConsole) els.paneConsole.hidden = activeBottomEditor !== "console";
    if (els.paneFoundationEditor) els.paneFoundationEditor.hidden = !isFoundationEditor;

    if (isFoundationEditor) {
      const info = FOUNDATION_EDITOR_COPY[activeBottomEditor];
      els.fePaneTitle.textContent = info.title;
      els.fePaneDesc.textContent = info.desc;
      const selName = state.inspector ? state.inspector.name : "None";
      const activeMat = (state.materials || []).find((m) => m.id === state.active_material_id);
      els.fePaneBinding.textContent =
        "Shared Scene: '" +
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
        { preset: "cube", label: "Polygon Cube", icon: "cube" },
        { preset: "sphere", label: "Polygon Sphere", icon: "sphere" },
        { preset: "cylinder", label: "Polygon Cylinder", icon: "cylinder" },
        { preset: "cone", label: "Polygon Cone", icon: "cone" },
        { preset: "torus", label: "Polygon Torus", icon: "torus" },
        { preset: "plane", label: "Polygon Plane", icon: "plane" },
        { preset: "point_light", label: "Point Light", icon: "light" },
        { preset: "spot_light", label: "Spot Light", icon: "spotLight" },
        { preset: "area_light", label: "Area Light", icon: "areaLight" },
      ].forEach((pr) => {
        const chip = document.createElement("button");
        chip.type = "button";
        chip.className = "asset-chip";
        chip.dataset.testid = "asset-preset-" + pr.preset;
        chip.innerHTML = '<span class="btn-ic">' + REG.ICONS[pr.icon] + "</span><span>" + pr.label + "</span>";
        chip.addEventListener("click", () => dispatch({ action: "add_preset", preset: pr.preset }));
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
        chip.addEventListener("click", () => dispatch({ action: "select_material", id: mat.id }));
        els.assetBrowserList.appendChild(chip);
      });
    }

    const consoleSig = JSON.stringify(state.console || []);
    if (consoleSig !== lastConsoleSignature && els.consoleLogList) {
      lastConsoleSignature = consoleSig;
      els.consoleLogList.innerHTML = "";
      (state.console || [])
        .slice(-40)
        .reverse()
        .forEach((entry) => {
          const line = document.createElement("div");
          line.className = "console-line" + (entry.level === "error" ? " is-error" : "");
          line.textContent = "#" + entry.seq + " " + entry.message;
          els.consoleLogList.appendChild(line);
        });
    }
  }

  function setRightDockMode(mode) {
    rightDockMode = mode;
    document.querySelectorAll("[data-right-mode]").forEach((b) => {
      b.classList.toggle("is-active", b.dataset.rightMode === mode);
    });
    if (els.rightDockStack) els.rightDockStack.hidden = mode === "ae_only";
    const aeWrap = $("slot-attribute-editor");
    if (aeWrap) aeWrap.hidden = mode === "cb_only";
    if (els.splitterRightStack) els.splitterRightStack.hidden = mode !== "stacked";
    if (mode === "stacked") {
      if (els.rightDockStack) els.rightDockStack.style.flex = "";
      if (aeWrap) aeWrap.style.flex = "";
    } else if (mode === "cb_only") {
      if (els.rightDockStack) els.rightDockStack.style.flex = "1 1 100%";
    } else {
      if (aeWrap) aeWrap.style.flex = "1 1 100%";
    }
    if (currentState) renderWorkspaceAndBottomDock(currentState);
  }

  function toggleToolShelf() {
    if (!els.toolShelf) return;
    els.toolShelf.hidden = !els.toolShelf.hidden;
    if (els.btnVpRestoreShelf) els.btnVpRestoreShelf.hidden = !els.toolShelf.hidden;
    if (currentState) renderWorkspaceAndBottomDock(currentState);
  }

  function toggleRightDockVisibility() {
    const mode = document.body.dataset.layoutMode || "desktop";
    if (mode === "landscape") {
      const closed = document.body.classList.toggle("is-landscape-drawer-closed");
      if (els.btnLandscapeInspectorToggle) els.btnLandscapeInspectorToggle.classList.toggle("is-active", !closed);
    } else {
      isRightDockCollapsed = !isRightDockCollapsed;
    }
    if (currentState) renderWorkspaceAndBottomDock(currentState);
  }

  function setBottomDockCollapsed(collapsed) {
    if (!els.dockBottom) return;
    if (collapsed) els.dockBottom.hidden = false;
    els.dockBottom.classList.toggle("is-collapsed", collapsed);
    if (els.btnCollapseBottom) {
      els.btnCollapseBottom.textContent = collapsed ? "+" : "−";
      els.btnCollapseBottom.setAttribute("aria-expanded", collapsed ? "false" : "true");
      els.btnCollapseBottom.title = collapsed ? "Expand Bottom Dock" : "Collapse Bottom Dock";
    }
    if (currentState) renderWorkspaceAndBottomDock(currentState);
  }

  function toggleResolutionGate() {
    if (!els.viewportResGate) return;
    els.viewportResGate.hidden = !els.viewportResGate.hidden;
    if (els.btnVpResGate) els.btnVpResGate.classList.toggle("is-active", !els.viewportResGate.hidden);
  }

  function openScriptLog() {
    activeBottomEditor = "console";
    if (els.bottomEditorSelect) els.bottomEditorSelect.value = "console";
    setBottomDockCollapsed(false);
    if (currentState) renderWorkspaceAndBottomDock(currentState);
  }

  // =========================================================================
  // OUTLINER
  // =========================================================================
  function renderOutliner(state) {
    if (!els.outlinerList) return;
    const selectedId = state.viewport ? state.viewport.selected_object_id : 0;
    const selIds = (state.viewport && state.viewport.selected_object_ids) || (selectedId ? [selectedId] : []);
    const selSet = new Set(selIds);
    const q = outlinerFilterQuery.trim().toLowerCase();
    const sig = JSON.stringify([selIds, state.objects, q, Array.from(outlinerCollapsed)]);
    if (sig === lastOutlinerSignature) return;
    lastOutlinerSignature = sig;

    const objects = state.objects || [];
    const childrenOf = new Map();
    objects.forEach((o) => {
      const pid = o.parent_id || 0;
      if (!childrenOf.has(pid)) childrenOf.set(pid, []);
      childrenOf.get(pid).push(o);
    });
    const subtreeHasMatch = (obj) => {
      if (!q) return true;
      if ((obj.name || "").toLowerCase().indexOf(q) !== -1) return true;
      const kids = childrenOf.get(obj.id) || [];
      return kids.some(subtreeHasMatch);
    };

    els.outlinerList.innerHTML = "";
    const rendered = new Set();
    const appendNode = (obj) => {
      if (rendered.has(obj.id)) return;
      rendered.add(obj.id);
      const kids = childrenOf.get(obj.id) || [];
      if (!subtreeHasMatch(obj)) return;

      const isSel = selSet.has(obj.id) || obj.id === selectedId;
      const row = document.createElement("div");
      row.className =
        "outliner-row" +
        (isSel ? " is-selected" : "") +
        (selSet.size > 1 && isSel ? " is-multi-selected" : "") +
        (!obj.visible ? " is-hidden-obj" : "") +
        (obj.locked ? " is-locked-obj" : "");
      row.setAttribute("role", "treeitem");
      row.setAttribute("aria-selected", isSel ? "true" : "false");
      row.dataset.testid = "outliner-item-" + obj.name;
      row.dataset.objId = String(obj.id);
      row.draggable = true;
      row.style.paddingLeft = 4 + obj.depth * 12 + "px";

      const collapsed = outlinerCollapsed.has(obj.id);
      const twisty = document.createElement("button");
      twisty.type = "button";
      twisty.className = "outliner-twisty" + (kids.length === 0 ? " is-leaf" : "");
      twisty.textContent = collapsed ? "▸" : "▾";
      twisty.addEventListener("click", (ev) => {
        ev.stopPropagation();
        if (outlinerCollapsed.has(obj.id)) outlinerCollapsed.delete(obj.id);
        else outlinerCollapsed.add(obj.id);
        lastOutlinerSignature = "";
        renderOutliner(currentState);
      });

      const iconWrap = document.createElement("span");
      iconWrap.className = "btn-ic";
      if (obj.has_camera) iconWrap.innerHTML = REG.ICONS.camera;
      else if (obj.has_light) iconWrap.innerHTML = REG.ICONS.light;
      else if (obj.has_mesh) iconWrap.innerHTML = REG.ICONS.cube;
      else iconWrap.innerHTML = REG.ICONS.pivot;

      const nameSpan = document.createElement("span");
      nameSpan.className = "outliner-name";
      nameSpan.textContent = obj.name;
      nameSpan.title = obj.name + " (double-click to rename · drag onto another node to reparent)";

      const tagSpan = document.createElement("span");
      tagSpan.className = "outliner-tag";
      const lt = obj.light_type || "directional";
      tagSpan.textContent = obj.has_camera
        ? "CAM"
        : obj.has_light
        ? lt === "point"
          ? "PT"
          : lt === "spot"
          ? "SPOT"
          : lt === "area"
          ? "AREA"
          : "SUN"
        : obj.has_mesh
        ? "MESH"
        : "PIVOT";

      const lockBtn = document.createElement("button");
      lockBtn.type = "button";
      lockBtn.className = "outliner-lock-btn" + (obj.locked ? " is-locked" : "");
      lockBtn.dataset.testid = "toggle-lock-" + obj.name;
      lockBtn.title = obj.locked ? "Unlock Node" : "Lock Node";
      lockBtn.innerHTML = '<span class="btn-ic">' + (obj.locked ? REG.ICONS.lock : REG.ICONS.unlock) + "</span>";
      lockBtn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        dispatch({ action: "set_object_lock", id: obj.id, locked: !obj.locked });
      });

      const eyeBtn = document.createElement("button");
      eyeBtn.type = "button";
      eyeBtn.className = "outliner-eye-btn";
      eyeBtn.dataset.testid = "toggle-vis-" + obj.name;
      eyeBtn.title = obj.visible ? "Hide in Viewport" : "Show in Viewport";
      eyeBtn.innerHTML = '<span class="btn-ic">' + (obj.visible ? REG.ICONS.eye : REG.ICONS.eyeOff) + "</span>";
      eyeBtn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        dispatch({ action: "set_visibility", id: obj.id, visible: !obj.visible });
      });

      row.appendChild(twisty);
      row.appendChild(iconWrap);
      row.appendChild(nameSpan);
      row.appendChild(tagSpan);
      row.appendChild(lockBtn);
      row.appendChild(eyeBtn);

      row.addEventListener("click", (ev) => {
        if (ev.shiftKey || ev.ctrlKey || ev.metaKey) {
          dispatch({ action: "select_object", id: obj.id, additive: true });
        } else {
          dispatch({ action: "select_object", id: obj.id });
        }
        setHelpLine("Selected " + obj.name + " — Shift+Click adds to selection, double-click renames");
      });

      row.addEventListener("dblclick", (ev) => {
        ev.stopPropagation();
        const input = document.createElement("input");
        input.type = "text";
        input.className = "outliner-rename-input";
        input.value = obj.name;
        input.setAttribute("data-testid", "outliner-rename-input");
        row.replaceChild(input, nameSpan);
        input.focus();
        input.select();
        const commit = () => {
          const next = input.value.trim();
          if (next && next !== obj.name) dispatch({ action: "rename_object", id: obj.id, name: next });
          else {
            lastOutlinerSignature = "";
            if (currentState) renderOutliner(currentState);
          }
        };
        input.addEventListener("blur", commit);
        input.addEventListener("keydown", (e) => {
          if (e.key === "Enter") {
            e.preventDefault();
            input.blur();
          } else if (e.key === "Escape") {
            input.value = obj.name;
            input.blur();
          }
        });
      });

      row.addEventListener("contextmenu", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        closeAllDropdownMenus();
        closeShelfMenus();
        openOutlinerCtxMenu(obj, ev.clientX, ev.clientY);
      });

      row.addEventListener("pointerdown", (ev) => {
        if (ev.button !== 0) return;
        beginOutlinerDrag(obj.id, ev.clientX, ev.clientY);
      });

      row.addEventListener("dragstart", (ev) => {
        ev.dataTransfer.setData("text/plain", String(obj.id));
        ev.dataTransfer.effectAllowed = "move";
      });
      row.addEventListener("dragover", (ev) => {
        ev.preventDefault();
        row.classList.add("is-drop-target");
      });
      row.addEventListener("dragleave", () => row.classList.remove("is-drop-target"));
      row.addEventListener("drop", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        row.classList.remove("is-drop-target");
        const childId = Number(ev.dataTransfer.getData("text/plain"));
        if (Number.isFinite(childId) && childId > 0 && childId !== obj.id) {
          dispatch({ action: "reparent_object", child_id: childId, parent_id: obj.id });
        }
      });

      els.outlinerList.appendChild(row);
      if (!collapsed) kids.forEach(appendNode);
    };

    const byId = new Set(objects.map((o) => o.id));
    objects
      .filter((o) => !o.parent_id || !byId.has(o.parent_id))
      .forEach(appendNode);
  }

  // Pointer-based drag reparenting (works with mouse, pen and touch input in every host).
  const outlinerDrag = { id: 0, moved: false, x: 0, y: 0 };

  function beginOutlinerDrag(id, x, y) {
    outlinerDrag.id = id;
    outlinerDrag.moved = false;
    outlinerDrag.x = x;
    outlinerDrag.y = y;
  }

  function endOutlinerDrag(clientX, clientY) {
    const sourceId = outlinerDrag.id;
    const moved = outlinerDrag.moved;
    outlinerDrag.id = 0;
    outlinerDrag.moved = false;
    document.querySelectorAll(".outliner-row.is-drop-target, .outliner-root-drop.is-drop-target").forEach((el) =>
      el.classList.remove("is-drop-target")
    );
    if (!sourceId || !moved) return;
    const el = document.elementFromPoint(clientX, clientY);
    if (!el) return;
    const row = el.closest ? el.closest(".outliner-row") : null;
    if (row && row.dataset.objId) {
      const targetId = Number(row.dataset.objId);
      if (Number.isFinite(targetId) && targetId !== sourceId) {
        dispatch({ action: "reparent_object", child_id: sourceId, parent_id: targetId });
      }
      return;
    }
    const rootDrop = el.closest ? el.closest(".outliner-root-drop") : null;
    if (rootDrop) dispatch({ action: "reparent_object", child_id: sourceId, parent_id: 0 });
  }

  // ---- Outliner node context menu (Rename / Duplicate / Delete / Group / Parent / Lock / Hide) ----
  let outlinerCtxMenuEl = null;

  function closeOutlinerCtxMenu() {
    if (outlinerCtxMenuEl) outlinerCtxMenuEl.hidden = true;
  }

  function ensureOutlinerCtxMenu() {
    if (outlinerCtxMenuEl) return outlinerCtxMenuEl;
    const menu = document.createElement("div");
    menu.className = "viewport-context-menu outliner-ctx-menu";
    menu.dataset.testid = "outliner-ctx-menu";
    menu.hidden = true;
    const header = document.createElement("div");
    header.className = "ctx-menu-header";
    header.textContent = "Outliner Node";
    menu.appendChild(header);
    const entries = [
      ["rename", "Rename…"],
      ["duplicate", "Duplicate"],
      ["group", "Group Selected"],
      ["parent", "Parent Selected"],
      ["unparent", "Unparent"],
      ["lock", "Lock / Unlock"],
      ["hide", "Hide / Show"],
      ["delete", "Delete", "ctx-item-danger"],
    ];
    entries.forEach(([cmd, label, cls]) => {
      if (cmd === "delete") {
        const sep = document.createElement("div");
        sep.className = "menu-sep";
        menu.appendChild(sep);
      }
      const btn = document.createElement("button");
      btn.type = "button";
      btn.dataset.outlinerCmd = cmd;
      btn.dataset.testid = "ol-cmd-" + cmd;
      if (cls) btn.className = cls;
      btn.textContent = label;
      btn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        closeOutlinerCtxMenu();
        const insp = currentState && currentState.inspector;
        if (!insp) return;
        if (cmd === "rename") {
          shelfPrompt("Rename Node", "Node Name", insp.name, (value) => {
            if (value) dispatch({ action: "rename_object", id: insp.id, name: value });
          });
        } else if (cmd === "duplicate") {
          dispatch({ action: "duplicate_selected" });
        } else if (cmd === "group") {
          dispatch({ action: "group_selected" });
        } else if (cmd === "parent") {
          dispatch({ action: "parent_selected" });
        } else if (cmd === "unparent") {
          dispatch({ action: "unparent_selected" });
        } else if (cmd === "lock") {
          dispatch({ action: "set_object_lock", id: insp.id, locked: !insp.locked });
        } else if (cmd === "hide") {
          dispatch({ action: "set_visibility", id: insp.id, visible: !insp.visible });
        } else if (cmd === "delete") {
          dispatch({ action: "delete_selected" });
        }
      });
      menu.appendChild(btn);
    });
    document.body.appendChild(menu);
    outlinerCtxMenuEl = menu;
    return menu;
  }

  function openOutlinerCtxMenu(obj, clientX, clientY) {
    const menu = ensureOutlinerCtxMenu();
    dispatch({ action: "select_object", id: obj.id });
    menu.hidden = false;
    const rect = menu.getBoundingClientRect();
    const winW = window.innerWidth || 1280;
    const winH = window.innerHeight || 720;
    menu.style.left = Math.max(4, Math.min(winW - rect.width - 4, clientX)) + "px";
    menu.style.top = Math.max(4, Math.min(winH - rect.height - 4, clientY)) + "px";
    setHelpLine("Outliner context menu · Rename, Duplicate, Group, Parent, Lock, Hide or Delete node");
  }

  // =========================================================================
  // CHANNEL BOX
  // =========================================================================
  function channelValuesFromInspector(insp) {
    return {
      tx: formatFloat(insp.position[0], 3),
      ty: formatFloat(insp.position[1], 3),
      tz: formatFloat(insp.position[2], 3),
      rx: formatFloat(insp.rotation_deg[0], 2),
      ry: formatFloat(insp.rotation_deg[1], 2),
      rz: formatFloat(insp.rotation_deg[2], 2),
      sx: formatFloat(insp.scale[0], 3),
      sy: formatFloat(insp.scale[1], 3),
      sz: formatFloat(insp.scale[2], 3),
    };
  }

  function renderChannelBox(state) {
    const insp = state.inspector;
    if (!els.cbTable || !els.cbEmpty) return;
    if (!insp) {
      els.cbTable.hidden = true;
      els.cbEmpty.hidden = false;
      if (els.cbNodeTitle) els.cbNodeTitle.textContent = "Channel Box";
      if (els.cbShapes) els.cbShapes.textContent = "";
      if (els.cbCustomList) els.cbCustomList.innerHTML = "";
      return;
    }
    els.cbTable.hidden = false;
    els.cbEmpty.hidden = true;
    if (els.cbNodeTitle) els.cbNodeTitle.textContent = insp.name;

    const vals = channelValuesFromInspector(insp);
    const map = {
      tx: els.cbTx,
      ty: els.cbTy,
      tz: els.cbTz,
      rx: els.cbRx,
      ry: els.cbRy,
      rz: els.cbRz,
      sx: els.cbSx,
      sy: els.cbSy,
      sz: els.cbSz,
    };
    Object.keys(map).forEach((k) => setInputValueIfUnfocused(map[k], vals[k]));
    if (els.cbVis && !isFocused(els.cbVis)) els.cbVis.value = insp.visible ? "1" : "0";

    const locked = Number(insp.locked_channels || 0);
    document.querySelectorAll("[data-cb-lock]").forEach((btn) => {
      const bit = Number(btn.dataset.cbLock);
      const isLocked = Boolean(insp.locked) || (locked & (1 << bit)) !== 0;
      btn.classList.toggle("is-locked", isLocked);
      btn.title = isLocked ? "Unlock Channel " + bit : "Lock Channel " + bit;
    });
    document.querySelectorAll(".cb-row[data-cb-bit]").forEach((row) => {
      const bit = Number(row.dataset.cbBit);
      row.classList.toggle("is-locked", Boolean(insp.locked) || (locked & (1 << bit)) !== 0);
    });

    if (els.cbShapes) {
      const parts = [];
      if (insp.mesh) parts.push("Mesh: " + insp.mesh.name);
      if (insp.camera) parts.push("Camera: " + insp.camera.projection);
      if (insp.light) parts.push("Light: " + insp.light.type);
      if (!parts.length) parts.push("Transform");
      els.cbShapes.textContent = parts.join("  ·  ");
    }

    const attrs = insp.custom_attributes || [];
    const sig = JSON.stringify([insp.id, attrs]);
    if (sig !== lastCbSig && els.cbCustomList) {
      lastCbSig = sig;
      els.cbCustomList.innerHTML = "";
      attrs.forEach((a) => {
        const row = document.createElement("div");
        row.className = "cb-row cb-custom-row";
        const lbl = document.createElement("span");
        lbl.className = "cb-lbl";
        lbl.textContent = a.name;
        const input = document.createElement("input");
        input.type = "number";
        input.step = "0.1";
        input.value = formatFloat(a.value, 3);
        input.disabled = Boolean(a.locked);
        input.dataset.testid = "cb-attr-" + a.name;
        input.addEventListener("change", () => {
          dispatch({
            action: "update_custom_attribute",
            id: insp.id,
            name: a.name,
            value: Number(input.value),
          });
        });
        const lock = document.createElement("button");
        lock.type = "button";
        lock.className = "cb-lock-btn" + (a.locked ? " is-locked" : "");
        lock.textContent = "L";
        lock.dataset.testid = "cb-attr-lock-" + a.name;
        lock.title = a.locked ? "Unlock Attribute" : "Lock Attribute";
        lock.addEventListener("click", () => {
          dispatch({
            action: "update_custom_attribute",
            id: insp.id,
            name: a.name,
            locked: !a.locked,
          });
        });
        row.appendChild(lbl);
        row.appendChild(input);
        row.appendChild(lock);
        els.cbCustomList.appendChild(row);
      });
    }
  }

  function dispatchChannelBoxTransform() {
    const insp = currentState && currentState.inspector;
    if (!insp) return;
    const nums = [
      Number(els.cbTx.value),
      Number(els.cbTy.value),
      Number(els.cbTz.value),
      Number(els.cbRx.value),
      Number(els.cbRy.value),
      Number(els.cbRz.value),
      Number(els.cbSx.value),
      Number(els.cbSy.value),
      Number(els.cbSz.value),
    ];
    if (nums.some((v) => !Number.isFinite(v))) return;
    dispatch({
      action: "set_transform",
      id: insp.id,
      position: [nums[0], nums[1], nums[2]],
      rotation_deg: [nums[3], nums[4], nums[5]],
      scale: [nums[6], nums[7], nums[8]],
    });
  }

  function toggleChannelLock(bit) {
    const insp = currentState && currentState.inspector;
    if (!insp) return;
    const mask = Number(insp.locked_channels || 0) ^ (1 << bit);
    dispatch({ action: "set_channel_locks", id: insp.id, mask: mask });
  }

  // =========================================================================
  // LAYER EDITOR
  // =========================================================================
  function renderLayerEditor(state) {
    if (!els.layerList) return;
    const dl = state.display_layers || {};
    const layers = dl.layers || [];
    const activeId = dl.active_layer_id || 0;
    const sig = JSON.stringify([layers, activeId]);
    if (sig === lastLayerSig) return;
    lastLayerSig = sig;
    els.layerList.innerHTML = "";
    layers.forEach((layer) => {
      const row = document.createElement("div");
      row.className = "layer-row" + (layer.id === activeId ? " is-active" : "");
      row.dataset.testid = "layer-row-" + layer.id;

      const stateBtn = document.createElement("button");
      stateBtn.type = "button";
      stateBtn.className = "layer-state-box";
      stateBtn.dataset.testid = "layer-mode-" + layer.id;
      stateBtn.textContent = layer.mode === "template" ? "T" : layer.mode === "reference" ? "R" : "N";
      stateBtn.title = "Cycle Display Mode: Normal / Template / Reference";
      stateBtn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        const next = layer.mode === "normal" ? "template" : layer.mode === "template" ? "reference" : "normal";
        dispatch({ action: "set_display_layer_state", layer_id: layer.id, mode: next });
      });

      const eye = document.createElement("button");
      eye.type = "button";
      eye.className = "layer-state-box";
      eye.dataset.testid = "layer-vis-" + layer.id;
      eye.textContent = layer.visible ? "V" : "-";
      eye.title = layer.visible ? "Hide Layer" : "Show Layer";
      eye.addEventListener("click", (ev) => {
        ev.stopPropagation();
        dispatch({ action: "set_display_layer_state", layer_id: layer.id, visible: !layer.visible });
      });

      const swatch = document.createElement("span");
      swatch.className = "layer-swatch";
      const c = layer.color || [0.34, 0.56, 0.84];
      swatch.style.background = rgbFloatToHex(c);

      const name = document.createElement("span");
      name.className = "layer-name";
      name.textContent = layer.name;
      name.title = "Double-click to rename (defaultLayer is fixed)";
      name.addEventListener("dblclick", (ev) => {
        ev.stopPropagation();
        if (layer.id === 0) return;
        shelfPrompt("Rename Display Layer", "Layer Name", layer.name, (value) => {
          if (value) dispatch({ action: "rename_display_layer", layer_id: layer.id, name: value });
        });
      });

      const count = document.createElement("span");
      count.className = "layer-count";
      count.textContent = String((layer.member_ids || []).length);

      row.appendChild(stateBtn);
      row.appendChild(eye);
      row.appendChild(swatch);
      row.appendChild(name);
      row.appendChild(count);
      row.addEventListener("click", () => {
        dispatch({ action: "select_display_layer", layer_id: layer.id, select_members: false });
      });
      els.layerList.appendChild(row);
    });
  }

  // =========================================================================
  // TIMELINE
  // =========================================================================
  function renderTimeline(state, tl) {
    if (!els.tlScrubber) return;
    const sceneStart = tl.scene_start !== undefined ? tl.scene_start : 1;
    const sceneEnd = tl.scene_end !== undefined ? tl.scene_end : 240;
    const playStart = tl.playback_start !== undefined ? tl.playback_start : 1;
    const playEnd = tl.playback_end !== undefined ? tl.playback_end : 120;
    const cur = Math.round(tl.current_frame || 1);

    els.tlScrubber.min = String(sceneStart);
    els.tlScrubber.max = String(sceneEnd);
    setInputValueIfUnfocused(els.tlScrubber, String(cur));
    setInputValueIfUnfocused(els.tlFrameNum, String(cur));
    setInputValueIfUnfocused(els.tlSceneStart, String(sceneStart));
    setInputValueIfUnfocused(els.tlPlayStart, String(playStart));
    setInputValueIfUnfocused(els.tlPlayEnd, String(playEnd));
    setInputValueIfUnfocused(els.tlSceneEnd, String(sceneEnd));
    if (els.tlFpsSelect && !isFocused(els.tlFpsSelect)) {
      const fps = Math.round(tl.fps || 24);
      els.tlFpsSelect.value = String(fps === 30 || fps === 60 ? fps : 24);
    }
    if (els.tlLoopSelect && !isFocused(els.tlLoopSelect)) els.tlLoopSelect.value = tl.loop_mode || "loop";

    if (els.tlRangeFill) {
      const span = Math.max(1, sceneEnd - sceneStart);
      const left = ((playStart - sceneStart) / span) * 100;
      const width = Math.max(2, ((playEnd - playStart) / span) * 100);
      els.tlRangeFill.style.left = Math.max(0, Math.min(100, left)) + "%";
      els.tlRangeFill.style.width = Math.min(100, width) + "%";
    }

    if (els.tlKeyTicks) {
      els.tlKeyTicks.innerHTML = "";
      const keys = tl.selected_object_keys || [];
      const span = Math.max(1, sceneEnd - sceneStart);
      keys.forEach((k) => {
        const tick = document.createElement("span");
        tick.className = "maya-key-tick";
        tick.style.left = Math.max(0, Math.min(100, ((k - sceneStart) / span) * 100)) + "%";
        tick.title = "Keyframe @ " + k;
        els.tlKeyTicks.appendChild(tick);
      });
    }

    document.querySelectorAll(".cb-row[data-cb-bit]").forEach((row) => {
      const bit = Number(row.dataset.cbBit);
      row.classList.toggle("is-keyed", false || bit === -1);
    });
  }

  // =========================================================================
  // INSPECTOR (Attribute Editor)
  // =========================================================================
  function renderInspector(state, ed) {
    const insp = state.inspector;
    if (!insp) {
      els.inspectorIdBadge.textContent = "None";
      els.inspectorEmpty.hidden = false;
      els.inspectorBody.hidden = true;
      renderAeNodeTabs(null);
      return;
    }

    els.inspectorEmpty.hidden = true;
    els.inspectorBody.hidden = false;
    els.inspectorIdBadge.textContent = "#" + insp.id;

    renderAeNodeTabs(insp);

    setInputValueIfUnfocused(els.inspName, insp.name);
    if (!isFocused(els.inspVisible)) els.inspVisible.checked = Boolean(insp.visible);

    if (!isFocused(els.inspParent)) {
      const parentSig =
        insp.id +
        ":" +
        (state.objects || [])
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
        (state.objects || []).forEach((candidate) => {
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
          noneOpt.textContent = "(Vertex Color)";
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
      els.meshEditSelectionTag.textContent = selCount + " " + ed.mode + (selCount === 1 ? "" : "s");
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
      els.btnMeshAssignMat.disabled = ed.mode !== "face" || selCount === 0 || !state.active_material_id;
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
        const viewingThisLens = Boolean(insp.camera.is_active && state.viewport && state.viewport.camera_view_active);
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
          lt === "point" ? "Point Light Shape" : lt === "spot" ? "Spot Light Shape" : lt === "area" ? "Area Light Shape" : "Directional Light Shape";
      }
      if (els.lightEnabled && !isFocused(els.lightEnabled)) els.lightEnabled.checked = insp.light.enabled !== false;
      if (els.lightType && !isFocused(els.lightType)) els.lightType.value = lt;
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

      if (isRanged && insp.light.range !== undefined) setInputValueIfUnfocused(els.lightRange, formatFloat(insp.light.range, 2));
      if ((isPoint || isSpot) && insp.light.falloff) {
        if (!isFocused(els.lightFalloff)) els.lightFalloff.value = insp.light.falloff;
        if (insp.light.attenuation !== undefined) setInputValueIfUnfocused(els.lightAttenuation, formatFloat(insp.light.attenuation, 2));
      }
      if (isPoint && insp.light.source_radius !== undefined) {
        setInputValueIfUnfocused(els.lightSourceRadius, formatFloat(insp.light.source_radius, 3));
      }
      if (isSpot) {
        if (insp.light.inner_cone_deg !== undefined) setInputValueIfUnfocused(els.lightInnerCone, formatFloat(insp.light.inner_cone_deg, 1));
        if (insp.light.outer_cone_deg !== undefined) setInputValueIfUnfocused(els.lightOuterCone, formatFloat(insp.light.outer_cone_deg, 1));
        if (insp.light.cone_falloff !== undefined) setInputValueIfUnfocused(els.lightConeFalloff, formatFloat(insp.light.cone_falloff, 2));
      }
      if (isArea) {
        const areaShape = insp.light.shape || "rectangle";
        if (els.lightAreaShape && !isFocused(els.lightAreaShape)) els.lightAreaShape.value = areaShape;
        if (els.lblLightAreaWidth) els.lblLightAreaWidth.textContent = areaShape === "disk" ? "Diameter" : "Width";
        if (els.lightRowAreaHeight) els.lightRowAreaHeight.hidden = areaShape === "disk";
        if (insp.light.width !== undefined) setInputValueIfUnfocused(els.lightAreaWidth, formatFloat(insp.light.width, 2));
        if (insp.light.height !== undefined) setInputValueIfUnfocused(els.lightAreaHeight, formatFloat(insp.light.height, 2));
        if (els.lightAreaTwoSided && !isFocused(els.lightAreaTwoSided)) {
          els.lightAreaTwoSided.checked = Boolean(insp.light.two_sided);
        }
        if (els.lightAreaSamples && !isFocused(els.lightAreaSamples) && insp.light.sample_quality) {
          els.lightAreaSamples.value = String(insp.light.sample_quality);
        }
      }

      const sh = insp.light.shadow || {};
      if (els.lightShadowEnabled && !isFocused(els.lightShadowEnabled)) els.lightShadowEnabled.checked = Boolean(sh.enabled);
      if (els.lightShadowQuality && !isFocused(els.lightShadowQuality) && sh.quality) els.lightShadowQuality.value = sh.quality;
      if (els.lightShadowFilter && !isFocused(els.lightShadowFilter) && sh.filter) els.lightShadowFilter.value = sh.filter;
      if (sh.strength !== undefined) setInputValueIfUnfocused(els.lightShadowStrength, formatFloat(sh.strength, 2));
      if (sh.bias !== undefined) setInputValueIfUnfocused(els.lightShadowBias, formatFloat(sh.bias, 4));
      if (sh.normal_bias !== undefined) setInputValueIfUnfocused(els.lightShadowNormalBias, formatFloat(sh.normal_bias, 3));
      if (sh.softness !== undefined) setInputValueIfUnfocused(els.lightShadowSoftness, formatFloat(sh.softness, 2));

      const ll = insp.light.linking || { mode: "all", include_descendants: true, object_ids: [] };
      const linkMode = ll.mode || "all";
      if (els.lightLinkMode && !isFocused(els.lightLinkMode)) els.lightLinkMode.value = linkMode;
      if (els.lightLinkListWrap) els.lightLinkListWrap.hidden = linkMode === "all";
      if (els.lightLinkDescendants && !isFocused(els.lightLinkDescendants)) {
        els.lightLinkDescendants.checked = ll.include_descendants !== false;
      }
      if (els.lightLinkTargets && linkMode !== "all") {
        const eligibleObjs = (state.objects || []).filter(
          (o) => o.id !== insp.id && (o.has_mesh || (!o.has_camera && !o.has_light))
        );
        const rawLinkedIds = ll.object_ids || [];
        const linkedSet = new Set(rawLinkedIds);
        const linkSig = JSON.stringify([insp.id, linkMode, rawLinkedIds, eligibleObjs.map((o) => [o.id, o.name])]);
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
            const sp = document.createElement("span");
            sp.textContent = o.name;
            lbl.appendChild(cb);
            lbl.appendChild(sp);
            cb.addEventListener("change", () => {
              if (!currentState || !currentState.inspector) return;
              dispatch({
                action: "toggle_light_link_target",
                id: currentState.inspector.id,
                target_id: o.id,
                linked: Boolean(cb.checked),
              });
            });
            els.lightLinkTargets.appendChild(lbl);
          });
        }
      }
    } else {
      els.cardLight.hidden = true;
    }

    if (els.cardDataEmpty) els.cardDataEmpty.hidden = Boolean(insp.camera || insp.light);
  }

  function renderAeNodeTabs(insp) {
    if (!els.aeNodeTabs) return;
    const nodes = [];
    if (!insp) {
      els.aeNodeTabs.innerHTML = "";
      lastAeNodeSig = "";
      activeAeNodeId = "";
      applyAeNodeFilter();
      return;
    }
    nodes.push({ id: "transform", label: insp.name || "transform" });
    if (insp.mesh) nodes.push({ id: "shape", label: (insp.mesh.name || "polyShape") });
    if (insp.camera) nodes.push({ id: "shape", label: "cameraShape" });
    if (insp.light) nodes.push({ id: "shape", label: "lightShape" });
    nodes.push({ id: "material", label: "material" });
    nodes.push({ id: "environment", label: "environment" });
    nodes.push({ id: "diagnostics", label: "diagnostics" });

    const sig = JSON.stringify([insp.id, nodes.map((n) => n.label)]);
    if (sig !== lastAeNodeSig) {
      lastAeNodeSig = sig;
      if (!nodes.some((n) => n.id === activeAeNodeId)) activeAeNodeId = "";
      els.aeNodeTabs.innerHTML = "";
      nodes.forEach((n) => {
        const btn = document.createElement("button");
        btn.type = "button";
        btn.className = "ae-node-tab" + (activeAeNodeId === n.id ? " is-active" : "");
        btn.dataset.testid = "ae-node-" + n.id;
        btn.textContent = n.label;
        btn.addEventListener("click", () => {
          activeAeNodeId = activeAeNodeId === n.id ? "" : n.id;
          renderAeNodeTabs(currentState ? currentState.inspector : null);
          applyAeNodeFilter();
        });
        els.aeNodeTabs.appendChild(btn);
      });
    } else {
      Array.from(els.aeNodeTabs.children).forEach((btn) => {
        btn.classList.toggle("is-active", btn.dataset.testid === "ae-node-" + activeAeNodeId);
      });
    }
    applyAeNodeFilter();
  }

  function renderEnvironmentCard(state) {
    const env = state.environment;
    if (!env || !els.envEnabled) return;
    if (!isFocused(els.envEnabled)) els.envEnabled.checked = env.enabled !== false;
    if (els.envHdriStatus) {
      if (env.has_hdri) {
        const hName = env.hdri_name || env.source_path || "HDRI";
        const hW = env.hdri_width || 0;
        const hH = env.hdri_height || 0;
        els.envHdriStatus.textContent = hW > 0 ? hName + " (" + hW + "×" + hH + ")" : hName;
      } else {
        els.envHdriStatus.textContent = "Studio Sky (Fallback)";
      }
    }
    if (els.btnClearHdri) els.btnClearHdri.disabled = !env.has_hdri;
    if (els.envIntensity) {
      setInputValueIfUnfocused(els.envIntensity, Number(env.intensity).toFixed(2));
      if (els.valEnvIntensity) els.valEnvIntensity.textContent = Number(env.intensity).toFixed(2);
    }
    if (els.envRotation) {
      setInputValueIfUnfocused(els.envRotation, String(Math.round(Number(env.rotation_deg || 0))));
      if (els.valEnvRotation) els.valEnvRotation.textContent = Math.round(Number(env.rotation_deg || 0)) + "°";
    }
    if (els.envDiffuseScale) {
      setInputValueIfUnfocused(els.envDiffuseScale, Number(env.diffuse_ibl_scale).toFixed(2));
      if (els.valEnvDiffuse) els.valEnvDiffuse.textContent = Number(env.diffuse_ibl_scale).toFixed(2);
    }
    if (els.envSpecularScale) {
      setInputValueIfUnfocused(els.envSpecularScale, Number(env.specular_ibl_scale).toFixed(2));
      if (els.valEnvSpecular) els.valEnvSpecular.textContent = Number(env.specular_ibl_scale).toFixed(2);
    }
  }

  function renderMaterialEditor(state) {
    if (!els.matSelect) return;
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
    if (!isFocused(els.matBaseColor)) els.matBaseColor.value = rgbFloatToHex(activeMat.base_color);
    setInputValueIfUnfocused(els.matMetallic, activeMat.metallic.toFixed(2));
    els.valMatMetallic.textContent = Number(activeMat.metallic).toFixed(2);
    setInputValueIfUnfocused(els.matRoughness, activeMat.roughness.toFixed(2));
    els.valMatRoughness.textContent = Number(activeMat.roughness).toFixed(2);
    setInputValueIfUnfocused(els.matAo, activeMat.ambient_occlusion.toFixed(2));
    els.valMatAo.textContent = Number(activeMat.ambient_occlusion).toFixed(2);
    if (!isFocused(els.matEmissiveColor)) els.matEmissiveColor.value = rgbFloatToHex(activeMat.emissive_color);
    setInputValueIfUnfocused(els.matEmissiveStrength, formatFloat(activeMat.emissive_strength, 2));
    setInputValueIfUnfocused(els.matOpacity, activeMat.opacity.toFixed(2));
    els.valMatOpacity.textContent = Number(activeMat.opacity).toFixed(2);
  }

  // =========================================================================
  // VIEW NAVIGATION GIZMO
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

    const size = Math.max(64, cssSize || 90);
    const cx = size * 0.5;
    const cy = size * 0.5;
    const sphereR = size * 0.445;
    const axisR = size * 0.355;
    const diagR = size * 0.225;

    function projectDir(d, radius) {
      const vx = d[0] * camRight[0] + d[1] * camRight[1] + d[2] * camRight[2];
      const vy = d[0] * camUp[0] + d[1] * camUp[1] + d[2] * camUp[2];
      const vz = -(d[0] * camFwd[0] + d[1] * camFwd[1] + d[2] * camFwd[2]);
      return { x: cx + vx * radius, y: cy - vy * radius, vz: vz };
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
      return { id: d.id, kind: "diagonal", preset: d.preset, tip: d.tip, x: p.x, y: p.y, vz: p.vz };
    });

    return {
      size,
      cx,
      cy,
      sphereR,
      axisR,
      diagR,
      axes,
      diagonals,
      center: { id: "center", kind: "center", preset: "home", tip: "Home View (Drag to Orbit)", x: cx, y: cy },
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
    if (centerDist <= (isTouch ? 10.0 : 7.8)) return geom.center;
    if (centerDist <= geom.sphereR + 4) return { id: "orbit_sphere", kind: "sphere", preset: null, tip: "Drag to Orbit" };
    return null;
  }

  function getNavGizmoLayout() {
    if (!els.viewNavGizmoCanvas) return null;
    syncFromEngine(true);
    const rect = els.viewNavGizmoCanvas.getBoundingClientRect();
    const cssSize = rect.width > 16 ? rect.width : 90;
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
    const cssSize = Math.round(rect.width > 24 ? rect.width : 90);
    const dpr = Math.max(1, Math.min(3, window.devicePixelRatio || 1));
    const pxSize = Math.round(cssSize * dpr);
    if (cvs.width !== pxSize || cvs.height !== pxSize) {
      cvs.width = pxSize;
      cvs.height = pxSize;
    }

    const geom = computeViewNavGizmoGeometry(state, cssSize);
    const scaleFactor = cssSize / 90;
    ctx.save();
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, cssSize, cssSize);

    const activeSphere = viewGizmoUiState.pointerInside || viewGizmoUiState.dragging;
    const grad = ctx.createRadialGradient(
      geom.cx - geom.sphereR * 0.28,
      geom.cy - geom.sphereR * 0.28,
      geom.sphereR * 0.1,
      geom.cx,
      geom.cy,
      geom.sphereR
    );
    grad.addColorStop(0, activeSphere ? "rgba(58, 74, 96, 0.85)" : "rgba(40, 48, 60, 0.62)");
    grad.addColorStop(1, activeSphere ? "rgba(20, 24, 32, 0.8)" : "rgba(16, 20, 26, 0.55)");
    ctx.beginPath();
    ctx.arc(geom.cx, geom.cy, geom.sphereR, 0, Math.PI * 2);
    ctx.fillStyle = grad;
    ctx.fill();
    ctx.lineWidth = viewGizmoUiState.dragging ? 1.6 : 1.0;
    ctx.strokeStyle = viewGizmoUiState.dragging
      ? "rgba(118, 176, 226, 0.9)"
      : activeSphere
      ? "rgba(170, 200, 236, 0.45)"
      : "rgba(255, 255, 255, 0.16)";
    ctx.stroke();

    for (let i = 0; i < geom.diagonals.length; ++i) {
      const d = geom.diagonals[i];
      if (d.vz <= 0.12) continue;
      const isHov = viewGizmoUiState.hoveredId === d.id;
      ctx.beginPath();
      ctx.arc(d.x, d.y, isHov ? 4.8 : 2.5, 0, Math.PI * 2);
      ctx.fillStyle = isHov ? "#ffe27a" : "rgba(200, 216, 238, 0.36)";
      ctx.fill();
    }

    const sortedAxes = geom.axes.slice().sort((a, b) => a.vz - b.vz);
    const posCapR = 8.4 * scaleFactor;
    const negCapR = 7.0 * scaleFactor;

    for (let i = 0; i < sortedAxes.length; ++i) {
      const a = sortedAxes[i];
      const isHov = viewGizmoUiState.hoveredId === a.id;
      const depthAlpha = a.vz < -0.15 ? 0.55 : 0.96;

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
        const capGrad = ctx.createRadialGradient(a.x - capR * 0.3, a.y - capR * 0.3, capR * 0.1, a.x, a.y, capR);
        capGrad.addColorStop(0, isHov ? "#fff5b8" : a.fillHi);
        capGrad.addColorStop(1, a.color);
        ctx.fillStyle = capGrad;
        ctx.fill();
        if (isHov) {
          ctx.lineWidth = 1.6;
          ctx.strokeStyle = "#ffffff";
          ctx.stroke();
        }
        ctx.fillStyle = "#101318";
        ctx.font = "700 " + Math.round(9.8 * scaleFactor) + "px Inter, system-ui, sans-serif";
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        ctx.fillText(a.label, a.x, a.y + 0.4);
      } else {
        ctx.fillStyle = isHov ? "rgba(44, 56, 72, 0.96)" : "rgba(18, 22, 30, 0.9)";
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

    ["mousedown", "mouseup", "click", "dblclick", "contextmenu", "touchstart", "touchmove", "touchend", "wheel"].forEach(
      (evtName) => {
        pod.addEventListener(evtName, (ev) => {
          ev.stopPropagation();
          if (evtName === "contextmenu") ev.preventDefault();
        });
      }
    );

    function localCoords(ev) {
      const rect = cvs.getBoundingClientRect();
      return {
        x: ev.clientX - rect.left,
        y: ev.clientY - rect.top,
        size: rect.width > 24 ? rect.width : 90,
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
      pod.classList.toggle("is-hover-target", Boolean(hit && hit.preset));
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
        dispatch({ action: "set_viewport_view", view: targetHit.preset, smooth: true });
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
        dispatch({ action: "reset_viewport_view", smooth: true });
        scheduleViewTransitionSync();
      });
    }

    if (els.btnNavProjToggle) {
      els.btnNavProjToggle.addEventListener("pointerdown", (ev) => ev.stopPropagation());
      els.btnNavProjToggle.addEventListener("click", (ev) => {
        ev.stopPropagation();
        if (!wasmApi) return;
        dispatch({ action: "toggle_viewport_projection" });
      });
    }

    window.addEventListener("resize", () => {
      renderViewNavGizmo(currentState);
      setTimeout(() => syncFromEngine(false), 30);
    });
  }

  // =========================================================================
  // TOUCH / POINTER VIEWPORT GESTURES
  // =========================================================================
  function bindViewportTouchAndPointer() {
    if (!canvas) return;
    const activeTouches = new Map();
    const touchState = { mode: "idle", movedBeyondTap: false, lastPinchDist: 0, lastMidX: 0, lastMidY: 0 };

    function canvasRelativeCss(ev) {
      const rect = canvas.getBoundingClientRect();
      return { x: ev.clientX - rect.left, y: ev.clientY - rect.top };
    }

    canvas.addEventListener("pointerdown", (ev) => {
      if (ev.pointerType === "mouse") return;
      if (!wasmApi) return;
      ev.preventDefault();
      try {
        canvas.setPointerCapture(ev.pointerId);
      } catch (_) {}
      const pt = canvasRelativeCss(ev);
      activeTouches.set(ev.pointerId, { startX: pt.x, startY: pt.y, x: pt.x, y: pt.y });
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
        if (!touchState.movedBeyondTap && totalDist < 8) return;
        touchState.movedBeyondTap = true;
        if (touchNavMode === "pan") wasmApi.cameraPan(dx, dy);
        else if (touchNavMode === "zoom") wasmApi.cameraZoom(-dy * 0.04);
        else wasmApi.cameraOrbit(dx, dy);
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
        if (Math.abs(midDx) > 0.2 || Math.abs(midDy) > 0.2) wasmApi.cameraPan(midDx, midDy);
        if (Math.abs(pinchDelta) > 0.5) wasmApi.cameraZoom(pinchDelta * 0.035);
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

      if (!canceled && touchState.mode === "single" && !touchState.movedBeyondTap && activeTouches.size === 0) {
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
  // DOCK SPLITTERS
  // =========================================================================
  function bindDockSplitters() {
    function bindHorizontalSplitter(handle, targetProvider, minPx, maxFn, cssVar) {
      if (!handle) return;
      handle.addEventListener("pointerdown", (ev) => {
        if (ev.button !== 0 && ev.pointerType === "mouse") return;
        ev.preventDefault();
        const target = targetProvider();
        if (!target) return;
        document.body.classList.add("is-resizing-row");
        handle.classList.add("is-dragging");
        try {
          handle.setPointerCapture(ev.pointerId);
        } catch (_) {}
        const startY = ev.clientY;
        const startH = target.getBoundingClientRect().height;

        function onMove(mev) {
          const max = maxFn();
          const next = Math.round(Math.max(minPx, Math.min(max, startH + (mev.clientY - startY))));
          document.documentElement.style.setProperty(cssVar, next + "px");
        }
        function onUp(uev) {
          document.body.classList.remove("is-resizing-row");
          handle.classList.remove("is-dragging");
          handle.removeEventListener("pointermove", onMove);
          handle.removeEventListener("pointerup", onUp);
          handle.removeEventListener("pointercancel", onUp);
          try {
            handle.releasePointerCapture(uev.pointerId);
          } catch (_) {}
        }
        handle.addEventListener("pointermove", onMove);
        handle.addEventListener("pointerup", onUp);
        handle.addEventListener("pointercancel", onUp);
      });
    }

    bindHorizontalSplitter(
      els.splitterBottom,
      () => els.dockBottom,
      60,
      () => Math.max(76, Math.min(360, Math.floor(((window.innerHeight || 720) - 90) * 0.46))),
      "--bottom-dock-h"
    );

    bindHorizontalSplitter(
      els.splitterRightStack,
      () => els.rightDockStack,
      70,
      () => Math.max(80, Math.floor((els.dockRight ? els.dockRight.getBoundingClientRect().height : 600) * 0.8)),
      "--cb-stack-pct"
    );

    if (els.splitterOutliner) {
      els.splitterOutliner.addEventListener("pointerdown", (ev) => {
        if (ev.button !== 0 && ev.pointerType === "mouse") return;
        ev.preventDefault();
        document.body.classList.add("is-resizing-row");
        els.splitterOutliner.classList.add("is-dragging");
        try {
          els.splitterOutliner.setPointerCapture(ev.pointerId);
        } catch (_) {}
        const startY = ev.clientY;
        const dockRect = els.dockLeft.getBoundingClientRect();
        const startH = els.panelOutliner.getBoundingClientRect().height;

        function onMove(mev) {
          const totalH = Math.max(200, dockRect.height);
          const nextPx = startH + (mev.clientY - startY);
          const nextPct = Math.max(18, Math.min(80, (nextPx / totalH) * 100));
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

    function bindVerticalSplitter(handle, cssVar, targetProvider, invert, minPx) {
      if (!handle) return;
      handle.addEventListener("pointerdown", (ev) => {
        if (ev.button !== 0 && ev.pointerType === "mouse") return;
        ev.preventDefault();
        const target = targetProvider();
        if (!target) return;
        document.body.classList.add("is-resizing-col");
        handle.classList.add("is-dragging");
        try {
          handle.setPointerCapture(ev.pointerId);
        } catch (_) {}
        const startX = ev.clientX;
        const startW = target.getBoundingClientRect().width;

        // Keep the central 3D Viewport dominant: the dragged dock may never consume more than
        // 45% of the window, and the opposite dock + tool shelf + splitters are subtracted.
        function viewportPriorityMaxWidth() {
          const w = window.innerWidth || 1280;
          const shelfW = els.toolShelf ? els.toolShelf.getBoundingClientRect().width : 0;
          const minViewport = Math.max(320, Math.floor(w * 0.55));
          const oppositeW =
            cssVar === "--right-dock-w"
              ? (els.dockLeft ? els.dockLeft.getBoundingClientRect().width : 0)
              : (els.dockRight ? els.dockRight.getBoundingClientRect().width : 0);
          return Math.max(minPx, Math.min(520, w - minViewport - shelfW - oppositeW - 8));
        }

        function onMove(mev) {
          const maxW = Math.max(minPx, Math.min(viewportPriorityMaxWidth(), cssVar === "--right-dock-w" ? 520 : 340));
          const delta = mev.clientX - startX;
          const nextW = Math.round(Math.max(minPx, Math.min(maxW, invert ? startW - delta : startW + delta)));
          document.documentElement.style.setProperty(cssVar, nextW + "px");
        }
        function onUp(uev) {
          document.body.classList.remove("is-resizing-col");
          handle.classList.remove("is-dragging");
          handle.removeEventListener("pointermove", onMove);
          handle.removeEventListener("pointerup", onUp);
          handle.removeEventListener("pointercancel", onUp);
          try {
            handle.releasePointerCapture(uev.pointerId);
          } catch (_) {}
          if (wasmApi) {
            const ratio = target.getBoundingClientRect().width / Math.max(1, window.innerWidth);
            if (cssVar === "--right-dock-w") dispatch({ action: "set_dock_ratios", right: ratio });
            else if (cssVar === "--left-dock-w") dispatch({ action: "set_dock_ratios", left: ratio });
            persistWorkspaceToStorage();
          }
        }
        handle.addEventListener("pointermove", onMove);
        handle.addEventListener("pointerup", onUp);
        handle.addEventListener("pointercancel", onUp);
      });
    }

    bindVerticalSplitter(els.splitterRight, "--right-dock-w", () => els.dockRight, true, 212);
    bindVerticalSplitter(els.splitterLeft, "--left-dock-w", () => els.dockLeft, false, 130);
  }

  // =========================================================================
  // WORKSPACE PERSISTENCE
  // =========================================================================
  function persistWorkspaceToStorage() {
    if (!wasmApi) return;
    try {
      const wsJson = wasmApi.exportWorkspaceJson();
      if (wsJson) window.localStorage.setItem(WORKSPACE_STORAGE_KEY, wsJson);
    } catch (_) {}
  }

  function syncWorkspaceTabFromLayoutName(layoutName) {
    if (!layoutName || !REG || !REG.WORKSPACES) return;
    const norm = String(layoutName).trim().toLowerCase();
    const alias =
      norm === "layout" || norm === "maya classic" || norm === "default studio" || norm === "default"
        ? "layout"
        : null;
    const match = alias
      ? REG.WORKSPACES.find((w) => w.id === alias)
      : REG.WORKSPACES.find((w) => w.label.toLowerCase() === norm || w.id.toLowerCase() === norm);
    if (match) {
      activeWorkspaceId = match.id;
      activeBottomEditor = match.bottomEditor || "timeline";
      if (els.workspacePresetSelect) els.workspacePresetSelect.value = match.id;
      if (match.menuSet) applyMenuSet(match.menuSet, false);
      if (match.id === "layout") setInspectorCategory("all");
      else if (match.inspectorTab) setInspectorCategory(match.inspectorTab);
      renderWorkspaceTabBar();
    }
  }

  function applyDockRatiosCss(wsObj) {
    if (!wsObj) return;
    const totalW = window.innerWidth || 1280;
    const totalH = window.innerHeight || 720;
    const shelfW = els.toolShelf ? els.toolShelf.getBoundingClientRect().width : 38;
    const minViewport = Math.max(320, Math.floor(totalW * 0.55));
    if (typeof wsObj.right_dock_width_ratio === "number" && totalW >= 960) {
      const leftW = els.dockLeft ? els.dockLeft.getBoundingClientRect().width : 150;
      const maxRight = Math.max(212, Math.min(520, totalW - minViewport - shelfW - leftW - 8));
      const pxW = Math.round(Math.max(212, Math.min(maxRight, wsObj.right_dock_width_ratio * totalW)));
      document.documentElement.style.setProperty("--right-dock-w", pxW + "px");
    }
    if (typeof wsObj.left_dock_width_ratio === "number" && totalW >= 960) {
      const rightW = els.dockRight ? els.dockRight.getBoundingClientRect().width : 224;
      const maxLeft = Math.max(130, Math.min(340, totalW - minViewport - shelfW - rightW - 8));
      const pxL = Math.round(Math.max(130, Math.min(maxLeft, wsObj.left_dock_width_ratio * totalW)));
      document.documentElement.style.setProperty("--left-dock-w", pxL + "px");
    }
    if (typeof wsObj.bottom_dock_height_ratio === "number" && totalH >= 480) {
      const pxH = Math.round(Math.max(60, Math.min(340, wsObj.bottom_dock_height_ratio * totalH)));
      document.documentElement.style.setProperty("--bottom-dock-h", pxH + "px");
    }
  }

  function restoreWorkspaceFromStorage() {
    if (!wasmApi) return false;
    try {
      const saved = window.localStorage.getItem(WORKSPACE_STORAGE_KEY);
      if (saved && saved.trim().startsWith("{")) {
        const parsed = JSON.parse(saved);
        const wsObj = parsed.workspace || parsed;
        syncWorkspaceTabFromLayoutName(wsObj.layout_name);
        dispatch({ action: "import_workspace_json", json_text: saved });
        applyDockRatiosCss(wsObj);
        return true;
      }
    } catch (_) {}
    return false;
  }

  function saveNamedCustomWorkspace() {
    if (!wasmApi) return;
    const wsJson = wasmApi.exportWorkspaceJson();
    if (wsJson && REG.saveCustomWorkspaceLayout(wsJson)) {
      persistWorkspaceToStorage();
      setStatusMessage("Saved custom workspace layout (scene data untouched)", false);
    }
  }

  function restoreNamedCustomWorkspace() {
    if (!wasmApi) return;
    const saved = REG.loadCustomWorkspaceLayout();
    if (!saved) {
      setStatusMessage("No custom workspace layout saved yet.", false);
      return;
    }
    try {
      const parsed = JSON.parse(saved);
      const wsObj = parsed.workspace || parsed;
      isRightDockCollapsed = false;
      syncWorkspaceTabFromLayoutName(wsObj.layout_name);
      dispatch({ action: "import_workspace_json", json_text: saved });
      applyDockRatiosCss(wsObj);
      persistWorkspaceToStorage();
      setStatusMessage("Restored saved custom workspace layout", false);
    } catch (_) {}
  }

  function resetWorkspaceLayout() {
    isRightDockCollapsed = false;
    isLeftDockCollapsed = false;
    setBottomDockCollapsed(false);
    document.documentElement.style.removeProperty("--right-dock-w");
    document.documentElement.style.removeProperty("--left-dock-w");
    document.documentElement.style.removeProperty("--bottom-dock-h");
    document.documentElement.style.removeProperty("--outliner-h-pct");
    document.documentElement.style.removeProperty("--cb-stack-pct");
    setRightDockMode("stacked");
    activateWorkspaceById(activeWorkspaceId, true);
    setStatusMessage("Reset workspace to Maya Classic defaults", false);
  }

  // =========================================================================
  // VIEWPORT CONTEXT (MARKING) MENU
  // =========================================================================
  function openViewportContextMenu(clientX, clientY) {
    if (!els.viewportContextMenu || !els.viewportStage) return;
    closeAllDropdownMenus();

    const ed = currentState && currentState.editor ? currentState.editor : null;
    const insp = currentState && currentState.inspector ? currentState.inspector : null;
    const isMeshMode = Boolean(ed && ed.mode !== "object" && ed.editable_mesh && ed.editable_mesh.editable);

    if (els.ctxMenuTitle) {
      els.ctxMenuTitle.textContent = isMeshMode
        ? ed.mode.charAt(0).toUpperCase() + ed.mode.slice(1) + " Marking Menu"
        : insp
        ? "Object Marking Menu · " + insp.name
        : "Viewport Marking Menu";
    }
    if (els.ctxCmdDuplicate) els.ctxCmdDuplicate.disabled = !insp || isMeshMode;
    if (els.ctxCmdResetTf) els.ctxCmdResetTf.disabled = !insp;
    if (els.ctxCmdDelete) els.ctxCmdDelete.disabled = !insp && !isMeshMode;
    if (els.ctxMeshSep) els.ctxMeshSep.hidden = !isMeshMode;
    if (els.ctxCmdExtrude) {
      els.ctxCmdExtrude.hidden = !isMeshMode;
      els.ctxCmdExtrude.disabled = !isMeshMode || ed.mode !== "face" || (ed.mesh_selection.faces || []).length === 0;
    }
    if (els.ctxCmdBoundary) {
      els.ctxCmdBoundary.hidden = !isMeshMode;
      els.ctxCmdBoundary.disabled = !isMeshMode || ed.editable_mesh.open_boundary_edges < 3;
    }
    if (els.ctxCmdFill) {
      els.ctxCmdFill.hidden = !isMeshMode;
      els.ctxCmdFill.disabled = !isMeshMode || ed.mode !== "vertex" || (ed.mesh_selection.vertices || []).length < 3;
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
    els.viewportContextMenu.style.left = Math.round(Math.max(6, Math.min(maxX, relX))) + "px";
    els.viewportContextMenu.style.top = Math.round(Math.max(6, Math.min(maxY, relY))) + "px";
  }

  // =========================================================================
  // HOTBOX
  // =========================================================================
  function toggleHotbox() {
    hotboxOpen = !hotboxOpen;
    if (els.mayaHotbox) els.mayaHotbox.hidden = !hotboxOpen;
    if (hotboxOpen) setHelpLine("Hotbox open — release Space to close, or click a marked region to execute.");
    else setHelpLine("Select Tool / Move Tool: LMB pick or drag gizmo · Alt+LMB Orbit · Alt+MMB Pan · Alt+RMB Zoom · Hold Space for Hotbox");
  }

  // =========================================================================
  // HOTKEY ENGINE
  // =========================================================================
  function comboFromEvent(ev) {
    const parts = [];
    if (ev.ctrlKey || ev.metaKey) parts.push("Ctrl");
    if (ev.altKey) parts.push("Alt");
    if (ev.shiftKey) parts.push("Shift");
    let key = ev.key;
    if (!key || key === "Unidentified") return "";
    if (key.length === 1) key = key.toUpperCase();
    else if (key === " ") key = "Space";
    parts.push(key);
    return parts.join("+");
  }

  function comboCodes(combo) {
    if (!combo) return [];
    const key = combo.split("+").pop();
    if (!key) return [];
    if (key.length === 1) {
      if (key >= "A" && key <= "Z") return ["Key" + key];
      if (key >= "0" && key <= "9") return ["Digit" + key];
      return [];
    }
    if (key === "Space") return ["Space"];
    return [key];
  }

  function rebuildOverrideMap() {
    overriddenDomCodes = new Set();
    const defaults = {};
    REG.DEFAULT_HOTKEYS.forEach((h) => {
      defaults[h.id] = h.combo;
    });
    hotkeys.forEach((h) => {
      const def = defaults[h.id];
      if (!h.combo || h.combo === def) return;
      comboCodes(h.combo).forEach((code) => overriddenDomCodes.add(code));
    });
    // Maya reserves Space for the Hotbox (rig transport is on the playback strip / Alt+V)
    overriddenDomCodes.add("Space");
  }

  window.__FE3D_IS_KEY_OVERRIDDEN__ = function (domCode) {
    if (!domCode) return false;
    return overriddenDomCodes.has(domCode);
  };

  function hotkeyActionForCombo(combo) {
    if (!combo) return null;
    const hk = hotkeys.find((h) => h.combo === combo);
    return hk ? hk.id : null;
  }

  function handleGlobalHotkey(ev) {
    if (ev.defaultPrevented) return;
    if (document.body.dataset.appView !== "studio") return;
    if (ev.key === "Escape") return;
    const inText = isTextEntryTarget(ev.target);
    const combo = comboFromEvent(ev);
    if (!combo) return;

    const actionId = hotkeyActionForCombo(combo);
    if (!actionId) return;

    const isPlainKey = combo.indexOf("+") === -1;
    if (inText && isPlainKey) return;

    const defaults = {};
    REG.DEFAULT_HOTKEYS.forEach((h) => {
      defaults[h.id] = h.combo;
    });
    const isDefaultCombo = defaults[actionId] === combo;
    // Default combos backed by the C++ session are handled natively by the engine.
    if (isDefaultCombo && !JS_HANDLED_ACTIONS.has(actionId)) return;

    ev.preventDefault();
    ev.stopPropagation();
    executeEditorCommand(actionId);
  }

  function openHotkeyEditor() {
    if (!els.hotkeyModal) return;
    els.hotkeyEditorList.innerHTML = "";
    hotkeys.forEach((hk) => {
      const row = document.createElement("div");
      row.className = "hotkey-row";
      const label = document.createElement("span");
      label.textContent = hk.label + "  ·  " + hk.id;
      const input = document.createElement("input");
      input.type = "text";
      input.value = hk.combo || "";
      input.dataset.testid = "hotkey-input-" + hk.id;
      input.placeholder = "e.g. Ctrl+Shift+K";
      row.appendChild(label);
      row.appendChild(input);
      els.hotkeyEditorList.appendChild(row);
    });
    els.hotkeyModal.hidden = false;
  }

  function commitHotkeyEditor() {
    const rows = els.hotkeyEditorList.querySelectorAll(".hotkey-row");
    const next = [];
    rows.forEach((row, idx) => {
      const input = row.querySelector("input");
      next.push({ id: hotkeys[idx].id, label: hotkeys[idx].label, combo: input.value.trim() });
    });
    hotkeys = next;
    REG.saveHotkeys(hotkeys);
    rebuildOverrideMap();
    els.hotkeyModal.hidden = true;
    setStatusMessage("Saved " + hotkeys.length + " keyboard shortcuts", false);
  }

  // =========================================================================
  // COMMAND LINE
  // =========================================================================
  function runCommandLine() {
    if (!wasmApi || !els.mayaCommandInput) return;
    const text = els.mayaCommandInput.value.trim();
    if (!text) return;
    dispatch({ action: "execute_command_line", command: text });
    syncFromEngine(true);
    if (currentState && currentState.status && currentState.status.message) {
      setStatusMessage(currentState.status.message, currentState.status.level === "error");
    }
    els.mayaCommandInput.value = "";
  }

  // =========================================================================
  // COMMAND SEARCH (Status Line)
  // =========================================================================
  let searchIndexBuilt = false;
  function buildSearchIndex() {
    const items = [];
    Object.keys(COMMAND_LABELS).forEach((cmd) => {
      items.push({ cmd: cmd, label: COMMAND_LABELS[cmd] });
    });
    return items;
  }

  function renderCommandSearch(query) {
    if (!els.commandSearchResults) return;
    const idx = buildSearchIndex();
    const q = (query || "").trim().toLowerCase();
    const matches = (q ? idx.filter((i) => (i.label + " " + i.cmd).toLowerCase().indexOf(q) !== -1) : idx).slice(0, 14);
    els.commandSearchResults.innerHTML = "";
    if (matches.length === 0) {
      els.commandSearchResults.hidden = true;
      return;
    }
    matches.forEach((m) => {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.dataset.testid = "cmd-search-" + m.cmd;
      btn.innerHTML = "<span>" + escapeHtml(m.label) + "</span><kbd>" + escapeHtml(m.cmd) + "</kbd>";
      btn.addEventListener("click", () => {
        executeEditorCommand(m.cmd);
        if (els.commandSearchInput) els.commandSearchInput.value = "";
        closeCommandSearch();
      });
      els.commandSearchResults.appendChild(btn);
    });
    els.commandSearchResults.hidden = false;
  }

  // =========================================================================
  // FLOATING (UNDOCKED) PANELS
  // =========================================================================
  const FLOATING_DEFS = {
    outliner: { slotId: "slot-outliner-left", title: "Outliner" },
    channel_box: { slotId: "slot-channel-box", title: "Channel Box" },
    layer_editor: { slotId: "slot-layer-editor", title: "Display Layers" },
    attribute_editor: { slotId: "slot-attribute-editor", title: "Attribute Editor" },
  };
  const floatingState = {};

  function toggleFloatingPanel(key) {
    const def = FLOATING_DEFS[key];
    if (!def || !els.floatingHost) return;
    const slot = $(def.slotId);
    if (!slot) return;

    if (floatingState[key]) {
      const { win, placeholder } = floatingState[key];
      placeholder.parentNode.insertBefore(slot, placeholder);
      placeholder.remove();
      win.remove();
      delete floatingState[key];
      return;
    }

    const placeholder = document.createElement("div");
    placeholder.style.display = "none";
    slot.parentNode.insertBefore(placeholder, slot);

    const win = document.createElement("div");
    win.className = "floating-maya-window";
    win.dataset.testid = "floating-" + key;
    const bar = document.createElement("div");
    bar.className = "floating-win-titlebar";
    bar.innerHTML = "<span>" + escapeHtml(def.title) + "</span>";
    const closeBtn = document.createElement("button");
    closeBtn.type = "button";
    closeBtn.className = "ghost-btn";
    closeBtn.textContent = "Dock";
    closeBtn.addEventListener("click", () => toggleFloatingPanel(key));
    bar.appendChild(closeBtn);
    const body = document.createElement("div");
    body.className = "floating-win-body";
    body.appendChild(slot);
    win.appendChild(bar);
    win.appendChild(body);

    const offset = Object.keys(floatingState).length * 26;
    win.style.left = 140 + offset + "px";
    win.style.top = 120 + offset + "px";

    let dragX = 0;
    let dragY = 0;
    let dragging = false;
    bar.addEventListener("pointerdown", (ev) => {
      if (ev.target === closeBtn) return;
      dragging = true;
      dragX = ev.clientX - win.offsetLeft;
      dragY = ev.clientY - win.offsetTop;
      try {
        bar.setPointerCapture(ev.pointerId);
      } catch (_) {}
    });
    bar.addEventListener("pointermove", (ev) => {
      if (!dragging) return;
      win.style.left = Math.max(0, ev.clientX - dragX) + "px";
      win.style.top = Math.max(0, ev.clientY - dragY) + "px";
    });
    const stopDrag = () => {
      dragging = false;
    };
    bar.addEventListener("pointerup", stopDrag);
    bar.addEventListener("pointercancel", stopDrag);

    els.floatingHost.appendChild(win);
    floatingState[key] = { win, placeholder };
  }

  // =========================================================================
  // EVENT BINDINGS
  // =========================================================================
  function bindUiEvents() {
    renderStudioSelector();
    renderSelectedStudioHomePanel();
    renderWorkspaceTabBar();
    setInspectorCategory("all");
    buildCommandLabelIndex();
    renderDynamicMenuSet("modeling");
    bindMenuTriggers(document);
    renderShelf();
    rebuildOverrideMap();

    if (els.launcherProjectAuthor) els.launcherProjectAuthor.value = appPrefs.defaultAuthor || "Final Effect 3D";

    // ---------------- Launcher ----------------
    els.btnLauncherCreate.addEventListener("click", () => {
      if (!wasmApi) return;
      const projName = (els.launcherProjectName.value || "").trim() || "Untitled Studio Project";
      const projAuthor = (els.launcherProjectAuthor.value || "").trim() || appPrefs.defaultAuthor || "Final Effect 3D";
      activeProjectRecordId = null;
      dispatch({
        action: "create_new_project",
        template: selectedTemplateId,
        project_name: projName,
        author: projAuthor,
      });
      activateWorkspaceById("layout", false);
      setAppView("studio");
      requestAnimationFrame(() => {
        requestAnimationFrame(() => snapshotActiveProjectToRecent("Created New Project"));
      });
    });

    els.btnLauncherOpen.addEventListener("click", () => {
      els.fileOpenInput.value = "";
      els.fileOpenInput.click();
    });

    els.btnHomeResumeStudio.addEventListener("click", () => setAppView("studio"));

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

    // ---------------- Global document dismissal ----------------
    document.addEventListener("click", (ev) => {
      if (!ev.target.closest || !ev.target.closest(".menu-item")) closeAllDropdownMenus();
      if (!ev.target.closest || !ev.target.closest(".cmd-search-wrap")) closeCommandSearch();
      if (!ev.target.closest || !ev.target.closest("#btn-shelf-gear, #shelf-gear-menu")) {
        if (els.shelfGearMenu) els.shelfGearMenu.hidden = true;
      }
      if (!ev.target.closest || !ev.target.closest("#shelf-item-ctx-menu")) {
        const ctx = $("shelf-item-ctx-menu");
        if (ctx) ctx.hidden = true;
      }
      if (
        els.viewportContextMenu &&
        !els.viewportContextMenu.hidden &&
        !els.viewportContextMenu.contains(ev.target)
      ) {
        closeViewportContextMenu();
      }
      if (outlinerCtxMenuEl && !outlinerCtxMenuEl.hidden && !outlinerCtxMenuEl.contains(ev.target)) {
        closeOutlinerCtxMenu();
      }
    });

    window.addEventListener(
      "keydown",
      (ev) => {
        if (ev.key === "Escape") {
          const hadMenuOpen = Boolean(document.querySelector(".menu-item.is-open"));
          const hadCtxOpen = Boolean(els.viewportContextMenu && !els.viewportContextMenu.hidden);
          const hadModalOpen =
            Boolean(els.settingsModal && !els.settingsModal.hidden) ||
            Boolean(els.hotkeyModal && !els.hotkeyModal.hidden) ||
            Boolean(els.shelfModal && !els.shelfModal.hidden);
          const hadSearch = Boolean(els.commandSearchResults && !els.commandSearchResults.hidden);
          if (hadMenuOpen || hadCtxOpen || hadModalOpen || hadSearch || hotboxOpen) {
            closeAllDropdownMenus();
            closeViewportContextMenu();
            closeOutlinerCtxMenu();
            closeShelfMenus();
            closeCommandSearch();
            closeHotbox();
            if (els.settingsModal) els.settingsModal.hidden = true;
            if (els.hotkeyModal) els.hotkeyModal.hidden = true;
            closeShelfModal();
            ev.preventDefault();
            ev.stopImmediatePropagation();
          }
        }
      },
      true
    );

    // ---------------- Menu set selector ----------------
    if (els.mayaMenuSetSelect) {
      els.mayaMenuSetSelect.addEventListener("change", () => {
        applyMenuSet(els.mayaMenuSetSelect.value, true);
        setStatusMessage("Menu Set: " + els.mayaMenuSetSelect.options[els.mayaMenuSetSelect.selectedIndex].text, false);
      });
    }

    // ---------------- Menubar static commands ----------------
    document.querySelectorAll("[data-menu-cmd]").forEach((btn) => {
      btn.addEventListener("click", () => {
        const cmd = btn.dataset.menuCmd;
        closeAllDropdownMenus();
        if (!wasmApi) return;
        executeEditorCommand(cmd);
      });
    });

    // ---------------- Quick create presets (Status Line & Create menu) ----------------
    document.querySelectorAll("[data-preset]").forEach((btn) => {
      btn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        closeAllDropdownMenus();
        if (!wasmApi) return;
        const preset = btn.dataset.preset;
        if (!preset) return;
        dispatch({ action: "add_preset", preset: preset });
        setHelpLine("Created " + preset.replace(/_/g, " ") + " — press W/E/R to transform, F to frame");
      });
    });

    // ---------------- Panel toggles ----------------
    document.querySelectorAll("[data-panel-toggle]").forEach((btn) => {
      btn.addEventListener("click", () => {
        closeAllDropdownMenus();
        if (!wasmApi || !currentState || !currentState.workspace) return;
        const key = btn.dataset.panelToggle;
        const p = currentState.workspace.panels.find((item) => item.key === key);
        const nextVis = p ? !p.visible : true;
        if (nextVis && (key === "scene_outliner" || key === "properties" || key === "material_editor")) {
          isRightDockCollapsed = false;
          isLeftDockCollapsed = false;
        }
        dispatch({ action: "set_panel_visible", panel: key, visible: nextVis });
        persistWorkspaceToStorage();
      });
    });

    // ---------------- File / project actions ----------------
    els.btnOpen.addEventListener("click", () => {
      els.fileOpenInput.value = "";
      els.fileOpenInput.click();
    });
    els.fileOpenInput.addEventListener("change", () => importSelectedFiles(els.fileOpenInput.files));
    els.btnExportFe3d.addEventListener("click", exportPortableFe3d);
    els.btnExportBundle.addEventListener("click", exportBinaryBundle);

    els.btnUndo.addEventListener("click", () => dispatch({ action: "undo" }));
    els.btnRedo.addEventListener("click", () => dispatch({ action: "redo" }));
    els.btnDuplicate.addEventListener("click", () => dispatch({ action: "duplicate_selected" }));
    els.btnDelete.addEventListener("click", () => dispatch({ action: "delete_selected" }));
    els.btnFrame.addEventListener("click", () => dispatch({ action: "frame_selection" }));
    els.btnClearSelection.addEventListener("click", () => dispatch({ action: "select_object", id: 0 }));
    if (els.btnRenderFrame) {
      els.btnRenderFrame.addEventListener("click", () => executeEditorCommand("render_final_frame"));
    }
    els.btnToggleRig.addEventListener("click", () => executeEditorCommand("toggle_rig"));

    // ---------------- Modals / preferences ----------------
    [els.btnOpenSettings, els.btnStudioSettings].forEach((b) => {
      if (!b) return;
      b.addEventListener("click", () => {
        applyPreferencesAndResponsiveMode();
        els.settingsModal.hidden = false;
      });
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
    if (els.btnPrefOpenHotkeys) els.btnPrefOpenHotkeys.addEventListener("click", openHotkeyEditor);

    if (els.btnCloseHotkeys) els.btnCloseHotkeys.addEventListener("click", () => (els.hotkeyModal.hidden = true));
    if (els.btnSaveHotkeys) els.btnSaveHotkeys.addEventListener("click", commitHotkeyEditor);
    if (els.btnResetHotkeys) {
      els.btnResetHotkeys.addEventListener("click", () => {
        hotkeys = REG.resetDefaultHotkeys();
        rebuildOverrideMap();
        openHotkeyEditor();
        setStatusMessage("Restored default Maya shortcuts", false);
      });
    }
    if (els.btnCloseShelfModal) els.btnCloseShelfModal.addEventListener("click", closeShelfModal);

    // ---------------- Editor mode / gizmo / tools ----------------
    document.querySelectorAll("[data-edit-mode]").forEach((btn) => {
      btn.addEventListener("click", () => dispatch({ action: "set_editor_mode", mode: btn.dataset.editMode }));
    });
    document.querySelectorAll("[data-gizmo-mode]").forEach((btn) => {
      btn.addEventListener("click", () => dispatch({ action: "set_gizmo_mode", mode: btn.dataset.gizmoMode }));
    });
    document.querySelectorAll("[data-maya-tool]").forEach((btn) => {
      btn.addEventListener("click", () => {
        // Move/Rotate/Scale buttons are also [data-gizmo-mode] handles; that handler already
        // routes them to set_gizmo_mode so the engine records the active toolbox tool once.
        if (btn.dataset.gizmoMode) return;
        const tool = btn.dataset.mayaTool;
        if (tool === "component_select") {
          const ed = currentState && currentState.editor;
          dispatch({ action: "set_editor_mode", mode: ed && ed.mode !== "object" ? "object" : "vertex" });
          return;
        }
        if (tool === "multi_component") {
          const masks = (currentState && currentState.editor && currentState.editor.selection_masks) || {};
          dispatch({ action: "set_selection_masks", multi_component_mode: !masks.multi_component_mode });
          return;
        }
        const toolSlug = tool === "pivot_edit" ? "pivot" : tool;
        dispatch({ action: "set_active_tool", tool: toolSlug });
      });
    });
    if (els.btnToolSelect) els.btnToolSelect.addEventListener("click", () => canvas.focus());
    if (els.btnToolLast) els.btnToolLast.addEventListener("click", () => dispatch({ action: "activate_last_tool" }));

    if (els.btnGizmoSpace) {
      els.btnGizmoSpace.addEventListener("click", () => {
        const ed = currentState && currentState.editor;
        const nextSpace = ed && ed.gizmo_space === "local" ? "world" : "local";
        dispatch({ action: "set_gizmo_space", space: nextSpace });
      });
    }
    if (els.tsCoordSpace) {
      els.tsCoordSpace.addEventListener("change", () =>
        dispatch({ action: "set_gizmo_space", space: els.tsCoordSpace.value })
      );
    }
    if (els.tsSnapStep) {
      els.tsSnapStep.addEventListener("change", () => {
        const step = Number(els.tsSnapStep.value);
        if (Number.isFinite(step) && step > 0) dispatch({ action: "set_snap_settings", grid_step: step });
      });
    }
    if (els.btnTsCenterPivot) els.btnTsCenterPivot.addEventListener("click", () => executeEditorCommand("center_pivot"));
    if (els.btnTsResetPivot) els.btnTsResetPivot.addEventListener("click", () => executeEditorCommand("reset_pivot"));
    if (els.btnTsFreeze) els.btnTsFreeze.addEventListener("click", () => executeEditorCommand("freeze_transforms"));
    if (els.btnTsResetTool) {
      els.btnTsResetTool.addEventListener("click", () => {
        dispatch({ action: "set_gizmo_space", space: "world" });
        dispatch({ action: "set_snap_settings", grid: false, point: false, curve: false, surface: false });
        setStatusMessage("Reset active tool settings", false);
      });
    }
    document.querySelectorAll("[data-smooth-lvl]").forEach((btn) => {
      btn.addEventListener("click", () => {
        executeEditorCommand("smooth_" + btn.dataset.smoothLvl);
        document.querySelectorAll("[data-smooth-lvl]").forEach((b) => b.classList.toggle("is-active", b === btn));
      });
    });

    // ---------------- Status line masks / snapping ----------------
    document.querySelectorAll("[data-sel-mask]").forEach((btn) => {
      btn.addEventListener("click", () => {
        const key = btn.dataset.selMask;
        const masks = (currentState && currentState.editor && currentState.editor.selection_masks) || {};
        dispatch({ action: "set_selection_masks", [key]: !masks[key] });
      });
    });
    document.querySelectorAll("[data-snap-toggle]").forEach((btn) => {
      btn.addEventListener("click", () => {
        const key = btn.dataset.snapToggle;
        const snap = (currentState && currentState.editor && currentState.editor.snap) || {};
        dispatch({ action: "set_snap_settings", [key]: !snap[key] });
      });
    });
    function toggleActiveLayerMode(targetMode) {
      const dl = (currentState && currentState.display_layers) || {};
      const layerId = dl.active_layer_id || 0;
      const layer = (dl.layers || []).find((l) => l.id === layerId);
      const currentMode = layer ? layer.mode : "normal";
      dispatch({
        action: "set_display_layer_state",
        layer_id: layerId,
        mode: currentMode === targetMode ? "normal" : targetMode,
      });
    }
    if (els.btnStatusTemplate) els.btnStatusTemplate.addEventListener("click", () => toggleActiveLayerMode("template"));
    if (els.btnStatusReference) els.btnStatusReference.addEventListener("click", () => toggleActiveLayerMode("reference"));
    if (els.btnStatusCb) els.btnStatusCb.addEventListener("click", () => setRightDockMode("cb_only"));
    if (els.btnStatusAe) els.btnStatusAe.addEventListener("click", () => setRightDockMode("ae_only"));
    if (els.btnToggleToolSettings) {
      els.btnToggleToolSettings.addEventListener("click", () => {
        isToolSettingsCollapsed = !isToolSettingsCollapsed;
        if (els.panelToolSettings) els.panelToolSettings.hidden = isToolSettingsCollapsed;
        if (els.splitterOutliner) {
          els.splitterOutliner.hidden = isToolSettingsCollapsed || !els.panelOutliner || els.panelOutliner.hidden;
        }
        els.btnToggleToolSettings.classList.toggle("is-active", !isToolSettingsCollapsed);
      });
    }

    // ---------------- Shelves ----------------
    if (els.btnShelfGear) {
      els.btnShelfGear.addEventListener("click", (ev) => {
        ev.stopPropagation();
        const menu = els.shelfGearMenu;
        if (!menu) return;
        menu.hidden = !menu.hidden;
        els.btnShelfGear.classList.toggle("is-on", !menu.hidden);
      });
    }
    document.querySelectorAll("[data-shelf-op]").forEach((btn) => {
      btn.addEventListener("click", () => {
        const op = btn.dataset.shelfOp;
        if (els.shelfGearMenu) els.shelfGearMenu.hidden = true;
        if (op === "new_shelf") {
          shelfPrompt("New Custom Shelf", "Shelf Name", "My Shelf", (name) => {
            if (!name) return;
            const id = "custom_" + Date.now().toString(36);
            shelfData.push({ id: id, name: name, builtIn: false, items: [] });
            activeShelfId = id;
            persistShelves();
            renderShelf();
            setStatusMessage("Created shelf '" + name + "'", false);
          });
        } else if (op === "rename_shelf") {
          const sh = currentShelf();
          if (!sh) return;
          shelfPrompt("Rename Shelf", "Shelf Name", sh.name, (name) => {
            if (!name || !sh) return;
            sh.name = name;
            persistShelves();
            renderShelf();
          });
        } else if (op === "delete_shelf") {
          const sh = currentShelf();
          if (!sh) return;
          if (sh.builtIn) {
            setStatusMessage("Built-in shelves cannot be deleted (use Restore Default Shelves)", true);
            return;
          }
          shelfData = shelfData.filter((s) => s.id !== sh.id);
          activeShelfId = shelfData[0] ? shelfData[0].id : "general";
          persistShelves();
          renderShelf();
        } else if (op === "add_tool") {
          shelfAddToolDialog();
        } else if (op === "restore_defaults") {
          executeEditorCommand("restore_default_shelves");
        }
      });
    });
    if (els.btnShelfAddItem) els.btnShelfAddItem.addEventListener("click", shelfAddToolDialog);
    document.querySelectorAll("[data-shelf-item-op]").forEach((btn) => {
      btn.addEventListener("click", () => {
        const op = btn.dataset.shelfItemOp;
        const ctx = $("shelf-item-ctx-menu");
        if (ctx) ctx.hidden = true;
        if (op === "exec") {
          if (shelfCtxTarget) executeEditorCommand(shelfCtxTarget.cmd);
        } else if (op === "left") moveShelfItem(-1);
        else if (op === "right") moveShelfItem(1);
        else if (op === "remove") removeShelfItem();
      });
    });

    // ---------------- Toolbox collapse / restore ----------------
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
    document.querySelectorAll("[data-right-mode]").forEach((btn) => {
      btn.addEventListener("click", () => setRightDockMode(btn.dataset.rightMode));
    });
    document.querySelectorAll("[data-float-panel]").forEach((btn) => {
      btn.addEventListener("click", () => toggleFloatingPanel(btn.dataset.floatPanel));
    });

    // ---------------- Workspace tab strip ----------------
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
        if (els.workspaceTabs) els.workspaceTabs.scrollBy({ left: -180, behavior: "smooth" });
      });
    }
    if (els.btnWsScrollRight) {
      els.btnWsScrollRight.addEventListener("click", () => {
        if (els.workspaceTabs) els.workspaceTabs.scrollBy({ left: 180, behavior: "smooth" });
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
    if (els.workspacePresetSelect) {
      els.workspacePresetSelect.addEventListener("change", () =>
        activateWorkspaceById(els.workspacePresetSelect.value, true)
      );
    }

    // ---------------- Viewport panel bar ----------------
    [
      [els.btnVpWireframe, () => executeEditorCommand("shading_wireframe")],
      [els.btnVpShaded, () => executeEditorCommand("shading_shaded")],
      [els.btnVpTextured, () => executeEditorCommand("shading_textured")],
      [els.btnVpWos, () => executeEditorCommand("toggle_wire_on_shaded")],
      [els.btnVpLights, () => executeEditorCommand("shading_lights")],
      [els.btnVpXray, () => executeEditorCommand("toggle_xray")],
      [els.btnVpIsolate, () => executeEditorCommand("toggle_isolate")],
      [els.btnVpGrid, () => executeEditorCommand("toggle_editor_grid")],
      [els.btnVpResGate, () => toggleResolutionGate()],
    ].forEach(([el, fn]) => {
      if (el) el.addEventListener("click", fn);
    });

    // ---------------- RMB viewport context (marking) menu ----------------
    let rmbDownPos = null;
    canvas.addEventListener("pointerdown", (ev) => {
      closeViewportContextMenu();
      closeAllDropdownMenus();
      rmbDownPos = ev.pointerType === "mouse" && ev.button === 2 ? { x: ev.clientX, y: ev.clientY } : null;
    });
    canvas.addEventListener("pointerup", (ev) => {
      if (ev.pointerType === "mouse" && ev.button === 2 && rmbDownPos) {
        const dist = Math.hypot(ev.clientX - rmbDownPos.x, ev.clientY - rmbDownPos.y);
        rmbDownPos = null;
        if (dist < 5) openViewportContextMenu(ev.clientX, ev.clientY);
      }
    });

    document.querySelectorAll("[data-ctx-cmd]").forEach((btn) => {
      btn.addEventListener("click", (ev) => {
        ev.stopPropagation();
        const cmd = btn.dataset.ctxCmd;
        closeViewportContextMenu();
        if (!wasmApi) return;
        switch (cmd) {
          case "frame":
            dispatch({ action: "frame_selection" });
            return;
          case "duplicate":
            dispatch({ action: "duplicate_selected" });
            return;
          case "reset_transform":
            if (currentState && currentState.inspector) {
              dispatch({ action: "reset_transform", id: currentState.inspector.id });
            }
            return;
          case "freeze_transforms":
            executeEditorCommand("freeze_transforms");
            return;
          case "assign_active_mat":
            executeEditorCommand("mat_assign");
            return;
          case "extrude":
            executeEditorCommand("extrude_faces");
            return;
          case "select_boundary":
            executeEditorCommand("select_boundary");
            return;
          case "fill_hole":
            executeEditorCommand("fill_hole");
            return;
          case "delete":
            executeEditorCommand("delete_mesh_selection");
            return;
          case "deselect": {
            const ed = currentState && currentState.editor;
            if (ed && ed.mode !== "object") dispatch({ action: "clear_mesh_selection" });
            else dispatch({ action: "select_object", id: 0 });
            return;
          }
          case "mode_object":
          case "mode_vertex":
          case "mode_edge":
          case "mode_face":
          case "mode_uv":
            executeEditorCommand(cmd);
            return;
          default:
            return;
        }
      });
    });

    // ---------------- Hotbox buttons ----------------
    document.querySelectorAll("[data-hotbox-cmd]").forEach((btn) => {
      btn.addEventListener("click", () => {
        executeEditorCommand(btn.dataset.hotboxCmd);
        closeHotbox();
      });
    });

    // ---------------- Touch nav / multi-select ----------------
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

    // ---------------- Portrait drawers ----------------
    function activatePortraitDrawer(drawerId) {
      activeMobileDrawer = drawerId;
      document.body.dataset.mpDrawer = activeMobileDrawer;
      document.querySelectorAll("[data-mp-drawer]").forEach((b) => {
        b.classList.toggle("is-active", b.dataset.mpDrawer === activeMobileDrawer);
      });
      if (activeMobileDrawer === "inspector") setInspectorCategory("all");
      else if (activeMobileDrawer === "materials") setInspectorCategory("material");
      else if (activeMobileDrawer === "assets") {
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
      btn.addEventListener("click", () => activatePortraitDrawer(btn.dataset.mpDrawer));
    });
    document.querySelectorAll("[data-mp-close-drawer]").forEach((btn) => {
      btn.addEventListener("click", () => activatePortraitDrawer("viewport"));
    });

    // ---------------- Bottom dock ----------------
    els.bottomEditorSelect.addEventListener("change", () => {
      activeBottomEditor = els.bottomEditorSelect.value;
      setBottomDockCollapsed(false);
      if (currentState) renderWorkspaceAndBottomDock(currentState);
    });
    document.querySelectorAll("[data-bottom-tab]").forEach((tab) => {
      tab.addEventListener("click", () => {
        activeBottomEditor = tab.dataset.bottomTab;
        setBottomDockCollapsed(false);
        if (currentState) renderWorkspaceAndBottomDock(currentState);
      });
    });
    els.btnCollapseBottom.addEventListener("click", () => {
      setBottomDockCollapsed(!els.dockBottom.classList.contains("is-collapsed"));
    });
    if (els.btnOpenScriptLog) els.btnOpenScriptLog.addEventListener("click", openScriptLog);

    // ---------------- Timeline ----------------
    if (els.tlScrubber) {
      els.tlScrubber.addEventListener("input", () => {
        const frame = Number(els.tlScrubber.value);
        if (Number.isFinite(frame)) dispatch({ action: "timeline_set_frame", frame: frame });
      });
    }
    if (els.tlFrameNum) {
      els.tlFrameNum.addEventListener("change", () => {
        const frame = Number(els.tlFrameNum.value);
        if (Number.isFinite(frame)) dispatch({ action: "timeline_set_frame", frame: frame });
      });
    }
    if (els.btnTlStart) els.btnTlStart.addEventListener("click", () => executeEditorCommand("timeline_start"));
    if (els.btnTlEnd) els.btnTlEnd.addEventListener("click", () => executeEditorCommand("timeline_end"));
    if (els.btnTlPrevFrame) els.btnTlPrevFrame.addEventListener("click", () => dispatch({ action: "timeline_step", delta: -1 }));
    if (els.btnTlNextFrame) els.btnTlNextFrame.addEventListener("click", () => dispatch({ action: "timeline_step", delta: 1 }));
    if (els.btnTlPrevKey) els.btnTlPrevKey.addEventListener("click", () => executeEditorCommand("jump_prev_key"));
    if (els.btnTlNextKey) els.btnTlNextKey.addEventListener("click", () => executeEditorCommand("jump_next_key"));
    if (els.btnTimelinePlay) els.btnTimelinePlay.addEventListener("click", () => executeEditorCommand("timeline_toggle_play"));
    if (els.btnTlSetKey) els.btnTlSetKey.addEventListener("click", () => dispatch({ action: "timeline_set_key" }));
    if (els.btnTlDelKey) els.btnTlDelKey.addEventListener("click", () => dispatch({ action: "timeline_delete_key" }));
    if (els.btnTlAutokey) {
      els.btnTlAutokey.addEventListener("click", () => {
        const tl = (currentState && currentState.timeline) || {};
        dispatch({ action: "timeline_set_auto_key", enabled: !tl.auto_key });
      });
    }
    [els.tlSceneStart, els.tlPlayStart, els.tlPlayEnd, els.tlSceneEnd].forEach((el) => {
      if (!el) return;
      el.addEventListener("change", () => {
        const ss = Number(els.tlSceneStart.value);
        const ps = Number(els.tlPlayStart.value);
        const pe = Number(els.tlPlayEnd.value);
        const se = Number(els.tlSceneEnd.value);
        if ([ss, ps, pe, se].some((v) => !Number.isFinite(v))) return;
        dispatch({
          action: "timeline_set_ranges",
          scene_start: ss,
          scene_end: se,
          playback_start: ps,
          playback_end: pe,
        });
      });
    });
    if (els.tlFpsSelect) {
      els.tlFpsSelect.addEventListener("change", () =>
        dispatch({ action: "timeline_set_fps", fps: Number(els.tlFpsSelect.value) })
      );
    }
    if (els.tlLoopSelect) {
      els.tlLoopSelect.addEventListener("change", () =>
        dispatch({ action: "timeline_set_loop_mode", mode: els.tlLoopSelect.value })
      );
    }

    // ---------------- Attribute editor rail ----------------
    document.querySelectorAll("[data-insp-tab]").forEach((btn) => {
      btn.addEventListener("click", () => setInspectorCategory(btn.dataset.inspTab));
    });

    // ---------------- Outliner ----------------
    if (els.outlinerFilter) {
      els.outlinerFilter.addEventListener("input", () => {
        outlinerFilterQuery = els.outlinerFilter.value || "";
        lastOutlinerSignature = "";
        if (currentState) renderOutliner(currentState);
      });
    }
    if (els.btnOutlinerGroup) els.btnOutlinerGroup.addEventListener("click", () => executeEditorCommand("group_selected"));
    if (els.btnOutlinerUnparent) {
      els.btnOutlinerUnparent.addEventListener("click", () => executeEditorCommand("unparent_selected"));
    }
    if (els.outlinerRootDrop) {
      els.outlinerRootDrop.addEventListener("dragover", (ev) => {
        ev.preventDefault();
        els.outlinerRootDrop.classList.add("is-drop-target");
      });
      els.outlinerRootDrop.addEventListener("dragleave", () =>
        els.outlinerRootDrop.classList.remove("is-drop-target")
      );
      els.outlinerRootDrop.addEventListener("drop", (ev) => {
        ev.preventDefault();
        els.outlinerRootDrop.classList.remove("is-drop-target");
        const childId = Number(ev.dataTransfer.getData("text/plain"));
        if (Number.isFinite(childId) && childId > 0) {
          dispatch({ action: "reparent_object", child_id: childId, parent_id: 0 });
        }
      });
    }

    // ---------------- Channel Box ----------------
    [els.cbTx, els.cbTy, els.cbTz, els.cbRx, els.cbRy, els.cbRz, els.cbSx, els.cbSy, els.cbSz].forEach((el) => {
      if (!el) return;
      el.addEventListener("change", dispatchChannelBoxTransform);
    });
    if (els.cbVis) {
      els.cbVis.addEventListener("change", () => {
        const insp = currentState && currentState.inspector;
        if (!insp) return;
        dispatch({ action: "set_visibility", id: insp.id, visible: els.cbVis.value === "1" });
      });
    }
    document.querySelectorAll("[data-cb-lock]").forEach((btn) => {
      btn.addEventListener("click", () => toggleChannelLock(Number(btn.dataset.cbLock)));
    });
    if (els.btnCbKey) els.btnCbKey.addEventListener("click", () => dispatch({ action: "timeline_set_key" }));
    if (els.btnCbFreeze) els.btnCbFreeze.addEventListener("click", () => executeEditorCommand("freeze_transforms"));
    if (els.btnCbAddAttr) {
      els.btnCbAddAttr.addEventListener("click", () => {
        const insp = currentState && currentState.inspector;
        if (!insp) {
          setStatusMessage("Select an object before adding a custom attribute", true);
          return;
        }
        shelfPrompt("Add Custom Attribute", "Attribute Name", "customAttr", (name) => {
          if (!name) return;
          dispatch({ action: "add_custom_attribute", id: insp.id, name: name, value: 0 });
        });
      });
    }

    // ---------------- Layer editor ----------------
    if (els.btnLayerCreateSelected) {
      els.btnLayerCreateSelected.addEventListener("click", () =>
        dispatch({ action: "create_display_layer", assign_selected: true })
      );
    }
    if (els.btnLayerCreateEmpty) {
      els.btnLayerCreateEmpty.addEventListener("click", () =>
        dispatch({ action: "create_display_layer", assign_selected: false })
      );
    }
    if (els.btnLayerAssign) {
      els.btnLayerAssign.addEventListener("click", () => {
        const dl = (currentState && currentState.display_layers) || {};
        dispatch({ action: "assign_display_layer", layer_id: dl.active_layer_id || 0 });
      });
    }
    if (els.btnLayerDelete) {
      els.btnLayerDelete.addEventListener("click", () => {
        const dl = (currentState && currentState.display_layers) || {};
        const activeId = dl.active_layer_id || 0;
        if (!activeId) {
          setStatusMessage("defaultLayer cannot be deleted", true);
          return;
        }
        dispatch({ action: "delete_display_layer", layer_id: activeId });
      });
    }

    // ---------------- Inspector inputs ----------------
    els.inspName.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      dispatch({ action: "rename_object", id: currentState.inspector.id, name: els.inspName.value.trim() });
    });
    els.inspVisible.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      dispatch({ action: "set_visibility", id: currentState.inspector.id, visible: els.inspVisible.checked });
    });
    els.inspParent.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      const newParentId = Number(els.inspParent.value) || 0;
      const ok = dispatch({ action: "reparent_object", child_id: currentState.inspector.id, parent_id: newParentId });
      if (!ok && currentState.inspector) els.inspParent.value = String(currentState.inspector.parent_id || 0);
    });
    els.inspMaterial.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      dispatch({
        action: "assign_object_material",
        object_id: currentState.inspector.id,
        material_id: Number(els.inspMaterial.value) || 0,
      });
    });
    els.btnResetTransform.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      dispatch({ action: "reset_transform", id: currentState.inspector.id });
    });
    [els.posX, els.posY, els.posZ, els.rotX, els.rotY, els.rotZ, els.sclX, els.sclY, els.sclZ].forEach((input) => {
      input.addEventListener("change", () => dispatchTransformFromInputs(false));
    });
    [els.camProjection, els.camFov, els.camOrtho, els.camNear, els.camFar].forEach((el) => {
      el.addEventListener("change", dispatchCameraFromInputs);
    });
    if (els.btnToggleCameraView) {
      els.btnToggleCameraView.addEventListener("click", () => {
        if (!wasmApi || !currentState || !currentState.inspector || !currentState.inspector.camera) return;
        const insp = currentState.inspector;
        const vp = currentState.viewport || {};
        if (insp.camera.is_active && vp.camera_view_active) dispatch({ action: "set_camera_view", active: false });
        else {
          if (!insp.camera.is_active) dispatch({ action: "set_active_camera", id: insp.id });
          dispatch({ action: "set_camera_view", active: true });
        }
      });
    }
    els.btnSetActiveCamera.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.inspector) return;
      dispatch({ action: "set_active_camera", id: currentState.inspector.id });
    });
    if (els.lightType) {
      els.lightType.addEventListener("change", () => {
        if (!wasmApi || !currentState || !currentState.inspector || !currentState.inspector.light) return;
        dispatch({ action: "change_light_type", id: currentState.inspector.id, light_type: els.lightType.value });
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
    if (els.lightLinkMode) els.lightLinkMode.addEventListener("change", dispatchLightLinkingFromInputs);
    if (els.lightLinkDescendants) els.lightLinkDescendants.addEventListener("change", dispatchLightLinkingFromInputs);

    if (els.envEnabled) els.envEnabled.addEventListener("change", () => dispatchEnvironmentFromInputs(false));
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
    if (els.btnClearHdri) els.btnClearHdri.addEventListener("click", () => dispatch({ action: "clear_hdri" }));
    document.querySelectorAll("[data-hdri-preset]").forEach((btn) => {
      btn.addEventListener("click", () => dispatch({ action: "load_hdri_preset", preset: btn.dataset.hdriPreset }));
    });

    // ---------------- Mesh card ----------------
    els.btnMeshExtrude.addEventListener("click", () => executeEditorCommand("extrude_faces"));
    els.btnMeshBoundary.addEventListener("click", () => dispatch({ action: "select_boundary_loop" }));
    els.btnMeshFill.addEventListener("click", () => dispatch({ action: "fill_selected_face" }));
    els.btnMeshAssignMat.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.active_material_id) return;
      dispatch({ action: "assign_face_material", material_id: currentState.active_material_id });
    });
    els.btnMeshDelete.addEventListener("click", () => dispatch({ action: "delete_mesh_selection" }));
    document.querySelectorAll("[data-mesh-quick]").forEach((btn) => {
      btn.addEventListener("click", () => {
        const op = btn.dataset.meshQuick;
        if (op === "extrude") executeEditorCommand("extrude_faces");
        else if (op === "boundary") dispatch({ action: "select_boundary_loop" });
        else if (op === "fill") dispatch({ action: "fill_selected_face" });
        else if (op === "delete") dispatch({ action: "delete_mesh_selection" });
      });
    });

    // ---------------- Materials ----------------
    els.matSelect.addEventListener("change", () => {
      if (!wasmApi) return;
      const mid = Number(els.matSelect.value) || 0;
      if (mid > 0) dispatch({ action: "select_material", id: mid });
    });
    els.btnMatNew.addEventListener("click", () => dispatch({ action: "create_material", name: "PBR Material" }));
    els.btnMatDup.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.active_material_id) return;
      dispatch({ action: "duplicate_material", id: currentState.active_material_id });
    });
    els.btnMatDel.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.active_material_id) return;
      dispatch({ action: "delete_material", id: currentState.active_material_id });
    });
    els.matName.addEventListener("change", () => {
      if (!wasmApi || !currentState || !currentState.active_material_id) return;
      dispatch({ action: "rename_material", id: currentState.active_material_id, name: els.matName.value.trim() });
    });
    els.btnMatAssignObj.addEventListener("click", () => {
      if (!wasmApi || !currentState || !currentState.inspector || !currentState.active_material_id) return;
      dispatch({
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

    // ---------------- Metadata ----------------
    [els.metaProjectName, els.metaAuthor, els.metaSceneName].forEach((input) => {
      input.addEventListener("change", dispatchMetadataFromInputs);
    });

    // Enter blurs numeric/text fields so viewport hotkeys re-arm (never while composing)
    document.querySelectorAll('input[type="text"], input[type="number"], input[type="search"]').forEach((input) => {
      input.addEventListener("keydown", (ev) => {
        if (ev.key === "Enter") {
          ev.preventDefault();
          input.blur();
        }
      });
    });

    // ---------------- Command line ----------------
    if (els.btnCommandExec) els.btnCommandExec.addEventListener("click", runCommandLine);
    if (els.mayaCommandInput) {
      els.mayaCommandInput.addEventListener("keydown", (ev) => {
        if (ev.key === "Enter") {
          ev.preventDefault();
          runCommandLine();
        }
      });
    }

    // ---------------- Command search ----------------
    if (els.commandSearchInput) {
      els.commandSearchInput.addEventListener("input", () => renderCommandSearch(els.commandSearchInput.value));
      els.commandSearchInput.addEventListener("focus", () => renderCommandSearch(els.commandSearchInput.value));
      els.commandSearchInput.addEventListener("keydown", (ev) => {
        if (ev.key === "Enter") {
          ev.preventDefault();
          const first = els.commandSearchResults && els.commandSearchResults.querySelector("button");
          if (first) first.click();
        }
      });
    }

    // ---------------- Global keyboard (hotkeys, hotbox) ----------------
    window.addEventListener("keydown", (ev) => {
      if (document.body.dataset.appView !== "studio") return;
      if (ev.code === "Space" && !isTextEntryTarget(ev.target) && !ev.ctrlKey && !ev.altKey && !ev.metaKey) {
        ev.preventDefault();
        if (!spaceHeld) {
          spaceHeld = true;
          toggleHotbox();
        }
        return;
      }
      handleGlobalHotkey(ev);
    });
    window.addEventListener("keyup", (ev) => {
      if (ev.code === "Space") {
        spaceHeld = false;
        if (hotboxOpen) closeHotbox();
      }
    });

    // ---------------- Drag & drop project loading ----------------
    if (els.viewportStage) {
      els.viewportStage.addEventListener("dragover", (ev) => ev.preventDefault());
      els.viewportStage.addEventListener("drop", (ev) => {
        ev.preventDefault();
        if (ev.dataTransfer && ev.dataTransfer.files) importSelectedFiles(ev.dataTransfer.files);
      });
    }

    document.addEventListener("pointermove", (ev) => {
      if (!outlinerDrag.id) return;
      if (!outlinerDrag.moved) {
        if (Math.hypot(ev.clientX - outlinerDrag.x, ev.clientY - outlinerDrag.y) <= 6) return;
        outlinerDrag.moved = true;
      }
      const el = document.elementFromPoint(ev.clientX, ev.clientY);
      const row = el && el.closest ? el.closest(".outliner-row") : null;
      const rootDrop = el && el.closest ? el.closest(".outliner-root-drop") : null;
      document
        .querySelectorAll(".outliner-row.is-drop-target, .outliner-root-drop.is-drop-target")
        .forEach((e2) => e2.classList.remove("is-drop-target"));
      if (row && Number(row.dataset.objId) !== outlinerDrag.id) row.classList.add("is-drop-target");
      else if (rootDrop) rootDrop.classList.add("is-drop-target");
    });
    document.addEventListener("pointerup", (ev) => {
      if (outlinerDrag.id) endOutlinerDrag(ev.clientX, ev.clientY);
    });
    document.addEventListener("pointercancel", () => {
      outlinerDrag.id = 0;
      outlinerDrag.moved = false;
    });

    window.addEventListener("mouseup", () => setTimeout(() => syncFromEngine(false), 20));
    window.addEventListener("keyup", () => setTimeout(() => syncFromEngine(false), 20));

    bindViewportTouchAndPointer();
    bindViewNavGizmo();
    bindDockSplitters();
    setRightDockMode("stacked");
  }

  // =========================================================================
  // WASM BINDING & BOOT LOOP
  // =========================================================================
  function tryBindWasmApi() {
    const M = window.Module;
    if (!M || (!M.__fe3dRuntimeReady && !M.calledRun) || typeof M._fe3d_is_ready !== "function" || typeof M.ccall !== "function") {
      return false;
    }

    if (typeof M._fe3d_get_boot_stage === "function") {
      const stage = M._fe3d_get_boot_stage();
      const msg = typeof M._fe3d_get_boot_message === "function" ? M.ccall("fe3d_get_boot_message", "string", [], []) : "";
      if (stage === 1) setSplashProgress(2, 45, msg);
      else if (stage === 2) setSplashProgress(3, 72, msg);
      else if (stage === 3) setSplashProgress(4, 90, msg);
    }

    const ready = M._fe3d_is_ready();
    if (ready === 0) return false;
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
        const bytes = bytesInput instanceof Uint8Array ? bytesInput : new Uint8Array(bytesInput || []);
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
        return typeof M._fe3d_hit_test_gizmo === "function" ? M._fe3d_hit_test_gizmo(cssX, cssY) === 1 : false;
      },
      touchTap: function (cssX, cssY, additive) {
        return typeof M._fe3d_touch_tap === "function" ? M._fe3d_touch_tap(cssX, cssY, additive ? 1 : 0) === 1 : false;
      },
      pointerEvent: function (kind, button, cssX, cssY, shift, ctrl, alt, meta) {
        if (typeof M._fe3d_pointer_event === "function") {
          M._fe3d_pointer_event(kind, button, cssX, cssY, shift ? 1 : 0, ctrl ? 1 : 0, alt ? 1 : 0, meta ? 1 : 0);
        }
      },
      cameraOrbit: function (dxCss, dyCss) {
        if (typeof M._fe3d_camera_orbit === "function") M._fe3d_camera_orbit(dxCss, dyCss);
      },
      editorCameraOrbit: function (dxCss, dyCss) {
        if (typeof M._fe3d_editor_camera_orbit === "function") M._fe3d_editor_camera_orbit(dxCss, dyCss);
        else this.dispatchAction({ action: "orbit_editor_camera", dx: dxCss, dy: dyCss });
      },
      cameraPan: function (dxCss, dyCss) {
        if (typeof M._fe3d_camera_pan === "function") M._fe3d_camera_pan(dxCss, dyCss);
      },
      cameraZoom: function (steps) {
        if (typeof M._fe3d_camera_zoom === "function") M._fe3d_camera_zoom(steps);
      },
      renderFinalFrame: function () {
        if (typeof M._fe3d_render_final_frame === "function") return M._fe3d_render_final_frame() !== 0;
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
        previewCtx.putImageData(new ImageData(pixels, w, h), 0, 0);
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
      runCommand: function (cmd) {
        wasmApi.dispatchAction({ action: "execute_command_line", command: cmd });
        syncFromEngine(true);
      },
      getShelves: function () {
        return shelfData;
      },
      getHotkeys: function () {
        return hotkeys;
      },
    };

    rebuildOverrideMap();
    const restoredWorkspace = restoreWorkspaceFromStorage();
    syncFromEngine(true);

    // Maya Classic is the default workstation: Menu Bar + Status Line + Shelf + Tool Box +
    // Viewport + Outliner/Tool Settings + Channel Box/Layers/Attribute Editor + Time Slider.
    if (!restoredWorkspace) {
      activateWorkspaceById("layout", false);
      syncFromEngine(true);
    }

    const params = new URLSearchParams(window.location.search || "");
    if (params.get("view") === "studio") setAppView("studio");
    else setAppView("home");
    return true;
  }

  bindUiEvents();

  if (canvas) {
    canvas.addEventListener("contextmenu", (ev) => ev.preventDefault());
  }

  const bootTimer = setInterval(() => {
    if (tryBindWasmApi()) {
      clearInterval(bootTimer);
      setInterval(() => syncFromEngine(false), 60);
    }
  }, 35);
})();
