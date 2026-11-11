/*
 * wubu_sdr_session.c -- the SDR POP-side session layer.
 *
 * See wubu_sdr_session.h.
 */
#include "wubu_sdr_session.h"

#include <string.h>

void wubu_sdr_sessions_init(wubu_sdr_sessions_t *s, uint32_t vip_base,
                            uint32_t vport_base)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->vip_base = vip_base;
    s->next_vport = vport_base;
}

const wubu_sdr_session_t *wubu_sdr_sessions_find(const wubu_sdr_sessions_t *s,
                                                 uint32_t vport)
{
    if (!s) return NULL;
    for (uint32_t i = 0; i < WUBU_SDR_MAX_SESSIONS; i++) {
        if (s->sessions[i].in_use && s->sessions[i].addr.vport == vport)
            return &s->sessions[i];
    }
    return NULL;
}

bool wubu_sdr_session_is_alive(const wubu_sdr_session_t *sess, uint64_t now_ms)
{
    if (!sess || !sess->in_use || sess->closed) return false;
    if (sess->keepalive_ms == 0) return true;   /* no keepalive: lives until closed */
    return (now_ms - sess->last_seen_ms) < sess->keepalive_ms;
}

int wubu_sdr_sessions_open(wubu_sdr_sessions_t *s, uint64_t peer_id,
                           uint32_t pop_id, uint32_t *out_vport, uint64_t now_ms)
{
    if (!s || !out_vport) return -1;

    /* IDEMPOTENT: a peer has ONE virtual address on a POP.  A client retrying
     * a connect gets the same address, not a second one. */
    for (uint32_t i = 0; i < WUBU_SDR_MAX_SESSIONS; i++) {
        wubu_sdr_session_t *e = &s->sessions[i];
        if (e->in_use && e->peer_id == peer_id) {
            if (e->pop_id != pop_id) return -1;   /* the peer moved POPs */
            *out_vport = e->addr.vport;
            return 0;
        }
    }

    /* find a FREE SLOT -- one that is not in_use at all.  A closed session
     * still holds its port until it is REAPED, so a closed slot is NOT free:
     * reusing it would deliver the old peer's packets to the new one. */
    uint32_t slot = WUBU_SDR_MAX_SESSIONS;
    for (uint32_t i = 0; i < WUBU_SDR_MAX_SESSIONS; i++) {
        if (!s->sessions[i].in_use) { slot = i; break; }
    }
    if (slot == WUBU_SDR_MAX_SESSIONS) return -1;   /* no free slot */

    /* Take a port from the FREE LIST first -- a port reaches that list only by
     * being REAPED, so it can no longer receive the old peer's traffic.  A
     * monotonic counter would never reuse a port and would exhaust the 16-bit
     * space; handing out a port that is still in_use would deliver one peer's
     * traffic to another. */
    uint16_t chosen;
    if (s->free_count > 0) {
        chosen = s->free_ports[--s->free_count];
    } else if (s->next_vport < 65536u) {
        chosen = (uint16_t)s->next_vport++;
    } else {
        return -1;                                  /* the port space is full */
    }

    {
        wubu_sdr_session_t *e = &s->sessions[slot];
        memset(e, 0, sizeof(*e));
        e->in_use = true;
        e->addr.vip = s->vip_base;
        e->addr.vport = chosen;
        e->peer_id = peer_id;
        e->pop_id = pop_id;
        e->created_ms = now_ms;
        e->last_seen_ms = now_ms;
        e->keepalive_ms = 30000;      /* 30 seconds */
        e->closed = false;
        *out_vport = e->addr.vport;
        s->count++;
        return 0;
    }
    return -1;                       /* the table is full */
}

int wubu_sdr_sessions_touch(wubu_sdr_sessions_t *s, uint32_t vport,
                            uint64_t now_ms)
{
    if (!s) return -1;
    const wubu_sdr_session_t *e = wubu_sdr_sessions_find(s, vport);
    if (!e) return -1;               /* an unknown session is an ERROR */
    /* touch through the public const pointer would be wrong; find the slot */
    for (uint32_t i = 0; i < WUBU_SDR_MAX_SESSIONS; i++) {
        if (s->sessions[i].in_use && s->sessions[i].addr.vport == vport) {
            s->sessions[i].last_seen_ms = now_ms;
            return 0;
        }
    }
    return -1;
}

int wubu_sdr_sessions_close(wubu_sdr_sessions_t *s, uint32_t vport)
{
    if (!s) return -1;
    for (uint32_t i = 0; i < WUBU_SDR_MAX_SESSIONS; i++) {
        wubu_sdr_session_t *e = &s->sessions[i];
        if (e->in_use && e->addr.vport == vport) {
            if (e->closed) return -1;    /* closing twice is an ERROR */
            e->closed = true;
            return 0;
        }
    }
    return -1;                       /* unknown port */
}

uint32_t wubu_sdr_sessions_reap(wubu_sdr_sessions_t *s, uint64_t now_ms)
{
    if (!s) return 0;
    uint32_t reaped = 0;
    for (uint32_t i = 0; i < WUBU_SDR_MAX_SESSIONS; i++) {
        wubu_sdr_session_t *e = &s->sessions[i];
        if (!e->in_use) continue;
        if (!wubu_sdr_session_is_alive(e, now_ms)) {
            /* the port goes on the FREE LIST: reusable, but only from here --
             * never while the session is merely closed. */
            if (s->free_count < WUBU_SDR_MAX_SESSIONS)
                s->free_ports[s->free_count++] = e->addr.vport;
            memset(e, 0, sizeof(*e));    /* the slot becomes free */
            s->count--;
            reaped++;
        }
    }
    return reaped;
}

uint32_t wubu_sdr_sessions_alive(const wubu_sdr_sessions_t *s, uint64_t now_ms)
{
    if (!s) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < WUBU_SDR_MAX_SESSIONS; i++) {
        if (wubu_sdr_session_is_alive(&s->sessions[i], now_ms)) n++;
    }
    return n;
}
