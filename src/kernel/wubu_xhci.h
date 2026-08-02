/*
 * wubu_xhci.h  --  the xHCI controller driver (gap E1)
 *
 * wubu_usb.h was design-only; this is the implementation: the xHCI
 * capability/operational register model, the controller reset + run
 * sequence, the command ring, and device-slot allocation. The HID
 * interrupt-in transfer path is the follow-on; the controller driver
 * (probe/start/slots) is the E1 close.
 *
 * Freestanding C11, no heap (the ring buffers are static).
 */
#ifndef WUBU_XHCI_H
#define WUBU_XHCI_H

#include <stdint.h>

typedef struct {
    int      present;      /* a xHCI controller is on the PCI bus */
    uint64_t mmio_base;    /* the capability registers' MMIO base */
    uint32_t cap_length;   /* the operational register offset */
    uint32_t hcs_params;   /* HCSPARAMS1: slots, ports, contexts */
    uint32_t hcc_params;   /* HCCPARAMS1: 64-bit addressing, xECP */
    uint32_t db_off;       /* the doorbell array offset */
    uint32_t rt_off;       /* the runtime registers offset */
    uint64_t op_base;      /* the operational registers' address */
    uint32_t port_count;   /* the number of root ports */
    uint32_t slot_count;   /* the number of device slots */
} wubu_xhci_t;

/* Find the xHCI controller (PCI class 0x0C0330) + read its caps.
 * Returns 0 with present=1 on success. */
int wubu_xhci_probe(wubu_xhci_t *out);

/* Reset the controller, point the command ring, enable the run bit.
 * Returns 0 on success. */
int wubu_xhci_start(wubu_xhci_t *xh);

/* Allocate a device slot (Address Device command). Returns the slot id
 * (>= 1) or -1. */
int wubu_xhci_slot_alloc(wubu_xhci_t *xh);

/* Test hook: parse the capability registers from a synthetic MMIO
 * image (host tests). */
int wubu_xhci_probe_regs(uint64_t mmio, wubu_xhci_t *out);

#endif
