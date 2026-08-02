/*
 * wubu_verifier.h  --  WuBuOS Independent Verifier (DA-3 promotion gate)
 *
 * The first REAL verifier on metal: the Colonel's self-improve loop is
 * dormant (k->verifier == NULL => no promotion) until an independent
 * scorer is wired. This module scores agent spans against a fixed,
 * kernel-resident policy:
 *
 *   - well-formedness   (printable ASCII, sane length, no NUL tricks)
 *   - emitter trust     (payload must name a known emitter: bonzi/agent/
 *                        supervisor)
 *   - semantic budget   (known verbs + structure => higher score)
 *
 * A span passes when its score clears the threshold. Passing spans are
 * promoted (promoted_total) -- the loop becomes LIVE and observable.
 *
 * This is the seed of the test-suite-as-verifier doctrine: the final
 * verifier runs the kernel's own test gates; today's static policy is the
 * honest first step (a real, independent, deterministic scorer).
 *
 * Freestanding: no malloc, no hosted APIs.
 */
#ifndef WUBU_VERIFIER_H
#define WUBU_VERIFIER_H

#include <stdint.h>
#include <stdbool.h>

/* Score a span payload. Returns 0..100. Out: *passed (>= threshold). */
float wubu_verifier_score(const char *payload, uint64_t ts_ms,
                          void *ud, bool *passed);

/* Wire the verifier onto the global AGI kernel. Idempotent. */
void wubu_verifier_install(void);

/* The promotion threshold (spans >= this pass). */
#define WUBU_VERIFIER_THRESHOLD 60.0f

/* Tunables (fixed policy, documented). */
#define WUBU_VERIFIER_MAX_LEN   191    /* payload cap (span data) */
#define WUBU_VERIFIER_MIN_LEN   4      /* refuse noise spans */

#endif /* WUBU_VERIFIER_H */
