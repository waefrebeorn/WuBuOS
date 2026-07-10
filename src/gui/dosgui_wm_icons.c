/* dosgui_wm_icons.c -- Desktop icon subsystem for the window manager.
 *
 * Self-contained: desktop icon grid management (remove/find/set-position),
 * URL-shortcut creation, hit-testing, and icon lookup. Uses g_dwm (extern in
 * dosgui_wm_internal.h). dosgui_icon_get is declared in dosgui_wm.h (public).
 * Minimal includes.
 */

#include "dosgui_wm_internal.h"

void dosgui_icon_remove(int grid_x, int grid_y) {
    for (int i = 0; i < g_dwm.icon_count; i++) {
        if (g_dwm.icons[i].alive && g_dwm.icons[i].grid_x == grid_x && g_dwm.icons[i].grid_y == grid_y) {
            g_dwm.icons[i].alive = false;
            /* Compact array */
            for (int j = i; j < g_dwm.icon_count - 1; j++) {
                g_dwm.icons[j] = g_dwm.icons[j + 1];
            }
            g_dwm.icon_count--;
            return;
        }
    }
}

int dosgui_icon_find_at(int grid_x, int grid_y) {
    for (int i = 0; i < g_dwm.icon_count; i++) {
        if (g_dwm.icons[i].alive && g_dwm.icons[i].grid_x == grid_x && g_dwm.icons[i].grid_y == grid_y) {
            return i;
        }
    }
    return -1;
}

void dosgui_icon_set_position(int grid_x, int grid_y, int new_gx, int new_gy) {
    int idx = dosgui_icon_find_at(grid_x, grid_y);
    if (idx >= 0) {
        DosGuiIcon *icon = &g_dwm.icons[idx];
        /* Check if target position is occupied */
        if (dosgui_icon_find_at(new_gx, new_gy) < 0) {
            icon->grid_x = new_gx;
            icon->grid_y = new_gy;
            icon->x = 20 + new_gx * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
            icon->y = 20 + new_gy * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
        }
    }
}

int dosgui_shortcut_create_url(const char *name, const char *url, int grid_x, int grid_y) {
    return dosgui_icon_add_ex(name, DESK_ICON_URL, url, grid_x, grid_y, 0xFF8000, NULL);
}

int dosgui_icon_hit_test(int mx, int my) {
    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *ic = &g_dwm.icons[i];
        if (mx >= ic->x && mx < ic->x + DOSGUI_ICON_SIZE &&
            my >= ic->y && my < ic->y + DOSGUI_ICON_SIZE)
            return i;
    }
    return -1;
}

DosGuiIcon *dosgui_icon_get(int idx) {
    if (idx < 0 || idx >= g_dwm.icon_count) return NULL;
    return &g_dwm.icons[idx];
}
