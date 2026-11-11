/*
 * wubu_sdr_relay_test.c -- the relay decision and the rate limiter.
 *
 * The load-bearing rule: a relay is an access-control device, so EVERY check
 * must pass and the FIRST failure decides.  There is no "allow by default".
 */
#include "wubu_sdr_relay.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static void ticket(wubu_sdr_ticket_t *t, uint64_t now)
{
    memset(t, 0, sizeof(*t));
    t->pop_id = 0x1234;
    t->src.vip = 0x0A000001u; t->src.vport = 27015;
    t->dst.vip = 0x0A000002u; t->dst.vport = 27016;
    t->issued_at = now > 100 ? now - 100 : 0;
    t->expires_at = t->issued_at + 1000;
}

int main(void)
{
    printf("=== test_wubu_sdr_relay ===\n");
    wubu_sdr_table_t tab;
    wubu_sdr_table_init(&tab);

    wubu_sdr_addr_t from = { 0x0A000001u, 27015 };
    wubu_sdr_addr_t to   = { 0x0A000002u, 27016 };
    uint64_t now = 5000;

    wubu_sdr_route_t r;
    memset(&r, 0, sizeof(r));
    r.addr = to; r.pop_id = 1; r.peer_id = 2; r.ready = false;
    wubu_sdr_table_add(&tab, &r);

    wubu_sdr_ticket_t t;
    ticket(&t, now);

    /* -- fail closed, in order -- */
    CHECK(wubu_sdr_relay(&tab, &from, &to, &t, now) == WUBU_SDR_DENY_NOT_READY,
          "a not-ready route is refused (forwarding would black-hole the packet)");

    wubu_sdr_table_set_ready(&tab, &to, true);
    CHECK(wubu_sdr_relay(&tab, &from, &to, &t, now) == WUBU_SDR_FORWARD,
          "with a ready route and a valid ticket, the packet is FORWARDED");

    CHECK(wubu_sdr_relay(&tab, &from, &to, NULL, now) == WUBU_SDR_DENY_NO_TICKET,
          "no ticket is refused");
    CHECK(strcmp(wubu_sdr_decision_name(WUBU_SDR_DENY_NO_TICKET),
                 "deny: no ticket") == 0, "the decision has a readable name");

    /* -- no route -- */
    {
        wubu_sdr_addr_t unknown = { 0x0A999999u, 1 };
        CHECK(wubu_sdr_relay(&tab, &from, &unknown, &t, now)
              == WUBU_SDR_DENY_NO_ROUTE,
              "an unknown destination is refused before the ticket is even read");
    }

    /* -- expiry -- */
    {
        wubu_sdr_ticket_t old = t;
        old.issued_at = now - 2000;
        old.expires_at = now - 1000;
        CHECK(wubu_sdr_relay(&tab, &from, &to, &old, now)
              == WUBU_SDR_DENY_TICKET_EXPIRED, "an expired ticket is refused");
        wubu_sdr_ticket_t future = t;
        future.issued_at = now + 100;
        future.expires_at = now + 2000;
        CHECK(wubu_sdr_relay(&tab, &from, &to, &future, now)
              == WUBU_SDR_DENY_TICKET_EXPIRED,
              "a ticket that is not valid YET is refused");
    }

    /* -- the ticket must match BOTH addresses -- */
    {
        /* the destination must HAVE a route, or the relay refuses at the
         * route check -- before the ticket is even read */
        wubu_sdr_addr_t wrong_dst = { 0x0A000003u, 27016 };
        wubu_sdr_route_t wr;
        memset(&wr, 0, sizeof(wr));
        wr.addr = wrong_dst; wr.pop_id = 1; wr.peer_id = 3; wr.ready = true;
        wubu_sdr_table_add(&tab, &wr);
        CHECK(wubu_sdr_relay(&tab, &from, &wrong_dst, &t, now)
              == WUBU_SDR_DENY_TICKET_MISMATCH,
              "a ticket for a different destination does not authorize this one");
        wubu_sdr_ticket_t other = t;
        other.dst.vip = 0x0A000009u;
        CHECK(wubu_sdr_relay(&tab, &from, &to, &other, now)
              == WUBU_SDR_DENY_TICKET_MISMATCH,
              "a ticket whose DESTINATION differs does not authorize this one");
        wubu_sdr_ticket_t other_src = t;
        other_src.src.vip = 0x0A000009u;
        CHECK(wubu_sdr_relay(&tab, &from, &to, &other_src, now)
              == WUBU_SDR_DENY_TICKET_MISMATCH,
              "a ticket whose SOURCE differs does not authorize this one");
    }

    /* -- loopback -- */
    {
        wubu_sdr_route_t self;
        memset(&self, 0, sizeof(self));
        self.addr = from; self.pop_id = 1; self.peer_id = 1; self.ready = true;
        wubu_sdr_table_add(&tab, &self);
        wubu_sdr_ticket_t lt;
        ticket(&lt, now);
        lt.src = from; lt.dst = from;
        CHECK(wubu_sdr_relay(&tab, &from, &from, &lt, now)
              == WUBU_SDR_DENY_LOOPBACK,
              "a packet to its own sender is refused (a relay must not loop)");
    }

    /* -- bad arguments -- */
    CHECK(wubu_sdr_relay(NULL, &from, &to, &t, now) == WUBU_SDR_DENY_BAD_ARGUMENT,
          "a null table is refused");
    CHECK(wubu_sdr_relay(&tab, NULL, &to, &t, now) == WUBU_SDR_DENY_BAD_ARGUMENT,
          "a null source is refused");
    CHECK(wubu_sdr_relay(&tab, &from, NULL, &t, now) == WUBU_SDR_DENY_BAD_ARGUMENT,
          "a null destination is refused");
    CHECK(strcmp(wubu_sdr_decision_name((wubu_sdr_decision_t)999),
                 "deny: unknown") == 0, "an unknown decision has a name");

    /* -- the rate limiter: a token bucket, integer only -- */
    {
        wubu_sdr_rate_limit_t rl;
        wubu_sdr_rate_init(&rl, 10, 5, 0);       /* 10/sec, burst 5 */
        CHECK(wubu_sdr_rate_tokens(&rl) == 5000, "the bucket starts FULL (5 packets)");
        CHECK(wubu_sdr_rate_allow(&rl, 0), "packet 1 allowed");
        CHECK(wubu_sdr_rate_allow(&rl, 0), "packet 2 allowed");
        CHECK(wubu_sdr_rate_allow(&rl, 0), "packet 3 allowed");
        CHECK(wubu_sdr_rate_allow(&rl, 0), "packet 4 allowed");
        CHECK(wubu_sdr_rate_allow(&rl, 0), "packet 5 allowed");
        CHECK(!wubu_sdr_rate_allow(&rl, 0),
              "packet 6 is REFUSED: the burst is spent");
        CHECK(wubu_sdr_rate_tokens(&rl) == 0, "the bucket is empty");

        /* 100ms at 10/sec = 1 packet */
        CHECK(wubu_sdr_rate_allow(&rl, 100), "after 100ms one packet is available");
        CHECK(!wubu_sdr_rate_allow(&rl, 100), "but only one");
        /* 500ms at 10/sec = 5 packets, capped at the burst */
        CHECK(wubu_sdr_rate_allow(&rl, 600), "after 500ms more are available");
        CHECK(wubu_sdr_rate_tokens(&rl) <= 5000, "the bucket never exceeds its burst");

        /* a zero rate blocks everything */
        wubu_sdr_rate_limit_t zero;
        wubu_sdr_rate_init(&zero, 0, 5, 0);
        CHECK(!wubu_sdr_rate_allow(&zero, 0), "a zero rate blocks all packets");

        CHECK(!wubu_sdr_rate_allow(NULL, 0), "a null limiter refuses");
        CHECK(wubu_sdr_rate_tokens(NULL) == 0, "a null limiter has no tokens");
        wubu_sdr_rate_init(NULL, 1, 1, 0);
    }

    if (failures == 0) printf("ALL WUBU_SDR_RELAY TESTS PASSED\n");
    else printf("%d WUBU_SDR_RELAY FAILURES\n", failures);
    return failures ? 1 : 0;
}
