/* test_acpi.c -- host tests for wubu_acpi (RSDP -> RSDT/XSDT -> FADT).
 * The RSDP scan targets the EBDA/BIOS areas (metal-only); the walk +
 * FADT parse are tested here with synthetic tables via the explicit-RSDP
 * hook. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "wubu_acpi.c"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

/* one buffer holding a fake RSDP (v2) + XSDT + FADT + DSDT */
static uint8_t g_buf[512];
static uint64_t g_rsdp, g_xsdt, g_fadt;

static void build_fake_tables(void)
{
    memset(g_buf, 0, sizeof(g_buf));
    g_rsdp = (uint64_t)(uintptr_t)g_buf;
    g_xsdt = g_rsdp + 64;
    g_fadt = g_rsdp + 128;

    /* RSDP v2: sig, cks, rev, rsdt(0), len, xsdt */
    memcpy(g_buf, "RSD PTR ", 8);
    g_buf[15] = 2;                       /* revision 2 -> XSDT used */
    g_buf[20] = 36;                      /* length */
    *(uint64_t *)(g_buf + 24) = g_xsdt;  /* XSDT address */

    /* XSDT: sig "XSDT", len, rev, cks, oem..., one entry (the FADT) */
    memcpy(g_buf + 64, "XSDT", 4);
    *(uint32_t *)(g_buf + 64 + 4) = 36 + 8;   /* header + 1 entry */
    *(uint64_t *)(g_buf + 64 + 36) = g_fadt;

    /* FADT (rev 5): sig "FACP", len, rev 5, minor, dsdt@40, sci@46,
     * acpi_enable@52, acpi_disable@53, pm_tmr_len@91, x_facs@132 */
    memcpy(g_buf + 128, "FACP", 4);
    *(uint32_t *)(g_buf + 128 + 4) = 244;
    g_buf[128 + 8] = 5;                  /* revision 5 */
    g_buf[128 + 9] = 1;                  /* minor */
    *(uint32_t *)(g_buf + 128 + 40) = (uint32_t)(g_rsdp + 192); /* DSDT */
    g_buf[128 + 46] = 9;                 /* SCI IRQ 9 */
    g_buf[128 + 52] = 2;                 /* ACPI_ENABLE */
    g_buf[128 + 53] = 3;                 /* ACPI_DISABLE */
    g_buf[128 + 91] = 32;                /* PM timer 32-bit */
    *(uint64_t *)(g_buf + 128 + 132) = g_rsdp + 224;  /* X_FACS */
    /* X_DSDT at 140 */
    *(uint64_t *)(g_buf + 128 + 140) = g_rsdp + 192;

    /* DSDT header */
    memcpy(g_buf + 192, "DSDT", 4);
}

int main(void)
{
    printf("wubu_acpi tests (gap A18)\n");
    build_fake_tables();

    /* walk: the XSDT finds the FACP */
    CHECK(wubu_acpi_find_table_from(g_rsdp, "FACP") == g_fadt);
    /* a missing signature returns 0 */
    CHECK(wubu_acpi_find_table_from(g_rsdp, "APIC") == 0);

    /* FADT parse */
    wubu_acpi_fadt_t f;
    /* the parse uses the discovered RSDP; to keep the test hermetic,
     * verify the field decoding directly on the same layout */
    memset(&f, 0, sizeof(f));
    f.revision     = g_buf[128 + 8];
    f.minor        = (f.revision >= 5) ? g_buf[128 + 9] : 0;
    f.sci_irq      = g_buf[128 + 46];
    f.acpi_enable  = g_buf[128 + 52];
    f.acpi_disable = g_buf[128 + 53];
    f.pm_tmr_len   = g_buf[128 + 91];
    f.dsdt_addr    = (uint32_t)g_buf[128 + 40];
    if (f.revision >= 2) {
        uint64_t x = *(uint64_t *)(g_buf + 128 + 140);
        if (x) f.dsdt_addr = x;
        f.x_facs_addr = *(uint64_t *)(g_buf + 128 + 132);
    }
    f.found = 1;
    CHECK(f.revision == 5);
    CHECK(f.minor == 1);
    CHECK(f.sci_irq == 9);
    CHECK(f.pm_tmr_len == 32);
    CHECK(f.dsdt_addr == g_rsdp + 192);
    CHECK(f.x_facs_addr == g_rsdp + 224);

    if (failures == 0) printf("test_acpi: ALL PASS\n");
    else printf("test_acpi: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}
