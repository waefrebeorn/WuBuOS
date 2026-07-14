/*
 * dosgui_wm_window.c -- WuBuOS DosGui WM: window lifecycle + icons
 *
 * Self-contained concern split out of dosgui_wm.c (the WM facade):
 *   - Window lifecycle: spawn / close / raise / destroy / focus / lookup
 *   - Z-order + focus management
 *   - Virtual-desktop migration (Ctrl+Alt+Left/Right, Win+Shift+arrows)
 *   - Desktop icon registry (dosgui_icon_add / add_ex / shortcut)
 *   - Icon-layout persistence + wallpaper reload
 *
 * Depends only on the shared WM state (dosgui_wm_internal.h) and the settings
 * public API. No rendering, no input dispatch, no theme engine wiring.
 */

#include "dosgui_wm_internal.h"
#include "wubu_settings.h"

#include <string.h>
#include <stdio.h>

/* -- Z-order + window lifecycle ----------------------------------- */

void raise_win(int i) {
    int j = 0;
    while (j < g_dwm.nz && g_dwm.zorder[j] != i) j++;
    if (j == g_dwm.nz) return;
    for (; j < g_dwm.nz - 1; j++)
        g_dwm.zorder[j] = g_dwm.zorder[j + 1];
    g_dwm.zorder[g_dwm.nz - 1] = i;
}

int spawn_window(int x, int y, int w, int h, const char *title) {
    DosGuiWindow *win = NULL;
    int i;
    for (i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (!g_dwm.windows[i].alive) { win = &g_dwm.windows[i]; break; }
    }
    if (!win) return -1;

    memset(win, 0, sizeof(*win));
    win->id = g_dwm.next_id++;
    win->flags = DOSGUI_WIN_NORMAL;
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->desktop = g_dwm.current_desktop;  /* Assign to current virtual desktop */
    win->alive = true;
    strncpy(win->title, title ? title : "Window", sizeof(win->title) - 1);

    g_dwm.zorder[g_dwm.nz++] = i;
    g_dwm.focused_id = i;
    return i;
}

void close_win(int i) {
    if (i < 0 || i >= DOSGUI_MAX_WINDOWS) return;
    g_dwm.windows[i].alive = false;
    g_dwm.windows[i].flags = DOSGUI_WIN_UNUSED;
    int j = 0;
    while (j < g_dwm.nz && g_dwm.zorder[j] != i) j++;
    for (; j < g_dwm.nz - 1; j++)
        g_dwm.zorder[j] = g_dwm.zorder[j + 1];
    if (j < g_dwm.nz) g_dwm.nz--;
    if (g_dwm.drag_id == i) g_dwm.drag_id = -1;
    if (g_dwm.focused_id == i)
        g_dwm.focused_id = g_dwm.nz ? g_dwm.zorder[g_dwm.nz - 1] : -1;
}

int hit_test(int x, int y) {
    for (int j = g_dwm.nz - 1; j >= 0; j--) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.zorder[j]];
        if (w->alive && w->desktop == g_dwm.current_desktop &&
            x >= w->x && x < w->x + w->w &&
            y >= w->y && y < w->y + w->h)
            return g_dwm.zorder[j];
    }
    return -1;
}

DosGuiWindow *dosgui_wm_create(int x, int y, int w, int h,
                               const char *title) {
    int i = spawn_window(x, y, w, h, title);
    if (i < 0) return NULL;
    return &g_dwm.windows[i];
}

void dosgui_wm_destroy(DosGuiWindow *win) {
    if (!win) return;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (&g_dwm.windows[i] == win) { close_win(i); return; }
    }
}

void dosgui_wm_set_focus(DosGuiWindow *win) {
    if (!win) return;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (&g_dwm.windows[i] == win) {
            raise_win(i);
            g_dwm.focused_id = i;
            return;
        }
    }
}

DosGuiWindow *dosgui_wm_get_focused(void) {
    if (g_dwm.focused_id < 0) return NULL;
    return &g_dwm.windows[g_dwm.focused_id];
}

DosGuiWindow *dosgui_wm_find_by_id(int id) {
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++)
        if (g_dwm.windows[i].alive && g_dwm.windows[i].id == id)
            return &g_dwm.windows[i];
    return NULL;
}

int dosgui_wm_window_count(void) {
    return g_dwm.nz;
}

/* -- Virtual Desktop Migration ------------------------------------- */

void dosgui_wm_move_window_to_desktop(DosGuiWindow *win, int desktop) {
    if (!win) return;
    if (desktop < 0 || desktop >= g_dwm.desktop_count) return;
    win->desktop = desktop;
    /* If moved away from current desktop, unfocus it */
    if (win->desktop != g_dwm.current_desktop && g_dwm.focused_id >= 0) {
        DosGuiWindow *focused = &g_dwm.windows[g_dwm.focused_id];
        if (focused == win) {
            g_dwm.focused_id = -1;
        }
    }
}

int dosgui_wm_get_current_desktop(void) {
    return g_dwm.current_desktop;
}

void dosgui_wm_set_current_desktop(int desktop) {
    if (desktop < 0 || desktop >= g_dwm.desktop_count) return;
    g_dwm.current_desktop = desktop;
    /* Unfocus window if it's not on the new desktop */
    if (g_dwm.focused_id >= 0) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
        if (w->alive && w->desktop != g_dwm.current_desktop) {
            g_dwm.focused_id = -1;
        }
    }
}

