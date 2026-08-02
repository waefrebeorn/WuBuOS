/*
 * wubu_apic.c -- local APIC + I/O APIC bring-up (q35-correct delivery).
 *
 * Steps (see wubu_apic.h for the "why"):
 *   1. Identity-map 0xFEC00000..0xFF000000 (IOAPIC + LAPIC MMIO) with
 *      static 4KB-aligned .bss page tables wired into the crt0 tables.
 *   2. Enable the LAPIC (apic_init: MSR EN bit, SVR, mask LINT0/1).
 *   3. Route ISA IRQs through the I/O APIC redirection table:
 *        INTIN1 -> 33 (PS/2 keyboard), INTIN2 -> 32 (PIT), INTIN12 -> 44
 *        (PS/2 mouse), the rest -> 35..47 (legacy vectors 32+n).
 *   The IOAPIC accessor fix (interrupt_apic.h) is what makes the
 *   redirection table actually programmable.
 */
#include "wubu_apic.h"
#include "interrupt.h"
#include "interrupt_apic.h"
#include "klog.h"
#include <stdint.h>

/* Page tables for the APIC MMIO region.  Static + 4KB aligned (no heap);
 * the kernel .bss is covered by PT_high.  Phys = VA - 0xffffffff80000000. */
static uint64_t wubu_apic_pd[512]   __attribute__((aligned(4096)));
static uint64_t wubu_apic_pt_lo[512] __attribute__((aligned(4096))); /* 0xFEC00000 */
static uint64_t wubu_apic_pt_hi[512] __attribute__((aligned(4096))); /* 0xFEE00000 */

/* Scratch pool for mapping extra MMIO (PCI device BARs live above 1GB on
 * q35 -- e.g. the xHCI controller at ~0xC1040000).  These PTs plug into
 * wubu_apic_pd (the PDP[3] window 0xC0000000..0xDFFFFFFF). */
static uint64_t wubu_pt_pool[8][512] __attribute__((aligned(4096)));
static uint32_t wubu_pt_pool_used;

#define WUBU_KERNEL_VIRT_BASE 0xffffffff80000000ull

static uint64_t wubu_apic_phys(uint64_t va) { return va - WUBU_KERNEL_VIRT_BASE; }

/* Identity-map `pages` 4KB pages of physical MMIO inside the PDP[3]
 * window (0xC0000000..0xDFFFFFFF).  Returns the VA (= phys) or 0.
 * Requires wubu_apic_map_mmio() to have run (PDP[3] -> wubu_apic_pd). */
uint64_t wubu_map_phys_range(uint64_t phys, uint32_t pages)
{
    if (phys < 0xC0000000ull ||
        phys + (uint64_t)pages * 4096 > 0xE0000000ull)
        return 0;
    uint64_t cur = phys;
    while (pages > 0) {
        uint32_t pd_idx = (uint32_t)((cur - 0xC0000000ull) >> 21);
        uint32_t pt_off  = (uint32_t)((cur >> 12) & 0x1FF);
        uint32_t n = 512 - pt_off;
        if (n > pages) n = pages;
        if (wubu_pt_pool_used >= 8) return 0;
        uint64_t *pt = wubu_pt_pool[wubu_pt_pool_used++];
        for (int j = 0; j < 512; j++) pt[j] = 0;
        wubu_apic_pd[pd_idx] = wubu_apic_phys((uint64_t)pt) | 3;
        for (uint32_t i = 0; i < n; i++)
            pt[pt_off + i] = (cur + (uint64_t)i * 4096) | 3;
        cur += (uint64_t)n * 4096;
        pages -= n;
    }
    /* TLB flush */
    __asm__ volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax");
    return phys;
}

static void wubu_apic_map_mmio(void)
{
    uint64_t *pdp_ident = (uint64_t *)0x301000;   /* crt0: PDP (identity slot) */

    for (int i = 0; i < 512; i++) {
        wubu_apic_pd[i]    = 0;
        wubu_apic_pt_lo[i] = 0;
        wubu_apic_pt_hi[i] = 0;
    }
    /* PDP[3] = VA range 0xC0000000..0xDFFFFFFF -> PD_apic.
     * PD[0x1F6] = 0xFEC00000 2MB (IOAPIC), PD[0x1F7] = 0xFEE00000 2MB (LAPIC). */
    pdp_ident[3] = wubu_apic_phys((uint64_t)wubu_apic_pd) | 3;
    wubu_apic_pd[0x1F6] = wubu_apic_phys((uint64_t)wubu_apic_pt_lo) | 3;
    wubu_apic_pd[0x1F7] = wubu_apic_phys((uint64_t)wubu_apic_pt_hi) | 3;
    for (int i = 0; i < 512; i++) {
        wubu_apic_pt_lo[i] = (0xFEC00000ull + (uint64_t)i * 4096) | 3;
        wubu_apic_pt_hi[i] = (0xFEE00000ull + (uint64_t)i * 4096) | 3;
    }
    /* Flush the TLB. */
    __asm__ volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax");
}

int wubu_apic_enable(void)
{
    wubu_apic_map_mmio();
    if (apic_init() != 0) return -1;      /* LAPIC on + IOAPIC detected */

    /* The I/O APIC delivery path is NOT used (it produced a spurious
     * vector-0 #DE during the boot bisect).  Instead the system tick
     * comes from the LAPIC timer itself: PERIODIC (mode 0b01 = 1<<17 --
     * 0b10 would be TSC-deadline, which fires once and dies), vector 32.
     * count = bus_hz * period / div (QEMU x86 bus ~ 1 GHz, div 16). */
    lapic_write(LAPIC_LVT_TIMER, 32 | (0x1 << 17));   /* periodic + vec 32 */
    lapic_write(LAPIC_TIMER_DIV, 0x3);                /* divide by 16 */
    lapic_write(LAPIC_TIMER_INIT_CNT, 625000);        /* ~100 Hz @ 1 GHz bus */

    if (klog_printf) {
        uint32_t svr = lapic_read(LAPIC_SVR);
        klog_printf("WuBuOS: APIC enabled, LAPIC timer @ ~100 Hz (svr=%x)\n",
                    (unsigned)svr);
    }
    return 0;
}
