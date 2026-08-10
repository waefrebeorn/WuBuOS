/*
 * wubu_ns_session.c -- the /n/session control subtree (the SteamOS
 * session-select control plane).
 *
 *   /n/session/current  -> read = the active session ("game"|"desktop"),
 *                          write "game"|"desktop" switches it
 *   /n/session/cmd      -> the launch command of the last switch
 *
 * SteamOS reaches session select through a systemd service; WuBuOS
 * expresses it through ONE file on the /n namespace.
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_session.h"

#include <stdio.h>
#include <string.h>

int wubu_ns_publish_session(void)
{
    char sub[128];
    if (ns_mkdir("session") != 0) return -1;

    snprintf(sub, sizeof(sub), "session/current");
    if (ns_write(sub, wubu_session_name(wubu_session_current())) != 0)
        return -1;
    snprintf(sub, sizeof(sub), "session/cmd");
    if (ns_write(sub, wubu_session_last_cmd()) != 0)
        return -1;
    return 0;
}

/* `echo game > /n/session/current` — switch the session. */
int wubu_ns_session_set(const char *name)
{
    int s = wubu_session_from_name(name);
    if (s < 0) return -1;
    if (wubu_session_set(s) != 0) return -1;
    char sub[128];
    snprintf(sub, sizeof(sub), "session/current");
    if (ns_write(sub, wubu_session_name(wubu_session_current())) != 0)
        return -1;
    snprintf(sub, sizeof(sub), "session/cmd");
    return ns_write(sub, wubu_session_last_cmd());
}
