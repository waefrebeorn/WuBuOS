/*
 * tasking.h — My Seed Kernel Task/Process Management
 *
 * Ring-0 cooperative multitasking (like ZealOS).
 * Each task has its own stack, context, and heap.
 * Inspired by ZealOS/src/Kernel/Sched.ZC
 */

#ifndef MYSEED_TASKING_H
#define MYSEED_TASKING_H

#include <stdint.h>
#include <stddef.h>

/* ── Task States ────────────────────────────────────────────────── */

typedef enum {
    TASK_UNUSED    = 0,
    TASK_RUNNING   = 1,
    TASK_READY     = 2,
    TASK_BLOCKED   = 3,  /* Waiting for I/O, sleep, etc. */
    TASK_SLEEPING  = 4,
    TASK_DYING     = 5,
} TaskState;

/* ── Task Priority ──────────────────────────────────────────────── */

typedef enum {
    PRIO_IDLE    = 0,
    PRIO_LOW     = 1,
    PRIO_NORMAL  = 2,
    PRIO_HIGH    = 3,
    PRIO_REALTIME= 4,
} TaskPriority;

#define TASK_SIGNATURE_VAL  0x7A5C3E01

/* ── Task Context (saved registers for switch) ──────────────────── */

typedef struct TaskContext {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t rip;
    uint64_t rsp;
    uint64_t rflags;
} TaskContext;

/* ── Task Control Block ─────────────────────────────────────────── */

typedef struct CTask CTask;
struct CTask {
    uint32_t       task_signature;   /* TASK_SIGNATURE_VAL           */
    TaskState      state;
    TaskPriority   priority;
    int            task_id;          /* Unique ID                    */
    char           name[32];        /* Human-readable name           */

    /* Scheduling */
    TaskContext    context;          /* Saved register state          */
    uint64_t      *stack_base;       /* Bottom of allocated stack     */
    size_t         stack_size;       /* Stack size in bytes           */

    /* Timing */
    uint64_t       wake_tick;        /* Tick to wake (if sleeping)    */
    uint64_t       total_ticks;      /* Total CPU ticks consumed      */

    /* Linked list */
    CTask         *next;
    CTask         *prev;

    /* User data (generic pointer for app state) */
    void          *user_data;

    /* Entry function */
    void         (*entry)(void *arg);
    void          *entry_arg;
};

/* ── Global State ───────────────────────────────────────────────── */

/* Get the currently running task. */
CTask *task_current(void);

/* Get the task list head. */
CTask *task_list_head(void);

/* Get total task count. */
int task_count(void);

/* Get global tick counter. */
uint64_t task_tick_count(void);

/* ── Task Lifecycle ─────────────────────────────────────────────── */

/*
 * Create a new task.
 * @param name     Human-readable name
 * @param entry    Function to run
 * @param arg      Argument passed to entry
 * @param stack_sz Stack size in bytes (0 = default 64KB)
 * @param prio     Priority level
 * @return New task handle, or NULL on failure
 */
CTask *task_create(const char *name, void (*entry)(void *arg), void *arg,
                    size_t stack_sz, TaskPriority prio);

/* Destroy a task (marks as dying, cleaned up by scheduler). */
void task_destroy(CTask *task);

/* Yield CPU to next ready task. */
void task_yield(void);

/* Block current task (sets state to BLOCKED). */
void task_block(void);

/* Unblock a blocked task (sets state to READY). */
void task_unblock(CTask *task);

/* Sleep current task for `ticks` timer ticks. */
void task_sleep(uint64_t ticks);

/* ── Scheduler ──────────────────────────────────────────────────── */

/* Initialize the tasking subsystem. Creates idle task. */
int  tasking_init(void);

/* Shutdown — destroy all tasks. */
void tasking_shutdown(void);

/*
 * Pick next task to run (round-robin with priority).
 * Called by timer interrupt or explicit yield.
 * Returns the task that should run next.
 */
CTask *task_schedule_next(void);

/* Idle task (runs when nothing else is ready). */
void task_idle(void *arg);

/* ── Context Switch (arch-specific) ─────────────────────────────── */

/*
 * Save current context and switch to target task.
 * This is the actual register save/restore.
 * Implemented in tasking_switch.S (assembly) or
 * simulated with setjmp/longjmp for hosted testing.
 */
void task_switch_to(CTask *target);

#endif /* MYSEED_TASKING_H */
