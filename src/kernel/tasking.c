/*
 * tasking.c  --  My Seed Kernel Task Management (hosted test impl)
 *
 * Uses setjmp/longjmp for context switching in hosted (Linux) mode.
 * Real kernel will use assembly task_switch_to in tasking_switch.S.
 *
 * Design from ZealOS/src/Kernel/Sched.ZC  --  cooperative, ring-0, round-robin.
 */

#include "tasking.h"
#include "memory.h"
#include "interrupt.h"
#include "libc.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#ifndef WUBU_BAREMETAL
#include <setjmp.h>
#endif

/* -- Internal: jmp_buf wrapper for context ------------------------ */

typedef struct {
    jmp_buf jb;
    int     primed;   /* 1 if setjmp was called, 0 if not */
} TaskJmp;

/* -- Global State -------------------------------------------------- */

static CTask   *g_current   = NULL;
static CTask   *g_head      = NULL;  /* Doubly-linked circular list */
static int      g_next_id   = 1;
static uint64_t g_tick      = 0;
static uint64_t g_next_wake = ~0ull;   /* D2: earliest pending wake   */
static int      g_initialized = 0;
static int      g_preemptive = 0;   /* 1 = timer-driven preemption enabled */

/* Forward declaration for assembly context switch */
#if WUBU_BAREMETAL
extern void task_switch_asm(TaskContext *old_ctx, TaskContext *new_ctx);
/* First-run trampoline (see task_create): the assembly switch lands here
 * on a brand-new task's stack; it calls the task's entry, then retires it. */
static void task_trampoline(void);
#endif

/* Watchdog (gap A4): a task that never yields for this many ticks
 * (50s at 100 Hz) is declared stuck. */
#define WATCHDOG_STALL_LIMIT 5000ull

/* -- Helpers ------------------------------------------------------ */

static void task_insert(CTask *t) {
    if (!g_head) {
        g_head = t;
        t->next = t;
        t->prev = t;
    } else {
        t->next = g_head;
        t->prev = g_head->prev;
        g_head->prev->next = t;
        g_head->prev = t;
    }
}

static void task_remove(CTask *t) {
    if (t->next == t) {
        g_head = NULL;
    } else {
        t->prev->next = t->next;
        t->next->prev = t->prev;
        if (g_head == t) g_head = t->next;
    }
    t->next = NULL;
    t->prev = NULL;
}

/* -- Accessors ---------------------------------------------------- */

CTask *task_current(void)    { return g_current; }
const char *task_name(const CTask *t) {
    return (t && t->name[0]) ? t->name : "?";
}
CTask *task_list_head(void)  { return g_head; }
int    task_count(void) {
    if (!g_head) return 0;
    int n = 0;
    CTask *t = g_head;
    do { n++; t = t->next; } while (t != g_head);
    return n;
}
uint64_t task_tick_count(void) { return g_tick; }

/* Gap D3: priority accessors for the sync module's inheritance. */
int task_prio_get(const CTask *t) { return t ? (int)t->priority : 0; }
void task_prio_set(CTask *t, int prio) { if (t) t->priority = (TaskPriority)prio; }

/* -- Task Lifecycle ----------------------------------------------- */

