// SPDX-License-Identifier: GPL-3.0-or-later
// Final Effect 3D - generic fix applier used while integrating the .fe3d / .fe3a binary
// container. Reads tools/fe3d-fix.json (an array of { file, old, new }) and applies every
// entry as an exact, idempotent text replacement: each "old" must match exactly once unless
// the "new" text is already present. Both this script and its workflow are temporary.
import { readFileSync, writeFileSync } from "node:fs";

const edits = JSON.parse(readFileSync("tools/fe3d-fix.json", "utf8"));
let changed = 0;
let skipped = 0;
for (const edit of edits) {
  const text = readFileSync(edit.file, "utf8");
  if (text.includes(edit.new)) {
    skipped += 1;
    continue;
  }
  const occurrences = text.split(edit.old).length - 1;
  if (occurrences !== 1) {
    console.error("edit for " + edit.file + " matched " + occurrences + " times (expected 1)");
    console.error("old head: " + JSON.stringify(edit.old.slice(0, 120)));
    process.exit(1);
  }
  writeFileSync(edit.file, text.replace(edit.old, edit.new));
  changed += 1;
}
console.log("applied " + changed + " edit(s), already present " + skipped);
