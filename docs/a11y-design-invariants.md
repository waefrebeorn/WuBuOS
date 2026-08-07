# A11y cluster design — INVARIANTS (2026-08-07)

> **This document supersedes the previous scattered design notes.** The
> `docs/a11y-window-placement-research.md` file lists sources; this file
> lists the *invariants* any acceptable a11y cluster must satisfy.
> Geometry (where pixels go) is derived FROM the invariants, not from
> any reference image, GC controller, or previous implementation.

## Why invariants, not coordinates

Every previous attempt at this cluster picked a layout (yellow at 0.20w,
green at 0.65w, etc.) and shipped it. The user has called each one
"looks like shit" because the layout was chosen before the *purpose*
was articulated. Picking numbers first makes every subsequent design
decision feel arbitrary; picking the *purpose* first makes any
acceptable geometry self-evident.

The five viewpoints below are the authoritative reference; the
invariants are the synthesis.

## Five viewpoints (authoritative sources)

### V1. Wikipedia — Nintendo GameCube controller (face-button geometry)
**Source:** https://en.wikipedia.org/wiki/GameCube_controller (verbatim, 2026-08-07)

> "The four face buttons are on the right side of the controller with
> a **large green A button in the center**, flanked by a **smaller red
> B button to its bottom left** and **two kidney-shaped buttons to its
> right (X) and top (Y)**."
>
> "The green A button was made the largest to give the holder the idea
> that it performs the primary function. That button would be
> surrounded by a smaller red B button to the left and two colorless
> **kidney-shaped** Y and X buttons to the top and right, respectively.
> **The B button was initially also kidney-shaped before it was made a
> circle.**"
>
> "The B button was initially also kidney-shaped before it was made a
> circle."

**Lessons:**
- **Real GC = 2 shapes**: kidney (X, Y) + circle (A, B). Not 4 colors of
  one shape. Mixed shape vocabulary IS the design language.
- **A is the biggest = primary function.** Size encodes importance.
- **B is bottom-LEFT**, not bottom-right. (Most prior a11y designs had
  red bottom-right.)
- **2.6:1 size ratio** (A:B by diameter) was a deliberate hierarchy choice.

### V2. WCAG 2.5.5 (AAA) — Target Size (Enhanced)
**Source:** https://www.w3.org/WAI/WCAG22/Understanding/target-size-enhanced
(verbatim, retrieved 2026-08-07)

> "The size of the target for pointer inputs is at least **44 by 44 CSS
> pixels** except when: … Equivalent … Inline … User Agent Control …
> Essential"

> "While this criterion defines a minimum target size, it is recommended
> that **larger sizes are used** to reduce the possibility of
> unintentional actions. This is particularly relevant if any of the
> following are true:
> - **the control is used frequently**;
> - **the result of the interaction cannot be easily undone**;
> - **the control is positioned where it will be difficult to reach, or
>   is near the edge of the screen**;
> - **the control is part of a sequential task**."

**Lessons:**
- **44×44 is the floor, not the target.** All 4 of our controls are
  used frequently; close/purge is irreversible; corner resize IS
  near the edge. So our minimums should exceed 44.
- **Implicit hierarchy**: the close/purge button (irreversible, edge)
  should be the LARGEST, not the smallest. **Inverting GC's
  A=largest convention.**

### V3. WCAG 2.5.8 (AA) — Target Size (Minimum)
**Source:** https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum (verbatim)

> "The size of the target for pointer inputs is at least **24 by 24 CSS
> pixels**, except when: **Spacing** — Undersized targets (those less
> than 24 by 24 CSS pixels) are positioned so that if a **24 CSS pixel
> diameter circle is centered on the bounding box of each, the circles
> do not intersect another target**"
>
> "For important links/controls, consider aiming for the stricter
> 2.5.5 Target Size (Enhanced)."

**Lessons:**
- 24px is the absolute floor. Anything below that needs 24px spacing
  from neighbors. **We meet both bars easily with 44+ px shapes.**
- Spacing rule = if any button is <24px, it must be 24px-clear of
  neighbors. Bean shapes overlap their bounding box geometrically;
  treat the geometric centroid as the bounding center for spacing.

### V4. Google Android developer docs (touch target size, contrast)
**Source:** https://developer.android.com/guide/topics/ui/accessibility (verbatim)

> "Learn about **color contrast, touch target size, content labeling**,
> and other practices that make a big difference to your users."
>
> "Accessibility features benefit all users."

**Lessons:**
- **Contrast is non-optional.** Yellow + green + red on a beige
  window face = fail. We need **dark glyphs on saturated
  backgrounds** (as the reference sketch shows).
