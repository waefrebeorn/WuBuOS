/*
 * interrupt_io.h -- inline port-I/O helpers for the interrupt subsystem.
 *
 * These were file-local `static inline` helpers in interrupt.c. They are
 * shared by interrupt.c and the extracted leaf modules (PIT, APIC, syscall),
 * so they live here as inline definitions. No god-header: only the three
 * port-I/O primitives + lidt.
 */
#ifndef WUBU_INTERRUPT_IO_H
#define WUBU_INTERRUPT_IO_H

#include <stdint.h>
/* --- 8259 PIC registers --- */
#define PIC1_CMD        0x20
#define PIC1_DATA       0x21
#define PIC2_CMD        0xA0
#define PIC2_DATA       0xA1

/* PIT IRQ handler (defined in interrupt.c, registered by PIT init) */
void pit_handler(uint8_t irq, void *ctx);


/* --- PIT (Programmable Interval Timer) registers --- */
#define PIT_CH0_DATA   0x40
#define PIT_CH1_DATA   0x41
#define PIT_CH2_DATA   0x42
#define PIT_CMD        0x43

/* Defined once in interrupt.c */
extern uint32_t g_pit_hz;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void lidt(const void *ptr) {
    __asm__ volatile ("lidt %0" : : "m"(*(const uint8_t(*)[6])ptr));
}

#endif /* WUBU_INTERRUPT_IO_H */
