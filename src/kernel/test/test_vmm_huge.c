/* test_vmm_huge.c -- 2 MB KV slab gate: alignment, PS=1 PDE, and the
 * 4 KiB mapping path still works beside it. Hosted test installs the
 * page-table scratch at the kernel's fixed PML4_BASE (0x300000). */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include "wubu_vmm.h"
#include "wubu_memmap.h"
#include "wubu_vmm.c"

/* Firmware is absent in this unit gate: vmm falls back to 256 MB. */
int wubu_memmap_init(wubu_memmap_info_t *info)
{
    (void)info;
    return 0;
}

char _kernel_start[1] = {0};
char _kernel_end[1] = {0};
char _stack_top[1] = {0};

struct CTask;
struct CTask *task_current(void) { return 0; }
int task_prio_get(const struct CTask *t) { (void)t; return 0; }
void task_prio_set(struct CTask *t, int prio) { (void)t; (void)prio; }

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); ++fails; } } while (0)

int main(void)
{
    void *tables = mmap((void *)0x300000, 0x8000,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    CHECK(tables == (void *)0x300000, "fixed page-table scratch");
    uint64_t *pml4 = (uint64_t *)0x300000;
    uint64_t *pdp = (uint64_t *)0x301000;
    uint64_t *pd = (uint64_t *)0x302000;
    uint64_t *pt4k = (uint64_t *)0x303000;
    for (int i = 0; i < 512; i++) {
        pml4[i] = 0; pdp[i] = 0; pd[i] = 0; pt4k[i] = 0;
    }
    pml4[0] = 0x301000u | 3u;
    for (int i = 0; i < 512; i++) pdp[i] = 0x302000u | 3u;
    pd[1] = 0x303000u | 3u;   /* 4 KiB regression VA slot */

    wubu_vmm_init();
    uint64_t huge = wubu_vmm_alloc_huge();
    CHECK(huge != 0, "2 MB contiguous allocation");
    CHECK((huge & 0x1fffffull) == 0, "2 MB physical alignment");

    const uint64_t va = 0x4000000000ull;
    CHECK(wubu_vmm_map_huge(va, huge, 3u) == 0, "map PS=1 PDE");
    uint64_t pde = wubu_vmm_huge_pde(va);
    CHECK((pde & 1u) != 0, "huge PDE present");
    CHECK((pde & 0x80u) != 0, "huge PDE PS bit set");
    CHECK((pde & 0x000ffffffffff000ull) == huge, "huge PDE base");

    uint64_t p4 = wubu_vmm_alloc_pages(1);
    CHECK(p4 != 0, "4 KiB page alongside huge slab");
    CHECK(wubu_vmm_map_page(va + 0x200000ull, p4, 3u) == 0,
          "4 KiB mapping still works");
    CHECK((wubu_vmm_huge_pde(va) & 0x80u) != 0,
          "4 KiB mapping did not clear huge PS bit");
    CHECK(wubu_vmm_unmap(va + 0x200000ull) == 0, "unmap 4 KiB page");
    CHECK(wubu_vmm_unmap_huge(va) == 0, "unmap huge page");
    CHECK(wubu_vmm_huge_pde(va) == 0, "huge PDE cleared");
    wubu_vmm_free_pages(p4, 1);
    wubu_vmm_free_huge(huge);

    /* ---- the 2 MB arena: multi-slab span, contiguous phys, reuse ----
     * The arena window has its own PML4[511] PDP slot; install its tables
     * in the same fixed scratch so the walk never needs a fake phys page. */
    uint64_t *arena_ph = (uint64_t *)0x306000;   /* PDP for the arena window */
    uint64_t *arena_pd = (uint64_t *)0x307000;   /* PD holding the slots */
    for (int i = 0; i < 512; i++) { arena_ph[i] = 0; arena_pd[i] = 0; }
    uint32_t ai4 = (uint32_t)(WUBU_VMM_HUGE_VA_BASE >> 39) & 0x1FFu;
    uint32_t ai3 = (uint32_t)(WUBU_VMM_HUGE_VA_BASE >> 30) & 0x1FFu;
    pml4[ai4] = 0x306000u | 3u;
    arena_ph[ai3] = 0x307000u | 3u;

    CHECK(wubu_vmm_huge_arena_free_slots() == WUBU_VMM_HUGE_SLOTS,
          "arena starts with every slot free");
    uint64_t a2 = wubu_vmm_huge_arena_alloc(2);
    CHECK(a2 == WUBU_VMM_HUGE_VA_BASE, "arena span starts at the window base");
    CHECK(wubu_vmm_huge_arena_owned(a2) == 1, "arena owns slab 0");
    CHECK(wubu_vmm_huge_arena_owned(a2 + 0x200000ull) == 1, "arena owns slab 1");
    uint64_t ap0 = wubu_vmm_huge_pde(a2);
    uint64_t ap1 = wubu_vmm_huge_pde(a2 + 0x200000ull);
    CHECK((ap0 & 0x81u) == 0x81u, "arena slab 0 is a present PS=1 PDE");
    CHECK((ap1 & 0x81u) == 0x81u, "arena slab 1 is a present PS=1 PDE");
    CHECK((ap0 & 0x000ffffffffff000ull) + 0x200000ull ==
          (ap1 & 0x000ffffffffff000ull),
          "2-slab arena run is physically contiguous");
    CHECK(wubu_vmm_huge_arena_free_slots() == WUBU_VMM_HUGE_SLOTS - 2,
          "two slots reserved");
    uint64_t a3 = wubu_vmm_huge_arena_alloc(1);
    CHECK(a3 == a2 + 2 * 0x200000ull, "next alloc takes the next slot");
    CHECK(wubu_vmm_huge_arena_alloc(WUBU_VMM_HUGE_SLOTS) == 0,
          "an over-sized ask is refused");
    wubu_vmm_huge_arena_free(a3, 1);
    wubu_vmm_huge_arena_free(a2, 2);
    CHECK(wubu_vmm_huge_arena_free_slots() == WUBU_VMM_HUGE_SLOTS,
          "arena free returns every slot");
    CHECK(wubu_vmm_huge_pde(a2) == 0, "arena free clears the PDE");
    CHECK(wubu_vmm_huge_arena_owned(a2) == 0, "a freed VA is not owned");
    CHECK(wubu_vmm_huge_arena_alloc(1) == WUBU_VMM_HUGE_VA_BASE,
          "first-fit reuses the lowest slot");
    wubu_vmm_huge_arena_free(WUBU_VMM_HUGE_VA_BASE, 1);

    if (!fails) printf("test_vmm_huge: ALL PASS\n");
    else printf("test_vmm_huge: %d FAILURES\n", fails);
    return fails ? 1 : 0;
}
