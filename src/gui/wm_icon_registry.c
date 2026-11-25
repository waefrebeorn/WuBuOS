/*
 * wm_icon_registry.c -- WuBuOS desktop icon registry + layout persistence
 *
 * The icons themselves live in g_dwm (wm_internal.h); this file owns only the two
 * things that are not windowing:
 *   - registering an icon (name, grid cell, type, target, callback)
 *   - persisting the grid to settings and restoring it by name on the next boot
 *
 * No rendering, no input dispatch, no z-order. It sat inside wm_window.c, whose job is
 * window lifecycle and virtual desktops -- a registry that ran there meant every reader of
 * "windows" had to read icon persistence too.
 *
 * C11, self-contained leaf: wm_internal.h for the shared state, wubu_settings.h for the
 * store. Symbol names are unchanged (icon_add, icon_add_ex, shortcut_create,
 * wm_save_icon_layout, wm_restore_icon_layout) because headers and callers bind to them.
 */

#include "wm_internal.h"
#include "wubu_settings.h"

#include <string.h>

/* -- Registry ------------------------------------------------------- */

int icon_add(const char *name, int gx, int gy,
                        void (*on_click)(void)) {
    if (g_dwm.icon_count >= DOSGUI_MAX_ICONS) return -1;
    DosGuiIcon *icon = &g_dwm.icons[g_dwm.icon_count];
    memset(icon, 0, sizeof(*icon));
    strncpy(icon->name, name, sizeof(icon->name) - 1);
    icon->grid_x = gx; icon->grid_y = gy;
    icon->x = 20 + gx * DOSGUI_ICON_COL_PITCH;
    icon->y = 20 + gy * DOSGUI_ICON_ROW_PITCH;
    icon->on_click = on_click;
    icon->type = DESK_ICON_APP;
    icon->icon_color = 0x0080FF;  /* Default blue */
    icon->alive = true;           /* Registered icon is live (matches icon_add_ex) */
    return g_dwm.icon_count++;
}

int icon_add_ex(const char *name, DeskIconType type,
                        const char *target, int gx, int gy,
                        uint32_t icon_color, void (*on_execute)(void)) {
    if (g_dwm.icon_count >= DOSGUI_MAX_ICONS) return -1;
    DosGuiIcon *icon = &g_dwm.icons[g_dwm.icon_count];
    memset(icon, 0, sizeof(*icon));
    strncpy(icon->name, name, sizeof(icon->name) - 1);
    icon->grid_x = gx; icon->grid_y = gy;
    icon->x = 20 + gx * DOSGUI_ICON_COL_PITCH;
    icon->y = 20 + gy * DOSGUI_ICON_ROW_PITCH;
    icon->type = type;
    icon->icon_color = icon_color ? icon_color : 0x0080FF;
    if (target) strncpy(icon->target, target, sizeof(icon->target) - 1);
    icon->on_execute = on_execute;
    icon->alive = true;
    return g_dwm.icon_count++;
}

/* Shortcut Creation */
int shortcut_create(const char *name, const char *target,
                            const char *description, int grid_x, int grid_y) {
    (void)description;  /* kept in the public signature; the desktop stores target only */
    return icon_add_ex(name, DESK_ICON_SHORTCUT, target, grid_x, grid_y, 0x00FF00, NULL);
}

/* -- Layout persistence --------------------------------------------- */

/* Persist the current live desktop icon grid into settings
 * (ReactOS-style: store string name + position, not callbacks). */
void wm_save_icon_layout(void) {
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
void wm_restore_icon_layout(void) {
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
                ic->x = 20 + e->grid_x * DOSGUI_ICON_COL_PITCH;
                ic->y = 20 + e->grid_y * DOSGUI_ICON_ROW_PITCH;
                break;
            }
        }
    }
}
