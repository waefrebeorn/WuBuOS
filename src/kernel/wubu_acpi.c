/*
 * wubu_acpi.c  --  ACPI table discovery (RSDP -> RSDT/XSDT -> FADT)
 *
 * Gap A18. The RSDP lives either at the top of the EBDA (the BIOS data
 * area pointer chain) or in the 0xE0000..0xFFFFF legacy BIOS region.
 * Its 8-byte signature is "RSD PTR " (note the trailing space). The
 * RSDT/XSDT that follow hold an array of 32/64-bit table addresses;
 * we scan for the FADT ("FACP") and parse its fixed fields. All reads
 * are direct physical accesses -- on bare metal the identity map covers
 * the low 1 GB. Freestanding C11.
 */
#include "wubu_acpi.h"

/* ---- raw physical reads -------------------------------------------- */

static uint8_t rd8(uint64_t p)  { return *(volatile uint8_t  *)(uintptr_t)p; }
static uint16_t rd16(uint64_t p){ return *(volatile uint16_t *)(uintptr_t)p; }
static uint32_t rd32(uint64_t p){ return *(volatile uint32_t *)(uintptr_t)p; }
static uint64_t rd64(uint64_t p){ return *(volatile uint64_t *)(uintptr_t)p; }

/* 8/4-byte signature compare (no libc dependency). */
static int sig_eq8(uint64_t p, const char *sig)
{
    for (int i = 0; i < 8; i++)
        if ((char)rd8(p + (uint64_t)i) != sig[i]) return 0;
    return 1;
}
static int sig_eq4(uint64_t p, const char *sig)
{
    for (int i = 0; i < 4; i++)
        if ((char)rd8(p + (uint64_t)i) != sig[i]) return 0;
    return 1;
}

/* ---- RSDP ----------------------------------------------------------- */

uint64_t wubu_acpi_find_rsdp(void)
{
    /* 1) the EBDA: the real-mode pointer at 0x40E gives its segment.
     * The RSDP may sit in its first KB. */
    uint16_t ebda_seg = rd16(0x40E);
    if (ebda_seg >= 0x8000) {                 /* sanity: > 512 KB */
        uint64_t ebda = (uint64_t)ebda_seg << 4;
        for (uint64_t off = 0; off < 1024; off += 16) {
            if (sig_eq8(ebda + off, "RSD PTR ")) return ebda + off;
        }
    }
    /* 2) the legacy BIOS area 0xE0000..0xFFFFF. */
    for (uint64_t p = 0xE0000ull; p < 0x100000ull; p += 16) {
        if (sig_eq8(p, "RSD PTR ")) return p;
    }
    return 0;
}

/* ---- table walk ------------------------------------------------------ */

uint64_t wubu_acpi_find_table(const char sig[4])
{
    uint64_t rsdp = wubu_acpi_find_rsdp();
    if (!rsdp) return 0;
    return wubu_acpi_find_table_from(rsdp, sig);
}

uint64_t wubu_acpi_find_table_from(uint64_t rsdp, const char sig[4])
{
    if (!rsdp) return 0;
    /* RSDP v1: checksum byte at 8; v2+ adds XSDT at 24. */
    uint8_t rev = rd8(rsdp + 15);
    uint64_t xsdt = (rev >= 2) ? rd64(rsdp + 24) : 0;
    uint64_t rsdt = rd32(rsdp + 16);

    /* prefer the XSDT (64-bit entries); fall back to the RSDT */
    if (xsdt) {
        uint32_t len = rd32(xsdt + 4);
        uint32_t n = (len - 36) / 8;
        for (uint32_t i = 0; i < n; i++) {
            uint64_t t = rd64(xsdt + 36 + (uint64_t)i * 8);
            if (!t) continue;
            if (sig_eq4(t, sig)) return t;
        }
    }
    if (rsdt) {
        uint32_t len = rd32(rsdt + 4);
        uint32_t n = (len - 36) / 4;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t t = rd32(rsdt + 36 + (uint64_t)i * 4);
            if (!t) continue;
            if (sig_eq4((uint64_t)t, sig)) return (uint64_t)t;
        }
    }
    return 0;
}

/* ---- FADT ------------------------------------------------------------ */

int wubu_acpi_parse_fadt(wubu_acpi_fadt_t *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));

    uint64_t facp = wubu_acpi_find_table("FACP");
    if (!facp) return -1;

    out->revision    = rd8(facp + 8);
    out->minor       = (out->revision >= 5) ? rd8(facp + 9) : 0;
    out->sci_irq     = rd8(facp + 46);
    out->acpi_enable = rd8(facp + 52);       /* ACPI_ENABLE */
    out->acpi_disable= rd8(facp + 53);       /* ACPI_DISABLE */
    out->pm_tmr_len  = rd8(facp + 91);       /* PM_TMR_LEN (bits) */
    out->dsdt_addr   = (uint64_t)rd32(facp + 40);
    if (out->revision >= 2) {
        /* X_DSDT (64-bit) at 140 when present; X_FACS at 132 */
        uint64_t xdsdt = rd64(facp + 140);
        if (xdsdt) out->dsdt_addr = xdsdt;
        out->x_facs_addr = rd64(facp + 132);
    }
    out->found = 1;
    return 0;
}

int wubu_acpi_init(wubu_acpi_fadt_t *out)
{
    return wubu_acpi_parse_fadt(out);
}
