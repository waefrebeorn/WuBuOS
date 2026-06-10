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

#endif
