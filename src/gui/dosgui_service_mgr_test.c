/*
 * dosgui_service_mgr_test.c -- Regression test for E3 integration:
 * wubu_archd wired as the Desktop's autostart/service manager.
 *
 * Asserts real behavior (not just non-crash):
 *  - the manager initializes a live archd handle,
 *  - registering autostart entries does real dedup'd bookkeeping,
 *  - boot() iterates every registered entry and drives wubu_archd_svc_start,
 *    recording per-entry results,
 *  - shutdown tears the manager down (active=false, count reset).
 *
 * C11, no nested functions.
 */
#include "dosgui_service_mgr.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define CHECK(cond, msg) do { if (cond) { printf("  ✅ %s\n", msg); } \
                              else { printf("  ❌ %s\n", msg); return 1; } } while (0)

int main(void) {
    printf("Testing Desktop service manager (E3: wubu_archd wiring)...\n");

    /* Init: creates a live archd handle. */
    int rc = dosgui_service_mgr_init();
    CHECK(rc == 0, "service manager initializes");
    CHECK(dosgui_service_mgr_active(), "manager reports active after init");
    CHECK(dosgui_service_mgr_handle() != NULL, "archd handle is live");

    /* Register two distinct autostart entries. */
    int i0 = dosgui_service_mgr_register_autostart("base", "sshd");
    int i1 = dosgui_service_mgr_register_autostart("base", "nginx");
    CHECK(i0 == 0, "first entry registered at index 0");
    CHECK(i1 == 1, "second entry registered at index 1");
    CHECK(dosgui_service_mgr_count() == 2, "count == 2 after two registers");

    /* Dedup: re-registering the same (root,svc) must not grow the list. */
    int dup = dosgui_service_mgr_register_autostart("base", "sshd");
    CHECK(dup == 0, "duplicate registration returns existing index");
    CHECK(dosgui_service_mgr_count() == 2, "count unchanged after duplicate");

    /* Entries are stored correctly. */
    const DosguiAutostartEntry *e0 = dosgui_service_mgr_entry(0);
    CHECK(e0 && strcmp(e0->root, "base") == 0 && strcmp(e0->svc, "sshd") == 0,
          "entry[0] holds (base, sshd)");

    /* Boot: iterates both entries and drives wubu_archd_svc_start.
     * In this sandbox there is no real Arch root, so svc_start returns <0;
     * the test asserts the iteration + result recording happened (real work),
     * not that the (unavailable) systemd start succeeded. */
    DosguiBootResult res = dosgui_service_mgr_boot();
    CHECK(res.attempted == 2, "boot attempted both autostart entries");
    CHECK(res.attempted == res.started + res.failed, "boot results account for all attempts");

    const DosguiAutostartEntry *e1 = dosgui_service_mgr_entry(1);
    CHECK(e1 && e1->booted, "entry[1] marked booted");

    /* Invalid registers rejected. */
    CHECK(dosgui_service_mgr_register_autostart(NULL, "x") < 0, "null root rejected");
    CHECK(dosgui_service_mgr_register_autostart("base", "") < 0, "empty svc rejected");
    CHECK(dosgui_service_mgr_count() == 2, "count still 2 after invalid registers");

    /* Shutdown tears down cleanly. */
    dosgui_service_mgr_shutdown();
    CHECK(!dosgui_service_mgr_active(), "manager inactive after shutdown");
    CHECK(dosgui_service_mgr_handle() == NULL, "handle null after shutdown");
    CHECK(dosgui_service_mgr_count() == 0, "count reset after shutdown");

    /* Re-init works (idempotent lifecycle). */
    CHECK(dosgui_service_mgr_init() == 0, "re-init succeeds");
    dosgui_service_mgr_shutdown();

    printf("✅ All Desktop service manager tests passed\n");
    return 0;
}
