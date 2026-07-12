/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "interrupt.h"
#include "interrupt_apic.h"
#include "tasking.h"
#include "memory.h"
#include "interrupt_io.h"
#include <stdint.h>
#include <string.h>
#include <signal.h>

int syscall_init(void) {
#ifdef MYSEED_METAL
    /* STAR MSR: bits 63:48 = SYSCALL CS/SS, bits 47:32 = SYSRET CS/SS */
    /* Kernel GDT: code=0x08 (index 1), data=0x10 (index 2) */
    /* User GDT: code=0x23 (index 4, RPL=3), data=0x2B (index 5, RPL=3) */
    /* SYSCALL: CS=0x08, SS=0x10 → SYSRET: CS=0x23, SS=0x2B */
    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x23 << 48);
    wrmsr(MSR_IA32_STAR, star);

    /* LSTAR: 64-bit syscall entry point */
    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry);

    /* CSTAR: compat mode syscall entry (not used) */
    wrmsr(MSR_IA32_CSTAR, 0);

    /* FMASK: RFLAGS to clear on syscall (IF, TF, DF, RF, NT) */
    wrmsr(MSR_IA32_FMASK, 0x4700);  /* IF=0x200, TF=0x100, DF=0x400, RF=0x10000, NT=0x4000 */

    /* Enable SYSCALL/SYSRET in EFER */
    uint64_t efer = rdmsr(0xC0000080);
    efer |= 1;  /* SCE bit */
    wrmsr(0xC0000080, efer);

    return 0;
#else
    return -1;
#endif
}
