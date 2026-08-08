/* test_svc_super.c — N2 gate: the in-process service supervisor is real.
 *
 * Proves (no systemd, no shell):
 *  1. start() fork/execs a service binary, status() sees it RUNNING
 *  2. stop() SIGTERMs and reaps it, status() sees DISABLED
 *  3. restart() works
 *  4. unexpected death + auto_restart=true -> supervisor restarts it
 *  5. auto_restart=false + death -> FAILED + on_fail callback fires
 *  6. Type=notify: stays STARTING until notify_ready(), then RUNNING
 *  7. ordered boot: dep must be RUNNING before dependent starts
 *  8. plain-text journal is written and readable
 *
 * Services are real host binaries (/bin/sleep, /bin/true) with
 * exec_path passed explicitly — no Arch root needed for the test.
 */
#include "wubu_archd_svc.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

#define ROOT "testroot"

static int failures = 0;
#define CHECK(c, msg) do { \
        if (c) printf("  [PASS] %s\n", msg); \
        else { printf("  [FAIL] %s\n", msg); failures++; } \
    } while (0)

static WubuArchService st;   /* reused status buffer */

static void on_fail_cb(const char *root, const char *svc, void *ud) {
    (void)root; (void)ud;
    printf("  [cb] on_fail fired for '%s'\n", svc);
}

