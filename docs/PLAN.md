# Final Effect 3D — Architecture, Licensing and Build Plan

**Status:** draft 1 · planning phase · no binaries built yet
**Product:** Final Effect 3D — a VFX-focused 3D creation suite
**Base:** fork of Blender (`finalalpha1org-lab/blender`, upstream `blender/blender`)
**Licence of this repository:** GPL-3.0 (see LICENSE). The product derived from Blender is a free-software product; see docs/COMPLIANCE.md.

---

## 0. Where we are today (verified facts)

| Item | State |
|---|---|
| `finalalpha1org-lab/blender` | real fork of `blender/blender` (GitHub mirror of the official repo), single branch **main**, last pushed 2026-09-24, **1,554 commits behind upstream**, **0 commits ahead** — i.e. an untouched copy, not yet a modified base |
| `finalalpha1org-lab/Final-Effect-3D` | this repository: README + GPL-3.0 LICENSE + .gitignore only |
| Upstream at time of survey | commit **25952e3d8f953e9103a01c2497d0857ed7fa320f**, branch main, version **5.4 alpha**, file format version 504/subversion 4, 22,289 tree entries (20,908 files, 1,381 directories, 295 MB working tree, 11.7 GB git history) |
| Upstream application size | `source/blender`: 7,284 files / 99.3 MB across 33 modules (editors 26 MB, blenkernel 12 MB, draw 9.6 MB, makesrna 7.8 MB, blenlib 6.6 MB, gpu 5.7 MB, nodes 5.7 MB, …) |
| Upstream licence | GPL-2.0-or-later; `COPYING` states Blender is not available under other licences |
| Mobile | upstream is porting to **Android (tablet-first)**, target H1 2027, iPad on hold; there is no shipping mobile build today |

## 1. Product definition

**What we are building:** Final Effect 3D, a desktop 3D / visual-effects suite for Windows, macOS and Linux, produced by rebranding and extending the Blender codebase.

**Positioning:** not a general-purpose "Blender clone" — a VFX-oriented edition. The differentiating layer we own is:

- curated **effect presets and node-group asset packs** (compositor, geometry nodes, shader, simulation),
- VFX-oriented **render presets** and output pipelines (multi-pass EXR, AOV sets, denoising, colour pipelines),
- **templates and project starters** (shot-based workflow: plate, track, light, comp),
- an onboarding/UI theme and default settings tailored to VFX artists.

**Platform decision (recorded):** desktop-first. "VFX on your mobile" is explicitly a later phase and only viable on top of upstream's Android tablet port (H1 2027); see ADR-6. Rebuilding Blender itself from scratch is not in scope — it is ~100 MB of C++ and two decades of engineering.

## 2. Legal basis, in one page

