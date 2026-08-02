/*
 * wubu_smbios.c  --  SMBIOS/DMI discovery (gap I3)
 *
 * Entry point search: the 32-bit anchor "_SM_" has its table address
 * 8 bytes in; the 64-bit "_SM3_" puts it 16 bytes in (after the
 * 5-byte checksum). The structures are length-prefixed: header (4) +
 * strings area (double-NUL terminated). We walk them, capturing the
 * BIOS (type 0) and system (type 1) string pointers for the summary.
 * Freestanding C11; direct physical reads (identity-mapped low RAM).
 */
#include "wubu_smbios.h"
#include <string.h>

static uint8_t rd8(uint64_t p)  { return *(volatile uint8_t  *)(uintptr_t)p; }
static uint16_t rd16(uint64_t p){ return *(volatile uint16_t *)(uintptr_t)p; }

uint64_t wubu_smbios_find_eps(void)
{
    for (uint64_t p = 0xF0000ull; p < 0x100000ull; p += 16) {
        if (rd8(p) == '_' && rd8(p + 1) == 'S' && rd8(p + 2) == 'M') {
            uint8_t c3 = rd8(p + 3);
            if (c3 == '_')            /* 32-bit anchor "_SM_" */
                return p;
            if (c3 == '3' && rd8(p + 4) == '_')   /* 64-bit "_SM3_" */
                return p;
        }
    }
    return 0;
}

int wubu_smbios_probe(wubu_smbios_t *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    uint64_t eps = wubu_smbios_find_eps();
    if (!eps) return -1;

    /* the table address: "_SM_" + 8, "_SM3_" + 16 */
    uint64_t table = 0;
    if (rd8(eps + 3) == '3')
        table = *(volatile uint64_t *)(uintptr_t)(eps + 16);
    else
        table = *(volatile uint32_t *)(uintptr_t)(eps + 8);
    if (!table) return -1;
    return wubu_smbios_walk(table, out);
}

int wubu_smbios_walk(uint64_t table, wubu_smbios_t *out)
{
    if (!out || !table) return -1;
    memset(out, 0, sizeof(*out));

    /* walk the structures until the 0x00 0x00 terminator */
    uint64_t p = table;
    uint32_t count = 0;
    for (int guard = 0; guard < 512; guard++) {
        uint8_t type = rd8(p);
        uint8_t len  = rd8(p + 1);
        if (type == 0 && len == 0) break;       /* terminator */
        uint16_t handle = rd16(p + 2);
        (void)handle;

        /* find the end of the strings area: the first NUL pair ends it.
         * Direct lookahead: two consecutive NULs -> advance 2 + stop. */
        uint64_t sp = p + len;
        uint32_t str_guard = 0;
        while (str_guard++ < 4096) {
            if (rd8(sp) == 0 && rd8(sp + 1) == 0) { sp += 2; break; }
            sp++;
        }

        if (type == 0) {   /* BIOS information */
            out->bios_major = rd8(p + 0x12);
            out->bios_minor = rd8(p + 0x13);
            /* string 1 = vendor (the FIRST string after the format) */
            uint64_t s = p + len;
            if (rd8(s)) {
                uint64_t e = s;
                while (rd8(e)) e++;
                out->bios_vendor_len = (uint16_t)(e - s + 1);
            }
        } else if (type == 1) {  /* system information */
            uint64_t s = p + len;
            uint32_t want[] = { 1, 2, 3, 4 };   /* 1-based string numbers:
                                                   mfr, product, ver, serial */
            uint32_t cur = 1, i = 0;
            while (i < 4 && rd8(s)) {
                uint64_t e = s;
                while (rd8(e)) e++;
                if (cur == want[i]) {
                    uint16_t l = (uint16_t)(e - s + 1);
                    if (i == 0) out->system_manufacturer_len = l;
                    else if (i == 1) out->system_product_len = l;
                    else if (i == 2) out->system_version_len = l;
                    else out->system_serial_len = l;
                    i++;
                }
                cur++;
                s = e + 1;
            }
        }
        count++;
        p = sp;
    }
    out->tables = count;
    out->found = 1;
    return 0;
}
