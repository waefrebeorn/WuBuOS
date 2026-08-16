/*
 * test_gauntlet_runner.c — Main entry point for the WuBuOS Universal Test Gauntlet.
 *
 * Actually compiles each test via HolyC, executes it, checks the result.
 * C18 pure. Self-hosted capable.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "wubu_test_gauntlet.h"
#include "wubu_isa_driver.h"

/* HolyC compiler API */
extern int64_t hc_eval(const char *source);

/* Target names */
static const char *target_names[] = {
    "x86-64", "8086", "m68k", "6502", "riscv", "z80",
    "arm64", "mips", "8051", "avr", "pic", "amdgpu", "ptx", "wasm"
};
#define N_TARGETS (sizeof(target_names) / sizeof(target_names[0]))

/* Compile + run a test on x86-64 (native JIT) */
static int run_test_x86_64(const char *source, int64_t expected, test_result_t *result, int64_t *actual) {
    *actual = hc_eval(source);
    *result = (*actual == expected) ? TEST_PASS : TEST_FAIL;
    return 0;
}

/* For non-native targets, use the ISA driver directly */
static int run_test_isa_driver(const char *source, const char *target, int64_t expected, test_result_t *result, int64_t *actual) {
    /* Non-native ISAs use the interpreter: HolyC is parsed and evaluated
     * on x86-64 first (for the expected value), then the same source is
     * compiled to each ISA driver's machine code and interpreted.
     * Currently only x86-64 has a real backend; other drivers are wired
     * but pending MIR bridge integration. */
    const wubu_isa_driver_t *driver = wubu_isa_find(target);
    if (!driver) {
        *result = TEST_SKIP;
        *actual = 0;
        return 0;
    }
    /* For drivers without a compile+run path yet, skip */
    if (!driver->compile || !driver->run) {
        *result = TEST_SKIP;
        *actual = 0;
        return 0;
    }
    /* TODO: HolyC → MIR → driver->compile → driver->run */
    *result = TEST_SKIP;
    *actual = 0;
    return 0;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    gauntlet_state_t g;
    gauntlet_init(&g, target_names, N_TARGETS);

    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  WuBuOS Universal Test Gauntlet                         ║\n");
    printf("║  C18 pure · %d ISAs · %3u tests · %2u targets          ║\n",
           N_TARGETS,
           gauntlet_integer_test_count + gauntlet_control_test_count +
           gauntlet_bitwise_test_count + gauntlet_comparison_test_count +
           gauntlet_stress_test_count + gauntlet_memory_test_count,
           g.n_targets);
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* Collect all test suites */
    const test_entry_t *suites[] = {
        gauntlet_integer_tests, gauntlet_control_tests, gauntlet_bitwise_tests,
        gauntlet_comparison_tests, gauntlet_stress_tests, gauntlet_memory_tests,
    };
    const uint32_t counts[] = {
        gauntlet_integer_test_count, gauntlet_control_test_count,
        gauntlet_bitwise_test_count, gauntlet_comparison_test_count,
        gauntlet_stress_test_count, gauntlet_memory_test_count,
    };
    const uint32_t n_suites = sizeof(suites) / sizeof(suites[0]);

    g.n_tests = 0;
    for (uint32_t s = 0; s < n_suites; s++) g.n_tests += counts[s];

    printf("=== WuBuOS Test Gauntlet ===\n");
    printf("  Tests: %u\n", g.n_tests);
    printf("  Targets: %u\n\n", g.n_targets);

    uint32_t test_idx = 0;
    for (uint32_t s = 0; s < n_suites; s++) {
        for (uint32_t i = 0; i < counts[s]; i++) {
            const test_entry_t *t = &suites[s][i];
            printf("  [%3u/%3u] %-30s ", test_idx + 1, g.n_tests, t->name);
            fflush(stdout);

            test_result_t x86_result = TEST_SKIP;
            int64_t x86_actual = 0;

            for (uint32_t tgt = 0; tgt < g.n_targets; tgt++) {
                test_result_t result = TEST_SKIP;
                int64_t actual = 0;

                if (strcmp(target_names[tgt], "x86-64") == 0) {
                    run_test_x86_64(t->source, t->expected, &result, &actual);
                    x86_result = result;
                    x86_actual = actual;
                } else {
                    run_test_isa_driver(t->source, target_names[tgt], t->expected, &result, &actual);
                }

                /* Store result */
                g.results[test_idx * 16 + tgt].result = result;

                /* Update counts */
                if (result == TEST_PASS) {
                    g.pass_by_cat[__builtin_ctz(t->categories)]++;
                    g.pass_by_target[tgt]++;
                } else if (result == TEST_FAIL) {
                    g.fail_by_cat[__builtin_ctz(t->categories)]++;
                    g.fail_by_target[tgt]++;
                } else {
                    g.skip_by_cat[__builtin_ctz(t->categories)]++;
                }

                const char *sym = result == TEST_PASS ? "." :
                                  result == TEST_FAIL ? "F" :
                                  result == TEST_ERROR ? "E" : "S";
                printf("%s", sym);
            }

            /* Show details for failures — reuse result from target loop */
            if (x86_result == TEST_FAIL) {
                printf("  [FAIL: expected %lld, got %lld]", (long long)t->expected, (long long)x86_actual);
            } else if (x86_result == TEST_ERROR) {
                printf("  [ERROR: compilation failed]");
            }

            printf("\n");
            test_idx++;
        }
    }

    /* Print summary */
    gauntlet_print_summary(&g);

    /* Export CSV */
    gauntlet_export_csv(&g, "/tmp/gauntlet_results.csv");
    printf("\n  CSV exported to /tmp/gauntlet_results.csv\n");

    /* Return non-zero on failures */
    uint32_t total_fail = 0;
    for (int c = 0; c < 16; c++) total_fail += g.fail_by_cat[c];

    if (total_fail > 0) {
        printf("\n  ❌ %u test(s) FAILED\n", total_fail);
        return 1;
    }

    printf("\n  ✅ ALL TESTS PASSED\n");
    return 0;
}
