/*
 * test_agi_kernel_stub.c -- Minimal kernel-API shims so wubu_agi_kernel.c
 * links + runs in the HOSTED unit test without the real ring-0 scheduler.
 *
 * The AGI kernel's run loop (task_create/task_yield) is NOT exercised here --
 * the test drives init/cycle/tick directly. These stubs just satisfy the
 * linker and the (unused-in-test) run path. On bare metal, real tasking/klog
 * are linked instead.
 */
#include "tasking.h"
#include "klog.h"
#include <stdio.h>

CTask *task_create(const char *name, void (*entry)(void *arg), void *arg,
                   size_t stack_sz, TaskPriority prio) {
    (void)name; (void)entry; (void)arg; (void)stack_sz; (void)prio;
    return NULL;   /* test never calls wubu_agi_kernel_run() */
}
void task_yield(void) {}
void task_preempt_enable(void) {}
void task_timer_tick(void) {}

int klog_printf(const char *fmt, ...) {
    (void)fmt;
    return 0;   /* silence kernel logging in the hosted test */
}