/* Move focused window to adjacent desktop (Win+Shift+Left/Right) */
void dosgui_wm_move_focused_window(int delta) {
    if (g_dwm.focused_id < 0) return;
    DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
    if (!w->alive) return;
    int new_desktop = w->desktop + delta;
    if (new_desktop < 0) new_desktop = 0;
    if (new_desktop >= g_dwm.desktop_count) new_desktop = g_dwm.desktop_count - 1;
    if (new_desktop != w->desktop) {
        dosgui_wm_move_window_to_desktop(w, new_desktop);
    }
}

DosGuiWindow *dosgui_wm_spawn(int x, int y, int w, int h,
                               const char *title,
                               void (*on_draw)(DosGuiWindow *, uint32_t *, int, int)) {
    int i = spawn_window(x, y, w, h, title);
    if (i < 0) return NULL;
    g_dwm.windows[i].on_draw = on_draw;
    return &g_dwm.windows[i];
}

/* -- Desktop Icons -------------------------------------------------- */

int dosgui_icon_add(const char *name, int gx, int gy,
                        void (*on_click)(void)) {
    if (g_dwm.icon_count >= DOSGUI_MAX_ICONS) return -1;
    DosGuiIcon *icon = &g_dwm.icons[g_dwm.icon_count];
    memset(icon, 0, sizeof(*icon));
    strncpy(icon->name, name, sizeof(icon->name) - 1);
    icon->grid_x = gx; icon->grid_y = gy;
    icon->x = 20 + gx * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    icon->y = 20 + gy * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
    icon->on_click = on_click;
    icon->type = DESK_ICON_APP;
    icon->icon_color = 0x0080FF;  /* Default blue */
    icon->alive = true;           /* Registered icon is live (matches dosgui_icon_add_ex) */
    return g_dwm.icon_count++;
}

int dosgui_icon_add_ex(const char *name, DeskIconType type,
                        const char *target, int gx, int gy,
                        uint32_t icon_color, void (*on_execute)(void)) {
    if (g_dwm.icon_count >= DOSGUI_MAX_ICONS) return -1;
    DosGuiIcon *icon = &g_dwm.icons[g_dwm.icon_count];
    memset(icon, 0, sizeof(*icon));
    strncpy(icon->name, name, sizeof(icon->name) - 1);
    icon->grid_x = gx; icon->grid_y = gy;
    icon->x = 20 + gx * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    icon->y = 20 + gy * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
    icon->type = type;
    icon->icon_color = icon_color ? icon_color : 0x0080FF;
    if (target) strncpy(icon->target, target, sizeof(icon->target) - 1);
    icon->on_execute = on_execute;
    icon->alive = true;
    return g_dwm.icon_count++;
}

/* Shortcut Creation */
int dosgui_shortcut_create(const char *name, const char *target,
                            const char *description, int grid_x, int grid_y) {
    return dosgui_icon_add_ex(name, DESK_ICON_SHORTCUT, target, grid_x, grid_y, 0x00FF00, NULL);
}

/* -- Icon persistence (Stream 2) & wallpaper reload (Stream 4) ---- */

bool dosgui_wm_is_initialized(void) { return g_dwm.screen_w > 0; }

int dosgui_wm_wallpaper_mode(void) { return g_dwm.wallpaper_mode; }
int dosgui_wm_wallpaper_w(void)    { return g_dwm.wallpaper_w; }
int dosgui_wm_wallpaper_h(void)    { return g_dwm.wallpaper_h; }

/* Persist the current live desktop icon grid into settings
 * (ReactOS-style: store string name + position, not callbacks). */
void dosgui_wm_save_icon_layout(void) {
    WubuSettings *s = wubu_settings_mut();
    if (!s) return;
    s->theme.icon_layout_count = 0;
    for (int i = 0; i < g_dwm.icon_count && s->theme.icon_layout_count < WUBU_ICON_LAYOUT_MAX; i++) {
        DosGuiIcon *ic = &g_dwm.icons[i];
        if (!ic->alive) continue;
        IconLayoutEntry *e = &s->theme.icon_layout[s->theme.icon_layout_count++];
        strncpy(e->name, ic->name, sizeof(e->name) - 1);
        e->grid_x = ic->grid_x;
        e->grid_y = ic->grid_y;
        e->alive  = true;
    }
    wubu_settings_save();
}

/* Restore live icon grid positions from persisted settings, matching
 * by icon name. Positions survive a desktop restart. */
void dosgui_wm_restore_icon_layout(void) {
    const WubuSettings *s = wubu_settings_get();
    if (!s || s->theme.icon_layout_count <= 0) return;
    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *ic = &g_dwm.icons[i];
        if (!ic->alive) continue;
        for (int j = 0; j < s->theme.icon_layout_count; j++) {
            const IconLayoutEntry *e = &s->theme.icon_layout[j];
            if (e->alive && strncmp(e->name, ic->name, sizeof(e->name)) == 0) {
                ic->grid_x = e->grid_x;
                ic->grid_y = e->grid_y;
                ic->x = 20 + e->grid_x * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
                ic->y = 20 + e->grid_y * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
                break;
            }
        }
    }
}

/* Re-decode the configured wallpaper from settings (Control Panel apply). */
void dosgui_wm_reload_wallpaper(void) {
    if (g_dwm.wallpaper) { free(g_dwm.wallpaper); g_dwm.wallpaper = NULL; }
    load_default_wallpaper();
}
