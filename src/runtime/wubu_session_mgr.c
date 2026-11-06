/*
 * wubu_session_mgr.c -- P3 session-boot glue (see wubu_session_mgr.h).
 * Brings up the rich background: session daemon (bus+login+auth wired),
 * default power policy, autostart, de-surface registration, and the full
 * /n/background publish. C11, single-threaded.
 */
#include "wubu_session_mgr.h"
#include "wubu_ns_background.h"
#include "wubu_auth.h"
#include "wubu_logind.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct wubu_session_mgr {
    wubu_session_daemon_t *daemon;
    wubu_deskenv_t *de;
};

wubu_session_mgr_t *wubu_session_mgr_boot(char *const *autostart_dirs, int n_dirs,
                                        wubu_session_daemon_launch_fn launcher,
                                        void *launch_ud) {
    wubu_session_mgr_t *bg = (wubu_session_mgr_t *)calloc(1, sizeof(*bg));
    if (!bg) return NULL;
    bg->daemon = wubu_session_daemon_create();
    bg->de = wubu_deskenv_create();
    if (!bg->daemon || !bg->de) {
        if (bg->daemon) wubu_session_daemon_destroy(bg->daemon);
        if (bg->de)     wubu_deskenv_destroy(bg->de);
        free(bg);
        return NULL;
    }

    /* active login session (seat0, x11, user, uid 1000) */
    wubu_logindd_session_t s;
    memset(&s, 0, sizeof(s));
    s.id = 1;
    snprintf(s.seat, sizeof(s.seat), "%s", "seat0");
    s.state = WUBU_SESSION_STATE_ACTIVE;
    s.type = WUBU_SESSION_TYPE_X11;
    snprintf(s.class_, sizeof(s.class_), "%s", "user");
    s.uid = 1000;
    wubu_session_daemon_add_session(bg->daemon, &s);

    /* default power policy on the wired authority: suspend granted for uid
     * 1000; reboot + shutdown require an interactive agent (a user prompt). */
    wubu_auth_t *auth = wubu_session_daemon_auth(bg->daemon);
    wubu_auth_add_rule(auth, 1000, "power.suspend", WUBU_AUTH_ALLOW);
    wubu_auth_add_rule(auth, WUBU_AUTH_UID_ANY, "power.reboot",   WUBU_AUTH_INTERACT);
    wubu_auth_add_rule(auth, WUBU_AUTH_UID_ANY, "power.shutdown", WUBU_AUTH_INTERACT);

    /* autostart through the injectable launcher */
    wubu_session_daemon_set_launcher(bg->daemon, launcher, launch_ud);
    if (autostart_dirs && n_dirs > 0)
        wubu_session_daemon_start(bg->daemon, autostart_dirs, n_dirs);

    return bg;
}

void wubu_session_mgr_shutdown(wubu_session_mgr_t *bg) {
    if (!bg) return;
    if (bg->daemon) wubu_session_daemon_destroy(bg->daemon);
    if (bg->de)     wubu_deskenv_destroy(bg->de);
    free(bg);
}

int wubu_session_mgr_register_desktop(wubu_session_mgr_t *bg, const char *owner) {
    if (!bg || !bg->de) return -1;
    return wubu_deskenv_desktop_set(bg->de, owner);
}

int wubu_session_mgr_register_tray(wubu_session_mgr_t *bg, const char *slot,
                                  const char *owner) {
    if (!bg || !bg->de) return -1;
    return wubu_deskenv_tray_claim(bg->de, slot, owner);
}

wubu_session_daemon_t *wubu_session_mgr_daemon(wubu_session_mgr_t *bg) {
    return bg ? bg->daemon : NULL;
}
wubu_deskenv_t *wubu_session_mgr_deskenv(wubu_session_mgr_t *bg) {
    return bg ? bg->de : NULL;
}
