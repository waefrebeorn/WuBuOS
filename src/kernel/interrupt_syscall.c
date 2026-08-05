/* interrupt_syscall.c — Syscall registration + management table
 *
 * Extracted from interrupt.c — the syscall_fn_t typedef, syscall_register,
 * syscall_set_name, and related management functions. Separated so the
 * core interrupt dispatch stays lean while syscall plumbing is isolated.
 *
 * C11.
 */
#include "interrupt.h"

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