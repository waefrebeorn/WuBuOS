# 075 — UX supplementation without theatre

The ask: another deep regimen on user-experience supplementation for the OS, and
gap lists that **maintain** a UX instead of tricking people into believing one
exists.

The distinction matters because a window that renders is not a feature. A menu
that opens is not a menu that works. A button that highlights is not an action.
This note defines the failure classes, grounds them in platform guidance, and
turns them into a list a test can hold us to.

## Hop 1 — platform baselines are concrete numbers

Microsoft's desktop UX checklist is still the best enumeration of the boring
things an OS must get right:

- support **800×600** effective resolution; **640×480** for safe-mode UIs;
  reserve **48 relative pixels** for the taskbar
- optimize resizable layouts for **1024×768**
- test at **96 dpi (800×600), 120 dpi (1024×768), 144 dpi (1200×900)**; look for
  clipped controls, stretched icons, truncated text
- owned windows open **centered on the owner**, never underneath; later opens
  may restore the last position relative to the owner
- touch/mobile scenarios optimize for **120 dpi**

These are checkable. They are also the first things a screenshot-driven UI
skips: it looks right at one size on one monitor.

## Hop 2 — accessibility is a specification, not a mood

WCAG 2.2 is organized as POUR (Perceivable, Operable, Understandable, Robust)
and, critically for a desktop OS, contains testable criteria rather than
aspirations:

- **2.1.1 Keyboard** — every function available from a keyboard
- **2.4.3 / 2.4.7 Focus order and visible focus** — the focus ring must be
  visible and the order must follow meaning
- **2.5.8 Target size** — 24×24 CSS px minimum (enhanced 44×44)
- **1.4.3 Contrast** — 4.5:1 body text, 3:1 large text
- **1.4.11 Non-text contrast** — 3:1 for controls and focus indicators
- **4.1.2 Name, role, value** — a control must expose what it is and its state

An OS that ships a mouse-only Start menu fails 2.1.1 regardless of how good the
animation is.

## Hop 3 — modern HIGs describe composition, not decoration

GNOME's HIG splits into containers, navigation, feedback, controls — a useful
taxonomy because it forces each element to declare its job. A "feedback"
pattern with no feedback is a named gap, not an aesthetic choice.

The same applies to Microsoft's guidance: the checklist is about behaviour
(centering, resizing, focus, defaults), not about how many gradients a title bar
has.

## Hop 4 — implementation patterns that make UX maintainable

From open-source desktop practice (GTK/Qt/Win32), four patterns separate a real
surface from a painted one:

1. **Command objects** for every mutating action — undo/redo becomes possible
   by construction, not by afterthought.
2. **Model–view separation** for lists (explorer, start menu, task manager) —
   the view can be re-rendered without owning state, and the model can be tested
   headlessly.
3. **One input path** — keyboard, mouse and (later) touch produce the same
   semantic event; otherwise keyboard support rots silently.
4. **Persistence at the edge** — state is written where it changes, not on exit;
   a crash must not lose the user's work.

## Hop 5 — latency is part of the UX

The classical thresholds still hold: **0.1 s** feels instantaneous, **1 s** keeps
flow, **10 s** is the limit before attention wanders. An OS that blocks the
compositor on a file read violates all three at once. Any UX claim should carry
a measured number, not an adjective.

## Hop 6 — the failure modes that produce fake UX

- **Render-only surface**: the window draws, the controls are decorative.
- **Dead control**: a button with an empty or `(void)` handler.
- **Unpersisted state**: settings that reset on reboot.
- **No feedback**: an action that takes seconds with no busy indicator.
- **No error path**: failure is silent, or a modal that says "Error".
- **No keyboard path**: mouse-only.
- **No undo**: destructive actions are irreversible.
- **Untested effect**: a test asserts the render, not the result.

Each of these is detectable. None requires taste to find.

## Hop 7 — convergence: one rule per control

> Every visible control must have (a) an effect, (b) an undo or a confirmation
> for destructive effects, (c) a keyboard path, and (d) a test that asserts the
> effect rather than the pixels.

That rule is the whole supplementation strategy. A surface that cannot meet it
is not "coming later" — it is a gap with a name.

## What we measured in WuBuOS (2026-10-18)

`tools/ux_gap_audit.py` scans `src/gui` and `src/apps`:

| Metric | Value |
|---|---:|
| shipped UX files | 155 |
| lines in shipped UX code | 32,946 |
| test-scaffolding files (excluded) | 46 |
| stub markers in shipped code | 43 |
| empty function bodies in shipped code | 2 |
| shipped files with no `mk/tests.mk` reference | 15 |
| shipped files flagged by either heuristic | 22 |

Representative findings, each with a file and line:

- `src/gui/wubu_compositor.c:207` — "Focus handling - stub". Focus is the one
  thing a compositor must not stub: without it, keyboard navigation and
  accessibility are fiction.
- `src/apps/dosgui_apps.c:65` — "Generic placeholder draw for apps without
  dedicated UI yet". The window exists; the app does not.
- `src/gui/dosgui_term_ansi.c:268` — "No-op for now (cursor visibility, etc.)".
  A terminal that cannot show the cursor is not a terminal.
- `src/gui/dosgui_wm.c:142` — weak no-op defaults so binaries link. Legitimate
  as a link seam, but it means "the symbol exists" proves nothing.

The full machine-generated list is `docs/UX-GAP-REGISTRY.md`; the hand-audited
capability view is `docs/UX-CAPABILITY-MATRIX.md`.

## The gap-list format that maintains UX

Each row needs four columns, and a row is only "done" when all four are true:

| Column | Question | Evidence |
|---|---|---|
| Entry point | Can a user reach it without a test harness? | shell/menu path |
| Effect | Does the action change state? | test asserting state |
| Failure | What does the user see when it fails? | error path, not silence |
| Maintenance | Which test fails if this rots? | named target |

A list without the fourth column becomes archaeology within a month.

## Honest limits

- The audit's heuristics catch *declared* stubs and empty bodies. A handler that
  does something useless passes. "Unflagged" means "not caught", not "verified".
- The capability matrix is a hand audit; it will drift unless regenerated.
- We have not yet run a keyboard-only traversal or a screen-reader pass on the
  live image. Those are the next two measurements, and until they exist, any
  accessibility claim is unproven.

## Sources

- Microsoft, *UX checklist for desktop applications* (Win32 uxguide)
- W3C, *WCAG 2.2 Quick Reference*
- GNOME, *Human Interface Guidelines*
- Nielsen Norman Group, response-time limits (0.1/1/10 s)
- Local: `tools/ux_gap_audit.py`, `docs/UX-GAP-REGISTRY.md`,
  `docs/UX-CAPABILITY-MATRIX.md`
