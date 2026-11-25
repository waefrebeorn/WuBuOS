/*
 * alt_tab_switcher.h -- the Alt+Tab switcher (Win11): previews while Alt is held.
 *
 * This exists because the WM is press-driven and deliberately drops key-ups, so Alt+Tab could
 * only ever "cycle now". A switcher needs to know when Alt is RELEASED -- that is the commit.
 * Rather than change what every widget sees, key-ups are routed here alone: the WM calls
 * alt_tab_key_up() and nothing else changes.
 */

#ifndef WUBU_ALT_TAB_SWITCHER_H
#define WUBU_ALT_TAB_SWITCHER_H

#include <stdint.h>
#include <stdbool.h>

/* Alt+Tab pressed: open (or advance). Returns true when the switcher consumed the key. */
bool alt_tab_key_down(uint32_t key, uint32_t mods);
/* Any key release. Returns true when it committed and the caller should stop. */
bool alt_tab_key_up(uint32_t key, uint32_t mods);
bool alt_tab_is_open(void);
void alt_tab_close(bool commit);
void alt_tab_render(uint32_t *fb, int fb_w, int fb_h);
bool alt_tab_click(int x, int y);          /* true when consumed */

/* Test surface: the model behind the pixels. */
int  alt_tab_count(void);                  /* candidates shown */
int  alt_tab_selected(void);               /* index within the grid */
int  alt_tab_window_at(int i);             /* window id at grid position i */
bool alt_tab_card_rect(int i, int fb_w, int fb_h, int *x, int *y, int *w, int *h);

#endif /* WUBU_ALT_TAB_SWITCHER_H */
