/*
 * interrupt_pic.c -- 8259 PIC layer + IRQ routing for the WuBuOS kernel.
 *
 * Extracted from the monolithic interrupt.c (2026-07-16) as a self-contained
 * module. Owns:
 *   - PIC ICW programming (pic_remap)
 *   - PIC End-Of-Interrupt (pic_eoi)
 *   - the IRQRoute linked list (irq_route_add / irq_route_remove)
 *
 * It pulls only what it needs: port I/O + lidt from interrupt_io.h, APIC
 * routing/allocator decls from interrupt_apic.h, and the public PIC/routing
 * API from interrupt_pic.h. No god-header — only PIC + routing concerns.
 */
#include "interrupt_pic.h"
#include "interrupt_io.h"
#include "interrupt_apic.h"
#include "memory.h"

#include <stdint.h>

/* ------------------------------------------------------------------
 * PIC Remapping (bare metal only)
 * ----------------------------------------------------------------- */
void pic_remap(uint8_t offset1, uint8_t offset2) {
#ifdef MYSEED_METAL
    uint8_t mask1 = inb(PIC1_DATA);  /* Save masks */
    uint8_t mask2 = inb(PIC2_DATA);

    /* ICW1: Initialize PICs */
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);

    /* ICW2: Remap IRQs to vectors offset1/offset2 */
    outb(PIC1_DATA, offset1);   /* Master: IRQ0-7 -> vectors offset1..offset1+7 */
    outb(PIC2_DATA, offset2);   /* Slave:  IRQ8-15 -> vectors offset2..offset2+7 */

    /* ICW3: Master IRQ2 connects to slave */
    outb(PIC1_DATA, 0x04);      /* Master: slave on IRQ2 */
    outb(PIC2_DATA, 0x02);      /* Slave:  cascade identity 2 */

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    /* Restore masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
#else
    (void)offset1; (void)offset2;
#endif
}

/* ------------------------------------------------------------------
 * PIC EOI
 * ----------------------------------------------------------------- */
void pic_eoi(uint8_t vector) {
#ifdef MYSEED_METAL
    if (vector >= 40) {           /* IRQ8-15 (remapped to 40-47) */
        outb(PIC2_CMD, PIC_EOI);  /* Slave */
    }
    if (vector >= 32) {           /* IRQ0-7 (remapped to 32-39) */
        outb(PIC1_CMD, PIC_EOI);  /* Master */
    }
#else
    (void)vector;
#endif
}

/* ------------------------------------------------------------------
 * IRQ Routing Infrastructure (PCI/MSI legacy routing via I/O APIC)
 * ----------------------------------------------------------------- */

static IRQRoute *g_irq_routes = NULL;

int irq_route_add(uint8_t src_irq, uint8_t dst_vector, uint8_t dest_apic_id, uint16_t flags) {
    IRQRoute *route = (IRQRoute *)mem_alloc(sizeof(IRQRoute));
    if (!route) return -1;

    route->src_irq = src_irq;
    route->dst_vector = dst_vector;
    route->dest_apic_id = dest_apic_id;
    route->flags = flags;
    route->next = g_irq_routes;
    g_irq_routes = route;

#ifdef MYSEED_METAL
    /* Program I/O APIC if this is a legacy IRQ (ring-0 hardware poke) */
    if (src_irq < g_ioapic_irq_count) {
        ioapic_route_irq(src_irq, dst_vector, dest_apic_id);
    }
#endif
    return 0;
}

int irq_route_remove(uint8_t src_irq) {
    IRQRoute **pp = &g_irq_routes;
    while (*pp) {
        if ((*pp)->src_irq == src_irq) {
            IRQRoute *tmp = *pp;
            *pp = (*pp)->next;
            mem_free(tmp);
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
}
