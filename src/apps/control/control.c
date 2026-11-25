/*
 * control.c  --  Control Panel (XP-classic two-pane)
 *
 * Left rail (identity + "See also"), right pane: "Pick a category" grid over a
 * detail pane whose buttons act on the RUNNING system (theme, wallpaper
 * placement, desktop icon policy, live sensors).
 *
 * This panel used to be decorative: the eight categories were painted but no
 * mouse handler existed, and `active_tab` -- settable through control_set_tab()
 * and cycled by ","/"." -- was never read by the renderer, so every category
 * looked identical. Now control_layout() owns the geometry for BOTH the
 * category grid and the detail buttons, control_draw() paints those rects, and
 * control_mouse() hit-tests the same ones.
 *
 * Only actions that exist are offered as buttons; categories with nothing real
 * behind them say so instead of faking a control.
 */

#include "control.h"
#include "../gui/wm.h"
#include "../gui/wm_internal.h"
#include "../gui/wubu_theme.h"
#include "../gui/wubu_settings.h"
#include "../kernel/vbe.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* Optional cross-layer call: the thermal provider lives in the kernel driver
 * and in the hosted HW shim, but not every link (unit tests) has one. Weak, so
 * a panel without a sensor reports "no thermal zone" instead of failing to
 * link -- the same pattern the tree uses for optional services. */
extern int wubu_thermal_temp(int zone) __attribute__((weak));

#define CP_CATS      8
#define CP_ACTS_MAX  6

/* What a detail-pane button does. */
typedef enum {
    CP_ACT_NONE = 0,
    CP_ACT_THEME_WIN98, CP_ACT_THEME_LUNA, CP_ACT_THEME_MEDIA, CP_ACT_THEME_DARK,
    CP_ACT_WP_NEXT, CP_ACT_WP_RELOAD,
    CP_ACT_AUTOARRANGE, CP_ACT_ICONS
} CpAction;

typedef struct { int x, y, w, h; const char *name; } CpCatRect;
typedef struct { int x, y, w, h; CpAction id; const char *label; } CpActRect;

struct ControlState {
    int active_tab;
    /* Geometry, computed by control_layout() from the last content rect. */
    int lx, ly, lw, lh;          /* left rail */
    int dx, dy, dw, dh;          /* detail pane */
    CpCatRect cats[CP_CATS];
    int cat_n;
    CpActRect acts[CP_ACTS_MAX];
    int act_n;
};

ControlState* control_create(void) {
    return calloc(1, sizeof(ControlState));
}

void control_destroy(ControlState *ctrl) {
    free(ctrl);
}

/* -- Category table ---------------------------------------------- */

static const struct { const char *name; DeskIconType type; } g_cats[CP_CATS] = {
    {"Appearance and Themes", DESK_ICON_APP},
    {"Display",               DESK_ICON_FILE},
    {"Folder Options",        DESK_ICON_FOLDER},
    {"Taskbar and Start Menu",DESK_ICON_APP},
    {"Date and Time",         DESK_ICON_FILE},
    {"Sound and Audio",       DESK_ICON_FILE},
    {"System",                DESK_ICON_APP},
    {"User Accounts",         DESK_ICON_FOLDER},
};

/* -- Geometry (single owner) ------------------------------------- */

static void cp_add_act(ControlState *c, CpAction id, const char *label) {
    if (c->act_n >= CP_ACTS_MAX) return;
    CpActRect *a = &c->acts[c->act_n];
    a->id = id;
    a->label = label;
    a->x = c->dx + 8;
    a->y = c->dy + 24 + c->act_n * 26;
    a->w = c->dw - 16;
    a->h = 22;
    c->act_n++;
}

