/*
 * calc_test_stub.c  --  minimal linker stub for the calculator unit test.
 *
 * calc.c's calc_launch() references dosgui_wm_create(); the headless
 * engine test doesn't exercise the GUI launch path, so we provide a
 * trivial stub that satisfies the linker without pulling in the full
 * window-manager stack. DosGuiWindow is defined in dosgui_wm.h.
 */

#include "calc.h"
#include "../gui/dosgui_wm.h"
#include <stdlib.h>

DosGuiWindow *dosgui_wm_create(int x, int y, int w, int h,
                               const char *title) {
    (void)x; (void)y; (void)w; (void)h; (void)title;
    return calloc(1, sizeof(DosGuiWindow));
}
