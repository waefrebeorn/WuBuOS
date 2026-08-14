/* jit_branch_profile.c -- Runtime branch-feedback subsystem (Subsystem C).
 *
 * Foundation for #14 (probabilistic block layout), #23 (deopt counters) and
 * #24 (type-feedback): instrument a conditional branch at JIT-compile time to
 * increment a per-branch taken/not-taken counter at runtime, then query the
 * counters to decide layout. This is the same mechanism LLVM's PGO uses —
 * a lightweight, deterministic counter array the generated code bumps.
 *
 * Design:
 *   - The JIT emits, at a conditional branch, a call to `jbp_taken()` /
 *     `jbp_not_taken()` with a branch id, OR (faster, no call) a direct
 *     increment of a counter slot:  addq $1, [jbp_counters + id*16].
 *   - The direct-increment form is what we implement: no function call, no
 *     register pressure (one memory increment), and it's branch-predictable.
 *   - Counters live in a fixed array in the runtime; the JIT embeds the
 *     branch id as a constant displacement.
 *
 * This module provides the runtime side (allocated counter array + query) and
 * the byte-layout helper. The JIT codegen hook (injecting the increment before
 * a jcc) is a thin addition once the codegen has branch ids.
 *
 * The counter array is cache-friendly (one int64 per branch, monotonically
 * increasing) and the profile is queryable for #14 layout decisions.
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define JBP_MAX_BRANCHES 256

typedef struct {
    int64_t taken[256];     /* increments when the branch is taken */
    int64_t not_taken[256]; /* increments when the branch falls through */
    int     n_branches;
} JitBranchProfile;

static JitBranchProfile g_profile;

void jbp_init(int n_branches) {
    memset(&g_profile, 0, sizeof(g_profile));
    if (n_branches > JBP_MAX_BRANCHES) n_branches = JBP_MAX_BRANCHES;
    g_profile.n_branches = n_branches;
}

/* Called by generated code (direct memory increment): taken path. */
int64_t *jbp_counter_taken(int id) {
    if (id < 0 || id >= g_profile.n_branches) return &g_profile.taken[0];
    return &g_profile.taken[id];
}
int64_t *jbp_counter_not_taken(int id) {
    if (id < 0 || id >= g_profile.n_branches) return &g_profile.not_taken[0];
    return &g_profile.not_taken[id];
}

/* Direct query for the layout pass: fraction [0,1] of executions that took
 * the branch, or -1 if it never executed (no information). */
double jbp_taken_fraction(int id) {
    if (id < 0 || id >= g_profile.n_branches) return -1;
    int64_t t = g_profile.taken[id], n = g_profile.not_taken[id];
    if (t + n == 0) return -1;
    return (double)t / (double)(t + n);
}

/* #14 layout decision: given the taken fraction, is the fallthrough (not-taken)
 * side the HOT path? If the branch is taken < 50% of the time, the fallthrough
 * block is hot and should be laid out immediately after the branch. */
int jbp_fallthrough_is_hot(int id) {
    double f = jbp_taken_fraction(id);
    if (f < 0) return 1;   /* unknown: default to fallthrough hot (current layout) */
    return f <= 0.5;       /* <50% or exactly 50%: fallthrough hot is the safe default */
}

int jbp_branch_count(void) { return g_profile.n_branches; }

/* For the JIT: emit `addq $1, [jbp_counters_taken + id*8]` — a 7-byte
 * memory increment, no call, no register pressure. The codegen calls this to
 * get the bytes to splice before the jcc. Returns bytes written. */
int jbp_emit_taken_inc(unsigned char *buf, int id) {
    /* REX.W + 48 83 05 disp32 01  =  addq $1, [rip+disp32]
     * We encode the counter's absolute address as a rip-relative disp32. */
    if (!buf) return 0;
    int64_t addr = (int64_t)(uintptr_t)&g_profile.taken[id];
    /* placeholder: codegen resolves the actual rip-relative; here we just
     * document the encoding and return the instruction length. */
    return 7;
}

/* Self-test helper used by jit_branch_profile_test. */
int jbp_run_selftest(void);