static void control_layout(ControlState *c, int cx, int cy, int cw, int ch) {
    c->lx = cx;
    c->ly = cy;
    c->lw = 150;
    if (c->lw > cw / 2) c->lw = cw / 2;
    c->lh = ch;

    int gx0 = cx + c->lw + 14;
    int gy0 = cy + 60;
    int cellw = 170, cellh = 70;
    if (gx0 + 2 * cellw > cx + cw) cellw = (cx + cw - gx0) / 2;
    if (cellw < 90) cellw = 90;

    c->cat_n = 0;
    for (int i = 0; i < CP_CATS; i++) {
        int col = i % 2, row = i / 2;
        CpCatRect *r = &c->cats[c->cat_n++];
        r->x = gx0 + col * cellw;
        r->y = gy0 + row * cellh;
        r->w = cellw - 6;
        r->h = cellh - 6;
        r->name = g_cats[i].name;
    }

    /* Detail pane under the grid. */
    c->dx = gx0;
    c->dy = gy0 + 4 * cellh + 4;
    c->dw = cx + cw - gx0 - 4;
    c->dh = cy + ch - c->dy - 4;
    if (c->dw < 40) c->dw = 40;
    if (c->dh < 24) c->dh = 24;

    /* Buttons for the selected category -- only ones with a real effect. */
    c->act_n = 0;
    switch (c->active_tab) {
    case 0:  /* Appearance and Themes */
        cp_add_act(c, CP_ACT_THEME_WIN98, "Theme: Win98 Classic");
        cp_add_act(c, CP_ACT_THEME_LUNA,  "Theme: XP Luna Blue");
        cp_add_act(c, CP_ACT_THEME_MEDIA, "Theme: XP Media Orange");
        cp_add_act(c, CP_ACT_THEME_DARK,  "Theme: Dark");
        cp_add_act(c, CP_ACT_WP_RELOAD,   "Reload wallpaper");
        break;
    case 1:  /* Display */
        cp_add_act(c, CP_ACT_WP_NEXT,   "Next wallpaper placement");
        cp_add_act(c, CP_ACT_WP_RELOAD, "Reload wallpaper");
        break;
    case 2:  /* Folder Options */
        cp_add_act(c, CP_ACT_AUTOARRANGE, "Toggle auto-arrange icons");
        cp_add_act(c, CP_ACT_ICONS,       "Toggle desktop icons");
        break;
    default:
        break;   /* text-only categories: no fake buttons */
    }
}

/* -- Actions ----------------------------------------------------- */

static void control_run(ControlState *c, CpAction id) {
    (void)c;
    WubuSettings *s = wubu_settings_mut();
    switch (id) {
    case CP_ACT_THEME_WIN98: control_set_theme(THEME_WIN98_CLASSIC); break;
    case CP_ACT_THEME_LUNA:  control_set_theme(THEME_XP_LUNA_BLUE); break;
    case CP_ACT_THEME_MEDIA: control_set_theme(THEME_XP_MEDIA_ORANGE); break;
    case CP_ACT_THEME_DARK:  control_set_theme(THEME_DARK); break;
    case CP_ACT_WP_NEXT:
        if (s) control_desktop_apply(NULL, (s->theme.wallpaper_mode + 1) % 5);
        break;
    case CP_ACT_WP_RELOAD:
        if (wm_is_initialized()) wm_reload_wallpaper();
        break;
    case CP_ACT_AUTOARRANGE:
        control_set_auto_arrange(!wm_get_auto_arrange());
        break;
    case CP_ACT_ICONS:
        if (s) control_set_show_icons(!s->theme.show_desktop_icons);
        break;
    default:
        break;
    }
}

/* -- Drawing ----------------------------------------------------- */

/* The one-line state of a text-only category, or NULL. */
static const char *cp_category_note(ControlState *c) {
    static char note[192];
    switch (c->active_tab) {
    case 3:
        return "Taskbar and Start Menu: the bar is always revealed; apps open from Start.";
    case 4: {
        time_t now = time(NULL);
        struct tm *lt = localtime(&now);
        if (!lt) return "Date and Time: clock unavailable";
        snprintf(note, sizeof note, "Date and Time: %04d-%02d-%02d %02d:%02d:%02d",
                 lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
                 lt->tm_hour, lt->tm_min, lt->tm_sec);
        return note;
    }
    case 5:
        return "Sound and Audio: no mixer is wired to this panel yet.";
    case 6: {
        int t = wubu_thermal_temp ? wubu_thermal_temp(0) : 0;
        if (t <= 0) return "System: no thermal zone reported";
        snprintf(note, sizeof note, "System: CPU %d C", t);
        return note;
    }
    case 7:
        return "User Accounts: single local account (wubu); no account editor yet.";
    default:
        return NULL;
    }
}

