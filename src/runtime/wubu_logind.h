/*
 * wubu_logind.h -- the logind session contract (Mint baseline §4.3 / §153).
 *
 * logind "keeps track of user logins and seats, allocating a per-user slice +
 * session scope, managing session IDs, and exposing machine power/sleep
 * operations" gated by the auth surface. This is the WuBuOS logind-equivalent:
 * a SESSION REGISTRY where a login manager registers the active session (id,
 * seat, type, class, uid, state), and machine power operations (suspend /
 * reboot / shutdown) are GATED by an injectable authorization check — the
 * polkit seam, wired to wubu_auth next.
 *
 * Scope: the SESSION + SEAT + POWER-AUTHORIZATION contract, as data + a tiny
 * policy. The conjugate /n/session surface (§153: one entry per logged-in
 * session with seat/type/state, plus gated power ops) is published from here.
 *
 * C11, runtime host, single-threaded. Opaque struct.
 */
#ifndef WUBU_LOGIN_H
#define WUBU_LOGIN_H

#include <stdint.h>
#include <stddef.h>

#define WUBU_LOGIN_MAX_SESSIONS 16
#define WUBU_LOGIN_MAX_STR      64

/* session state (loginctl's model: active / online / ...) */
typedef enum {
    WUBU_SESSION_STATE_ACTIVE = 0,
    WUBU_SESSION_STATE_ONLINE,
    WUBU_SESSION_STATE_CLOSING
} wubu_logindd_state_t;

/* session type (loginctl Type=) */
typedef enum {
    WUBU_SESSION_TYPE_X11 = 0,
    WUBU_SESSION_TYPE_WAYLAND,
    WUBU_SESSION_TYPE_TTY
} wubu_logindd_type_t;

typedef struct {
    uint32_t id;                     /* session id (loginctl: c1) */
    char seat[WUBU_LOGIN_MAX_STR];   /* seat0 */
    wubu_logindd_state_t state;
    wubu_logindd_type_t type;
    char class_[WUBU_LOGIN_MAX_STR]; /* user / greeter */
    uint32_t uid;                    /* the logged-in user's uid */
} wubu_logindd_session_t;

/* power operations (logind's exposed machine ops) */
typedef enum {
    WUBU_POWER_SUSPEND = 0,
    WUBU_POWER_REBOOT,
    WUBU_POWER_SHUTDOWN
} wubu_logindd_power_op_t;

typedef struct wubu_logind wubu_logindd_t;

/* Auth gate: should `op` be allowed for uid on the active session? 0 = allow,
 * nonzero = deny. The session manager wires this to the polkit authority
 * (wubu_auth next); without one, the default is to DENY power ops. */
typedef int (*wubu_logindd_authorize_fn)(wubu_logindd_t *login,
                                       const wubu_logindd_session_t *sess,
                                       wubu_logindd_power_op_t op, void *ud);

/* Lifecycle --------------------------------------------------------------- */
wubu_logindd_t *wubu_logindd_create(void);
void wubu_logindd_destroy(wubu_logindd_t *login);

/* Register/log the given session (replaces any session with the same id).
 * Returns 0 on success, -1 on invalid input or a full registry. */
int wubu_logindd_add_session(wubu_logindd_t *login, const wubu_logindd_session_t *s);

/* How many sessions are tracked? */
int wubu_logindd_session_count(const wubu_logindd_t *login);
/* The i-th session (0..count-1); NULL out of range. */
const wubu_logindd_session_t *wubu_logindd_session_at(const wubu_logindd_t *login, int i);
/* The session with the given id; NULL if not tracked. */
const wubu_logindd_session_t *wubu_logindd_session_by_id(const wubu_logindd_t *login,
                                                     uint32_t id);
/* The ACTIVE session (state == ACTIVE); NULL if none. */
const wubu_logindd_session_t *wubu_logindd_active(const wubu_logindd_t *login);

/* Short strings for the logind enums (for /n publishing):
 * seat/type/state -> "x11","wayland","tty"/"active","online","closing" (or -1). */
const char *wubu_logindd_type_str(wubu_logindd_type_t t);
const char *wubu_logindd_state_str(wubu_logindd_state_t s);

/* Power operations -------------------------------------------------------- */
void wubu_logindd_set_authorizer(wubu_logindd_t *login,
                               wubu_logindd_authorize_fn fn, void *ud);
/* Run a machine power op IF authorized for the current active session.
 * Returns 0 if authorized (the op was allowed to proceed), a positive DENY
 * code equal to the authorizer's nonzero return if denied, or -1 on setup
 * error (no active session, no authorizer set). */
int wubu_logindd_power(wubu_logindd_t *login, wubu_logindd_power_op_t op);

#endif /* WUBU_LOGIN_H */
