/* interrupt_exceptions.c — Exception handlers (double fault, NMI, page fault, GPF)
 *
 * Extracted from interrupt.c — these are the fault handlers that run in
 * ring-0 when the CPU raises an exception. They are separated so interrupt.c
 * stays focused on IDT setup + dispatch, while the heavy fault-reporting
 * logic (panic dumps, stack walking, register state) lives here.
 *
 * C11, freestanding.
 */
#include "interrupt.h"

void handle_double_fault(InterruptFrame *frame) {
    (void)frame;
    /* In a real kernel: log fault, kill current task, panic */
    while (1) { __asm__ volatile ("hlt"); }
}

/* NMI handler (uses IST2) -- gap A3: distinct from the generic fault
 * path. An NMI is a HARDWARE error (ECC, watchdog, I/O channel check):
 * log it distinctly + dump the panic ring, then halt. Recovery is a
 * future gap; the evidence is what matters now. */
void handle_nmi(InterruptFrame *frame) {
    extern int klog_printf(const char *, ...);
    extern void panic_dump_ring(void);
    klog_printf("WuBuOS NMI: hardware error (rip=%x cs=%x rflags=%x)\n",
                (unsigned)(frame ? (uint64_t)frame->rip : 0),
                (unsigned)(frame ? frame->cs : 0),
                (unsigned)(frame ? (uint64_t)frame->rflags : 0));
    panic_dump_ring();
    while (1) { __asm__ __volatile__ ("hlt"); }
}

/* Page fault handler (uses IST1) */
/* Dump the klog panic ring (gap A7) before halting -- the post-mortem
 * evidence: the last 4 KB of kernel output leading to the fault. */
static void panic_dump_ring(void)
{
    extern int  klog_printf(const char *, ...);
    extern int  klog_ring_snapshot(char *, size_t);
    extern void klog_write_n(const char *, size_t);
    extern uint64_t interrupt_get_count(uint8_t);
    extern struct CTask *task_current(void);
    extern const char *task_name(const struct CTask *);
    static char buf[2048];
    /* C2: name the faulting task + C3: the live fault counters. */
    const struct CTask *cur = task_current();
    klog_printf("-- task: %s --\n", cur ? task_name(cur) : "?");
    klog_printf("-- ex counts: #PF=%u #GP=%u #DF=%u #UD=%u spurious=%u overruns=%u --\n",
                (unsigned)interrupt_get_count(14),
                (unsigned)interrupt_get_count(13),
                (unsigned)interrupt_get_count(8),
                (unsigned)interrupt_get_count(6),
                (unsigned)interrupt_get_count(0xFF),
                (unsigned)interrupt_isr_overruns());
    int n = klog_ring_snapshot(buf, sizeof(buf));
    if (n > 0) {
        klog_printf("-- panic ring (%d bytes) --\n", n);
        klog_write_n(buf, (size_t)n);
        klog_printf("\n-- end ring --\n");
    }
    /* Gap A8: the crash record goes to the disk (the evidence outlives
     * the boot; the pickup reports it next boot). */
    {
        extern int wubu_crash_dump(const char *, uint64_t, uint64_t,
                                   uint32_t);
        if (wubu_crash_dump("kernel panic", 0, 0, 0) == 0)
            klog_printf("-- crash record written to disk --\n");
    }
}

/* Public wrapper for the watchdog / NMI paths (the panic ring dump). */
void interrupt_panic_dump(void) { panic_dump_ring(); }

