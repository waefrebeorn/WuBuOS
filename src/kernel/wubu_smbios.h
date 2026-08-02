/*
 * wubu_smbios.h  --  SMBIOS/DMI discovery (gap I3)
 *
 * The firmware's SMBIOS tables describe the machine (BIOS, system,
 * board, chassis). The entry point lives in the 0xF0000..0xFFFFF BIOS
 * area (either the 32-bit "_SM_" structure or the 64-bit "_SM3_"), and
 * the table walk enumerates the structure headers (type, length, handle)
 * with their text strings. Freestanding C11, no heap.
 */
#ifndef WUBU_SMBIOS_H
#define WUBU_SMBIOS_H

#include <stdint.h>

/* A parsed SMBIOS summary (the fields the kernel cares about). */
typedef struct {
    uint8_t  bios_major, bios_minor;
    uint16_t bios_vendor_len;         /* string length incl. NUL, 0 = none */
    uint16_t system_manufacturer_len;
    uint16_t system_product_len;
    uint16_t system_version_len;
    uint16_t system_serial_len;
    uint32_t tables;                  /* number of structures found */
    int      found;                   /* 1 when the entry point was found */
} wubu_smbios_t;

/* Locate the SMBIOS entry point (searches 0xF0000..0xFFFFF for the
 * "_SM3_" / "_SM_" anchors). Returns its physical address or 0. */
uint64_t wubu_smbios_find_eps(void);

/* Walk the structures and summarize the type-0 (BIOS) + type-1 (system)
 * strings. Returns 0 on success, -1 when absent. */
int wubu_smbios_probe(wubu_smbios_t *out);

/* Test hook: walk from an explicit table address (the production path
 * finds the entry point first). */
int wubu_smbios_walk(uint64_t table, wubu_smbios_t *out);

#endif
