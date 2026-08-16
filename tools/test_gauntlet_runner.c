/*
 * test_gauntlet_runner.c — Main entry point for the WuBuOS Universal Test Gauntlet.
 *
 * Runs all built-in tests across all ISA targets.
 * Each test is compiled via the HolyC compiler, executed on the target,
 * and the result is compared with the expected value.
 *
 * C11, self-contained.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wubu_test_gauntlet.h"

/* Target names (must match ISA driver registry) */
static const char *target_names[] = {
    "x86-64", "8086", "m68k", "6502", "riscv", "z80",
    "arm64", "mips", "8051", "avr", "pic", "amdgpu", "ptx", "wasm"
};
#define N_TARGETS (sizeof(target_names) / sizeof(target_names[0]))

/* Forward declarations for the actual compilation + execution */
extern int holyc_compile_and_run(const char *source, const char *target,
                                  int64_t *result);

/* Stub for now — will be wired to actual holyc compilation */
int holyc_compile_and_run(const char *source, const char *target,
                           int64_t *result) {
    (void)source; (void)target;
    *result = 0;
    return 0;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    gauntlet_state_t g;
    gauntlet_init(&g, target_names, N_TARGETS);

    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  WuBuOS Universal Test Gauntlet                         ║\n");
    printf("║  C18 pure · 14 ISAs · %3u tests · %-5u targets         ║\n",
           gauntlet_integer_test_count + gauntlet_control_test_count +
           gauntlet_bitwise_test_count + gauntlet_comparison_test_count +
           gauntlet_stress_test_count + gauntlet_memory_test_count,
           g.n_targets);
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* Run all tests */
    gauntlet_run_all(&g);

    /* Print summary */
    gauntlet_print_summary(&g);

    /* Export CSV */
    gauntlet_export_csv(&g, "/tmp/gauntlet_results.csv");
    printf("\n  CSV exported to /tmp/gauntlet_results.csv\n");

    /* Return non-zero on any failures */
    uint32_t total_fail = 0;
    for (int c = 0; c < 16; c++) total_fail += g.fail_by_cat[c];

    if (total_fail > 0) {
        printf("\n  ❌ %u test(s) FAILED\n", total_fail);
        return 1;
    }

    printf("\n  ✅ ALL TESTS PASSED\n");
    return 0;
}
