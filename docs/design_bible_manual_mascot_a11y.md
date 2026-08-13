# WuBuOS GUI Design Bible — Manual Mascot + Accessibility Cluster + Shell Direction

**Date:** 2026-08-13  **Status:** DESIGN BIBLE (user-authored reference, captured)
**Author:** WaefreBeorn (user) — "I made design bible graphic art to help."

This file captures the USER'S OWN design references so future waves build to
them. It is NOT an implementation plan; it is the visual contract.

---

## 1. TWO mascots — they are NOT the same thing (user correction, 2026-08-13)

> "you are not redesigning the buddy — this mascot is for the manual.
>  the buddy is designed using buddy psychology"

### 1a. The BUDDY (existing — DO NOT REDESIGN)
- **Purple** desktop companion, sits on the desktop, drawn by the WM render
  loop above windows. Round purple body, white eyes that blink (4-stage:
  Anticipation→Close→Open→Relax), smile, blush cheeks, arms.
- Built on **buddy psychology**: friendly, non-threatening, gamified gateway
  to the AGI (clicking opens the HolyC/AGI terminal).
- Animation = Disney 12 principles (squash/stretch, anticipation, ease,
  follow-through). File: `src/gui/wubu_bonzi.c`.
- **Stays purple.** Do not recolor it. It is NOT the manual mascot.

### 1b. The MANUAL MASCOT (the new white WuBu rabbit — SEPARATE)
- The future **"operating system manual"** mascot — the encyclopedia that used
  to ship on the disk with your computer. We are building one eventually.
- **A white rabbit** with:
  - a **light green aviator cap** with a **"W"** on the front (the W is
    magenta/pink `#FF6B9D` in some references, red in others — pick the
    magenta/pink per the first design-bible sheet),
  - a **red / magenta neckerchief** tied around the neck,
  - long upright ears with pink inner ear,
  - thick clean black outlines, flat vector style (Xiaomi "Mitu"-like).