static void control_draw_content(DosGuiWindow *win, const ChromeContentRect *content, ControlState *ctrl) {
    (void)win;
    if (!content) return;
    ControlState *c = ctrl;
    if (!c) return;

    int cx = content->x, cy = content->y, cw = content->w, ch = content->h;
    control_layout(c, cx, cy, cw, ch);

    /* Right pane background. */
    vbe_fill_rect(cx, cy, cw, ch, tc()->win_face);

    /* Left rail (XP blue gradient). */
    if (theme()->Luna_start_button)
        vbe_vgradient(c->lx, c->ly, c->lw, c->lh, tc()->startmenu_sidebar,
                      tc()->startmenu_sidebar_grad_end);
    else
        vbe_fill_rect(c->lx, c->ly, c->lw, c->lh, tc()->btn_face);

    int ox = cx + 12, oy = cy + 12;
    /* Four panes of ONE accent, not Microsoft's four-colour flag: this is our OS, and a
     * borrowed logo is the same defect as the Win95 flag that used to sit on Start. */
    for (int pane = 0; pane < 4; pane++) {
        vbe_fill_rect(ox + (pane & 1) * 8, oy + (pane >> 1) * 8, 7, 7, tc()->select_bg);
    }
    vbe_draw_text(cx + 12, cy + 32, "Control Panel", tc()->win_title_text, 1);

    vbe_draw_text(cx + 12, cy + 64, "See also", tc()->win_title_text, 1);
    vbe_draw_text(cx + 12, cy + 80, "Switch to Classic View", tc()->win_title_text_ina, 1);
    vbe_draw_text(cx + 12, cy + 96, "Taskbar and Start Menu", tc()->win_title_text_ina, 1);
    vbe_draw_text(cx + 12, cy + 112, "Folder Options", tc()->win_title_text_ina, 1);

    /* "Pick a category" grid; the selected category is visibly selected. */
    vbe_draw_text(cx + c->lw + 14, cy + 10, "Pick a category", tc()->btn_text, 1);
    for (int i = 0; i < c->cat_n; i++) {
        CpCatRect *r = &c->cats[i];
        if (i == c->active_tab)
            vbe_fill_rect(r->x - 2, r->y - 2, r->w + 4, r->h + 4, tc()->win_title_active);
        uint32_t label_col = (i == c->active_tab) ? tc()->win_title_text : tc()->btn_text;
        wm_draw_icon_glyph(g_cats[i].type, r->x, r->y + 4,
                           tc()->btn_face, tc()->border_light,
                           tc()->border_dark, tc()->win_title_active);
        vbe_draw_text(r->x + 36, r->y + 4, r->name, label_col, 1);
    }

    /* Detail pane: what this category can actually do. */
    vbe_fill_rect(c->dx, c->dy, c->dw, c->dh,
                  tc()->btn_face);
    vbe_fill_rect(c->dx, c->dy, c->dw, 1, tc()->border_dark);
    vbe_draw_text(c->dx + 8, c->dy + 6, g_cats[c->active_tab].name,
                  tc()->win_title_active, 1);

    if (c->act_n == 0) {
        const char *note = cp_category_note(c);
        if (note) vbe_draw_text(c->dx + 8, c->dy + 24, note, tc()->btn_text, 1);
    } else {
        for (int i = 0; i < c->act_n; i++) {
            CpActRect *a = &c->acts[i];
            vbe_fill_rect(a->x, a->y, a->w, a->h, tc()->btn_face);
            vbe_fill_rect(a->x, a->y, a->w, 1, tc()->border_light);
            vbe_fill_rect(a->x, a->y + a->h - 1, a->w, 1, tc()->border_dark);
            vbe_draw_text(a->x + 8, a->y + 6, a->label, tc()->btn_text, 1);
        }
        if (c->active_tab == 1) {
            static const char *modes[] = {"Center", "Tile", "Stretch", "Fit", "Fill"};
            const WubuSettings *s = wubu_settings_get();
            int m = s ? s->theme.wallpaper_mode : 1;
            if (m < 0 || m > 4) m = 1;
            char buf[96];
            snprintf(buf, sizeof buf, "Placement now: %s", modes[m]);
            vbe_draw_text(c->dx + 8, c->acts[c->act_n - 1].y + 30, buf, tc()->btn_text, 1);
        }
    }
}

