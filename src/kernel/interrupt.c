/*
 * interrupt.c — My Seed Interrupt Controller (hosted stub)
 */
#include "interrupt.h"
#include <string.h>

#define MAX_IRQ 256

typedef struct {
    ISRHandler handler;
    void      *ctx;
} IRQEntry;

static IRQEntry g_irq_table[MAX_IRQ];

int interrupt_init(void) {
    memset(g_irq_table, 0, sizeof(g_irq_table));
    return 0;
}

void interrupt_shutdown(void) {
    memset(g_irq_table, 0, sizeof(g_irq_table));
}

void interrupt_register(uint8_t irq, ISRHandler handler, void *ctx) {
    g_irq_table[irq].handler = handler;
    g_irq_table[irq].ctx = ctx;
}

void interrupt_unregister(uint8_t irq) {
    g_irq_table[irq].handler = NULL;
    g_irq_table[irq].ctx = NULL;
}

void interrupt_eoi(uint8_t irq) { (void)irq; }
void interrupt_disable(void) { }
void interrupt_enable(void) { }

void interrupt_fire(uint8_t irq) {
    if (g_irq_table[irq].handler)
        g_irq_table[irq].handler(irq, g_irq_table[irq].ctx);
}
