/*
 * wubu_logind.c -- logind-equivalent session registry + gated power ops (P3).
 * C11, runtime host, single-threaded. Sessions tracked in a fixed array;
 * power ops gated by an injectable authorizer.
 */
#include "wubu_logind.h"
#include <stdlib.h>
#include <string.h>

struct wubu_logind {
    wubu_logindd_session_t sessions[WUBU_LOGIN_MAX_SESSIONS];
    int n_sessions;
    wubu_logindd_authorize_fn authorize;
    void *authorize_ud;
};

wubu_logindd_t *wubu_logindd_create(void) {
    return (wubu_logindd_t *)calloc(1, sizeof(wubu_logindd_t));
}

void wubu_logindd_destroy(wubu_logindd_t *login) {
    if (login) free(login);
}

int wubu_logindd_add_session(wubu_logindd_t *login, const wubu_logindd_session_t *s) {
    if (!login || !s) return -1;
    /* replace an existing session with the same id */
    for (int i = 0; i < login->n_sessions; i++) {
        if (login->sessions[i].id == s->id) {
            login->sessions[i] = *s;
            return 0;
        }
    }
    if (login->n_sessions >= WUBU_LOGIN_MAX_SESSIONS) return -1;
    login->sessions[login->n_sessions++] = *s;
    return 0;
}

int wubu_logindd_session_count(const wubu_logindd_t *login) {
    return login ? login->n_sessions : 0;
}

const wubu_logindd_session_t *wubu_logindd_session_at(const wubu_logindd_t *login, int i) {
    if (!login || i < 0 || i >= login->n_sessions) return NULL;
    return &login->sessions[i];
}

const wubu_logindd_session_t *wubu_logindd_session_by_id(const wubu_logindd_t *login,
                                                     uint32_t id) {
    for (int i = 0; i < login->n_sessions; i++)
        if (login->sessions[i].id == id) return &login->sessions[i];
    return NULL;
}

const wubu_logindd_session_t *wubu_logindd_active(const wubu_logindd_t *login) {
    for (int i = 0; i < login->n_sessions; i++)
        if (login->sessions[i].state == WUBU_SESSION_STATE_ACTIVE)
            return &login->sessions[i];
    return NULL;
}

const char *wubu_logindd_type_str(wubu_logindd_type_t t) {
    switch (t) {
        case WUBU_SESSION_TYPE_X11:     return "x11";
        case WUBU_SESSION_TYPE_WAYLAND: return "wayland";
        case WUBU_SESSION_TYPE_TTY:     return "tty";
        default:                        return NULL;
    }
}

const char *wubu_logindd_state_str(wubu_logindd_state_t s) {
    switch (s) {
        case WUBU_SESSION_STATE_ACTIVE:  return "active";
        case WUBU_SESSION_STATE_ONLINE:  return "online";
        case WUBU_SESSION_STATE_CLOSING: return "closing";
        default:                         return NULL;
    }
}

void wubu_logindd_set_authorizer(wubu_logindd_t *login,
                               wubu_logindd_authorize_fn fn, void *ud) {
    login->authorize = fn;
    login->authorize_ud = ud;
}

int wubu_logindd_power(wubu_logindd_t *login, wubu_logindd_power_op_t op) {
    if (!login) return -1;
    const wubu_logindd_session_t *active = wubu_logindd_active(login);
    if (!active) return -1;                 /* no active session to act for */
    if (!login->authorize) return -1;       /* no authorizer -> cannot authorize */
    int verdict = login->authorize(login, active, op, login->authorize_ud);
    if (verdict != 0) return verdict;       /* DENIED (nonzero = the deny code) */
    return 0;                               /* ALLOWED */
}
