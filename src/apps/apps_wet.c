/*
 * apps_wet.c -- discovering programs that are files.
 *
 * A program is a FILE: the applet directory is the only place a new program
 * can come from, so this is the single owner of that directory and its scan.
 * It lives apart from apps.c because apps.c also owns the compiled-in registry
 * (g_app_defs), the launchers, and the terminal/notepad implementations -- and
 * the start menu needs to ask "what applets exist?" without linking all of that.
 *
 * Nothing here touches the GUI: it fills plain DosGuiAppDef records.  The
 * launcher is a weak symbol, so a link without the applet host still compiles
 * and simply has no runtime behind a discovered entry.
 */
/* strdup/opendir/readdir need POSIX; without this a link that does not
 * define _POSIX_C_SOURCE (e.g. the start menu test) gets an implicit
 * declaration, and the returned pointer is truncated to 32 bits. */
#define _POSIX_C_SOURCE 200809L

#include "../egojar.h"
#include "apps.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The applet host.  Weak, for the same reason as in apps.c: a minimal link
 * (the start menu's own test) has no applet runtime. */
__attribute__((weak)) DosGuiWindow *launch_wet_applet(const char *path);

#define APP_WET_MAX 32
#define DESK_ICON_WET_BASE (DESK_ICON_COUNT + 64)

static DosGuiAppDef g_wet_apps[APP_WET_MAX];
static int         g_wet_count;

int app_wet_count(void) { return g_wet_count; }

/* Where programs live.  A program is a file: the directory is the only place
 * a new one can come from. */
void app_wet_dir(char *out, size_t cap) {
    if (!out || cap == 0) return;
    const char *env = getenv("WUBU_APPLET_DIR");
    if (env && env[0]) { snprintf(out, cap, "%s", env); return; }
    const char *home = getenv("HOME");
    if (home && home[0]) { snprintf(out, cap, "%s/.wubu/applets", home); return; }
    snprintf(out, cap, "/tmp/.wubu/applets");
}

const DosGuiAppDef *app_wet_at(int i) {
    if (i < 0 || i >= g_wet_count) return NULL;
    return &g_wet_apps[i];
}

/* The generic launcher for a discovered applet.  DosGuiAppDef.launch takes no
 * arguments, so the path rides in a file-static that app_launch sets first --
 * the only caller, and it is not re-entrant (a launch does not scan). */
static const char *g_launch_path;

static DosGuiWindow *launch_wet_entry(void) {
    return launch_wet_applet(g_launch_path);
}

static bool has_wet_ext(const char *name) {
    size_t n = strlen(name);
    return n > 4 && strcmp(name + n - 4, ".wet") == 0;
}

/*
 * Scan a directory for .wet scripts and register each as a desktop program.
 * Idempotent for a given path (a second scan of the same dir is a no-op), so
 * a caller can refresh on every boot without duplicating entries.
 */
int app_scan_wet_dir(const char *dir) {
    if (!dir || !dir[0]) return -1;
    DIR *d = opendir(dir);
    if (!d) return -1;

    int added = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (!has_wet_ext(ent->d_name)) continue;

        char full[1024];
        snprintf(full, sizeof full, "%s/%s", dir, ent->d_name);

        /* Idempotence: skip anything already registered. */
        bool seen = false;
        for (int i = 0; i < g_wet_count; i++)
            if (strcmp(g_wet_apps[i].path, full) == 0) { seen = true; break; }
        if (seen) continue;
        if (g_wet_count >= APP_WET_MAX) break;

        /* The registry key is the file name without its extension. */
        char base[256];
        snprintf(base, sizeof base, "%s", ent->d_name);
        base[strlen(base) - 4] = '\0';

        DosGuiAppDef *slot = &g_wet_apps[g_wet_count];
        slot->name       = strdup(base);
        slot->title      = slot->name;
        slot->icon_type  = DESK_ICON_WET_BASE + g_wet_count;
        slot->icon_color = 0x00C0A0E0;
        slot->launch     = NULL;      /* the generic path launcher is used */
        slot->path       = strdup(full);
        g_wet_count++;
        added++;
    }
    closedir(d);
    return added;
}

/* -- Lookups over the discovered set ------------------------------- */
/*
 * apps.c's app_find / app_find_by_name delegate here rather than walking the
 * array themselves: one owner per concern, and the start menu can resolve a
 * discovered applet without linking the compiled-in registry at all.
 */

/* Launch a discovered applet by path.  The path launcher itself lives here
 * (with g_launch_path), so apps.c does not reach into a file-static. */
DosGuiWindow *app_launch_wet_path(const char *path) {
    if (!path || !path[0]) return NULL;
    g_launch_path = path;
    return launch_wet_entry();
}

const DosGuiAppDef *app_find_wet(int icon_type) {
    for (int i = 0; i < g_wet_count; i++)
        if (g_wet_apps[i].icon_type == icon_type) return &g_wet_apps[i];
    return NULL;
}

const DosGuiAppDef *app_find_wet_by_name(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_wet_count; i++)
        if (g_wet_apps[i].name && strcmp(g_wet_apps[i].name, name) == 0)
            return &g_wet_apps[i];
    return NULL;
}
