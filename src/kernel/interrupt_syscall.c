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

    /* Gap H1: register + name the first syscalls (the docs table). */
    extern int syscall_register(uint32_t, syscall_fn_t);
    extern void syscall_set_name(uint32_t, const char *);
    extern int64_t wubu_sys_get_uptime(int64_t, int64_t, int64_t,
                                       int64_t, int64_t, int64_t);
    extern int64_t wubu_sys_klog(int64_t, int64_t, int64_t,
                                 int64_t, int64_t, int64_t);
    syscall_register(0, wubu_sys_get_uptime);
    syscall_set_name(0, "get_uptime");
    syscall_register(1, wubu_sys_klog);
    syscall_set_name(1, "klog");

    return 0;
#else
    return -1;
#endif
}

/* ---- the first syscalls (gap H1) ------------------------------------ */

/* syscall 0: get_uptime -- the AGI's uptime in ms (arg0: unused). */
int64_t wubu_sys_get_uptime(int64_t a1, int64_t a2, int64_t a3,
                            int64_t a4, int64_t a5, int64_t a6)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    extern uint64_t wubu_agi_kernel_uptime_ms(void *);
    extern void *wubu_agi_kernel_global(void);
    return (int64_t)wubu_agi_kernel_uptime_ms(wubu_agi_kernel_global());
}

/* syscall 1: klog -- a userspace log line (the arg0 pointer is validated
 * by the dispatcher's H2 kernel-range check). Bounded copy. */
int64_t wubu_sys_klog(int64_t a1, int64_t a2, int64_t a3,
                      int64_t a4, int64_t a5, int64_t a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    const char *msg = (const char *)(uintptr_t)a1;
    if (!msg) return -1;
    char buf[192];
    size_t n = 0;
    for (const char *p = msg; n + 1 < sizeof(buf) && *p; p++) buf[n++] = *p;
    buf[n] = '\0';
    extern void klog_printf(const char *, ...);
    klog_printf("sys: %s\n", buf);
    return (int64_t)n;
}
