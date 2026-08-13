/* test_iommu.c -- host tests for the IOMMU/VT-d discovery (gap E5).
 * The engine-capability MMIO reads are metal-only; the DMAR table walk
 * + the DRHD parse are tested here with a synthetic table. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "wubu_iommu.h"
#include "wubu_iommu.c"

/* the RSDP/RSDT walk is the ACPI module's (linked separately on metal);
 * the host test stubs it */
uint64_t wubu_acpi_find_table_from(const char *sig) { (void)sig; return 0; }

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

int main(void)
{
    printf("wubu_iommu tests (gap E5)\n");

    /* synthetic DMAR: header + one DRHD */
    uint8_t dmar[64];
    memset(dmar, 0, sizeof(dmar));
    dmar[0] = 'D'; dmar[1] = 'M'; dmar[2] = 'A'; dmar[3] = 'R';
    dmar[4] = 1;                  /* revision */
    dmar[5] = 0;                  /* flags: no intr remap */
    dmar[6] = 0; dmar[7] = 64;    /* length 64 */
    /* the DRHD record at offset 36: type 0, len 24, segment 0 */
    dmar[36] = 0;
    dmar[37] = 0;
    dmar[38] = 24; dmar[39] = 0;
    dmar[40] = 0; dmar[41] = 0;   /* flags */
    dmar[42] = 0; dmar[43] = 0;   /* segment (low) */
    /* engine base = 0: the capability MMIO read is metal-only, so the
     * host test exercises the DRHD walk without the MMIO deref */

    wubu_iommu_t s;
    /* no engine base read on the host: base 0 -> found stays 0, the
     * walk still validates the DRHD parse (no crash, sane output) */
    CHECK(wubu_iommu_probe_table(dmar, 64, &s) == 0);
    CHECK(s.version == 1);

    /* a bad signature is rejected */
    dmar[0] = 'X';
    CHECK(wubu_iommu_probe_table(dmar, 64, &s) == 0);
    CHECK(s.found == 0);

    /* NULL is rejected */
    CHECK(wubu_iommu_probe_table(NULL, 0, &s) == -1);

    if (failures == 0) printf("test_iommu: ALL PASS\n");
    else printf("test_iommu: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}