1. Blender is licensed **GPL-2.0-or-later**. Any application built from its source is a **derivative work**.
2. A derivative work must be distributed under the GPL, with **complete corresponding source code** available to everyone who receives a binary. Free-of-charge *or paid* distribution both require this.
3. Your GPL-3.0 licensing of this repository is compatible: the "or later" clause allows GPLv3.
4. The **Blender name and logo are trademarks** of the Blender Foundation. Rebranding is mandatory; you may not imply that Final Effect 3D is Blender or is endorsed by the Blender Foundation.
5. Bundled third-party code keeps its own licences; the authoritative inventory ships in the Blender tree as **release/license/licenses.json** plus per-library files under **extern/*/**. It must be re-checked and reproduced in our releases.

Details, the per-release checklist and the exact obligations are in **docs/COMPLIANCE.md**. This plan is engineering guidance, not legal advice.

## 3. Architecture decisions

### ADR-1 — Fork and rebrand Blender rather than rewrite
Chosen: a rebranded fork. A rewrite would take many years and never reach feature parity.
Consequences: we inherit upstream's maintenance burden and must keep syncing (ADR-4); we must keep a rebaseable separation between upstream code and our branding (ADR-2).

### ADR-2 — Branding lives in a small, reviewable patch set on a dedicated branch
The fork keeps upstream code as close to pristine as possible:

- **upstream-main** — bare mirror of `blender/blender` main, never edited by us.
- **final-effect-3d** — our branch: upstream-main plus a small number of branding commits (one commit per area: identity, splash, icons, packaging, docs).
- Rebasing this branch onto upstream-main is the sync mechanism. Keeping the patch small and confined to branding files is what makes a monthly or per-release sync cheap instead of a merge nightmare.

### ADR-3 — Rename only user-visible identity; keep every technical identifier
We do **not** rename: the `bpy` Python module, the `.blend` file format and its magic, `BLENDER_FILE_VERSION` / min-version constants, command-line flags, environment variables (`BLENDER_USER_CONFIG` etc.), the extension/add-on API, or internal symbol names. Rationale: the whole ecosystem (add-ons, exporters, pipeline scripts, existing .blend files, tutorials) depends on these; renaming buys nothing and breaks everything.

### ADR-4 — Track upstream on a schedule
The fork is 1,554 commits behind at the start. Sync must become routine:

- monthly rebase of `final-effect-3d` onto upstream main, plus a sync on every upstream release candidate that we intend to ship.
- a CI job that reports (a) how far behind we are and (b) whether our branding commits still apply cleanly.

### ADR-5 — Keep user data paths initially, revisit with a migration shim
Blender's user config, add-on and cache paths embed the brand (`~/.config/blender/<version>/config/userpref.blend`, `%APPDATA%/Blender Foundation/Blender/...`, `~/.cache/blender`). Phase 1 keeps them unchanged (predictable, no data migration, add-on documentation still valid). Renaming them is a Phase 3 option and, if taken, must ship a one-time migration that copies the existing configuration forward.

### ADR-6 — Desktop first, mobile later as a client of a Blender-backed service
Upstream's tablet/Android port is unfinished and not a stable base. If a mobile product is required earlier, the viable shape is a mobile client plus a headless Blender/`bpy` render or VFX service (GPL stays server-side, the client's licence is our choice). That is a separate workstream and is not started by this plan.

## 4. Engineering architecture

**What we inherit unchanged:** the C++ application (`source/blender/*`), the Python layer (`scripts/**`, 12 bundled add-ons), the build system (CMake >= 3.21 with the `GNUmakefile` / `make.bat` wrappers), the dependency build environment (`build_files/build_environment`: 126 CMake scripts + 78 patches), tests (`tests/**`) and the dependency submodules `lib/{linux_x64,linux_arm64,macos_arm64,windows_x64,windows_arm64}`.

**What we own:**

1. the branding patch set (see docs/REBRANDING.md for the exact inventory and do-not-touch list),
2. our effect/preset asset packs and templates, distributed as Blender extensions/add-ons under a licence compatible with the GPL,
3. release engineering: build matrix, packaging (WiX for Windows, .app/dmg for macOS, tar/AppImage/snap for Linux), signing, and the GPL source publication step,
4. our CI on GitHub Actions and the fork-sync automation.

**Build reality:** a full build needs a machine with roughly 40 GB of disk, several hundred MB to a few GB of downloaded/built dependencies, a C++ toolchain and hours of compile time on first run. GitHub-hosted runners (14 GB disk, 6 h limit) cannot produce a full release build; plan on a self-hosted runner (or a cloud VM with ccache) for release builds, and use GitHub-hosted runners only for light configurations such as `make lite` and static checks.

## 5. Roadmap

### Phase 0 — Rebaseable base (days)
- Sync `finalalpha1org-lab/blender` to upstream main; keep **main** as the pristine mirror.
- Create the **final-effect-3d** branch and record the sync procedure in docs/SYNC.md (to be written).
- Add CI: "how far behind are we", branding-patch-applies check, style/static checks.
- Exit criteria: fork is current; the branding branch exists and rebases cleanly.

### Phase 1 — Minimum branded build (1-3 weeks, needs a build machine)
- Apply the identity patch set: product name strings, splash, icons, app id, installers, desktop/metainfo files, docs and about/splash text.
- Produce a first internally shippable build per platform; verify the checklist in docs/REBRANDING.md (about box, splash, title bar, installer name, bundle id, desktop entry, `--version`, .blend roundtrip, a Python add-on still works).
- Exit criteria: a build that carries no Blender logo or wordmark in user-visible places, passes the verification checklist, and runs an existing .blend file unchanged.

### Phase 2 — Release engineering (2-4 weeks)
- Tagged releases with reproducible build scripts, installers per platform, checksums and code signing (Windows EV certificate, Apple Developer ID + notarisation, Linux package metadata).
- GPL publication step wired into the release: source archive of the branded branch attached to every binary release, plus NOTICE/AUTHORS/third-party licence bundle.
- Update/distribution channel of our own (no reliance on Blender's update infrastructure or URLs).
- Exit criteria: one full release produced end-to-end by a documented, repeatable process.

### Phase 3 — Product differentiation (continuous)
- Shinier identity: UI theme, splash art, default workspace arrangement, workspace templates.
- VFX asset packs: compositor/geometry/shader node groups, render presets (passes, AOVs, colour management), starter projects.
- Optional: branded user-data paths with a migration shim (ADR-5).
- Exit criteria: users can produce a plausible VFX shot end-to-end with shipped presets only.

### Phase 4 — Distribution and community (continuous)
- Public website/downloads, docs, support channel, contribution policy (DCO sign-off) and a documented branding-change procedure.
- Trademark and licence review of every distributed artefact before each public release.

### Phase 5 — Mobile (gated, not scheduled)
- Re-evaluate when upstream's Android port reaches a usable state (targeted H1 2027), or build the mobile-client + render-service architecture of ADR-6.

## 6. Risks

| Risk | Mitigation |
|---|---|
| Fork rot — falling behind upstream | ADR-2 + ADR-4: small branding patch, scheduled rebase, CI that measures divergence |
| Trademark exposure — Blender logo/wordmark shipped accidentally | docs/REBRANDING.md inventory + a release checklist gate + grep-based CI check for known brand paths |
| GPL non-compliance — releasing binaries without source | Phase 2 release pipeline always attaches the branded source archive and licence notices |
| Build cost — full builds are heavy | self-hosted/cloud runner with ccache; `make lite` on hosted runners; prebuilt `lib/*` submodules instead of building every dependency |
| Scope creep into "rewrite Blender" | product definition (§1): we brand and extend, we do not reimplement |
| Expectation gap — add-ons/ecosystem users expect the Blender name | keep technical identifiers (ADR-3) so existing add-ons and files keep working; document migration in the README |
| Localisation churn — ~82 MB of `.po` files mention the brand | global, scripted replacement on rebrand, then upstream-translator sync; do not hand-edit |

## 7. Immediate next actions

1. Sync the fork to upstream and create the `final-effect-3d` branch (Phase 0).
2. Write the branding patch inventory into an executable script (`tools/rebrand/`) rather than manual edits, so it can be re-applied after every rebase.
3. Stand up a build machine/runner and produce a first `make lite` build.
4. Decide the product's public licence notices (GPL-3.0 for the repo; notices for third-party code) and add a NOTICE file.
5. Only after Phase 1 verification: publish anything publicly.

## 8. Appendix — facts used in this plan

- Upstream survey commit: 25952e3d8f953e9103a01c2497d0857ed7fa320f (main), version 5.4 alpha, file format 504.4, minimum readable file 4.05.85.
- Language mix at that commit (Gitea statistics, source files only): C++ 112.3 MB, Python 21.0 MB, C 2.2 MB, CMake 1.6 MB, Objective-C++ 0.95 MB, GLSL 0.85 MB, then Metal/Cuda/shell.
- Working tree at that commit: 295 MB across 20,908 files; `source/` 99.5 MB, `release/` 37.3 MB, `extern/` 25.4 MB, `intern/` 14.2 MB, `tests/` 14.8 MB, `locale/` 81.9 MB.
- Packaging surfaces in-tree: `release/windows/{icons,installer_wix,msix,manifest,batch}`, `release/darwin/Blender.app/Contents`, `release/freedesktop/{blender.desktop,org.blender.Blender.metainfo.xml,icons,snap}`, `release/datafiles/splash.png`, `release/datafiles/icons*`.
- Brand identity knobs in the build: root `CMakeLists.txt` declares `project(Blender)`, `BLENDER_WIN_APPID="blender.<major>.<minor>"`, `BLENDER_WIN_APPID_FRIENDLY_NAME="Blender <major>.<minor>"`; `source/creator/CMakeLists.txt` declares `add_executable(blender ...)` and `TARGETDIR_VER "bpy/${BLENDER_VERSION}"`.
- The version string itself is numeric (`5.4.0-alpha`); the brand word appears in UI strings, packaging metadata and asset files, not in the version number.

---

See also: **docs/REBRANDING.md** (exact inventory and do-not-touch list) and **docs/COMPLIANCE.md** (obligations and release checklist).