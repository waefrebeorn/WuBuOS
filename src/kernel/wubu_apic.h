/*
 * wubu_apic.h -- local APIC + I/O APIC bring-up (q35-correct delivery).
 *
 * QEMU q35 / real Intel chipsets wire the PIT, PS/2 keyboard/mouse and
 * every other legacy INTx line to the I/O APIC, not the 8259 PIC.  x86-64
 * CPUs also boot with the LAPIC enabled and its LINT0 (the PIC's INTR
 * input) masked, so PIC-delivered interrupts never reach the IDT.  The
 * kernel's interrupt path therefore needs the APIC: map the LAPIC/IOAPIC
 * MMIO (0xFEC00000..0xFF000000, above the crt0 identity map), enable the
 * LAPIC, and route each ISA IRQ through the I/O APIC redirection table to
 * the kernel's existing vectors (32+n).
 */
#ifndef WUBU_APIC_H
#define WUBU_APIC_H

#include <stdint.h>

int wubu_apic_enable(void);

/* Identity-map physical MMIO (4KB pages) inside the 0xC0000000..0xDFFFFFFF
 * window (used for PCI device BARs above 1GB, e.g. xHCI at ~0xC1040000).
 * Returns the VA (= phys) or 0.  Must run after wubu_apic_enable(). */
uint64_t wubu_map_phys_range(uint64_t phys, uint32_t pages);

#endif /* WUBU_APIC_H */
