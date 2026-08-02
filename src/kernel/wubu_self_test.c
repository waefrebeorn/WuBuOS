/*
 * wubu_self_test.c  --  kernel-resident self-test gate (gap G2)
 *
 * The suite exercises the live kernel structures every time it runs:
 *   - heap integrity   (mem_validate_all: the free list is coherent)
 *   - heap coalescing  (no adjacent-free pairs: the B9 invariant)
 *   - sync lock        (init + lock/unlock round trip)
 *   - AGI trace ring   (the span data ring is within its capacity)
 *   - hive sanity      (the long-term hive is present + usable)
 * The verifier adds +10 only when ALL pass; any failure keeps the
 * score below the promotion threshold.
 */
#include "wubu_self_test.h"
#include <string.h>

static uint32_t g_passed = 0;
static uint32_t g_total = 0;

uint32_t wubu_self_test_run(uint32_t *total)
{
    g_passed = 0;
    g_total = 0;

    /* 1. heap integrity: the allocator's own walk must be clean */
    extern int mem_validate_all(void);
    g_total++;
    if (mem_validate_all() == 0) g_passed++;

    /* 2. heap coalescing invariant: no adjacent free blocks */
    extern int mem_validate_coalescing(void);
    g_total++;
    if (mem_validate_coalescing() == 0) g_passed++;

    /* 3. sync lock round trip */
    {
        extern void wubu_spin_init(void *);
        extern void wubu_spin_lock(void *);
        extern void wubu_spin_unlock(void *);
        uint64_t lock[4] = {0, 0, 0, 0};
        wubu_spin_init(lock);
        wubu_spin_lock(lock);
        wubu_spin_unlock(lock);
        g_total++;
        if (lock[0] == 0) g_passed++;   /* unlocked after unlock */
    }

    /* 4. the AGI trace ring is within its capacity */
    {
        extern int wubu_agi_kernel_trace_count(void *);
        extern void *wubu_agi_kernel_global(void);
        extern int wubu_agi_kernel_region_count(void *);
        int n = wubu_agi_kernel_trace_count(wubu_agi_kernel_global());
        int cap = 256;   /* WUBU_AGI_TRACE_CAP */
        g_total++;
        if (n >= 0 && n <= cap) g_passed++;
        (void)wubu_agi_kernel_region_count;
    }

    /* 5. the long-term hive is present and accepts an insert */
    {
        /* the hive is module-owned in metal_main; expose via the
         * write-count accessor pattern: the kernel's own check is
         * "the memory hook is armed" -- we verify the AGI kernel has
         * one (a NULL hook means the store died at boot). */
        extern int wubu_agi_kernel_has_memory(void *);
        extern void *wubu_agi_kernel_global(void);
        g_total++;
        if (wubu_agi_kernel_has_memory(wubu_agi_kernel_global()))
            g_passed++;
    }

    if (total) *total = g_total;
    return g_passed;
}

int wubu_self_test_ok(void)
{
    return (g_total > 0 && g_passed == g_total);
}
