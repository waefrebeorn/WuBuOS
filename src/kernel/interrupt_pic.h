/*
 * interrupt_pic.h -- 8259 PIC (Programmable Interrupt Controller) layer.
 *
 * Self-contained concern extracted from interrupt.c (2026-07-16). Holds the
 * PIC I/O port macros, the ICW configuration constants, and the public
 * interface for PIC remapping + EOI. IRQ routing (IRQRoute linked list) is
 * also PIC-context, so its API lives here too. No god-header: only the two
 * adjacent PIC concerns are present.
 */
#ifndef WUBU_INTERRUPT_PIC_H
#define WUBU_INTERRUPT_PIC_H

#include <stdint.h>

/* --- 8259 PIC I/O ports (master + slave) --- */
#define PIC1_CMD        0x20
#define PIC1_DATA       0x21
#define PIC2_CMD        0xA0
#define PIC2_DATA       0xA1

#define PIC_EOI         0x20    /* End of Interrupt command */

/* ICW1 (initialization command word 1) bits */
#define ICW1_ICW4       0x01    /* ICW4 needed */
#define ICW1_SINGLE     0x02    /* Single (cascade) mode */
#define ICW1_INTERVAL4  0x04    /* Call address interval 4 (8) */
#define ICW1_LEVEL      0x08    /* Level triggered (edge) */
#define ICW1_INIT       0x10    /* Initialization */

/* ICW4 (initialization command word 4) bits */
#define ICW4_8086       0x01    /* 8086/88 mode */
#define ICW4_AUTO       0x02    /* Auto EOI */
#define ICW4_BUF_SLAVE  0x08    /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C    /* Buffered mode/master */
#define ICW4_SFNM       0x10    /* Special fully nested mode */

/*
 * Remap the two 8259 PICs so IRQs 0-15 map to vectors offset1..offset1+7
 * (master) and offset2..offset2+7 (slave). Called once at interrupt_init
 * time on bare metal: pic_remap(32, 40) routes IRQ0-7 -> 32-39, IRQ8-15 ->
 * 40-47, leaving the first 32 vectors free for exceptions.
 */
void pic_remap(uint8_t offset1, uint8_t offset2);

/*
 * Send End-Of-Interrupt to the appropriate PIC(s) for a remapped vector.
 * vector 32-39 -> master; 40-47 -> slave then master. Used by interrupt_eoi
 * in interrupt.c (which also EOIs the I/O APIC when present).
 */
void pic_eoi(uint8_t vector);

/* ------------------------------------------------------------------
 * IRQ Routing (PCI/MSI legacy routing via the I/O APIC)
 *
 * A small singly-linked list of IRQRoute entries mapping a source IRQ to a
 * destination IDT vector + target APIC. irq_route_add programs the I/O APIC
 * when the source is within the APIC's IRQ range; irq_route_remove tears the
 * entry down. Extracted from interrupt.c so the routing bookkeeping is its
 * own self-contained module rather than living inside the dispatch core.
 * ----------------------------------------------------------------- */
typedef struct IRQRoute {
    uint8_t  src_irq;        /* Source IRQ (PCI pin, APIC line) */
    uint8_t  dst_vector;     /* Destination vector (32-255) */
    uint8_t  dest_apic_id;   /* Target APIC ID */
    uint16_t flags;          /* LEVEL/EDGE, ACTIVE_HIGH/LOW */
    struct IRQRoute *next;
} IRQRoute;

int  irq_route_add(uint8_t src_irq, uint8_t dst_vector,
                   uint8_t dest_apic_id, uint16_t flags);
int  irq_route_remove(uint8_t src_irq);

#endif /* WUBU_INTERRUPT_PIC_H */