- Rendered in a huge **pose sheet** (user's AI-generated design bible):
  surfing, sleeping, racing, camping, playing guitar, reading, coding,
  jumping, cheering, magnifying-glass-inspecting, ice-cream, walking a dog,
  crying, gaming, traveling, question-mark, running, cooking (flipping
  pancakes), superhero, cold-in-snow, eating pizza, treasure chest, karate,
  puddle-jumping, beach, firefighter, technician, panicked-runner, hugging a
  duck, sleeping, holding a sign, pointing, banner, skeleton key, star,
  waving, writing at a desk, "bug/error" terrified at monitor, magic-carpet
  success, break/sleep-on-spinner, troubleshooting, victory-with-checkmark.
- **This is the WuBu Manual.** It is the face of the future documentation —
  a companion that explains the OS the way the printed encyclopedia did.

> **Rule:** Manual mascot = white WuBu rabbit (green W-aviator cap + red
> neckerchief). Buddy = purple. Never conflate them.

---

## 2. The Accessibility Cluster — the "buttons I gave you" (user reference)

> "as it is now it needs ground up redesigned"
> "i want windows xp with classic availability"
> "the buttons i gave you are meant to help touch design and old people"
> "the bottom corners are ghost purple beans like the top left yellow bean
>  they are corners"
> "the vibe is gamecube"

### The four controls (GameCube face-button psychology, reversed for desktop)
| Control | Where | Color | Action | Notes |
|---|---|---|---|---|
| **Y (bean)** | top-LEFT | yellow | click=minimize, drag=rotate | a crescent/"bean", NOT a dot |
| **A (orb)** | left of B | GREEN, biggest | grab+drag=move, click=maximize | largest = most-used, green = "go" |
| **B (orb)** | right of A | red, with X | click=close, drag=purge | smallest |
| **Purple bean** | BOTH BOTTOM CORNERS | purple | resize | ghost/edge-revealed, "they are corners" |

### Key user directives (from the working code comment)
- **NO panel background.** The buttons float DIRECTLY on the window. A
  lavender panel box + translucent shadow reads as a "bad JPEG transparency"
  cloud on the window face. The buttons are the whole UI — no card.
- **Touch + old people first.** Big, high-saturation, high-contrast buttons.
  Color/size/shape psychology: green=go, yellow=transform, red=stop, biggest
  = most important.
- **Vibe = GameCube.** Nostalgic, playful, friendly, not corporate.
- **Rounded window corner** is itself an accessibility affordance the cluster
  anchors onto.

### Current implementation status (VERIFIED 2026-08-13)
- `src/gui/wubu_a11y.c` — the cluster. **WAS BROKEN**: `wubu_a11y_draw()`
  called `panel_rect()` which is **never defined**, so the WM + all capture
  harnesses failed to link (`undefined reference to panel_rect`). That is the
  "never been made right": the design existed, was fully wired into
  `dosgui_wm_render.c` + `dosgui_wm_input.c`, but was dead code.
- **FIXED** (`6ac6082`): removed the dead call + unused `px/py`. Now links.
- **VERIFIED RENDERING** (vision-model + crop of `desktop_shot` frames):
  all four controls render — yellow bean TL, green A, red B-X, ghost purple
  beans at BOTH bottom corners. GameCube vibe intact.

---

## 3. Shell direction — "windows xp with classic availability"

- Base look = **Windows XP** (Luna blue: gradient titles, rounded buttons,
  green Start orb, blue taskbar) — `THEME_XP_LUNA_BLUE`.
- But a **classic availability** mode must always be reachable
  (`THEME_WIN98_CLASSIC`: square 3D buttons, teal desktop, navy titles) for
  power users / the visually-unfamiliar.
- Runtime-switchable (the theme engine already does this). The a11y cluster
  sits ON TOP of either shell — it is not a theme, it's an overlay tier for
  the "newcomer/politician/child" tier of users.

---

## 4. The disorganization (honest — what "ground-up redesign" must fix)

Verified by surveying `src/gui/` (150 files) + `src/kernel/`:

1. **TWO window managers.** `dosgui_wm.*` (Win98 shell — THE REAL ONE, used
   by `hosted.c`) AND `wubu_wm.*`/`wubu_compositor.*` (a parallel abstraction
   also in the object list). Both implement drag/resize/desktops. The
   hosted boot path only calls `dosgui_wm_init`; `wubu_wm` is effectively a
   second, competing implementation.
   **RESOLUTION:** these are NOT two WMs fighting — they are two
   ARCHITECTURAL TRACKS. `dosgui_wm` is the canonical Win98-shell for the
   hosted/Wayland-surface boot (hosted.c + hosted_wayland_surface.c both call
   `dosgui_wm_init`). `wubu_wm`/`wubu_compositor_standalone` is a separate,
   standalone Wayland-compositor implementation with its OWN run loop —
   the native/Wayland-migration future path (a real WuBuOS direction, not
   dead code). They never init together; `dosgui_wm` is the canonical hosted
   shell and stays that way.
2. **TWO theme engines.** `src/gui/wubu_theme.*` (THEME_COUNT=5, the applied
   colors used by `tc()`) AND `src/kernel/wubu_theme.*` (KTHEME, a writable
   `/theme` node tree). They don't call each other; the GUI colors struct and
   the kernel node tree are two designs for the same thing.
   **RESOLVED (bridge, `7699aeb`):** the kernel `/theme` namespace is now the
   AGI write surface and the GUI reads it. `wubu_theme_sync_from_kernel()`
   overlays every kernel node onto a mutable live-colors struct that
   `wubu_theme_colors()`/`tc()` returns, so "the AGI writes a node and the
   next frame re-renders" actually works (verified by `test_theme_bridge`).
   The two still define the same function names (`wubu_theme_get` etc.) so
   they can't be in ONE binary, but they're also never both linked: kernel
   builds use the kernel engine, hosted/GUI builds use the GUI engine. The
   write-surface is bridged via a weak reference (no-ops when kernel absent).
3. **The a11y cluster couldn't link** (fixed above) — a symptom of the
   disorganization: a fully-designed feature shipped as dead code.
4. **Manual mascot doesn't exist yet.** The white WuBu rabbit is only design
   bible art; no sprite/pose renders it.

**Ground-up redesign direction:** pick ONE WM (dosgui_wm), pick ONE theme
source of truth (the kernel `/theme` node tree writing INTO the GUI colors
struct, or vice-versa), keep the a11y overlay tier, and add the manual-mascot
sprite set as a separate documentation companion.

---

## 5. Design bible assets (user-provided)

- Neumorphic accessible-buttons reference (lavender panel, orange drag
  handle, green circle, red X circle) — the *feel* of the buttons.
- White WuBu rabbit pose sheets (6, 9, 6, 6, 6, 9, 6, 6, 5 panels) — the
  manual-mascot pose set. Green W-aviator cap + red/magenta neckerchief.
