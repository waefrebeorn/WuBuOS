/*
 * wubu_sdr_session.h -- the SDR POP-side session layer.
 *
 * The relay decides about PACKETS; this manages the SESSIONS behind them: a
 * peer's virtual address on a POP, its liveness, and its teardown.
 *
 * The rule that matters most is PORT REASSIGNMENT:
 *
 *   A virtual port must NEVER be handed to a new session while the old one
 *   could still receive traffic.  Otherwise packets for the previous peer are
 *   delivered to the new one -- a session hijack, and indistinguishable from a
 *   legitimate handover.  So a port becomes reusable only after its session is
 *   REAPED (expired past keepalive), never merely closed.
 *
 * The other rules:
 *   * A session is ALIVE only while it is not closed AND its last-seen is
 *     within the keepalive window.  Silence means gone.
 *   * Opening a session for a peer that already has one is IDEMPOTENT and
 *     returns the existing address -- a client retrying a connect must not get
 *     a second address.
 *   * Touching or closing an unknown session is an ERROR, not a no-op: a POP
 *     that silently accepted those would mask a client bug or an attack.
 *   * Reaping is explicit, so a caller decides WHEN liveness is evaluated --
 *     and the return value says how many sessions died.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_SDR_SESSION_H
#define WUBU_SDR_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wubu_sdr.h"

#define WUBU_SDR_MAX_SESSIONS 256

typedef struct {
    wubu_sdr_addr_t addr;
    uint64_t        peer_id;
    uint32_t        pop_id;
    uint64_t        created_ms;
    uint64_t        last_seen_ms;
    uint32_t        keepalive_ms;
    bool            closed;
    bool            in_use;
} wubu_sdr_session_t;

typedef struct {
    uint32_t           count;
    wubu_sdr_session_t sessions[WUBU_SDR_MAX_SESSIONS];
    uint32_t           next_vport;
    uint32_t           vip_base;
    uint16_t           free_ports[WUBU_SDR_MAX_SESSIONS];  /* ports freed by a reap */
    uint32_t           free_count;
} wubu_sdr_sessions_t;

void wubu_sdr_sessions_init(wubu_sdr_sessions_t *s, uint32_t vip_base,
                            uint32_t vport_base);

/* Open a session for `peer_id`, or return the existing one (idempotent).
 * Returns 0 and fills *out_vport, or -1 when full or the peer is already on a
 * different POP. */
int wubu_sdr_sessions_open(wubu_sdr_sessions_t *s, uint64_t peer_id,
                           uint32_t pop_id, uint32_t *out_vport, uint64_t now_ms);

/* Is this session alive at `now_ms`?  Not closed, and within keepalive. */
bool wubu_sdr_session_is_alive(const wubu_sdr_session_t *sess, uint64_t now_ms);

/* Record traffic.  Returns -1 for an unknown port. */
int wubu_sdr_sessions_touch(wubu_sdr_sessions_t *s, uint32_t vport,
                            uint64_t now_ms);

/* Close a session.  Closing twice is an ERROR.  The port is NOT reusable
 * until the session is reaped. */
int wubu_sdr_sessions_close(wubu_sdr_sessions_t *s, uint32_t vport);

/* Remove sessions that are no longer alive.  Returns how many were reaped. */
uint32_t wubu_sdr_sessions_reap(wubu_sdr_sessions_t *s, uint64_t now_ms);

const wubu_sdr_session_t *wubu_sdr_sessions_find(const wubu_sdr_sessions_t *s,
                                                 uint32_t vport);

uint32_t wubu_sdr_sessions_alive(const wubu_sdr_sessions_t *s, uint64_t now_ms);

#endif /* WUBU_SDR_SESSION_H */
