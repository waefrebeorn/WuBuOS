/*
 * interrupt.h — My Seed IDT/PIC Interrupt Controller
 */
#ifndef MYSEED_INTERRUPT_H
#define MYSEED_INTERRUPT_H
#include <stdint.h>

typedef void (*ISRHandler)(uint8_t irq, void *ctx);

/* Init IDT + PIC */
int  interrupt_init(void);
void interrupt_shutdown(void);

/* Register handler for IRQ number */
void interrupt_register(uint8_t irq, ISRHandler handler, void *ctx);

/* Unregister */
void interrupt_unregister(uint8_t irq);

/* Acknowledge IRQ to PIC */
void interrupt_eoi(uint8_t irq);

/* Disable/enable interrupts */
void interrupt_disable(void);
void interrupt_enable(void);

/* Simulate IRQ (for hosted testing) */
void interrupt_fire(uint8_t irq);

#endif
