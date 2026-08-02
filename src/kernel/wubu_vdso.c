/*
 * wubu_vdso.c  --  kernel vDSO/vsyscall page (gap H6)
 *
 * The vDSO page is a static (BSS) page mapped read-only at its fixed
 * VA. The timer tick refreshes the counters; the kernel's own tests
 * can read them back. The syscall-stub slot is reserved for the future
 * ring-3 entry trampoline (the H4 user/kernel split).
 */
#include "wubu_vdso.h"

static wubu_vdso_t g_vdso;    /* the published page (one 4K page's worth) */

int wubu_vdso_init(void)
{
    g_vdso.magic = WUBU_VDSO_MAGIC;
    g_vdso.version = 1;
    g_vdso.uptime_ms = 0;
    g_vdso.tick = 0;
    g_vdso.promoted_total = 0;

    /* map the page read-only at the fixed VA (the page tables live at
     * PML4_BASE; the flags: P=1, W=0 -- user-read in the future split). */
    extern int wubu_vmm_map_page(uint64_t virt, uint64_t phys,
                                 uint32_t flags);
    if (wubu_vmm_map_page(WUBU_VDSO_VA, (uint64_t)(uintptr_t)&g_vdso, 1) != 0)
        return -1;
    return 0;
}

void wubu_vdso_update(uint64_t uptime_ms, uint64_t tick,
                      uint32_t promoted_total)
{
    g_vdso.uptime_ms = uptime_ms;
    g_vdso.tick = tick;
    g_vdso.promoted_total = promoted_total;
}

const wubu_vdso_t *wubu_vdso_get(void)
{
    return &g_vdso;
}
