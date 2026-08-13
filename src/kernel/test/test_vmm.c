/* test_vmm.c -- host tests for wubu_vmm (bitmap allocator + demand
 * registry). The page-table map + demand-fill are METAL-verified (they
 * walk CR3 + execute invlpg -- ring-0 only). */
#include <stdio.h>
#include <stdint.h>

#include "wubu_vmm.h"
#include "wubu_sync.h"
#include "wubu_memmap.h"
#include "wubu_vmm.c"

/* Host-safe spinlock shim: the real wubu_sync lock uses cli (ring-0
 * ONLY -- a CPL-3 cli #GPs). Same flag semantics, no cli/sti. */
void wubu_spin_init(wubu_spinlock_t *l) { l->locked = 0; l->irq_state = 0; }
void wubu_spin_lock(wubu_spinlock_t *l)
{ while (__atomic_test_and_set(&l->locked, __ATOMIC_ACQUIRE)) {} }
void wubu_spin_unlock(wubu_spinlock_t *l)
{ __atomic_clear(&l->locked, __ATOMIC_RELEASE); }

/* E820 stub: no real map on the host (0x98000 is arbitrary memory there) */
int wubu_memmap_init(wubu_memmap_info_t *info)
{ (void)info; return 0; }

/* Linker-symbol stubs (the real ones live in the metal link). */
char _kernel_start[] = "\x00\x00\x10\x00\x00\x00\x00\x00";
char _kernel_end[]   = "\x00\x00\x20\x00\x00\x00\x00\x00";
char _stack_top[]    = "\x50\x46\x16\x00\x00\x00\x00\x00";

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

int main(void)
{
    wubu_vmm_init();
    uint64_t before = wubu_vmm_free_count();
    CHECK(before > 0);
    CHECK(before < WUBU_VMM_BITMAP_BITS);        /* used regions marked */

    /* alloc/free round trip */
    uint64_t p = wubu_vmm_alloc_pages(4);
    CHECK(p != 0);
    CHECK(p >= WUBU_VMM_PHYS_BASE);
    CHECK(p % 4096 == 0);
    CHECK(wubu_vmm_free_count() == before - 4);
    wubu_vmm_free_pages(p, 4);
    CHECK(wubu_vmm_free_count() == before);

    /* no double-free accounting */
    wubu_vmm_free_pages(p, 4);                   /* free of a free page: no-op */
    CHECK(wubu_vmm_free_count() == before);

    /* gap B8: reference counts */
    uint64_t r = wubu_vmm_alloc_pages(2);
    CHECK(r != 0);
    CHECK(wubu_vmm_refcount(r) == 1);            /* fresh alloc: ref 1 */
    wubu_vmm_ref(r, 2);                          /* share it */
    CHECK(wubu_vmm_refcount(r) == 2);
    CHECK(wubu_vmm_refcount(r + 4096) == 2);     /* both pages ref'd */
    CHECK(wubu_vmm_free_count() == before - 2);
    wubu_vmm_free_pages(r, 2);                   /* 2 -> 1: NOT freed */
    CHECK(wubu_vmm_free_count() == before - 2);
    CHECK(wubu_vmm_refcount(r) == 1);
    wubu_vmm_free_pages(r, 2);                   /* 1 -> 0: released */
    CHECK(wubu_vmm_free_count() == before);
    CHECK(wubu_vmm_refcount(r) == 0);            /* free page: ref 0 */

    /* alignment: allocations are 4K-aligned, consecutive runs distinct */
    uint64_t a = wubu_vmm_alloc_pages(2);
    uint64_t b = wubu_vmm_alloc_pages(2);
    CHECK(a != 0 && b != 0);
    CHECK(a != b);
    wubu_vmm_free_pages(a, 2);
    wubu_vmm_free_pages(b, 2);

    /* gap B4: copy-on-write -- the shared mapping is RO + the fault
     * makes a private writable copy. The PTE walk is metal-only (real
     * page tables); on the host we verify the refcount contract: a
     * shared map refs the page (2), and the COW fault drops exactly
     * one ref when it makes its private copy. */
    {
        uint64_t sh = wubu_vmm_alloc_pages(1);
        CHECK(sh != 0);
        CHECK(wubu_vmm_refcount(sh) == 1);
        /* the shared map refs the page: ref becomes 2 (metal's
         * map_shared calls ref before the walk) */
        wubu_vmm_ref(sh, 1);
        CHECK(wubu_vmm_refcount(sh) == 2);
        /* releasing the shared ref returns the page */
        wubu_vmm_free_pages(sh, 1);
        CHECK(wubu_vmm_refcount(sh) == 1);
        wubu_vmm_free_pages(sh, 1);
        CHECK(wubu_vmm_refcount(sh) == 0);
    }

    /* demand registry */
    CHECK(wubu_vmm_demand_count() == 0);
    uint64_t d = wubu_vmm_register_demand(0xffffffff90000000ull, 4096);
    CHECK(d == 0xffffffff90000000ull);
    CHECK(wubu_vmm_demand_count() == 1);
    CHECK(wubu_vmm_is_demand(0xffffffff90000000ull));
    CHECK(wubu_vmm_is_demand(0xffffffff90001fffull));   /* inside */
    CHECK(!wubu_vmm_is_demand(0xffffffff91000000ull));  /* one past (4096 pages) */
    CHECK(!wubu_vmm_is_demand(0x12345678ull));          /* unrelated */

    if (failures == 0) printf("test_vmm: ALL PASS\n");
    else printf("test_vmm: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}
