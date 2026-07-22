/*
 * vsl_syscall_mac_internal.h  --  VSL macOS Syscall Dispatch (Internal)
 *
 * Shared types and declarations for the macOS syscall dispatch
 * sub-modules. Extern consumers should use vsl_syscall_numbers_mac.h
 * and vsl_mach_ipc.h only.
 *
 * Split into:
 *   vsl_syscall_mac.c       — core dispatch + semaphores + entry point
 *   vsl_syscall_mac_bsd.c   — BSD syscall handlers + dispatch table
 *   vsl_syscall_mac_mach.c  — Mach trap handlers + port table + dispatch table
 */
#ifndef VSL_SYSCALL_MAC_INTERNAL_H
#define VSL_SYSCALL_MAC_INTERNAL_H

#include "vsl/vsl_syscall_internal.h"
#include "vsl/vsl_syscall_numbers_mac.h"
#include "vsl/vsl_macho.h"
#include "vsl/vsl_mach_ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/shm.h>

/* -- Semaphore management (used by Mach semaphore traps) ----------- */
typedef struct mac_sem {
    char name[64];
    sem_t *sem;
} mac_sem_t;

#define MAC_MAX_SEMS 64
extern mac_sem_t g_mac_sems[];
extern int g_n_mac_sems;

/* -- Syscall handler type ----------------------------------------- */
typedef int64_t (*mac_syscall_fn)(uint64_t a, uint64_t b, uint64_t c,
                                   uint64_t d, uint64_t e, uint64_t f);

/* -- Helper: translate macOS errno to VSL errno ------------------- */
static inline int64_t mac_errno(int err) {
    return -(int64_t)err;
}

/* -- BSD dispatch table (defined in vsl_syscall_mac_bsd.c) -------- */
extern const mac_syscall_fn mac_bsd_table[512];

/* -- Mach trap dispatch table (defined in vsl_syscall_mac_mach.c) - */
extern const mac_syscall_fn mac_trap_table[96];

/* -- Mach port table (defined in vsl_syscall_mac_mach.c) ---------- */
#define MAC_MAX_PORTS 256
typedef struct {
    mach_port_name_t name;
    int              type;
    void            *object;
} mac_port_t;
extern mac_port_t g_mac_ports[];
extern int g_mac_n_ports;

/* -- mmap flag translation (used by BSD mmap handler) ------------- */
int mac_translate_mmap_flags(int mac_flags);

/* -- Forward declarations for dispatch tables --------------------- */
extern const mac_syscall_fn mac_bsd_table[512];
extern const mac_syscall_fn mac_trap_table[96];

#endif /* VSL_SYSCALL_MAC_INTERNAL_H */
