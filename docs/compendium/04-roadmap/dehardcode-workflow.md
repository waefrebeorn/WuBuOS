# De-hardcoding workflow — one owner per constant

Owner directive (2026-11-19): fix the myopic hardcoding scattered through the
modules; the OS was shortsighted in the beginning. This is a reorganization and
workflow effort that pays dividends — go hard, go meticulous.

This file is the standing ledger for that job. It complements
`docs/STANDARDS_AND_WORKFLOW.md` (the rules and their owners) and the GUI slerm
workflow (`docs/compendium/04-roadmap/gui-slerm-impossible-os.md`): every surface
slermed from the reference is ALSO de-hardcoded as it lands, so the two efforts
compound instead of fighting.

## Org-wide scope (owner directive 2026-11-19, later same day)

The effort covers the WHOLE ~WuBu~ family, not just the GUI surfaces:
`docs/CONSTANTS.md` is the front door (where every kind of constant lives,
per layer of the organism), and `tools/hardcode_census.py` measures the
org-wide frontier (duplicate defines, path literals, env knobs, literal
allocations, literal sleeps) in any checkout via `--root`. The per-repo
frontier and triage classes live in
`docs/compendium/04-roadmap/hardcoding-census.md`.

Protocol for sibling repos (wubunos, wubuwizard, wubucompositor, wubulens,
W-Code, WuBuKernel): run the census with `--root <checkout>`, work CONFLICTS
first, edit ONLY in that repo's own checkout, gate with THAT repo's tests,
commit/push per its branch protocol, record the numbers in the census doc.
Never edit a submodule's content from the body; never carry a body gate
claim across a repo boundary.

## The principle

A constant hardcoded in a surface is a second owner of a decision that already
has one. It silently disagrees with theme switching, the `/theme` namespace,
dynamic colour, high-contrast, DPI scaling and the metric grid — every mechanism
the OS built to change that decision at runtime. The fix is never "delete the
number": it is MOVE the number to the owner that already exists, or create the
owner once and point every consumer at it.

## The owners (search here before inventing)

| Concern | Owner | Gate |
|---|---|---|
| spacing, radii, borders, control sizes | `include/wubu_gui_metrics.h` (canonical) + `src/gui/wubu_tokens.h` (aliases + named surface constants) | `make test_gui_metrics` |
| surface colours, state fills, semantic rows | `src/gui/wubu_theme.c` palette table (all 6 themes) via `tc()` | `make test_wm`, contrast audit |
| theme-independent control art / brand art | named in `wubu_tokens.h`, or `brand-art` / `icon-identity` line markers | `tools/std_audit.py` skips marked lines |
| user-tunable caps, timeouts, allocation bounds | revolver pattern (`wubu_revolver_init` + env override, clamped, refusal reported) — see the wubu-revolver-pattern skill | `make test_revolver` |
| persisted user choices | `src/gui/wubu_settings.c` object; apply through the setter, THEN persist | surface gates |
| paths, ports, device names | module-level named constants or settings; never inline in logic | review |

## The measurement (ratchet, not vibes)

```sh
python3 tools/std_audit.py                     # report
make test_standards                            # gated: fails only on GROWTH
python3 tools/std_audit.py --write-baseline tools/std_audit_baseline.json
```

Rules measured: R2 file length, R4 include fan-in, R7 geometry literals and
R11 theme-bypassing colours in the governed desktop surfaces. The surface list
is deliberately narrow (`TOKEN_GOVERNED` in the script): a tree-wide "no
numbers" rule is noise that trains people to ignore the tool. New categories
earn their way in the same way — narrow scope, real offenders, a ratchet key.

Baseline hygiene: regenerate ONLY when the count went DOWN for the key you
improved. The 2026-11-19 baseline: 126 long / 3 fat / 136 geometry / 21 color.
`startmenu.c` is the worked example: 87 geometry + 18 colour hits -> 0, with
zero behaviour change except two deliberate correctness fixes (XP item text was
white-on-light via the sidebar colour; the ellipsis reserve now measures the
dots instead of guessing 6px).

