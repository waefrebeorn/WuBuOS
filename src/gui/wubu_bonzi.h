/*
 * wubu_bonzi.h -- WuBuOS WuBu Buddy desktop mascot (AGI gateway).
 *
 * A WuBu-style purple companion that sits on the desktop as the friendly face of
 * the AGI. Clicking the buddy opens the HolyD/AGI terminal (the "Live
 * Colonel" ring-0 REPL where the Brain is hosted in the Body). It bobs
 * gently (idle animation), blinks, and shows a speech bubble greeting.
 *
 * Rendered as a chrome-less DESKTOP ELEMENT (like the a11y cluster) — the
 * WM render loop draws it above windows, the input loop hit-tests it. No
 * DosGuiWindow chrome, no taskbar entry.
 *
 * Self-contained: pixel-art drawn through vbe_* primitives only (no image
 * assets), following the house glyph discipline. Opaque API — no god headers.
 */

#ifndef WUBU_BONZI_H
#define WUBU_BONZI_H

#include <stdbool.h>
#include <stdint.h>

/* Enable/disable the desktop mascot. Default: enabled after wubu_bonzi_init. */
void wubu_bonzi_set_enabled(bool on);
bool wubu_bonzi_is_enabled(void);

/* Place the mascot at desktop coordinates (x,y). Idempotent; re-arms the
 * animation clock. Returns true on success. */
bool wubu_bonzi_init(int x, int y);

/* Mascot screen position (set by init). */
int  wubu_bonzi_x(void);
int  wubu_bonzi_y(void);
int  wubu_bonzi_w(void);
int  wubu_bonzi_h(void);

/* Advance the mascot animation clock (call each WM tick). dt_ms is
 * milliseconds since the last call. */
void wubu_bonzi_tick(int dt_ms);

/* Draw the mascot over the desktop. Called by dosgui_wm_render AFTER the
 * window loop and BEFORE the taskbar, like the a11y cluster. */
void wubu_bonzi_draw(uint32_t *fb, int fb_w, int fb_h);

/* Route a mouse event to the mascot. Returns true if consumed (clicked the
 * buddy or its speech bubble). */
bool wubu_bonzi_mouse(int x, int y, int btn, int kind);

/* The buddy's click action: launch the AGI HolyD terminal near the buddy. */
void wubu_bonzi_open_agi(void);

/* Balloon Help (System 7 lesson): retarget the speech-bubble text.
 * Callers pass short strings (<= 39 chars); they are copied, never aliased.
 * Pass NULL for either line to leave it unchanged. */
void wubu_bonzi_set_bubble(const char *line1, const char *line2);

#endif /* WUBU_BONZI_H */
