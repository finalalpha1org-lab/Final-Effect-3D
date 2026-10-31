/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Final Effect 3D — Studio Registry, Maya Shelves, Hotkeys & Persistent Storage (Stage 1 Maya Redesign) */

(function () {
  "use strict";

  const ICONS = {
    logo: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.85" stroke-linecap="round" stroke-linejoin="round"><polygon points="12 2 21 7 21 17 12 22 3 17 3 7"/><polyline points="3 7 12 12 21 7"/><line x1="12" y1="22" x2="12" y2="12"/><circle cx="12" cy="12" r="2.2" fill="currentColor"/></svg>',
    brand: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.85" stroke-linecap="round" stroke-linejoin="round"><polygon points="12 2 21 7 21 17 12 22 3 17 3 7"/><polyline points="3 7 12 12 21 7"/><line x1="12" y1="22" x2="12" y2="12"/><circle cx="12" cy="12" r="2.2" fill="currentColor"/></svg>',
    home: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M2.2 6.8L8 2.2l5.8 4.6v6.2a.8.8 0 0 1-.8.8H3a.8.8 0 0 1-.8-.8V6.8z"/><path d="M6.2 13.8V9.2h3.6v4.6"/></svg>',
    select: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M3.2 2.2l9.2 6.2-4.2.9 2.1 4.1-1.8.9-2.1-4.2-3.2 2.6z"/></svg>',
    lasso: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-dasharray="2.1 1.6"><ellipse cx="8" cy="7.4" rx="5.5" ry="4.4"/><circle cx="5.2" cy="12.6" r="1.4" stroke-dasharray="none"/></svg>',
    multiComp: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><polygon points="8 2.2 13.6 6.2 11.4 13 4.6 13 2.4 6.2"/><circle cx="8" cy="2.2" r="1.4" fill="currentColor"/><circle cx="13.6" cy="6.2" r="1.4" fill="currentColor"/><circle cx="2.4" cy="6.2" r="1.4" fill="currentColor"/></svg>',
    move: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M8 1.8v12.4M1.8 8h12.4M8 1.8L6.2 3.6M8 1.8l1.8 1.8M8 14.2l-1.8-1.8M8 14.2l1.8-1.8M1.8 8l1.8-1.8M1.8 8l1.8 1.8M14.2 8l-1.8-1.8M14.2 8l-1.8 1.8"/></svg>',
    rotate: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M13.2 8a5.2 5.2 0 1 1-1.5-3.7"/><polyline points="13.4 2.2 13.4 5.2 10.4 5.2"/></svg>',
    scale: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><rect x="2.2" y="7.8" width="6" height="6" rx="0.6"/><path d="M8.5 7.5L13.6 2.4M10.2 2.4h3.4v3.4"/></svg>',
    universal: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.35"><rect x="3" y="3" width="10" height="10" rx="1"/><circle cx="8" cy="8" r="3.2"/><line x1="8" y1="1.2" x2="8" y2="4.2"/><line x1="14.8" y1="8" x2="11.8" y2="8"/></svg>',
    manip: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.45" stroke-linecap="round"><circle cx="8" cy="8" r="2.2"/><line x1="8" y1="1.6" x2="8" y2="5.8"/><line x1="14.2" y1="8" x2="10.2" y2="8"/><line x1="3.4" y1="12.6" x2="6.4" y2="9.6"/></svg>',
    pivotEdit: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.45" stroke-linecap="round"><circle cx="8" cy="8" r="3.4"/><circle cx="8" cy="8" r="1" fill="currentColor"/><line x1="8" y1="1.4" x2="8" y2="4"/><line x1="8" y1="12" x2="8" y2="14.6"/><line x1="1.4" y1="8" x2="4" y2="8"/><line x1="12" y1="8" x2="14.6" y2="8"/></svg>',
    cube: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><polygon points="8 1.8 13.6 5 13.6 11 8 14.2 2.4 11 2.4 5"/><polyline points="2.4 5 8 8.2 13.6 5"/><line x1="8" y1="14.2" x2="8" y2="8.2"/></svg>',
    sphere: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="5.8"/><ellipse cx="8" cy="8" rx="5.8" ry="2.3"/><ellipse cx="8" cy="8" rx="2.3" ry="5.8"/></svg>',
    cylinder: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><ellipse cx="8" cy="3.6" rx="4.8" ry="1.8"/><path d="M3.2 3.6v8.8c0 1 2.15 1.8 4.8 1.8s4.8-.8 4.8-1.8V3.6"/></svg>',
    cone: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linejoin="round"><ellipse cx="8" cy="12.5" rx="5" ry="1.8"/><path d="M3 12.5L8 2l5 10.5"/></svg>',
    torus: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><ellipse cx="8" cy="8.2" rx="6.2" ry="4.2"/><ellipse cx="8" cy="8.2" rx="2.4" ry="1.3"/></svg>',
    pyramid: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><polygon points="8 2 14 12.6 8 14.2 2 12.6"/><line x1="8" y1="2" x2="8" y2="14.2"/></svg>',
    plane: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><polygon points="8 4.2 14.4 8 8 11.8 1.6 8"/><line x1="4.8" y1="6.1" x2="11.2" y2="9.9"/><line x1="11.2" y1="6.1" x2="4.8" y2="9.9"/></svg>',
    camera: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><rect x="1.8" y="4.5" width="8.8" height="7" rx="1"/><polygon points="10.6 6.8 14.2 4.8 14.2 11.2 10.6 9.2"/></svg>',
    light: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="3"/><path d="M8 1.5v2M8 12.5v2M1.5 8h2M12.5 8h2M3.4 3.4l1.4 1.4M11.2 11.2l1.4 1.4M12.6 3.4l-1.4 1.4M4.8 11.2l-1.4 1.4"/></svg>',
    spotLight: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linejoin="round"><circle cx="4" cy="4" r="1.8"/><path d="M5.4 5.2L14 9.2 9.2 14z"/></svg>',
    areaLight: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><rect x="2.5" y="3.5" width="7.5" height="9" rx="0.8"/><line x1="10" y1="6" x2="14" y2="4.8"/><line x1="10" y1="8" x2="14.2" y2="8"/><line x1="10" y1="10" x2="14" y2="11.2"/></svg>',
    pivot: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><line x1="8" y1="2" x2="8" y2="14"/><line x1="2" y1="8" x2="14" y2="8"/><circle cx="8" cy="8" r="2.2"/></svg>',
    group: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><rect x="2" y="3" width="5" height="4.5" rx="0.6"/><rect x="9" y="8.5" width="5" height="4.5" rx="0.6"/><path d="M4.5 7.5v3.2H9"/></svg>',
    vertex: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><line x1="3.5" y1="12.5" x2="8" y2="3.5"/><line x1="8" y1="3.5" x2="12.5" y2="12.5"/><line x1="3.5" y1="12.5" x2="12.5" y2="12.5"/><circle cx="8" cy="3.5" r="2" fill="currentColor"/><circle cx="3.5" cy="12.5" r="1.6"/><circle cx="12.5" cy="12.5" r="1.6"/></svg>',
    edge: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><polygon points="3 12.5 8 3.2 13 12.5" stroke-opacity="0.45"/><line x1="3" y1="12.5" x2="8" y2="3.2" stroke-width="2.4"/></svg>',
    face: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><polygon points="3 4 13 3 12 13 4 12" fill="currentColor" fill-opacity="0.32"/></svg>',
    uv: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.35"><rect x="2.2" y="2.2" width="11.6" height="11.6"/><line x1="8" y1="2.2" x2="8" y2="13.8"/><line x1="2.2" y1="8" x2="13.8" y2="8"/></svg>',
    objectProps: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><rect x="3.2" y="3.2" width="9.6" height="9.6" rx="1"/><path d="M1.5 5h1.7M1.5 11h1.7M12.8 5h1.7M12.8 11h1.7M5 1.5v1.7M11 1.5v1.7M5 12.8v1.7M11 12.8v1.7"/></svg>',
    modifiers: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M10.5 2.5a3.2 3.2 0 0 0-3 4.3L2.6 11.7a1.2 1.2 0 0 0 1.7 1.7l4.9-4.9a3.2 3.2 0 0 0 4.3-3l-2 2-1.8-.4-.4-1.8 2-2z"/></svg>',
    material: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="5.6"/><path d="M8 2.4a5.6 5.6 0 0 1 0 11.2z" fill="currentColor" fill-opacity="0.35"/></svg>',
    sceneData: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="5.6"/><line x1="2.4" y1="8" x2="13.6" y2="8"/><path d="M8 2.4c1.8 1.6 1.8 9.6 0 11.2M8 2.4c-1.8 1.6-1.8 9.6 0 11.2"/></svg>',
    world: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="5.6"/><line x1="2.4" y1="8" x2="13.6" y2="8"/><path d="M8 2.4c1.8 1.6 1.8 9.6 0 11.2M8 2.4c-1.8 1.6-1.8 9.6 0 11.2"/></svg>',
    outliner: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="3.2" cy="3.5" r="1.3"/><line x1="6" y1="3.5" x2="13.5" y2="3.5"/><circle cx="5.2" cy="8" r="1.3"/><line x1="8" y1="8" x2="13.5" y2="8"/><circle cx="5.2" cy="12.5" r="1.3"/><line x1="8" y1="12.5" x2="13.5" y2="12.5"/><path d="M3.2 4.8v7.7h.7"/></svg>',
    timeline: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><rect x="1.6" y="3" width="12.8" height="10" rx="1"/><line x1="8" y1="3" x2="8" y2="13"/><polygon points="6.6 1.6 9.4 1.6 8 3.4" fill="currentColor"/><line x1="4.4" y1="8" x2="6.2" y2="6.2"/><line x1="4.4" y1="8" x2="6.2" y2="9.8"/></svg>',
    assets: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><rect x="2" y="2.2" width="5" height="5" rx="0.8"/><rect x="9" y="2.2" width="5" height="5" rx="0.8"/><rect x="2" y="9.2" width="5" height="5" rx="0.8"/><rect x="9" y="9.2" width="5" height="5" rx="0.8"/></svg>',
    console: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><rect x="1.6" y="2.4" width="12.8" height="11.2" rx="1"/><polyline points="4.2 6 6.8 8 4.2 10"/><line x1="7.8" y1="10.2" x2="11.4" y2="10.2"/></svg>',
    undo: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M4.2 6.2h5.2a3.4 3.4 0 1 1 0 6.8H6.5"/><polyline points="6.5 3.8 3.8 6.2 6.5 8.6"/></svg>',
    redo: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M11.8 6.2H6.6a3.4 3.4 0 1 0 0 6.8h2.9"/><polyline points="9.5 3.8 12.2 6.2 9.5 8.6"/></svg>',
    save: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M2.5 2.2h8.8l2.2 2.2v9.1a.8.8 0 0 1-.8.8H2.5a.8.8 0 0 1-.8-.8V3a.8.8 0 0 1 .8-.8z"/><rect x="4.6" y="2.2" width="5.4" height="3.6"/><rect x="4.2" y="8.8" width="7.6" height="5.4"/></svg>',
    folder: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M1.8 4a1 1 0 0 1 1-1h3.4l1.5 1.7h5.5a1 1 0 0 1 1 1v6.8a1 1 0 0 1-1 1H2.8a1 1 0 0 1-1-1V4z"/></svg>',
    eye: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M1.5 8s2.4-4.2 6.5-4.2S14.5 8 14.5 8s-2.4 4.2-6.5 4.2S1.5 8 1.5 8z"/><circle cx="8" cy="8" r="1.8"/></svg>',
    eyeOff: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M2.2 2.2l11.6 11.6M4.2 5C2.5 6.3 1.5 8 1.5 8s2.4 4.2 6.5 4.2c1.4 0 2.6-.5 3.6-1.1M6.8 4c.4-.1.8-.2 1.2-.2 4.1 0 6.5 4.2 6.5 4.2a11 11 0 0 1-1.9 2.4"/></svg>',
    lock: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.45" stroke-linejoin="round"><rect x="3.2" y="7.2" width="9.6" height="6.6" rx="1"/><path d="M5.2 7.2V5a2.8 2.8 0 0 1 5.6 0v2.2"/></svg>',
    unlock: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.45" stroke-linejoin="round"><rect x="3.2" y="7.2" width="9.6" height="6.6" rx="1"/><path d="M5.2 7.2V4.8a2.8 2.8 0 0 1 5.2-1.4"/></svg>',
    settings: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="2.2"/><path d="M8 1.6v1.8M8 12.6v1.8M1.6 8h1.8M12.6 8h1.8M3.5 3.5l1.3 1.3M11.2 11.2l1.3 1.3M12.5 3.5l-1.3 1.3M4.8 11.2l-1.3 1.3"/></svg>',
    orbit: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="5.5" stroke-dasharray="2.2 1.4"/><ellipse cx="8" cy="8" rx="6.2" ry="2.4" transform="rotate(-24 8 8)"/></svg>',
    pan: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M8 2v12M2 8h12M6 3.8L8 1.8l2 2M6 12.2l2 2 2-2M3.8 6L1.8 8l2 2M12.2 6l2 2-2 2"/></svg>',
    zoom: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="7" cy="7" r="4.6"/><line x1="10.4" y1="10.4" x2="14" y2="14"/><line x1="5" y1="7" x2="9" y2="7"/><line x1="7" y1="5" x2="7" y2="9"/></svg>',
    frame: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M2.2 5.4V2.2h3.2M10.6 2.2h3.2v3.2M13.8 10.6v3.2h-3.2M5.4 13.8H2.2v-3.2"/><circle cx="8" cy="8" r="2"/></svg>',
    play: '<svg viewBox="0 0 16 16" fill="currentColor"><polygon points="4.2 2.6 13.2 8 4.2 13.4"/></svg>',
    pause: '<svg viewBox="0 0 16 16" fill="currentColor"><rect x="3.8" y="2.8" width="3" height="10.4" rx="0.5"/><rect x="9.2" y="2.8" width="3" height="10.4" rx="0.5"/></svg>',
    plus: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"><line x1="8" y1="2.8" x2="8" y2="13.2"/><line x1="2.8" y1="8" x2="13.2" y2="8"/></svg>',
    trash: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.45" stroke-linecap="round"><line x1="2.5" y1="4.2" x2="13.5" y2="4.2"/><path d="M5.5 4.2V2.6h5v1.6M4.2 4.2l.7 9.2h6.2l.7-9.2"/></svg>',
    snapGrid: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.35"><rect x="2" y="2" width="12" height="12"/><line x1="6" y1="2" x2="6" y2="14"/><line x1="10" y1="2" x2="10" y2="14"/><line x1="2" y1="6" x2="14" y2="6"/><line x1="2" y1="10" x2="14" y2="10"/><circle cx="10" cy="6" r="1.8" fill="currentColor"/></svg>',
    snapPoint: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M4 11V6a4 4 0 0 1 8 0v5"/><line x1="3" y1="11" x2="6" y2="11"/><line x1="10" y1="11" x2="13" y2="11"/><circle cx="8" cy="13.4" r="1.4" fill="currentColor"/></svg>',
    snapCurve: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.45" stroke-linecap="round"><path d="M2.2 12.6C4 4.2 12 11.8 13.8 3.4"/><circle cx="8" cy="8" r="2" fill="currentColor"/></svg>',
    snapSurface: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.35"><path d="M1.8 10.5Q8 5.2 14.2 10.5"/><path d="M3.5 13Q8 8.2 12.5 13"/><circle cx="8" cy="6.8" r="1.8" fill="currentColor"/></svg>',
    wireframe: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><polygon points="8 1.8 13.8 5 13.8 11 8 14.2 2.2 11 2.2 5"/><line x1="2.2" y1="5" x2="13.8" y2="11"/><line x1="13.8" y1="5" x2="2.2" y2="11"/><line x1="8" y1="1.8" x2="8" y2="14.2"/></svg>',
    shaded: '<svg viewBox="0 0 16 16" fill="currentColor"><circle cx="8" cy="8" r="5.8" fill-opacity="0.72" stroke="currentColor" stroke-width="1.2"/></svg>',
    textured: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><circle cx="8" cy="8" r="5.8"/><path d="M8 2.2v11.6M2.2 8h11.6"/><path d="M8 2.2A5.8 5.8 0 0 1 13.8 8H8z" fill="currentColor" fill-opacity="0.55"/><path d="M2.2 8A5.8 5.8 0 0 0 8 13.8V8z" fill="currentColor" fill-opacity="0.55"/></svg>',
    xray: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-dasharray="2 1.4"><rect x="2.4" y="4.4" width="7.6" height="7.6"/><rect x="6" y="2.4" width="7.6" height="7.6" stroke-dasharray="none"/></svg>',
    isolate: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><rect x="5" y="5" width="6" height="6" fill="currentColor" fill-opacity="0.35"/><path d="M2 5V2h3M11 2h3v3M14 11v3h-3M5 14H2v-3"/></svg>',
    layers: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linejoin="round"><polygon points="8 2 14.2 5.4 8 8.8 1.8 5.4"/><polyline points="1.8 8.4 8 11.8 14.2 8.4"/><polyline points="1.8 11.2 8 14.4 14.2 11.2"/></svg>',
    keyframe: '<svg viewBox="0 0 16 16" fill="currentColor"><polygon points="8 2 13.5 8 8 14 2.5 8"/></svg>',
    floatWin: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.35"><rect x="4.8" y="2.2" width="9" height="8.2" rx="0.8"/><path d="M11 10.4v3.2H2.2V5.4h2.6"/></svg>',
    search: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.45" stroke-linecap="round"><circle cx="7" cy="7" r="4.4"/><line x1="10.4" y1="10.4" x2="13.8" y2="13.8"/></svg>',
    image: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.45"><rect x="2" y="2.5" width="12" height="11" rx="1.2"/><circle cx="5.6" cy="6" r="1.2"/><path d="M2.4 12l3.6-3.4 2.6 2.4 2.2-2 2.8 3"/></svg>',
    vfx: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.45" stroke-linecap="round"><path d="M8 1.5v3M8 11.5v3M1.5 8h3M11.5 8h3M3.4 3.4l2.1 2.1M10.5 10.5l2.1 2.1M12.6 3.4l-2.1 2.1M5.5 10.5l-2.1 2.1"/><circle cx="8" cy="8" r="2"/></svg>',
    landscape: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><polygon points="1.5 13 6 5.2 9.5 10.4 11.8 7.5 14.5 13"/></svg>',
    character: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="3.6" r="1.9"/><path d="M3.2 6.8h9.6M8 5.6v4.6M8 10.2l-2.6 4.2M8 10.2l2.6 4.2"/></svg>',
    draw2d: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M2.5 13.5l2.8-.6 7.6-7.6a1.4 1.4 0 0 0-2-2L3.3 10.9l-.8 2.6z"/></svg>',
    sculpt: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="7.4" cy="8.6" r="4.8"/><path d="M10.5 2.2l3.3 3.3-3.2 3.2"/></svg>',
    shade: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="5.2" cy="5.2" r="2.4"/><circle cx="11.2" cy="10.8" r="2.4"/><path d="M7.6 5.2h1.2v5.6h1"/></svg>',
  };

  // Extensible Studio Registry — Preserves all 6 suite studio IDs & honest planned badges
  const STUDIOS = [
    {
      id: "3d_studio",
      name: "3D Studio",
      shortName: "3D Studio",
      icon: "cube",
      operational: true,
      badge: "OPERATIONAL · v0.5.0",
      tagline: "Maya-Style Scene Graph, Polygon Modeling, PBR Shading, Lighting & Keyframe Animation",
      description:
        "Full Autodesk Maya-inspired C++20 WebGL 2.0 3D authoring studio with Menu Sets, Status Line, Customizable Shelves, Toolbox, Outliner, Channel Box, Display Layers, Tabbed Attribute Editor, Real Keyframe Timeline, Hotbox, CommandStack Undo/Redo, and .fe3d/.fe3dbin v1.3 persistence.",
      templates: [
        {
          id: "starter",
          name: "Studio Showcase",
          subtitle: "Hierarchical rig, HeroCube, AccentPyramid, Camera, Light & 3 PBR materials",
          icon: "cube",
        },
        {
          id: "minimal_cube",
          name: "General 3D Scene",
          subtitle: "Clean startup cube, Studio Clay PBR material, camera & key light",
          icon: "objectProps",
        },
        {
          id: "shader_ball",
          name: "PBR LookDev Sphere",
          subtitle: "UV shader sphere + Gold, Chrome & Ceramic materials",
          icon: "material",
        },
        {
          id: "empty_stage",
          name: "Empty Stage",
          subtitle: "Calibrated MainCamera & SunLight ready for scratch modeling",
          icon: "camera",
        },
      ],
    },
    {
      id: "landscaping_studio",
      name: "Landscaping Studio",
      shortName: "Landscaping",
      icon: "landscape",
      operational: false,
      badge: "PLANNED · FUTURE STAGE",
      tagline: "Procedural Heightfield Terrain, Erosion & Environment Scattering Architecture",
      description:
        "Landscaping Studio is a planned creative module in the Final Effect 3D multi-studio architecture. Terrain clipmaps, hydraulic/thermal erosion operators, biome material splatting, and instanced foliage scattering are not yet implemented in v0.5.0.",
      plannedCapabilities: [
        "Chunked LOD heightfield terrain meshes integrated with finaleffect::scene",
        "Non-destructive procedural noise & erosion height layers",
        "Slope/altitude PBR material blending & GPU foliage instancing",
      ],
    },
    {
      id: "character_studio",
      name: "Character Studio",
      shortName: "Character",
      icon: "character",
      operational: false,
      badge: "PLANNED · FUTURE STAGE",
      tagline: "Skeletal Armatures, Linear-Blend/Dual-Quat Skinning & Rigging Constraints",
      description:
        "Character Studio is reserved for skeletal rigging, joint hierarchies, skin weight painting, inverse kinematics, and pose libraries. No fake bone or weight tools are exposed until the C++20 armature evaluator lands.",
      plannedCapabilities: [
        "Typed BoneId / SkeletonAsset hierarchy inside finaleffect::scene",
        "GPU vertex skinning & interactive weight visualization",
        "IK/FK constraint solver & pose keyframe integration",
      ],
    },
    {
      id: "studio_2d",
      name: "2D Studio",
      shortName: "2D Studio",
      icon: "draw2d",
      operational: false,
      badge: "PLANNED · FUTURE STAGE",
      tagline: "2D Stroke Layers, Cel Animation & 2D-in-3D Viewport Canvas",
      description:
        "2D Studio is an architectural expansion slot for vector stroke drawing, onion-skinned cel timelines, and hybrid 2D/3D camera planes. This studio is intentionally non-interactive in v0.5.0.",
      plannedCapabilities: [
        "Pressure/tilt-aware stroke curves via Pointer Events",
        "Frame-by-frame cel layers & onion-skinning renderer pass",
        "True 3D camera parallax integration with 3D Studio scenes",
      ],
    },
    {
      id: "sculpting_studio",
      name: "Sculpting Studio",
      shortName: "Sculpting",
      icon: "sculpt",
      operational: false,
      badge: "PLANNED · FUTURE STAGE",
      tagline: "Spatial BVH Brush Evaluation, Clay/Crease/Smooth & Dynamic Topology",
      description:
        "Sculpting Studio is reserved for high-resolution digital sculpting. While polygon mesh editing (Vertex/Edge/Face extrude, dissolve, fill) is active today in 3D Studio, brush-based sculpting is scheduled for a future engine stage.",
      plannedCapabilities: [
        "Octree/BVH localized vertex brush evaluation in C++20",
        "Symmetry planes, falloff curves & undoable sculpt stroke deltas",
        "Quad/multiresolution remeshing",
      ],
    },
    {
      id: "shade_studio",
      name: "Shade Studio",
      shortName: "Shade Studio",
      icon: "shade",
      operational: false,
      badge: "PLANNED · FUTURE STAGE",
      tagline: "Dedicated Procedural Material Graph & Texture Baking Studio",
      description:
        "Shade Studio will host standalone node-graph shader authoring, procedural noises, and PBR map baking. Note: Direct Metallic-Roughness PBR material authoring & live 3D sphere previews are already fully operational inside 3D Studio (Shading workspace).",
      plannedCapabilities: [
        "DAG shader node compiler targeting GLSL 330 & GLSL ES 3.00",
        "2D albedo/normal/roughness/AO texture channels & IBL environment maps",
        "Procedural curvature/AO texture baker",
      ],
      jumpTo3dWorkspace: "shading",
    },
  ];

  // 12 Workspace Presets (Maya Classic default + Modeling/Shading/Animation/Rendering + Foundation Workspaces)
  const WORKSPACES = [
    {
      id: "layout",
      label: "Maya Classic",
      preset: "layout",
      menuSet: "modeling",
      inspectorTab: "all",
      bottomEditor: "timeline",
      showToolShelf: true,
      foundationNotice: null,
    },
    {
      id: "modeling",
      label: "Modeling",
      preset: "modeling",
      menuSet: "modeling",
      inspectorTab: "modifiers",
      bottomEditor: "console",
      showToolShelf: true,
      foundationNotice: null,
    },
    {
      id: "sculpting",
      label: "Sculpting",
      preset: "sculpting",
      menuSet: "modeling",
      inspectorTab: "modifiers",
      bottomEditor: "console",
      showToolShelf: true,
      foundationNotice: {
        title: "Sculpting Workspace Foundation",
        detail:
          "Connected to the shared 3D scene. Brush-based sculpting strokes (Draw, Clay, Smooth, Crease) are planned for a future stage; polygon Vertex/Edge/Face editing remains available.",
      },
    },
    {
      id: "shading",
      label: "Shading",
      preset: "shading",
      menuSet: "rendering",
      inspectorTab: "material",
      bottomEditor: "shader_foundation",
      showToolShelf: true,
      foundationNotice: null,
    },
    {
      id: "animation",
      label: "Animation",
      preset: "animation",
      menuSet: "animation",
      inspectorTab: "object",
      bottomEditor: "timeline",
      showToolShelf: true,
      foundationNotice: null,
    },
    {
      id: "rendering",
      label: "Rendering",
      preset: "rendering",
      menuSet: "rendering",
      inspectorTab: "render",
      bottomEditor: "console",
      showToolShelf: false,
      foundationNotice: null,
    },
    {
      id: "compositing",
      label: "Compositing",
      preset: "compositing",
      menuSet: "rendering",
      inspectorTab: "render",
      bottomEditor: "compositor_foundation",
      showToolShelf: false,
      foundationNotice: {
        title: "Compositing Workspace Foundation",
        detail:
          "Live 3D scene viewport remains connected above. Node-based post-processing & render-layer compositing operators are planned for a future stage.",
      },
    },
    {
      id: "vfx",
      label: "FX",
      preset: "vfx",
      menuSet: "fx",
      inspectorTab: "data",
      bottomEditor: "timeline",
      showToolShelf: true,
      foundationNotice: {
        title: "FX & Motion Tracking Foundation",
        detail:
          "Connected to the active scene camera & timeline. Planar/point camera tracking and particle/fluid solvers are planned for a future stage.",
      },
    },
    {
      id: "geometry_nodes",
      label: "Geometry Nodes",
      preset: "geometry_nodes",
      menuSet: "modeling",
      inspectorTab: "modifiers",
      bottomEditor: "geonodes_foundation",
      showToolShelf: true,
      foundationNotice: {
        title: "Geometry Nodes Workspace Foundation",
        detail:
          "Connected to the shared EditableMesh scene. Procedural geometry evaluation graphs are planned for a future stage.",
      },
    },
    {
      id: "uv_editing",
      label: "UV Editing",
      preset: "uv_editing",
      menuSet: "modeling",
      inspectorTab: "modifiers",
      bottomEditor: "uv_foundation",
      showToolShelf: true,
      foundationNotice: {
        title: "UV Editing Workspace Foundation",
        detail:
          "EditableMesh stores per-corner UV coordinates today. Interactive 2D UV unwrapping, seam marking & island packing are planned for a future stage.",
      },
    },
    {
      id: "animation_2d",
      label: "2D Animation",
      preset: "animation_2d",
      menuSet: "animation",
      inspectorTab: "object",
      bottomEditor: "timeline",
      showToolShelf: true,
      foundationNotice: {
        title: "2D Animation Workspace Foundation",
        detail:
          "Shared 3D camera & scene stage active. 2D stroke layers and cel keyframe drawing tools are planned for a future stage.",
      },
    },
    {
      id: "video_editing",
      label: "Video Editing",
      preset: "video_editing",
      menuSet: "animation",
      inspectorTab: "render",
      bottomEditor: "vse_foundation",
      showToolShelf: false,
      foundationNotice: {
        title: "Video Sequencer Workspace Foundation",
        detail:
          "Scene preview remains connected to the active 3D project. Multi-track NLE video/audio clip sequencing is planned for a future stage.",
      },
    },
  ];

  // Customizable Maya Shelves (persisted independently of scene files)
  const DEFAULT_SHELVES = [
    {
      id: "general",
      name: "General",
      builtIn: true,
      items: [
        { id: "sh_select", cmd: "tool_select", code: "Slct", label: "Select Tool (Q)", icon: "select" },
        { id: "sh_move", cmd: "tool_move", code: "Move", label: "Move Tool (W)", icon: "move" },
        { id: "sh_rotate", cmd: "tool_rotate", code: "Rot", label: "Rotate Tool (E)", icon: "rotate" },
        { id: "sh_scale", cmd: "tool_scale", code: "Scal", label: "Scale Tool (R)", icon: "scale" },
        { id: "sh_cube", cmd: "preset_cube", code: "Cube", label: "Create Polygon Cube", icon: "cube" },
        { id: "sh_sphere", cmd: "preset_sphere", code: "Sphr", label: "Create Polygon Sphere", icon: "sphere" },
        { id: "sh_cyl", cmd: "preset_cylinder", code: "Cyl", label: "Create Polygon Cylinder", icon: "cylinder" },
        { id: "sh_group", cmd: "group_selected", code: "Grp", label: "Group Selected (Ctrl+G)", icon: "group" },
        { id: "sh_freeze", cmd: "freeze_transforms", code: "FrzT", label: "Freeze Transformations", icon: "manip" },
        { id: "sh_cpivot", cmd: "center_pivot", code: "CPvt", label: "Center Pivot", icon: "pivotEdit" },
        { id: "sh_frame", cmd: "frame_selection", code: "FrmS", label: "Frame Selected (F)", icon: "frame" },
      ],
    },
    {
      id: "poly_modeling",
      name: "Poly Modeling",
      builtIn: true,
      items: [
        { id: "pm_cube", cmd: "preset_cube", code: "Cube", label: "Polygon Cube", icon: "cube" },
        { id: "pm_sphere", cmd: "preset_sphere", code: "Sphr", label: "Polygon Sphere", icon: "sphere" },
        { id: "pm_cyl", cmd: "preset_cylinder", code: "Cyl", label: "Polygon Cylinder", icon: "cylinder" },
        { id: "pm_cone", cmd: "preset_cone", code: "Cone", label: "Polygon Cone", icon: "cone" },
        { id: "pm_torus", cmd: "preset_torus", code: "Tors", label: "Polygon Torus", icon: "torus" },
        { id: "pm_plane", cmd: "preset_plane", code: "Plan", label: "Polygon Plane", icon: "plane" },
        { id: "pm_extrude", cmd: "extrude_faces", code: "Extr", label: "Extrude Selected Faces", icon: "face" },
        { id: "pm_bound", cmd: "select_boundary", code: "BndL", label: "Select Boundary Loop", icon: "edge" },
        { id: "pm_fill", cmd: "fill_hole", code: "Fill", label: "Fill Selected Hole", icon: "vertex" },
        { id: "pm_soft", cmd: "normals_soften", code: "Soft", label: "Soften Normals", icon: "shaded" },
        { id: "pm_hard", cmd: "normals_harden", code: "Hard", label: "Harden Facet Normals", icon: "wireframe" },
        { id: "pm_rev", cmd: "normals_reverse", code: "RevN", label: "Reverse Face Normals", icon: "rotate" },
      ],
    },
    {
      id: "sculpting",
      name: "Sculpting",
      builtIn: true,
      items: [
        { id: "sc_sphere", cmd: "preset_sphere", code: "Quad", label: "Create Sculpt Sphere", icon: "sphere" },
        { id: "sc_sm1", cmd: "smooth_1", code: "Sm 1", label: "Base Cage Display (1)", icon: "wireframe" },
        { id: "sc_sm3", cmd: "smooth_3", code: "Sm 3", label: "Smooth Shaded Display (3)", icon: "shaded" },
        { id: "sc_soft", cmd: "normals_soften", code: "Soft", label: "Smooth Surface Normals", icon: "shaded" },
        { id: "sc_xray", cmd: "toggle_xray", code: "XRay", label: "Toggle X-Ray Shading", icon: "xray" },
        { id: "sc_iso", cmd: "toggle_isolate", code: "Isol", label: "Isolate Select (Ctrl+1)", icon: "isolate" },
      ],
    },
    {
      id: "rigging",
      name: "Rigging",
      builtIn: true,
      items: [
        { id: "rg_loc", cmd: "preset_pivot", code: "Jnt", label: "Create Joint / Locator Pivot", icon: "pivot" },
        { id: "rg_grp", cmd: "group_selected", code: "Grp", label: "Create Control Group (Ctrl+G)", icon: "group" },
        { id: "rg_par", cmd: "parent_selected", code: "Prnt", label: "Parent Selected (P)", icon: "outliner" },
        { id: "rg_unp", cmd: "unparent_selected", code: "UnPr", label: "Unparent Selected (Shift+P)", icon: "outliner" },
        { id: "rg_piv", cmd: "tool_pivot", code: "EPvt", label: "Edit Pivot Tool (D)", icon: "pivotEdit" },
        { id: "rg_cpv", cmd: "center_pivot", code: "CPvt", label: "Center Pivot", icon: "pivot" },
        { id: "rg_frz", cmd: "freeze_transforms", code: "FrzT", label: "Freeze Transformations", icon: "manip" },
        { id: "rg_lck", cmd: "toggle_lock_selected", code: "Lock", label: "Lock / Unlock Node", icon: "lock" },
      ],
    },
    {
      id: "animation",
      name: "Animation",
      builtIn: true,
      items: [
        { id: "an_key", cmd: "set_keyframe", code: "SKey", label: "Set Keyframe (S)", icon: "keyframe" },
        { id: "an_dkey", cmd: "delete_keyframe", code: "DKey", label: "Delete Key at Current Frame", icon: "trash" },
        { id: "an_auto", cmd: "toggle_autokey", code: "Auto", label: "Toggle Auto Keyframe", icon: "timeline" },
        { id: "an_play", cmd: "timeline_toggle_play", code: "Play", label: "Play / Pause Timeline", icon: "play" },
        { id: "an_strt", cmd: "timeline_start", code: "|<<", label: "Go to Playback Start", icon: "undo" },
        { id: "an_rig", cmd: "toggle_rig", code: "TRig", label: "Toggle Showcase Turntable Rig", icon: "rotate" },
      ],
    },
    {
      id: "uv_editing",
      name: "UV Editing",
      builtIn: true,
      items: [
        { id: "uv_mode", cmd: "mode_uv", code: "UV", label: "UV Component Mode (F12)", icon: "uv" },
        { id: "uv_face", cmd: "mode_face", code: "Face", label: "Face Selection Mode (F11)", icon: "face" },
        { id: "uv_edge", cmd: "mode_edge", code: "Edge", label: "Edge Seam Mode (F10)", icon: "edge" },
        { id: "uv_tex", cmd: "shading_textured", code: "Tex6", label: "Textured Shading (6)", icon: "textured" },
        { id: "uv_wos", cmd: "toggle_wire_on_shaded", code: "WoS", label: "Wireframe on Shaded", icon: "wireframe" },
      ],
    },
    {
      id: "shading",
      name: "Shading",
      builtIn: true,
      items: [
        { id: "shd_new", cmd: "mat_create", code: "Mat+", label: "Create New PBR Material", icon: "material" },
        { id: "shd_asgn", cmd: "mat_assign", code: "Asgn", label: "Assign Active Material to Selection", icon: "cube" },
        { id: "shd_dup", cmd: "mat_duplicate", code: "DupM", label: "Duplicate Active Material", icon: "plus" },
        { id: "shd_soft", cmd: "hdri_softbox", code: "Soft", label: "Studio Softbox HDRI", icon: "world" },
        { id: "shd_sun", cmd: "hdri_sunset", code: "Suns", label: "Warm Sunset HDRI", icon: "world" },
        { id: "shd_crty", cmd: "hdri_courtyard", code: "Crty", label: "Cool Courtyard HDRI", icon: "world" },
      ],
    },
    {
      id: "rendering",
      name: "Rendering",
      builtIn: true,
      items: [
        { id: "rn_dir", cmd: "preset_directional_light", code: "DirL", label: "Create Directional Light", icon: "light" },
        { id: "rn_pnt", cmd: "preset_point_light", code: "PntL", label: "Create Point Light", icon: "light" },
        { id: "rn_spt", cmd: "preset_spot_light", code: "SptL", label: "Create Spot Light", icon: "spotLight" },
        { id: "rn_area", cmd: "preset_area_light", code: "Area", label: "Create Area Light", icon: "areaLight" },
        { id: "rn_cam", cmd: "preset_camera", code: "Cam+", label: "Create Camera", icon: "camera" },
        { id: "rn_l7", cmd: "shading_lights", code: "Lgt7", label: "Use All Lights (7)", icon: "light" },
        { id: "rn_lens", cmd: "toggle_camera_view", code: "Lens", label: "Look Through Active Camera", icon: "camera" },
        { id: "rn_rndr", cmd: "render_final_frame", code: "Rndr", label: "Render Final Frame", icon: "image" },
      ],
    },
    {
      id: "fx",
      name: "FX",
      builtIn: true,
      items: [
        { id: "fx_sph", cmd: "preset_sphere", code: "Emit", label: "Create Emitter Sphere", icon: "vfx" },
        { id: "fx_pnt", cmd: "preset_point_light", code: "Glow", label: "Create Point Glow Light", icon: "light" },
        { id: "fx_rig", cmd: "toggle_rig", code: "Sim", label: "Play / Pause Turntable Simulation", icon: "play" },
        { id: "fx_fall", cmd: "frame_all", code: "FitA", label: "Frame All FX Bounds (A)", icon: "isolate" },
      ],
    },
  ];

  // Customizable Maya Shortcut Map
  const DEFAULT_HOTKEYS = [
    { id: "tool_select", label: "Select Tool", combo: "Q" },
    { id: "tool_move", label: "Move Tool", combo: "W" },
    { id: "tool_rotate", label: "Rotate Tool", combo: "E" },
    { id: "tool_scale", label: "Scale Tool", combo: "R" },
    { id: "tool_show_manip", label: "Show Manipulator", combo: "T" },
    { id: "tool_last", label: "Last Used Tool", combo: "Y" },
    { id: "tool_pivot", label: "Edit Pivot Mode", combo: "D" },
    { id: "frame_selection", label: "Frame Selected", combo: "F" },
    { id: "frame_all", label: "Frame All", combo: "A" },
    { id: "set_keyframe", label: "Set Keyframe", combo: "S" },
    { id: "duplicate_selected", label: "Duplicate Selected", combo: "Ctrl+D" },
    { id: "group_selected", label: "Group Selected", combo: "Ctrl+G" },
    { id: "parent_selected", label: "Parent Selected", combo: "P" },
    { id: "unparent_selected", label: "Unparent Selected", combo: "Shift+P" },
    { id: "save_fe3d", label: "Save Portable .fe3d", combo: "Ctrl+S" },
    { id: "undo", label: "Undo", combo: "Ctrl+Z" },
    { id: "redo", label: "Redo", combo: "Ctrl+Y" },
    { id: "shading_wireframe", label: "Wireframe Display", combo: "4" },
    { id: "shading_shaded", label: "Smooth Shaded Display", combo: "5" },
    { id: "shading_textured", label: "Textured / Material Display", combo: "6" },
    { id: "shading_lights", label: "Use All Scene Lights", combo: "7" },
    { id: "toggle_isolate", label: "Isolate Select", combo: "Ctrl+1" },
    { id: "mode_object_toggle", label: "Toggle Object / Component", combo: "F8" },
    { id: "mode_vertex", label: "Vertex Component Mode", combo: "F9" },
    { id: "mode_edge", label: "Edge Component Mode", combo: "F10" },
    { id: "mode_face", label: "Face Component Mode", combo: "F11" },
    { id: "mode_uv", label: "UV Component Mode", combo: "F12" },
  ];

  // Real Recent Projects & Preferences Store (IndexedDB payload + localStorage metadata).
  const RECENT_META_KEY = "fe3d.recent_projects.v2";
  const PREFS_KEY = "fe3d.app_preferences.v2";
  const SAVED_LAYOUT_KEY = "fe3d.saved_workspace_layout.v2";
  const SHELVES_KEY = "fe3d.maya_shelves.v1";
  const HOTKEYS_KEY = "fe3d.maya_hotkeys.v1";
  const IDB_NAME = "FinalEffect3DProjectCache";
  const IDB_STORE = "project_payloads";
  const memoryPayloadFallback = new Map();

  function cloneJson(val) {
    return JSON.parse(JSON.stringify(val));
  }

  function openPayloadDb() {
    return new Promise((resolve) => {
      if (!window.indexedDB) {
        resolve(null);
        return;
      }
      try {
        const req = window.indexedDB.open(IDB_NAME, 1);
        req.onupgradeneeded = function () {
          const db = req.result;
          if (!db.objectStoreNames.contains(IDB_STORE)) {
            db.createObjectStore(IDB_STORE);
          }
        };
        req.onsuccess = function () {
          resolve(req.result);
        };
        req.onerror = function () {
          resolve(null);
        };
      } catch (_) {
        resolve(null);
      }
    });
  }

  async function saveProjectPayload(id, fe3dJson) {
    memoryPayloadFallback.set(id, fe3dJson);
    const db = await openPayloadDb();
    if (!db) return;
    await new Promise((resolve) => {
      try {
        const tx = db.transaction(IDB_STORE, "readwrite");
        tx.objectStore(IDB_STORE).put(fe3dJson, id);
        tx.oncomplete = () => resolve();
        tx.onerror = () => resolve();
      } catch (_) {
        resolve();
      }
    });
  }

  async function loadProjectPayload(id) {
    if (memoryPayloadFallback.has(id)) {
      return memoryPayloadFallback.get(id);
    }
    const db = await openPayloadDb();
    if (!db) return null;
    return await new Promise((resolve) => {
      try {
        const tx = db.transaction(IDB_STORE, "readonly");
        const req = tx.objectStore(IDB_STORE).get(id);
        req.onsuccess = () => resolve(req.result || null);
        req.onerror = () => resolve(null);
      } catch (_) {
        resolve(null);
      }
    });
  }

  async function removeProjectPayload(id) {
    memoryPayloadFallback.delete(id);
    const db = await openPayloadDb();
    if (!db) return;
    return await new Promise((resolve) => {
      try {
        const tx = db.transaction(IDB_STORE, "readwrite");
        tx.objectStore(IDB_STORE).delete(id);
        tx.oncomplete = () => resolve();
        tx.onerror = () => resolve();
      } catch (_) {
        resolve();
      }
    });
  }

  function listRecentProjects() {
    try {
      const raw = window.localStorage.getItem(RECENT_META_KEY);
      if (!raw) return [];
      const parsed = JSON.parse(raw);
      return Array.isArray(parsed) ? parsed : [];
    } catch (_) {
      return [];
    }
  }

  function writeRecentProjects(list) {
    try {
      window.localStorage.setItem(RECENT_META_KEY, JSON.stringify(list.slice(0, 16)));
    } catch (_) {}
  }

  async function recordRecentProject(entry, fe3dJson) {
    const list = listRecentProjects();
    const id = entry.id || "proj_" + Date.now() + "_" + Math.random().toString(36).slice(2, 7);
    const record = {
      id: id,
      name: entry.name || "Untitled Project",
      author: entry.author || "Final Effect 3D",
      studioId: entry.studioId || "3d_studio",
      sceneName: entry.sceneName || "Scene",
      objectCount: Number(entry.objectCount) || 0,
      materialCount: Number(entry.materialCount) || 0,
      sourceLabel: entry.sourceLabel || "In-App Project",
      thumbnailDataUrl: entry.thumbnailDataUrl || null,
      updatedAt: Date.now(),
    };
    const filtered = list.filter((item) => item.id !== id && item.name !== record.name);
    filtered.unshift(record);
    writeRecentProjects(filtered);
    if (fe3dJson) {
      await saveProjectPayload(id, fe3dJson);
    }
    return record;
  }

  async function removeRecentProject(id) {
    const list = listRecentProjects().filter((item) => item.id !== id);
    writeRecentProjects(list);
    await removeProjectPayload(id);
  }

  function loadPreferences() {
    const defaults = {
      layoutMode: "auto", // "auto" | "desktop" | "landscape" | "portrait"
      uiDensity: "comfortable", // "compact" | "comfortable"
      showViewportHud: true,
      showShelfBar: true,
      showCommandBar: true,
      defaultAuthor: "Final Effect 3D",
    };
    try {
      const raw = window.localStorage.getItem(PREFS_KEY);
      if (!raw) return defaults;
      return Object.assign({}, defaults, JSON.parse(raw));
    } catch (_) {
      return defaults;
    }
  }

  function savePreferences(prefs) {
    try {
      window.localStorage.setItem(PREFS_KEY, JSON.stringify(prefs));
    } catch (_) {}
  }

  function saveCustomWorkspaceLayout(jsonStr) {
    try {
      window.localStorage.setItem(SAVED_LAYOUT_KEY, jsonStr);
      return true;
    } catch (_) {
      return false;
    }
  }

  function loadCustomWorkspaceLayout() {
    try {
      return window.localStorage.getItem(SAVED_LAYOUT_KEY);
    } catch (_) {
      return null;
    }
  }

  function loadShelves() {
    try {
      const raw = window.localStorage.getItem(SHELVES_KEY);
      if (!raw) return cloneJson(DEFAULT_SHELVES);
      const parsed = JSON.parse(raw);
      if (!Array.isArray(parsed) || parsed.length === 0) {
        return cloneJson(DEFAULT_SHELVES);
      }
      return parsed;
    } catch (_) {
      return cloneJson(DEFAULT_SHELVES);
    }
  }

  function saveShelves(shelves) {
    try {
      window.localStorage.setItem(SHELVES_KEY, JSON.stringify(shelves));
      return true;
    } catch (_) {
      return false;
    }
  }

  function resetDefaultShelves() {
    const fresh = cloneJson(DEFAULT_SHELVES);
    saveShelves(fresh);
    return fresh;
  }

  function loadHotkeys() {
    try {
      const raw = window.localStorage.getItem(HOTKEYS_KEY);
      if (!raw) return cloneJson(DEFAULT_HOTKEYS);
      const parsed = JSON.parse(raw);
      if (!Array.isArray(parsed) || parsed.length === 0) {
        return cloneJson(DEFAULT_HOTKEYS);
      }
      return DEFAULT_HOTKEYS.map((def) => {
        const found = parsed.find((p) => p.id === def.id);
        return found && typeof found.combo === "string"
          ? { id: def.id, label: def.label, combo: found.combo }
          : cloneJson(def);
      });
    } catch (_) {
      return cloneJson(DEFAULT_HOTKEYS);
    }
  }

  function saveHotkeys(hotkeys) {
    try {
      window.localStorage.setItem(HOTKEYS_KEY, JSON.stringify(hotkeys));
      return true;
    } catch (_) {
      return false;
    }
  }

  function resetDefaultHotkeys() {
    const fresh = cloneJson(DEFAULT_HOTKEYS);
    saveHotkeys(fresh);
    return fresh;
  }

  window.FE3D_REGISTRY = {
    ICONS,
    STUDIOS,
    WORKSPACES,
    DEFAULT_SHELVES,
    DEFAULT_HOTKEYS,
    listRecentProjects,
    recordRecentProject,
    loadProjectPayload,
    removeRecentProject,
    loadPreferences,
    savePreferences,
    saveCustomWorkspaceLayout,
    loadCustomWorkspaceLayout,
    loadShelves,
    saveShelves,
    resetDefaultShelves,
    loadHotkeys,
    saveHotkeys,
    resetDefaultHotkeys,
  };
})();
