/*
 * wubu_session.c -- the SESSION MANAGER (the SteamOS desktop-space
 * steal).
 *
 * SteamOS runs two sessions and switches between them
 * (steamos-session-select):
 *   - GAME MODE: the gamescope compositor session (the Deck UI) —
 *     gamescope owns the display, Steam is the shell
 *   - DESKTOP MODE: the Plasma session — a full desktop (KDE), SDDM
 *     is the login manager
 *
 * WuBuOS's session manager mirrors that contract for ITS two
 * sessions:
 *   - WUBU_SESSION_GAME: the gamescope compositor session (the
 *     proton2 gamescope launch path)
 *   - WUBU_SESSION_DESKTOP: the dosgui desktop (the Win98/XP-style
 *     GUI WuBuOS already ships)
 *
 * The manager:
 *   wubu_session_init()          — start in the current session
 *   wubu_session_switch()        — switch to the other one
 *   wubu_session_set()           — set to a specific session
 *   wubu_session_current()       — the active session
 *   /n/session/current           — read it / write game|desktop
 *
 * The switch is recorded + the launch command for the target session
 * is produced (the compositor cmd for game, the desktop cmd for
 * desktop) — WuBuOS's session OWNERSHIP stays with the dosgui shell,
 * exactly as SteamOS's Plasma owns Desktop Mode.
 *
 * C11, self-contained.
 */
#include "wubu_session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int  current;         /* WUBU_SESSION_* */
    int  initialized;
    char last_cmd[1024];  /* the launch command of the last switch */
} wubu_session_t;

static wubu_session_t g_ss;

const char *wubu_session_name(int s)
{
    switch (s) {
    case WUBU_SESSION_GAME:    return "game";
    case WUBU_SESSION_DESKTOP: return "desktop";
    default:                   return "unknown";
    }
}

/* SS1: init — start in the given session (or desktop by default). */
void wubu_session_init(int session)
{
    memset(&g_ss, 0, sizeof(g_ss));
    g_ss.current = (session == WUBU_SESSION_GAME) ? WUBU_SESSION_GAME
                                                  : WUBU_SESSION_DESKTOP;
    g_ss.initialized = 1;
    /* the launch command for the starting session */
    if (g_ss.current == WUBU_SESSION_GAME)
        snprintf(g_ss.last_cmd, sizeof(g_ss.last_cmd),
                 "gamescope --session steam -- %s",
                 WUBU_SESSION_GAME_CMD);
    else
        snprintf(g_ss.last_cmd, sizeof(g_ss.last_cmd), "%s",
                 WUBU_SESSION_DESKTOP_CMD);
}

/* SS2: the current session. */
int wubu_session_current(void)
{
    return g_ss.initialized ? g_ss.current : WUBU_SESSION_DESKTOP;
}

/* SS3: set to a specific session. Returns 0 on success. */
int wubu_session_set(int session)
{
    if (!g_ss.initialized) return -1;
    if (session != WUBU_SESSION_GAME && session != WUBU_SESSION_DESKTOP)
        return -1;
    g_ss.current = session;
    if (session == WUBU_SESSION_GAME)
        snprintf(g_ss.last_cmd, sizeof(g_ss.last_cmd),
                 "gamescope --session steam -- %s",
                 WUBU_SESSION_GAME_CMD);
    else
        snprintf(g_ss.last_cmd, sizeof(g_ss.last_cmd), "%s",
                 WUBU_SESSION_DESKTOP_CMD);
    return 0;
}

/* SS4: switch to the OTHER session. */
int wubu_session_switch(void)
{
    if (!g_ss.initialized) return -1;
    return wubu_session_set(g_ss.current == WUBU_SESSION_GAME
                            ? WUBU_SESSION_DESKTOP : WUBU_SESSION_GAME);
}

/* SS5: the launch command of the last switch. */
const char *wubu_session_last_cmd(void)
{
    return g_ss.last_cmd;
}

/* SS6: the test hooks (the view type lives in wubu_session.h) */
int wubu_session_get(wubu_session_view_t *out)
{
    if (!out) return -1;
    out->current = g_ss.current;
    out->initialized = g_ss.initialized;
    snprintf(out->last_cmd, sizeof(out->last_cmd), "%s", g_ss.last_cmd);
    return 0;
}

/* SS7: parse a session name ("game" / "desktop"), -1 on error. */
int wubu_session_from_name(const char *name)
{
    if (!name) return -1;
    if (strcmp(name, "game") == 0) return WUBU_SESSION_GAME;
    if (strcmp(name, "desktop") == 0) return WUBU_SESSION_DESKTOP;
    return -1;
}
