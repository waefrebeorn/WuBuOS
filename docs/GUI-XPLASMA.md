# XPlasma-Plus → WuBuOS GUI

Reference: <https://github.com/andymenard/XPlasma-Plus> (GPL-3.0), a fork of
hotsocket's XPlasma that reproduces the Windows XP Luna desktop on KDE Plasma.

## What was actually taken

**Nothing from its source.** It is GPL-3.0 C++/QML for KDE; WuBuOS is C11 with
its own compositor. What transfers is the *design*, and one design element was
already present here.

### Already had it (checked, not assumed)

`src/gui/dosgui_window_chrome.c` already draws the Luna caption as a vertical
gradient via `theme()->gradient_title` + `vbe_vgradient`, lighter at the top and
darker at the bottom. An earlier claim in this session that our title bars were
flat was wrong — the gradient is there.

### Genuinely missing: the focus flash

XPlasma-Plus ships `effects/xp-caption-animation`: when a window takes focus its
caption briefly brightens. WuBuOS had no equivalent.

`src/gui/wubu_caption.{c,h}` implements the two primitives in integer C11:

- `wubu_caption_px(colors, y, h, active)` — the row colour of a gradient
  caption; inactive ones stay flat and dim so focus is readable.
- `wubu_caption_focus_glow(base, accent, t)` — parabola envelope `4t(1-t)`,
  exactly `base` at `t<=0` and `t>=1`, brightest mid-animation. No libm.
- `wubu_caption_luma(rgb)` — Rec.601 integer luminance for contrast checks.

`src/gui/wubu_caption_test.c` — **15 checks, 0 failures**: flat inactive, real
gradient, monotonic luminance, degenerate heights, clamped rows, NULL safety,
exact endpoints for the flash, brightening at mid-flash, luma sanity.

`src/gui/wubu_caption_demo.c` renders a 480×160 PPM of all three bands. It was
generated and inspected: top band lighter-at-top blue gradient, middle flat grey,
bottom band bright mid and dark at both ends.

## Not wired into the window manager yet — and why

The chrome painter has no frame clock. There is no tick source exposed to
`src/gui/`, so `t` has nowhere to come from. Wiring the flash properly needs:

1. a monotonic millisecond counter reachable from the GUI layer, and
2. a `focus_tick` per window set in `dosgui_wm_set_focus()`.

Until both exist the effect is a tested library plus a rendered proof, not a
live behaviour. Do not describe it as running in the WM.

## Next candidates from that project

- IE6-style browser frame (`browser/ie6`) — maps onto our Win98 shell chrome.
- Wallpaper collection — only if the licence on each image is checked first;
  the repo credits WindowsAesthetics, which is not a licence.
