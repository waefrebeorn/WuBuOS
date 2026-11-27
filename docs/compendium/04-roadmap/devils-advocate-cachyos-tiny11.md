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

4. **~~No non-ASCII text.~~ CLOSED.** The face now carries Latin-1 (`0xA0..0xFF`), the draw path decodes
   UTF-8 `C2`/`C3` sequences, and a codepoint the face cannot draw renders a visible `?` instead of a silent
   space. Gated by `font_case` (72 checks, mutation-proven: shrinking the range back to `0x7E` fails, and
   making the decoder return a space instead of `?` fails). CachyOS still ships 235 locales to our 0; what is
   closed here is *Latin-1 text renders*, not *the OS is localised* — a locale table is separate work.
   Remaining in this line: the bitmap path (text scale ≠ 1) has no Latin-1, so an accessibility scale shows
   `?` where an accent belongs.
5. **The text stack is one face.** No font selection, no bold/italic, no kerning, integer scaling only.
   `font_size` is now LIVE (base px, so it composes with `font_scale` and `dpi`: effective px =
   `font_size * font_scale * dpi/96`; gated by `test_settings`, mutant proves it). `font_family` still reaches
   no renderer *because only one face is baked* — that is a bake job (a second weight at the same size, with
   `--preserve` semantics), not a wiring job, and it is the next increment here.
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
