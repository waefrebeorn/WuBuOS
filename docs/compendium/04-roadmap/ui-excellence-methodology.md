# UI excellence: a measurable methodology

Status: **active**. This is the method, not the taste. Every clause is either a
number, a procedure, or a gate; anything that cannot be measured is listed as a
declared gap rather than asserted.

The premise: "looks good" is not a property we can argue about, but **legibility,
consistency, coverage and edge quality are**. Those four cover nearly everything a
person means when they say an interface looks bad.

## 0. Sources of truth (never memory)

| Question | Source | Where |
|---|---|---|
| What does the reference look like? | harvested images | `docs/reference/*/REPORT.md` |
| What number do we use? | `include/wubu_gui_metrics.h` | single owner |
| What palette? | `wubu_theme.c` (`THEME_*`) | single owner |
| Does the built product obey? | `tools/gate_gui_fluent.py` + this method's gates | ratchet |

Real values already recovered (quoted, not estimated):

- Windows 11 26100.1742: accent `#0078D4` (`ColorizationColor=0xC40078D4`), `LogPixels=96`,
  `SegUIVar.ttf` + `SegoeIcons.ttf` shipped, Segoe UI and Segoe UI Variable
  **metric-compatible** (cap 1434 / x 1024 @ 2048 upem), `WallpaperStyle=10`.
- Plasma 6.8.3 / Breeze Dark: panel **30px floating, 8px per edge**, 9-slice edges 4px,
  window `#202326`, view `#141618`, text `#fcfcfc`, accent `#3daee9`,
  **Noto Sans 10pt @ 96dpi**, cursors 24px, `AnimationDurationFactor=0.25`,
  and **no corner radius anywhere (41 rects, rx=0)**.
- Not recoverable from either image: Windows per-control caption/padding metrics
  (`VARIANT`/`CMAP` are undocumented blobs; `WindowMetrics` absent). Declared gap.

## 1. The four measurable axes

### 1.1 Legibility (contrast)
Every text/background pair must meet **WCAG AA (4.5:1)** for body and **3:1** for
large/bold, computed from the actual palette, not from intent. `tools/gui_audit.py`
computes the whole matrix and fails below threshold. A theme that cannot pass is
not shippable, however it looks in a mockup.

### 1.2 Consistency (geometry)
One spacing scale, one type ramp, one radius set, one elevation set:

- Spacing: the **4px grid** (`WUBU_SPACE_XS..XXL`), and nothing off it.
- Type ramp: 4 sizes only (caption 11, body 13, subtitle 16, title 20), each with a
  line height fixed in the metrics header.
- Radii: window 8, control 4, pill = height/2. No other radius may appear.
- Elevation: 2 levels (window shadow, flyout shadow). No third.

The audit counts **distinct values in use**; a value outside the set is a finding.

### 1.3 Coverage (is anything missing?)
A UI is "plain" when graphics are absent, not when they are simple. Coverage is
countable:

- app icons: every entry in the launcher/dock must resolve to an icon, or be listed
- file-type icons: every extension the explorer renders
- action icons: back/forward/refresh/search/new/open/save/close/min/max
- state graphics: empty, loading, error, success, locked, offline

The audit prints a coverage table (present / missing / placeholder) — a percentage,
not an opinion.

### 1.4 Edge quality (is it blocky?)
Two numbers: **antialiased primitives in use** (rounded fill/rect, 3d edges, text) and
**hard-transition ratio** — the share of 1px neighbour pairs that jump by more than 24
levels, measured on a rendered frame. A good UI at 96dpi sits low; a 1-bit UI sits high.
Reported every render, compared to the previous run.

## 2. The loop (never one-shot)

For each surface, in this order, and never skipping 3:

1. **Render** the real product (`wubu --screenshot`, fresh profile).
2. **Measure** with `tools/gui_audit.py` (contrast, geometry set, coverage, edges).
3. **Look** at the frame with eyes (a vision pass), and write down only what the
   numbers cannot express (composition, weight, rhythm).
4. **Fix one layer** — the primitive or the owner, never the instance
   (see `x11-reality-check.md`: patching widgets under a broken primitive wasted a
   whole session).
5. **Re-measure**. A fix that does not move a number is not a fix, and a change that
   moves a number the wrong way is reverted, not argued about.
6. **Gate** it, then commit.

## 3. Icons: a system, not drawings

### 3.1 The grid
- Canvas 24×24 (drawn at 24, 20, 16 — the three sizes we ship).
- **2px stroke** at 24, 1.75 at 20, 1.5 at 16 (self-similar, not scaled).
- Corner radius 2 on strokes; 4 on filled shapes. Nothing sharper.
- Optical alignment to the 24-grid: circles and diagonals may exceed the box by 1px
  where it reads better — a rule that serves legibility, applied deliberately.
- Two weights only: outline (default), solid (active/filled state).
- One palette source: theme tokens, never per-icon hex.

### 3.2 Production pipeline
`SVG (source) -> bake -> C sprite table / .WET surface`.

- SVG is the master, one file per icon, in `assets/icons/<category>/<name>.svg`.
- Baking uses the existing sprite-baking path (see the `wubuos-sprite-asset-baking`
  skill); the bake is reproducible and the output is committed, never hand-edited.
- **No bitmap in a render path.** The buddy taught this: a PNG sprite came back into
  the desktop because `wm_render` still called the old draw function. Icons follow the
  same rule — if a surface needs an icon it asks the icon system, and the system's
  backend is data (a table or a .WET surface), not a decoded image at draw time.

### 3.3 Naming and coverage
`<category>.<name>` (`action.close`, `file.image`, `app.terminal`). The audit resolves
every icon name the product asks for; an unresolved name is a build finding, not a
blank square.

## 4. Graphics: what "plain" actually means

A desktop reads as plain when three things are missing, and each has a different fix:

1. **No depth** — every surface is one flat fill. Fix: the 2-level elevation set, a
   1px hairline, and *coverage-antialiased* rounded corners (already owned by
   `vbe_fill_rect_rounded`).
2. **No hierarchy** — everything is the same weight and size. Fix: the 4-step type ramp
   plus a strict 4px grid.
3. **No identity** — no imagery at all. Fix: one wallpaper set, one icon set, one
   mascot rendered as a shader (not a sprite), all theme-aware.

## 5. Gates that keep it honest

| Gate | Measures | Fails when |
|---|---|---|
| `gate_gui_fluent.py` | colour/geometry literals | any new literal appears (ratchet) |
| `gui_audit.py --contrast` | palette matrix | any pair below AA |
| `gui_audit.py --geometry` | values in use | a value outside the metric set |
| `gui_audit.py --edges` | hard-transition ratio | ratio rises above baseline |
| `gui_audit.py --coverage` | icon resolution | any requested icon unresolved |

## 6. Declared gaps (do not pretend)

- Windows caption/padding metrics: unreadable from the harvested image.
- Icon optical sizes for tray/clock: compiled into Plasma `.so`s, not shipped as data.
- HiDPI scale: no `kscreen` state in the CachyOS rootfs; our scale story is our own.
