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
#include <stdlib.h>     /* qsort */
#include <fcntl.h>      /* O_CREAT / O_EXCL / O_WRONLY */
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

/* Forward decl: defined just below; called by set_auto_arrange + boot path. */
static void reflow_icons_column(void);

void dosgui_wm_set_auto_arrange(bool on) {
    g_dwm.auto_arrange = on;
    /* Persist the toggle (ReactOS NTUSER auto-arrange setting) so it survives
     * a restart. */
    WubuSettings *s = wubu_settings_mut();
    if (s) {
        s->theme.auto_arrange = on;
        wubu_settings_save();
    }
    /* Apply immediately: when turning ON, re-flow icons into the column. */
    if (on) reflow_icons_column();
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

/* Live show/hide of all desktop icons (Control Panel "Show desktop icons"). */
void dosgui_wm_set_icons_visible(bool show) {
    if (!dosgui_wm_is_initialized()) return;
    if (show) {
        dosgui_wm_refresh_desktop();
    } else {
        for (int i = 0; i < g_dwm.icon_count; i++) g_dwm.icons[i].alive = false;
        g_dwm.icon_count = 0;
    }
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

/* Real filesystem refresh: enumerate ~/Desktop namespace (folders, .desktop
 * shortcuts, and regular files) and add live icons for any not already
 * present (dedup by name). Mirrors ReactOS explorer/desktop.cpp, which shows
 * folders and files on the desktop, not only .lnk/.desktop shortcuts. */
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
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char path[640];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);

        /* Classify entry: directory -> folder icon, .desktop -> shortcut,
         * anything else -> file icon. */
        DeskIconType type = DESK_ICON_FILE;
        bool is_desktop = false;
        size_t len = strlen(e->d_name);
        if (len >= 8 && strcmp(e->d_name + len - 8, ".desktop") == 0) {
            type = DESK_ICON_SHORTCUT;
            is_desktop = true;
        } else {
            struct stat st;
            if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) type = DESK_ICON_FOLDER;
        }

        /* Display name: strip .desktop extension, keep full name otherwise. */
        char disp[256];
        if (is_desktop) snprintf(disp, sizeof(disp), "%.*s", (int)(len - 8), e->d_name);
        else snprintf(disp, sizeof(disp), "%s", e->d_name);

        /* Dedup by display name. */
        bool dup = false;
        for (int i = 0; i < g_dwm.icon_count; i++)
            if (g_dwm.icons[i].alive &&
                strncasecmp(g_dwm.icons[i].name, disp, 255) == 0) { dup = true; break; }
        if (dup) continue;

        if (g_dwm.icon_count < DOSGUI_MAX_ICONS) {
            if (type == DESK_ICON_FOLDER)
                dosgui_icon_add_ex(disp, DESK_ICON_FOLDER, path, 0, gy++, 0x00AA00, NULL);
            else if (type == DESK_ICON_SHORTCUT)
                dosgui_shortcut_create(disp, path, "Desktop shortcut", 0, gy++);
            else
                dosgui_icon_add_ex(disp, DESK_ICON_FILE, path, 0, gy++, 0x00CCCC, NULL);
        }
    }
    closedir(d);
}

/* Create a real folder in ~/Desktop (ReactOS shell32/CDesktopFolder lesson),
 * then re-enumerate + re-flow so it appears as a live desktop icon. */
int dosgui_wm_new_folder(void) {
    char dir[512];
    desktop_dir_path(dir, sizeof(dir));
    struct stat st;
    if (stat(dir, &st) != 0) {
        if (mkdir(dir, 0755) != 0 && errno != EEXIST) return -1;
    }
    char name[256] = "New Folder";
    char path[768];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    int counter = 1;
    while (mkdir(path, 0755) != 0) {
        if (errno != EEXIST) return -1;
        snprintf(name, sizeof(name), "New Folder (%d)", counter++);
        snprintf(path, sizeof(path), "%s/%s", dir, name);
    }
    dosgui_wm_refresh_desktop();
    reflow_all_icons_column();
    return 0;
}

/* Create a real empty text document in ~/Desktop, then re-enumerate + re-flow. */
int dosgui_wm_new_text_doc(void) {
    char dir[512];
    desktop_dir_path(dir, sizeof(dir));
    struct stat st;
    if (stat(dir, &st) != 0) {
        if (mkdir(dir, 0755) != 0 && errno != EEXIST) return -1;
    }
    char name[256] = "New Text Document.txt";
    char path[768];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    int counter = 1;
    while (1) {
        int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0644);
        if (fd >= 0) { close(fd); break; }
        if (errno != EEXIST) return -1;
        snprintf(name, sizeof(name), "New Text Document (%d).txt", counter++);
        snprintf(path, sizeof(path), "%s/%s", dir, name);
    }
    dosgui_wm_refresh_desktop();
    reflow_all_icons_column();
    return 0;
}

/* Desktop icon sort modes (ReactOS "Arrange Icons By"). Resolves live
 * metadata via stat() on each icon's target path. */
static DosGuiSortMode g_sort_mode = DOSGUI_SORT_NAME;

static int icon_cmp(const void *a, const void *b) {
    const DosGuiIcon *ia = (const DosGuiIcon *)a;
    const DosGuiIcon *ib = (const DosGuiIcon *)b;
    switch (g_sort_mode) {
    case DOSGUI_SORT_SIZE: {
        struct stat sa, sb;
        off_t sa_size = ia->target[0] && stat(ia->target, &sa) == 0 ? sa.st_size : 0;
        off_t sb_size = ib->target[0] && stat(ib->target, &sb) == 0 ? sb.st_size : 0;
        /* Larger items first. */
        return (sb_size > sa_size) - (sb_size < sa_size);
    }
    case DOSGUI_SORT_TYPE: {
        int ta = ia->type, tb = ib->type;
        if (ta != tb) return ta - tb;
        return strcasecmp(ia->name, ib->name);
    }
    case DOSGUI_SORT_DATE: {
        struct stat sa, sb;
        time_t ta_m = ia->target[0] && stat(ia->target, &sa) == 0 ? sa.st_mtime : 0;
        time_t tb_m = ib->target[0] && stat(ib->target, &sb) == 0 ? sb.st_mtime : 0;
        if (ta_m != tb_m) return (ta_m > tb_m) - (ta_m < tb_m);
        return strcasecmp(ia->name, ib->name);
    }
    case DOSGUI_SORT_NAME:
    default:
        return strcasecmp(ia->name, ib->name);
    }
}

void dosgui_wm_sort_icons(DosGuiSortMode mode) {
    g_sort_mode = mode;
    /* Compact live icons into a temp array, sort, write back. */
    DosGuiIcon tmp[DOSGUI_MAX_ICONS];
    int n = 0;
    for (int i = 0; i < g_dwm.icon_count; i++)
        if (g_dwm.icons[i].alive) tmp[n++] = g_dwm.icons[i];
    if (n > 1) qsort(tmp, n, sizeof(DosGuiIcon), icon_cmp);
    /* Write sorted live icons back, preserving slots for dead ones. */
    int w = 0;
    for (int i = 0; i < g_dwm.icon_count && w < n; i++) {
        if (g_dwm.icons[i].alive) g_dwm.icons[i] = tmp[w++];
    }
    reflow_icons_column();
}

