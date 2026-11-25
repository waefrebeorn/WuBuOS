/*
 * win11_snap.h -- Win+Z snap-layout flyout (Windows 11 parity).
 *
 * Win+Z (and hovering the maximize button) opens a picker of six LAYOUTS.  Each
 * layout is an arrangement of two to four unequal regions; clicking a region snaps
 * the focused window into that region.  The arrangements themselves live in
 * wm_layout.c (wubu_snap_layout_*), because the snapping path and this renderer must
 * agree about where every region is.
 */

#ifndef WUBU_WIN11_SNAP_H
#define WUBU_WIN11_SNAP_H

#include <stdint.h>
#include <stdbool.h>

void win11_snap_toggle(void);
bool win11_snap_is_open(void);
void win11_snap_close(void);

void win11_snap_render(uint32_t *fb, int fb_w, int fb_h);

/* Returns true when the press was consumed by the flyout. */
bool win11_snap_click(int x, int y);

/* Keyboard selection while the flyout is open. Windows 11 makes you use the mouse to
 * choose a REGION; here the arrows move the selection (by real geometry, see
 * wubu_snap_neighbor), 1-6 chooses the layout, Enter applies and Esc closes.
 * Returns true when the key was consumed. */
bool win11_snap_key(uint32_t key);

/* The current selection, for rendering and for tests. -1 when closed. */
int  win11_snap_selected_zone(void);
int  win11_snap_selected_layout(void);

/* Picker geometry shared with the WM: how many buttons there are and how big each
 * one is.  Exposed so callers positioning or hit-testing around the flyout do not
 * restate these numbers. */
/* -- Snap assist ---------------------------------------------------
 * Windows offers the remaining windows in the half you did NOT just fill. Without it a
 * snap leaves half the screen empty and the user drags a window there by hand. */
void win11_snap_assist_open(int free_zone, int exclude_id);  /* 0 = left, 1 = right half */
void win11_snap_assist_close(void);
bool win11_snap_assist_active(void);
int  win11_snap_assist_zone(void);
/* Candidates, most recently focused first, excluding `exclude_id`. Returns the count. */
int  win11_snap_assist_candidates(int *ids, int max, int exclude_id);
void win11_snap_assist_render(uint32_t *fb, int fb_w, int fb_h);
/* Card i's rect, for hit-testing and for tests -- one geometry owner, not two. */
bool win11_snap_assist_card_rect(int i, int *x, int *y, int *w, int *h);
bool win11_snap_assist_click(int x, int y);   /* true when the click was consumed */

int win11_snap_button_count(void);
int win11_snap_button_size(int *w, int *h);

#endif /* WUBU_WIN11_SNAP_H */
