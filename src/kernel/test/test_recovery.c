/*
 * test_recovery.c -- host tests for the 5+1 recovery substrate.
 * Compiles the freestanding wubu_recovery.c on the host.
 */
#include <stdio.h>
#include <string.h>
#include "wubu_recovery.h"

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_recovery (5+1 rollback + Jesus state) ===\n");

    wubu_recovery_t r;
    wubu_recovery_principles_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    strcpy(p.identity, "wubuwizard-colonel");
    p.max_rollback_attempts = 5;
    p.jesus_armed = 1;
    p.human_gate_required = 1;
    p.growth_loop = 1;
    p.human_centric = 1;
    p.no_third_party = 1;
    p.no_stubs = 1;
    p.license_origin = 3;

    /* R1: init */
    CHECK(wubu_recovery_init(&r, &p) == 0, "init");
    CHECK(wubu_recovery_healthy(&r) == 1, "healthy after init");
    CHECK(r.principles.magic == WUBU_RECOVERY_PRINCIPLES_MAGIC, "principles magic");

    /* R2: five checkpoints fill the ring */
    char states[5][16];
    for (int i = 0; i < 5; i++) {
        snprintf(states[i], sizeof(states[i]), "state-%d", i);
        int s = wubu_recovery_checkpoint(&r, states[i], 8);
        CHECK(s == i, "checkpoint into slot");
    }
    CHECK(wubu_recovery_live(&r) == 5, "ring full (5 live)");

    /* R3: roll back to each slot */
    char out[16];
    for (int i = 0; i < 5; i++) {
        int n = wubu_recovery_rollback(&r, (uint32_t)i, out, sizeof(out));
        CHECK(n == 8 && memcmp(out, states[i], 8) == 0, "rollback exact");
    }

    /* the ring rotates: a 6th checkpoint overwrites slot 0 */
    snprintf(states[0], sizeof(states[0]), "state-5");
    CHECK(wubu_recovery_checkpoint(&r, states[0], 8) == 0, "ring rotates");
    CHECK(wubu_recovery_live(&r) == 5, "still 5 live");

    /* R6: integrity holds */
    for (int i = 0; i < 5; i++)
        CHECK(wubu_recovery_verify(&r, (uint32_t)i) == 1, "slot verifies");

    /* R7/R8/R9: telemetry */
    wubu_recovery_log(&r, 1, 0, r.seq);
    CHECK(wubu_recovery_healthy(&r) == 1, "healthy mid-flight");

    /* R10: containerized working set */
    uint32_t cid = 99;
    char ws[10] = "experiment";
    CHECK(wubu_recovery_container(&r, ws, 8, &cid) == 0, "containerized");
    CHECK(cid < WUBU_RECOVERY_SLOTS, "container id is a slot");

    /* R4: the Jesus state -- clean slate, divine good intact */
    char clean[16];
    wubu_recovery_principles_t divine;
    CHECK(wubu_recovery_jesus(&r, clean, sizeof(clean), &divine) == 0, "jesus fires");
    CHECK(r.jesus_used == 1, "jesus used");
    CHECK(wubu_recovery_live(&r) == 0, "clean slate (0 live)");
    CHECK(strcmp(divine.identity, "wubuwizard-colonel") == 0, "identity survives");
    CHECK(divine.human_centric == 1, "human-centric survives");
    CHECK(divine.no_third_party == 1, "no-third-party survives");
    CHECK(divine.growth_loop == 1, "growth loop survives");

    /* R5: disarming the Jesus state blocks the emergency */
    CHECK(wubu_recovery_arm_jesus(&r, 0) == 0, "disarm");
    CHECK(wubu_recovery_jesus(&r, clean, sizeof(clean), &divine) == -2, "jesus gated");

    /* the divine principles survive corruption of the ring */
    memset(&r.slots, 0xAA, sizeof(r.slots));
    CHECK(wubu_recovery_healthy(&r) == 1, "divine intact despite corruption");

    if (failures == 0) printf("ALL RECOVERY TESTS PASSED\n");
    else printf("%d RECOVERY FAILURES\n", failures);
    return failures ? 1 : 0;
}
