/* desktop_icons.c -- Desktop icons subsystem for the WuBuOS WM.
 *
 * Self-contained module extracted from wm.c. Owns the icon grid
 * snapping math, auto-arrange / sort / refresh of desktop icons, and real
 * ~/.desktop shortcut creation. Uses the shared g_dwm state and theme engine
 * via ../wm_internal.h -- no god headers, no reach into wm.c internals.
 *
 * Public API (declared in ../wm_internal.h):
 *   icon_grid_x / icon_grid_y / snap_icon_to_grid / reflow_all_icons_column /
 *   wm_set_auto_arrange / wm_get_auto_arrange /
 *   wm_write_desktop_shortcut / wm_sort_icons_by_name /
 *   wm_refresh_desktop
 * Private helpers (static, used only here): reflow_icons_column, desktop_dir_path.
 */

#include "../../egojar.h"
#include "../wm_internal.h"
#include "../../apps/apps.h"
#include "../../runtime/wubu_guest_root.h"
#include <stdlib.h>     /* qsort */
#include <fcntl.h>      /* O_CREAT / O_EXCL / O_WRONLY */
#include <errno.h>
#include <dirent.h>
#include <strings.h>   /* strcasecmp / strncasecmp */

/* -- Cell size derived from the TEXT, not a constant ------------------------------------------------
 * The pitch was DOSGUI_ICON_TILE (84 px: icon + gap + 44 px of label). At a 2x accessibility scale the
 * label alone is ~76 px, so labels ran into the next cell, and reflow_icons_column() stacked every icon in
 * ONE column with no bound -- the 9th default icon landed under the taskbar and was clipped even at 1x in a
 * short window. Both cell dimensions now follow the renderer's line height, and the reflow wraps columns. */
extern int vbe_line_height(void) __attribute__((weak));
static int desk_line_h(void) { return vbe_line_height ? vbe_line_height() : 19; }

int desktop_cell_h(void) {
    int h = DOSGUI_ICON_SIZE + 8 + 2 * desk_line_h() + 6;   /* icon + gap + two label lines */
    return h > DOSGUI_ICON_TILE ? h : DOSGUI_ICON_TILE;
}
int desktop_cell_w(void) {
    /* labels grow with the line height, and Win11 cells are wide enough for the longest name */
    int w = DOSGUI_ICON_TILE + (desk_line_h() - 19) * 4;
    return w > DOSGUI_ICON_TILE ? w : DOSGUI_ICON_TILE;
}

int icon_grid_x(int x) {
    const int pitch = desktop_cell_w();
    int gx = (x - DOSGUI_ICON_MARGIN) / pitch;
    int maxc = (g_dwm.screen_w - DOSGUI_ICON_MARGIN) / pitch;
    if (gx < 0) gx = 0;
    if (gx > maxc) gx = maxc;
    return DOSGUI_ICON_MARGIN + gx * pitch;
}

int icon_grid_y(int y) {
    const int pitch = desktop_cell_h();
    int gy = (y - DOSGUI_ICON_MARGIN) / pitch;
    int maxr = (taskbar_top() - DOSGUI_ICON_MARGIN) / pitch;
    if (gy < 0) gy = 0;
    if (gy > maxr) gy = maxr;
    return DOSGUI_ICON_MARGIN + gy * pitch;
}

void snap_icon_to_grid(DosGuiIcon *icon) {
    icon->x = icon_grid_x(icon->x);
    icon->y = icon_grid_y(icon->y);
    icon->grid_x = (icon->x - DOSGUI_ICON_MARGIN) / desktop_cell_w();
    icon->grid_y = (icon->y - DOSGUI_ICON_MARGIN) / desktop_cell_h();
}

/* -- Desktop view options (Stream 3) --------------------------------- */

/* Forward decl: defined just below; called by set_auto_arrange + boot path. */
static void reflow_icons_column(void);

