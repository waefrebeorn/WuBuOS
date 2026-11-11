/*
 * wubu_sdr_relay.h -- the SDR relay decision (fail closed).
 *
 * The ticket and the route table say what MIGHT be allowed; this says what IS
 * allowed for one packet.  A relay is an access-control device, so the rule is:
 *
 *   EVERY check must pass, and the FIRST failure decides.
 *
 * The checks, in order:
 *   1. the destination must have a ROUTE            -> DENY_NO_ROUTE
 *   2. that route must be READY                     -> DENY_NOT_READY
 *      (a not-ready route would black-hole the packet -- a DoS on the sender)
 *   3. a TICKET must be presented                  -> DENY_NO_TICKET
 *   4. the ticket must be valid at `now`            -> DENY_TICKET_EXPIRED
 *   5. the ticket must match BOTH addresses         -> DENY_TICKET_MISMATCH
 *   6. the packet must not loop back to its sender  -> DENY_LOOPBACK
 *   7. the sender must not exceed its rate budget   -> DENY_RATE_LIMITED
 *
 * Only then is it FORWARDED.  There is no "allow by default".
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_SDR_RELAY_H
#define WUBU_SDR_RELAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wubu_sdr.h"

typedef enum {
    WUBU_SDR_FORWARD = 0,
    WUBU_SDR_DENY_NO_ROUTE,
    WUBU_SDR_DENY_NOT_READY,
    WUBU_SDR_DENY_NO_TICKET,
    WUBU_SDR_DENY_TICKET_EXPIRED,
    WUBU_SDR_DENY_TICKET_MISMATCH,
    WUBU_SDR_DENY_LOOPBACK,
    WUBU_SDR_DENY_RATE_LIMITED,
    WUBU_SDR_DENY_BAD_ARGUMENT
} wubu_sdr_decision_t;

const char *wubu_sdr_decision_name(wubu_sdr_decision_t d);

/* The relay decision for one packet.  `ticket` may be NULL (no ticket). */
wubu_sdr_decision_t wubu_sdr_relay(const wubu_sdr_table_t *t,
                                   const wubu_sdr_addr_t *from,
                                   const wubu_sdr_addr_t *to,
                                   const wubu_sdr_ticket_t *ticket,
                                   uint64_t now);

/* -- a token-bucket rate limiter, integer only -------------------------- */

typedef struct {
    uint32_t rate_per_sec;    /* sustained rate */
    uint32_t burst;           /* bucket capacity, in packets */
    uint32_t tokens_milli;    /* milli-tokens, so no floating point */
    uint64_t last_ms;         /* last refill, unix milliseconds */
} wubu_sdr_rate_limit_t;

void wubu_sdr_rate_init(wubu_sdr_rate_limit_t *rl, uint32_t rate_per_sec,
                        uint32_t burst, uint64_t now_ms);

/* Consume one packet's worth of budget.  Returns true when allowed. */
bool wubu_sdr_rate_allow(wubu_sdr_rate_limit_t *rl, uint64_t now_ms);

uint32_t wubu_sdr_rate_tokens(const wubu_sdr_rate_limit_t *rl);

#endif /* WUBU_SDR_RELAY_H */
