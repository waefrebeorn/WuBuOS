/*
 * wubu_archd_svc.h -- In-process service supervisor for WuBuOS archd.
 *
 * N1/N2 closure: wubu_archd_svc_* previously shelled out to
 * `arch-chroot ... systemctl` via popen() (shell injection + no-op without
 * an Arch root). This supervisor is a REAL service manager: it fork/execs
 * the service binary inside its root namespace, tracks the child PID, and
 * reports liveness from /proc -- no systemd, no shell.
 *
 * The Styx /n/services bus (wubu_ns_bridge) reads status from here, so the
 * whole control plane is real (N3). dosgui_service_mgr registers autostart
 * entries and boots them through this supervisor (N4 surfaces failures).
 *
 * Opaque struct, C11, minimal includes.
 */
#ifndef WUBU_ARCHD_SVC_H
#define WUBU_ARCHD_SVC_H

#include "wubu_archd.h"   /* WubuArchService, WUBU_ARCHD_MAX_* */

typedef struct wubu_svc_supervisor wubu_svc_supervisor_t;

/* Create/destroy a supervisor instance (owns no global state). */
wubu_svc_supervisor_t *wubu_svc_supervisor_create(void);
void wubu_svc_supervisor_destroy(wubu_svc_supervisor_t *s);

/* Register a managed service. exec_path may be NULL -> a default binary is
 * resolved at start time (root/usr/bin/<svc>, root/bin/<svc>, then host
 * <svc>). argv is currently unused (bare exec). Returns 0/-1. */
int wubu_svc_supervisor_add(wubu_svc_supervisor_t *s, const char *root,
                            const char *svc, const char *exec_path,
                            char *const argv[]);

int  wubu_svc_supervisor_start(wubu_svc_supervisor_t *s, const char *root,
                               const char *svc);
int  wubu_svc_supervisor_stop(wubu_svc_supervisor_t *s, const char *root,
                              const char *svc);
int  wubu_svc_supervisor_restart(wubu_svc_supervisor_t *s, const char *root,
                                 const char *svc);
int  wubu_svc_supervisor_status(wubu_svc_supervisor_t *s, const char *root,
                                const char *svc, WubuArchService *out);
/* Reap dead children, update states, honor auto_restart. Returns #reaped. */
int  wubu_svc_supervisor_reap(wubu_svc_supervisor_t *s);
int  wubu_svc_supervisor_count(const wubu_svc_supervisor_t *s);

/* Health heartbeat (N8): register a callback fired for each managed
 * service that WAS running and died unexpectedly (not auto_restart).
 * wubu_svc_supervisor_poll() reaps dead children, restarts
 * auto_restart ones, and invokes the callback for the rest.
 * Returns the number of unexpected deaths detected this poll. */
typedef void (*wubu_svc_on_fail_fn)(const char *root, const char *svc,
                                    void *ud);
void wubu_svc_supervisor_set_on_fail(wubu_svc_supervisor_t *s,
                                    wubu_svc_on_fail_fn fn, void *ud);
int  wubu_svc_supervisor_poll(wubu_svc_supervisor_t *s);

/* -- Declarative unit model (systemd's GOOD ideas, minus the
 *  notorious parts) ------------------------------------------
 * We keep: Requires=/After= ordering, Type= (simple/forking/notify),
 * Restart=. We DROP: PID1-coupling, binary journald, and the
 * "active(running)" lie (spawned != ready). Readiness is
 * TRUTHFUL: a Type=notify unit stays STARTING until it calls
 * wubu_svc_supervisor_notify_ready(); a Type=forking unit is
 * only ready when its daemon grandchild is alive (not just the
 * fork). Boot is ORDERED: deps start first, each must reach
 * RUNNING (or time out -> FAILED) before dependents. */
typedef enum { SVC_TYPE_SIMPLE, SVC_TYPE_FORKING, SVC_TYPE_NOTIFY } wubu_svc_type_t;

/* Set declarative unit fields (call before start/boot). */
int wubu_svc_supervisor_add_dep(wubu_svc_supervisor_t *s, const char *root,
                                  const char *svc, const char *dep);   /* Requires= */
int wubu_svc_supervisor_add_after(wubu_svc_supervisor_t *s, const char *root,
                                   const char *svc, const char *pre);  /* After= */
void wubu_svc_supervisor_set_type(wubu_svc_supervisor_t *s, const char *root,
                                  const char *svc, wubu_svc_type_t t);
void wubu_svc_supervisor_set_restart(wubu_svc_supervisor_t *s, const char *root,
                                    const char *svc, bool on);
void wubu_svc_supervisor_set_timeout_ms(wubu_svc_supervisor_t *s, const char *root,
                                       const char *svc, int ms);

/* Type=notify readiness: a service calls this (e.g. on a real
 * socket/pipe event) to declare it is actually serving. Until
 * then its state is STARTING, never the lying RUNNING. */
int wubu_svc_supervisor_notify_ready(wubu_svc_supervisor_t *s,
                                     const char *root, const char *svc);

/* Ordered boot (kills boot races): starts units so that every
 * Requires= dep and After= predecessor is RUNNING (or FAILED)
 * before the unit itself is started; each unit is waited on up to
 * its timeout. Returns the number of units that did NOT reach
 * RUNNING. */
int wubu_svc_supervisor_boot(wubu_svc_supervisor_t *s);

/* -- Plain-text journal (kills journald) -----------------------
 * Logs are human-grepable text, never a binary blob. Each write
 * also lands in an in-memory ring so the desktop can tail without
 * dbus. Files live at /tmp/wubu-svc/<root>/<svc>.log. */
int  wubu_svc_supervisor_log(wubu_svc_supervisor_t *s, const char *root,
                                const char *svc, const char *msg);
int  wubu_svc_supervisor_logs(wubu_svc_supervisor_t *s, const char *root,
                                 const char *svc, char *out, size_t n);

/* Route the daemon's wubu_archd_svc_* ops through this supervisor when set.
 * NULL restores the external (no-supervisor) fallback. */
void wubu_archd_svc_set_supervisor(wubu_svc_supervisor_t *s);
wubu_svc_supervisor_t *wubu_archd_svc_get_supervisor(void);

#endif /* WUBU_ARCHD_SVC_H */
