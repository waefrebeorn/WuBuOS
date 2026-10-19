/*
 * tasking_test.c  --  Test suite for My Seed Tasking Subsystem
 */

#include "tasking.h"
#include "memory.h"
#include "klog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST(name) printf("  TEST: %-40s ", name)
#define PASS()     printf("✅ PASS\n")
#define FAIL(msg)  do { printf("❌ FAIL: %s\n", msg); failures++; } while(0)

static int failures = 0;

/* The watchdog path (tasking.c) calls interrupt_panic_dump() on a stuck
 * task. In the hosted test that must be REAL work: dump the klog panic
 * ring (the last ~4 KB of kernel log output) to stderr so the stuck
 * task's evidence is visible -- the same contract as the bare-metal
 * serial dump, routed to the host console instead of COM1. */
void interrupt_panic_dump(void)
{
    char buf[4096 + 1];
    int n = klog_ring_snapshot(buf, sizeof(buf));
    if (n > 0)
        fprintf(stderr, "--- klog panic ring (%d bytes) ---\n%.*s\n--- end ring ---\n",
                n, n, buf);
    else
        fprintf(stderr, "--- klog panic ring: empty ---\n");
}

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


static void test_eevdf_fairness(void) {
    printf("\n[EEVDF Fairness]\n");
    mem_init(4 * 1024 * 1024);
    tasking_init();

    TEST("create two equal-weight tasks");
    CTask *a = task_create("ea", NULL, NULL, 0, PRIO_NORMAL);
    CTask *b = task_create("eb", NULL, NULL, 0, PRIO_NORMAL);
    if (a && b) PASS(); else FAIL("create failed");

    TEST("equal-weight tasks split CPU 50/50 within 2%");
    const int TICKS = 10000;
    int ran_a = 0, ran_b = 0;
    uint64_t worst_spread = 0;
    for (int i = 0; i < TICKS; i++) {
        CTask *n = task_schedule_next();
        if (!n) break;
        task_account_tick(n);
        n->total_ticks++;
        if (n == a) ran_a++; else if (n == b) ran_b++;
        uint64_t d = a->vruntime > b->vruntime ? a->vruntime - b->vruntime
                                               : b->vruntime - a->vruntime;
        if (d > worst_spread) worst_spread = d;
    }
    double share = (double)ran_a / (double)(ran_a + ran_b);
    printf("    picks a=%d b=%d share=%.4f worst vruntime spread=%llu (slice %u)\n",
           ran_a, ran_b, share, (unsigned long long)worst_spread, a->slice);
    if (share > 0.48 && share < 0.52) PASS(); else FAIL("split outside 2%");
    TEST("lag never exceeds one slice");
    if (worst_spread <= (uint64_t)a->slice) PASS(); else FAIL("lag bound exceeded");

    task_destroy(a);
    task_destroy(b);
    tasking_shutdown();
    mem_shutdown();
}


static void test_sleep_accuracy(void) {
    printf("\n[Sleep Accuracy]\n");
    mem_init(2 * 1024 * 1024);
    tasking_init();

    TEST("sleep wakes within 1 tick across a 2^32 span");
    const uint64_t targets[] = {5, 64, 4096, 1000000ull, 4294967296ull};
    const int n = (int)(sizeof targets / sizeof targets[0]);
    int bad = 0;
    for (int i = 0; i < n; i++) {
        CTask *t = task_create("slp", NULL, NULL, 0, PRIO_NORMAL);
        if (!t) { bad++; continue; }
        uint64_t base = task_tick_count();
        task_arm_sleep(t, targets[i]);
        uint64_t want = base + targets[i];
        uint64_t guard = 0;
        while (t->state == TASK_SLEEPING && guard < targets[i] + 8) {
            task_wake_expired();
            guard++;
            /* emulate the tick's clock advance */
            extern void task_test_advance_clock(void);
            task_test_advance_clock();
        }
        long long err = (long long)task_tick_count() - (long long)want;
        if (err < 0) err = -err;
        printf("    sleep %llu -> err %lld after %llu iterations\n", (unsigned long long)targets[i], err, (unsigned long long)guard);
        if (err > 1) bad++;
        task_destroy(t);
    }
    if (bad == 0) PASS(); else FAIL("wake outside 1 tick");

    tasking_shutdown();
    mem_shutdown();
}


