/*
 * wubu_session.h -- the session manager (SteamOS desktop-space steal).
 */
#ifndef WUBU_SESSION_H
#define WUBU_SESSION_H

/* the sessions (mirrors SteamOS's Game Mode / Desktop Mode) */
enum {
    WUBU_SESSION_GAME    = 0,   /* the gamescope compositor session */
    WUBU_SESSION_DESKTOP = 1,   /* the dosgui/Plasma desktop session */
};

/* the session launch commands (what the switch produces) */
#define WUBU_SESSION_GAME_CMD     "steam -steamos3 -steampal"
#define WUBU_SESSION_DESKTOP_CMD  "wubu-desktop"

/* SS1: init — start in the given session (desktop by default). */
void wubu_session_init(int session);

/* SS2: the current session. */
int wubu_session_current(void);

/* SS3: set to a specific session. Returns 0 on success. */
int wubu_session_set(int session);

/* SS4: switch to the OTHER session. */
int wubu_session_switch(void);

/* SS5: the launch command of the last switch. */
const char *wubu_session_last_cmd(void);

/* SS6: the test hooks. */
typedef struct {
    int  current;
    int  initialized;
    char last_cmd[1024];
} wubu_session_view_t;
int wubu_session_get(wubu_session_view_t *out);

/* SS7: parse a session name ("game" / "desktop"), -1 on error. */
int wubu_session_from_name(const char *name);

/* the session names (for the /n control plane) */
const char *wubu_session_name(int s);

#endif