CTask *task_create(const char *name, void (*entry)(void *arg), void *arg,
                    size_t stack_sz, TaskPriority prio) {
    if (!stack_sz) stack_sz = 65536;

    CTask *t = (CTask *)mem_alloc(sizeof(CTask));
    if (!t) return NULL;
    memset(t, 0, sizeof(CTask));

    t->stack_base = (uint64_t *)mem_alloc(stack_sz);
    if (!t->stack_base) { mem_free(t); return NULL; }
    /* Stack canary (the overflow tripwire): 8 bytes at the LOW end of
     * the stack -- a stack that grows down past its base clobbers the
     * canary BEFORE anything else. Checked at every switch; a trip is
     * the corruption's smoking gun (the panic ring holds the last
     * messages). */
    *(volatile uint64_t *)t->stack_base = 0xCAFEBABECAFEBABEULL;

    t->task_signature = TASK_SIGNATURE_VAL;
    t->state    = TASK_READY;
    t->priority = prio;
    t->task_id  = g_next_id++;
    strncpy(t->name, name ? name : "anon", sizeof(t->name) - 1);
    t->stack_size = stack_sz;
    t->entry     = entry;
    t->entry_arg = arg;
    t->wake_tick = 0;
    t->total_ticks = 0;

    /* Allocate jmp_buf in user_data for hosted mode */
    t->user_data = mem_alloc(sizeof(TaskJmp));
    if (!t->user_data) { mem_free(t->stack_base); mem_free(t); return NULL; }
    memset(t->user_data, 0, sizeof(TaskJmp));

#if WUBU_BAREMETAL
    /* Prime the first-run context: the first switch jumps to the trampoline
     * on the new stack. rflags IF (0x200) set so the PIT can preempt the
     * task immediately. */
    t->context.rsp    = (uint64_t)((uint8_t *)t->stack_base + stack_sz);
    t->context.rip    = (uint64_t)(uintptr_t)&task_trampoline;
    t->context.rflags = 0x200;
    /* Prime the FPU/SSE area (gap C8): a zeroed FXSAVE area is
     * fxrstor-legal, but the standard defaults are cleaner -- FCW 0x37F
     * (x87: round-nearest + all exceptions masked) + MXCSR 0x1F80 (all
     * exceptions masked). Without this a first-run fxrstor could #GP on
     * an unvalidated control word. */
    memset(t->context.fxsave, 0, sizeof(t->context.fxsave));
    *(uint16_t *)(t->context.fxsave + 0)  = 0x037F;  /* FCW  */
    *(uint32_t *)(t->context.fxsave + 24) = 0x1F80;  /* MXCSR */
#endif

    task_insert(t);
    return t;
}

void task_destroy(CTask *task) {
    if (!task) return;
    task->state = TASK_DYING;
    task_remove(task);
    mem_free(task->user_data);
    mem_free(task->stack_base);
    mem_free(task);
}

/* -- Scheduler ---------------------------------------------------- */

static int task_stack_ok(const CTask *t)
{
    if (!t || !t->stack_base) return 1;   /* not our stack: don't trip */
    return *(volatile uint64_t *)t->stack_base == 0xCAFEBABECAFEBABEULL;
}

CTask *task_schedule_next(void) {
    if (!g_head) return NULL;

    /* Round-robin with priority: scan from current for highest prio ready task */
    CTask *best = NULL;
    CTask *t = g_current ? g_current->next : g_head;
    CTask *start = t;

    do {
        if (t->state == TASK_READY || t->state == TASK_RUNNING) {
            if (!best || t->priority > best->priority)
                best = t;
        }
        /* Check sleeping tasks */
        if (t->state == TASK_SLEEPING && g_tick >= t->wake_tick) {
            t->state = TASK_READY;
            if (!best || t->priority > best->priority)
                best = t;
        }
        t = t->next;
    } while (t != start);

    if (!best) {
        /* Only idle or blocked tasks  --  find idle */
        t = g_head;
        do {
            if (t->state == TASK_READY && t->priority == PRIO_IDLE)
                return t;
            t = t->next;
        } while (t != g_head);
    }

    return best;
}

/* -- Reaper (gap A5) ------------------------------------------------ */

/* Free a DYING task's resources + unlink it from the ring. MUST run in
 * TASK context (the heap is not ISR-safe): the main loop calls this
 * every iteration. The current task is never reaped. */
void task_reap(void) {
    if (!g_head || !g_initialized) return;
    extern void mem_free(void *);
    CTask *t = g_head;
    do {
        CTask *next = t->next;
        if (t->state == TASK_DYING && t != g_current) {
            /* unlink */
            CTask *prev = t;
            while (prev->next != t) prev = prev->next;
            prev->next = next;
            if (g_head == t) g_head = (next == t) ? NULL : next;
            mem_free(t->stack_base);
            mem_free(t->user_data);
            mem_free(t);
            t = next;
            if (!g_head) break;
            continue;
        }
        t = next;
    } while (t != g_head && g_head);
}

