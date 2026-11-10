/*
 * wubu_session_daemon.c -- P3 session-manager daemon (the role-up).
 *
 * Composition over the five shipped contracts. Owns a session-boot (bus +
 * autostart), a logind registry, a polkit authority WIRED as the logind
 * power-op gate, and publishes /n/background. C11, single-threaded.
 */
#include "wubu_session_daemon.h"
#include "wubu_ns_background.h"
#include "wubu_ns_effect.h"
#include <stdlib.h>

struct wubu_session_daemon {
    wubu_session_boot_t *boot;
    wubu_logindd_t *login;
    wubu_auth_t *auth;
    wubu_session_daemon_launch_fn launch;
    void *launch_ud;
};

/* bridge: session-boot's launcher signature forwards to the daemon's */
static int daemon_launch_bridge(wubu_session_boot_t *boot,
                                const wubu_xdga_entry_t *entry,
                                char **argv, void *ud) {
    wubu_session_daemon_t *d = (wubu_session_daemon_t *)ud;
    if (!d || !d->launch) return -1;
    return d->launch(boot, entry, argv, d->launch_ud);
}

/* bridge: the authority answers the logind power-op gate */
static int auth_authorizer(wubu_logindd_t *login,
                           const wubu_logindd_session_t *sess,
                           wubu_logindd_power_op_t op, void *ud) {
    (void)login; (void)sess;
    wubu_auth_t *auth = (wubu_auth_t *)ud;
    if (!auth) return -1;
    /* map the logind power op to a polkit action string */
    const char *action = (op == WUBU_POWER_SUSPEND) ? "power.suspend" :
                         (op == WUBU_POWER_REBOOT)  ? "power.reboot"  :
                                                      "power.shutdown";
    int agent_enabled = 0;
    wubu_auth_decision_t d = wubu_auth_check(auth, sess->uid, action, &agent_enabled);
    /* the login power gate treats NONZERO as deny, 0 as allow.
     * WUBU_AUTH_DENY==0, so on denial return a nonzero code (1). */
    return (d == WUBU_AUTH_ALLOW) ? 0 : WUBU_AUTH_DENY + 1;
}

wubu_session_daemon_t *wubu_session_daemon_create(void) {
    wubu_session_daemon_t *d = (wubu_session_daemon_t *)calloc(1, sizeof(*d));
    if (!d) return NULL;
    d->boot  = wubu_session_boot_create();
    d->login = wubu_logindd_create();
    d->auth  = wubu_auth_create();
    if (!d->boot || !d->login || !d->auth) {
        if (d->boot)  wubu_session_boot_destroy(d->boot);
        if (d->login) wubu_logindd_destroy(d->login);
        if (d->auth)  wubu_auth_destroy(d->auth);
        free(d);
        return NULL;
    }
    /* wire the authority as the logind power gate */
    wubu_logindd_set_authorizer(d->login, auth_authorizer, d->auth);
    /* wire the daemon's launcher into the internal session-boot */
    wubu_session_boot_set_launcher(d->boot, daemon_launch_bridge, d);
    return d;
}

void wubu_session_daemon_destroy(wubu_session_daemon_t *daemon) {
    if (!daemon) return;
    if (daemon->boot)  wubu_session_boot_destroy(daemon->boot);
    if (daemon->login) wubu_logindd_destroy(daemon->login);
    if (daemon->auth)  wubu_auth_destroy(daemon->auth);
    free(daemon);
}

wubu_session_boot_t *wubu_session_daemon_boot(wubu_session_daemon_t *daemon) {
    return daemon ? daemon->boot : NULL;
}
wubu_logindd_t *wubu_session_daemon_login(wubu_session_daemon_t *daemon) {
    return daemon ? daemon->login : NULL;
}
wubu_auth_t *wubu_session_daemon_auth(wubu_session_daemon_t *daemon) {
    return daemon ? daemon->auth : NULL;
}

void wubu_session_daemon_set_launcher(wubu_session_daemon_t *daemon,
                                      wubu_session_daemon_launch_fn fn, void *ud) {
    daemon->launch = fn;
    daemon->launch_ud = ud;
}

int wubu_session_daemon_add_session(wubu_session_daemon_t *daemon,
                                    const wubu_logindd_session_t *sess) {
    if (!daemon || !daemon->login || !sess) return -1;
    return wubu_logindd_add_session(daemon->login, sess);
}

int wubu_session_daemon_start(wubu_session_daemon_t *daemon,
                              char *const *autostart_dirs, int n_dirs) {
    if (!daemon || !daemon->boot) return -1;
    int n = wubu_session_boot_scan(daemon->boot, autostart_dirs, n_dirs);
    if (n < 0) return -1;
    return wubu_session_boot_start_all(daemon->boot);
}

int wubu_session_daemon_publish(wubu_session_daemon_t *daemon) {
    if (!daemon) return -1;
    if (wubu_ns_publish_background(daemon->boot) != 0) return -1;
    if (wubu_ns_publish_login(daemon->login) != 0) return -1;
    if (wubu_ns_publish_auth(daemon->auth) != 0) return -1;
    /* The desktop effect stack.  Published empty; a program fills it through
     * /n/effect/set and wm_render applies it on the next frame. */
    if (wubu_ns_publish_effect() != 0) return -1;
    return 0;
}

int wubu_session_daemon_power(wubu_session_daemon_t *daemon,
                              wubu_logindd_power_op_t op) {
    if (!daemon || !daemon->login) return -1;
    return wubu_logindd_power(daemon->login, op);
}
