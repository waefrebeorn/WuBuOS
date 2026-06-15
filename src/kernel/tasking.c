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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>

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
static int      g_initialized = 0;
static int      g_preemptive = 0;   /* 1 = timer-driven preemption enabled */

/* Forward declaration for assembly context switch */
#ifdef MYSEED_METAL
extern void task_switch_asm(TaskContext *old_ctx, TaskContext *new_ctx);
#endif

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
CTask *task_list_head(void)  { return g_head; }
int    task_count(void) {
    if (!g_head) return 0;
    int n = 0;
    CTask *t = g_head;
    do { n++; t = t->next; } while (t != g_head);
    return n;
}
uint64_t task_tick_count(void) { return g_tick; }

/* -- Task Lifecycle ----------------------------------------------- */

CTask *task_create(const char *name, void (*entry)(void *arg), void *arg,
                    size_t stack_sz, TaskPriority prio) {
    if (!stack_sz) stack_sz = 65536;

    CTask *t = (CTask *)mem_alloc(sizeof(CTask));
    if (!t) return NULL;
    memset(t, 0, sizeof(CTask));

    t->stack_base = (uint64_t *)mem_alloc(stack_sz);
    if (!t->stack_base) { mem_free(t); return NULL; }

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

/* -- Timer Tick Handler (Preemptive Scheduling) --------------------- */

/* Called from interrupt context (IRQ0/PIT)  --  must be fast, no locks */
void task_timer_tick(void) {
    g_tick++;

    if (!g_initialized || !g_current) return;

    g_current->total_ticks++;

    /* Wake sleeping tasks whose time has come */
    CTask *t = g_head;
    if (t) {
        do {
            if (t->state == TASK_SLEEPING && g_tick >= t->wake_tick) {
                t->state = TASK_READY;
            }
            t = t->next;
        } while (t != g_head);
    }

    /* Preemptive scheduling: if enabled, yield current task */
    if (g_preemptive) {
        CTask *next = task_schedule_next();
        if (next && next != g_current) {
            /* Switch context  --  save old, restore new */
            g_current->state = TASK_READY;
            next->state = TASK_RUNNING;
            CTask *old = g_current;
            g_current = next;

#ifdef MYSEED_METAL
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
    while (1) {
        task_yield();
    }
}

/* -- Yield / Block / Unblock / Sleep ------------------------------ */

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
    /* We return here when another task yields back to us */
}

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
