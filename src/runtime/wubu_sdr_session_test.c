/*
 * wubu_sdr_session_test.c -- the POP-side session layer.
 *
 * The load-bearing rule: a virtual port is NEVER reassigned while the old
 * session could still receive traffic -- only a REAPED slot is reusable, so
 * packets for the previous peer cannot be delivered to the new one.
 */
#include "wubu_sdr_session.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_wubu_sdr_session ===\n");
    wubu_sdr_sessions_t s;
    uint32_t port = 0;
    const wubu_sdr_session_t *sess = NULL;

    wubu_sdr_sessions_init(&s, 0x0A000000u, 27000);
    CHECK(wubu_sdr_sessions_alive(&s, 0) == 0, "no sessions at start");

    /* -- open -- */
    CHECK(wubu_sdr_sessions_open(&s, 1001, 1, &port, 1000) == 0,
          "a session opens");
    CHECK(port == 27000, "the first virtual port is the base");
    sess = wubu_sdr_sessions_find(&s, port);
    CHECK(sess != NULL, "it is found");
    CHECK(sess->peer_id == 1001 && sess->addr.vip == 0x0A000000u,
          "the peer and virtual address are recorded");
    CHECK(wubu_sdr_sessions_alive(&s, 1000) == 1, "one session is alive");

    /* -- idempotent -- */
    uint32_t again = 0;
    CHECK(wubu_sdr_sessions_open(&s, 1001, 1, &again, 2000) == 0,
          "re-opening for the same peer succeeds");
    CHECK(again == port,
          "and returns the SAME virtual port (a retry must not get a second)");
    CHECK(wubu_sdr_sessions_alive(&s, 2000) == 1, "still one session");

    /* -- keepalive -- */
    CHECK(wubu_sdr_session_is_alive(sess, 30000), "alive just inside the window");
    CHECK(wubu_sdr_session_is_alive(sess, 30999), "alive at 29.999s");
    CHECK(!wubu_sdr_session_is_alive(sess, 31000),
          "NOT alive once the keepalive window has elapsed (half-open)");
    CHECK(wubu_sdr_sessions_alive(&s, 31000) == 0, "nothing is alive");

    /* -- touch keeps it alive -- */
    CHECK(wubu_sdr_sessions_touch(&s, port, 30000) == 0, "traffic is recorded");
    CHECK(wubu_sdr_session_is_alive(sess, 30000), "still alive after traffic");
    CHECK(!wubu_sdr_session_is_alive(sess, 61000), "and expires again later");

    /* -- a peer on another POP is refused -- */
    uint32_t moved = 0;
    CHECK(wubu_sdr_sessions_open(&s, 1001, 2, &moved, 70000) == -1,
          "the same peer on a DIFFERENT POP is refused");

    /* -- reap -- */
    CHECK(wubu_sdr_sessions_reap(&s, 70000) == 1, "the expired session is reaped");
    CHECK(wubu_sdr_sessions_find(&s, port) == NULL, "its slot is gone");

    /* -- the port is now reusable, for a DIFFERENT peer -- */
    uint32_t port2 = 0;
    CHECK(wubu_sdr_sessions_open(&s, 2002, 1, &port2, 71000) == 0,
          "a new peer opens after the reap");
    CHECK(port2 == port,
          "and gets the freed port (reuse only AFTER the reap, never before)");

    /* -- a CLOSED session still holds its port until reaped -- */
    CHECK(wubu_sdr_sessions_close(&s, port2) == 0, "the session is closed");
    CHECK(wubu_sdr_sessions_find(&s, port2) != NULL,
          "a closed session still EXISTS until reaped");
    {
        uint32_t third = 0;
        CHECK(wubu_sdr_sessions_open(&s, 3003, 1, &third, 72000) == 0,
              "another peer opens");
        CHECK(third != port2,
              "and does NOT get the closed session's port (that would hijack it)");
    }

    /* -- closing twice is an ERROR -- */
    CHECK(wubu_sdr_sessions_close(&s, port2) == -1,
          "closing a closed session is an error, not a no-op");

    /* -- unknown sessions are ERRORS -- */
    CHECK(wubu_sdr_sessions_touch(&s, 65530, 80000) == -1,
          "touching an unknown port is an error");
    CHECK(wubu_sdr_sessions_close(&s, 65530) == -1,
          "closing an unknown port is an error");
    CHECK(wubu_sdr_sessions_find(&s, 65530) == NULL,
          "an unknown port is not found");

    /* -- the table is bounded -- */
    {
        wubu_sdr_sessions_t big;
        wubu_sdr_sessions_init(&big, 0x0A000000u, 40000);
        int opened = 0;
        for (uint64_t p = 1; p <= WUBU_SDR_MAX_SESSIONS + 10; p++) {
            uint32_t vp = 0;
            if (wubu_sdr_sessions_open(&big, 0x10000 + p, 1, &vp, 0) == 0) opened++;
        }
        CHECK(opened == WUBU_SDR_MAX_SESSIONS,
              "the table holds exactly its bound");
        uint32_t vp = 0;
        CHECK(wubu_sdr_sessions_open(&big, 0x999999, 1, &vp, 0) == -1,
              "a full table refuses new sessions");
    }

    /* -- keepalive of 0 means 'lives until closed' -- */
    {
        wubu_sdr_sessions_t k;
        wubu_sdr_sessions_init(&k, 0x0A000000u, 50000);
        uint32_t vp = 0;
        wubu_sdr_sessions_open(&k, 1, 1, &vp, 0);
        wubu_sdr_session_t *e = (wubu_sdr_session_t *)wubu_sdr_sessions_find(&k, vp);
        e->keepalive_ms = 0;                     /* no keepalive */
        CHECK(wubu_sdr_session_is_alive(e, 999999999u),
              "a session with no keepalive lives until it is closed");
        CHECK(wubu_sdr_sessions_close(&k, vp) == 0, "then it can be closed");
        CHECK(!wubu_sdr_session_is_alive(e, 1000000000u),
              "and a closed session is never alive");
    }

    /* -- null handling -- */
    wubu_sdr_sessions_init(NULL, 1, 1);
    CHECK(wubu_sdr_sessions_open(NULL, 1, 1, &port, 0) == -1, "a null table is refused");
    CHECK(wubu_sdr_sessions_find(NULL, 1) == NULL, "a null table finds nothing");
    CHECK(wubu_sdr_sessions_touch(NULL, 1, 0) == -1, "a null table cannot be touched");
    CHECK(wubu_sdr_sessions_close(NULL, 1) == -1, "a null table cannot be closed");
    CHECK(wubu_sdr_sessions_reap(NULL, 0) == 0, "a null table reaps nothing");
    CHECK(wubu_sdr_sessions_alive(NULL, 0) == 0, "a null table has nothing alive");
    CHECK(!wubu_sdr_session_is_alive(NULL, 0), "a null session is not alive");

    if (failures == 0) printf("ALL WUBU_SDR_SESSION TESTS PASSED\n");
    else printf("%d WUBU_SDR_SESSION FAILURES\n", failures);
    return failures ? 1 : 0;
}
