# Devil's advocate against the reference images (CachyOS, Tiny11)

Method: measure the reference trees, then measure the same axis on this tree, and only call something a gap
when both sides have a number. Nothing here is from memory; every count came from `find`/`grep` on the images
under `/home/wubu/opt/refos/` on 2026-11-26.

## Reference measurements

| Axis | CachyOS rootfs | Tiny11 extract | This tree |
|---|---|---|---|
| font files | 870 | — | 1 baked AA face (0x20..0x7E) + 2 bitmap tables (90 glyphs) |
| font packages | 9 | — | 0 (no font system at all) |
| locales | 235 | — | 0 |
| systemd services | 438 | — | archd autostart (different design, not a stub) |
| binaries in /usr/bin | 3115 | — | 17 app sources |
| shell packages | — | 44 SystemApps | 14 Control Panel pages, all mapped in the gap ledger |

## FIXED in this turn (each mutation-proven)

1. **Saved accessibility settings were never applied at boot.** `wubu_settings_init()` set the theme and
   stopped; `wubu_settings_apply_live()` — the only path that pushes `a11y.font_scale` and
   `a11y.high_contrast` into the renderer — had exactly one caller: `src/apps/control/control.c:343`, i.e. a
   user opening Settings and changing something. A saved font scale did nothing on every boot.
2. **`fonts.dpi` was decorative.** Written at `wubu_settings_io.c:54`, read at `:198`, consumed by nothing: a
   192-dpi machine drew exactly like a 96-dpi one. Text scale is now `font_scale * dpi/96`.
3. **Five codes had no bitmap glyph** (` (96), `{`, `|`, `}`, `~` (123..126)) and drew as a space in the
   bitmap path — JSON and shell code at a non-unit text scale rendered with holes.

## OPEN, with the measurement that makes each one a gap

4. **No non-ASCII text.** `src/kernel/vbe.c:511` draws any byte outside the tables as a space, and the AA face
   covers only `0x20..0x7E`. An accented filename, an em dash, or any non-English string renders BLANK — not
   mojibake, absent. CachyOS ships 235 locales and 870 font files. Smallest real increment: a UTF-8 decoder
   plus one Latin-1 supplement range in the baked face, gated by drawing every code point 0xA0..0xFF and
   asserting ink for each; then a locale table for the ~40 languages the OS's own strings would need.
5. **The text stack is one face.** No font selection, no bold/italic, no kerning, integer scaling only. The
   settings already carry `font_family` and `font_size`; neither reaches a renderer — same class as `dpi` was.
   Smallest increment: make `font_size` select between two baked sizes and gate that the rendered cap height
   actually changes.
6. **Scope fact, not a gap:** 17 app sources against 3115 binaries is the difference between an OS and a
   distribution's whole userland. Recording it so the number is not mistaken for parity work.

## Retracted during the audit

- A first reading claimed normal text lost `{|}~`. It does not: at scale 1 `vbe_draw_text` goes to
  `aa_draw_text`, whose baked face covers `0x20..0x7E`. The hole was only in the bitmap fallback used at
  accessibility scales, and my first gate passed with the fix removed because it sampled scale-1 text. The
  gate now draws at text scale 2 and fails 5 checks when the fix is reverted.

## Why these three fixes mattered more than the ledger

The gap ledger said 42/42 owned, and it was not lying about *features*. What it does not measure is whether a
setting a user changes actually reaches the renderer. Two of these three findings were settings that were
written, read and ignored — the same failure the ledger's "wired" verdict cannot see, because the code that
would consume them never existed.
