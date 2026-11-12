/*
 * win11_snap.h -- Win+Z snap-layout flyout (Windows 11 parity).
 *
 * Hovering a window's maximize button in Windows 11 shows a layout picker;
 * Win+Z opens the same picker from the keyboard.  The flyout is a small
 * rounded bar above the focused window's title bar with six zone buttons
 * (two halves + four quarters).  Picking one calls snap_window_to_zone().
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

#endif /* WUBU_WIN11_SNAP_H */