- **Content labeling** — color alone isn't enough; each button needs
  a recognizable shape OR glyph. This is why the reference has
  yellow=bean, green=circle, red=circle-with-X, purple=bean. Three
  shapes, four colors.
- Google's Material Design target size is 48dp (≈48px). Slightly
  more generous than WCAG AA.

### V5. iOS AssistiveTouch (training-ground-truth, Apple HIG JS-walled)
**iOS since 5 (2011).** Apple HIG requires JS — direct retrieval
blocked; relying on training-ground-truth.

- **One giant primary button** (≈240px diameter circle), draggable,
  translucent over any content.
- Long-press reveals a panel of secondary actions.
- **Single primary entry point** = Hick's law collapsed; no
  confusion about which button is "the" button.

**Lessons:**
- **Hick's law applies to the cluster as a whole** — fewer distinct
  gestures per button is better than more. Our red button (close)
  has 3 different behaviors (click, partial-drag, full-drag); the
  other 3 buttons should each have ONE behavior (click).
- **Translucent / no panel background** — buttons float on the
  window face, not on a colored sticker. (Confirmed by the v2
  reference: NO lavender panel box.)

---

## The 12 invariants

These are derived from V1–V5. **Any a11y cluster design that violates
ANY of these is wrong, regardless of whether it matches a reference
image or "looks OK in a screenshot".**

### I1 — Size: every button exceeds WCAG 2.5.5 AAA (44px)
- Minimum diameter of any single button: **44px**
- Default scaling: **48px** (Android Material convention)
- Scale-up rules: button size scales with `min(window_w, window_h)`
  so the cluster reads correctly on small AND large windows.

### I2 — Size hierarchy by reversibility (NOT by frequency)
- **Most-used ≠ biggest.** The biggest button is the one whose
  consequence is hardest to reverse.
- Close/Purge (irreversible) > Resize (annoying to redo) >
  Move (reversible) > Minimize (easily reversed)
- **Result: yellow minimize is the LARGEST, red close is small.**
  This INVERTS the GC convention (A=largest = primary). The user's
  elderly-target audience cares about not breaking things, not
  about hitting the most-used button fast.

### I3 — Shape vocabulary = 3 shapes (kidney + circle + circle)
- **Yellow = kidney bean** (crescent with rounded tips)
- **Green = circle**
- **Red = circle with X-glyph**
- **Purple = kidney bean** (mirrors yellow for shape symmetry)
- Two kidney beans + two circles = a balanced shape vocabulary
  that reads even in monochrome (color-blind users).

### I4 — Position: cluster floats in content area, NOT corners
- Cluster anchor = `(win.x + border_w + 8, win.y + title_bar_h + 8)`
  — top-left of content rect with 8px breathing room.
