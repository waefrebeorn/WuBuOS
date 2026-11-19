# GUI slerm workflow — Impossible OS as the design reference

Owner directive (2026-11-19, verbatim intent): our GUI is a toy — non-functional,
non-usable. Take the GUI from `github.com/rizonesoft/impossible-os`, slerm it into
WuBuOS, take more from it over time, and organize the work so every agent after
this one works better. Conserve tokens; ship verified legs, not plans.

This file is the standing workflow for that job. Read it with
`docs/HANDOFF-SOTA.md` (orientation) and `docs/compendium/04-roadmap/grunt-work-tiers.md`
(the rest of the laundry list). Every tier item below is a keep-working directive
with an evidence method and a done-when that requires real output.

## The reference, pinned

- Upstream: https://github.com/rizonesoft/impossible-os — GPL-3.0-only.
- Pinned commit: `be8c2865dcbc1eca3c765107e394c5691d90d727` (main, 2026-11-19).
- Local reference checkout: `/home/wubu/opt/reference/impossible-os` (read-only
  reference; never commit there, never edit it). If absent, re-create:
  `git clone https://github.com/rizonesoft/impossible-os /home/wubu/opt/reference/impossible-os && git -C /home/wubu/opt/reference/impossible-os checkout be8c2865`
- What it is: a from-scratch x86-64 OS with a Win11-parity Fluent desktop —
  frosted-glass compositor, TrueType text, a generated design-token system, and a
  pixel-measured shell spec. Pre-alpha by its own README; we slerm its DESIGN and
  TECHNIQUES, not its maturity claims.

## License boundary (do not blur this)

Slerm = ground-up reimplementation in our own C11 (see the `slermes` README and
the wubu-slerm-workflow skill). We do NOT copy upstream source files into this
tree, and we do not link upstream code. Upstream is GPL-3.0-only; WuBuOS ships
under the WaefreBeorn Umbrella License v3.0, so copied GPL code would relicense
the tree — that is the failure mode to avoid.

What transfers freely:
- Functional behaviour, layout measurements, token values, algorithm ideas
  (facts and techniques, not expression).
- Design assets ONLY with their own licenses checked and kept: upstream's fonts
  are Selawik/Inter/Cascadia Code (SIL OFL family) and Fluent System Icons (MIT);
  upstream's wallpapers/icons are generated from SVG sources in its resources
  dir. Adopting an asset = keep its license file next to it and record it in the
  module header. Prefer generating our own equivalents (the silk wallpaper is a
  generator, not a photograph — we can write our own generator).

Every slermed module carries a header comment: design reference = upstream repo
+ pinned commit + the upstream file(s) studied. Example shape:
`/* Design ref: rizonesoft/impossible-os@be8c286 src/kernel/gfx/gfx_blur.c (GPL-3.0).
   Ground-up WuBuOS C11 implementation; no upstream code copied. */`

## What "toy" means here — measured, not vibes

