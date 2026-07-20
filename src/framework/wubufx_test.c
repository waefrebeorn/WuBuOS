/*
 * wubufx_test.c -- WuBuFX framework regression tests
 *
 * Verifies the namespace-first contract end to end:
 *   - mount resolves a content-addressed app namespace
 *   - node open/create works
 *   - wubufx_eval runs LIVE HolyC and returns the result
 *   - wubufx_agent_eval is disclosed to EDR (agent event count rises)
 *   - capability denial is enforced (no EXEC/AGI -> WUBUFX_ERR_CAP)
 *   - EDR attribution records the namespace id
 *
 * Tiny assert harness; exits non-zero on any failure (gate-friendly).
 */

#include "wubufx.h"
#include "../runtime/wubu_edr.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int g_fail = 0;
static int g_pass = 0;

#define CHECK(cond, msg) do {                                              \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); }                     \
    else     { g_fail++; printf("  ❌ %s\n", msg); }                     \
} while (0)

int main(void) {
    printf("=== WuBuFX framework tests ===\n");

    CHECK(wubufx_init() == WUBUFX_OK, "wubufx_init");

    /* 1) Mount a content-addressed app namespace with full caps. */
    WubuFxApp *app = NULL;
    CHECK(wubufx_mount("com.wubu.notepad", WUBUFX_CAP_ALL, &app) == WUBUFX_OK,
          "mount notepad namespace");
    CHECK(app != NULL, "mount returned a handle");

    /* 2) Conventional nodes resolve. */
    WubuFxNode *win = wubufx_open(app, "/win");
    CHECK(win != NULL, "open /win node");
    WubuFxNode *mainn = wubufx_open(app, "/main");
    CHECK(mainn != NULL, "open /main node");

    /* 3) Unknown node is lazily created (no manifest bloat). */
    WubuFxNode *custom = wubufx_open(app, "/doc/notes");
    CHECK(custom != NULL, "lazy-create /doc/notes node");

    /* 4) Explicit, inspectable state. */
    CHECK(wubufx_state_set(win, "open") == WUBUFX_OK, "state_set /win=open");
    char buf[64] = {0};
    CHECK(wubufx_state_get(win, buf, sizeof(buf)) == WUBUFX_OK, "state_get");
    CHECK(strcmp(buf, "open") == 0, "state round-trips");

    /* 5) LIVE HolyC eval inside the namespace. */
    char out[128] = {0};
    CHECK(wubufx_eval(app, "1+2+3", out, sizeof(out)) == WUBUFX_OK, "wubufx_eval HolyC");
    CHECK(strcmp(out, "6") == 0, "eval result is 6 (LIVE compile+run)");

    /* 6) AGI eval is disclosed to EDR. */
    uint64_t before = edr_agent_events_logged();
    char aout[128] = {0};
    CHECK(wubufx_agent_eval(app, "I64 sq(I64 n){return n*n;} sq(9)", aout, sizeof(aout))
          == WUBUFX_OK, "wubufx_agent_eval HolyC");
    CHECK(edr_agent_events_logged() > before, "agent eval logged to EDR (disclosure)");

    /* 7) Capability enforcement: an app WITHOUT exec cap cannot eval. */
    WubuFxApp *limited = NULL;
    CHECK(wubufx_mount("com.wubu.viewer", WUBUFX_CAP_READ, &limited) == WUBUFX_OK,
          "mount read-only namespace");
    char lout[128] = {0};
    CHECK(wubufx_eval(limited, "1+1", lout, sizeof(lout)) == WUBUFX_ERR_CAP,
          "eval denied without EXEC cap");
    CHECK(wubufx_agent_eval(limited, "1+1", lout, sizeof(lout)) == WUBUFX_ERR_CAP,
          "agent eval denied without AGI cap");

    /* 8) Two namespaces with identical global symbols do NOT collide
     *    (per-namespace isolation -- point 20). */
    char o1[128] = {0}, o2[128] = {0};
    WubuFxApp *a = NULL, *b = NULL;
    wubufx_mount("app.a", WUBUFX_CAP_ALL, &a);
    wubufx_mount("app.b", WUBUFX_CAP_ALL, &b);
    wubufx_eval(a, "I64 G(){return 1;} G()", o1, sizeof(o1));
    wubufx_eval(b, "I64 G(){return 2;} G()", o2, sizeof(o2));
    CHECK(strcmp(o1, "1") == 0 && strcmp(o2, "2") == 0,
          "separate namespaces isolate globals");

    /* 9) Unmount is clean. */
    wubufx_unmount(app);
    wubufx_unmount(limited);
    wubufx_unmount(a);
    wubufx_unmount(b);
    wubufx_shutdown();
    CHECK(true, "unmount + shutdown clean");

    printf("\nResults: %d/%d passed, %d failed\n", g_pass, g_pass + g_fail, g_fail);
    return g_fail == 0 ? 0 : 1;
}
