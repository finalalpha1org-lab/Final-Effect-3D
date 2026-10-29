# Rebranding Blender into Final Effect 3D — inventory and procedure

This document is the working checklist for the branding patch set described in **docs/PLAN.md** (ADR-2, ADR-3). Everything here is verified against upstream commit 25952e3d8f953e9103a01c2497d0857ed7fa320f (main). Paths are listed as they appear in the Blender tree.

## 1. Principle

**Rename user-visible identity, never technical identity.** The rebrand must be small enough to rebase onto upstream in minutes, and must not break the ecosystem: .blend files, add-ons, pipeline scripts and existing user configuration keep working.

## 2. Do NOT touch (hard rules)

| Item | Why |
|---|---|
| `.blend` file format and its file magic (the `BLENDER_V100` fixed-12-byte identifier written by `source/blender/blenloader/intern/writefile.cc` and read by `readfile.cc`) | renaming it breaks every existing .blend file and every third-party tool |
| `BLENDER_FILE_VERSION`, `BLENDER_FILE_SUBVERSION`, `BLENDER_FILE_MIN_VERSION`, `BLENDER_FILE_MIN_SUBVERSION` in `source/blender/blenkernel/BKE_blender_version.h` | format compatibility rules and upstream versioning logic |
| The `bpy` Python module name and `TARGETDIR_VER "bpy/${BLENDER_VERSION}"` in `source/creator/CMakeLists.txt` | every add-on does `import bpy`; also the path where the Python module is installed |
| Python API names, operator ids, RNA identifiers, `.blend` datablock names | public API. Renaming them is not branding, it is a fork-breaking change |
| Command-line flags and environment variables (`-b/--background`, `BLENDER_USER_CONFIG`, `BLENDER_USER_SCRIPTS`, …) | pipelines, render farms and CI depend on them |
| Extension / add-on API and the `extensions` mechanism | third-party add-on ecosystem compatibility |
| Licence texts: `COPYING`, `doc/license/*` (GPL, LGPL, Apache, BSD, MIT, MPL, Zlib, Boost, SPDX list), `release/license/licenses.json` | removing or altering them is a licence violation |
| Copyright headers and `AUTHORS` (38 KB of upstream contributors) | attribution must be preserved; add our own contributors alongside, never replace |
| `SPDX-FileCopyrightText` / `SPDX-License-Identifier` lines in every source file | required notices |

Also keep in mind: many localised UI strings (`locale/po/*.po`, 49 files / ~82 MB) mention Blender. Do not hand-edit them; use one scripted replacement pass and let the two strings diverge naturally from upstream afterwards.

## 3. Change inventory by area

### 3.1 Build and product identity
| Location | What it holds | Action |
|---|---|---|
| root `CMakeLists.txt` | `project(Blender)`, and the brand knobs `BLENDER_WIN_APPID="blender.<major>.<minor>"`, `BLENDER_WIN_APPID_FRIENDLY_NAME="Blender <major>.<minor>"` | set product name; introduce our own app id such as `com.finalalpha.finaleffect3d`; keep the executable target name decision consistent with 3.2 |
| `source/creator/CMakeLists.txt` | `add_executable(blender ...)`, Windows launcher, `WITH_INSTALL_COPYRIGHT`, `WITH_INSTALL_PORTABLE`, `TARGETDIR_VER` | optionally rename the executable target; keep `TARGETDIR_VER` on `bpy/...` |
| `build_files/cmake/**`, `build_files/build_environment/**` | dependency build environment, 126 CMake scripts + 78 patches | no branding, but this is where the build cost lives (see PLAN §4) |
| `pyproject.toml`, `.clang-format`, `.clang-tidy`, `.editorconfig` | style configuration | leave as-is |

### 3.2 Splash, icons and default assets
| Location | What it holds | Action |
|---|---|---|
| `release/datafiles/splash.png`, `release/datafiles/splash_template.xcf` | the startup splash | replace with Final Effect 3D artwork |
| `release/datafiles/icons_svg/` (801 files), `release/datafiles/icons/` (153), `release/datafiles/icons_blend/`, `release/datafiles/blender_icons_geom.py` | UI icon set, generated atlas and the Blender-logo geometry icons | replace or drop the Blender logo marks; keep the generic icons |
| `release/datafiles/startup.blend`, `preview.blend`, `preview_grease_pencil.blend` | default startup scene, preview assets | rebuild the default scene so it carries no Blender branding |
| `release/datafiles/{fonts,colormanagement,cursors,studiolights,userdef}` | fonts, colour management LUTs, cursors, studio lights | keep; check third-party licences (fonts are Noto/DejaVu) |

