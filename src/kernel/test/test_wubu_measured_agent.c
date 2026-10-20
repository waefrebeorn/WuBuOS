/*
 * test_wubu_measured_agent.c -- asserts the agent self-integrity gate
 * (AGIEDR #44): baseline lock from the measured boot, checkpoint chain,
 * tamper detection, fail-closed promotion.
 * Hosted build: kernel attest + sha256 + the module under test.
 */
#include "wubu_measured_agent.h"
#include "wubu_attest.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

int main(void)
{
    /* ---- no root of trust: everything fails closed ---- */
    wubu_attest_clear();
    wubu_measured_agent_reset();
    CHECK(wubu_measured_agent_init() == -1);
    CHECK(wubu_measured_agent_ready() == 0);
    CHECK(wubu_measured_agent_may_selfmodify() == 0);
    CHECK(wubu_measured_agent_verify() == -2);
    CHECK(wubu_measured_agent_checkpoint("x", 1) == -1);

    /* ---- craft a valid measured-boot snapshot + ingest it ---- */
    wubu_attest_t snap;
    memset(&snap, 0, sizeof(snap));
    snap.magic = WUBU_AGI_MAGIC;
    snap.version = WUBU_AGI_ATTEST_VERSION;
    snap.sb_enabled = 1;
    snap.pcr_count = WUBU_AGI_PCR_COUNT;
    snap.boot_counter = 7;
    for (int i = 0; i < 32; i++) snap.pcr[4][i] = (uint8_t)(0x40 + i);
    CHECK(wubu_attest_ingest(&snap) == 0);
    CHECK(wubu_attest_valid());

    /* ---- baseline lock ---- */
    CHECK(wubu_measured_agent_init() == 0);
    CHECK(wubu_measured_agent_ready() == 1);
    uint8_t base[WUBU_MA_PCR_SZ];
    CHECK(wubu_measured_agent_baseline(base) == 0);
    CHECK(memcmp(base, snap.pcr[4], 32) == 0);   /* the measured PCR4 */
    CHECK(wubu_measured_agent_init() == -2);     /* one lock per boot */
    CHECK(wubu_measured_agent_checkpoint_count() == 0);
    CHECK(wubu_measured_agent_verify() == 0);    /* origin = zero PCR */
    CHECK(wubu_measured_agent_may_selfmodify() == 1);

    /* ---- checkpoints chain into the runtime PCR ---- */
    CHECK(wubu_measured_agent_checkpoint("state-A", 7) == 1);
    CHECK(wubu_measured_agent_checkpoint("state-B", 7) == 2);
    CHECK(wubu_measured_agent_checkpoint_count() == 2);
    CHECK(wubu_measured_agent_verify() == 0);
    CHECK(wubu_measured_agent_may_selfmodify() == 1);
    /* the runtime PCR actually moved off zero */
    uint8_t rt[WUBU_MA_PCR_SZ];
    CHECK(wubu_attest_runtime_pcr(rt) == 0);
    uint8_t zeros[32]; memset(zeros, 0, sizeof(zeros));
    CHECK(memcmp(rt, zeros, 32) != 0);

    /* ---- tamper: an extend we did not authorize ---- */
    CHECK(wubu_attest_extend_runtime("evil", 4) == 0);
    CHECK(wubu_measured_agent_verify() == -1);
    CHECK(wubu_measured_agent_may_selfmodify() == 0);
    /* a fresh authorized checkpoint re-locks the chain from here */
    CHECK(wubu_measured_agent_checkpoint("state-C", 7) == 3);
    CHECK(wubu_measured_agent_verify() == 0);
    CHECK(wubu_measured_agent_may_selfmodify() == 1);

    /* ---- root of trust dies mid-flight: promotion refuses ---- */
    wubu_attest_clear();
    CHECK(wubu_measured_agent_may_selfmodify() == 0);
    CHECK(wubu_measured_agent_verify() == -2);  /* unreadable, not broken */
    CHECK(wubu_attest_ingest(&snap) == 0);      /* restore trust */
    CHECK(wubu_measured_agent_verify() == 0);   /* chain itself intact */
    CHECK(wubu_measured_agent_may_selfmodify() == 1);

    /* ---- bad args ---- */
    CHECK(wubu_measured_agent_checkpoint(NULL, 4) == 0);
    CHECK(wubu_measured_agent_checkpoint("x", 0) == 0);

    /* ---- reset ---- */
    wubu_measured_agent_reset();
    CHECK(wubu_measured_agent_ready() == 0);
    CHECK(wubu_measured_agent_verify() == -2);

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