- **Purple resize grips live at the window's BOTTOM-LEFT and
  BOTTOM-RIGHT CORNERS** (separate from the cluster).
  - Reason: corners are "infinite edges" (Fitts's law, cursor
    can't overshoot). The cluster has the most-used controls;
    corners have the most-spatial-control. Don't conflate.

### I5 — No panel background — buttons float on the window face
- NO colored sticker rectangle behind the buttons.
- NO drop shadow that reads as a halo.
- Each button draws directly on the theme's `win_face`.
- This is **explicitly what the v2 reference image shows** and
  what "looks like a clean HUD" requires.

### I6 — Color contrast: glyphs must be DARK on saturated face
- Glyph color: `darken(face, 60%)` minimum contrast.
- The v2 reference uses dark drag mark on green, dark X on red,
  dark handle on yellow. White-on-saturated reads as alien.
- WCAG AA text contrast = 4.5:1. Our glyphs should hit at least
  7:1 (AAA).

### I7 — Reachability: 24px-clear of nearest neighbor
- Any button center is at least 24px from any other button center
  (WCAG 2.5.8 spacing rule, even though our buttons all exceed
  the 24px diameter floor).
- Yellow/green/red cluster: arranged as a triangle with pairwise
  distance ≥ 32px (1.33× the floor, comfortable margin).
- Purple grips: in the window corners, ≥ window_w/3 from the
  cluster — never visually adjacent.

### I8 — Reversibility ladder on destructive button
- The CLOSE button (red) MUST have a three-way release ladder
  matching the user's previous design:
  - **click** = end session (close)
  - **full drag (≥30px)** = purge cache + close (irreversible)
  - **partial drag (<30px)** = STIM feedback pulse (window STAYS
    ALIVE — "a partial drag does not close the window")
- The other three buttons have ONE behavior each (no ladder),
  because they are reversible or low-consequence.

### I9 — Edge-detection reveal on purple resize
- Purple resize grips are **invisible (alpha 0) unless** the cursor
  is within ~46px of the nearest corner.
- Fade ramp: 0 alpha at 46px → full alpha at 18px.
- Once a resize drag is active, grips stay full-alpha.
- Hit-test uses the same 46px proximity window so you can't grab
  what you can't see.

### I10 — Geometry mirrored in 5 files (sync or test fails)
- `src/gui/wubu_a11y.c` — render + hit-test
- `src/gui/wubu_a11y.h` — public API + macros
- `src/gui/wubu_a11y_test.c` — unit tests (geometry constants)
- `src/gui/wubu_a11y_shot.c` — capture harness
- `wubu_desktop_shot.c` — desktop shot harness
- Drift = `test_a11y` aborts. Sync all five or none.

### I11 — Theme-aware
- Cluster uses `tc()->win_face` for the no-panel background
  (inherits the active theme's window face color).
- Glyph colors come from `tc()->*_dark` palette slots (every theme
  must define these).
- The button face colors (yellow, green, red, purple) are HARDCODED
  — they encode identity, not themability.

### I12 — Verification: every rendered frame must pass
- `make test_a11y` — unit-level geometry + hit-test (green)
- `make a11y_shot` — capture harness produces frames in
  `/tmp/wubu_a11y_shots/`
- **Pixel verification** (no vision model — direct PIL):
  - Yellow centroid matches (cx, cy) within ±2px
  - Green centroid matches within ±2px
  - Red centroid matches within ±2px
  - Purple grips visible at both window bottom corners
  - Zero pixels of the old "panel box" background color
  - Glyphs are dark (R<100, G<100, B<100) on saturated button face

---

## Open design questions (decided per-render, not pre-baked)

These are NOT invariants because reasonable designs can pick either.
Three renders will be produced; the user picks one.

- **Q1: Triangle orientation.** Yellow on top (point-down) or
  yellow at left (point-right)? Both satisfy I4 + I7. The v2
  reference has yellow UPPER-LEFT (point-down-right).
- **Q2: Yellow color.** True yellow (255, 220, 50) or warm orange
  (240, 180, 60)? Both pass I6 contrast. The v2 reference uses
  yellow; the cairo render uses yellow; some prior attempts used
  orange.
- **Q3: Panel anchoring at top-left vs. centering.** v2 reference
  has the cluster at upper-left of content area (per I4). An
  alternative — center cluster horizontally, anchor at top of
  content — better satisfies the "predictable, learnable" elderly
  requirement but contradicts the reference.

## Reference images (for visual context, NOT geometry)

The following PNGs exist on disk and embody versions of this design.
They are **NOT** geometry sources — geometry derives from invariants.

| Image | Path | What it shows |
|---|---|---|
| v2 reference (latest) | `/tmp/wubu_png/a11y_design_v2.png` | User's latest sketch, GC-style diamond, horizontal green-red pair, purple corners. |
| cairo render | `/tmp/wubu_png/a11y_design_cairo.png` | Earlier cairo-generated render of the v1 cluster. |
| yellow detail | `/tmp/wubu_png/a11y_design_yellow.png` | Zoom of the yellow kidney bean. |
| purple bottom-right | `/tmp/wubu_png/a11y_design_purple_br.png` | Zoom of the corner resize grip. |
| purple bottom-left | `/tmp/wubu_png/a11y_design_purple_bl.png` | Zoom of the corner resize grip. |

## What this document is NOT

- It is **not** a coordinate spec. Coordinates follow in a separate
  commit (`a11y cluster: derive coordinates from invariants`).
- It is **not** an SVG. The SVG deliverable follows once the user
  picks a render.
- It is **not** a test plan. The test plan is in
  `references/a11y-invariants-test-plan.md` (TODO).

## What was wrong before (for the historical record)

- v3 ("GC diamond", `d46a7ce`): yellow r11 was too small (under AA);
  green r20 didn't satisfy AAA either; layout had purple in the
  cluster at bottom-LEFT, contradicting the user's "purple at both
  bottom corners" directive.
- v2 ("SVG-traced", `5efb8dc`): layout numbers (yellow at 0.289w,
  green at 0.363w, red at 0.579w) trace the user's reference image
  but violated I2 (red close was the smallest; should be large per
  AAA's "irreversible actions should be even larger").
- Pre-crash WIP (`wubu_a11y` stash): the doc said green at 0.65w and
  red at 0.85w but the code said green at 0.50cw and red at 0.66cw.
  Doc-vs-code drift = no single source of truth. **This document
  IS the single source of truth going forward.**
