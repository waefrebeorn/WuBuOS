/*
 * wubu_hpet.h  --  High Precision Event Timer (gap A19)
 *
 * The kernel's time source was the PIT/LAPIC. The HPET is discovered
 * via the ACPI "HPET" table (the FADT's sibling), which gives its MMIO
 * base. Reading the main counter is a 64-bit MMIO load; the period is
 * in the general-capabilities register (femto-seconds). Freestanding.
 */
#ifndef WUBU_HPET_H
#define WUBU_HPET_H

#include <stdint.h>

/* HPET MMIO register offsets */
#define WUBU_HPET_GCAP_ID    0x00   /* general capabilities + ID */
#define WUBU_HPET_GCFG       0x10   /* general configuration */
#define WUBU_HPET_MAIN_CNT   0xF0   /* main counter (64-bit) */
#define WUBU_HPET_CNT_EN     0x1u   /* GCFG bit 0: enable the counter */

/* Probe the ACPI HPET table (needs wubu_acpi). Returns the MMIO base
 * and the counter period in femtoseconds (via *period_fs). 0 = absent. */
uint64_t wubu_hpet_probe(uint64_t *period_fs);

/* Enable the main counter. */
void wubu_hpet_enable(uint64_t mmio_base);

/* Read the main counter (nanoseconds since enable). */
uint64_t wubu_hpet_ns(uint64_t mmio_base, uint64_t period_fs);

/* Pure helper: counter ticks -> ns. Host-testable. */
static inline uint64_t hpet_ticks_to_ns(uint64_t ticks, uint64_t period_fs)
{
    /* period is in femto-seconds (1e-15 s); ns = ticks * fs / 1e6 */
    return (ticks * period_fs) / 1000000ull;
}

#endif
