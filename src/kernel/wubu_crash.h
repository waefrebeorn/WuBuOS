/*
 * wubu_crash.h  --  crash dump to the disk + boot pickup (gaps A8/F10)
 *
 * The panic path writes a fixed-size record to the LAST sectors of the
 * AHCI port-0 disk (the sim disk's tail) -- the A7 panic ring + the
 * reason + the faulting registers. The boot picks it up and reports it,
 * so a crash is EVIDENCE on the disk, not just serial vapor. The write
 * is raw-sector (ahci_read/ahci_write): no heap, ISR-safe by design.
 */
#ifndef WUBU_CRASH_H
#define WUBU_CRASH_H

#include <stdint.h>

#define WUBU_CRASH_MAGIC   0x43524153u   /* 'CRAS' */
#define WUBU_CRASH_RING_SZ 2048          /* the panic ring snapshot */

/* The on-disk record (one 512-byte sector). */
typedef struct {
    uint32_t magic;
    uint32_t seq;            /* crash counter (bumped per dump) */
    uint64_t rip;
    uint64_t rsp;
    uint32_t vector;         /* the fault vector, or 0 */
    char     reason[64];
    char     ring[WUBU_CRASH_RING_SZ];  /* the A7 panic ring */
} wubu_crash_record_t;

/* Disk location: the last sector of the 8 MB sim disk. */
#define WUBU_CRASH_LBA  (8 * 1024 * 1024 / 512 - 1)

/* Write the crash record (called from the panic path). 0 on success. */
int wubu_crash_dump(const char *reason, uint64_t rip, uint64_t rsp,
                    uint32_t vector);

/* Read + report the record if one exists. Returns 1 when a crash was
 * found (and reports it), 0 when the disk is clean. */
int wubu_crash_pickup(void);

#endif