### 3.3 Platform packaging
| Location | What it holds | Action |
|---|---|---|
| `release/windows/installer_wix/WIX.template`, `WixUI_Blender.wxs`, `WIX_UI_BANNER.bmp`, `WIX_UI_DIALOG.png` | WiX installer definition, installer artwork, licence page | rename product, replace artwork, point the licence page at our GPL text |
| `release/windows/{icons,msix,manifest,batch}` | app icons, MSIX package definition, launcher batch files | rename package/launcher, replace icons |
| `release/darwin/Blender.app/Contents`, `blender_liquid_glass.icon`, `entitlements.plist`, `release/darwin/scripts` | macOS bundle (Info.plist, bundle id, icon), signing entitlements, packaging scripts | rename bundle to Final Effect 3D.app, set our bundle id, replace the icon |
| `release/freedesktop/blender.desktop` | `Name=Blender`, `GenericName=3D modeler`, translated Comment lines | rename the entry, keep .blend file association working |
| `release/freedesktop/org.blender.Blender.metainfo.xml` | AppStream id `org.blender.Blender`, `project_license GPL-3.0`, description text | new id (e.g. `com.finalalpha.FinalEffect3D`) and rewritten description |
| `release/freedesktop/{icons,snap}` | icon theme names and Snap packaging | rename consistently with the desktop entry |
| `release/text/`, `release/release_notes`, `release/license` | readme/licence text shipped with builds | rewrite; keep the third-party licence bundle |

### 3.4 In-application strings
The product name appears in menus, the splash screen text, the About dialog, window titles and error messages. The reliable way to find every occurrence is a search over the tree at rebrand time, not a hand-maintained list:

    git grep -n -I -w "Blender"   -- ':!doc/license' ':!COPYING' ':!AUTHORS' ':!locale'
    git grep -n -I "blender.org" -- ':!doc/license' ':!locale'
    git grep -n -I -i "suzanne"

Then classify each hit: user-visible brand (change), technical identifier or licence text (leave), upstream URL (replace with our own or remove), documentation reference (rewrite). Expect the bulk to live in `source/blender/editors/`, `source/blender/windowmanager/`, `scripts/startup/bl_ui/**`, and packaging files.

### 3.5 Documentation and metadata in this repository and the fork
- `README.md` of the fork currently reads "Official mirror of Blender" — must be replaced (it is also a false description of our fork).
- `doc/manpage/blender.1`, `doc/python_api/**`, `doc/doxygen/**` reference the Blender project; the Python API docs are largely generated from docstrings and can stay, but the project-name references should be rewritten where they describe the product.
- Add our own: `NOTICE` (modification notice with date, per GPLv3 §5a), `CONTRIBUTORS` (our contributors, appended to upstream AUTHORS), `docs/COMPLIANCE.md` (already in this repo).

### 3.6 User data paths (deferred — ADR-5)
Brand strings are embedded in user-visible filesystem locations: on Linux `~/.config/blender/<version>/config/{userpref,startup}.blend` and `~/.cache/blender`; on Windows `%APPDATA%/Blender Foundation/Blender/...` and the `Blender Foundation/Blender/Cache` tree (see `source/blender/blenkernel/intern/appdir.cc`). Phase 1 keeps these unchanged. If they are renamed in Phase 3, ship a migration that copies the old configuration and add-ons forward, and document it.

## 4. Procedure

1. **Never edit `main` of the fork.** Keep it a pristine mirror of upstream.
2. Work on the `final-effect-3d` branch, **one commit per area** (3.1 … 3.5) with messages prefixed `brand:`. This is what makes rebasing cheap and reviewable.
3. Prefer a script over manual edits for mechanical replacements (product name in packaging metadata, path renames), stored under `tools/rebrand/` in the fork, so it can be re-run after every upstream sync.
4. After each upstream sync: rebase, re-run the script, re-run the verification list below.
5. Keep a mapping file (`tools/rebrand/mapping.md`) listing old path/identifier → new, so nothing is forgotten and the decisions are auditable.

## 5. Verification checklist (run after every branded build)

- [ ] The installed application name, window title and About box show Final Effect 3D and no Blender wordmark or logo.
- [ ] Splash screen and app icons are our artwork; no Suzanne or Blender logo asset is shipped in user-visible places.
- [ ] Installer/package names, bundle id, desktop entry, AppStream metadata and file associations are ours.
- [ ] `--version` and the About box report the version without claiming to be Blender (a factual, non-endorsement statement such as "based on Blender, GPL" is fine and encouraged).
- [ ] An existing `.blend` file opens unchanged; saving and reopening works; a third-party add-on still installs and runs; `import bpy` works.
- [ ] The release bundle contains: our source archive, `COPYING`, `doc/license/*`, `release/license/licenses.json`, `NOTICE`, `AUTHORS` + our contributors.
- [ ] `git grep` above shows no remaining user-visible Blender brand strings outside the whitelist.
- [ ] The rebrand commit set still applies cleanly on top of current upstream main.

## 6. Open questions to settle before Phase 1

1. Executable/binary name: keep `blender` (maximum compatibility with scripts and CI) or rename to `final-effect-3d` (cleaner, breaks every external pipeline that calls `blender`)? Recommendation: keep `blender` internally in Phase 1 and expose the product name in packaging and shortcuts.
2. Product version numbering: continue upstream's numbers (5.4.x) or start our own (1.0)? Recommendation: keep upstream's numbers with our own suffix so upstream syncs stay legible.
3. Whether any of our own effect packs will be shipped closed-source — they cannot be, if they run inside the GPL application (see docs/COMPLIANCE.md §4).