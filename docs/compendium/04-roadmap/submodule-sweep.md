# Submodule sweep: the methodology applied to trees we did not write

*Measured 2026-11-25 by `tools/tree_audit.py`. Every number on this page is a count the
script produced against the checked-out trees; nothing here is an impression.*

## Why this page exists

`docs/compendium/04-roadmap/ui-excellence-methodology.md` and `tools/gate_gui_fluent.py`
hold `src/gui` to a standard: numbers live in one header, colours live in the theme,
edges antialias by one rule, no literal in a render path. **Every imported tree has been
exempt from all of it.** The parent was being cleaned while ten submodules and vendored
directories kept the defects we had already proved were bugs.

## The measurement

| submodule | branch | files | LOC | colour | strncpy | libm | stubs | dirty | built |
|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| src/audio/bigmac | master | 1532 | 525,831 | 7,360 | 99 | 215 | 6 | 0 | yes |
| src/brain | main | 2049 | 239,699 | 199 | 42 | 894 | 4 | 0 | yes |
| src/gui/vws | main | 39 | 12,520 | 12 | 0 | 1 | 12 | **1** | yes |
| src/gui/wet | main | 26 | 10,498 | 62 | 0 | 1 | 12 | 0 | yes |
| src/kernel/WuBuKernel | main | 17 | 1,478 | 4 | 0 | 0 | 1 | 0 | yes |
| src/lua/www | main | 416 | 83,808 | 8 | 7 | 22 | 0 | **1** | yes |
| src/tools/wubugrep | main | 11 | 4,247 | 0 | 0 | 0 | 0 | 0 | own target |
| src/wemu | main | 16 | 10,401 | 1 | 0 | 1 | 0 | 0 | yes |
| src/apps/wubupad | main | 76 | 9,775 | 1 | 4 | 0 | 2 | 0 | **NO** |
| src/apps/wubuoffice | main | 661 | 71,641 | 61 | 60 | 79 | 0 | 0 | **NO** |
| **total** | | **4,843** | **969,898** | **7,708** | **212** | **1,213** | **37** | | |

"built" is decided by a `make hosted test` dry run, not by grepping the makefiles: mk
defines variables like `$(WET)`, so a literal path search reported `src/gui/wet` as
never compiled while `test_wet` compiles every file in it. The first version of this
script made exactly that mistake, which is the same species of error as the WCAG
comments: **a plausible check that was never run against reality.**

## What the numbers already say

1. **`src/apps/wubupad` and `src/apps/wubuoffice` are not in our build at all.** Two
   shipped applications, 81k lines, with no path from any target. Either they are
   orphaned (and `gate_reachability` cannot see them, because it only knows make
   targets), or they are built by some other mechanism that does not exist. This is the
   same defect as `wubu_buddy_orb.c` passing its gate while the desktop drew a PNG.
2. **`src/gui/wet`, the framework that is supposed to own shell surfaces, still draws
   with 62 colour literals and 12 `(void)arg;` stubs.** The .WET port of the dock and
   taskbar cannot be judged "done" while the toolkit itself is unthemed and stubbed.
3. **`bigmac` carries 7,360 colour literals and 215 `sqrt` calls.** It is an audio
   engine, so much of that is DSP maths and legitimate -- which is exactly why the
   sweep must not report these as violations without checking. A measure is a target.
4. **`src/brain` has 894 `sqrt` calls**; the kernel-side rule (no libm in freestanding
   units) does not apply to it, but the number explains why it links `-lm` and must not
   be pulled into a freestanding graph.

## Order of the sweep (same order that worked in src/gui)

1. **Wire or declare.** Decide wubupad/wubuoffice: a target, or an explicit "not shipped"
   note with a reason. A tree that is in the repo and not in the build is a lie of
   omission.
2. **`.WET` first**, because the dock and taskbar cannot be ported onto a toolkit that
   has 12 stubs. Its 62 literals go through the same theme/metrics owners as the parent.
3. **`wubugrep`** last: 0 colour, 0 libm, 0 stubs -- it is the proof that a submodule can
   pass the parent standard without much work.
4. **Re-run this script after each step**, and lower nothing by hand: the numbers fall or
   they do not.

## The rule this page adds

A standard enforced only inside the parent is a standard for the code that was already
being fixed. **The methodology applies to every tree the binary links, and the count is
the same count.**
