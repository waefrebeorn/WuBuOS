/*
 * interrupt_syscall.c — Syscall dispatcher + registry
 *
 * Split from interrupt.c: the syscall table (register/handler/audit) plus
 * syscall_init() (STAR/LSTAR MSR setup) and the first default syscalls.
 * Separated so the core interrupt dispatch stays lean while the syscall
 * plumbing is isolated.
 *
 * C11.
 */

#include "interrupt.h"
#include "interrupt_apic.h"
#include "tasking.h"
#include "memory.h"
#include "interrupt_io.h"
#include <stdint.h>
#include <string.h>
#include <signal.h>

/* ------------------------------------------------------------------
 * SYSCALL Dispatcher (C-level)
 * ------------------------------------------------------------------ */

/* Syscall function pointer type */
typedef int64_t (*syscall_fn_t)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

/* Syscall table (up to 512 syscalls) */
#define MAX_SYSCALLS 512
static syscall_fn_t g_syscall_table[MAX_SYSCALLS] = {0};

/* Gap H1/H3: the syscall registry -- name + call audit per number. */
typedef struct {
    const char *name;
    uint64_t    calls;
    uint64_t    last_ts_ms;
} syscall_meta_t;
static syscall_meta_t g_syscall_meta[MAX_SYSCALLS];

/* Register a syscall handler */
int syscall_register(uint32_t num, syscall_fn_t handler) {
    if (num >= MAX_SYSCALLS) return -1;
    g_syscall_table[num] = handler;
    return 0;
}

/* Gap H1: name a syscall (the docs table). */
void syscall_set_name(uint32_t num, const char *name) {
    if (num >= MAX_SYSCALLS) return;
    g_syscall_meta[num].name = name;
}

/* Gap H3: the audit accessors. */
uint64_t syscall_call_count(uint32_t num) {
    if (num >= MAX_SYSCALLS) return 0;
    return g_syscall_meta[num].calls;
}
const char *syscall_name(uint32_t num) {
    if (num >= MAX_SYSCALLS) return NULL;
    return g_syscall_meta[num].name;
}

/* Syscall dispatcher - called from syscall_entry assembly stub.
 * Returns the result in RAX (the syscall epilogue never restores rax, so
 * the return value survives into sysretq -- the old frame->rax write
 * clobbered the saved R11 slot at +112 and was never restored). */
int64_t syscall_handler(InterruptFrame *frame, uint64_t num) {
    (void)frame;

    if (num < MAX_SYSCALLS && g_syscall_table[num]) {
        /* Extract args from frame (RDI, RSI, RDX, R10, R8, R9) */
        int64_t arg1 = frame->rdi;
        int64_t arg2 = frame->rsi;
        int64_t arg3 = frame->rdx;
        int64_t arg4 = frame->r10;
        int64_t arg5 = frame->r8;
        int64_t arg6 = frame->r9;

        /* Gap H2: validate the pointer-sized args -- nothing may point
         * into the kernel's own window (0xffffffff80000000..0xffffffff
         * a0000000). The user space lives outside it; an arg inside is
         * an attempted kernel poke -> EFAULT (-14). */
        const int64_t klo = (int64_t)0xffffffff80000000ull;
        const int64_t khi = (int64_t)0xffffffffa0000000ull;
        if ((arg1 >= klo && arg1 < khi) || (arg2 >= klo && arg2 < khi) ||
            (arg3 >= klo && arg3 < khi) || (arg4 >= klo && arg4 < khi) ||
            (arg5 >= klo && arg5 < khi) || (arg6 >= klo && arg6 < khi))
            return -14;   /* -EFAULT */

        /* Gap H3: the audit counters. */
        g_syscall_meta[num].calls++;

        return g_syscall_table[num](arg1, arg2, arg3, arg4, arg5, arg6);
    } else {
        return -1;  /* ENOSYS */
    }
}

/* ------------------------------------------------------------------
 * SYSCALL/SYSRET fast-path setup (STAR/LSTAR/CSTAR/FMASK/EFER MSRs)
 * ------------------------------------------------------------------ */
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
#ifdef MYSEED_METAL
    /* (the uptime counter lives in the tasking layer) */
    return (int64_t)task_tick_count();
#else
    return 0;
#endif
}

/* syscall 1: klog -- write a kernel log line (arg0: cstr pointer). */
int64_t wubu_sys_klog(int64_t a1, int64_t a2, int64_t a3,
                      int64_t a4, int64_t a5, int64_t a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
#ifdef MYSEED_METAL
    /* Gap H2 already rejected pointers into the kernel window; this is
     * a user pointer, readable from ring 0. */
    if (a1) {
        const char *s = (const char *)a1;
        /* bounded: 512 chars max, never walk off the end */
        char buf[512];
        for (size_t i = 0; i < sizeof(buf) - 1; i++) {
            buf[i] = s[i];
            if (s[i] == '\0') break;
        }
        buf[sizeof(buf) - 1] = '\0';
        klog_printf("%s", buf);
        return 0;
    }
    return -1;   /* EINVAL */
#else
    return -1;
#endif
}