## The batch protocol (every agent, every batch)

1. Run the audit; pick the file with the most hits in ONE concern.
2. Read the WHOLE file before editing (render + click + hover + key paths —
   geometry shared by draw and hit-test must move together or clicks drift).
3. For each literal: find the existing owner token; if none exists, name it in
   the owner ONCE (value unchanged unless the reference/grid demands otherwise,
   and then say so in the commit). Semantic colours go into the theme table for
   ALL themes, not just the one you're looking at.
4. Behaviour gates: run the surface's gate + `make test_gui_metrics` +
   `make test_wm`. A pixel-identical refactor is the default; any deliberate
   visual change is named in the commit body.
5. Re-run the audit; write the baseline only for keys that dropped.
6. Commit with the before/after numbers; push per wubu-integration-workflow.

## Frontier ledger (2026-11-19, after the startmenu + taskbar batches)

Geometry / colour hits per governed surface — work top-down:

- `src/gui/shell/wm_ctxmenu.c` — 16 / 1. NEXT.
- `src/gui/shell/wm_systray.c` — 16 / 3.
- `src/gui/shell/win11_widgets.c` — 17 / 0.
- `src/gui/shell/window_chrome.c` — 0 / 0. DONE (batch 3: caption_close_hover
  theme semantic, reference #C42B1C; ChromeButton enum aligned to the live
  hit-test convention and used by both sides; labels centre via
  vbe_text_width — strlen*8 assumed an 8px cell, the font advances 6px).
- `src/gui/shell/startmenu.c` — 0 / 0. DONE (batch 1).
- `src/gui/shell/wm_taskbar.c` — 0 / 0. DONE (batch 2, with GUI slerm 0.5:
  flat Fluent tiles, running-indicator pills gated by test_taskbar_indicator,
  developer hint text removed; classic era bevels kept, every constant named).

Beyond the governed surfaces (ledger only — no standing gate yet, scope each
before measuring): explorer's internal ladder (`explorer_internal.h` EX_* vs
the metric owner), term cell metrics, controlpanel row heights, runtime
timeouts/caps that should be revolvers, hardcoded paths. Each becomes a gated
batch when picked up; do not boil the ocean in one pass.

## Status

- 2026-11-19 batch 3: window_chrome.c geometry 16->0, colour 10->0. New theme
  semantic caption_close_hover (all 6 themes, reference #C42B1C). Unused
  ChromeButton enum contradicted the live hit-test numbering — aligned and
  used by chrome_hit_test_button + chrome_handle_button_click. Text centring
  via vbe_text_width (was strlen*8, off-centre). Audit markers gained
  percent-math / btn-id classes. Ratchet: geometry 65->49, colour 14->4.
  Gates: test_wm 55/55, test_gui_metrics, test_standards clean, hosted,
  test_docs 581/0.
- 2026-11-19: R11 colour rule added to std_audit + ratchet; startmenu.c fully
  migrated (geometry 87->0, colour 18->0); theme table gained semantic
  `startmenu_shutdown_bg/fg` for all 6 themes; tokens gained the menu/start
  constant block; stale `WUBU_MENU_ROW_H 26` reconciled to the canonical row 24.
  Gates: test_wm 54/54, test_startmenu 6/6, test_gui_metrics, test_standards
  clean, hosted green, test_docs 581/0.
- 2026-11-19 (batch 2): wm_taskbar.c fully migrated (geometry 71->0, colour
  7->0) together with GUI slerm 0.5 — modern tiles are flat Fluent surfaces,
  running state reads from 3px indicator pills (6 idle / 16 focused, accent),
  the cycling developer-hint text is gone; classic path keeps its era bevels
  with every constant named in wubu_tokens.h. Tree ratchet: geometry 136->65,
  colour 21->14. Gates: test_wm 55/55 (new test_taskbar_indicator),
  test_startmenu 6/6, test_standards clean, hosted green, test_docs 581/0.