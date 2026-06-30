/*
 * control_test.c  --  WuBuOS Control Panel Test Suite
 * Cell 395: Win98-style settings panel
 */

#include "control.h"
#include <stdio.h>

static int g_run = 0, g_pass = 0;

#define T(cond, msg) do { \
    g_run++; \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else { printf("  ❌ %s\n", msg); } \
} while(0)

int main(void) {
    printf("=== WuBuOS Control Panel Test Suite ===\n\n");

    /* -- Lifecycle -- */
    printf("[Lifecycle]\n");
    control_init();
    T(1, "control_init succeeds");

    /* -- Window Creation -- */
    printf("\n[Window Creation]\n");
    control_open();
    T(1, "control_open succeeds");

    /* -- Shutdown -- */
    printf("\n[Shutdown]\n");
    control_shutdown();
    T(1, "control_shutdown succeeds");

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}