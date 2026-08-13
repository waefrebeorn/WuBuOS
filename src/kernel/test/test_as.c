/* test_as.c -- host tests for the address-space module (gap B7).
 * The PML4 clone + CR3 switch normally touch real page tables; on the
 * host we map a fake boot PML4 at the module's PML4_BASE so the clone
 * path is exercised for real. */
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#include "wubu_as.h"
#include "wubu_as.c"

/* metal-only stubs: the page allocator returns frames from a REAL
 * mapped pool so the clone actually writes somewhere valid */
static uint64_t g_off = 0;
uint64_t wubu_vmm_alloc_pages(uint32_t n)
{
    static uint64_t pool = 0;
    if (!pool)
        pool = (uint64_t)(uintptr_t)mmap(NULL, 1 << 20,
                                         PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    uint64_t r = pool + g_off;
    g_off += (uint64_t)n * 4096;
    return r;
}
void wubu_vmm_free_pages(uint64_t p, uint32_t n) { (void)p; (void)n; }

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

int main(void)
{
    printf("wubu_as tests (gap B7)\n");

    /* the kernel singleton exists + is current */
    CHECK(wubu_as_kernel() != NULL);
    CHECK(wubu_as_current() == wubu_as_kernel());
    CHECK(wubu_as_count() == 1);

    /* destroy guards the kernel AS */
    wubu_as_destroy(wubu_as_kernel());
    CHECK(wubu_as_count() == 1);

    /* map a fake boot PML4 at the module's PML4_BASE (0x300000) so the
     * create-clone derefs a valid page on the host */
    void *fake = mmap((void *)0x300000, 4096,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (fake != (void *)0x300000) {
        printf("test_as: cannot map the fake PML4 (host limit) -- skipping clone checks\n");
        return failures ? 1 : 0;
    }
    for (int i = 0; i < 512; i++) ((uint64_t *)0x300000)[i] = 0x80000000ull | (uint64_t)i;

    /* creating an AS clones the kernel window + bumps the count */
    wubu_as_t *as = wubu_as_create();
    CHECK(as != NULL);
    CHECK(as != wubu_as_kernel());
    CHECK(wubu_as_count() == 2);
    /* the clone copied the fake kernel entries */
    CHECK(as->pml4 != (uint64_t *)0x300000);
    CHECK(as->pml4[10] == (0x80000000ull | 10));

    /* bind + switch + destroy lifecycle */
    CHECK(wubu_as_bind(as) == 0);
    CHECK(wubu_as_current() == as);
    wubu_as_bind(wubu_as_kernel());
    CHECK(wubu_as_current() == wubu_as_kernel());
    wubu_as_destroy(as);
    CHECK(wubu_as_count() == 1);

    munmap(fake, 4096);
    if (failures == 0) printf("test_as: ALL PASS\n");
    else printf("test_as: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}
