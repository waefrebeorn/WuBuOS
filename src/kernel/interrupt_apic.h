/*
 * interrupt_apic.h -- shared APIC/LAPIC/MSR primitives for the interrupt
 * subsystem split out of interrupt.c.
 *
 * Holds the MSR address macros, the LAPIC register offsets, the rdmsr/wrmsr/
 * lapic / ioapic inline accessors, and extern declarations for the shared
 * globals (g_lapic_base, g_io_priv_ok) + sigsegv_handler. Included by
 * interrupt.c and the extracted leaf modules (apic/syscall/timer). Not a
 * god-header: only APIC-low-level concerns live here.
 */
#ifndef WUBU_INTERRUPT_APIC_H
#define WUBU_INTERRUPT_APIC_H

#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include "interrupt_io.h"

/* --- Shared globals (defined once in interrupt.c) --- */
extern volatile uint32_t *g_lapic_base;
extern volatile uint32_t *g_ioapic_base;
extern int g_io_priv_ok;
void sigsegv_handler(int sig, siginfo_t *info, void *ucontext);

/* --- MSR addresses --- */
#define MSR_IA32_APIC_BASE     0x1B
#define MSR_IA32_STAR          0xC0000081
#define MSR_IA32_LSTAR         0xC0000082
#define MSR_IA32_CSTAR         0xC0000083
#define MSR_IA32_FMASK         0xC0000084
#define MSR_IA32_KERNEL_GS_BASE 0xC0000101

/* --- LAPIC register offsets --- */
#define LAPIC_ID               0x020
#define LAPIC_VERSION          0x030
#define LAPIC_TPR              0x080
#define LAPIC_APR              0x090
#define LAPIC_PPR              0x0A0
#define LAPIC_EOI              0x0B0
#define LAPIC_RRD              0x0C0
#define LAPIC_LDR              0x0D0
#define LAPIC_DFR              0x0E0
#define LAPIC_SVR              0x0F0
#define LAPIC_ISR              0x100
#define LAPIC_TMR              0x180
#define LAPIC_IRR              0x200
#define LAPIC_ESR              0x280
#define LAPIC_ICR_LOW          0x300
#define LAPIC_ICR_HIGH         0x310
#define LAPIC_LVT_TIMER        0x320
#define LAPIC_LVT_THERMAL      0x330
#define LAPIC_LVT_PERF         0x340
#define LAPIC_LVT_LINT0        0x350
#define LAPIC_LVT_LINT1        0x360
#define LAPIC_LVT_ERROR        0x370
#define LAPIC_TIMER_INIT_CNT   0x380
#define LAPIC_TIMER_CUR_CNT    0x390
#define LAPIC_TIMER_DIV        0x3E0
#define LAPIC_SVR_ENABLE       0x100
#define LAPIC_LVT_MASKED       0x10000
#define LAPIC_ICR_DEST_SELF    0x40000
#define LAPIC_ICR_DEST_BROADCAST 0x80000
#define LAPIC_ICR_DEST_ALL_INC  0x80000
#define LAPIC_ICR_DEST_ALL_BUT 0xC0000
#define LAPIC_ICR_LEVEL        0x4000
#define LAPIC_ICR_LEVEL_ASSERT 0x4000
#define LAPIC_ICR_ASSERT       0x4000
#define LAPIC_ICR_TRIGGER_LEVEL 0x8000
#define LAPIC_ICR_DELIVERY_FIXED 0x0000
#define LAPIC_ICR_DELIVERY_NMI 0x400
#define LAPIC_ICR_DELIVERY_INIT 0x500
#define LAPIC_ICR_DELIVERY_STARTUP 0x600
#define LAPIC_TIMER_DIV_16     0x3
#define LAPIC_BASE_DEFAULT     0xFEE00000

#define IOAPIC_BASE_DEFAULT    0xFEC00000
#define IOAPIC_ID              0x00
#define IOAPIC_VER             0x01
#define IOAPIC_ARB             0x02
#define IOAPIC_REDTBL          0x10
#define IOAPIC_REDTBL_HIGH     0x11
#define IOAPIC_RED_ENTRIES(v)  (((v) >> 16) & 0xFF)
#define IOAPIC_RED_DELIV_FIXED 0x0
#define IOAPIC_RED_DELIV_LOW_PRI 0x1
#define IOAPIC_RED_DELIV_SMI   0x2
#define IOAPIC_RED_DELIV_NMI   0x4
#define IOAPIC_RED_DELIV_INIT  0x5
#define IOAPIC_RED_DELIV_EXTINT 0x7
#define IOAPIC_RED_DESTMODE_PHYS 0x0
#define IOAPIC_RED_DESTMODE_LOG  0x8
#define IOAPIC_RED_DELIV_STATUS  0x1000
#define IOAPIC_RED_REMOTE_IRR    0x4000
#define IOAPIC_RED_TRIGGER_LEVEL 0x8000
#define IOAPIC_RED_POLARITY_LOW  0x2000
#define IOAPIC_RED_MASKED        0x10000

/* Assembly stub (isr_stubs.S) */
extern void syscall_entry(void);
/* --- TSS / IST --- */
#define TSS_IST_COUNT   7
#define TSS_SIZE        104
#define IST_EXCEPTION   1
#define IST_NMI         2
#define IST_DEBUG       3
#define IST_TIMER       4

typedef struct TSS {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[TSS_IST_COUNT];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_base;
} __attribute__((packed)) TSS;

extern TSS g_tss;
extern uint32_t g_ioapic_irq_count;
extern uint8_t g_ist_stacks[4][8192];

/* --- Inline MSR / LAPIC / IOAPIC accessors --- */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val, hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" :: "c"(msr), "a"(lo), "d"(hi));
}
static inline uint32_t lapic_read(uint32_t reg) {
    return g_lapic_base[reg / 4];
}
static inline void lapic_write(uint32_t reg, uint32_t val) {
    g_lapic_base[reg / 4] = val;
}
static inline uint32_t ioapic_read(uint32_t reg) {
    return *(volatile uint32_t *)(g_ioapic_base + reg);
}
static inline void ioapic_write(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(g_ioapic_base + reg) = val;
}

#endif /* WUBU_INTERRUPT_APIC_H */
