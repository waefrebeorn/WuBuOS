/*
 * interrupt.h — My Seed IDT/PIC Interrupt Controller
 *
 * Implements full x86_64 IDT with 256 interrupt gates.
 * Uses assembly ISR stubs (isr_stubs.S) for vector entry points.
 * Common handler dispatches to C handlers or preemptive scheduler.
 */
#ifndef MYSEED_INTERRUPT_H
#define MYSEED_INTERRUPT_H

#include <stdint.h>
#include <stddef.h>

/* ──────────────────────────────────────────────────────────────────
 * IDT Structures (x86_64 16-byte interrupt gate)
 * ────────────────────────────────────────────────────────────────── */

#define IDT_ENTRIES       256
#define IDT_GATE_INT      0x8E    /* 64-bit Interrupt Gate: P=1, DPL=0, Type=0xE */
#define IDT_GATE_TRAP     0x8F    /* 64-bit Trap Gate: P=1, DPL=0, Type=0xF */
#define IDT_GATE_TASK     0x85    /* 64-bit Task Gate (legacy, not used) */
#define IDT_DPL_KERNEL    0x00
#define IDT_DPL_USER      0x60    /* DPL=3 for syscall gate */

typedef struct IDTEntry {
    uint16_t offset_low;     /* Offset bits 0-15 */
    uint16_t selector;       /* Code segment selector (kernel CS = 0x08) */
    uint8_t  ist;            /* IST index (0 = none) */
    uint8_t  type_attr;      /* Gate type + DPL + P */
    uint16_t offset_mid;     /* Offset bits 16-31 */
    uint32_t offset_high;    /* Offset bits 32-63 */
    uint32_t reserved;       /* Must be 0 */
} __attribute__((packed)) IDTEntry;

typedef struct IDTPtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) IDTPtr;

/* ──────────────────────────────────────────────────────────────────
 * Interrupt Stack Frame (pushed by CPU + our stubs)
 * ────────────────────────────────────────────────────────────────── */

typedef struct InterruptFrame {
    /* Pushed by common_isr_handler (callee-saved + caller-saved) */
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    /* Pushed by stub + CPU */
    uint64_t vector;         /* Interrupt vector (0-255) */
    uint64_t error_code;     /* Error code (0 for vectors without) */
    uint64_t rip;            /* Return instruction pointer */
    uint64_t cs;             /* Code segment */
    uint64_t rflags;         /* RFLAGS */
    uint64_t rsp;            /* Stack pointer (user/kernel) */
    uint64_t ss;             /* Stack segment */
} InterruptFrame;

/* ──────────────────────────────────────────────────────────────────
 * ISR Handler Type
 * ────────────────────────────────────────────────────────────────── */

typedef void (*ISRHandler)(uint8_t irq, void *ctx);

/* ──────────────────────────────────────────────────────────────────
 * Public API
 * ────────────────────────────────────────────────────────────────── */

/* Initialize IDT, PIC, and install ISR handlers */
int  interrupt_init(void);
void interrupt_shutdown(void);

/* Register C handler for IRQ (0-15 for PIC, 32-47 for remapped) */
void interrupt_register(uint8_t irq, ISRHandler handler, void *ctx);
void interrupt_unregister(uint8_t irq);

/* Acknowledge IRQ to PIC */
void interrupt_eoi(uint8_t irq);

/* Disable/enable interrupts (CLI/STI) */
void interrupt_disable(void);
void interrupt_enable(void);

/* Simulate IRQ (for hosted testing) */
void interrupt_fire(uint8_t irq);

/* Get IDT pointer for lidt */
const IDTPtr *interrupt_idt_ptr(void);

/* Set IDT gate manually (for syscall, etc.) */
void interrupt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector,
                         uint8_t type_attr, uint8_t ist);

/* Bare-metal PIT timer initialization */
#ifdef MYSEED_METAL
int  pit_init(uint32_t hz);         /* Initialize PIT to fire at hz frequency */
void pit_shutdown(void);            /* Disable PIT timer */
#endif

/* External C handler called from assembly */
void isr_dispatch(uint8_t vector, InterruptFrame *frame);

/* ──────────────────────────────────────────────────────────────────
 * Extended API (bare metal)
 * ────────────────────────────────────────────────────────────────── */

#ifdef MYSEED_METAL

/* APIC Initialization */
int apic_init(void);

/* I/O APIC IRQ Routing */
int ioapic_route_irq(uint8_t irq, uint8_t vector, uint8_t dest_apic_id);
int ioapic_mask_irq(uint8_t irq);
int ioapic_unmask_irq(uint8_t irq);

/* LAPIC Timer */
int lapic_timer_init(uint32_t hz, uint8_t vector);
void lapic_eoi(void);

/* IPI (Inter-Processor Interrupts) */
int lapic_send_ipi(uint32_t dest_apic_id, uint8_t vector, uint8_t delivery_mode);
int lapic_broadcast_ipi(uint8_t vector, uint8_t delivery_mode);

/* SYSCALL/SYSRET Fast Path */
int syscall_init(void);

/* IRQ Routing Infrastructure (PCI/MSI) */
int irq_route_add(uint8_t src_irq, uint8_t dst_vector, uint8_t dest_apic_id, uint16_t flags);
int irq_route_remove(uint8_t src_irq);

/* Enhanced IDT Initialization with IST and SYSCALL */
int interrupt_init_full(void);

/* Timer Calibration (PIT vs HPET vs TSC) */
uint64_t timer_calibrate_tsc(void);
int timer_init_deadline(uint64_t ns);

/* Interrupt Statistics / Debug */
void interrupt_count(uint8_t irq);
uint64_t interrupt_get_count(uint8_t irq);

/* Syscall API */
typedef int64_t (*syscall_fn_t)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int syscall_register(uint32_t num, syscall_fn_t handler);
void syscall_handler(InterruptFrame *frame, uint64_t num);

#endif

#endif /* MYSEED_INTERRUPT_H */
