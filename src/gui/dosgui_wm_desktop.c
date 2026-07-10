/* dosgui_wm_desktop.c -- Desktop icons subsystem for the WuBuOS WM.
 *
 * Self-contained module extracted from dosgui_wm.c. Owns the icon grid
 * snapping math, auto-arrange / sort / refresh of desktop icons, and real
 * ~/.desktop shortcut creation. Uses the shared g_dwm state and theme engine
 * via dosgui_wm_internal.h -- no god headers, no reach into wm.c internals.
 *
 * Public API (declared in dosgui_wm_internal.h):
 *   icon_grid_x / icon_grid_y / snap_icon_to_grid / reflow_all_icons_column /
 *   dosgui_wm_set_auto_arrange / dosgui_wm_get_auto_arrange /
 *   dosgui_wm_write_desktop_shortcut / dosgui_wm_sort_icons_by_name /
 *   dosgui_wm_refresh_desktop
 * Private helpers (static, used only here): reflow_icons_column, desktop_dir_path.
 */

#include "dosgui_wm_internal.h"
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <strings.h>   /* strcasecmp / strncasecmp */

int icon_grid_x(int x) {
    int grid_x = (x - 20) / (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    if (grid_x < 0) grid_x = 0;
    if (grid_x > 15) grid_x = 15;
    return 20 + grid_x * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
}

int icon_grid_y(int y) {
    int grid_y = (y - 20) / (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
    if (grid_y < 0) grid_y = 0;
    if (grid_y > 15) grid_y = 15;
    return 20 + grid_y * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
}

void snap_icon_to_grid(DosGuiIcon *icon) {
    icon->x = icon_grid_x(icon->x);
    icon->y = icon_grid_y(icon->y);
    icon->grid_x = (icon->x - 20) / (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    icon->grid_y = (icon->y - 20) / (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
}

/* -- Desktop view options (Stream 3) --------------------------------- */

void dosgui_wm_set_auto_arrange(bool on) {
    g_dwm.auto_arrange = on;
}

bool dosgui_wm_get_auto_arrange(void) {
    return g_dwm.auto_arrange;
}

/* Re-flow all live desktop icons into a single top-left column,
 * preserving their current order. Mirrors ReactOS desktop arrange. */
static void reflow_icons_column(void) {
    const int x0 = 20;
    const int y0 = 20;
    const int step = DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8;
    int y = y0;
    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *ic = &g_dwm.icons[i];
        if (!ic->alive) continue;
        ic->x = x0;
        ic->y = y;
        ic->grid_x = 0;
        ic->grid_y = (y - y0) / step;
        y += step;
    }
}

/* Public wrapper so other modules (context menu) can re-flow icons. */
void reflow_all_icons_column(void) {
    reflow_icons_column();
}

/* Resolve the user's Desktop directory (XDG or ~/Desktop). */
static void desktop_dir_path(char *out, size_t n) {
    const char *xdg = getenv("XDG_DESKTOP_DIR");
    if (xdg && *xdg) { snprintf(out, n, "%s", xdg); return; }
    const char *home = getenv("HOME");
    if (home && *home) { snprintf(out, n, "%s/Desktop", home); return; }
    snprintf(out, n, "/tmp/Desktop");
}

/* Write a real Freedesktop .desktop shortcut into ~/Desktop and return 0
 * on success. The shortcut is also surfaced as a live desktop icon. */
int dosgui_wm_write_desktop_shortcut(const char *name, const char *exec) {
    if (!name || !*name) return -1;
    char dir[512];
    desktop_dir_path(dir, sizeof(dir));
    struct stat st;
    if (stat(dir, &st) != 0) {
        if (mkdir(dir, 0755) != 0 && errno != EEXIST) return -1;
    }
    char fname[320];
    snprintf(fname, sizeof(fname), "%s/%.200s.desktop", dir, name);
    for (char *p = fname; *p; p++) if (*p == ' ') *p = '_';

    char exec_buf[512];
    if (exec && *exec) snprintf(exec_buf, sizeof(exec_buf), "%s", exec);
    else snprintf(exec_buf, sizeof(exec_buf), "wubu-app %s", name);

    FILE *f = fopen(fname, "w");
    if (!f) return -1;
    fprintf(f, "[Desktop Entry]\n");
    fprintf(f, "Type=Application\n");
    fprintf(f, "Version=1.0\n");
    fprintf(f, "Name=%s\n", name);
    fprintf(f, "Comment=WuBuOS desktop shortcut\n");
    fprintf(f, "Exec=%s\n", exec_buf);
    fprintf(f, "Terminal=false\n");
    fprintf(f, "Categories=WuBuOS;\n");
    fclose(f);

    /* Surface it immediately as a live desktop icon (auto-arranged column). */
    if (g_dwm.auto_arrange) {
        int gy = 0;
        for (int i = 0; i < g_dwm.icon_count; i++)
            if (g_dwm.icons[i].alive) gy++;
        dosgui_shortcut_create(name, fname, "WuBuOS desktop shortcut", 0, gy);
    }
    return 0;
}

/* Sort the live desktop icons alphabetically by name (case-insensitive),
 * preserving the alive flag, then re-flow them into the auto-arrange column.
 * Mirrors ReactOS "Arrange Icons By Name". */
void dosgui_wm_sort_icons_by_name(void) {
    /* Simple insertion sort over the live icon array. */
    for (int i = 1; i < g_dwm.icon_count; i++) {
        DosGuiIcon key = g_dwm.icons[i];
        int j = i - 1;
        while (j >= 0 &&
               strcasecmp(g_dwm.icons[j].name, key.name) > 0) {
            g_dwm.icons[j + 1] = g_dwm.icons[j];
            j--;
        }
        g_dwm.icons[j + 1] = key;
    }
    reflow_icons_column();
}

/* Real filesystem refresh: scan ~/Desktop for *.desktop files and add live
 * icons for any not already present (dedup by name). Mirrors Explorer refresh. */
void dosgui_wm_refresh_desktop(void) {
    char dir[512];
    desktop_dir_path(dir, sizeof(dir));
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    int gy = 0;
    for (int i = 0; i < g_dwm.icon_count; i++)
        if (g_dwm.icons[i].alive) gy++;
    while ((e = readdir(d)) != NULL) {
        size_t len = strlen(e->d_name);
        if (len < 8 || strcmp(e->d_name + len - 8, ".desktop") != 0) continue;
        /* Dedup by name (strip .desktop). */
        char base[256];
        snprintf(base, sizeof(base), "%.*s", (int)(len - 8), e->d_name);
        bool dup = false;
        for (int i = 0; i < g_dwm.icon_count; i++)
            if (g_dwm.icons[i].alive &&
                strncasecmp(g_dwm.icons[i].name, base, 255) == 0) { dup = true; break; }
        if (dup) continue;
        char path[640];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        if (g_dwm.icon_count < DOSGUI_MAX_ICONS) {
            dosgui_shortcut_create(base, path, "Desktop shortcut", 0, gy++);
        }
    }
    closedir(d);
}
