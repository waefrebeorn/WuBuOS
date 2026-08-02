/*
 * wubu_vdso.h  --  kernel vDSO/vsyscall page (gap H6)
 *
 * A read-only page the kernel publishes for user space: it carries the
 * current uptime + tick counters so a future ring-3 process can read
 * the time WITHOUT a syscall trap. The kernel refreshes the counters
 * every tick; user space maps the page read-only (the future user/kernel
 * split's first shared artifact). Freestanding C11, no heap.
 */
#ifndef WUBU_VDSO_H
#define WUBU_VDSO_H

#include <stdint.h>

#define WUBU_VDSO_MAGIC   0x5644534Fu   /* 'VDSO' */

/* The vDSO page layout (one 4K page, fixed offsets -- a stable ABI). */
typedef struct {
    uint32_t magic;          /* WUBU_VDSO_MAGIC */
    uint32_t version;        /* layout version = 1 */
    uint64_t uptime_ms;      /* the AGI supervisor's uptime */
    uint64_t tick;           /* the kernel tick counter */
    uint32_t promoted_total; /* supervisor promotions */
    uint32_t reserved[2];
    char     syscall_stub[16];   /* reserved: the vsyscall entry */
} wubu_vdso_t;

/* Where the vDSO lives (a reserved page above the kernel window, in the
 * demand region's neighborhood; mapped read-only at init). */
#define WUBU_VDSO_VA  0xffffffff9ffe0000ull

/* Publish the vDSO page (maps it read-only + stamps the header).
 * Returns 0 on success. */
int wubu_vdso_init(void);

/* Refresh the counters (call from the timer tick). */
void wubu_vdso_update(uint64_t uptime_ms, uint64_t tick,
                      uint32_t promoted_total);

/* The published vDSO (read-only access for the kernel's own tests). */
const wubu_vdso_t *wubu_vdso_get(void);

#endif