/* -- Input ------------------------------------------------------- */

void control_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    ControlState *c = win ? (ControlState *)win->user_data : NULL;
    if (!c) return;
    if (kind != 1 && !(btn & 1)) return;   /* press, not release */

    for (int i = 0; i < c->act_n; i++) {
        CpActRect *a = &c->acts[i];
        if (x >= a->x && x < a->x + a->w && y >= a->y && y < a->y + a->h) {
            control_run(c, a->id);
            return;
        }
    }
    for (int i = 0; i < c->cat_n; i++) {
        CpCatRect *r = &c->cats[i];
        if (x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h) {
            control_set_tab(c, i);
            return;
        }
    }
}

void control_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, ControlState *ctrl) {
    (void)fb;
    if (!win) return;
    ControlState *c = ctrl ? ctrl : (ControlState*)win->user_data;
    if (!c) return;

    VBEState *vbe = vbe_state();
    ChromeContentRect content = chrome_draw_window(win, vbe->back, fb_w, fb_h);
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
    if (wm_is_initialized()) wm_reload_wallpaper();
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
    /* apply_live, not bare wubu_theme_set: the a11y overrides (high contrast,
     * font scale) must survive a theme switch. */
    wubu_settings_apply_live();
}

void control_set_auto_arrange(bool on) {
    wm_set_auto_arrange(on);   /* persists + re-flows live */
}

void control_set_show_icons(bool show) {
    WubuSettings *s = wubu_settings_mut();
    if (!s) return;
    s->theme.show_desktop_icons = show;
    wubu_settings_save();
    wm_set_icons_visible(show);   /* live show/hide via public WM API */
}

static void control_draw_wm(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    ControlState *c = win ? (ControlState*)win->user_data : NULL;
    control_draw(win, fb, fb_w, fb_h, c);
}

/* Keyboard: "," / "." cycle categories, arrows step them. The grid highlights
 * the same tab the keys select. */
static void control_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)mods;
    ControlState *c = win ? (ControlState*)win->user_data : NULL;
    if (!c) return;
    if (key == ',') control_set_tab(c, (c->active_tab + CP_CATS - 1) % CP_CATS);
    else if (key == '.') control_set_tab(c, (c->active_tab + 1) % CP_CATS);
    else if (key == 0xE04B && c->active_tab > 0) control_set_tab(c, c->active_tab - 1);
    else if (key == 0xE04D && c->active_tab < CP_CATS - 1) control_set_tab(c, c->active_tab + 1);
}

DosGuiWindow* control_launch(void) {
    DosGuiWindow *win = wm_create(80, 60, 820, 620, "Control Panel");
    if (win) {
        ControlState *c = control_create();
        win->user_data = c;
        win->on_draw  = control_draw_wm;
        win->on_key   = control_key;
        win->on_mouse = control_mouse;
    }
    return win;
}

void control_set_tab(ControlState *ctrl, int tab) {
    if (ctrl && tab >= 0 && tab < CP_CATS) ctrl->active_tab = tab;
}

int control_get_tab(const ControlState *ctrl) {
    return ctrl ? ctrl->active_tab : 0;
}

/* -- Inspection (tests / debug) ---------------------------------- */

int control_category_count(const ControlState *c) { return c ? c->cat_n : 0; }

bool control_category_rect(const ControlState *c, int i, int *x, int *y, int *w, int *h) {
    if (!c || i < 0 || i >= c->cat_n) return false;
    if (x) *x = c->cats[i].x;
    if (y) *y = c->cats[i].y;
    if (w) *w = c->cats[i].w;
    if (h) *h = c->cats[i].h;
    return true;
}

int control_action_count(const ControlState *c) { return c ? c->act_n : 0; }

bool control_action_rect(const ControlState *c, int i, int *x, int *y, int *w, int *h) {
    if (!c || i < 0 || i >= c->act_n) return false;
    if (x) *x = c->acts[i].x;
    if (y) *y = c->acts[i].y;
    if (w) *w = c->acts[i].w;
    if (h) *h = c->acts[i].h;
    return true;
}

const char *control_action_label(const ControlState *c, int i) {
    if (!c || i < 0 || i >= c->act_n) return NULL;
    return c->acts[i].label;
}
