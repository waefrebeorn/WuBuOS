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

int apic_init(void) {
#ifdef MYSEED_METAL
    /* --------------------------------------------------------------
     * Enable LAPIC via MSR
     * -------------------------------------------------------------- */
    uint64_t apic_base = rdmsr(MSR_IA32_APIC_BASE);
    apic_base |= 0x800;  /* Set EN bit (bit 11) */
    apic_base &= ~0xFFF; /* Clear base address bits */
    apic_base |= LAPIC_BASE_DEFAULT;
    wrmsr(MSR_IA32_APIC_BASE, apic_base);

    g_lapic_base = (volatile uint32_t *)LAPIC_BASE_DEFAULT;

    /* --------------------------------------------------------------
     * Configure LAPIC
     * -------------------------------------------------------------- */

    /* Enable LAPIC: set SVR bit 8 + a SAFE spurious vector (0xFF).  The
     * reset SVR's vector field is 0, so a spurious APIC event would
     * deliver VECTOR 0 (#DE) and kill the kernel. */
    lapic_write(LAPIC_SVR, 0x100 | 0xFF);

    /* Set LAPIC timer to one-shot mode initially (will be configured per-use) */
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_TIMER_DIV, 0x3);  /* Divide by 16 */

    /* Mask LINT0, LINT1, ERROR LVT entries */
    lapic_write(LAPIC_LVT_LINT0, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT1, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_ERROR, LAPIC_LVT_MASKED);

    /* --------------------------------------------------------------
     * Detect and configure I/O APIC
     * -------------------------------------------------------------- */
    g_ioapic_base = (volatile uint32_t *)IOAPIC_BASE_DEFAULT;

    /* Read I/O APIC version to get number of IRQ entries (via the fixed
     * select/window accessor -- the old inline read garbage here). */
    uint32_t ioapic_ver = ioapic_read(IOAPIC_VER);
    g_ioapic_irq_count = IOAPIC_RED_ENTRIES(ioapic_ver);

    /* Mask all I/O APIC interrupts initially */
    for (uint32_t i = 0; i < g_ioapic_irq_count; i++) {
        ioapic_write(IOAPIC_REDTBL + i * 2, IOAPIC_RED_MASKED);
        ioapic_write(IOAPIC_REDTBL_HIGH + i * 2, 0);
    }

    /* NOTE: the TSS/IST wiring is skipped -- the kernel's GDT has no TSS
     * descriptor, so the historical `ltr` here would #GP.  The IDT gates
     * all use IST=0 and the kernel does not (yet) use IST stacks. */

    return 0;
#else
    (void)g_lapic_base;
    (void)g_ioapic_base;
    (void)g_ioapic_irq_count;
    (void)g_tss;
    return -1;
#endif
}

/* ------------------------------------------------------------------
 * I/O APIC IRQ Routing
 * ------------------------------------------------------------------ */

int ioapic_route_irq(uint8_t irq, uint8_t vector, uint8_t dest_apic_id) {
#ifdef MYSEED_METAL
    if (irq >= g_ioapic_irq_count) return -1;

    /* Redirection table entry: vector, delivery mode fixed, destination mode physical */
    uint32_t red_low = vector | IOAPIC_RED_DELIV_FIXED | IOAPIC_RED_DESTMODE_PHYS;
    uint32_t red_high = (uint32_t)dest_apic_id << 24;

    ioapic_write(IOAPIC_REDTBL + irq * 2, red_low);
    ioapic_write(IOAPIC_REDTBL_HIGH + irq * 2, red_high);

    return 0;
#else
    (void)irq; (void)vector; (void)dest_apic_id;
    return -1;
#endif
}

int ioapic_mask_irq(uint8_t irq) {
#ifdef MYSEED_METAL
    if (irq >= g_ioapic_irq_count) return -1;
    uint32_t red = ioapic_read(IOAPIC_REDTBL + irq * 2);
    ioapic_write(IOAPIC_REDTBL + irq * 2, red | IOAPIC_RED_MASKED);
    return 0;
#else
    (void)irq;
    return -1;
#endif
}

int ioapic_unmask_irq(uint8_t irq) {
#ifdef MYSEED_METAL
    if (irq >= g_ioapic_irq_count) return -1;
    uint32_t red = ioapic_read(IOAPIC_REDTBL + irq * 2);
    ioapic_write(IOAPIC_REDTBL + irq * 2, red & ~IOAPIC_RED_MASKED);
    return 0;
#else
    (void)irq;
    return -1;
#endif
}

/* ------------------------------------------------------------------
 * LAPIC Timer (for per-CPU timer interrupts)
 * ------------------------------------------------------------------ */

int lapic_timer_init(uint32_t hz, uint8_t vector) {
#ifdef MYSEED_METAL
    if (!g_lapic_base) return -1;

    /* Mask timer LVT */
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED);

    /* Configure timer: periodic mode, vector */
    lapic_write(LAPIC_LVT_TIMER, vector | (0x2 << 17));  /* Periodic mode = bit 17 */
    lapic_write(LAPIC_TIMER_DIV, 0x3);  /* Divide by 16 */

    /* Calibrate using PIT (rough approximation) */
    /* For now, use a fixed initial count - real impl would calibrate */
    lapic_write(LAPIC_TIMER_INIT_CNT, 1000000 / hz);

    return 0;
#else
    (void)hz; (void)vector;
    return -1;
#endif
}

void lapic_eoi(void) {
#ifdef MYSEED_METAL
    if (g_lapic_base) {
        lapic_write(LAPIC_EOI, 0);
    }
#endif
}

/* ------------------------------------------------------------------
 * IPI (Inter-Processor Interrupts)
 * ------------------------------------------------------------------ */

int lapic_send_ipi(uint32_t dest_apic_id, uint8_t vector, uint8_t delivery_mode) {
#ifdef MYSEED_METAL
    if (!g_lapic_base) return -1;

    uint32_t icr_high = dest_apic_id << 24;
    uint32_t icr_low = vector | delivery_mode | LAPIC_ICR_LEVEL_ASSERT;

    lapic_write(LAPIC_ICR_HIGH, icr_high);
    lapic_write(LAPIC_ICR_LOW, icr_low);

    /* Wait for delivery */
    while (lapic_read(LAPIC_ICR_LOW) & (1 << 12)) {
        /* Busy wait */
    }

    return 0;
#else
    (void)dest_apic_id; (void)vector; (void)delivery_mode;
    return -1;
#endif
}

int lapic_broadcast_ipi(uint8_t vector, uint8_t delivery_mode) {
#ifdef MYSEED_METAL
    if (!g_lapic_base) return -1;

    uint32_t icr_low = vector | delivery_mode | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DEST_ALL_INC;
    uint32_t icr_high = 0;

    lapic_write(LAPIC_ICR_HIGH, icr_high);
    lapic_write(LAPIC_ICR_LOW, icr_low);

    while (lapic_read(LAPIC_ICR_LOW) & (1 << 12)) {
        /* Busy wait */
    }

    return 0;
#else
    (void)vector; (void)delivery_mode;
    return -1;
#endif
}
