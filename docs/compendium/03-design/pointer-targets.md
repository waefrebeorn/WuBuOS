# Pointer targets: the box you can click must be the box you can see

Two measurements from outside this tree settle what an interactive target has to be.

**Windows' own desktop metric is the CELL, not the glyph.** Icon spacing is stored as `IconSpacing` /
`IconVerticalSpacing` under `HKCU\Control Panel\Desktop\WindowMetrics` (default `-1125`, i.e. 75 px at 96 dpi),
and Microsoft's own note on it says horizontal spacing also determines *how wide the text under the icon can
be*. The label is part of the target — not decoration hanging below it.
(Source: windowsdigitals.com "How to Change Desktop Icon Spacing in Windows 11", 2023-08; the same keys are
documented in Microsoft Q&A.)

**Minimum target sizes** (humanstandards.org, collected from the standards themselves):

| Standard | Size | Level |
|---|---|---|
| WCAG 2.5.8 | 24×24 CSS px | AA (required: ADA, Section 508, EAA) |
| WCAG 2.5.5 | 44×44 CSS px | AAA |
| Apple HIG | 44×44 pt | platform guideline |
| Material 3 | 48×48 dp | platform guideline |

Adjacent targets: 8 px gap recommended; 16 px+ between a destructive control and a safe one.

## The rules that follow

1. **A hit box is computed from the same function as the drawn box.** If the layout asks
   `desktop_cell_h()`, the hit-test asks `desktop_cell_h()`. A second constant is a second answer, and the one
   that is wrong is always the one the user is pointing at.
2. **The label is inside the target.** Windows says so; users aim at the word.
3. **No target under 24×24 px**, and prefer 44×44 for anything primary.
4. **A test must probe the EDGE of the box, not 4 px inside it.** The old icon check probed
   `ICON_SIZE + 4` and passed while the label area was dead.

## What this fixed

`icon_hit_test()` accepted `0 <= dy < DOSGUI_ICON_SIZE + 12` while the cell is 84 px, so the label area of every
desktop icon was unclickable — measured live: a double-click 20 px below the icon's centre created no window,
while the same click higher up selected it. Now the box is `desktop_cell_h()` (the drawn cell), gated by
`test_wm` probing the cell's bottom edge and one pixel past it, with a mutant that restores the old constant.

## Still to check against these rules

- Resize grips (a 4 px band is conventional but is below 24; Windows widens the grab region beyond the visible
  edge — the same trick belongs here).
- Scrollbars: the visible thumb is narrow; the grab region should be wider than it looks.
- Toast close buttons and taskbar overflow chevrons.
