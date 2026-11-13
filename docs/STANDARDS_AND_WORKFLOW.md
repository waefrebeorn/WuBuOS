# WuBuOS standards and the workflow that enforces them

Read this before bringing old code up to standard, or before arguing about what
"up to standard" means. The rules here are the ones the tree already follows in
its best parts; `tools/std_audit.py` turns three of them into measurements so
the conversation is about numbers.

## The rules

Each rule names its owner, so there is one place to change when the rule does.

**R1 — C11, opaque structs, minimal includes.**
Public headers declare types and functions; they never define them. Structs are
opaque outside their own `.c`. A new module does not include a header it does
not use.
Owner: every file. Checked by the include-count rule (R4).

**R2 — One concern per file, ~500 lines.**
Past that, a file has stopped being about one thing and started being about
everything that was convenient. Split along concern boundaries — a `_name.c`
leaf plus a public `.h`, not a bigger file.
Measured by: `std_audit.py`, budget 600 (the covenant says 400-500; the budget is
set slightly high so legacy code is not an instant emergency).

**R3 — Names mean something.**
A bare first token on a new file names its layer (`docs/naming.md`). Kill stale,
overloaded or vague names. A rename must bind BOTH sides — definition and every
caller — or it links wrong.

**R4 — No god headers.**
An `#include` count in the dozens means a module has no boundaries. Three files
currently fail this: `src/kernel/wubu_probe.c` (176), `wubu_hw_detect.c` (161),
`src/hosted/hosted.c` (45).
Measured by: `std_audit.py`, budget 40.

**R5 — Fail closed.**
Every refusal returns -1 (or false/NULL) and leaves the caller's buffer
untouched. A partial write is worse than no write: it tells the client the data
is there when it is not.
Owners: `apps/guest_file.h`, `hosted_fs_persist.c`, `runtime/wubu_spawn.c`.

**R6 — One owner per concern. Call it; never copy it.**
Before writing something that smells like a copy, search the tree. Real examples
of reuse that already exist:
- file access -> `apps/guest_file.h` (never poke `g_fs`)
- run a command and capture output -> `gui/wubu_runline.c`
- parse a `.desktop` -> `wubu_mime_desktop.c::parse_desktop_file`
- layout geometry -> `gui/wubu_tokens.h`
- temperatures -> `kernel/wubu_thermal_read.c`

**R7 — Geometry comes from the token scale.**
Spacing, radii, borders and control metrics live in `src/gui/wubu_tokens.h`.
Nothing else may hardcode them. Enforced over the surfaces that draw the desktop
(taskbar, start menu, chrome, systray, context menus, widgets, desktop) because
that is where the scale applies -- a tree-wide "no numbers" rule produced 2852
hits on enums, colour channels and generated Wayland constants, which is noise
that teaches people to ignore the tool.

**R8 — Comments explain WHY, never narrate WHAT.**
"A void* would dangle after the wl_client is destroyed, so the destroy listener
NULLs it" is worth a line. "Set x to 0" is not.

**R9 — Behaviour gates, not assert-nothing tests.**
A test that passes while the feature is broken is worse than no test. Every new
module gets a target in `mk/tests.mk`; a gate that cannot fail is not a gate.

**R10 — Weak glue for optional modules.**
When the WM needs a shell function but headless tests must link without the
whole shell, the test provides a weak stub and the real build provides the real
one (`wm_test_stub.c`: `launch_app`, `startmenu_open`, `desktop_launch_icon`).
This is why tests do not drag in the world.

## The workflow

One module at a time, in this order. Do not try to fix the baseline in one pass.

1. **Measure.** `make test_standards` prints the current state and fails if it
   got WORSE than the baseline. Read the file it names first.
2. **Pick one offender** and decide which rule it breaks.
3. **Split or refactor** along the concern boundary. Build after every step --
   never land a batch of edits behind one build.
4. **Add or extend a gate** that would have caught the bug. If the bug was found
   by hand, the gate is the fix's other half.
5. **Re-run the audit** and update the baseline ONLY when the number went DOWN:
   `python3 tools/std_audit.py --write-baseline tools/std_audit_baseline.json`.
   Lowering the baseline after making things worse is how ratchets get abused.
6. **Commit and push** with the measurement in the message.

### Baseline hygiene

The baseline is a floor, not a score. It exists because 121 over-length files at
once is not a plan -- it is a reason to give up. The point is that next month it
must be 120, then 119. When you split a file, regenerate the baseline and watch
the count drop.

## Current baseline (2026-11-13)

- 121 first-party `.c` files over 600 lines. Worst: `audio/bigmac/tools/wb_daw.c`
  (6260), `wb_selftest.c` (4952), `wb_session.c` (2823), `wb_compositor.c`
  (2789), `brain/src/wubu_ssm.c` (2411).
- 3 god headers over 40 includes: `wubu_probe.c` (176), `wubu_hw_detect.c`
  (161), `hosted.c` (45).
- 242 geometry literals in the token-governed surfaces.
- 24 unfinished-work markers and 110 placeholders across 93 files.

## Where to start

Highest user-visible value per unit of risk:

1. `src/gui/shell/startmenu.c` (1089) -- one surface users touch constantly;
   splitting it makes search and layout work safer.
2. `src/gui/wm_input.c` (728) -- input routing; every click and keystroke goes
   through it.
3. `src/kernel/wubu_hw_detect.c` / `wubu_probe.c` -- 161 and 176 includes each;
   these two are almost certainly doing several jobs.
