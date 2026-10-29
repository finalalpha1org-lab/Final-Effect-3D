# Licence and trademark compliance — Final Effect 3D

Final Effect 3D is a derivative work of Blender. This document lists what that means in practice, what must ship with every release, and what to check before publishing anything. It is engineering guidance, not legal advice — have counsel review the first public release.

## 1. The core obligations

1. **Blender is GPL-2.0-or-later.** `COPYING` in the Blender tree states: "Blender uses the GNU General Public License … Apart from the GNU GPL, Blender is not available under other licenses."
2. **Our combined work is GPL.** This repository is GPL-3.0, which is permitted: the "or later" clause lets GPL-2.0-or-later code be combined under GPLv3. We must not strip the "or later" wording in the upstream headers, and we must not add restrictions of our own.
3. **Source for every binary.** Whoever receives a Final Effect 3D binary — free or paid — must be able to obtain the complete corresponding source of the exact version we shipped, including our modifications. Distributing binaries without that is a violation.
4. **Notice modifications.** GPLv3 §5(a) requires modified versions to carry prominent notices stating that we modified the files and the date. Practically: a `NOTICE` file at the repository root, a line in the About box, and a README statement that this is a modified version of Blender.
5. **Preserve attribution and licence texts.** Keep `AUTHORS`, all copyright/SPDX headers and `doc/license/*`; add our contributors alongside, never instead.
6. **Third-party components keep their own licences.** Blender bundles code under MIT, BSD-2/3, Apache-2.0, MPL-2.0, Zlib, Boost and LGPL terms (see the licence texts in `doc/license/`). The authoritative machine-readable inventory ships in the tree as **`release/license/licenses.json`** together with per-library files under `extern/*/` (for example Bullet, audaspace, gtest/gmock, nlohmann/json, toml11, glog/gflags, xxHash, md4c, fast_float). We must reproduce those notices in our distributions and verify the terms of anything we newly add.
7. **Verify rather than assume two specific items:** (a) Cycles is documented by the Blender project as Apache-2.0 when used standalone, but the surveyed tree contains no separate `LICENSE` file under `intern/cycles/` — confirm its terms in `release/license/licenses.json` before relying on permissive use; (b) fonts and colour-management assets in `release/datafiles/` have their own licences (Noto/DejaVu, OCIO configs) — reproduce their notices if we ship them.

## 2. Trademark

The **Blender** name, wordmark and logo (including Suzanne and the Blender icon set) belong to the Blender Foundation. Rebranding is not optional cosmetic work:

- do not ship the Blender logo, wordmark, splash or icon artwork as product identity (see docs/REBRANDING.md §3.2);
- do not use "Blender" in the product name, installer name, bundle id, package metadata, domain names or marketing that suggests the product *is* Blender or is endorsed by the Blender Foundation;
- factual, descriptive statements are fine and encouraged: e.g. "Final Effect 3D is based on Blender, © Blender Authors, licensed under the GPL";
- the Blender tree's "Official mirror of Blender" README text must be removed from the fork, as it is both a trademark misuse and factually wrong for our branch.

## 3. Release checklist (gate for every public release)

- [ ] Binaries released under GPLv3; the exact source revision tagged in the fork and archived.
- [ ] Source archive (or a written offer plus a hosted copy) attached to the same release page as the binaries.
- [ ] `COPYING`, `doc/license/*`, `release/license/licenses.json`, `NOTICE`, `AUTHORS` + our contributors included in the shipped bundle.
- [ ] `NOTICE` states that the files were modified, by whom, and when (year at minimum).
- [ ] No Blender logo, wordmark or Susan[na] asset in the bundle's user-visible surface; packaging metadata rebranded.
- [ ] No additional licence terms, EULA or DRM restrictions imposed on the GPL code or on the user's ability to modify and redistribute it. (A separate licence for *our own* non-code assets is possible — see §4 — but it must not restrict the GPL parts.)
- [ ] Third-party notices reproduced; any new dependency checked for licence compatibility.
- [ ] Checksums published; signatures/notarisation applied where the platform requires it.

## 4. Our own add-ons, presets and effect packs

The Blender Foundation's licensing guidance is that add-ons and scripts distributed to run inside Blender must be GPL-compatible (they are treated as part of the combined program, since they link against Blender's Python API and shipped libraries). Consequences for our product plan:

- effect packs, node groups and templates we ship **to run inside the application** must be GPL-compatible;
- if we ever want genuinely proprietary content, the clean architecture is a **separate program** that communicates with Blender over a documented interface (script/CLI/network) rather than an in-process add-on. The render-service architecture (PLAN §ADR-6) is the example;
- proprietary *assets* (a texture pack, a model library) are not code and can carry their own licence, as long as they are not a required part of the GPL program's operation.

## 5. Contributions to our fork

- Require a Developer Certificate of Origin sign-off (`Signed-off-by:`) on every incoming commit; add the template under `.github/`.
- Never accept code the contributor cannot license under GPL-3.0-compatible terms.
- Keep upstream-style SPDX headers on new files: `SPDX-FileCopyrightText: <year> Final Effect 3D Authors` and `SPDX-License-Identifier: GPL-2.0-or-later` (matching the file they derive from) or `GPL-3.0-or-later` for genuinely new files.
- Keep a `LICENSES/`-style or `NOTICE` inventory of every third-party addition.

## 6. What is *not* a problem

- **Charging money** for Final Effect 3D binaries, support or services: fully compatible with the GPL.
- **Keeping our branding, artwork and docs copyrighted by us**: our own contributions can be licensed by us; simply remember the combined work must still be distributable under the GPL.
- **Using Blender's .blend format, Python API and add-on API**: interoperability, not infringement.

## 7. Points to verify with counsel before the first public release

1. The exact licence of `intern/cycles/` and any other component we intend to use outside the GPL boundary.
2. The trademark position on the phrase "based on Blender" in marketing and in the About box.
3. Distribution of the bundled fonts, colour-management LUTs and studio-lights assets.
4. Whether our chosen distribution model (binaries + attached source archive) fully satisfies GPLv3 §6 for each distribution channel we use (including app stores, which impose additional restrictions and often conflict with the GPL).