/*
 * wubu_session_daemon.h -- P3 session-manager daemon (the role-up).
 *
 * This is the piece that ties ALL FIVE baseline service contracts into one
 * desktop-owned session manager — the cinnamon-session analogue that "starts
 * all the other processes" (§1) and owns the /n/background control surface.
 *
 * The daemon owns:
 *   - the session bus via a wubu_session_boot (claims names + START the
 *     autostart apps through an injectable launcher);
 *   - the logind-equivalent registry (wubu_logind: sessions/seats/active);
 *   - the polkit-equivalent authority (wubu_auth) WIRED AS the logind power-
 *     op gate (per baseline §4.3 power 'funnels through' the auth surface);
 *   - the /n/background control state (publishes bus + session + auth).
 *
 * It is a COMPOSITION over the tested primitives, not a re-implementation: the
 * daemon exposes 'boot the session' once, publish once, and delegate power
 * decisions to the wired authority. The launcher is injectable so a host can
 * route autostart apps through wubu_exec / a container.
 *
 * C11, runtime host, single-threaded (call boot/publish/power from one loop).
 */
#ifndef WUBU_SESSION_DAEMON_H
#define WUBU_SESSION_DAEMON_H

#include <stdint.h>
#include <stddef.h>
#include "wubu_session_boot.h"
#include "wubu_logind.h"
#include "wubu_auth.h"

typedef struct wubu_session_daemon wubu_session_daemon_t;

typedef int (*wubu_session_daemon_launch_fn)(wubu_session_boot_t *boot,
                                             const wubu_xdga_entry_t *entry,
                                             char **argv, void *ud);

/* Lifecycle --------------------------------------------------------------- */
wubu_session_daemon_t *wubu_session_daemon_create(void);
void wubu_session_daemon_destroy(wubu_session_daemon_t *daemon);

/* Owned components (for wiring / inspection) ------------------------------ */
wubu_session_boot_t *wubu_session_daemon_boot(wubu_session_daemon_t *daemon);
wubu_logindd_t        *wubu_session_daemon_login(wubu_session_daemon_t *daemon);
wubu_auth_t         *wubu_session_daemon_auth(wubu_session_daemon_t *daemon);

/* Configuration ----------------------------------------------------------- */
void wubu_session_daemon_set_launcher(wubu_session_daemon_t *daemon,
                                      wubu_session_daemon_launch_fn fn, void *ud);

/* Session bootstrap ------------------------------------------------------- */
/* Register the active login session (id/seat/type/uid). Returns 0. */
int wubu_session_daemon_add_session(wubu_session_daemon_t *daemon,
                                    const wubu_logindd_session_t *sess);
/* Bring the session up: scan autostart dirs + start every launchable app
 * through the injectable launcher (via the internal session-boot). Returns
 * how many apps were started, or -1 on no-launcher/scan error. */
int wubu_session_daemon_start(wubu_session_daemon_t *daemon,
                              char *const *autostart_dirs, int n_dirs);
/* Publish the whole /n/background state (bus + session + auth). 0 on ok. */
int wubu_session_daemon_publish(wubu_session_daemon_t *daemon);

/* Power (goes through login, which gates on the wired authority) ---------- */
int wubu_session_daemon_power(wubu_session_daemon_t *daemon,
                              wubu_logindd_power_op_t op);

#endif /* WUBU_SESSION_DAEMON_H */
