/*
 * power_menu.h -- the Win+X power-user menu (Windows 11 parity).
 *
 * A small vertical menu anchored above the Start button.  Every entry maps to
 * something the shell already does; there are no dead items.
 */

#ifndef WUBU_POWER_MENU_H
#define WUBU_POWER_MENU_H

#include <stdint.h>
#include <stdbool.h>

void power_menu_open(void);
void power_menu_close(void);
bool power_menu_is_open(void);

/* Returns true when the press was inside the menu (and therefore consumed). */
bool power_menu_handle_click(int x, int y);

/* Escape closes.  Arrow keys move the highlight; Enter activates. */
void power_menu_handle_key(uint32_t key, uint32_t mods);

/* Geometry, so the WM and the pointer path agree. */
void power_menu_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h);

void power_menu_render(uint32_t *fb, int fb_w, int fb_h);

#endif /* WUBU_POWER_MENU_H */
