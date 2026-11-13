/*
 * test_nt_abi.c -- conformance gate for docs/ABI.md.
 *
 * The contract makes four promises a caller can actually test:
 *   1. dispatch is fail-closed (out-of-range and empty slots -> NOT_IMPLEMENTED)
 *   2. every slot has a state, and the states sum to NT_TBL_SIZE
 *   3. probing reports REAL / STUB / EMPTY correctly without invoking anything
 *   4. no slot is contested (a conflict is a defect, not expected state)
 *
 * Nothing here calls a real handler directly: the argument sweep in
 * vsl_nt_contract_test already covers that, forked.  This file checks the surface
 * itself.
 */

#include "vsl/vsl_nt_internal.h"

#include <stdio.h>
#include <string.h>

static int g_checks = 0, g_fails = 0;
#define TEST(cond, msg) do {                                        \
        g_checks++;                                                 \
        if (!(cond)) { g_fails++;                                   \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); } \
    } while (0)

int main(void)
{
    /* -- 1. out-of-range probes are refused ---------------------------- */
    wubu_nt_slot_state_t st = WUBU_NT_SLOT_REAL;
    TEST(vsl_nt_probe(0, &st) == -1,     "slot 0 is not a valid syscall number");
    /* Fail-closed: a refused probe must leave EMPTY, never the caller's previous
     * contents -- a caller that forgets to check the return value then sees "no
     * support" rather than acting on whatever was on the stack. */
    TEST(st == WUBU_NT_SLOT_EMPTY,
         "a failed probe leaves a safe EMPTY state behind");
    TEST(vsl_nt_probe(513, &st) == -1,   "slot 513 is beyond NT_TBL_SIZE");
    TEST(vsl_nt_probe(99999, &st) == -1, "far out-of-range is refused");
    TEST(vsl_nt_probe(1, NULL) == -1,    "NULL state pointer cannot crash");

    /* -- 2. states partition the table --------------------------------- */
    /* Deliberately probe BEFORE anything triggers nt_dispatch_init(): a caller
     * must get a truthful answer even when no syscall has been dispatched yet. */
    uint32_t pre_real = 0;
    for (uint32_t n = 1; n <= 512; n++) {
        wubu_nt_slot_state_t v = WUBU_NT_SLOT_EMPTY;
        vsl_nt_probe(n, &v);
        if (v == WUBU_NT_SLOT_REAL) pre_real++;
    }
    printf("        REAL slots before first dispatch: %u\n", pre_real);
    TEST(pre_real == 0,
         "nothing may look REAL before the table is initialised");

    /* Dispatch is what lazily runs nt_dispatch_init().  Force it NOW so the
     * counts below describe the live table rather than the pre-init zeros --
     * otherwise the per-slot scan at the end would be comparing two different
     * states and every cross-check would fail. */
    (void)vsl_nt_syscall_dispatch(NULL, 1, NULL, 0);

    uint32_t e = 0, s = 0, r = 0, c = 0;
    vsl_nt_slot_counts(&e, &s, &r, &c);
    printf("        slots: %u empty, %u stub, %u real, %u conflict (of %d)\n",
           e, s, r, c, 512);
    TEST(e + s + r + c == 512,
         "every one of the 512 slots is in exactly one state");
    TEST(vsl_nt_coverage() == s + r + c,
         "coverage counts everything that is not EMPTY");

    /* -- 3. per-slot probe agrees with the totals ---------------------- */
    uint32_t seen_real = 0, seen_stub = 0, seen_empty = 0;
    for (uint32_t n = 1; n <= 512; n++) {
        wubu_nt_slot_state_t v = WUBU_NT_SLOT_EMPTY;
        if (vsl_nt_probe(n, &v) != 0) { TEST(0, "probe failed inside range"); break; }
        switch (v) {
        case WUBU_NT_SLOT_REAL:  seen_real++;  break;
        case WUBU_NT_SLOT_STUB:  seen_stub++;  break;
        case WUBU_NT_SLOT_EMPTY: seen_empty++; break;
        case WUBU_NT_SLOT_CONFLICT: break;
        default: TEST(0, "probe returned an unknown state"); break;
        }
        /* A slot cannot be both EMPTY and something else. */
        if (v == WUBU_NT_SLOT_EMPTY &&
            vsl_nt_syscall_dispatch(NULL, n, NULL, 0) != NT_STATUS_NOT_IMPLEMENTED)
            TEST(0, "an EMPTY slot must fail closed");
    }
    TEST(seen_real == r, "per-slot REAL count matches vsl_nt_slot_counts");
    TEST(seen_stub == s, "per-slot STUB count matches vsl_nt_slot_counts");
    TEST(seen_empty == e, "per-slot EMPTY count matches vsl_nt_slot_counts");

    /* -- 4. fail-closed dispatch --------------------------------------- */
    /* NOTE ordinal 0 is special-cased in vsl_nt_syscall_dispatch: it fans out to
     * NtFlushWriteBuffer / NtYieldExecution / ... so it does NOT fail closed.
     * Only out-of-range numbers do. */
    TEST(vsl_nt_syscall_dispatch(NULL, 513, NULL, 0) == NT_STATUS_NOT_IMPLEMENTED,
         "dispatch(513) fails closed");
    TEST(vsl_nt_syscall_dispatch(NULL, 65535, NULL, 0) == NT_STATUS_NOT_IMPLEMENTED,
         "dispatch(far out of range) fails closed");

    /* -- 5. no contested slots ----------------------------------------- */
    TEST(c == 0, "no slot may be contested (two REAL handlers on one number)");
    TEST(vsl_nt_collision_count() == 0, "collision log is empty");

    printf("=== test_nt_abi: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
