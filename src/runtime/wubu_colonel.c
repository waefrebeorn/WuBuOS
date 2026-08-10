/*
 * wubu_colonel.c -- the everything-through-the-Colonel dispatcher. C11.
 */
#include "wubu_colonel.h"
#include <string.h>
#include <ctype.h>

static const char *const g_apps[] = {
    "calc", "notepad", "paint", "explorer", "terminal",
    "holyc", "controlpanel", "taskmgr", "canvas", "freedoom",
    "bonzi", "comfy", "settings", "packagemanager", "containermanager",
    "sound", "music", "browser", "notes", "todo",
    "bpm", "hardware"
};
#define N_APPS (int)(sizeof(g_apps) / sizeof(g_apps[0]))

/* ==================================================================
 * App Registry (the Revolver Doctrine: hot-swappable app set)
 *
 * The built-in app names SEED the registry; new apps can be installed
 * at runtime (a new .wubu container or GUI app registers its name so
 * `colonel run <name>` and app_known() learn it without a recompile).
 * ================================================================== */

#define COLONEL_APP_REGISTRY_MAX 128   /* physical cylinder bound */
static char g_colonel_apps[COLONEL_APP_REGISTRY_MAX][64];
static int  g_colonel_apps_n = 0;
static int  g_colonel_apps_seeded = 0;

static void colonel_apps_seed(void)
{
    if (g_colonel_apps_seeded) return;
    for (int i = 0; i < N_APPS && i < COLONEL_APP_REGISTRY_MAX; i++) {
        snprintf(g_colonel_apps[g_colonel_apps_n], 63, "%s", g_apps[i]);
        g_colonel_apps_n++;
    }
    g_colonel_apps_seeded = 1;
}

int wubu_colonel_app_register(const char *name)
{
    if (!name || !name[0]) return -1;
    colonel_apps_seed();
    for (int i = 0; i < g_colonel_apps_n; i++)
        if (strcmp(g_colonel_apps[i], name) == 0)
            return 0;   /* already known (idempotent cartridge) */
    if (g_colonel_apps_n >= COLONEL_APP_REGISTRY_MAX) return -1;
    snprintf(g_colonel_apps[g_colonel_apps_n], 63, "%s", name);
    g_colonel_apps_n++;
    return 0;
}

int wubu_colonel_app_known(const char *name)
{
    if (!name || !name[0]) return 0;
    colonel_apps_seed();
    for (int i = 0; i < g_colonel_apps_n; i++)
        if (strcmp(g_colonel_apps[i], name) == 0) return 1;
    return 0;
}

static int skip_ws(const char **s)
{
    while (**s == ' ' || **s == '\t') (*s)++;
    return **s != 0;
}

static int word(const char **s, char *out, int cap)
{
    int n = 0;
    while (**s && **s != ' ' && **s != '\t' && n < cap - 1)
        out[n++] = *(*s)++;
    out[n] = 0;
    return n;
}

int wubu_colonel_parse(const char *line, wubu_colonel_t *c)
{
    if (!line || !c) return WUBU_COLONEL_BAD;
    memset(c, 0, sizeof(*c));
    const char *s = line;
    if (!skip_ws(&s)) return WUBU_COLONEL_EMPTY;

    if (strncmp(s, "run ", 4) == 0) {
        s += 4;
        c->class = WUBU_COL_CMD_APP;
        word(&s, c->cmd, sizeof(c->cmd));
        return WUBU_COLONEL_OK;
    }
    if (strncmp(s, "eval ", 5) == 0) {
        s += 5;
        c->class = WUBU_COL_CMD_EVAL;
        strncpy(c->arg, s, sizeof(c->arg) - 1);
        return WUBU_COLONEL_OK;
    }
    if (strncmp(s, "os ", 3) == 0) {
        s += 3;
        c->class = WUBU_COL_CMD_OS;
        word(&s, c->cmd, sizeof(c->cmd));
        return WUBU_COLONEL_OK;
    }
    if (strncmp(s, "sys ", 4) == 0) {
        s += 4;
        c->class = WUBU_COL_CMD_SYS;
        word(&s, c->cmd, sizeof(c->cmd));
        return WUBU_COLONEL_OK;
    }
    if (strncmp(s, "agi ", 4) == 0) {
        s += 4;
        c->class = WUBU_COL_CMD_AGI;
        word(&s, c->cmd, sizeof(c->cmd));
        return WUBU_COLONEL_OK;
    }
    if (strncmp(s, "load ", 5) == 0) {
        s += 5;
        c->class = WUBU_COL_CMD_LOAD;
        word(&s, c->cmd, sizeof(c->cmd));
        skip_ws(&s);
        strncpy(c->arg, s, sizeof(c->arg) - 1);
        return WUBU_COLONEL_OK;
    }
    /* a bare token defaults to the app class (the run shorthand) */
    c->class = WUBU_COL_CMD_APP;
    word(&s, c->cmd, sizeof(c->cmd));
    return WUBU_COLONEL_OK;
}

int wubu_colonel_dispatch(const char *line, wubu_colonel_t *c,
                          int64_t (*eval_fn)(const char *))
{
    if (!line || !c) return WUBU_COLONEL_BAD;
    int r = wubu_colonel_parse(line, c);
    if (r != WUBU_COLONEL_OK) return r;
    switch (c->class) {
    case WUBU_COL_CMD_EVAL:
        if (!eval_fn) return WUBU_COLONEL_BAD;
        c->value = eval_fn(c->arg);
        return WUBU_COLONEL_OK;
    case WUBU_COL_CMD_APP:
        /* the GUI layer launches the app; here we validate the name */
        return wubu_colonel_app_known(c->cmd) ? WUBU_COLONEL_OK
                                              : WUBU_COLONEL_UNKNOWN;
    case WUBU_COL_CMD_OS:
    case WUBU_COL_CMD_SYS:
    case WUBU_COL_CMD_AGI:
    case WUBU_COL_CMD_LOAD:
        /* the verb classes: routed (the callers dispatch the verb);
         * the parse already validated the shape */
        return c->cmd[0] ? WUBU_COLONEL_OK : WUBU_COLONEL_UNKNOWN;
    default:
        return WUBU_COLONEL_UNKNOWN;
    }
}
