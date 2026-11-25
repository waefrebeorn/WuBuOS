# The X11 bridge: a reality check

Written after a devil's-advocate pass with **six real X clients**, not with
self-written tests. Everything below is measured on this machine, on the commit that
adds `wubu_x11_windows` + events + GC/fill handling.

## What was claimed, and what the measurement says

The earlier claim was "a real client runs to completion against it". That was true of
**`xdpyinfo`**, which is a *prober*: its whole job is to ask questions and print the
answers. It never draws. The claim was therefore about the question-answering path,
not about hosting applications — and the difference is the entire remaining work.

| client | requests | outcome |
|---|---|---|
| `xdpyinfo` | 9 | completes, prints our screen, `unknown=0` |
| `xlogo` | 40 → 46 after events | completes requests, then **draws** (5 × PolyFillRectangle) |
| `xclock` | 34 | dies: `BadImplementation` on **47 QueryFont** |
| `xcalc` | 27 | dies: `BadImplementation` on **85 AllocNamedColor** |
| `xeyes` | 56 | dies: `XInputExtension` missing |
| `xmessage` | 8 | needs an argument; no server-side failure |

Opcode numbers verified against `/usr/include/X11/Xproto.h` (120 core request
definitions; opcodes 0–127 are core, 128–255 are dynamically assigned extensions,
per x.org's OpCodes page).

## The finding that matters: events, not drawing

`xlogo` mapped its window and then sat silent. It had 40 requests done and no
`Expose`. Three separate things were missing before it drew anything:

1. **`CreateWindow`'s value list** carries `CWEventMask` (bit 11). Xt puts the mask
   there, so a client that "sets no attributes" still expects `Expose`.
2. **`MapSubwindows` (9) is parentage.** xlogo maps a *shell* and expects its
   *child* — the widget that actually selected `ExposureMask` — to be mapped too.
   Without parentage tracking the child never maps, so the mask is never armed.
3. **`Expose` must be sent when the mask is selected on an already-mapped window.**
   Waiting for a map that already happened means waiting forever.

With those three, `xlogo` went from 40 requests and silence to 46 requests and
**five `PolyFillRectangle`** calls. That is the difference between "the server
answers" and "the application draws".

## Drawing: proven exact, on a controlled rectangle

`PolyFillRectangle` + GC foreground are implemented. A hand-built client drew
`(10,10) 20x30` in white and the frame was checked at five coordinates: inside,
top-left corner, last pixel, one past the right edge, outside. All five exact.

This had to be proven with a controlled request, because `xlogo`'s own traffic mixes
background clears with **shape requests that are not implemented** (`PolyLine`,
`FillPoly`, arcs, text). Its five fills produce two white bars, not the X logo — the
pipeline is right, the *coverage* is not.

## What a real application needs, in order

Measured from the trace, not guessed:

1. **Events** — `Expose` (done), plus `ConfigureNotify`, `MapNotify`, key/button, and
   an event queue that does not require a request to arrive before it flushes.
2. **Fonts** — `QueryFont` (47), `ListFonts` (49), `ListFontsWithInfo` (50),
   `OpenFont` (45), `QueryTextExtents` (47 is QueryFont; text extents are 48).
   Every Xt client that draws text blocks here.
3. **Colours** — `AllocNamedColor` (85), `AllocColor` (84), `LookupColor` (92).
4. **Cursors** — `CreateGlyphCursor` (94), `CreateCursor` (93), `RecolorCursor` (95).
5. **Shape drawing** — `PolyLine` (64), `PolySegment` (65), `FillPoly` (68),
   `PolyArc` (67), `ImageText8` (75), `CopyArea` (61).

## What a *toolkit* needs (research, cited)

- GTK and Qt require `XRender`, `XFixes`, `XKB`, and usually `Composite` and
  `MIT-SHM`; Wine's own `configure.ac` probes for `shape.h`, `XInput.h`,
  `XInput2.h`, `XShm.h`, `Xfixes.h`, `Xinerama.h`, `Xrandr.h`, `Xrender.h`,
  `xf86vmode.h`, `xf86vmproto.h` — nine extensions plus core.
- `MIT-SHM` exists because `PutImage` through the socket is the dominant cost of a
  naive X server; every serious implementation shares memory instead.
- Xwayland exists precisely because writing an X server is expensive: the industry
  answer to "my compositor needs X apps" is *reuse a server*, not write one.

## The honest verdict on this path

The bridge is a **compatibility seam**, and it is at the "a client connects, is
answered, and can draw pixels into a window we own" stage. Running an ordinary X
application needs tiers 1–5 above; hosting a toolkit needs five extensions on top;
hosting Wine needs all nine of its probes. That is not a week of work, and it is not
what makes WuBuOS usable.

So the ranking stands, but with a correction:

1. **Guest framebuffer presentation** — the DOS box already presents `uint8_t *fb`
   into a `DosGuiWindow`. The next guest that draws (Wine in virtual-desktop mode, or
   a DOS/WEMU scanout) should use that same route. An X server is needed only for
   guests that *demand* X, and then the smallest honest target is tiers 1–4 with no
   extensions, accepting that toolkits will not run.
2. **Events before drawing** — always. A drawing request nobody receives looks like a
   broken server.
3. Do not add opcodes without a client that fails on their absence. Every one of the
   four bugs found in the request loop (no-reply abort, `ListExtensions` offset,
   `GetProperty` number, missing-property reply) was found by a real client, and none
   by the 43 self-written checks that preceded them.

## Method notes, because the mistakes repeat

- **Hand-computed request lengths were wrong three times** (7 words for a 32-byte
  `CreateWindow`, 3 words for an 8-byte `MapWindow`, 6 words for a 20-byte
  `CreateGC`). Each desynced the stream by four bytes and produced the same symptom:
  the server answers the first request and then appears to hang. Compute the length
  from `len(payload)`; never count by hand.
- **A little-endian CARD32 `0x00RRGGBB` arrives as `BB GG RR 00`.** Writing the
  bytes in value order sends blue.
- **The dump must be chosen by painted pixels, not by window area.** The largest
  window is the shell, and it is black — which looks exactly like "drawing is broken".

## GUI defect map (2026-11-25, from pixel probes, not impressions)

1. **"Random black bar" at the bottom** = the 9-row strip below the floating modern
   taskbar. The bar is drawn floating `MODERN_GAP_V` above the bottom
   (`bar_y = taskbar_top() - MODERN_GAP_V`, `bar_h = th`), and nothing paints under
   it: the pixels there are `desktop_bg` 0x17 (23,23,23) while the wallpaper above is
   0x62-ish. It is not a black bar, it is the *desktop colour* showing through a gap
   where the wallpaper should continue. Fix belongs in the wallpaper/desktop-bg pass
   (paint full height; the bar floats over it), not in the taskbar.
   NOTE: `taskbar_render()` has TWO paths — THEME_DARK returns early into
   `render_modern_taskbar()`, so any fix applied to the classic path is dead code.

2. **"Boxy/blocky everywhere"** = the primitives are 1-bit. `vbe_fill_rect` and
   `vbe_fill_rect_rounded` do not antialias; only the corner mask and the orb do.
   The systemic fix is antialiased primitives in the `vbe`/`gui_shape` owner so every
   widget inherits it. Per-widget patching cannot fix this class.

3. **"PNG buddy back"** = `wm_render` still called `wubu_bonzi_draw()`; the metaball
   orb was only ever called by its own test. Now `wubu_bonzi_draw_orb()` is preferred
   and `wubu_buddy_orb.o` is in GUI_OBJS. Lesson: a module that passes its gate and is
   in no render path is not shipped.

4. **Dock and taskbar styling** = hand-drawn C that predates the theme system, so it
   ignores the palette (rainbow icons, truncated labels, 24px icons in a 48px bar).
   Per the standard these are `.WET`/WUA surfaces; porting them is the fix.
