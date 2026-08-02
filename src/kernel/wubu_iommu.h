/*
 * wubu_iommu.h  --  IOMMU/VT-d discovery (gap E5)
 *
 * The anticheat's below-OS plane: DMA remapping. This module finds the
 * ACPI DMAR table (the firmware's DMA-remapping report), parses the
 * reported base address, and reads the VT-d engine's capability
 * registers (the hardware page-table formats it supports + the number
 * of fault-log entries). Full domain wiring + fault logging are the
 * follow-on; the discovery + capability gate is the E5 close.
 *
 * Freestanding C11, no heap.
 */
#ifndef WUBU_IOMMU_H
#define WUBU_IOMMU_H

#include <stdint.h>

typedef struct {
    int      found;         /* a DMAR table with a usable engine exists */
    uint32_t cap;           /* the engine's CAP register */
    uint32_t ecap;          /* the engine's ECAP register */
    uint64_t rtaddr;        /* the root-table address (may be 0 pre-setup) */
    uint16_t segment;       /* the first reported PCI segment */
    uint8_t  version;       /* the DMAR table revision */
    uint8_t  flags;         /* DMAR flags (INTR_REMAP etc.) */
} wubu_iommu_t;

/* Discover + read the VT-d engine. Returns 0 with found=1 on success;
 * 0 with found=0 when the firmware has no DMAR table; -1 on error. */
int wubu_iommu_probe(wubu_iommu_t *out);

/* Test hook: scan a given DMAR table image (host tests pass a
 * synthetic buffer). */
int wubu_iommu_probe_table(const uint8_t *dmar, uint32_t len,
                           wubu_iommu_t *out);

#endif
