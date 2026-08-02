/*
 * test_agi_kernel.c -- Verify the WuBuOS bare-metal AGI kernel supervisor
 * runs correctly (hosted harness; the same code runs freestanding on metal).
 *
 * Proves (no display, no QEMU needed):
 *   1. wubu_agi_kernel_init decomposes the viewport via GAAD (regions > 0).
 *   2. The WuBuFW attestation is consumed: valid firmware attestation makes
 *      the supervisor attest_valid; the root-of-trust state is recorded as
 *      an immutable SUPER trace span (PCR4 + kernel digest).
 *   3. ROOT-OF-TRUST GATE: without a live attestation, the self-improve
 *      cycle REFUSES to promote even with an independent verifier wired.
 *   4. With attestation live, the independent-verifier cycle PROMOTES spans
 *      the verifier signs off on; freeze/unfreeze block/restore promotion.
 *   5. The PIT tick advances uptime and runs cycles.
 */
#include "wubu_agi_kernel.h"
#include "wubu_attest.h"
#include "wubu_bonzi.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { fails++; printf("FAIL: %s\n", m); } \
                       else printf("  ok: %s\n", m); } while (0)

/* Independent verifier (DA-3): a STRONG model overseer, never the agent.
 * Promotes only spans that contain "propose" (a real self-mod proposal) and
 * scores them >= 0.5. Rejects plain agent steps (they are not changes). */
static float my_verifier(const char *payload, uint64_t ts_ms,
                          void *ud, bool *passed)
{
    (void)ts_ms; (void)ud;
    *passed = (strstr(payload, "propose") != NULL);
    return *passed ? 0.9f : 0.1f;
}

/* Build a well-formed attestation snapshot (firmware-layout identical). */
static void make_attest(wubu_attest_t *a, uint32_t boot_count, int sb_enabled)
{
    memset(a, 0, sizeof(*a));
    a->magic        = WUBU_AGI_MAGIC;
    a->version      = WUBU_AGI_ATTEST_VERSION;
    a->sb_enabled   = sb_enabled;
    a->sb_setup_mode = 1;
    a->pcr_count    = WUBU_AGI_PCR_COUNT;
    a->boot_counter = boot_count;
    /* PCR4 = boot image digest: give it a recognizable pattern. */
    for (int i = 0; i < WUBU_AGI_PCR_SZ; i++)
        a->pcr[4][i] = (uint8_t)(0xA0 + i);
}

/* Build a loader handoff block (as the WuBuFW loader lays it out). */
static void make_handoff(wubu_loader_handoff_t *h, const wubu_attest_t *a,
                         uint32_t kernel_size)
{
    memset(h, 0, sizeof(*h));
    h->magic       = WUBU_LOADER_HANDOFF_MAGIC;
    h->version     = 1;
    h->kernel_size = kernel_size;
    for (int i = 0; i < WUBU_AGI_PCR_SZ; i++)
        h->kernel_sha256[i] = (uint8_t)(0x10 + i);
    h->attest_addr = (uint64_t)(uintptr_t)a;
}

