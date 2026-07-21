/*
 * control.c  --  Control Panel (9 tabs) - minimal stub
 */

#include "control.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
#include "../gui/dosgui_window_chrome.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "../gui/wubu_settings.h"
#include <stdlib.h>
#include <string.h>

struct ControlState {
    int active_tab;
};

ControlState* control_create(void) {
    return calloc(1, sizeof(ControlState));
}

void control_destroy(ControlState *ctrl) {
    free(ctrl);
}

static void control_draw_content(DosGuiWindow *win, const ChromeContentRect *content, ControlState *ctrl) {
    (void)win;
    if (!content) return;
    ControlState *c = ctrl;
    if (!c) return;

    int cx = content->x;
    int cy = content->y;
    int cw = content->w;
    int ch = content->h;

    /* Right pane background (light panel). */
    vbe_fill_rect(cx, cy, cw, ch, theme()->Luna_start_button ? 0x00F8F8F8 : 0x00C0C0C0);

    /* -- Left rail (XP blue gradient) -- */
    int lw = 150;
    if (theme()->Luna_start_button)
        vbe_vgradient(cx, cy, lw, ch, tc()->startmenu_sidebar, tc()->startmenu_sidebar_grad_end);
    else
        vbe_fill_rect(cx, cy, lw, ch, 0x00C0C0C0);

    /* Orb + "Control Panel" banner. */
    int ox = cx + 12, oy = cy + 12;
    vbe_fill_rect(ox,      oy,      7, 7, 0xF24C3E);
    vbe_fill_rect(ox + 8,  oy,      7, 7, 0x6FCF3C);
    vbe_fill_rect(ox,      oy + 8,  7, 7, 0x39A0EC);
    vbe_fill_rect(ox + 8,  oy + 8,  7, 7, 0xFFC90E);
    vbe_draw_text(cx + 12, cy + 32, "Control Panel", 0x00FFFFFF, 1);

    /* "See also" link list (static, XP-style). */
    vbe_draw_text(cx + 12, cy + 64, "See also", 0x00FFFFFF, 1);
    vbe_draw_text(cx + 12, cy + 80, "Switch to Classic View", 0x00E0E0FF, 1);
    vbe_draw_text(cx + 12, cy + 96, "Taskbar and Start Menu", 0x00E0E0FF, 1);
    vbe_draw_text(cx + 12, cy + 112, "Folder Options", 0x00E0E0FF, 1);

    /* -- Right pane: "Pick a category" + icon grid -- */
    vbe_draw_text(cx + lw + 14, cy + 10, "Pick a category", 0x00000000, 1);

    /* Wallpaper/placement status (Appearance and Themes detail line). */
    const WubuSettings *s = wubu_settings_get();
    char buf[256];
    snprintf(buf, sizeof(buf), "Wallpaper: %s",
             (s && s->theme.wallpaper_path[0]) ? s->theme.wallpaper_path : "(default gradient)");
    vbe_draw_text(cx + lw + 14, cy + 26, buf, 0x00333333, 1);
    static const char *modes[] = {"Center", "Tile", "Stretch", "Fit", "Fill"};
    int m = s ? s->theme.wallpaper_mode : 1;
    if (m < 0 || m > 4) m = 1;
    snprintf(buf, sizeof(buf), "Placement: %s", modes[m]);
    vbe_draw_text(cx + lw + 14, cy + 40, buf, 0x00333333, 1);

    /* Category grid (2 columns x 4 rows) with glyph + label. */
    struct Cat { const char *name; DeskIconType type; };
    static const struct Cat cats[] = {
        {"Appearance and Themes", DESK_ICON_APP},
        {"Display",               DESK_ICON_FILE},
        {"Folder Options",        DESK_ICON_FOLDER},
        {"Taskbar and Start Menu",DESK_ICON_APP},
        {"Date and Time",         DESK_ICON_FILE},
        {"Sound and Audio",       DESK_ICON_FILE},
        {"System",                DESK_ICON_APP},
        {"User Accounts",         DESK_ICON_FOLDER},
    };
    int gx0 = cx + lw + 14, gy0 = cy + 60, cellw = 170, cellh = 70;
    int ncols = 2;
    for (int i = 0; i < 8; i++) {
        int col = i % ncols, row = i / ncols;
        int gx = gx0 + col * cellw;
        int gy = gy0 + row * cellh;
        dosgui_wm_draw_icon_glyph(cats[i].type, gx, gy,
                                  tc()->btn_face, tc()->border_light,
                                  tc()->border_dark, tc()->win_title_active);
        vbe_draw_text(gx + 36, gy + 4, cats[i].name, 0x00000000, 1);
    }
}

void control_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, ControlState *ctrl) {
    (void)fb_w; (void)fb_h;
    if (!win || !fb) return;
    ControlState *c = ctrl ? ctrl : (ControlState*)win->user_data;
    if (!c) return;

    /* Draw chrome (frame + title bar + buttons) and get content rect.
     * VBE drawing functions write to back buffer, so we pass the back buffer. */
    VBEState *vbe = vbe_state();
    ChromeContentRect content = dosgui_chrome_draw_window(win, vbe->back, fb_w, fb_h);

    /* Draw control panel content within chrome-provided rect. */
    control_draw_content(win, &content, c);
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

/* Switch the active theme (Win98 Classic / XP Luna Blue / XP Media Orange /
 * Zune / WuBu Custom). Persists theme_id and applies it live (ReactOS
 * themeui.dll / desk.cpl lesson). */
void control_set_theme(int theme_id) {
    if (theme_id < 0 || theme_id >= THEME_COUNT) return;
    WubuSettings *s = wubu_settings_mut();
    if (!s) return;
    s->theme.theme_id = theme_id;
    wubu_settings_save();
    wubu_theme_set((WubuThemeId)theme_id);
}

void control_set_auto_arrange(bool on) {
    dosgui_wm_set_auto_arrange(on);   /* persists + re-flows live */
}

void control_set_show_icons(bool show) {
    WubuSettings *s = wubu_settings_mut();
    if (!s) return;
    s->theme.show_desktop_icons = show;
    wubu_settings_save();
    dosgui_wm_set_icons_visible(show);   /* live show/hide via public WM API */
}

static void control_draw_wm(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    ControlState *c = win ? (ControlState*)win->user_data : NULL;
    control_draw(win, fb, fb_w, fb_h, c);
}

/* Keyboard: "," / "." cycle tabs; still accepts the public control_set_tab. */
static void control_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)mods;
    ControlState *c = win ? (ControlState*)win->user_data : NULL;
    if (!c) return;
    if (key == ',' ) control_set_tab(c, (c->active_tab + 8) % 9);
    else if (key == '.') control_set_tab(c, (c->active_tab + 1) % 9);
}

DosGuiWindow* control_launch(void) {
    DosGuiWindow *win = dosgui_wm_create(80, 60, 520, 440, "Control Panel");
    if (win) {
        ControlState *c = control_create();
        win->user_data = c;
        win->on_draw  = control_draw_wm;
        win->on_key   = control_key;
    }
    return win;
}

void control_set_tab(ControlState *ctrl, int tab) {
    if (tab >= 0 && tab < 9) ctrl->active_tab = tab;
}

int control_get_tab(const ControlState *ctrl) {
    return ctrl ? ctrl->active_tab : 0;
}