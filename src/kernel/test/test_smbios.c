/* test_smbios.c -- host tests for the SMBIOS walk (gap I3).
 * The EPS search targets the BIOS ROM area (metal-only); the structure
 * walk is tested here with a synthetic table via the explicit-address
 * hook. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "wubu_smbios.c"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

static uint8_t g_buf[256];

static void build_fake(void)
{
    memset(g_buf, 0, sizeof(g_buf));
    uint64_t t = (uint64_t)(uintptr_t)g_buf;

    /* type 0 (BIOS): header + format (0x18) + strings "ACME\0\0".
     * The strcpy writes "ACME\0" at 0x18..0x1C; the second NUL at
     * 0x1D terminates the area -> the next structure starts at 0x1E. */
    g_buf[0] = 0; g_buf[1] = 0x18; g_buf[2] = 0; g_buf[3] = 0;
    g_buf[0x12] = 5;               /* BIOS major 5 */
    g_buf[0x13] = 2;               /* BIOS minor 2 */
    strcpy((char *)g_buf + 0x18, "ACME");
    g_buf[0x18 + 5] = 0;           /* second NUL: strings done */

    /* type 1 (system): header + strings, right after the double NUL */
    uint64_t s1 = 0x1E;
    g_buf[s1] = 1; g_buf[s1 + 1] = 8; g_buf[s1 + 2] = 1; g_buf[s1 + 3] = 0;
    strcpy((char *)g_buf + s1 + 8, "WuBuCorp");
    g_buf[s1 + 8 + 8] = 0;
    strcpy((char *)g_buf + s1 + 17, "WubuBook-9000");
    g_buf[s1 + 17 + 13] = 0;
    strcpy((char *)g_buf + s1 + 31, "v1.0");
    g_buf[s1 + 31 + 4] = 0;
    strcpy((char *)g_buf + s1 + 36, "SN-1234");
    g_buf[s1 + 36 + 7] = 0;
    g_buf[s1 + 44] = 0;            /* double NUL */

    /* terminator */
    uint64_t e = s1 + 45;
    g_buf[e] = 0; g_buf[e + 1] = 0;
}

int main(void)
{
    printf("wubu_smbios tests (gap I3)\n");
    build_fake();
    uint64_t t = (uint64_t)(uintptr_t)g_buf;

    wubu_smbios_t s;
    CHECK(wubu_smbios_walk(t, &s) == 0);
    CHECK(s.found == 1);
    CHECK(s.bios_major == 5);
    CHECK(s.bios_minor == 2);
    CHECK(s.bios_vendor_len == 5);           /* "ACME" + NUL */
    CHECK(s.system_manufacturer_len == 9);   /* "WuBuCorp" + NUL */
    CHECK(s.system_product_len == 14);       /* "WubuBook-9000" + NUL */
    CHECK(s.system_version_len == 5);        /* "v1.0" + NUL */
    CHECK(s.system_serial_len == 8);         /* "SN-1234" + NUL */
    CHECK(s.tables == 2);

    /* absent table -> -1 */
    CHECK(wubu_smbios_walk(0, &s) == -1);

    if (failures == 0) printf("test_smbios: ALL PASS\n");
    else printf("test_smbios: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}