int main(void)
{
    printf("=== WuBuOS Bare-Metal AGI Kernel ===\n");

    /* ---- Attestation module in isolation ---- */
    wubu_attest_t att;
    make_attest(&att, 7, 1);
    CHECK(wubu_attest_ingest(&att) == 0, "ingest well-formed attestation");
    CHECK(wubu_attest_valid(), "attestation valid after ingest");
    CHECK(wubu_attest_boot_counter() == 7, "boot counter read back");
    CHECK(wubu_attest_sb_enabled(), "secure boot flag read back");
    uint8_t p4[32];
    CHECK(wubu_attest_pcr4_digest(p4) == 0 && p4[0] == 0xA0 && p4[31] == 0xBF,
          "PCR4 digest (code-as-data) copied");
    CHECK(wubu_attest_ingest((const void *)"garbage") == -1,
          "garbage ingest rejected (bad magic)");
    CHECK(!wubu_attest_valid(), "invalid ingest leaves attestation invalid");

    /* ---- Root-of-trust gate: no attestation => no promotion ---- */
    wubu_attest_clear();
    wubu_agi_kernel_t *k = wubu_agi_kernel_init(1920, 1080);
    CHECK(k != NULL, "agi kernel initialized");
    CHECK(wubu_agi_kernel_region_count(k) > 0, "GAAD viewport decomposed (regions > 0)");
    CHECK(!wubu_agi_kernel_attest_valid(k), "no attestation => attest invalid");
    wubu_agi_kernel_set_verifier(k, my_verifier, NULL);
    wubu_agi_kernel_agent_emit(k, 0, "agent.propose: change while unmeasured");
    CHECK(wubu_agi_kernel_cycle(k) == 0,
          "root-of-trust gate: 0 promotions WITHOUT firmware attestation");

    /* ---- With live attestation: promotion works ---- */
    wubu_loader_handoff_t hd;
    make_handoff(&hd, &att, 1310720);
    CHECK(wubu_attest_ingest_handoff(&hd) == 0,
          "ingest loader handoff (attest + kernel digest)");
    CHECK(wubu_attest_kernel_size() == 1310720, "kernel size from handoff");
    CHECK(wubu_attest_kernel_digest(p4) == 0 && p4[0] == 0x10, "kernel digest from handoff");

    k = wubu_agi_kernel_init(1920, 1080);   /* re-capture attestation at init */
    CHECK(wubu_agi_kernel_attest_valid(k), "attestation valid at init");

    /* The init SUPER span must record the root-of-trust state. */
    char sp[WUBU_AGI_SPAN_DATA];
    CHECK(wubu_agi_kernel_span_data(k, 0, sp, sizeof(sp)) == 0 &&
          strstr(sp, "attest: valid") != NULL &&
          strstr(sp, "pcr4=") != NULL,
          "SUPER span records attestation + PCR4 at boot");
    CHECK(strstr(sp, "boot=7") != NULL, "SUPER span records boot counter");

    /* No verifier => safe default: cycle promotes nothing. */
    CHECK(wubu_agi_kernel_cycle(k) == 0, "no verifier => 0 promotions (DA-3 safe)");

    wubu_agi_kernel_set_verifier(k, my_verifier, NULL);

    /* Simulate the agent realm emitting spans (what agi_agent_task does). */
    wubu_agi_kernel_agent_emit(k, 0, "agent.step=0 regions=64 viewport=1920x1080");
    wubu_agi_kernel_agent_emit(k, 0, "agent.step=1 regions=64 viewport=1920x1080");
    wubu_agi_kernel_agent_emit(k, 0, "agent.propose: tune agent cadence to regions=64");
    CHECK(wubu_agi_kernel_trace_count(k) >= 3, "agent emitted >=3 trace spans");

    /* Run a cycle: only the "propose" span should be promoted. */
    int promoted = wubu_agi_kernel_cycle(k);
    CHECK(promoted == 1, "independent verifier promoted exactly 1 self-mod span");
    CHECK(wubu_agi_kernel_promoted_total(k) == 1, "promoted_total == 1");

    /* Freeze blocks further promotion. */
    wubu_agi_kernel_freeze(k, true);
    CHECK(wubu_agi_kernel_is_frozen(k), "kernel reports frozen");
    wubu_agi_kernel_agent_emit(k, 0, "agent.propose: another change");
    CHECK(wubu_agi_kernel_cycle(k) == 0, "frozen => 0 promotions");
    CHECK(wubu_agi_kernel_promoted_total(k) == 1, "promoted_total unchanged while frozen");

    /* Unfreeze restores promotion. */
    wubu_agi_kernel_freeze(k, false);
    CHECK(wubu_agi_kernel_cycle(k) == 1, "unfrozen => promotes the queued proposal");
    CHECK(wubu_agi_kernel_promoted_total(k) == 2, "promoted_total == 2");

    /* PIT tick advances uptime + runs cycles. */
    uint64_t before = wubu_agi_kernel_uptime_ms(k);
    wubu_agi_kernel_tick(k);
    CHECK(wubu_agi_kernel_uptime_ms(k) > before, "PIT tick advances uptime");

    /* ---- Bonzi Buddy loop (bare-metal human interface) ---- */
    wubu_bonzi_t *bz = wubu_bonzi_init(k);
    CHECK(bz != NULL, "bonzi init (gorilla + console drawn)");
    CHECK(wubu_bonzi_handle_line(bz, "hello") == 1, "bonzi handles 'hello'");
    CHECK(strstr(wubu_bonzi_last_reply(bz), "BONZI") != NULL, "bonzi greets");

    CHECK(wubu_bonzi_handle_line(bz, "freeze") == 1, "bonzi dispatches 'freeze'");
    CHECK(wubu_agi_kernel_is_frozen(k), "freeze reached the real supervisor");
    CHECK(wubu_bonzi_handle_line(bz, "unfreeze") == 1, "bonzi dispatches 'unfreeze'");
    CHECK(!wubu_agi_kernel_is_frozen(k), "unfreeze reached the real supervisor");

    CHECK(wubu_bonzi_handle_line(bz, "attest") == 1, "bonzi dispatches 'attest'");
    CHECK(strstr(wubu_bonzi_last_reply(bz), "PCR4=") != NULL,
          "attest replies with PCR4 digest");
    CHECK(wubu_bonzi_handle_line(bz, "pcr 4") == 1, "bonzi dispatches 'pcr 4'");
    CHECK(strstr(wubu_bonzi_last_reply(bz), "PCR4=") != NULL,
          "pcr 4 replies with the PCR4 digest");
    CHECK(wubu_bonzi_handle_line(bz, "status") == 1, "bonzi dispatches 'status'");
    CHECK(strstr(wubu_bonzi_last_reply(bz), "ATTEST=VALID") != NULL,
          "status reports the live firmware attestation");
    CHECK(wubu_bonzi_handle_line(bz, "promote") == 1, "bonzi dispatches 'promote'");
    CHECK(strstr(wubu_bonzi_last_reply(bz), "PROMOTED") != NULL,
          "promote ran a real DA-3 cycle");
    CHECK(wubu_bonzi_handle_line(bz, "help") == 1, "bonzi dispatches 'help'");
    CHECK(wubu_bonzi_handle_line(bz, "crack the cipher") == 1,
          "unknown intent still routes to the operator");
    CHECK(strstr(wubu_bonzi_last_reply(bz), "ROUTING") != NULL,
          "unknown line routed to the operator");

    /* Every bonzi interaction is a trace span (human loop feeds the AGI). */
    {
        char sp[WUBU_AGI_SPAN_DATA];
        int found = 0;
        int n = wubu_agi_kernel_trace_count(k);
        for (int i = 0; i < n; i++) {
            if (wubu_agi_kernel_span_data(k, i, sp, sizeof(sp)) == 0 &&
                strstr(sp, "bonzi.cmd=FREEZE") != NULL) { found = 1; break; }
        }
        CHECK(found, "bonzi interactions recorded as trace spans");
    }
    CHECK(wubu_bonzi_action_count(bz) == 9, "bonzi action count == 9");
    CHECK(wubu_bonzi_tick(bz) == 0, "bonzi tick with no keys does nothing");

    if (fails > 0) {
        printf("\n%d TEST(S) FAILED\n", fails);
        return 1;
    }
    printf("\nALL AGI KERNEL TESTS PASSED\n");
    return 0;
}