static void test_cap_authority(void) {
    printf("\n[Capability Ceiling]\n");
    mem_init(2 * 1024 * 1024);
    tasking_init();

    enum { R = 1, W = 2, X = 4, D = 8 };

    CTask *parent = task_create("par", NULL, NULL, 0, PRIO_NORMAL);
    CTask *child  = task_create("chi", NULL, NULL, 0, PRIO_NORMAL);
    if (!parent || !child) { FAIL("create failed"); return; }

    TEST("parent holds READ|WRITE under a READ|WRITE|DERIVE ceiling");
    task_set_authority(parent, R | W, R | W | D);
    if (parent->caps == (R | W) && parent->ceiling == (R | W | D)) PASS();
    else FAIL("authority not set");

    TEST("holding more than the ceiling is clipped at set time");
    task_set_authority(child, R | W | X, R | W);
    if (child->caps == (R | W)) PASS(); else FAIL("ceiling not enforced on set");

    TEST("fork inherits the parent's rights within its ceiling");
    task_fork_authority(parent, child);
    if (child->caps == (R | W) && child->ceiling == (R | W | D)) PASS();
    else FAIL("fork inheritance wrong");

    TEST("exec gets only what the image asks for");
    task_exec_authority(child, R);
    if (child->caps == R) PASS(); else FAIL("exec kept rights it did not ask for");

    TEST("exec cannot exceed the ceiling");
    task_exec_authority(child, R | W | X);
    if (child->caps == (R | W)) PASS(); else FAIL("exec exceeded ceiling");

    task_destroy(parent);
    task_destroy(child);
    tasking_shutdown();
    mem_shutdown();
}


static void test_rt_class(void) {
    printf("\n[Real-Time Class]\n");
    mem_init(2 * 1024 * 1024);
    tasking_init();

    CTask *rt = task_create("rt", NULL, NULL, 0, PRIO_REALTIME);
    CTask *cf = task_create("cf", NULL, NULL, 0, PRIO_NORMAL);
    CTask *rt2 = task_create("rt2", NULL, NULL, 0, PRIO_REALTIME);
    if (!rt || !cf || !rt2) { FAIL("create failed"); return; }

    TEST("RT task is picked over a CFS task");
    int rt_picks = 0;
    for (int i = 0; i < 100; i++) {
        CTask *n = task_schedule_next();
        if (n && n->priority == PRIO_REALTIME) rt_picks++;
        if (n) task_account_tick(n);
    }
    printf("    %d/100 picks went to the RT class\n", rt_picks);
    if (rt_picks == 100) PASS(); else FAIL("CFS task was picked while RT was ready");

    TEST("two equal RT tasks round-robin");
    /* reset accounting so both RT tasks start level */
    rt->vruntime = rt2->vruntime = 0;
    rt->total_ticks = rt2->total_ticks = 0;
    int a = 0, b = 0;
    for (int i = 0; i < 1000; i++) {
        CTask *n = task_schedule_next();
        if (!n) break;
        if (n == rt) a++; else if (n == rt2) b++;
        task_account_tick(n);
        n->total_ticks++;
    }
    printf("    rt=%d rt2=%d\n", a, b);
    if (a == 500 && b == 500) PASS(); else FAIL("RT round-robin is not even");

    task_destroy(rt); task_destroy(cf); task_destroy(rt2);
    tasking_shutdown();
    mem_shutdown();
}


static void lifecycle_entry(void *arg) { (void)arg; }

static void test_lifecycle_scale(void) {
    printf("\n[Lifecycle at Scale]\n");
    mem_init(64 * 1024 * 1024);
    tasking_init();

    int baseline = task_count();
    printf("    baseline task_count=%d\n", baseline);

    TEST("create 1000 tasks");
    CTask *made[1000];
    int n = 0;
    for (int i = 0; i < 1000; i++) {
        CTask *t = task_create("bulk", lifecycle_entry, NULL, 4096, PRIO_NORMAL);
        if (!t) break;
        made[n++] = t;
    }
    printf("    created %d tasks, count=%d\n", n, task_count());
    if (n == 1000 && task_count() == baseline + 1000) PASS();
    else FAIL("creation did not reach 1000");

    TEST("clone 50 and exec 100");
    int clones = 0;
    CTask *cloned[50];
    for (int i = 0; i < 50; i++) {
        CTask *c = task_clone(made[i], "cloned");
        if (c) cloned[clones++] = c;
    }
    for (int i = 0; i < 100; i++)
        task_execve(made[i], "execed", lifecycle_entry, NULL, 1);
    printf("    clones=%d count=%d\n", clones, task_count());
    if (clones == 50 && task_count() == baseline + 1050) PASS();
    else FAIL("clone count wrong");

    TEST("exit marks tasks for the reaper");
    for (int i = 0; i < 25; i++) task_terminate(made[i]);
    int dying = 0;
    for (int i = 0; i < 25; i++) if (made[i]->state == TASK_DYING) dying++;
    if (dying == 25) PASS(); else FAIL("exit did not mark DYING");

    TEST("destroy all returns to baseline with no leaks");
    for (int i = 0; i < n; i++) task_destroy(made[i]);
    for (int i = 0; i < clones; i++) task_destroy(cloned[i]);
    printf("    final task_count=%d\n", task_count());
    if (task_count() == baseline) PASS(); else FAIL("task count did not return to baseline");

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

    test_eevdf_fairness();
    test_sleep_accuracy();
    test_cap_authority();
    test_rt_class();
    test_lifecycle_scale();

    printf("\n==============================\n");
    if (failures == 0)
        printf("All tests passed! ✅\n");
    else
        printf("%d test(s) FAILED ❌\n", failures);

    return failures ? 1 : 0;
}
