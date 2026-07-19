/*
 * dosgui_dos_window_test_stub.c -- Minimal WM window primitive stubs for the
 * DOS Box window test. We are testing the DOS-window glue, not the WM
 * compositor, so the window is a plain heap struct with no real rendering.
 */

#include "dosgui_wm.h"
#include <stdlib.h>
#include <string.h>

DosGuiWindow *dosgui_wm_create(int x, int y, int w, int h, const char *title) {
    DosGuiWindow *win = (DosGuiWindow *)calloc(1, sizeof(DosGuiWindow));
    if (!win) return NULL;
    win->x = x; win->y = y; win->w = w; win->h = h;
    if (title) {
        /* DosGuiWindow has a title field in the real WM; mirror it if present. */
    }
    (void)title;
    return win;
}

void dosgui_wm_destroy(DosGuiWindow *win) {
    if (win) free(win);
}
