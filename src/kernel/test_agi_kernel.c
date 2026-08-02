/*
 * test_agi_kernel.c -- Verify the WuBuOS bare-metal AGI kernel supervisor
 * runs correctly (hosted harness; the same code runs freestanding on metal).
 *
 * Proves (no display, no QEMU needed):
 *   1. wubu_agi_kernel_init decomposes the viewport via GAAD (regions > 0).
 *   2. The agent realm emits AGENT trace spans into the append-only ring.
 *   3. The independent-verifier self-improve cycle PROMOTES spans the
 *      verifier signs off on, and REFUSES when frozen / no verifier (DA-3).
 *   4. Freeze blocks promotion; unfreeze restores it.
 *   5. The PIT tick advances uptime and runs cycles.
 */
#include "wubu_agi_kernel.h"
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

int main(void)
{
    printf("=== WuBuOS Bare-Metal AGI Kernel ===\n");

    wubu_agi_kernel_t *k = wubu_agi_kernel_init(1920, 1080);
    CHECK(k != NULL, "agi kernel initialized");
    CHECK(wubu_agi_kernel_region_count(k) > 0, "GAAD viewport decomposed (regions > 0)");
    printf("    regions = %d\n", wubu_agi_kernel_region_count(k));

    /* No verifier => safe default: cycle promotes nothing. */
    CHECK(wubu_agi_kernel_cycle(k) == 0, "no verifier => 0 promotions (DA-3 safe)");

    /* Wire the independent verifier. */
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

    if (fails > 0) {
        printf("\n%d TEST(S) FAILED\n", fails);
        return 1;
    }
    printf("\nALL AGI KERNEL TESTS PASSED\n");
    return 0;
}
