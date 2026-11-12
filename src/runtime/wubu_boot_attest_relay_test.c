/*
 * wubu_boot_attest_relay_test.c -- a boot report must reach every peer the
 * relay allows and none it does not.  The point under test is the SEAM: the
 * report cannot bypass the relay's rules, and it is offered to everyone
 * rather than to one address.
 */
#include "wubu_boot_attest_relay.h"
#include "wubu_boot_attest.h"

#include <stdio.h>
#include <string.h>

static int failures, checks;
#define CHECK(c, m) do { checks++; if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

/* --- a transmit that records who got what --------------------------- */

#define MAX_SEEN 8
static struct {
    uint64_t peer;
    size_t   len;
    uint8_t  frame[WUBU_BOOT_ATTEST_FRAME_SZ];
} g_seen[MAX_SEEN];
static int g_seen_n;

static int record_tx(void *ctx, uint64_t peer_id,
                     const uint8_t *frame, size_t len)
{
    (void)ctx;
    if (g_seen_n >= MAX_SEEN) return -1;
    g_seen[g_seen_n].peer = peer_id;
    g_seen[g_seen_n].len = len;
    memcpy(g_seen[g_seen_n].frame, frame, len);
    g_seen_n++;
    return 0;
}

static wubu_boot_attest_t *make_report(uint32_t boot_counter)
{
    uint8_t pcrs[WUBU_BOOT_ATTEST_PCR_COUNT][32];
    for (int i = 0; i < WUBU_BOOT_ATTEST_PCR_COUNT; i++)
        memset(pcrs[i], (uint8_t)i, 32);
    uint8_t pcr_digest[32], kernel_digest[32];
    wubu_boot_attest_pcr_digest(pcrs, pcr_digest);
    memset(kernel_digest, 0x11, 32);

    wubu_boot_attest_t *b = wubu_boot_attest_create();
    wubu_boot_attest_bind(b, boot_counter, pcr_digest, kernel_digest);
    wubu_boot_attest_stage(b, "firmware", 0);
    wubu_boot_attest_stage(b, "kernel", 120);
    wubu_boot_attest_seal(b, 1763000000u);
    return b;
}

int main(void)
{
    printf("=== test_wubu_boot_attest_relay ===\n");

    wubu_boot_attest_t *report = make_report(42);

    /* Three routes: two ready, one not. */
    wubu_sdr_table_t table;
    wubu_sdr_table_init(&table);
    wubu_sdr_route_t r1 = { { 10, 1000 }, 1, 501, true };
    wubu_sdr_route_t r2 = { { 10, 1001 }, 1, 502, true };
    wubu_sdr_route_t r3 = { { 10, 1002 }, 1, 503, false };   /* not ready */
    CHECK(wubu_sdr_table_add(&table, &r1) == 0, "route 1 is added");
    CHECK(wubu_sdr_table_add(&table, &r2) == 0, "route 2 is added");
    CHECK(wubu_sdr_table_add(&table, &r3) == 0, "route 3 is added");

    wubu_sdr_addr_t from = { 10, 900 };
    wubu_sdr_ticket_t t = { 1, { 10, 900 }, { 10, 1000 },
                            1763000000u, 1763003600u };
    wubu_sdr_ticket_t t2 = { 1, { 10, 900 }, { 10, 1001 },
                             1763000000u, 1763003600u };
    wubu_sdr_ticket_t t3 = { 1, { 10, 900 }, { 10, 1002 },
                             1763000000u, 1763003600u };
    wubu_sdr_ticket_t tbad = { 1, { 10, 900 }, { 10, 4000 },
                               1763000000u, 1763003600u };
    wubu_sdr_ticket_t texp = { 1, { 10, 900 }, { 10, 1000 },
                               1763000000u, 1763000001u };

    /* -- the happy path: every ready route gets the report -- */
    g_seen_n = 0;
    int n = wubu_boot_attest_publish(report, &table, &from, &t,
                                     1763000010u, record_tx, NULL);
    CHECK(n == 1, "a ticket for one peer offers to that peer");
    CHECK(g_seen_n == 1 && g_seen[0].peer == 501, "peer 501 is offered the report");
    CHECK(g_seen[0].len > 0, "the offered bytes are a frame");

    /* -- a ticket for each peer: both ready peers get it, the not-ready
         one is skipped by the relay before the transmit -- */
    g_seen_n = 0;
    n = wubu_boot_attest_publish(report, &table, &from, &t2,
                                 1763000010u, record_tx, NULL);
    CHECK(n == 1 && g_seen_n == 1 && g_seen[0].peer == 502,
          "peer 502 is offered the report");

    g_seen_n = 0;
    n = wubu_boot_attest_publish(report, &table, &from, &t3,
                                 1763000010u, record_tx, NULL);
    CHECK(n == 0 && g_seen_n == 0,
          "a NOT-READY route is skipped (the relay would black-hole it)");

    g_seen_n = 0;
    n = wubu_boot_attest_publish(report, &table, &from, &tbad,
                                 1763000010u, record_tx, NULL);
    CHECK(n == 0 && g_seen_n == 0,
          "a ticket for an unknown address is refused");

    g_seen_n = 0;
    n = wubu_boot_attest_publish(report, &table, &from, &texp,
                                 1763000010u, record_tx, NULL);
    CHECK(n == 0 && g_seen_n == 0,
          "an expired ticket is refused");

    g_seen_n = 0;
    n = wubu_boot_attest_publish(report, &table, &from, NULL,
                                 1763000010u, record_tx, NULL);
    CHECK(n == 0 && g_seen_n == 0,
          "no ticket at all is refused");

    /* -- what the peer receives is a verifiable report -- */
    CHECK(g_seen_n == 0, "nothing was sent without a ticket");
    g_seen_n = 0;
    n = wubu_boot_attest_publish(report, &table, &from, &t,
                                 1763000010u, record_tx, NULL);
    CHECK(n == 1, "the report is offered again");
    wubu_boot_attest_view_t v;
    CHECK(wubu_boot_attest_decode(g_seen[0].frame, g_seen[0].len, &v) == 0,
          "the peer decodes what the relay forwarded");
    CHECK(v.boot_counter == 42, "the boot counter is the one that was sealed");
    CHECK(v.stage_count == 2, "both boot stages arrived");
    CHECK(strcmp(v.stages[0].name, "firmware") == 0, "stage 0 arrived by name");
    CHECK(v.stages[1].started_ms == 120, "stage 1 arrived with its start time");

    /* -- a report that is not sealed cannot be published -- */
    wubu_boot_attest_t *open_report = wubu_boot_attest_create();
    g_seen_n = 0;
    CHECK(wubu_boot_attest_publish(open_report, &table, &from, &t,
                                   1763000010u, record_tx, NULL) == -1,
          "an unsealed report is not published");
    CHECK(g_seen_n == 0, "nothing was transmitted for an unsealed report");

    CHECK(wubu_boot_attest_publish(NULL, &table, &from, &t,
                                   1763000010u, record_tx, NULL) == -1,
          "a NULL report is refused");

    wubu_boot_attest_destroy(report);
    wubu_boot_attest_destroy(open_report);

    printf("=== %d checks, %d failed ===\n", checks, failures);
    return failures ? 1 : 0;
}