/* -- Timer Tick Handler (Preemptive Scheduling) --------------------- */

/* Called from interrupt context (IRQ0/PIT)  --  must be fast, no locks */
void task_timer_tick(void) {
    g_tick++;

    if (!g_initialized || !g_current) return;

    g_current->total_ticks++;

    /* Watchdog (gap A4): a task that never yields past the limit is
     * stuck -- name it + dump the panic ring. (The limit is generous:
     * the busy tasks are preemptable, so a legit CPU-bound stretch is
     * fine; a genuinely stuck loop gets caught.) */
    if (g_current->stall_ticks++ > WATCHDOG_STALL_LIMIT) {
        extern int klog_printf(const char *, ...);
        extern void interrupt_panic_dump(void);
        klog_printf("WATCHDOG: task '%s' stuck (%u ticks, state=%d)\n",
                    g_current->name[0] ? g_current->name : "?",
                    (unsigned)g_current->stall_ticks,
                    (int)g_current->state);
        interrupt_panic_dump();
        for (;;) { __asm__ __volatile__("cli"); __asm__ __volatile__("hlt"); }
    }

    /* Wake sleeping tasks whose time has come. Gap D2: the wakeup
     * scan tracks the EARLIEST pending wake so a tick with nothing to
     * wake (the common case) skips the O(n) walk entirely. */
    if (g_tick >= g_next_wake) {
        g_next_wake = ~0ull;
        CTask *t = g_head;
        if (t) {
            do {
                if (t->state == TASK_SLEEPING) {
                    if (g_tick >= t->wake_tick) {
                        t->state = TASK_READY;
                    } else if (t->wake_tick < g_next_wake) {
                        g_next_wake = t->wake_tick;
                    }
                }
                t = t->next;
            } while (t != g_head);
        }
    }

    /* Preemptive scheduling: if enabled, yield current task */
    if (g_preemptive) {
        CTask *next = task_schedule_next();
        if (next && next != g_current) {
            /* Stack canary tripwire: a clobbered base = a stack grew
             * past its allocation. The trip is the corruption's smoking
             * gun; the panic ring (last 4KB of klog) is dumped via the
             * serial -- the corruptor's last messages are the evidence. */
            if (!task_stack_ok(g_current) || !task_stack_ok(next)) {
                extern int klog_printf(const char *, ...);
                klog_printf("STACK CANARY TRIPPED: cur=%s next=%s\n",
                            g_current ? g_current->name : "?",
                            next ? next->name : "?");
                for (;;) { __asm__ __volatile__("cli"); __asm__ __volatile__("hlt"); }
            }
            /* Switch context  --  save old, restore new */
            g_current->state = TASK_READY;
            next->state = TASK_RUNNING;
            CTask *old = g_current;
            g_current = next;

#if WUBU_BAREMETAL
            /* Real metal: assembly context switch */
            task_switch_asm(&old->context, &next->context);
#else
            /* Hosted: use setjmp/longjmp via task_yield logic */
            TaskJmp *old_jmp = (TaskJmp *)old->user_data;
            TaskJmp *new_jmp = (TaskJmp *)next->user_data;

            if (setjmp(old_jmp->jb) == 0) {
                old_jmp->primed = 1;
                if (new_jmp->primed) {
                    longjmp(new_jmp->jb, 1);
                } else {
                    new_jmp->primed = 1;
                    if (next->entry) next->entry(next->entry_arg);
                    task_destroy(next);
                }
            }
#endif
        }
    }
}

/* Enable/disable preemptive scheduling */
void task_preempt_enable(void)  { g_preemptive = 1; }
void task_preempt_disable(void) { g_preemptive = 0; }
int  task_preempt_enabled(void) { return g_preemptive; }

/* -- Idle Task ------------------------------------------------------ */

void task_idle(void *arg) {
    (void)arg;
    /* Gap D1: the idle task HALTs instead of busy-yielding. This task's
     * primed rflags has IF set (0x200), so the PIT's vector-32 interrupt
     * wakes each hlt; there is no sti to shadow. */
    for (;;) __asm__ __volatile__("hlt");
}

