/*
 * taskbar_overflow_menu.h -- the list behind the taskbar's "+N" chip.
 *
 * Windows hides taskbar buttons that do not fit behind a chip, and the chip opens a list of
 * exactly those windows. This is that list: a small card anchored above the bar, holding the
 * identities the layout withheld (WmTaskLayout.hidden_*), so the user sees WHICH windows are
 * hidden, not just how many.
 */

#ifndef WUBU_TASKBAR_OVERFLOW_MENU_H
#define WUBU_TASKBAR_OVERFLOW_MENU_H

#include <stdint.h>
#include <stdbool.h>

/* Snapshot the hidden windows from the live layout and open the card. */
void taskbar_overflow_open(void);
void taskbar_overflow_close(void);
bool taskbar_overflow_is_open(void);

void taskbar_overflow_render(uint32_t *fb, int fb_w, int fb_h);
bool taskbar_overflow_click(int x, int y);        /* true when consumed */
bool taskbar_overflow_key(uint32_t key);          /* Esc closes; returns true when consumed */

/* Test surface. */
int  taskbar_overflow_rows(void);
int  taskbar_overflow_window_at(int i);   /* -1 for a pinned app: it has no window */
bool taskbar_overflow_row_rect(int i, int fb_w, int fb_h, int *x, int *y, int *w, int *h);

#endif /* WUBU_TASKBAR_OVERFLOW_MENU_H */
