/*
 * wubu_sdr_relay.c -- the SDR relay decision (fail closed).
 *
 * See wubu_sdr_relay.h.
 */
#include "wubu_sdr_relay.h"

#include <string.h>

const char *wubu_sdr_decision_name(wubu_sdr_decision_t d)
{
    switch (d) {
    case WUBU_SDR_FORWARD:             return "forward";
    case WUBU_SDR_DENY_NO_ROUTE:       return "deny: no route";
    case WUBU_SDR_DENY_NOT_READY:      return "deny: route not ready";
    case WUBU_SDR_DENY_NO_TICKET:      return "deny: no ticket";
    case WUBU_SDR_DENY_TICKET_EXPIRED: return "deny: ticket expired";
    case WUBU_SDR_DENY_TICKET_MISMATCH:return "deny: ticket mismatch";
    case WUBU_SDR_DENY_LOOPBACK:       return "deny: loopback";
    case WUBU_SDR_DENY_RATE_LIMITED:   return "deny: rate limited";
    case WUBU_SDR_DENY_BAD_ARGUMENT:   return "deny: bad argument";
    default:                           return "deny: unknown";
    }
}

wubu_sdr_decision_t wubu_sdr_relay(const wubu_sdr_table_t *t,
                                   const wubu_sdr_addr_t *from,
                                   const wubu_sdr_addr_t *to,
                                   const wubu_sdr_ticket_t *ticket,
                                   uint64_t now)
{
    if (!t || !from || !to) return WUBU_SDR_DENY_BAD_ARGUMENT;

    /* 1. the destination must be known */
    const wubu_sdr_route_t *r = wubu_sdr_table_find(t, to);
    if (!r) return WUBU_SDR_DENY_NO_ROUTE;

    /* 2. ... and ready.  Forwarding into a not-ready route black-holes the
     * packet, which is a denial of service on the SENDER, not just the
     * recipient -- so it is refused here. */
    if (!r->ready) return WUBU_SDR_DENY_NOT_READY;

    /* 3. a ticket is mandatory */
    if (!ticket) return WUBU_SDR_DENY_NO_TICKET;

    /* 4. and it must be valid right now */
    if (!wubu_sdr_ticket_is_valid(ticket, now))
        return WUBU_SDR_DENY_TICKET_EXPIRED;

    /* 5. and it must authorize exactly this connection */
    if (!wubu_sdr_ticket_matches(ticket, from, to))
        return WUBU_SDR_DENY_TICKET_MISMATCH;

    /* 6. never loop a packet back to its sender */
    if (from->vip == to->vip && from->vport == to->vport)
        return WUBU_SDR_DENY_LOOPBACK;

    return WUBU_SDR_FORWARD;
}

void wubu_sdr_rate_init(wubu_sdr_rate_limit_t *rl, uint32_t rate_per_sec,
                        uint32_t burst, uint64_t now_ms)
{
    if (!rl) return;
    memset(rl, 0, sizeof(*rl));
    rl->rate_per_sec = rate_per_sec;
    rl->burst = burst;
    /* the bucket starts FULL: an idle sender has not spent anything */
    rl->tokens_milli = (uint32_t)burst * 1000u;
    rl->last_ms = now_ms;
}

bool wubu_sdr_rate_allow(wubu_sdr_rate_limit_t *rl, uint64_t now_ms)
{
    if (!rl) return false;
    if (rl->rate_per_sec == 0) return false;      /* zero rate means blocked */

    /* refill for the elapsed time, in integer milli-tokens */
    if (now_ms > rl->last_ms) {
        uint64_t elapsed_ms = now_ms - rl->last_ms;
        /* tokens_milli is in MILLI-tokens and one packet costs 1000, so a rate
         * of N packets/second accrues N milli-tokens per MILLISECOND -- not
         * N/1000.  Dividing by 1000 made a 10/sec bucket refill 1 packet per
         * second instead of per 100ms. */
        uint64_t add = elapsed_ms * (uint64_t)rl->rate_per_sec;
        uint64_t cap = (uint64_t)rl->burst * 1000u;
        uint64_t t = (uint64_t)rl->tokens_milli + add;
        if (t > cap) t = cap;
        rl->tokens_milli = (uint32_t)t;
        rl->last_ms = now_ms;
    }

    if (rl->tokens_milli < 1000u) return false;    /* a packet costs 1000 */
    rl->tokens_milli -= 1000u;
    return true;
}

uint32_t wubu_sdr_rate_tokens(const wubu_sdr_rate_limit_t *rl)
{
    return rl ? rl->tokens_milli : 0;
}
