# Control Panel: two engines, one of them unreachable

Written because a session added four applets to a panel that no shipped code opens, and the gap matrix
temporarily claimed those features as owned. The correction is in `tools/gap_matrix.py`; this file exists
so the next person does not repeat it.

## What is true

- `src/apps/control/control.c` → `control_launch()` is the **shipped** Settings app. The desktop's
  Settings icon reaches it through `launch_settings()` in `src/apps/apps.c`. It is a category grid with
  action rows, 422 lines, and it does not know about the applet engine.
- `src/gui/control_panel.c` → `controlpanel_*` is an **applet engine**: a registry plus ten pages
  (display, network, sound, theme, hardware, firmware, mirror, account, limits, signin). Its renderer
  paints a 24-px colour-block sidebar with no labels and hands each applet the full width as its page.
  It is a sketch of a panel, not a panel.
- **Nothing in product code calls `controlpanel_init()`, `controlpanel_render()`, `controlpanel_show()`
  or `controlpanel_toggle()`.** Verified by grep over `src/` excluding tests: the only hits are inside
  `control_panel.c` itself. The engine is reachable from tests only.
- Therefore every page added to it is **invisible in a real session**, and its tests pass because they
  call the applets directly.

## Consequences already corrected

- `AccountsControl` and `BioEnrollment` went back to **gap** in `tools/gap_matrix.py`: the readers and the
  probes ship (`wubu_userinfo`, `wubu_biometric`) but the pages that would show them do not.
- `ParentalControls` stays owned, on the strength of `wubu_parental` + `wubu_shell/parental_guard` — the
  session really does lock itself outside the allowed hours — with the note saying the editor page is in
  the unwired engine.

## Consequences fixed here

- The Display and Network pages painted `0xFFF7F7F7` — a near-white card in a dark-only OS — plus
  `0xFF1060B0`/`0xFF202020` text. They now take colours from `wubu_theme_colors()`.
- `cp_display_set_snapshot()` / `cp_network_set_snapshot()` existed and were **never called by product
  code**: every real install showed "no display snapshot" while the tests injected a fake and passed.
  `controlpanel_init()` now installs `wubu_world_snapshot` when it is linked.

## What was done about it

`src/gui/control_panel.c` grew real chrome and a launcher: a 152-px sidebar with a NAME and a colour mark
per page, a page area beside it, pointer routing (a sidebar click selects; a press in the page is
translated into page-relative coordinates) and Up/Down page cycling. `control_panel_launch()` creates the
WM window and wires it to that engine, and `launch_settings()` in `src/apps/apps.c` now opens it, with the
old panel kept only as a fallback for links without a WM.

`test_cp_applets` grew the gate that would have caught the original defect: every sidebar row must hit
itself, the rows must tile without gaps, no row may be wider than the sidebar, a click on row 2 must
select page 2, Down/Up must move the selection, and a press in the page area must NOT change the page.
Mutation-proven: shifting the hit-test 40 px down fails four of those checks.

## The work that remains, and how it should be done

1. Retire `src/apps/control/control.c`. It is now only a fallback, and the tree's rule is no second copy
   of a component the OS already ships: either delete it (and its tests) or make it a thin adapter that
   calls the engine.
2. Whatever wins, the rule is the tree's own: **no second copy of a component the OS already ships.**
3. The gate that would have caught this: assert that the function the desktop's Settings icon calls is
   the one that renders the applet registry. Not "the applets render" — that was already true and meant
   nothing.

## Follow-up measurement: the panel DOES draw

Read back inside `cp_on_draw`, immediately after the render call, from the same framebuffer:

    CPRB: sidebar_px=001f1f1f page_px=00242424 page_ink=384 count=11

`001f1f1f` is exactly the sidebar colour, and 384 page pixels are neither the face colour nor untouched
(i.e. the page's text is there). So `controlpanel_render_at()` paints the sidebar, the rows, the border
and the page **into the surface it is given**, at the content rect the chrome hands it.

What is still true: the host window and the dumped frame show that area as plain face colour. Therefore
the defect is in PRESENTATION for this window -- the buffer the panel draws into is not the one the user
sees -- and not in the panel's drawing. That is the next thread to pull, and it is a compositor question:
where does `wm_render`'s `fb` go between the draw and the present, and does the window's own surface copy
happen before `on_draw` rather than after it?

Do not "fix" the panel again on the strength of an empty-looking window until that is answered; the
drawing side is proven.

## Three theories, all falsified (measured, 2026-11-26)

Each of these was tested against the live build with `WUBU_OPEN_SETTINGS=1`; none is the answer, and
recording them stops the next session from re-running the same experiments.

1. **"on_draw gets a NULL framebuffer."** It does on the first frames of a boot, and the panel now falls
   back to `vbe_state()->fb` -- but on the frames that matter the callback receives a real surface at a
   real content rect (81,93 858x537) and the read-back shows the panel painted in it.
2. **"The panel draws into the back buffer, which is never presented."** Painting the content into
   `vbe_state()->fb` (the surface the `vbe_*` calls and the chrome use) instead of the `fb` parameter
   changed nothing: the presented frame still showed face colour only.
3. **"It is a tear: content drawn, then the next frame's chrome covers it before capture."** Five
   consecutive host captures 300 ms apart, with the content area filled with pure magenta: zero magenta
   pixels in any of them. It is not a race.

What IS established:
- the presented window is the object whose callback runs (`win->title` set inside `on_draw` appeared in
  the presented title bar as "MAGENTA 0");
- the chrome reaches the presented frame every time;
- the content, after being written to the surface handed to `on_draw`, does not -- and neither does a
  full-rect magenta fill.

So the next reader should start at the hosted composition path: who owns the surface passed to
`on_draw`, and where does that surface go between the call and the blit to the screen.
`src/gui/compositor/gui_dbuf.c`, `src/gui/gui_blit.c` and `src/hosted/hosted_x11_present.c` are the files
the evidence points at.

## RESOLVED: the panel was drawing into the wrong buffer

The cause was one line of vbe.c. `vbe_row()` -- the backing for every `vbe_fill_rect` / `vbe_draw_text` --
is `(uint8_t *)g_vbe.back + y * g_vbe.pitch`. `vbe_state()->fb` is a DIFFERENT buffer, and the `fb`
parameter handed to `on_draw` is NULL in the hosted frame loop (`desktop_render(NULL, w, h)`). Chrome drew
through `vbe_*` and therefore appeared; the panel indexed a pointer directly and landed in memory nothing
presents. A full-rect magenta fill through that pointer never reached a frame, which is what ruled out
tearing and the back-buffer theory.

The fix is an accessor rather than a convention: `vbe_draw_surface(&stride_px, &height)` in vbe.c returns
the surface vbe_* writes, WITH ITS STRIDE (pitch/4, which is not always the width). The panel uses it and
draws into the presented surface.

Measured after the fix, in the presented frame: 20,133 sidebar pixels of `#1F1F1F`, 965 accent-blue pixels,
477 white text pixels. The Control Panel now shows its labelled sidebar (Settings, Display, Network, Sound,
Theme, Hardware, Firmware, Software sources, Your account, Family limits, Sign-in options, Revocation Sync)
and the selected page.

The gate for the class is `test_vbe_surface`: draw a pixel with the API, read it through the accessor, and
require the same memory (plus a far-away pixel, so a stride mix-up cannot pass). Mutation-proven by
returning `g_vbe.fb` instead of `g_vbe.back` -> three failures.

Rule for any future renderer that indexes pixels instead of calling `vbe_*`: take the surface from
`vbe_draw_surface()` and use the stride it reports.