void wm_set_auto_arrange(bool on) {
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

bool wm_get_auto_arrange(void) {
    return g_dwm.auto_arrange;
}

/* Re-flow all live desktop icons into a single top-left column,
 * preserving their current order. Mirrors ReactOS desktop arrange. */
static void reflow_icons_column(void) {
    const int x0 = DOSGUI_ICON_MARGIN;
    const int y0 = DOSGUI_ICON_MARGIN;
    const int ch = desktop_cell_h();
    const int cw = desktop_cell_w();
    /* How many cells the desktop can actually show. The old loop had no bound at all, so any icon past the
     * last visible row was drawn under the taskbar (or off the window) and simply disappeared. */
    int rows = (taskbar_top() - y0) / ch;
    if (rows < 1) rows = 1;
    int slot = 0;
    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *ic = &g_dwm.icons[i];
        if (!ic->alive) continue;
        int gy = slot % rows;
        int gx = slot / rows;
        ic->grid_x = gx;
        ic->grid_y = gy;
        ic->x = x0 + gx * cw;
        ic->y = y0 + gy * ch;
        slot++;
    }
}

/* Public wrapper so other modules (context menu) can re-flow icons. */
void reflow_all_icons_column(void) {
    reflow_icons_column();
}

/* Live show/hide of all desktop icons (Control Panel "Show desktop icons"). */
void wm_set_icons_visible(bool show) {
    if (!wm_is_initialized()) return;
    if (show) {
        wm_refresh_desktop();
    } else {
        for (int i = 0; i < g_dwm.icon_count; i++) g_dwm.icons[i].alive = false;
        g_dwm.icon_count = 0;
    }
}

/* Resolve the user's Desktop directory.
 *
 * This used to prefer $XDG_DESKTOP_DIR and then fall back to $HOME/Desktop -- two
 * host-OS conventions.  On the hosted port that meant the desktop showed whatever
 * the LINUX session had on its desktop, and creating a folder or shortcut wrote a
 * file into the host's home directory instead of the guest's own namespace.  The
 * Desktop is now a directory inside the GUEST sandbox ($WUBU_GUEST_ROOT/Desktop),
 * with WUBU_DESKTOP_DIR as an explicit operator override for anyone who wants it
 * elsewhere. */
static void desktop_dir_path(char *out, size_t n) {
    const char *env = getenv("WUBU_DESKTOP_DIR");
    if (env && *env) { snprintf(out, n, "%s", env); return; }

    char root[1024];
    wubu_guest_root(root, sizeof(root));
    snprintf(out, n, "%s/Desktop", root);
}

/* Write a real Freedesktop .desktop shortcut into ~/Desktop and return 0
 * on success. The shortcut is also surfaced as a live desktop icon. */
int wm_write_desktop_shortcut(const char *name, const char *exec) {
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
        shortcut_create(name, fname, "WuBuOS desktop shortcut", 0, gy);
    }
    return 0;
}

/* Seed the desktop with useful default icons on FIRST BOOT. A fresh desktop --
 * no .desktop files yet -- currently boots empty, which reads as "no desktop /
 * no icons". Plasma/SteamOS populate a new user's desktop with defaults; WuBuOS
 * does the same: Calculator, File Manager, Editor, Terminal. Never clobbers a
 * desktop that already has any shortcut (a user's curated desktop is theirs).
 * Returns the number of shortcuts seeded (0 if already populated or nothing
 * could be done). */
int wm_seed_default_desktop_icons(void) {
    char dir[512];
    desktop_dir_path(dir, sizeof(dir));
    struct stat st;
    if (stat(dir, &st) != 0) {
        if (mkdir(dir, 0755) != 0 && errno != EEXIST) return 0;
    }
    /* Only seed when the desktop has NO shortcuts at all. */
    DIR *d = opendir(dir);
    if (!d) return 0;
    int has_shortcut = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n > 8 && strcmp(e->d_name + n - 8, ".desktop") == 0) { has_shortcut = 1; break; }
    }
    closedir(d);
    if (has_shortcut) return 0;

    static const char *const defs[][2] = {
        { "Calculator",    "wubu-app Calculator" },
        { "File Manager",  "wubu-app File Manager" },
        { "Editor",        "wubu-app Editor" },
        { "Terminal",      "wubu-app Terminal" },
    };
    int seeded = 0;
    for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); i++)
        if (wm_write_desktop_shortcut(defs[i][0], defs[i][1]) == 0)
            seeded++;
    return seeded;
}

/* Sort the live desktop icons alphabetically by name (case-insensitive),
 * preserving the alive flag, then re-flow them into the auto-arrange column.
 * Mirrors ReactOS "Arrange Icons By Name". */
