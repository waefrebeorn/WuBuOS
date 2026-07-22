/*
 * vsl_syscall_mac.c  --  VSL macOS (XNU) Syscall Dispatch (Core)
 *
 * Central entry point for macOS syscall dispatch.
 * BSD handlers are in vsl_syscall_mac_bsd.c.
 * Mach trap handlers + port table are in vsl_syscall_mac_mach.c.
 *
 * C11, self-contained sub-modules via vsl_syscall_mac_internal.h.
 */
#include "vsl/vsl_syscall_mac_internal.h"
#include <stdio.h>

/* -- Global semaphore state (used by Mach semaphore traps) --------- */
mac_sem_t g_mac_sems[MAC_MAX_SEMS];
int g_n_mac_sems = 0;

/* ===================================================================
 * MAC SYSTEM CALL DISPATCH (entry point)
 * Called by VSL when a macOS binary makes a syscall.
 * =================================================================== */
int64_t vsl_mac_syscall_dispatch(uint64_t syscall_raw, uint64_t a, uint64_t b,
                                  uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    uint32_t syscall_class = (uint32_t)((syscall_raw & MAC_SYSCALL_CLASS_MASK) >> MAC_SYSCALL_CLASS_SHIFT);
    uint32_t syscall_num  = (uint32_t)(syscall_raw & MAC_SYSCALL_NUM_MASK);

    switch (syscall_class) {
        case MAC_SYSCALL_CLASS_MACH: {
            /* Mach trap */
            if (syscall_num < 96 && mac_trap_table[syscall_num]) {
                return mac_trap_table[syscall_num](a, b, c, d, e, f);
            }
            /* Unknown Mach trap: stub */
            fprintf(stderr, "[vsl_mac] unhandled Mach trap %u (0x%x)\n",
                    syscall_num, syscall_num);
            return mac_errno(ENOSYS);
        }

        case MAC_SYSCALL_CLASS_BSD: {
            /* BSD Unix syscall */
            if (syscall_num < 512 && mac_bsd_table[syscall_num]) {
                return mac_bsd_table[syscall_num](a, b, c, d, e, f);
            }
            /* Unknown BSD syscall */
            fprintf(stderr, "[vsl_mac] unhandled BSD syscall %u\n", syscall_num);
            return mac_errno(ENOSYS);
        }

        default:
            fprintf(stderr, "[vsl_mac] unknown syscall class %u (raw=0x%lx)\n",
                    syscall_class, (unsigned long)syscall_raw);
            return mac_errno(EINVAL);
    }
}
