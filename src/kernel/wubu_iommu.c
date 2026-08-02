/*
 * wubu_iommu.c  --  IOMMU/VT-d discovery (gap E5)
 *
 * The DMAR ACPI table: header (36 bytes), then a sequence of remapping
 * structure records (type 0 = DRHD). A DRHD carries the VT-d engine's
 * MMIO base (bytes 16..23 of the record). The engine's CAP (offset 0x8)
 * and ECAP (offset 0x10) report the supported page-table formats and
 * the fault-log entry count; GCMD (offset 0x18) + RTADDR (offset 0x20)
 * are the setup registers. We read the capability registers only --
 * the wiring of the root/context tables + the fault log is the follow-on.
 */
#include "wubu_iommu.h"

/* find the "DMAR" table through the ACPI RSDP/RSDT walk (same shape as
 * the ACPI module's find) -- returns the table's physical address. */
static uint64_t acpi_find_dmar(void)
{
    extern uint64_t wubu_acpi_find_table_from(const char *sig);
    return wubu_acpi_find_table_from("DMAR");
}

static uint32_t rd32(uint64_t a)
{
    return *(volatile uint32_t *)(uintptr_t)a;
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint64_t rd64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i);
    return v;
}

int wubu_iommu_probe_table(const uint8_t *dmar, uint32_t len,
                           wubu_iommu_t *out)
{
    if (!out) return -1;
    out->found = 0;
    if (!dmar) return -1;
    if (len < 36) return 0;
    if (dmar[0] != 'D' || dmar[1] != 'M' || dmar[2] != 'A' || dmar[3] != 'R')
        return 0;
    out->version = dmar[4];
    out->flags = dmar[5];
    out->rtaddr = 0;

    /* walk the remapping structures */
    uint32_t off = 36;
    while (off + 16 <= len) {
        uint8_t type = dmar[off];
        uint16_t slen = rd16(dmar + off + 2);
        if (slen < 16 || off + slen > len) break;
        if (type == 0) {                  /* DRHD */
            uint64_t base = rd64(dmar + off + 16);
            out->segment = rd16(dmar + off + 8);
            out->rtaddr = 0;
            if (base) {
                /* read the engine's capability registers (MMIO) */
                out->cap = rd32(base + 0x8);
                out->ecap = rd32(base + 0x10);
                out->found = 1;
            }
            break;                        /* first DRHD is enough */
        }
        off += slen;
    }
    return 0;
}

int wubu_iommu_probe(wubu_iommu_t *out)
{
    uint64_t dmar = acpi_find_dmar();
    if (!dmar) {
        if (out) out->found = 0;
        return 0;
    }
    /* the table's length is in its header (bytes 4..7 are the revision/
     * checksum; the total length is at bytes 4..7? no: ACPI headers put
     * the length at bytes 4..7 of the table header for RSDT tables; for
     * DMAR the header is the standard ACPI one: length at offset 4). */
    const uint8_t *p = (const uint8_t *)(uintptr_t)dmar;
    uint32_t len = (uint32_t)(p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24));
    return wubu_iommu_probe_table(p, len, out);
}
