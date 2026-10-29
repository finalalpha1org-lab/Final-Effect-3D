# Final Effect 3D

**Hollywood VFX in your studio — and, later, in your hands.**

Final Effect 3D is a visual-effects-oriented 3D creation suite for Windows, macOS and Linux, built on the open-source Blender codebase and rebranded and extended as its own product.

> **Status: planning / pre-alpha.** No builds are distributed yet. This repository currently holds the plan, the rebranding inventory and the compliance rules; the branded source lives in the [blender fork](https://github.com/finalalpha1org-lab/blender) as it is developed.

## What Final Effect 3D is

- The full Blender 3D pipeline — modelling, sculpting, animation, simulation, rendering, compositing, tracking, video editing — under our own name, branding and release process.
- A VFX-focused layer on top: curated effect presets and node-group packs, shot-oriented project templates, and render presets tuned for multi-pass / AOV visual-effects output.
- Desktop first. A mobile product is a later phase, gated on upstream Blender's Android tablet port (currently targeted for 2027) or on a mobile client working against a Blender-backed render service.

We do **not** reimplement Blender: it is roughly 100 MB of C++ and two decades of engineering. We brand it, package it, extend it and support it as our own product.

## Documents

| Document | Contents |
|---|---|
| [docs/PLAN.md](docs/PLAN.md) | Architecture decisions, product definition, base-repository facts, roadmap phases, risks, next actions |
| [docs/REBRANDING.md](docs/REBRANDING.md) | Exact rebrand inventory (build identity, splash/icons, Windows/macOS/Linux packaging, in-app strings), the do-not-touch list, the procedure and the verification checklist |
| [docs/COMPLIANCE.md](docs/COMPLIANCE.md) | GPL obligations, trademark rules, third-party licences and the gate checklist for every public release |

## Licensing

- This repository is licensed **GPL-3.0** (see [LICENSE](LICENSE)).
- Final Effect 3D is a modified version of Blender, which is licensed **GPL-2.0-or-later**; the combined work is distributed under the GPL, and the complete corresponding source of every released build is published with that release.
- "Blender" and the Blender logo are trademarks of the Blender Foundation. Final Effect 3D is an independent project, not affiliated with or endorsed by the Blender Foundation.
- Third-party components bundled in the base keep their own licences; see [release/license/licenses.json](https://github.com/finalalpha1org-lab/blender/blob/main/release/license/licenses.json) in the fork.

## Repository map

    Final-Effect-3D/        product documentation (this repository) — plans, compliance, future product assets
    blender/                the source: fork of blender/blender; branding work happens on the final-effect-3d branch

## Contributing

Contributions are welcome once the Phase 1 build exists. Until then, see [docs/PLAN.md](docs/PLAN.md) §7 for the work queue. All contributions must be GPL-compatible and signed off under the Developer Certificate of Origin.