int main(void) {
    printf("=== test_svc_super (N2 supervisor) ===\n");

    wubu_svc_supervisor_t *s = wubu_svc_supervisor_create();
    CHECK(s != NULL, "supervisor created");

    /* -- 1: start + status ---------------------------------------- */
    char *argv0[] = { (char *)"sleep", (char *)"30", NULL };
    CHECK(wubu_svc_supervisor_add(s, ROOT, "svc1", "/bin/sleep", argv0) == 0,
          "add svc1 (/bin/sleep 30)");
    /* SIMPLE type: alive == ready (truthful immediate-RUNNING checks) */
    wubu_svc_supervisor_set_type(s, ROOT, "svc1", SVC_TYPE_SIMPLE);
    CHECK(wubu_svc_supervisor_start(s, ROOT, "svc1") == 0, "start svc1");
    memset(&st, 0, sizeof(st));
    CHECK(wubu_svc_supervisor_status(s, ROOT, "svc1", &st) == 0, "status svc1");
    CHECK(st.pid > 0 && st.state == SERVICE_STATE_RUNNING,
          "svc1 RUNNING with pid>0");

    /* -- 2: stop -------------------------------------------------- */
    CHECK(wubu_svc_supervisor_stop(s, ROOT, "svc1") == 0, "stop svc1");
    memset(&st, 0, sizeof(st));
    wubu_svc_supervisor_status(s, ROOT, "svc1", &st);
    CHECK(st.pid == 0, "svc1 stopped (pid 0)");

    /* -- 3: restart ----------------------------------------------- */
    CHECK(wubu_svc_supervisor_restart(s, ROOT, "svc1") == 0, "restart svc1");
    memset(&st, 0, sizeof(st));
    wubu_svc_supervisor_status(s, ROOT, "svc1", &st);
    CHECK(st.pid > 0 && st.state == SERVICE_STATE_RUNNING,
          "svc1 RUNNING after restart");

    /* -- 4: auto-restart on death --------------------------------- */
    wubu_svc_supervisor_set_restart(s, ROOT, "svc1", true);
    pid_t p1 = st.pid;
    kill(p1, SIGKILL);           /* kill it; reap() must resurrect */
    usleep(100000);
    wubu_svc_supervisor_reap(s);
    memset(&st, 0, sizeof(st));
    wubu_svc_supervisor_status(s, ROOT, "svc1", &st);
    CHECK(st.pid > 0, "svc1 auto-restarted after SIGKILL");
    CHECK(st.restart_count >= 1, "restart_count >= 1");

    /* -- 5: no-restart + on_fail ---------------------------------- */
    wubu_svc_supervisor_set_restart(s, ROOT, "svc1", false);
    wubu_svc_supervisor_set_on_fail(s, on_fail_cb, NULL);
    pid_t p2 = st.pid;
    kill(p2, SIGKILL);
    usleep(100000);
    int n = wubu_svc_supervisor_poll(s);
    memset(&st, 0, sizeof(st));
    wubu_svc_supervisor_status(s, ROOT, "svc1", &st);
    CHECK(n == 1, "poll detected 1 unexpected death");
    CHECK(st.state == SERVICE_STATE_FAILED, "svc1 FAILED (no restart)");
    CHECK(st.pid == 0, "svc1 reaped (pid 0)");

    /* -- 6: Type=notify readiness --------------------------------- */
    char *argv6[] = { (char *)"sleep", (char *)"30", NULL };
    wubu_svc_supervisor_add(s, ROOT, "svc6", "/bin/sleep", argv6);
    wubu_svc_supervisor_set_type(s, ROOT, "svc6", SVC_TYPE_NOTIFY);
    wubu_svc_supervisor_start(s, ROOT, "svc6");
    memset(&st, 0, sizeof(st));
    wubu_svc_supervisor_status(s, ROOT, "svc6", &st);
    CHECK(st.pid > 0 && st.state != SERVICE_STATE_RUNNING,
          "svc6 (notify) NOT lying RUNNING before ready");
    wubu_svc_supervisor_notify_ready(s, ROOT, "svc6");
    memset(&st, 0, sizeof(st));
    wubu_svc_supervisor_status(s, ROOT, "svc6", &st);
    CHECK(st.state == SERVICE_STATE_RUNNING, "svc6 RUNNING after notify");

    /* -- 7: ordered boot (dep ordering) --------------------------- */
    wubu_svc_supervisor_t *s2 = wubu_svc_supervisor_create();
    char *a[] = { (char *)"sleep", (char *)"30", NULL };
    wubu_svc_supervisor_add(s2, ROOT, "db", "/bin/sleep", a);
    wubu_svc_supervisor_set_type(s2, ROOT, "db", SVC_TYPE_SIMPLE);
    wubu_svc_supervisor_add(s2, ROOT, "app", "/bin/true", a);
    wubu_svc_supervisor_set_type(s2, ROOT, "app", SVC_TYPE_SIMPLE);
    wubu_svc_supervisor_add_dep(s2, ROOT, "app", "db");   /* Requires=db */
    int not_ready = wubu_svc_supervisor_boot(s2);
    CHECK(not_ready == 0, "ordered boot: all units RUNNING");
    memset(&st, 0, sizeof(st));
    wubu_svc_supervisor_status(s2, ROOT, "db", &st);
    CHECK(st.state == SERVICE_STATE_RUNNING, "dep 'db' RUNNING");
    wubu_svc_supervisor_status(s2, ROOT, "app", &st);
    CHECK(st.state == SERVICE_STATE_RUNNING, "dependent 'app' RUNNING");
    /* missing dep must BLOCK the dependent */
    wubu_svc_supervisor_t *s3 = wubu_svc_supervisor_create();
    char *b[] = { (char *)"/bin/true", NULL };
    wubu_svc_supervisor_add(s3, ROOT, "orphan", "/bin/true", b);
    wubu_svc_supervisor_set_type(s3, ROOT, "orphan", SVC_TYPE_SIMPLE);
    wubu_svc_supervisor_add_dep(s3, ROOT, "orphan", "missing-svc");
    int nr3 = wubu_svc_supervisor_boot(s3);
    CHECK(nr3 >= 1, "missing dep blocks dependent (boot reports not_ready)");

    /* -- 8. journal ------------------------------------------------ */
    wubu_svc_supervisor_log(s, ROOT, "svc1", "hello journal");
    char jbuf[512];
    int jn = wubu_svc_supervisor_logs(s, ROOT, "svc1", jbuf, sizeof(jbuf));
    CHECK(jn > 0 && strstr(jbuf, "hello journal") != NULL,
          "journal ring contains the log line");

    /* -- 9. crash-loop backoff (s6 lesson) ------------------------- */
    /* A unit that dies INSTANTLY must not hot-restart forever: after
     * the first death the supervisor engages exponential backoff, so
     * immediate re-polls see the backoff window and PAUSE. */
    wubu_svc_supervisor_t *s4 = wubu_svc_supervisor_create();
    /* /bin/true exits immediately = instant crash loop */
    char *bt[] = { (char *)"/bin/true", NULL };
    CHECK(wubu_svc_supervisor_add(s4, ROOT, "flaky", "/bin/true", bt) == 0,
          "add crash-looping unit (/bin/true)");
    wubu_svc_supervisor_set_restart(s4, ROOT, "flaky", true);
    wubu_svc_supervisor_set_type(s4, ROOT, "flaky", SVC_TYPE_SIMPLE);
    wubu_svc_supervisor_start(s4, ROOT, "flaky");
    usleep(100000);
    int n4 = wubu_svc_supervisor_poll(s4);   /* 1st death: restart + backoff */
    CHECK(n4 == 0, "1st death restarts (backoff engaged, not 'unexpected')");
    memset(&st, 0, sizeof(st));
    wubu_svc_supervisor_status(s4, ROOT, "flaky", &st);
    CHECK(st.restart_count >= 1, "restart_count >= 1 after 1st crash");
    /* immediate second poll: still dying, but backoff window is active —
     * the supervisor must NOT keep restarting (crash-loop guard) */
    usleep(100000);
    int n4b = wubu_svc_supervisor_poll(s4);
    memset(&st, 0, sizeof(st));
    wubu_svc_supervisor_status(s4, ROOT, "flaky", &st);
    CHECK(st.pid == 0, "crash-loop guard: no hot restart inside backoff");
    (void)n4b;
    wubu_svc_supervisor_destroy(s4);

    wubu_svc_supervisor_destroy(s);
    wubu_svc_supervisor_destroy(s2);
    wubu_svc_supervisor_destroy(s3);

    if (failures) { printf("N2 GATE FAILED (%d)\n", failures); return 1; }
    printf("N2 GATE PASSED — supervisor is real (no systemd, no shell)\n");
    return 0;
}
