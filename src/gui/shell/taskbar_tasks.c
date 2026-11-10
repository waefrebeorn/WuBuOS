/*
 * taskbar_tasks.c -- the taskbar model.  See the header.
 *
 * Deliberately free of GUI concerns: no VBE, no WM.  The taskbar renderer asks
 * this module what to draw; this module never asks the renderer anything.  That
 * keeps it testable headless and lets the same model back a jump list later.
 *
 * The file format is one task per line:
 *
 *   pin <key>|<title>|<exec>
 *
 * `key` is the app identity and must not contain a `|`.  A leading '#' is a
 * comment, so a hand-edited file stays readable -- this is a config file a user
 * is expected to be able to look at.
 */
#include "taskbar_tasks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static WubuTask g_tasks[WUBU_TASKBAR_MAX];
static int      g_count;
static char     g_path[512];
static bool     g_dirty;

/* ------------------------------------------------------------- helpers */

static const char *home_dir(void) {
    const char *h = getenv("HOME");
    return (h && *h) ? h : "/tmp";
}

const char *wubu_taskbar_config_path(void) {
    if (!g_path[0])
        snprintf(g_path, sizeof g_path, "%s/.config/wubuos/taskbar",
                 home_dir());
    return g_path;
}

static WubuTask *find_slot(const char *key) {
    if (!key || !*key) return NULL;
    for (int i = 0; i < g_count; i++)
        if (strcmp(g_tasks[i].key, key) == 0) return &g_tasks[i];
    return NULL;
}

static WubuTask *alloc_slot(const char *key) {
    WubuTask *t = find_slot(key);
    if (t) return t;
    if (g_count >= WUBU_TASKBAR_MAX) return NULL;
    t = &g_tasks[g_count++];
    memset(t, 0, sizeof *t);
    snprintf(t->key, sizeof t->key, "%s", key);
    t->win_id = -1;
    return t;
}

/* ---------------------------------------------------------- lifecycle */

void wubu_taskbar_init(void) {
    wubu_taskbar_load();
}

void wubu_taskbar_shutdown(void) {
    wubu_taskbar_save();
}

/* -------------------------------------------------------------- pins */

int wubu_taskbar_pin(const char *key, const char *title, const char *exec) {
    WubuTask *t = alloc_slot(key);
    if (!t) return -1;
    if (title && *title) snprintf(t->title, sizeof t->title, "%s", title);
    if (exec  && *exec)  snprintf(t->exec,  sizeof t->exec,  "%s", exec);
    if (!t->pinned) { t->pinned = true; g_dirty = true; }
    return 0;
}

int wubu_taskbar_unpin(const char *key) {
    WubuTask *t = find_slot(key);
    if (!t || !t->pinned) return -1;
    t->pinned = false;
    g_dirty = true;
    /* A pin is the only reason to keep a task with no running window. */
    if (t->win_id < 0) {
        int idx = (int)(t - g_tasks);
        for (int i = idx + 1; i < g_count; i++) g_tasks[i - 1] = g_tasks[i];
        g_count--;
    }
    return 0;
}

bool wubu_taskbar_is_pinned(const char *key) {
    const WubuTask *t = find_slot(key);
    return t && t->pinned;
}

/* -------------------------------------------------------- running state */

void wubu_taskbar_set_running(const char *key, int win_id, const char *title) {
    WubuTask *t = alloc_slot(key);
    if (!t) return;
    t->win_id = win_id;
    if (title && *title) snprintf(t->title, sizeof t->title, "%s", title);
}

void wubu_taskbar_clear_running(int win_id) {
    for (int i = 0; i < g_count; i++) {
        if (g_tasks[i].win_id != win_id) continue;
        g_tasks[i].win_id = -1;
        /* Drop the task entirely if nothing else keeps it alive. */
        if (!g_tasks[i].pinned) {
            for (int j = i + 1; j < g_count; j++) g_tasks[j - 1] = g_tasks[j];
            g_count--;
            i--;
        }
        return;
    }
}

/* ------------------------------------------------------------ queries */

int wubu_taskbar_count(void) { return g_count; }

const WubuTask *wubu_taskbar_get(int i) {
    if (i < 0 || i >= g_count) return NULL;
    return &g_tasks[i];
}

const WubuTask *wubu_taskbar_find(const char *key) { return find_slot(key); }

int wubu_taskbar_running_count(void) {
    int n = 0;
    for (int i = 0; i < g_count; i++) if (g_tasks[i].win_id >= 0) n++;
    return n;
}

bool wubu_taskbar_is_running(int win_id) {
    for (int i = 0; i < g_count; i++)
        if (g_tasks[i].win_id == win_id) return true;
    return false;
}

/* --------------------------------------------------------- persistence */

void wubu_taskbar_save(void) {
    if (!g_dirty) return;

    /* Write atomically: a truncated config would lose the user's pins. */
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s.tmp", wubu_taskbar_config_path());

    /* ensure the parent directory exists */
    char dir[400];
    snprintf(dir, sizeof dir, "%s", wubu_taskbar_config_path());
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        /* mkdir -p, one level at a time (the depth is fixed: HOME/.config/wubuos) */
        for (char *p = dir + 1; *p; p++) {
            if (*p == '/') { *p = '\0'; mkdir(dir, 0755); *p = '/'; }
        }
        mkdir(dir, 0755);
    }

    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "# WuBuOS taskbar pins.  One per line: pin <key>|<title>|<exec>\n");
    fprintf(f, "# Edit by hand, or use the taskbar's Pin command.\n");
    for (int i = 0; i < g_count; i++) {
        if (!g_tasks[i].pinned) continue;
        fprintf(f, "pin %s|%s|%s\n", g_tasks[i].key,
                g_tasks[i].title, g_tasks[i].exec);
    }
    fclose(f);
    if (rename(tmp, wubu_taskbar_config_path()) == 0) g_dirty = false;
}

void wubu_taskbar_load(void) {
    g_count = 0;
    g_dirty = false;
    for (int i = 0; i < WUBU_TASKBAR_MAX; i++) g_tasks[i].win_id = -1;

    FILE *f = fopen(wubu_taskbar_config_path(), "r");
    if (!f) return;   /* no pins yet is a normal first boot */

    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        if (strncmp(p, "pin ", 4) != 0) continue;

        char *body = p + 4;
        char *nl = strchr(body, '\n');
        if (nl) *nl = '\0';

        char *b1 = strchr(body, '|');
        if (!b1) continue;
        *b1 = '\0';
        char *b2 = strchr(b1 + 1, '|');
        if (!b2) continue;
        *b2 = '\0';

        wubu_taskbar_pin(body, b1 + 1, b2 + 1);
    }
    fclose(f);
    g_dirty = false;   /* loading is not a change */
}
