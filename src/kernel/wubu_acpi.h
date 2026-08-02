/*
 * wubu_acpi.h  --  ACPI table discovery: RSDP -> RSDT/XSDT -> FADT
 *
 * Gap A18: the kernel assumed the firmware memory map instead of reading
 * the ACPI tables. This module locates the RSDP ("RSD PTR ") in the
 * BIOS areas, walks the RSDT/XSDT signature array, and parses the FADT
 * (the fixed ACPI description table: the DSDT address + the timer/IRQ
 * model). Freestanding C11, opaque-friendly, no heap.
 */
#ifndef WUBU_ACPI_H
#define WUBU_ACPI_H

#include <stdint.h>
#include <stddef.h>

/* The parsed FADT essentials. */
typedef struct {
    uint64_t dsdt_addr;          /* DSDT physical address */
    uint64_t x_facs_addr;        /* X_FACS (64-bit) if present */
    uint8_t  sci_irq;            /* system control interrupt */
    uint8_t  acpi_enable;        /* PM1a_CNT SLP_TYP enable reg offset */
    uint8_t  acpi_disable;
    uint8_t  pm_tmr_len;         /* PM timer length in bits (0/24/32) */
    uint8_t  revision;           /* FADT revision */
    uint8_t  minor;              /* FADT minor version (rev 5+) */
    int      found;              /* 1 = a valid FADT was parsed */
} wubu_acpi_fadt_t;

/* Locate the RSDP (searches the EBDA + the 0xE0000..0xFFFFF BIOS area).
 * Returns the physical address of the RSDP, or 0 if not found. */
uint64_t wubu_acpi_find_rsdp(void);

/* Walk the RSDT/XSDT for the table with the given 4-char signature.
 * Returns its physical address or 0. */
uint64_t wubu_acpi_find_table(const char sig[4]);

/* Test hook: same walk, but starting from an explicit RSDP address
 * (the production path discovers the RSDP itself). */
uint64_t wubu_acpi_find_table_from(uint64_t rsdp, const char sig[4]);

/* Parse the FADT into `out`; returns 0 on success, -1 if missing. */
int wubu_acpi_parse_fadt(wubu_acpi_fadt_t *out);

/* Convenience: rsdp + walk + fadt in one call (0 = OK, -1 = no ACPI). */
int wubu_acpi_init(wubu_acpi_fadt_t *out);

#endif
