/*
 * dosgui_bpm_games.c -- Big Picture Mode's GAMES grid (the real
 * games, not just the era apps).
 *
 * The BPM grid is the era-apps registry; this adds the REAL game
 * library: the binaries in ~/.wubu/games/ (the three goals — the
 * Halo PC demo, the Halo Mac demo, OpenArena) probed by the game
 * launcher's classifier and launched through the kernel's personality
 * routing (wubu_game_run). The gamepad input (the d-pad + A) drives
 * the same selection/launch contract as the era grid.
 *
 * C11.
 */
#include "dosgui_bpm.h"
#include "wubu_game_launch.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
#include "../kernel/vbe.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#define GAMES_MAX  32
#define GAMES_DIR  "/home/wubu/.wubu/games"

typedef struct {
    char  path[512];
    char  name[128];
    WUBU_GAME_KIND kind;
} GameEntry;

typedef struct {
    GameEntry games[GAMES_MAX];
    int       count;
    int       selected;
} GamesState;

static GamesState g_games;

/* the launch hook (injectable — the tests record) */
static int (*g_launch)(const char *path, const char *name);

static int games_default_launch(const char *path, const char *name)
{
    (void)name;
    /* the kernel's personality routing, shell-free */
    int64_t rc = wubu_game_run(path);
    return rc >= 0 ? 0 : -1;
}

/* BG1: init. */
void dosgui_bpm_games_init(void)
{
    memset(&g_games, 0, sizeof(g_games));
    g_launch = games_default_launch;
    g_games.selected = 0;
}

/* BG2: the launch hook override (the tests). */
void dosgui_bpm_games_set_launch(int (*fn)(const char *, const char *))
{
    g_launch = fn ? fn : games_default_launch;
}

/* BG3: scan the real games from ~/.wubu/games/. */
int dosgui_bpm_games_scan(void)
{
    memset(&g_games, 0, sizeof(g_games));
    mkdir("/home/wubu/.wubu", 0755);
    mkdir(GAMES_DIR, 0755);
    DIR *d = opendir(GAMES_DIR);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && g_games.count < GAMES_MAX) {
        if (e->d_name[0] == '.') continue;
        /* probe the magic to classify + accept only the hostable */
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", GAMES_DIR, e->d_name);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        unsigned char magic[4];
        size_t rd = fread(magic, 1, 4, f);
        fclose(f);
        if (rd != 4) continue;
        WUBU_GAME_KIND kind = wubu_game_classify(magic, 4);
        if (kind == WUBU_GAME_UNKNOWN) continue;   /* not a hostable game */
        GameEntry *g = &g_games.games[g_games.count];
        snprintf(g->path, sizeof(g->path), "%s", path);
        snprintf(g->name, sizeof(g->name), "%s", e->d_name);
        g->kind = kind;
        g_games.count++;
    }
    closedir(d);
    return g_games.count;
}

/* BG4: the game count + the accessors. */
int dosgui_bpm_games_count(void) { return g_games.count; }
const char *dosgui_bpm_games_name(int idx) { return g_games.games[idx].name; }
const char *dosgui_bpm_games_path(int idx) { return g_games.games[idx].path; }
WUBU_GAME_KIND dosgui_bpm_games_kind(int idx) { return g_games.games[idx].kind; }
int dosgui_bpm_games_selected(void) { return g_games.selected; }

/* BG5: launch the selected game through the kernel routing. */
int dosgui_bpm_games_launch_selected(void)
{
    if (g_games.count == 0) return -1;
    GameEntry *g = &g_games.games[g_games.selected];
    if (g_launch)
        return g_launch(g->path, g->name);
    return -1;
}

/* BG6: the gamepad navigation (shared with the era grid). */
void dosgui_bpm_games_select(int idx)
{
    if (idx >= 0 && idx < g_games.count) g_games.selected = idx;
}
void dosgui_bpm_games_move(int delta)
{
    if (g_games.count == 0) return;
    g_games.selected = (g_games.selected + delta + g_games.count) % g_games.count;
}

/* BG7: the test hooks */
typedef struct {
    int count;
    int selected;
    int launched;
} dosgui_bpm_games_view_t;

int dosgui_bpm_games_get(dosgui_bpm_games_view_t *out)
{
    if (!out) return -1;
    out->count = g_games.count;
    out->selected = g_games.selected;
    out->launched = 0;
    return 0;
}
