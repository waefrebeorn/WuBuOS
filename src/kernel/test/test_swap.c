/* test_swap.c -- host tests for the swap module (gap B3).
 * The phys window is identity-mapped metal memory, so the disk IO is
 * verified at the CONTRACT level: the swap must issue 8-sector (4K)
 * transfers at slot*8 offsets -- the regression was a 512-byte write
 * that dropped every page's bytes past sector 0. The stubs record the
 * calls; the full-page byte roundtrip is metal-verified (the page
 * tables + the identity map are metal-only). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "wubu_vmm.h"
#include "wubu_swap.c"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

/* the page-table walk is metal-only: stub it */
int wubu_vmm_map_page(uint64_t virt, uint64_t phys, uint32_t flags)
{
    (void)virt; (void)phys; (void)flags; return 0;
}
int wubu_vmm_unmap(uint64_t virt) { (void)virt; return 0; }

/* recorded disk calls: the swap must do 8-sector page IO */
static uint64_t g_last_lba = 0;
static uint32_t g_last_n = 0;
static int g_writes = 0, g_reads = 0;

int ahci_hba_init(ahci_hba_t *h) { (void)h; return 0; }
int ahci_enumerate_ports(ahci_hba_t *h) { (void)h; return 1; }
int ahci_port_init(ahci_hba_t *h, int p) { (void)h; (void)p; return 0; }
int ahci_sim_disk_create(ahci_hba_t *h, int p, int mb)
{ (void)h; (void)p; (void)mb; return 0; }
int ahci_read(ahci_hba_t *h, int p, uint64_t lba, uint32_t n, void *b)
{ (void)h; (void)p; (void)b; g_last_lba = lba; g_last_n = n; g_reads++; return (int)n; }
int ahci_write(ahci_hba_t *h, int p, uint64_t lba, uint32_t n, const void *b)
{ (void)h; (void)p; (void)b; g_last_lba = lba; g_last_n = n; g_writes++; return (int)n; }

int main(void)
{
    printf("wubu_swap tests (gap B3)\n");

    /* no pages swapped initially */
    CHECK(wubu_swap_count() == 0);
    CHECK(wubu_swap_slot_of(WUBU_VMM_PHYS_BASE) == -1);

    uint64_t phys = WUBU_VMM_PHYS_BASE;

    /* swap out: the write must span a FULL 4K page (8 sectors) at the
     * slot's own offset */
    int slot = wubu_swap_out(0xffffffff90000000ull, phys);
    CHECK(slot >= 0);
    CHECK(g_writes == 1);
    CHECK(g_last_n == WUBU_SWAP_PAGE_SECS);        /* 8 sectors, not 1 */
    CHECK(g_last_lba == WUBU_SWAP_SECTOR + (uint64_t)slot * WUBU_SWAP_PAGE_SECS);
    CHECK(wubu_swap_count() == 1);
    CHECK(wubu_swap_slot_of(phys) == slot);
    CHECK(wubu_swap_va_slot(0xffffffff90000000ull) == slot);

    /* swap back in: the read must be the same 8-sector span */
    CHECK(wubu_swap_in(0xffffffff90000000ull, (uint32_t)slot) == 0);
    CHECK(g_reads == 1);
    CHECK(g_last_n == WUBU_SWAP_PAGE_SECS);
    CHECK(g_last_lba == WUBU_SWAP_SECTOR + (uint64_t)slot * WUBU_SWAP_PAGE_SECS);
    CHECK(wubu_swap_count() == 0);
    CHECK(wubu_swap_slot_of(phys) == -1);
    CHECK(wubu_swap_va_slot(0xffffffff90000000ull) == -1);

    /* a bad slot is rejected */
    CHECK(wubu_swap_in(0xffffffff90000000ull, 9999) == -1);

    if (failures == 0) printf("test_swap: ALL PASS (8-sector page IO contract)\n");
    else printf("test_swap: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}
