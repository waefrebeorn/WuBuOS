/*
 * jit_branch_profile_test.c -- Verify the runtime branch-feedback subsystem
 * (Subsystem C): counters increment on the taken/not-taken paths, the taken
 * fraction drives the #14 layout decision (fallthrough hot when <50% taken),
 * and unknown branches default safely.
 *
 * DISCRIMINATORS:
 *   - a 100% taken branch => taken_fraction 1.0, fallthrough NOT hot
 *   - a 0% taken branch   => taken_fraction 0.0, fallthrough IS hot
 *   - a never-executed branch => -1, fallthrough defaults hot (safe current
 *     layout, never a wrong decision)
 *   - counters are monotonic (each call +1) and independent per branch
 */
#include "jit_branch_profile.h"
#include <stdio.h>

static int pass, fail;
#define CHECK(cond, msg) do { if (cond) pass++; else { fail++; printf("FAIL: %s\n", msg); } } while(0)

int main(void) {
    jbp_init(4);

    /* branch 0: simulate a mostly-taken branch (e.g. loop back-edge) */
    for (int i = 0; i < 100; i++) (*jbp_counter_taken(0))++;
    for (int i = 0; i < 10; i++)  (*jbp_counter_not_taken(0))++;
    CHECK(jbp_counter_taken(0)[0] == 100, "branch0 taken counter == 100");
    CHECK(jbp_counter_not_taken(0)[0] == 10, "branch0 not-taken counter == 10");
    CHECK(jbp_taken_fraction(0) > 0.90 && jbp_taken_fraction(0) <= 1.0,
          "branch0 taken fraction ~0.91");
    CHECK(jbp_fallthrough_is_hot(0) == 0, "branch0 (>50% taken) fallthrough NOT hot");

    /* branch 1: almost never taken (e.g. rare error check) */
    (*jbp_counter_taken(1))++;
    for (int i = 0; i < 100; i++) (*jbp_counter_not_taken(1))++;
    CHECK(jbp_fallthrough_is_hot(1) == 1, "branch1 (<50% taken) fallthrough IS hot");

    /* branch 2: never executed => unknown, defaults hot (safe) */
    CHECK(jbp_taken_fraction(2) == -1, "branch2 never executed => -1");
    CHECK(jbp_fallthrough_is_hot(2) == 1, "branch2 unknown defaults fallthrough hot");

    /* branch 3: exactly 50/50 => hot side is ambiguous, treat as hot */
    for (int i = 0; i < 5; i++) { (*jbp_counter_taken(3))++; (*jbp_counter_not_taken(3))++; }
    CHECK(jbp_taken_fraction(3) == 0.5, "branch3 50% taken");
    CHECK(jbp_fallthrough_is_hot(3) == 1, "branch3 at 50% defaults fallthrough hot");

    /* counters are independent per branch */
    CHECK(jbp_counter_taken(0)[0] == 100 && jbp_counter_taken(3)[0] == 5,
          "counters independent per branch");

    printf("=== jit_branch_profile_test: %d passed, %d failed ===\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
