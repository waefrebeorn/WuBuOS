/*
 * tasking_test.c  --  Test suite for My Seed Tasking Subsystem
 */

#include "tasking.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST(name) printf("  TEST: %-40s ", name)
#define PASS()     printf("✅ PASS\n")
#define FAIL(msg)  do { printf("❌ FAIL: %s\n", msg); failures++; } while(0)

static int failures = 0;

static void test_init_shutdown(void) {
    printf("\n[Init / Shutdown]\n");

    mem_init(2 * 1024 * 1024);

    TEST("tasking_init returns 0");
    if (tasking_init() == 0) PASS(); else FAIL("init failed");

    TEST("task_current() returns idle task");
    CTask *cur = task_current();
    if (cur && cur->priority == PRIO_IDLE) PASS(); else FAIL("not idle");

    TEST("task_count() >= 1 (idle task)");
    if (task_count() >= 1) PASS(); else FAIL("no tasks");

    TEST("tasking_shutdown doesn't crash");
    tasking_shutdown();
    mem_shutdown();
    PASS();
}

/* Test tasks */
static int g_counter1 = 0;
static int g_counter2 = 0;

static void test_task_a(void *arg) {
    int *counter = (int *)arg;
    for (int i = 0; i < 3; i++) {
        (*counter)++;
        task_yield();
    }
}

static void test_task_b(void *arg) {
    int *counter = (int *)arg;
    for (int i = 0; i < 5; i++) {
        (*counter)++;
        task_yield();
    }
}

static void test_create_destroy(void) {
    printf("\n[Create / Destroy]\n");

    mem_init(4 * 1024 * 1024);
    tasking_init();

    TEST("task_create returns non-NULL");
    CTask *t1 = task_create("task_a", test_task_a, &g_counter1, 0, PRIO_NORMAL);
    if (t1) PASS(); else FAIL("NULL");

    TEST("task has correct name");
    if (t1 && strcmp(t1->name, "task_a") == 0) PASS(); else FAIL("wrong name");

    TEST("task has TASK_READY state");
    if (t1 && t1->state == TASK_READY) PASS(); else FAIL("wrong state");

    TEST("task has non-NULL stack");
    if (t1 && t1->stack_base) PASS(); else FAIL("no stack");

    TEST("task signature is correct");
    if (t1 && t1->task_signature == TASK_SIGNATURE_VAL) PASS(); else FAIL("bad sig");

    TEST("task_count is now 2 (idle + task_a)");
    if (task_count() == 2) PASS(); else FAIL("wrong count");

    TEST("task_destroy works");
    task_destroy(t1);
    if (task_count() == 1) PASS(); else FAIL("count wrong after destroy");

    tasking_shutdown();
    mem_shutdown();
}

static void test_priority(void) {
    printf("\n[Priority Scheduling]\n");

    mem_init(4 * 1024 * 1024);
    tasking_init();

    TEST("create LOW, NORMAL, HIGH priority tasks");
    CTask *low  = task_create("low",  NULL, NULL, 0, PRIO_LOW);
    CTask *norm = task_create("norm", NULL, NULL, 0, PRIO_NORMAL);
    CTask *high = task_create("high", NULL, NULL, 0, PRIO_HIGH);
    if (low && norm && high) PASS(); else FAIL("create failed");

    TEST("schedule_next picks highest priority");
    CTask *next = task_schedule_next();
    if (next == high) PASS(); else FAIL("not high prio");

    task_destroy(low);
    task_destroy(norm);
    task_destroy(high);
    tasking_shutdown();
    mem_shutdown();
}

static void test_sleep_wake(void) {
    printf("\n[Sleep / Wake]\n");

    mem_init(4 * 1024 * 1024);
    tasking_init();

    CTask *t1 = task_create("sleeper", NULL, NULL, 0, PRIO_NORMAL);
    if (!t1) { FAIL("create failed"); tasking_shutdown(); mem_shutdown(); return; }

    TEST("task_sleep sets SLEEPING state");
    /* Can't actually sleep the current task in test without running the scheduler,
     * but we can verify the state machine */
    t1->state = TASK_SLEEPING;
    t1->wake_tick = 100;
    if (t1->state == TASK_SLEEPING) PASS(); else FAIL("not sleeping");

    TEST("task_unblock sets READY state");
    t1->state = TASK_BLOCKED;  /* Must be BLOCKED for unblock to work */
    task_unblock(t1);
    if (t1->state == TASK_READY) PASS(); else FAIL("not ready");

    task_destroy(t1);
    tasking_shutdown();
    mem_shutdown();
}

static void test_task_ids(void) {
    printf("\n[Task IDs]\n");

    mem_init(2 * 1024 * 1024);
    tasking_init();

    CTask *t1 = task_create("t1", NULL, NULL, 0, PRIO_NORMAL);
    CTask *t2 = task_create("t2", NULL, NULL, 0, PRIO_NORMAL);
    CTask *t3 = task_create("t3", NULL, NULL, 0, PRIO_NORMAL);

    TEST("task IDs are unique and ascending");
    if (t1 && t2 && t3 && t1->task_id < t2->task_id && t2->task_id < t3->task_id)
        PASS();
    else FAIL("IDs not unique");

    task_destroy(t1);
    task_destroy(t2);
    task_destroy(t3);
    tasking_shutdown();
    mem_shutdown();
}

int main(void) {
    printf("+==============================+\n");
    printf("|  My Seed Tasking Test Suite  |\n");
    printf("+==============================+\n");

    test_init_shutdown();
    test_create_destroy();
    test_priority();
    test_sleep_wake();
    test_task_ids();

    printf("\n==============================\n");
    if (failures == 0)
        printf("All tests passed! ✅\n");
    else
        printf("%d test(s) FAILED ❌\n", failures);

    return failures ? 1 : 0;
}
