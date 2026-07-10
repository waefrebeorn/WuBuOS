/* dosgui_startmenu_power.c -- Power options subsystem for the Start menu.
 *
 * Self-contained: power action handling (shutdown/restart/logoff/sleep) and
 * the power-options render. Uses g_open (extern in dosgui_startmenu_internal.h)
 * and the public dosgui_shutdown()/dosgui_startmenu_close() API. Minimal includes.
 */

#include "dosgui_startmenu_internal.h"
#include <stdio.h>

void dosgui_startmenu_power(PowerAction action) {
    switch (action) {
        case PWR_SHUTDOWN:
            dosgui_shutdown();
            break;
        case PWR_RESTART:
            dosgui_shutdown();
            break;
        case PWR_LOGOFF:
            break;
        case PWR_SLEEP:
            break;
        case PWR_HIBERNATE:
            break;
    }
    dosgui_startmenu_close();
}

void dosgui_startmenu_render_power_options(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w) {
    (void)fb; (void)fb_w; (void)fb_h;
    
    const char *labels[] = { "Shut Down", "Restart", "Log Off", "Sleep", "Hibernate" };
    int count = sizeof(labels) / sizeof(labels[0]);
    int item_h = 28;
    int ty = y;
    
    for (int i = 0; i < count; i++) {
        vbe_fill_rect_rounded(x, ty, w, item_h, 4, tc()->btn_face);
        vbe_3d_sunken_rounded_colors(x, ty, w, item_h, 4,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
        vbe_draw_text(x + 10, ty + (item_h - 8) / 2, labels[i], tc()->btn_text, 1);
        ty += item_h + 4;
    }
}
