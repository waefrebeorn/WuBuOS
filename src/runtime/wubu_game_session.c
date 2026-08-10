/*
 * wubu_game_session.c -- the GAME-PLAY TRAINING LEDGER (the AGI
 * learns via running games).
 *
 * The AGI doctrine: every user action is live training data. A game
 * session is a first-class training event — the ledger records:
 *
 *   - the game (name + format kind)
 *   - the launch timestamp + the duration
 *   - the WORLD STATE at launch (the hardware snapshot the game ran
 *     on: nvme/gpu/wifi/battery/heat) — the implicit-feedback stream
 *     the AGI consumes (the research/065 usage-ledger doctrine)
 *
 * The ledger appends one line per play to
 * ~/.wubu/games/plays.log (the 9P filesystem — real, persistent):
 *
 *   play|game|format|t0|dur_s|world
 *
 * The AGI reads the deltas between plays: which games run on which
 * hardware, how the world state shifts during play (heat rises, the
 * battery drains) — training data from the running OS.
 *
 * C11, minimal includes, shell-free.
 */
#include "wubu_game_session.h"
#include "wubu_world.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define PLAYS_LOG "/home/wubu/.wubu/games/plays.log"

/* the in-progress play (the start of the current session) */
static char g_cur_game[128];
static WUBU_GAME_KIND g_cur_kind = WUBU_GAME_UNKNOWN;
static int64_t g_cur_t0 = 0;
static int g_cur_active = 0;

static void ensure_dir(void)
{
    mkdir("/home/wubu/.wubu", 0755);
    mkdir("/home/wubu/.wubu/games", 0755);
}

/* GS1: begin a play session. */
void wubu_game_session_begin(const char *game, WUBU_GAME_KIND kind)
{
    ensure_dir();
    snprintf(g_cur_game, sizeof(g_cur_game), "%s", game ? game : "?");
    g_cur_kind = kind;
    g_cur_t0 = (int64_t)time(NULL);
    g_cur_active = 1;
}

/* GS2: end the play — append the training line to the ledger. */
void wubu_game_session_end(void)
{
    if (!g_cur_active) return;
    int64_t t1 = (int64_t)time(NULL);
    int64_t dur = t1 - g_cur_t0;
    if (dur < 0) dur = 0;

    /* the world state at the session end (the AGI's perception) */
    wubu_world_sample();
    const wubu_world_t *w = wubu_world_snapshot();

    ensure_dir();
    FILE *f = fopen(PLAYS_LOG, "a");
    if (f) {
        fprintf(f, "play|%s|%s|%lld|%lld|hw[%s%s%s%s] net[wifi:%d] "
                   "power[bat:%d%% %s] heat[cpu:%dC thr:%d]\n",
                g_cur_game,
                wubu_game_kind_name(g_cur_kind),
                (long long)g_cur_t0, (long long)dur,
                w && w->has_nvme ? "nvme+" : "",
                w && w->has_sd ? "sd+" : "",
                w && w->has_gpu ? "gpu+" : "",
                w && w->has_wifi ? "wifi+" : "",
                w ? w->wifi_link : 0,
                w ? (int)w->battery_pct : 0,
                w && w->battery_charging ? "charging" : "discharging",
                w ? (int)w->cpu_temp : 0,
                w ? w->throttled : 0);
        fclose(f);
    }
    g_cur_active = 0;
}

/* GS3: the session state (the test hooks). */
int wubu_game_session_active(void) { return g_cur_active; }

/* GS4: the ledger path (the AGI + the tests read it). */
const char *wubu_game_session_log(void) { return PLAYS_LOG; }

/* GS5: the test hooks */
void wubu_game_session_test_reset(void)
{
    g_cur_active = 0;
    g_cur_t0 = 0;
    g_cur_game[0] = '\0';
}
