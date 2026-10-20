/*
 * wubu_ns_pid_test.c -- asserts the per-task /n/pid/{tid} subtree
 * contract (NAMESPACE #598): registration, per-task isolation, lifecycle.
 */
#include "wubu_ns_pid.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

#define R_READ  0x1ULL
#define R_WRITE 0x2ULL

int main(void)
{
    wubu_ns_pid_reset();
    CHECK(wubu_ns_pid_count() == 0);
    CHECK(wubu_ns_pid_lookup(42) == NULL);
    CHECK(wubu_ns_pid_unregister(42) == -1);

    /* ---- registration + canonical path ---- */
    char path[32];
    CHECK(strcmp(wubu_ns_pid_path(42, path, sizeof(path)), "/n/pid/42") == 0);
    CHECK(wubu_ns_pid_path(42, NULL, 0) == NULL);
    CHECK(wubu_ns_pid_register(42, "/store/tasks/42") == 0);
    CHECK(wubu_ns_pid_count() == 1);
    CHECK(wubu_ns_pid_register(7, "/store/tasks/7") == 0);
    CHECK(wubu_ns_pid_count() == 2);
    /* duplicate tid rejected */
    CHECK(wubu_ns_pid_register(42, "/store/other") == -2);
    CHECK(wubu_ns_pid_count() == 2);
    /* bad args fail closed */
    CHECK(wubu_ns_pid_register(9, NULL) == -1);
    CHECK(wubu_ns_pid_register(9, "") == -1);

    /* ---- per-task isolation: each view resolves only its own subtree ---- */
    wubu_ns_root_t *v42 = wubu_ns_pid_lookup(42);
    wubu_ns_root_t *v7  = wubu_ns_pid_lookup(7);
    CHECK(v42 != NULL && v7 != NULL && v42 != v7);
    CHECK(strcmp(wubu_ns_root_name(v42), "pid-42") == 0);

    char src[WUBU_NS_ROOT_MAX_PATH];
    uint64_t rights = 0;
    /* task 42 sees its own files, mapped to its own storage base */
    CHECK(wubu_ns_root_lookup(v42, "/n/pid/42/ctrl", &rights, src) == 0);
    CHECK(strcmp(src, "/store/tasks/42/ctrl") == 0);
    CHECK(rights == (R_READ | R_WRITE));
    /* and CANNOT see task 7's subtree -- the view is the isolation */
    CHECK(wubu_ns_root_lookup(v42, "/n/pid/7/ctrl", &rights, src) == -1);
    CHECK(wubu_ns_root_lookup(v7, "/n/pid/7/ctrl", &rights, src) == 0);
    CHECK(strcmp(src, "/store/tasks/7/ctrl") == 0);
    CHECK(wubu_ns_root_lookup(v7, "/n/pid/42/ctrl", &rights, src) == -1);
    /* nothing outside /n/pid/<tid> is granted */
    CHECK(wubu_ns_root_lookup(v42, "/n/svc", &rights, src) == -1);

    /* ---- lifecycle: unregister destroys the view ---- */
    CHECK(wubu_ns_pid_unregister(42) == 0);
    CHECK(wubu_ns_pid_count() == 1);
    CHECK(wubu_ns_pid_lookup(42) == NULL);
    CHECK(wubu_ns_pid_unregister(42) == -1);
    /* re-register after exit works (a fresh view, fresh grant) */
    CHECK(wubu_ns_pid_register(42, "/store/tasks/42b") == 0);
    v42 = wubu_ns_pid_lookup(42);
    CHECK(v42 != NULL);
    CHECK(wubu_ns_root_lookup(v42, "/n/pid/42/x", &rights, src) == 0);
    CHECK(strcmp(src, "/store/tasks/42b/x") == 0);

    /* ---- full registry ---- */
    wubu_ns_pid_reset();
    int rc = 0;
    for (uint64_t t = 1; t <= WUBU_NS_PID_MAX_TASKS + 2 && rc != -3; t++) {
        char base[64];
        snprintf(base, sizeof(base), "/store/tasks/%llu",
                 (unsigned long long)t);
        rc = wubu_ns_pid_register(t, base);
    }
    CHECK(rc == -3);
    CHECK(wubu_ns_pid_count() == WUBU_NS_PID_MAX_TASKS);
    wubu_ns_pid_reset();
    CHECK(wubu_ns_pid_count() == 0);

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
