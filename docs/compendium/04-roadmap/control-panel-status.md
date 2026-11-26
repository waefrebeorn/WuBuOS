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

## The work that remains, and how it should be done

1. Make ONE panel. Either the applet engine grows a real chrome (labelled sidebar with a width of its
   own, page area beside it, keyboard and pointer routing) and `launch_settings()` points at it, or
   `control.c` learns to host applet pages and the engine is deleted.
2. Whatever wins, the rule is the tree's own: **no second copy of a component the OS already ships.**
3. The gate that would have caught this: assert that the function the desktop's Settings icon calls is
   the one that renders the applet registry. Not "the applets render" — that was already true and meant
   nothing.
