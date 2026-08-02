/*
 * wubu_smp.h  --  SMP: application-processor bring-up (gap I2)
 *
 * Only the bootstrap CPU was running. This module brings the APs up:
 * it programs the local APIC's ICR with the INIT-SIPI-SIPI sequence,
 * the APs run a 16-bit trampoline (copied to low memory) that jumps to
 * 64-bit long mode, and each AP parks in an idle loop bumping a per-CPU
 * counter the BSP can observe. Full per-CPU scheduling (the TSS RSP0 +
 * the LAPIC timer per CPU) is the follow-on; the bring-up + the
 * visibility of the APs is the I2 close.
 *
 * Freestanding C11, no heap.
 */
#ifndef WUBU_SMP_H
#define WUBU_SMP_H

#include <stdint.h>
#include <stddef.h>

#define WUBU_SMP_MAX_CPU  8

/* The number of CPUs the BSP detected (LAPIC IDs; at least 1). */
uint32_t wubu_smp_detect(void);

/* Send INIT-SIPI-SIPI to all APs; they run the trampoline + park.
 * Returns the number of APs that reported alive within the timeout. */
uint32_t wubu_smp_start_aps(void);

/* The count of CPUs observed alive (BSP + APs). */
uint32_t wubu_smp_cpu_count(void);

/* Per-CPU alive counter (the APs bump this every spin). */
uint32_t wubu_smp_alive(uint32_t cpu);

#endif
