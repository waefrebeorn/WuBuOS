# The GUI standard — surfaces, tokens, and the one way to make a program

**Status:** normative. The measured baseline lives in `tools/hardcode_baseline.json`
and is enforced by `tools/hardcode_ratchet.py` (`make test_hardcode_ratchet`).

## 1. Why this is a standard and not taste

"Jank" is not an opinion. It is the sum of small inconsistencies that a user
cannot name but always feels: a control inset by 3 px here and 4 px there, a
colour that does not follow the theme, a button with no pressed state, a dialog
that does not take focus, a label that clips instead of ellipsising. Every one of
those is measurable, and each one has a rule below.

We are judged against Windows 11 and CachyOS/KDE, whose rules are published:

- Microsoft, [Design guidelines for Windows apps](https://learn.microsoft.com/en-us/windows/apps/design/guidelines-overview)
  and [Typography in Windows](https://learn.microsoft.com/en-us/windows/apps/design/style/typography)
- [GNOME Human Interface Guidelines — Windows](https://developer.gnome.org/hig/patterns/containers/windows.html)
- [KDE Human Interface Guidelines](https://develop.kde.org/hig/)
- Nielsen, [Ten Usability Heuristics](https://www.nngroup.com/articles/ten-usability-heuristics/)

The three HIGs disagree about style and agree about structure: a fixed spacing
scale, a small type ramp, a complete set of interaction states, one focus model,
and a single place where an app declares itself. WuBuOS takes the structure.

## 2. Surface taxonomy (what a "GUI gap" can even be)

| Layer | Owner | Files | What a gap here looks like |
|---|---|---|---|
| Shell | `src/gui/shell/` | taskbar, tray, start menu, clock, notifications, greeter | the bar lies about state; a menu that cannot be keyboard-driven |
| Chrome | `src/gui/shell/window_chrome.c`, `wm_render.c` | title bar, buttons, borders, shadow | title clips, icon wrong, buttons too small to hit |
| Controls | `src/gui/wubu_gui_metrics.c`, dialogs | buttons, fields, lists, scrollbars, menus | no hover/press/focus/disabled state; inconsistent insets |
| Overlays | `open_with_dialog`, `wubu_notify`, `crash_notice` | modal and transient surfaces | modal that does not trap focus; toast with no action |
| Applets | `.wet` files, runtime in `src/gui/wet` (submodule) | scripted programs | no reload, no rollback, no state to keep |
| Apps | `g_app_defs` in `src/apps/apps.c` | built-in programs | registered in three places at once |
| Web | `src/web` (WuBuWeb) | pages | a page that renders as text but cannot be clicked |

A finding is only actionable when it names its layer. "The UI is bad" cannot be
assigned; "the shell's notification has no action affordance (overlay layer)"
can.

## 3. Tokens (the part that ends hardcoding)

The tree has **411 hardcoded values** — 65 files carrying icon-enum coupling, raw
`0xRRGGBB` colours outside the theme, and bare geometry integers in shell sizing.
That is the measured size of the jank. The ratchet freezes it; the tokens retire
it.

| Token family | Rule | Replaces |
|---|---|---|
| `WUBU_SPACE_*` | 4 / 8 / 12 / 16 / 24 / 32 only. No other inset exists. | every bare `4`, `6`, `8` in layout math |
| type ramp | 11 / 12 / 14 / 16 / 20 / 24 px, scaled by `font_scale` and `dpi` | per-file `font_size` literals |
| `WUBU_COLOR_*` | resolved from the active theme (26 palettes); a literal colour in a draw call is a bug | raw `0xRRGGBB` |
| states | every interactive control draws `normal / hover / pressed / focused / disabled` — five, no exceptions | controls with one appearance |
| focus | one focus ring, one Tab order per window, owned by the WM (not per app) | per-dialog ad-hoc focus |
| motion | 120 ms in, 160 ms out, and **none** when reduced motion is on | no animation system today |
| elevation | 2 shadow levels: window, overlay | ad-hoc shadow radii |

The rule that makes this enforceable: **a value that appears twice becomes a
token; a token that appears once is still allowed to be local.** We do not build a
design system to hold one number.

## 4. One way to make a program (WuBuIDE → .WET → WUA → rollback)

Today a program can be born in four ways that do not agree: a `g_app_defs` entry,
a `.desktop` file, a `.wet` script, or an applet in the VWS designer. That is why
adding an app touches an enum, a table and a switch.

The target pipeline has one definition and one artefact chain:

```
WuBuIDE (design + code)  ->  .wua project (manifest + .wet + assets)
                         ->  live compile in-OS
                         ->  run under watchdog
                         ->  rollback on failure (last-good kept)
                         ->  promote (install to /Apps, register, resolve icon)
```

1. **`.wua` is the project, not the program.** A directory: `manifest.wua`
   (id, title, icon, entry, capabilities, min API), sources, assets. The manifest
   is the ONLY place an app declares itself.
2. **The id is a string, never an enum.** `org.wubu.app.calculator`. The
   `DESK_ICON_*` enum stays a legacy layout hint and must not be extended; new
   apps never get an enum value. (Measured: the enum leaks into 7 files today;
   the ratchet stops it growing.)
3. **Compile in place, run under a watchdog.** A `.wet`/`.wua` that fails to
   compile, exceeds its time budget, or exits non-zero is rolled back to the
   recorded last-good artefact, and the failing source is kept beside it as
   `.bad` with the diagnostic — never deleted, so the user can read what broke.
4. **Rollback preserves state that is declarable.** Widgets may declare
   `keep` (value survives reload) or `reset`; the runtime restores `keep` fields
   on reload. This is the Dart hot-reload contract expressed as a manifest field
   instead of a language runtime.
5. **Promotion is a registration, not a copy.** `promote` writes the manifest
   into the app registry; the desktop, Start menu, taskbar and icon resolver all
   read that one registry and none of them keeps its own list.

WuBuIDE itself is an app on this pipeline: it edits a `.wua`, compiles it live,
and its own preview window is a rollback target.

## 5. Kernel loading, organised

The kernel side has the same disease in a different organ: 350 files whose load
order is implied by the Makefile and by `hosted.c` rather than declared.

| Stage | Responsibility | Rule |
|---|---|---|
| 0 boot | GOP/VBE, memory map, serial | must not depend on anything above it |
| 1 core | paging, GDT/IDT, APIC, timers | published as symbols, no init callbacks |
| 2 drivers | PCI, NVMe, input, NIC | register through one `wubu_driver_t` table |
| 3 namespace | Styx/9P `/n` | starts AFTER drivers, before any window |
| 4 services | settings, time, net, audio | declared in one service table with a start order |
| 5 shell | WM, taskbar, app launch | the FIRST consumer of `/n`, never a prober of it |

Two rules make it legible: a stage may only call downward, and every stage
publishes what it provides into one registry so a missing capability is a named
"absent", not a silent NULL.

The known bug this prevents is already recorded: boot windows opened before the
namespace existed, scanned an empty root, and the File Manager reported "no
files" while the files were there.

## 6. Order of work (each step measurable)

1. **Ratchet ON** (`done`): 411 values frozen, mutation-proven.
2. **Tokens for colour** — the theme is the only source of colour; retire 43 raw
   literals from draw paths.
3. **States** — five states for every control, starting with the dialog buttons
   (highest user contact).
4. **Focus** — one Tab order per window, owned by the WM, with a visible ring.
5. **App pipeline** — `.wua` manifest + promote + registry read by all four
   surfaces; retire `DESK_ICON_*` from new code.
6. **Rollback** — watchdog + last-good for `.wet`, then widget `keep` fields.
7. **Motion** — 2 curves, 2 durations, honour reduced motion.
