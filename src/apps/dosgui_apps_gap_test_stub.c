/*
 * dosgui_apps_gap_test_stub.c -- the WM stubs for the apps-gap test.
 *
 * notes.c/todo.c/music.c bind DosGuiWindow callbacks (dosgui_wm_create
 * + vbe_draw_text); the test only exercises the LOGIC (create/add/
 * scan/play), so these two functions get stub definitions here — the
 * real WM is far too heavy to link. (Precedent:
 * dosgui_dos_window_test_stub.c.)
 * C11.
 */
#include "../gui/dosgui_wm.h"

#include <stdlib.h>

/* a real DosGuiWindow would be allocated by the WM; the stubs return
 * NULL (the app launchers handle a NULL win). */
struct DosGuiWindow *dosgui_wm_create(int x, int y, int w, int h,
                                      const char *title)
{
    (void)x; (void)y; (void)w; (void)h; (void)title;
    return NULL;
}

void vbe_draw_text(int x, int y, const char *s, uint32_t color, int scale)
{
    (void)x; (void)y; (void)s; (void)color; (void)scale;
}
