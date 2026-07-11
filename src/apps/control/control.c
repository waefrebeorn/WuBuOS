/*
 * control.c  --  Control Panel (9 tabs) - minimal stub
 */

#include "control.h"
#include "../gui/dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "../gui/wubu_settings.h"
#include <stdlib.h>
#include <string.h>

ControlState* control_create(void) {
    return calloc(1, sizeof(ControlState));
}

void control_destroy(ControlState *ctrl) {
    free(ctrl);
}

void control_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, ControlState *ctrl) {
    (void)fb_w; (void)fb_h; (void)ctrl;
    if (!win || !fb) return;
    int x = win->x, y = win->y;
    vbe_fill_rect(x, y, win->w, win->h, 0x00E0E0E0);
    vbe_draw_text(x + 12, y + 8, "Control Panel - Desktop", 0x000000, 1);

    const WubuSettings *s = wubu_settings_get();
    char buf[256];
    snprintf(buf, sizeof(buf), "Wallpaper: %s",
             (s && s->theme.wallpaper_path[0]) ? s->theme.wallpaper_path : "(default gradient)");
    vbe_draw_text(x + 12, y + 40, buf, 0x000000, 1);

    static const char *modes[] = {"Center", "Tile", "Stretch", "Fit", "Fill"};
    int m = s ? s->theme.wallpaper_mode : 1;
    if (m < 0 || m > 4) m = 1;
    snprintf(buf, sizeof(buf), "Placement: %s", modes[m]);
    vbe_draw_text(x + 12, y + 60, buf, 0x000000, 1);

    /* Preview swatch of the configured placement mode. */
    vbe_fill_rect(x + 12, y + 80, 64, 48, 0x00408080);
    vbe_rect(x + 12, y + 80, 64, 48, 0x000000);
}

/* Apply a Desktop-tab change: persist wallpaper path + placement mode to
 * settings, then trigger a live wallpaper reload if the WM is running.
 * Mirrors ReactOS desk.cpl SetDesktopSettings() + refresh message. */
void control_desktop_apply(const char *wallpaper_path, int mode) {
    WubuSettings *s = wubu_settings_mut();
    if (!s) return;
    if (wallpaper_path && *wallpaper_path)
        strncpy(s->theme.wallpaper_path, wallpaper_path, sizeof(s->theme.wallpaper_path) - 1);
    if (mode >= 0 && mode <= 4) s->theme.wallpaper_mode = mode;
    wubu_settings_save();
    if (dosgui_wm_is_initialized()) dosgui_wm_reload_wallpaper();
}

DosGuiWindow* control_launch(void) {
    return dosgui_wm_create(80, 60, 520, 440, "Control Panel");
}

void control_set_tab(ControlState *ctrl, int tab) {
    if (tab >= 0 && tab < 9) ctrl->active_tab = tab;
}