/* -- Yield / Block / Unblock / Sleep ------------------------------ */

#if WUBU_BAREMETAL
/* First-run trampoline: the assembly context switch restored this task's
 * primed context and jumped here (g_current == this task). Call the entry,
 * then retire the task (state = DYING so the scheduler skips it; the stack
 * stays allocated -- the kernel boots once). */
static void task_trampoline(void)
{
    CTask *self = g_current;
    if (self && self->entry) {
        uint64_t sp;
        __asm__ volatile("movq %%rsp, %0" : "=r"(sp));
        sp &= ~15ULL;                       /* SysV 16-byte stack alignment */
        __asm__ volatile("movq %0, %%rsp" :: "r"(sp));
        self->entry(self->entry_arg);
    }
    if (self) self->state = TASK_DYING;
    for (;;) task_yield();
}

void task_yield(void) {
    /* Bare-metal: real round-robin context switch via the assembly switch. */
    if (!g_current || !g_initialized) return;

    /* Watchdog reset (gap A4): a yield proves the task is alive. */
    g_current->stall_ticks = 0;

    CTask *old = g_current;
    CTask *next = task_schedule_next();
    if (!next || next == old) return;

    old->state = TASK_READY;
    next->state = TASK_RUNNING;
    g_current = next;
    /* The switch must be atomic: a PIT IRQ landing between the RSP restore
     * and the jmp would push the iret frame on the wrong stack and re-enter
     * the scheduler mid-switch (corruption). The ISR path is already safe
     * (hardware clears IF on interrupt entry; the iret restores it). */
    __asm__ volatile("cli");
    task_switch_asm(&old->context, &next->context);
    __asm__ volatile("sti");
    /* Resumed here when the scheduler returns to this task. */
}
#else
void task_yield(void) {
    if (!g_current || !g_initialized) return;

    CTask *old = g_current;
    CTask *next = task_schedule_next();
    if (!next || next == old) return;

    old->state = TASK_READY;
    next->state = TASK_RUNNING;
    g_current = next;

    /* Hosted context switch via setjmp/longjmp */
    TaskJmp *old_jmp = (TaskJmp *)old->user_data;
    TaskJmp *new_jmp = (TaskJmp *)next->user_data;

    if (setjmp(old_jmp->jb) == 0) {
        old_jmp->primed = 1;
        if (new_jmp->primed) {
            longjmp(new_jmp->jb, 1);
        } else {
            /* First time running this task  --  call entry */
            new_jmp->primed = 1;
            if (next->entry) next->entry(next->entry_arg);
            /* Task returned  --  destroy it */
            task_destroy(next);
        }
    }
}
#endif

void task_block(void) {
    if (g_current) g_current->state = TASK_BLOCKED;
    task_yield();
}

void task_unblock(CTask *task) {
    if (task && task->state == TASK_BLOCKED)
        task->state = TASK_READY;
}

void task_sleep(uint64_t ticks) {
    if (g_current) {
        g_current->state = TASK_SLEEPING;
        g_current->wake_tick = g_tick + ticks;
        if (g_current->wake_tick < g_next_wake)  /* D2 */
            g_next_wake = g_current->wake_tick;
        task_yield();
    }
}

/* -- Init / Shutdown ---------------------------------------------- */

int tasking_init(void) {
    if (g_initialized) return 0;

    /* Create idle task */
    CTask *idle = task_create("idle", task_idle, NULL, 16384, PRIO_IDLE);
    if (!idle) return -1;

    g_current = idle;
    g_current->state = TASK_RUNNING;
    g_initialized = 1;
    return 0;
}

void tasking_shutdown(void) {
    while (g_head) {
        CTask *t = g_head;
        task_remove(t);
        mem_free(t->user_data);
        mem_free(t->stack_base);
        mem_free(t);
    }
    g_current = NULL;
    g_initialized = 0;
}

void task_switch_to(CTask *target) {
    /* In hosted mode, task_yield handles the switch */
    (void)target;
    task_yield();
}

/* ===================================================================
 * ZealOS Parity Functions
 * =================================================================== */