A headless capture of the product (2026-11-19, this box's persisted settings)
showed: one 8x8 VGA bitmap font for ALL text; Win98 bevels and navy title bars;
gray-on-gray terminal; empty file-manager panes; developer hint text printed on
the taskbar; a mascot blit with a solid white box. The reference desktop shows:
antialiased proportional UI type, flat Fluent surfaces with rounded corners and
soft shadows, frosted-glass taskbar/Start/flyouts, a real multi-size icon set,
and a generated silk wallpaper.

IMPORTANT verification trap: `settings_default_theme()` already ships
`THEME_DARK`, but a persisted `~/.config/wubu/settings.conf` overrides it
(this box had `theme_id: 0` = Win98). To see the PRODUCT DEFAULT, run with a
fresh HOME: `HOME=<scratch>/gui-home src/hosted/wubu -h --screenshot out.ppm`.
Never judge the default shell from this box's saved settings.

## What we already own (search before writing — law 3)

- Drawing owner: `src/kernel/vbe.c` / `src/kernel/vbe.h` — XRGB8888 back
  buffer, clipping, `vbe_blend_rect` (uniform-alpha fill), non-AA
  `vbe_fill_rect_rounded`, 8x8 bitmap font. All shell drawing goes through
  `vbe_*` (never the caller's `fb` pointer).
- Geometry owner: `include/wubu_gui_metrics.h` + `src/gui/wubu_tokens.h`
  (aliases) + gate `make test_gui_metrics`. New surfaces take numbers from
  here; extend the metric owner, never invent a second ladder.
- Theme engine: `src/gui/wubu_theme.c` — 6 runtime themes incl. `THEME_DARK`;
  `src/gui/shell/window_chrome.c` already has a Win11 caption path
  (46px full-height buttons) gated on `THEME_DARK`.
- Shell modules: taskbar, start menu, explorer, systray, clock, quick settings,
  snap, widgets, overview, lock, greeter — all under `src/gui/shell/`.
- Image decode backbone: `src/gui/wubu_image_ffmpeg.c` (subprocess ffmpeg) +
  native BMP in `src/gui/shell/wubu_wallpaper.c`. Do not write a second decoder.
- Font PARSER (no rasterizer): the wubuoffice submodule carries a clean-room
  SFNT/TrueType parser — `src/apps/wubuoffice/src/wubufont/wubufont.h`
  (sfnt tables, cmap fmt4, glyf outlines, hmtx advances; emits outlines, does
  not rasterize). It is behind wubuoffice's standalone CMake and NOT in the
  parent build today. Tier 0 decides: bridge it into the GUI build vs. slerm a
  parser leaf in `src/gui/font/`. Reuse beats duplication; the submodule stays
  untouched either way (compiling its files is not editing them).
- Compositor: WuBuCompositor (own repo, slerm'd, no wlroots) is the product
  compositor; weston is only the verification harness.

## Where slermed code lands (ownership map)

New family `src/gui/gfx/` — the compositing foundation (ARGB surfaces, alpha
blit, AA shapes, blur). Public `gui_gfx.h`, one concern per `.c`, opaque
structs, C11. It draws to the screen ONLY through `vbe_set_pixel`/
`vbe_get_pixel` so clipping and the render/present boundary stay intact.
New family `src/gui/font/` — glyph rasterizer + atlas cache + text API
(Tier 0). Shell surfaces keep their existing modules; a surface redesign is a
rewrite INSIDE its module (slerm the surface, keep the owner), not a parallel
second implementation.

## The slerm loop (every item, every agent)

1. READ the upstream core that produces the effect (not its wrappers). For a
   material that is the blur/blend math; for a surface it is the draw function
   plus the spec measurements in upstream docs/design/shell.md and controls.md.
2. EXTRACT the spec: exact sizes, colours (0xAARRGGBB), states, motion times.
   Write them into our metric/token owners first if they are new concepts.
3. RECONSTRUCT in our C11: one concern per file, opaque structs, minimal
   includes, integer math where upstream used integer math. Match semantics
   that callers depend on (blend formula, edge behaviour, corner coverage);
   document every deliberate divergence in the module header.
4. GATE with behaviour, not "it compiled": exact blend values against
   hand-computed references, coverage symmetry/monotonicity, blur invariants,
   input paths driven end-to-end. Add the target to `mk/tests.mk` and the
   objects to `GUI_OBJS` in `mk/objects.mk`; a new WM-called module must link
   in EVERY recipe that links the WM (see wubu-slerm-workflow + wubuos-wm skills).
5. WIRE into the product through the real entry point (`wm_render` call order,
   chrome, taskbar layout accessor). A module nothing calls is a museum piece.
6. VERIFY by running: the new gate, `make test_gui_metrics`, the affected
   surface gates, `make hosted` + a fresh-HOME screenshot, and pixel
   MEASUREMENT (scan rows/columns programmatically) — a vision model's
   description is not a measurement. Then `make -k -j24 test` before push.
7. COMMIT per logical leg with the attribution header in place; push per the
   wubu-integration-workflow skill. Update this file's tier status in the same
   commit (mark a leg DONE only with the gate line + commit SHA recorded here).

## Tier 0 — foundation (blocks everything; do these first)

0.1 DONE (this session): compositing foundation `src/gui/gfx/` — ARGB
    surfaces, exact src-over alpha blit to VBE (clip-honouring), AA rounded
    rect, clamped-edge separable box blur + 3-pass gaussian approximation.
    Gate: `make test_gui_gfx`. Design ref: upstream gfx_core/gfx_blend/gfx_blur.
    Deliberate divergences: straight (not premultiplied) alpha in surfaces;
    clamp-to-edge blur (upstream shrinks the window at edges, which darkens
    acrylic borders).
0.2 Wire the foundation into chrome: soft drop shadow under active windows in
    `THEME_DARK` (blur a rounded mask once per window geometry, cache it,
    composite before the frame). Evidence: pixel scan shows a falloff band
    outside the frame; gate asserts shadow present when focused, absent when
    not, and cache reuse (no re-blur on an unchanged frame). Done when
    `make test_wm` + the new shadow gate pass and a fresh-HOME capture shows it.
0.3 Typography: kill the single-bitmap-font toy. Decide the wubufont bridge
    (probe: can the parent build compile the submodule's parser files without
    editing the submodule? if yes, bridge; if no, slerm a parser leaf). Then
    slerm the rasterizer + atlas cache (upstream gfx_text.c/font_mgr.h: bake
    ASCII at common sizes, LRU for the rest, PMM/heap discipline for long-lived
    glyph bitmaps). Adopt one OFL UI face + one OFL mono face with license
    files, or generate our own. New text API in `src/gui/font/`; bitmap font
    stays as the bare-metal fallback. Evidence: gate renders a known string
    and asserts glyph coverage + advance widths differ per character (the
    bitmap font's fixed advance is the toy signature); a capture shows
    proportional antialiased text. Done when title bars, taskbar and Start use
    it and `make test_font_case` still passes for the fallback path.
0.4 Fluent token upgrade: bring `THEME_DARK`'s palette to the reference's
    dark/light token sets (upstream docs/design/tokens.json is the measurement
    source; values are facts). Extend the metric owner with the new concepts
    (materials, elevation, motion) instead of literals in surfaces. Evidence:
    `make test_gui_metrics` extended to assert token agreement; WCAG audit
    (`wubu_theme_contrast_audit`) stays >= 3.0.
0.5 Taskbar de-toy: remove the rotating developer hint text from the classic
    taskbar path (`src/gui/shell/wm_taskbar.c`), and in the modern path replace
    beveled tiles with flat subtle-fill tiles + running-indicator pills
    (upstream shell.md Taskbar section: 40px buttons, 4px gap, 3px indicator,
    6px idle / 16px focused). One layout accessor already exists
    (`taskbar_modern_layout`) — keep it the single geometry owner. Evidence:
    gate drives click/hover through the same rects; capture shows indicators.
0.6 Desktop icon set: replace programmer-art glyphs with a real multi-size
    icon set. Either adopt upstream's SVG sources under their license and bake
    to RGBA tables (wubuos-wm skill, references/baked-art-assets.md), or
    generate our own set. Bake at 16/24/32/48/96; blit through the new alpha
    path (this also fixes the mascot's white box). Evidence: gate asserts
    alpha-channel edges (corner pixels transparent, not white); capture scan.
0.7 OOBE default agreement: a fresh-HOME capture (2026-11-19) shows the
    greeter's theme list cursor on Win98 Classic while
    `settings_default_theme()` ships `THEME_DARK`. The first-run choice must
    default to the product default, and taking an offer must APPLY through
    the settings object before signing (wubu-hosted-product skill). Evidence:
    gate asserts the greeter's initial selection == settings default theme id;
    fresh-HOME capture walks Next and reads the LIVE theme afterwards.

## Tier 1 — shell surfaces (each is a slerm INSIDE the existing owner)

1.1 Acrylic materials: taskbar/Start/flyouts/menus get cached frosted glass
    (blur the backdrop region once, tint, grain, 1px edge highlight; recompute
    only when the backdrop changes — upstream shell.md Materials table has the
    exact tint/opacity/blur per surface). Add a transparency-off fallback
    (solid tint) and gate both. Never blur per frame.
1.2 Start menu rebuild to the reference layout: 640x720 clamped, search pill,
    6x3 pinned grid (96x84 tiles), recommended two-column list, 64px footer
    with user + power. Keep `startmenu_db.c`'s real app/file index — the slerm
    is the surface, the data path stays. Gate: search typing returns real
    results (ASCII path, not scancodes), pinned launch works, Escape/outside
    click closes.
1.3 Window chrome glass + motion: active frame glass, inactive solid; open/
    close/minimize transitions per upstream motion tokens; reduced-motion
    setting makes them instant. Gate the state machine, not just pixels.
1.4 Explorer Win11 layout: tabbed 40px title, 48px address row, 48px command
    bar, 220px nav pane with accent selection pill, details/icon views, 24px
    status bar (upstream shell.md File Explorer). The two filesystem stores
    (hosted_styxfs vs styxfs_host) must be unified FIRST or the view lies —
    see wubu-hosted-product skill references/guest-filesystem.md.
1.5 Terminal canvas: solid dark content area (upstream terminal tokens),
    proper scrollback rendering, prompt directly under the last line. The
    gray-body/green-text/floating-prompt state is a defect, not a style.
1.6 Settings as the one home: fold Control Panel pages into Settings
    (upstream shell.md Settings frame); keep canonical names as launch aliases.

## Tier 2 — controls and polish

2.1 Controls library slerm (upstream controls.md + src/desktop/controls.c):
    button variants, toggle switch, check/radio, slider, text box with focus
    underline, combo, list/tree selection pill, tabs, progress, scrollbars,
    tooltip, dialog with smoke scrim. Land as `src/gui/controls/` family;
    migrate apps onto it one at a time (each migration is its own gated leg).
2.2 Quick settings + notifications/calendar flyouts to spec (360 wide, 12px
    anchors, toggle grid, sliders, month grid, focus session).
2.3 Alt+Tab thumbnails, Task View, snap layouts flyout (six layouts), toast
    stack (max 3, dwell paused on hover).
2.4 Lock/sign-in screens to spec; boot splash parity with WuBuFW handoff.
2.5 DPI scaling: one scale factor through the metric owner; icon size picks
    nearest baked size >= scaled; blur radii scale.

## Tier 3 — take MORE from upstream (beyond the GUI)

3.1 Token drift gate: upstream generates its C header from tokens.json and
    fails the build on drift. Slerm that pattern for our metric/token owners
    (generator + `--check` in test_docs or a dedicated gate).
3.2 Parity inventory: upstream keeps a machine-readable inventory of reference
    measurements and checks the built OS against it. Adopt the pattern for our
    shell specs (a JSON inventory + a gate that compares accessors against it).
3.3 Their docs-coverage discipline (every roadmap section cites the design it
    implements) — extend our test_docs ethos, not a second doc system.

## Do NOT slerm

- Their kernel/boot/NT stack: WuBuOS has its own (WuBuFW, ZealOS-derived
  kernel, WuBuKernel, VSL). The GUI is the job; kernel ideas go through the
  normal research ledger, not this workflow.
- Their AI-agent doctrine files (CLAUDE.md/AGENTS.md): ours is the covenant +
  HANDOFF-SOTA. Do not import another orchestrator's rules.
- Anything that would put GPL code in the tree (see license boundary).

## Status ledger (update in the same commit that closes a leg)

- 0.1 DONE (2026-11-19) — `make test_gui_gfx` ALL PASS, 0 warnings;
  `make test_gui_metrics` ALL PASSED; `make test_docs` 581/0; hosted green.
- 0.2 DONE (2026-11-19) — `src/gui/shell/wm_shadow.c`: cached elevation shadow
  (upstream elevation.window_active = offset 16 / blur 32 / alpha 94), focused
  window only, THEME_DARK only, high-contrast suppressed. Gates:
  `make test_wm_shadow` ALL PASS + test_wm 54/54 (blur-count cache oracle,
  focused/unfocused pixel differential). Pitfall found: the idle screensaver
  blanks the screen mid-test — disable it in render-differential tests.
- 0.7 DONE (2026-11-19) — greeter initial theme cursor now reads the settings
  default; gate asserts selection == settings theme id (test_greeter 28/28).
- 0.3–0.6, 1.x–3.x OPEN.