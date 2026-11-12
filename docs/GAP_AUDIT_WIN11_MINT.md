# WuBuOS shell-surface gap audit (Windows 11 + Linux Mint Cinnamon)

Reproducible inventory of what the WuBuOS desktop shell already has versus the
two reference surfaces it is slerming past. Run it:

```
python3 tools/gap_audit.py
```

It greps the tree for **implementing symbols/files** (`.c`/`.h`, excluding
`third_party` and submodules). A hit is evidence that the feature is being
worked on, not proof that it is finished — read the result as a prioritized
work list, not a scorecard.

Current result (2026-11-12):

| Surface | Present | Absent |
|---------|---------|--------|
| Windows 11 shell | 31 / 56 | 25 |
| Linux Mint Cinnamon | 25 / 35 | 10 |

## Windows 11 — absent

Taskbar search button · Hover flyout on maximize · Voice typing (Win+H) ·
Power-user menu (Win+X) · Run dialog (Win+R) · Game Bar (Win+G) · Taskbar
progress badges · Taskbar alignment option · Taskbar size option · Multiple
monitor / per-output taskbar · Night light · Focus assist · Storage sense ·
Startup apps UI · VPN UI · Remote desktop · Mail client · Calendar app ·
Photos app · PDF reader · Archive manager · Font viewer · Character map ·
Solitaire / Minesweeper · Copilot

## Linux Mint Cinnamon — absent

File manager extensions · X-Apps (xed / xviewer / xreader / xplayer) · Driver
manager · Hot corners · Presentation mode · Night light · Locale settings ·
Backup tool / Timeshift · Archive manager (file-roller) · Image viewer

## Suggested priority (highest user-visible value first)

1. **Run dialog (Win+R)** — a core shell affordance; a small overlay that
   launches an app by name reuses `launch_app()`.
2. **Taskbar search button** — opens the Start menu with the search box
   focused; the Start menu and its results list already work.
3. **Power-user menu (Win+X)** — a small anchored menu over the Start button;
   entries map onto existing launchers (Terminal, Settings, File Manager,
   shutdown).
4. **Hover flyout on the maximize button** — the snap-layout flyout already
   exists (`win11_snap`); it only needs a hover trigger on the button.
5. **Archive manager, image viewer, PDF reader** — Mint basics; WuBuOS has
   MIME association (`wubu_mime`) but no viewers.
6. **Night light, locale settings, startup apps UI, hot corners,
   presentation mode** — settings-surface gaps, all small control-panel pages.
7. **Calendar app, font viewer, character map** — small utilities.

## Notes

- `Taskbar alignment` and `taskbar size` are Win11 settings that affect the
  existing floating bar; the bar is centre-only and fixed-height today.
- `Multiple monitor` is partly a compositor matter: the compositor advertises
  one `wl_output` and pins the client fullscreen, so per-output taskbars need
  the compositor to expose more than one output.
- `Mica/acrylic` and `window animations` are the two biggest *visual* gaps;
  the compositor gained a GLX GPU present path (commit `ff03ab6`) which is the
  substrate both need.