void wm_sort_icons_by_name(void) {
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
void wm_refresh_desktop(void) {
    char dir[512];
    desktop_dir_path(dir, sizeof(dir));

    /* Make sure the Desktop exists before enumerating it.  It lives inside the
     * guest sandbox, which is empty on a fresh install -- without this the shell
     * would silently show no desktop icons at all until something else happened to
     * create the directory, which reads as "the desktop is broken" on first boot. */
    struct stat st;
    if (stat(dir, &st) != 0) {
        if (mkdir(dir, 0755) != 0 && errno != EEXIST) return;
    }

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
                icon_add_ex(disp, DESK_ICON_FOLDER, path, 0, gy++, 0x00AA00, NULL);
            else if (type == DESK_ICON_SHORTCUT)
                shortcut_create(disp, path, "Desktop shortcut", 0, gy++);
            else
                icon_add_ex(disp, DESK_ICON_FILE, path, 0, gy++, 0x00CCCC, NULL);
        }
    }
    closedir(d);
}

/* Create a real folder in ~/Desktop (ReactOS shell32/CDesktopFolder lesson),
 * then re-enumerate + re-flow so it appears as a live desktop icon. */
int wm_new_folder(void) {
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
    wm_refresh_desktop();
    reflow_all_icons_column();
    return 0;
}

/* Create a real empty text document in ~/Desktop, then re-enumerate + re-flow. */
int wm_new_text_doc(void) {
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
    wm_refresh_desktop();
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

void wm_sort_icons(DosGuiSortMode mode) {
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

/* Exec resolution lives with the .desktop owner: this is the only place that knows what
 * an Exec line WE wrote means. The rule is pure and takes a matcher, so it can be
 * asserted without linking an app registry or a window system; the product passes the
 * registry in. */
static void strip_field_codes_impl(const char *in, char *out, size_t cap) {
    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!in) return;
    size_t o = 0;
    for (const char *p = in; *p && o + 1 < cap; p++) {
        if (*p == '%' && p[1]) {
            char f = p[1];
            if (f == 'f' || f == 'F' || f == 'u' || f == 'U' || f == 'i' ||
                f == 'c' || f == 'k') { p++; continue; }
            if (f == '%') { out[o++] = '%'; p++; continue; }
        }
        out[o++] = *p;
    }
    out[o] = '\0';
}

void strip_field_codes(const char *in, char *out, size_t cap) {
    strip_field_codes_impl(in, out, cap);
}

/* "wubu-app " is the launcher token our seeder writes and our pins store. Nothing ever
 * implemented a wubu-app PROGRAM, so those Exec lines resolved to a shell lookup of a
 * missing binary: the shortcut did nothing when clicked. */
#define WUBU_APP_TOKEN "wubu-app "

int wubu_exec_app_name(const char *exec,
                       int (*is_app)(const char *name, void *ctx), void *ctx,
                       char *out, size_t cap) {
    if (!is_app || !out || cap == 0) return -1;
    out[0] = '\0';
    if (!exec || !*exec) return -1;

    char cmd[512];
    strip_field_codes_impl(exec, cmd, sizeof cmd);
    char *lead = cmd;
    while (*lead == ' ') lead++;
    if (!*lead) return -1;

    const size_t tok = sizeof(WUBU_APP_TOKEN) - 1;
    if (strncmp(lead, WUBU_APP_TOKEN, tok) == 0) {
        char name[512];
        snprintf(name, sizeof name, "%s", lead + tok);
        char *n = name;
        while (*n == ' ') n++;
        /* Strip a trailing space left behind by removing a field code: "Calculator %U"
         * strips to "Calculator " and a matcher comparing whole names would miss it. */
        for (char *q = n + strlen(n); q > n && q[-1] == ' '; ) *--q = '\0';
        if (!*n || !is_app(n, ctx)) return -1;
        snprintf(out, cap, "%s", n);
        return 0;
    }

    char first[512];
    snprintf(first, sizeof first, "%s", lead);
    char *sp = strchr(first, ' ');
    if (sp) *sp = '\0';
    if (!first[0] || !is_app(first, ctx)) return -1;
    snprintf(out, cap, "%s", first);
    return 0;
}

/* WEAK: a .desktop test must be able to assert the RULE without linking the app
 * registry; the product links it and this resolves normally. */
__attribute__((weak)) const DosGuiAppDef *app_find_by_name(const char *name);

static int registry_has_app(const char *name, void *ctx) {
    (void)ctx;
    return app_find_by_name && app_find_by_name(name) != NULL;
}

int desktop_exec_app_name(const char *exec, char *out, size_t cap) {
    return wubu_exec_app_name(exec, registry_has_app, NULL, out, cap);
}
