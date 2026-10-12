/*
 * minesweeper_test_stub.c  --  stubs for the VBE/WM symbols minesweeper.c
 * references, so the game logic can be unit-tested without the GUI stack.
 */
#include "../kernel/vbe.h"
#include "../gui/dosgui_wm.h"
#include <stdlib.h>

/* VBE drawing stubs (no-ops for logic tests) */
void vbe_fill_rect(int x, int y, int w, int h, uint32_t color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
}
void vbe_rect(int x, int y, int w, int h, uint32_t color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
}
void vbe_draw_text(int x, int y, const char *s, uint32_t color, int scale) {
    (void)x; (void)y; (void)s; (void)color; (void)scale;
}

/* WM chrome/create stubs */
ChromeContentRect dosgui_chrome_draw_window(DosGuiWindow *win, uint32_t *fb,
                                            int fb_w, int fb_h) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h;
    ChromeContentRect r = { 0, 0, 320, 240 };
    return r;
}
DosGuiWindow *dosgui_wm_create(int x, int y, int w, int h, const char *title) {
    (void)x; (void)y; (void)w; (void)h; (void)title;
    return NULL;
}
