# Session handoff: the type stack and the accessibility gates (2026-11-28)

Read this before touching text, themes or layout. It records what is measured, what is open, and the
three traps that cost the most time.

## What shipped, in order

| Commit | What |
|---|---|
| `2c585d28` | SLERM-2027 stage 1: sfnt/cmap/metrics reader, gated 111/111 against Pillow |
| `8364d4b1` | stage 2: `glyf` outlines (simple + composite), flattening, scanline coverage |
| `3d2c5613` | two gates that refused the new module (`no gate`, stale manifest) |
| `f084837f` | Selawik vendored + digest-pinned; found the short-`hmtx` defect |
| `59ef1fcd` | stage 3b: glyph cache keyed by (gid, ppem, subpixel quarter) |
| `177f2f3b` | Selawik bake attempt, reverted; `--fallback` added to the baker |
| `adc2b40d`/`a21341ab` | revert rationale + the `replaces` field the backlog gate requires |
| `2efe616a` | SLERM-2028: cells baked from Selawik; exposed a 4px footer pad hidden by Segoe's line height |
| `9f328e9d` | `test_wubu_font_text` pins the baked table (224 glyphs, 12385 bytes) |
| `4e4d5e91` | SLERM-2029: runtime text face, quarter-pixel pen, whole-string oracle |
| `1d47677d` | SLERM-2030: 20px headings through our engine; 13 page titles consolidated at 28px |
| `8abcfb73` | link fix: enumerate targets by the helper's FILE, not the callers you remember |
| `2a8441e2` | SLERM-2031: magnified text re-rasterises (soft-edge share 78% -> 46%) |
| `e188b6b5` | SLERM-2032: control panel survives 200% text (20 failures -> 0) |
| `89e9177a` | SLERM-2033 audit installed: 2 of 56 text pairs failed WCAG 1.4.3 |
| `f5441cee` | SLERM-2033 fix: classic caption 3.95:1 -> 5.10:1; gate forced its own baseline stale |
| `6ab62f7d` | benchmark rows 451/452/453/454 updated to measured states |
| `9a5a7e8b` | non-text audit extended: 31 of 98 pairs below 3:1 |
| `cccc75f9` | SLERM-2034 part 1: scrollbar thumb visible in all 7 themes (theme 0 had thumb == track) |
| `01a0b140` | SLERM-2034 part 2: 14 cleared, 6 reclassified with reasoning, 11 baselined |
| `a1ac8e7b` | visual review of the dark-theme outline change |

## Verified numbers (do not re-derive; verify before quoting)

- `test_wubu_truetype` 18 checks; `test_selawik_font` 8; `test_wubu_text_cache` 27;
  `test_wubu_text_face` 131 (24 string rows at 13/20/26/28 px); `test_shell_text` 17;
  `test_shell_text_vbe` 13; `test_wubu_font_text` 7; `test_theme_contrast` 91 pairs (11 known
  violations, 14 exempt/info).
- Text face accuracy vs FreeType: worst advance delta 1.9px on a 20-glyph run, 0.9px on a
  44-character line; ink ratio 0.91..1.02 against a measured band of 0.89..1.05.
- Pretty text: at 200% the upscale spends 77-78% of its ink on soft edge pixels, the re-rasterised
  path 46%.
- Layout: `test_control_panel` (formerly `test_applet_layout`) runs THREE configurations: face
  titles, `WUBU_LAYOUT_FACE=none`, and `WUBU_TEXT_SCALE=2`; 276 checks each.
- Contrast: 56 text pairs, 0 fail after the fix; 91 pairs total including boundaries.

## Open goals

- SLERM-2034 (OPEN): 11 boundary violations remain -- `btn_face/win_face` (7) and
  `start_btn_face/taskbar_bg` (4). Both are controls drawn FLAT, where the fill is the boundary
  (`wm_taskbar.c:720` draws the start button as a fill-only rounded rect). This is design work.
- The shell has never been looked at by a human since the type/theme changes. The contrast gate can
  prove a ratio; it cannot prove a theme still looks like itself.
- Older, untouched: progressive JPEG/GIF animation, MP3/Vorbis, SLERM-2025 clipboard transport,
  `src/hosted/hosted_test.c` still has no target, namespace duplication in `styxfs_*`.

## The three traps that cost the most

1. **A face swap exposes a padding bug, not a layout that stopped fitting.** Five panels placed a
   footer at `h - line_h - WUBU_SPACE_XS` (4px) against a 6px floor; Segoe's line height was 3px
   taller than its ink and hid it for months. Selawik reported the true line height and the footer
   dropped. Fix the pad to satisfy the rule for ANY face (`WUBU_SPACE_SM`), and verify with BOTH
   faces.
2. **Layout must measure, never assume a character width.** `cp_text_wrapped` divided by
   `8 * scale` characters: present, used elsewhere, reviewed as correct, and wrong twice over. At
   200% it thought it had room for 48 characters and drew ~630px into a 780px line.
3. **A floor-only gate accepts over-fixes.** Scaling scrollbar thumbs to reach 3:1 from the wrong
   start point produced near-black thumbs at 18:1 -- passing, and wrong design. Pick the MINIMAL
   passing value, preserve tint, and look at the result.

## Where the oracles live

- FreeType/Pillow: `tests/fixtures/ttf_oracle.h`, `selawik_oracle.h`, `selawik_strings.h`, generated
  by `tools/make_ttf_oracle.py` and `tools/make_text_strings_oracle.py`.
- WCAG: `src/gui/theme_contrast.c` reports hex; `tools/contrast_audit.py` owns the formula; the
  baseline `tools/contrast_baseline.txt` fails in both directions.
- Pixels: ink rows / soft-edge share / painted bounds measured from renders, never from a vision
  model's opinion (it called a correctly-rendered `g` descender "clipped").
