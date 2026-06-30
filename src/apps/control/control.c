/*
 * control.c  --  Control Panel (9 tabs) - minimal stub
 */

#include "control.h"
#include "../gui/dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdlib.h>

struct ControlState {
    int active_tab;
};

ControlState* control_create(void) {
    return calloc(1, sizeof(ControlState));
}

void control_destroy(ControlState *ctrl) {
    free(ctrl);
}

void control_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, ControlState *ctrl) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h; (void)ctrl;
}

DosGuiWindow* control_launch(void) {
    return dosgui_wm_create(80, 60, 520, 440, "Control Panel");
}

void control_set_tab(ControlState *ctrl, int tab) {
    if (tab >= 0 && tab < 9) ctrl->active_tab = tab;
}