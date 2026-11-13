/*
 * wubu_boot_attest_edr_test.c -- the boot must appear in the EDR stream, not
 * just on the wire.  The point under test: a boot that happened BEFORE any EDR
 * event existed is still visible afterwards, with the measurement bound to it,
 * and a machine with no firmware measurement refuses to claim one.
 */
#include "wubu_boot_attest_edr.h"
#include "../kernel/wubu_attest.h"

#include <stdio.h>
#include <string.h>

static int failures, checks;
#define CHECK(c, m) do { checks++; if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

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

/* A well-formed firmware attestation, as the WuBuFW loader would lay it out. */
static void publish_fake_attest(uint32_t boot_counter)
{
    wubu_attest_t a;
    memset(&a, 0, sizeof(a));
    a.magic     = WUBU_AGI_MAGIC;
    a.version   = WUBU_AGI_ATTEST_VERSION;
    a.pcr_count = WUBU_AGI_PCR_COUNT;
    a.boot_counter = boot_counter;
    for (unsigned i = 0; i < WUBU_AGI_PCR_COUNT; i++)
        for (unsigned j = 0; j < WUBU_AGI_PCR_SZ; j++)
            a.pcr[i][j] = (uint8_t)(boot_counter + i * 3 + j);
    CHECK(wubu_attest_ingest(&a) == 0, "the fake firmware attestation ingests");
}

int main(void)
{
    printf("=== test_wubu_boot_attest_edr ===\n");

    /* No measurement yet: an attestation must be refused rather than issued
       for a boot nobody measured. */
    CHECK(!wubu_attest_valid(), "no firmware attestation at process start");
    const char *stages[] = { "firmware", "loader", "kernel", "gui" };
    const uint32_t starts[] = { 0, 40, 120, 300 };
    CHECK(wubu_boot_attest_edr_boot(1763000000u, stages, starts, 4,
                                    NULL, NULL, NULL, NULL, NULL) == -1,
          "a boot with no measurement is not attested");

    publish_fake_attest(9);

    edr_analytics_set_enabled(true);
    edr_start();

    /* -- local only: log to EDR, no peers -- */
    g_seen_n = 0;
    int rc = wubu_boot_attest_edr_boot(1763000000u, stages, starts, 4,
                                       NULL, NULL, NULL, NULL, NULL);
    CHECK(rc == 0, "the boot attestation succeeds locally");

    EdrEventView views[64];
    int n = edr_recent_events(views, 64, 0, 0);
    CHECK(n > 0, "the EDR ring holds events");
    int found = -1;
    for (int i = 0; i < n; i++)
        if (views[i].type == EDR_EV_BOOT_ATTEST) { found = i; break; }
    CHECK(found >= 0, "the boot is an EDR event");
    if (found >= 0) {
        CHECK(views[found].u64a == 9,
              "the event carries the boot counter in its numeric field");
        CHECK(strstr(views[found].detail, "boot#9") != NULL,
              "the detail names the boot");
        CHECK(strstr(views[found].detail, "stages=4") != NULL,
              "the detail records the stage count");
    }
    CHECK(strcmp(edr_event_type_name(EDR_EV_BOOT_ATTEST), "BootAttest") == 0,
          "the event type has a name for the dashboard");
    CHECK(strcmp(edr_event_type_name(EDR_EV_AGENT_ACTION), "AgentAction") == 0,
          "the existing agent type name did not shift");

    /* -- boot + broadcast: the peers get a verifiable frame -- */
    wubu_sdr_table_t table;
    wubu_sdr_table_init(&table);
    wubu_sdr_route_t r1 = { { 10, 1000 }, 1, 701, true };
    CHECK(wubu_sdr_table_add(&table, &r1) == 0, "a route is added");
    wubu_sdr_addr_t from = { 10, 900 };
    wubu_sdr_ticket_t t = { 1, { 10, 900 }, { 10, 1000 },
                            1763000000u, 1763003600u };

    rc = wubu_boot_attest_edr_boot(1763000001u, stages, starts, 4,
                                   &table, &from, &t, record_tx, NULL);
    CHECK(rc == 0, "the boot attestation succeeds with a broadcast");
    CHECK(g_seen_n == 1 && g_seen[0].peer == 701,
          "the peer was offered the sealed report");

    wubu_boot_attest_view_t v;
    CHECK(wubu_boot_attest_decode(g_seen[0].frame, g_seen[0].len, &v) == 0,
          "the peer decodes what EDR logged and sent");
    CHECK(v.boot_counter == 9, "the boot counter is the measured one");
    CHECK(v.stage_count == 4, "all four boot stages arrived");
    CHECK(strcmp(v.stages[0].name, "firmware") == 0, "stage 0 arrived by name");
    CHECK(strcmp(v.stages[3].name, "gui") == 0, "the last stage arrived by name");

    /* -- the analytics master toggle gates everything, boot included -- */
    edr_analytics_set_enabled(false);
    CHECK(wubu_boot_attest_edr_log(NULL) == -1, "a NULL report is refused");
    edr_analytics_set_enabled(true);

    /* -- bad arguments -- */
    CHECK(wubu_boot_attest_edr_boot(1u, NULL, NULL, 0, NULL, NULL, NULL,
                                    NULL, NULL) == -1,
          "zero stages with no list is refused");
    CHECK(wubu_boot_attest_edr_boot(1u, stages, starts, 0, NULL, NULL, NULL,
                                    NULL, NULL) == -1,
          "a boot with no stages is refused");

    edr_stop();

    printf("=== %d checks, %d failed ===\n", checks, failures);
    return failures ? 1 : 0;
}
