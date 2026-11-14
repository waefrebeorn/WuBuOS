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

/* Picker geometry shared with the WM: how many buttons there are and how big each
 * one is.  Exposed so callers positioning or hit-testing around the flyout do not
 * restate these numbers. */
int win11_snap_button_count(void);
int win11_snap_button_size(int *w, int *h);

#endif /* WUBU_WIN11_SNAP_H */
