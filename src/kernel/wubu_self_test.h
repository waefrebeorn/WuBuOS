/*
 * wubu_self_test.h  --  kernel-resident self-test gate (gap G2)
 *
 * The verifier's doctrine is "test suite as verifier": the promotion
 * gate should consult the kernel's OWN integrity tests, not just the
 * static policy. This module runs a small, deterministic suite against
 * the live kernel structures (heap integrity, hive sanity, trace ring,
 * sync lock) and reports the pass rate. The verifier folds it into the
 * score; a failing suite caps the score below the threshold so the
 * self-improve loop refuses to promote on a sick kernel.
 *
 * Freestanding C11, no heap of its own.
 */
#ifndef WUBU_SELF_TEST_H
#define WUBU_SELF_TEST_H

#include <stdint.h>

/* Run the kernel-resident suite. Returns the number of checks that
 * passed; *total receives the number run. */
uint32_t wubu_self_test_run(uint32_t *total);

/* Convenience: 1 when every check passed (the verifier's gate). */
int wubu_self_test_ok(void);

#endif
