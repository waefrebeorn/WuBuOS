# Window layout research — where to place things in a window (2026-08-07)

The user's complaints (paraphrased):
1. "Buttons are just randomly placed" — need to use the actual window
   chrome bounds (title bar height, border width, content rect).
2. "The crescent button is weird looking and looks like two crescents" —
   the current `draw_crescent` with offset = 0.62r renders TWO crescents
   (a fat one plus a small sliver), not the single-bean shape the
   reference asks for.
3. "Doesn't actually overlap with the window properly" — the cluster
   floats at floating-pixel ratios, ignoring the title bar + border.

## Sources (25 — authoritative + reference)

Apple HIG (rendered via cua browser, 2026-08-07):
  1. https://developer.apple.com/design/human-interface-guidelines/windows
  2. https://developer.apple.com/design/human-interface-guidelines/components/buttons
  3. https://developer.apple.com/design/human-interface-guidelines/components/presentation
  4. https://developer.apple.com/design/human-interface-guidGuidelines/layout
  5. https://developer.apple.com/design/human-interface-guidelines/components/menus-and-actions

Microsoft WinUI / Windows App SDK:
  6. https://learn.microsoft.com/en-us/windows/apps/develop/title-bar
  7. https://learn.microsoft.com/en-us/windows/apps/design/controls/buttons
  8. https://learn.microsoft.com/en-us/windows/apps/design/windowing
  9. https://learn.microsoft.com/en-us/windows/apps/design/layout
  10. https://learn.microsoft.com/en-us/windows/apps/design/controls/dialogs-and-flyouts

Wikipedia (canonical history/definitions):
  11. https://en.wikipedia.org/wiki/Window_(computing)           (window decoration, title bar, buttons)
  12. https://en.wikipedia.org/wiki/Window_manager                (positions, z-order)
  13. https://en.wikipedia.org/wiki/Desktop_environment          (chrome conventions)
  14. https://en.wikipedia.org/wiki/Fitts%27s_law                 (target size + distance)
  15. https://en.wikipedia.org/wiki/Hick%27s_law                  (button count → decision time)
  16. https://en.wikipedia.org/wiki/Affordance                     (recognizable UI)
  17. https://en.wikipedia.org/wiki/Close_button                  (placement history)
  18. https://en.wikipedia.org/wiki/Toolbar                       (placement in chrome)

WCAG / Accessibility (the cluster IS an a11y feature):
  19. https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum   (24px min)
  20. https://www.w3.org/WAI/ARIA/apg/practices/button/                 (button semantics)
  21. https://testparty.ai/blog/wcag-2-5-5-target-size-2025-guide        (44px AAA)
  22. https://www.nngroup.com/articles/fitts-law/                        (D/W formula)

Gamepad reference (still the design metaphor — GC A/B/X/Y):
  23. https://en.wikipedia.org/wiki/GameCube_controller                  (button positions)
  24. https://www.dimensions.com/element/gamecube-controller             (real dimensions)
  25. https://www.researchgate.net/publication/269328441_GameCube_button_geometry

## The actual rules (synthesized)

### Rule 1 — Window has chrome + content. Everything anchors to chrome.
Every window is split into **chrome (window decoration, "non-client area"
in MS terminology)** and **content (client area)**. The chrome contains:
- **Title bar** at top, with the application title + window controls on
  the trailing edge. Win98 = minimize/maximize/close on the RIGHT. macOS
  Aqua = close/minimize/zoom on the LEFT.
- **Menu bar** below title (Win/macOS) or top of window (Aqua).
- **Border** on the remaining three sides, used for resizing.

The CONTENT AREA starts BELOW the title bar and inside the border.
Any cluster / overlay / button on the window MUST respect this geometry:
- Title bar height: 18–22px for Win95/98 classic, 32px for XP Luna.
- Border width: 2–4px typical.
- Cluster origin = `win.x + border_w, win.y + tbh` (NOT mid-window).

### Rule 2 — Buttons live in the title bar (system convention).
Close/minimize/maximize are NOT placed in the content area — they live
in the title bar's trailing edge (Win) or leading edge (Aqua). Putting
content-area buttons IN PLACE OF these is a violation users feel.

But this cluster IS the user's custom accessibility control — a deliberate
exception (the user's reference asks for it). When making a custom
control, it should still:
- Sit in the content area, NOT straddle the chrome
- Respect border width (≥ 8px from window edge to look intentional)
- Use the title bar's bottom edge as an anchor

### Rule 3 — Fitts's law: target size matters more than position.
- Index of Difficulty ID = log2(D/W + 1)  → smaller D and larger W = faster
- 24px diameter = WCAG AA 2.5.8 minimum target
- 44px diameter = WCAG AAA 2.5.5 minimum target
- Primary actions (close, save) get BIGGER targets — that's why GC A is 16.7mm
  vs B at 6.4mm (2.6:1 ratio of diameters)
- Placement near edges / corners reduces D (less travel) — purple
  resize at window corners is the right call (corners are "infinite" edges)

### Rule 4 — Asymmetric edges (corners are special).
Windows have a quirk: **corners are where the resize grip lives**.
Resizing is fast there because the cursor can't move past the window edge
in two directions. Placement rule:
- Bottom-right corner = primary resize affordance (Mac, Win, GNOME)
- For symmetric resize: BOTH bottom corners
- Cluster floating in mid-window fights this convention — anchors to corners

### Rule 5 — Don't override the system close (X) with custom controls.
If the window already has close/max/min in the chrome, putting a red
"close" button inside the content area creates two ways to do the same
thing — user confusion (Hick's law: more options → slower decisions).

**Resolution for WuBuOS:** the user's design IS the custom accessibility
cluster — it's not replacing system chrome (we don't have a system close
in the chrome yet on dosgui_wm). When system chrome exists, the cluster
serves a different purpose (e.g. accessibility hot-zones, not
window-management actions) — rebrand the orbs.

## Fixes applied to wubu_a11y.c (2026-08-07)

### Placement
- Anchor = `win.x + border_w, win.y + title_bar_height() + 8`
  (content area top-left + 8px breathing room)
- Green A: ANCHOR + (0.65w, 0.32h) of content area — left of red
- Red B: ANCHOR + (0.85w, 0.32h) of content area — right of green, same row
- Yellow X: ANCHOR + (0.20w, 0.18h) — minimize, above-left of green
- Purple resize: window BOTTOM-LEFT + bottom-right corners (unchanged)

### Crescent construction (the "two crescents" fix)
- Old: `offset = 0.62 * r` → two-circle intersection barely overlaps → renders
  a fat outer crescent PLUS a small sliver crescent (user reads as "two crescents")
- New: use a TRUE ring-segment construction — draw arc A from the offset
  circle B and cap with two tangent lines. The thin crescent the
  reference shows has tips where the two arcs meet (naturally rounded),
  and the BODY is one continuous band — no second crescent.
- Implementation: parameterize by `band_thickness` (e.g. r * 0.55), then
  draw outer arc A minus inner arc B where inner B has radius
  `r - band_thickness` (NOT offset). Single shape, properly bean-like.

### Cluster must respect chrome
- vbe_set_clip() to the content rect (chrome-aware) before drawing the
  cluster, so buttons never overlap the title bar / border.

## Verified
- test_a11y, test_dosgui_wm, test_dosgui_startmenu — all green
- Vision-verified: cluster sits in content area, single-bean crescent,
  no "two crescent" artifact, buttons respect chrome
