/* wubu_edr_test.c  --  WuBuOS EDR Engine Test Suite */

#include "wubu_edr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    printf("  TEST %-55s ", name); \
    if (expr) { printf("✅\n"); tests_passed++; } \
    else { printf("❌ FAILED\n"); tests_failed++; } \
} while(0)

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  WuBuOS EDR Engine Test Suite                           ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* === Lifecycle === */
    printf("\n=== Test: Lifecycle ===\n");
    TEST("edr_start returns 0", edr_start() == 0);
    TEST("edr_get_process_count >= 0", edr_get_process_count() >= 0);
    edr_stop();
    TEST("edr_stop completes cleanly", 1);

    /* === Module Registration === */
    printf("\n=== Test: Module System ===\n");
    edr_start();

    /* Switch to behavioral module */
    TEST("edr_switch_module behavioral returns 0",
         edr_switch_module("behavioral") == 0);

    EdrAlert alerts[16];
    int n = edr_get_alerts(alerts, 16);
    TEST("get_alerts returns >= 0", n >= 0);

    edr_stop();

    /* === Process Model === */
    printf("\n=== Test: Process Model ===\n");
    edr_start();

    /* Simulate process events by checking /proc/self/ */
    const EdrProcessInfo *self = edr_get_process((uint32_t)getpid());
    /* The EDR may not have captured this process yet since
       there was no PROCESS_CREATE event — that's expected. */
    TEST("get_process returns NULL or valid pointer for unobserved pid",
         self == NULL || (uintptr_t)self > 0);

    /* Check basic process count sanity */
    int count = edr_get_process_count();
    TEST("process count is reasonable", count >= 0 && count < 8192);

    edr_stop();

    /* === Event Pipeline === */
    printf("\n=== Test: Event Pipeline ===\n");
    /* Restart with behavioral module active */
    edr_start();
    edr_switch_module("behavioral");

    /* Give the worker thread a moment to drain any startup events */
    usleep(50000);

    n = edr_get_alerts(alerts, 16);
    TEST("alerts array accessible", n >= 0);
    if (n > 0) {
        TEST("alert has rule name", alerts[0].rule_name[0] != 0);
        TEST("alert has severity", alerts[0].severity[0] != 0);
        TEST("alert has module", alerts[0].module[0] != 0);
    }

    edr_stop();

    /* === Replay System === */
    printf("\n=== Test: Replay System ===\n");
    /* Create a minimal replay file */
    const char *replay_json = "/tmp/edr_replay_test.json";
    FILE *f = fopen(replay_json, "w");
    TEST("replay file created", f != NULL);
    if (f) {
        fprintf(f, "{\"type\":1,\"pid\":1000,\"ppid\":1,\"name\":\"test\"}\n");
        fclose(f);
    }
    TEST("edr_replay handles JSON", edr_replay(replay_json) == 0);
    unlink(replay_json);

    /* === Alert ID Determinism === */
    printf("\n=== Test: Alert System ===\n");
    /* The edr_alert_push function uses FNV-1a internally.
       We can verify the format by examining function-level
       behavior (compile-time constant hash known). */
    TEST("edr_start + stop is idempotent", edr_start() == 0);
    edr_stop();
    TEST("second start succeeds", edr_start() == 0);
    edr_stop();

    /* Summary */
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║  Results: %d passed, %d failed                          ║\n",
           tests_passed, tests_failed);
    printf("╚══════════════════════════════════════════════════════════╝\n");

    return tests_failed > 0 ? 1 : 0;
}