/*
 * wubu_system_test.c -- WuBuOS System Root (immutable/atomic) tests.
 *
 * Verifies the SteamOS-style immutable/atomic system layer built on the real
 * snapshot manager: commit creates a read-only baseline, active label tracks
 * it, rollback restores a previous baseline, and DEVELOPER mode disables the
 * read-only guarantee.
 */

#define _POSIX_C_SOURCE 200809L
#include "wubu_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_run = 0, g_pass = 0;
#define T(cond, msg) do { \
    g_run++; \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else { printf("  ❌ %s (line %d)\n", msg, __LINE__); } \
} while (0)

int main(void) {
    /* Isolate from any real ~/.wubu by pointing the store at /tmp. */
    setenv("HOME", "/tmp", 1);
    unsetenv("WUBU_SYSTEM_DEV"); /* non-developer: read-only enforced */

    printf("=== WuBuOS System Root (immutable/atomic) Test Suite ===\n\n");

    /* Init with an isolated store. */
    char store[256];
    snprintf(store, sizeof(store), "/tmp/wubu-system-test-%d", (int)getpid());
    T(wubu_system_init(store) == 0, "system init");
    T(wubu_system_ready(), "system ready");

    /* No baseline yet -> not read-only, count 0. */
    T(wubu_system_baseline_count() == 0, "no baselines initially");
    T(!wubu_system_is_readonly(), "not read-only before first commit");

    /* Commit baseline #1. */
    char id1[64] = {0};
    T(wubu_system_commit("2026-07-07-stable", id1, sizeof(id1)) == 0, "commit baseline #1");
    T(id1[0] != '\0', "baseline #1 id assigned");
    T(wubu_system_baseline_count() == 1, "baseline count = 1");
    T(wubu_system_is_readonly(), "read-only after commit (immutable rootfs)");

    char lbl[128] = {0};
    T(wubu_system_active_label(lbl, sizeof(lbl)) == 0, "active label query");
    T(strcmp(lbl, "2026-07-07-stable") == 0, "active label is baseline #1");

    /* Commit baseline #2 (chains incrementally). */
    char id2[64] = {0};
    T(wubu_system_commit("2026-07-08-stable", id2, sizeof(id2)) == 0, "commit baseline #2");
    T(strcmp(id1, id2) != 0, "baseline #2 has distinct id");
    T(wubu_system_baseline_count() == 2, "baseline count = 2");

    char lbl2[128] = {0};
    T(wubu_system_active_label(lbl2, sizeof(lbl2)) == 0, "active label query #2");
    T(strcmp(lbl2, "2026-07-08-stable") == 0, "active label is baseline #2");

    /* Rollback to baseline #1. */
    T(wubu_system_rollback(id1) == 0, "rollback to baseline #1");
    char lbl3[128] = {0};
    T(wubu_system_active_label(lbl3, sizeof(lbl3)) == 0, "active label after rollback");
    T(strcmp(lbl3, "2026-07-07-stable") == 0, "active label restored to baseline #1");

    /* Shutdown + re-init against same store: state should persist in memory
     * (manager is re-created; this exercises the lifecycle path). */
    wubu_system_shutdown();
    T(!wubu_system_ready(), "not ready after shutdown");
    T(wubu_system_init(store) == 0, "re-init against same store");
    T(wubu_system_ready(), "ready after re-init");

    /* DEVELOPER mode disables read-only enforcement. */
    wubu_system_shutdown();
    setenv("WUBU_SYSTEM_DEV", "1", 1);
    T(wubu_system_init(store) == 0, "init in DEVELOPER mode");
    T(wubu_system_developer_mode(), "developer mode detected");
    T(!wubu_system_is_readonly(), "read-only NOT enforced in developer mode");

    wubu_system_shutdown();
    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
