/*
 * wubu_hpet.c  --  High Precision Event Timer (gap A19)
 *
 * The ACPI HPET table (signature "HPET") carries the event-timer block
 * base at offset 44 (a Generic Address Structure: space-id, width, and
 * the 64-bit address). The block starts with the GCAP_ID register whose
 * high 32 bits hold the counter period in femto-seconds; the main
 * counter at 0xF0 counts up once enabled (GCFG bit 0).
 * Freestanding C11; all MMIO reads are direct.
 */
#include "wubu_hpet.h"

static inline uint32_t rd32(uint64_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static inline uint64_t rd64(uint64_t p) { return *(volatile uint64_t *)(uintptr_t)p; }
static inline void wr32(uint64_t p, uint32_t v) { *(volatile uint32_t *)(uintptr_t)p = v; }

uint64_t wubu_hpet_probe(uint64_t *period_fs)
{
    extern uint64_t wubu_acpi_find_table(const char sig[4]);
    uint64_t tbl = wubu_acpi_find_table("HPET");
    if (!tbl) return 0;

    /* GAS: offset 44 = space-id (byte), bit-width (byte), bit-offset
     * (byte), address (8 bytes at 48). */
    uint8_t  space = *(volatile uint8_t  *)(uintptr_t)(tbl + 44);
    uint64_t base  = *(volatile uint64_t *)(uintptr_t)(tbl + 48);
    if (space != 0 || base == 0) return 0;   /* system-memory space */

    /* GCAP_ID: the period (femto-seconds) is bits 63:32. */
    uint64_t gcap = rd64(base + WUBU_HPET_GCAP_ID);
    if (period_fs) *period_fs = gcap >> 32;
    return base;
}

void wubu_hpet_enable(uint64_t mmio_base)
{
    if (!mmio_base) return;
    uint32_t cfg = rd32(mmio_base + WUBU_HPET_GCFG);
    wr32(mmio_base + WUBU_HPET_GCFG, cfg | WUBU_HPET_CNT_EN);
}

uint64_t wubu_hpet_ns(uint64_t mmio_base, uint64_t period_fs)
{
    if (!mmio_base) return 0;
    return hpet_ticks_to_ns(rd64(mmio_base + WUBU_HPET_MAIN_CNT), period_fs);
}