/* Kill a task by PID (sends signal, default SIGKILL) */
void task_kill(int pid, int sig) {
    (void)sig;
    if (pid <= 0) return;
    CTask *t = g_head;
    if (!t) return;
    do {
        if (t->task_id == pid) {
            task_destroy(t);
            return;
        }
        t = t->next;
    } while (t != g_head);
}

/* Exit current task with code */
void task_exit(int code) {
    (void)code;
    if (g_current && g_current != g_head) { /* Don't kill idle */
        CTask *old = g_current;
        g_current = g_head; /* Switch to idle */
        task_destroy(old);
    }
    task_yield();
}

/* Wake a sleeping task */
void task_wake(CTask *task) {
    if (task && (task->state == TASK_SLEEPING || task->state == TASK_BLOCKED)) {
        task->state = TASK_READY;
        task->wake_tick = 0;
    }
}

/* Suspend a task (make it not runnable) */
void task_suspend(CTask *task) {
    if (task && task->state == TASK_READY) {
        task->state = TASK_BLOCKED;
    }
}

/* Check if task is suspended */
int task_is_suspended(CTask *task) {
    return task && (task->state == TASK_BLOCKED || task->state == TASK_SLEEPING);
}

/* Make a task runnable (ZealOS TaskRun) */
void task_run(CTask *task) {
    if (task && task->state == TASK_UNUSED) {
        task->state = TASK_READY;
    }
}

/* Set task priority */
void task_set_priority(CTask *task, TaskPriority prio) {
    if (task) task->priority = prio;
}

/* Get task priority */
TaskPriority task_get_priority(CTask *task) {
    return task ? task->priority : PRIO_IDLE;
}

/* Focus next task in queue */
CTask *task_focus_next(void) {
    if (!g_current || !g_head) return NULL;
    CTask *t = g_current->next;
    while (t != g_current) {
        if (t->state == TASK_READY || t->state == TASK_RUNNING) return t;
        t = t->next;
    }
    return g_current;
}

/* Focus previous task in queue */
CTask *task_focus_prev(void) {
    if (!g_current || !g_head) return NULL;
    CTask *t = g_current->prev;
    while (t != g_current) {
        if (t->state == TASK_READY || t->state == TASK_RUNNING) return t;
        t = t->prev;
    }
    return g_current;
}

/* Validate task structure (check signature) */
int task_validate(CTask *task) {
    return task && task->task_signature == TASK_SIGNATURE_VAL;
}

/* Kill all tasks except idle */
void task_kill_all(void) {
    if (!g_head) return;
    CTask *t = g_head;
    do {
        CTask *next = t->next;
        if (t->priority != PRIO_IDLE) {
            task_destroy(t);
        }
        t = next;
    } while (t != g_head);
}

/* Context switch save (placeholder for assembly) */
void task_ctx_save(CTask *task) {
    (void)task;
    /* In real kernel: save registers to task->context */
}

/* Context switch restore (placeholder for assembly) */
void task_ctx_restore(CTask *task) {
    (void)task;
    /* In real kernel: restore registers from task->context */
}

/* Update derived values (placeholder for ZealOS compatibility) */
void task_derived_vals_update(CTask *task) {
    (void)task;
    /* In ZealOS this updates derived register values */
}

/* Wait for task death */
void task_death_wait(CTask *task) {
    if (!task) return;
    while (task->state != TASK_DYING && task->state != TASK_UNUSED) {
        task_yield();
    }
}

/* Walk task queue and call callback for each task */
int task_queue_walk(void (*callback)(CTask *task, void *arg), void *arg) {
    if (!g_head || !callback) return 0;
    int count = 0;
    CTask *t = g_head;
    do {
        callback(t, arg);
        count++;
        t = t->next;
    } while (t != g_head);
    return count;
}

/* Allocate task struct (internal helper for ZealOS parity) */
CTask *task_struct_alloc(void) {
    return (CTask *)mem_alloc(sizeof(CTask));
}

/* Get parent task (ZealOS compatibility - returns idle as parent for now) */
CTask *task_parent(CTask *task) {
    (void)task;
    return g_head; /* Return idle task as parent */
}