void handle_page_fault(InterruptFrame *frame) {
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    /* Real virtual memory: a fault inside a registered demand-zero region
     * allocates a fresh page, maps it, and RETURNS -- the iretq retries
     * the faulting instruction. Everything else is a genuine kernel bug. */
    extern int  wubu_vmm_is_demand(uint64_t);
    extern int  wubu_vmm_demand_fill(uint64_t);
    if (wubu_vmm_is_demand(cr2)) {
        if (wubu_vmm_demand_fill(cr2) == 0)
            return;                     /* retry the faulting instruction */
        /* OOM in the demand path: fall through to the panic dump */
    }
    /* Gap B4: copy-on-write -- a write fault on a shared (RO) page
     * makes a private copy + remaps writable, then retries. */
    extern int wubu_vmm_cow_fault(uint64_t);
    if (wubu_vmm_cow_fault(cr2) == 1)
        return;
    /* Gap B3: swap -- a fault on a swapped-out VA reads it back. */
    extern int wubu_swap_va_slot(uint64_t);
    extern int wubu_swap_in(uint64_t, uint32_t);
    int slot = wubu_swap_va_slot(cr2);
    if (slot >= 0 && wubu_swap_in(cr2, (uint32_t)slot) == 0)
        return;
    /* In a real kernel: demand paging, COW, etc. */
    extern int klog_printf(const char *fmt, ...);
    uint64_t *sp = frame ? (uint64_t *)(uintptr_t)frame->rsp : NULL;
    klog_printf("WuBuOS PF: cr2=%x rip=%x rcx=%x rsi=%x rdx=%x rsp=%x caller=[%x,%x,%x]\n",
                (unsigned)(cr2 & 0xFFFFFFFFu),
                (unsigned)(frame ? (uint64_t)frame->rip : 0),
                (unsigned)(frame ? (uint64_t)frame->rcx : 0),
                (unsigned)(frame ? (uint64_t)frame->rsi : 0),
                (unsigned)(frame ? (uint64_t)frame->rdx : 0),
                (unsigned)(frame ? (uint64_t)frame->rsp : 0),
                (unsigned)(sp ? sp[0] : 0),
                (unsigned)(sp ? sp[1] : 0),
                (unsigned)(sp ? sp[2] : 0));
    panic_dump_ring();
    while (1) { __asm__ volatile ("hlt"); }
}

/* General protection fault handler (uses IST1) */
void handle_gpf(InterruptFrame *frame) {
    extern int klog_printf(const char *fmt, ...);
    /* The iretq popped a bad frame -> #GP with err=0 at the iretq itself.
     * The frame's rip/cs/rflags are the iretq's OWN context; the iretq's
     * attempted pops are at faulting_rsp = (frame + 136) for ring-0
     * faults (the CPU pushed [rip,cs,rflags] right where the iretq was). */
    uint64_t *stk = frame ? (uint64_t *)((uintptr_t)frame + 136) : NULL;
    klog_printf("WuBuOS GP: rip=%x cs=%x rflags=%x err=%x iretframe=[%x,%x,%x,%x,%x,%x]\n",
                (unsigned)(frame ? (uint64_t)frame->rip : 0),
                (unsigned)(frame ? frame->cs : 0),
                (unsigned)(frame ? (uint64_t)frame->rflags : 0),
                (unsigned)(frame ? (uint64_t)frame->error_code : 0),
                (unsigned)(stk ? stk[-2] : 0), (unsigned)(stk ? stk[-1] : 0),
                (unsigned)(stk ? stk[0] : 0),  (unsigned)(stk ? stk[1] : 0),
                (unsigned)(stk ? stk[2] : 0),  (unsigned)(stk ? stk[3] : 0));
    panic_dump_ring();
    while (1) { __asm__ volatile ("hlt"); }
}

/* ------------------------------------------------------------------
 * Enhanced IDT Initialization with IST and SYSCALL
 * ------------------------------------------------------------------ */

int interrupt_init_full(void) {
    if (interrupt_init() != 0) return -1;

#ifdef MYSEED_METAL
    /* Initialize APIC subsystem */
    if (apic_init() != 0) return -1;

    /* Initialize SYSCALL/SYSRET fast path */
    if (syscall_init() != 0) return -1;

    /* Initialize LAPIC timer at 100Hz on vector 0xF0 (240) */
    if (lapic_timer_init(100, 240) != 0) return -1;

    /* Set up exception handlers with IST */
    interrupt_set_gate(8,  (uint64_t)handle_double_fault, 0x08, IDT_GATE_INT, IST_EXCEPTION);  /* #DF */
    interrupt_set_gate(2,  (uint64_t)handle_nmi,            0x08, IDT_GATE_INT, IST_NMI);       /* NMI */
    interrupt_set_gate(14, (uint64_t)handle_page_fault,     0x08, IDT_GATE_INT, IST_EXCEPTION); /* #PF */
    interrupt_set_gate(13, (uint64_t)handle_gpf,            0x08, IDT_GATE_INT, IST_EXCEPTION); /* #GP */

    /* Reload IDT with updated gates */
    lidt(&idt_ptr);
#endif

    return 0;
}
