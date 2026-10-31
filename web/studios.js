// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D — Studio Registry, Workspace Definitions, Vector Icon System & Persistence (Stage 5 v0.5.0)

(function () {
  "use strict";

  const ICONS = {
    logo: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><polygon points="12 2 21 7 21 17 12 22 3 17 3 7"/><polyline points="3 7 12 12 21 7"/><line x1="12" y1="22" x2="12" y2="12"/><circle cx="12" cy="12" r="2.2" fill="currentColor"/></svg>',
    home: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M2.2 6.8L8 2.2l5.8 4.6v6.2a.8.8 0 0 1-.8.8H3a.8.8 0 0 1-.8-.8V6.8z"/><path d="M6.2 13.8V9.2h3.6v4.6"/></svg>',
    select: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M3.2 2.2l9.2 6.2-4.2.9 2.1 4.1-1.8.9-2.1-4.2-3.2 2.6z"/></svg>',
    move: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M8 1.8v12.4M1.8 8h12.4M8 1.8L6.2 3.6M8 1.8l1.8 1.8M8 14.2l-1.8-1.8M8 14.2l1.8-1.8M1.8 8l1.8-1.8M1.8 8l1.8 1.8M14.2 8l-1.8-1.8M14.2 8l-1.8 1.8"/></svg>',
    rotate: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M13.2 8a5.2 5.2 0 1 1-1.5-3.7"/><polyline points="13.4 2.2 13.4 5.2 10.4 5.2"/></svg>',
    scale: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><rect x="2.2" y="7.8" width="6" height="6" rx="0.6"/><path d="M8.5 7.5L13.6 2.4M10.2 2.4h3.4v3.4"/></svg>',
    cube: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><polygon points="8 1.8 13.6 5 13.6 11 8 14.2 2.4 11 2.4 5"/><polyline points="2.4 5 8 8.2 13.6 5"/><line x1="8" y1="14.2" x2="8" y2="8.2"/></svg>',
    sphere: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="5.8"/><ellipse cx="8" cy="8" rx="5.8" ry="2.3"/><ellipse cx="8" cy="8" rx="2.3" ry="5.8"/></svg>',
    pyramid: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><polygon points="8 2 14 12.6 8 14.2 2 12.6"/><line x1="8" y1="2" x2="8" y2="14.2"/></svg>',
    plane: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><polygon points="8 4.2 14.4 8 8 11.8 1.6 8"/><line x1="4.8" y1="6.1" x2="11.2" y2="9.9"/><line x1="11.2" y1="6.1" x2="4.8" y2="9.9"/></svg>',
    camera: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><rect x="1.8" y="4.5" width="8.8" height="7" rx="1"/><polygon points="10.6 6.8 14.2 4.8 14.2 11.2 10.6 9.2"/></svg>',
    light: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="3"/><path d="M8 1.5v2M8 12.5v2M1.5 8h2M12.5 8h2M3.4 3.4l1.4 1.4M11.2 11.2l1.4 1.4M12.6 3.4l-1.4 1.4M4.8 11.2l-1.4 1.4"/></svg>',
    pivot: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><line x1="8" y1="2" x2="8" y2="14"/><line x1="2" y1="8" x2="14" y2="8"/><circle cx="8" cy="8" r="2.2"/></svg>',
    vertex: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><line x1="3.5" y1="12.5" x2="8" y2="3.5"/><line x1="8" y1="3.5" x2="12.5" y2="12.5"/><line x1="3.5" y1="12.5" x2="12.5" y2="12.5"/><circle cx="8" cy="3.5" r="2" fill="currentColor"/><circle cx="3.5" cy="12.5" r="1.6"/><circle cx="12.5" cy="12.5" r="1.6"/></svg>',
    edge: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><polygon points="3 12.5 8 3.2 13 12.5" stroke-opacity="0.45"/><line x1="3" y1="12.5" x2="8" y2="3.2" stroke-width="2.4"/></svg>',
    face: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><polygon points="3 4 13 3 12 13 4 12" fill="currentColor" fill-opacity="0.32"/></svg>',
    objectProps: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><rect x="3.2" y="3.2" width="9.6" height="9.6" rx="1"/><path d="M1.5 5h1.7M1.5 11h1.7M12.8 5h1.7M12.8 11h1.7M5 1.5v1.7M11 1.5v1.7M5 12.8v1.7M11 12.8v1.7"/></svg>',
    modifiers: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M10.5 2.5a3.2 3.2 0 0 0-3 4.3L2.6 11.7a1.2 1.2 0 0 0 1.7 1.7l4.9-4.9a3.2 3.2 0 0 0 4.3-3l-2 2-1.8-.4-.4-1.8 2-2z"/></svg>',
    material: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="5.6"/><path d="M8 2.4a5.6 5.6 0 0 1 0 11.2z" fill="currentColor" fill-opacity="0.35"/></svg>',
    sceneData: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="5.6"/><line x1="2.4" y1="8" x2="13.6" y2="8"/><path d="M8 2.4c1.8 1.6 1.8 9.6 0 11.2M8 2.4c-1.8 1.6-1.8 9.6 0 11.2"/></svg>',
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
    settings: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="2.2"/><path d="M8 1.6v1.8M8 12.6v1.8M1.6 8h1.8M12.6 8h1.8M3.5 3.5l1.3 1.3M11.2 11.2l1.3 1.3M12.5 3.5l-1.3 1.3M4.8 11.2l-1.3 1.3"/></svg>',
    orbit: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="5.5" stroke-dasharray="2.2 1.4"/><ellipse cx="8" cy="8" rx="6.2" ry="2.4" transform="rotate(-24 8 8)"/></svg>',
    pan: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M8 2v12M2 8h12M6 3.8L8 1.8l2 2M6 12.2l2 2 2-2M3.8 6L1.8 8l2 2M12.2 6l2 2-2 2"/></svg>',
    zoom: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="7" cy="7" r="4.6"/><line x1="10.4" y1="10.4" x2="14" y2="14"/><line x1="5" y1="7" x2="9" y2="7"/><line x1="7" y1="5" x2="7" y2="9"/></svg>',
    frame: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M2.2 5.4V2.2h3.2M10.6 2.2h3.2v3.2M13.8 10.6v3.2h-3.2M5.4 13.8H2.2v-3.2"/><circle cx="8" cy="8" r="2"/></svg>',
    play: '<svg viewBox="0 0 16 16" fill="currentColor"><polygon points="4.2 2.6 13.2 8 4.2 13.4"/></svg>',
    pause: '<svg viewBox="0 0 16 16" fill="currentColor"><rect x="3.8" y="2.8" width="3" height="10.4" rx="0.5"/><rect x="9.2" y="2.8" width="3" height="10.4" rx="0.5"/></svg>',
    landscape: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><polygon points="1.5 13 6 5.2 9.5 10.4 11.8 7.5 14.5 13"/></svg>',
    character: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="3.6" r="1.9"/><path d="M3.2 6.8h9.6M8 5.6v4.6M8 10.2l-2.6 4.2M8 10.2l2.6 4.2"/></svg>',
    draw2d: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M2.5 13.5l2.8-.6 7.6-7.6a1.4 1.4 0 0 0-2-2L3.3 10.9l-.8 2.6z"/></svg>',
    sculpt: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="7.4" cy="8.6" r="4.8"/><path d="M10.5 2.2l3.3 3.3-3.2 3.2"/></svg>',
    shade: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="5.2" cy="5.2" r="2.4"/><circle cx="11.2" cy="10.8" r="2.4"/><path d="M7.6 5.2h1.2v5.6h1"/></svg>',
    cylinder: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><ellipse cx="8" cy="3.6" rx="4.8" ry="1.8"/><path d="M3.2 3.6v8.8c0 1 2.15 1.8 4.8 1.8s4.8-.8 4.8-1.8V3.6"/></svg>',
    cone: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M8 2.2L3 12.2c0 1 2.24 1.8 5 1.8s5-.8 5-1.8L8 2.2z"/><ellipse cx="8" cy="12.2" rx="5" ry="1.6" stroke-opacity="0.5"/></svg>',
    torus: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><ellipse cx="8" cy="8" rx="6" ry="3.8"/><ellipse cx="8" cy="8" rx="2.3" ry="1.2"/></svg>',
    snapGrid: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><path d="M2 5.5h12M2 10.5h12M5.5 2v12M10.5 2v12"/><circle cx="10.5" cy="5.5" r="2" fill="currentColor"/></svg>',
    snapCurve: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M2.2 13C3 6 13 10 13.8 3"/><circle cx="8" cy="8" r="2.1" fill="currentColor"/></svg>',
    snapPoint: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="2.4" fill="currentColor"/><path d="M8 2v3M8 11v3M2 8h3M11 8h3"/></svg>',
    snapSurface: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><path d="M4.5 3.5v4a3.5 3.5 0 0 0 7 0v-4"/><line x1="3.5" y1="5.5" x2="6" y2="5.5"/><line x1="10" y1="5.5" x2="12.5" y2="5.5"/></svg>',
    lock: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><rect x="3.4" y="7.2" width="9.2" height="6.4" rx="1"/><path d="M5.4 7.2V5a2.6 2.6 0 0 1 5.2 0v2.2"/></svg>',
    unlock: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><rect x="3.4" y="7.2" width="9.2" height="6.4" rx="1"/><path d="M5.4 7.2V4.8a2.6 2.6 0 0 1 4.9-1.2"/></svg>',
    layers: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><polygon points="8 2.2 14.2 5.4 8 8.6 1.8 5.4"/><polyline points="1.8 8.4 8 11.6 14.2 8.4"/><polyline points="1.8 11.2 8 14.4 14.2 11.2"/></svg>',
    channelBox: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><rect x="2" y="2.2" width="12" height="11.6" rx="1"/><line x1="7.5" y1="2.2" x2="7.5" y2="13.8"/><line x1="2" y1="6" x2="14" y2="6"/><line x1="2" y1="9.8" x2="14" y2="9.8"/></svg>',
    keyframe: '<svg viewBox="0 0 16 16" fill="currentColor"><polygon points="8 2.2 13.4 8 8 13.8 2.6 8"/></svg>',
    universal: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><rect x="3.2" y="3.2" width="9.6" height="9.6" stroke-dasharray="2 1.2"/><path d="M8 1.4v13.2M1.4 8h13.2"/><circle cx="8" cy="8" r="1.8" fill="currentColor"/></svg>',
    showManip: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="5.2"/><line x1="8" y1="2.8" x2="8" y2="8"/><line x1="8" y1="8" x2="12.2" y2="10.4"/><circle cx="8" cy="8" r="1.5" fill="currentColor"/></svg>',
    xray: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><rect x="2.4" y="4.4" width="7.2" height="7.2"/><rect x="6.4" y="2.4" width="7.2" height="7.2" stroke-dasharray="1.8 1.2"/></svg>',
    isolate: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><circle cx="8" cy="8" r="5.6"/><circle cx="8" cy="8" r="2.2" fill="currentColor"/></svg>',
    renderFrame: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4"><rect x="1.8" y="3" width="12.4" height="10" rx="1"/><circle cx="8" cy="8" r="2.8"/><circle cx="12" cy="5" r="0.8" fill="currentColor"/></svg>',
  };

  // Extensible Studio Registry (Part 2.2) — Only 3D Studio is operational in Stage 5.
  const STUDIOS = [
    {
      id: "3d_studio",
      name: "3D Studio",
      shortName: "3D Studio",
      icon: "cube",
      operational: true,
      badge: "OPERATIONAL · v0.5.0",
      tagline: "Scene Layout, Polygon Mesh Modeling, 3D Transform Gizmos, Metallic-Roughness PBR Shading & .fe3d Pipeline",
      description:
        "Full C++20 WebGL 2.0 3D authoring studio with Object/Vertex/Edge/Face modes, Move/Rotate/Scale 3D gizmos, manifold polygon extrusion & boundary fill, energy-conserving Cook-Torrance PBR materials with live 3D sphere preview, CommandStack Undo/Redo, and .fe3d/.fe3dbin project persistence.",
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

  // 12 Blender-Inspired Workspace Tabs inside 3D Studio (Part 4.2).
  // Every workspace operates on the exact same live C++ Scene & ProjectDocument.
  const WORKSPACES = [
    {
      id: "layout",
      label: "Layout",
      preset: "layout",
      inspectorTab: "object",
      bottomEditor: "timeline",
      showToolShelf: true,
      foundationNotice: null,
    },
    {
      id: "modeling",
      label: "Modeling",
      preset: "modeling",
      inspectorTab: "modifiers",
      bottomEditor: "console",
      showToolShelf: true,
      foundationNotice: null,
    },
    {
      id: "sculpting",
      label: "Sculpting",
      preset: "sculpting",
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
      inspectorTab: "material",
      bottomEditor: "shader_foundation",
      showToolShelf: true,
      foundationNotice: null,
    },
    {
      id: "animation",
      label: "Animation",
      preset: "animation",
      inspectorTab: "object",
      bottomEditor: "timeline",
      showToolShelf: true,
      foundationNotice: null,
    },
    {
      id: "rendering",
      label: "Rendering",
      preset: "rendering",
      inspectorTab: "render",
      bottomEditor: "console",
      showToolShelf: false,
      foundationNotice: null,
    },
    {
      id: "compositing",
      label: "Compositing",
      preset: "compositing",
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
      label: "VFX",
      preset: "vfx",
      inspectorTab: "data",
      bottomEditor: "timeline",
      showToolShelf: true,
      foundationNotice: {
        title: "VFX & Motion Tracking Foundation",
        detail:
          "Connected to the active scene camera & timeline. Planar/point camera tracking and lens distortion solvers are planned for a future stage.",
      },
    },
    {
      id: "geometry_nodes",
      label: "Geometry Nodes",
      preset: "geometry_nodes",
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

  // Real Recent Projects & Preferences Store (IndexedDB payload + localStorage metadata).
  const RECENT_META_KEY = "fe3d.recent_projects.v2";
  const PREFS_KEY = "fe3d.app_preferences.v2";
  const SAVED_LAYOUT_KEY = "fe3d.saved_workspace_layout.v2";
  const IDB_NAME = "FinalEffect3DProjectCache";
  const IDB_STORE = "project_payloads";
  const memoryPayloadFallback = new Map();

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
    await new Promise((resolve) => {
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

  window.FE3D_REGISTRY = {
    ICONS,
    STUDIOS,
    WORKSPACES,
    listRecentProjects,
    recordRecentProject,
    loadProjectPayload,
    removeRecentProject,
    loadPreferences,
    savePreferences,
    saveCustomWorkspaceLayout,
    loadCustomWorkspaceLayout,
  };
})();
