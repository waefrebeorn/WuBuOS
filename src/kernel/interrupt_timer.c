/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "interrupt.h"
#include "interrupt_apic.h"
#include "tasking.h"
#include "memory.h"
#include "interrupt_io.h"
#include <stdint.h>
#include <string.h>
#include <signal.h>

uint64_t timer_calibrate_tsc(void) {
#ifdef MYSEED_METAL
    /* Read TSC, wait 10ms via PIT, read TSC again */
    uint64_t tsc_start, tsc_end;

    __asm__ volatile ("rdtsc" : "=a"(tsc_start), "=d"(tsc_end));
    tsc_start = ((uint64_t)tsc_end << 32) | tsc_start;

    /* Wait ~10ms using PIT */
    for (volatile int i = 0; i < 1000000; i++) { __asm__ volatile ("pause"); }

    __asm__ volatile ("rdtsc" : "=a"(tsc_start), "=d"(tsc_end));
    tsc_end = ((uint64_t)tsc_end << 32) | tsc_start;

    return tsc_end - tsc_start;  /* TSC ticks per ~10ms */
#else
    return 0;
#endif
}

int timer_init_deadline(uint64_t ns) {
#ifdef MYSEED_METAL
    if (!g_lapic_base) return -1;
    /* Set LAPIC timer to one-shot with deadline */
    lapic_write(LAPIC_LVT_TIMER, 240 | (0x0 << 17));  /* One-shot mode */
    lapic_write(LAPIC_TIMER_INIT_CNT, (uint32_t)(ns / 1000));  /* Rough */
    return 0;
#else
    (void)ns;
    return -1;
#endif
}
