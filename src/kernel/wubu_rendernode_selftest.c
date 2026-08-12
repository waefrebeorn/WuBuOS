/*
 * wubu_rendernode_selftest.c -- verifies GPU render node routing.
 */
#include "wubu_rendernode.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while(0)

static int failures = 0;
static int passed = 0;

int main(void)
{
    printf("=== wubu_rendernode_selftest ===\n");

    wubu_rendernode_probe();

    int p = wubu_rendernode_present();
    CHECK(p == 0 || p == 1, "rendernode present is boolean");

    /* FD validation. */
    CHECK(wubu_rendernode_valid_fd(5) == 1, "fd 5 = valid");
    CHECK(wubu_rendernode_valid_fd(-1) == 0, "fd -1 = invalid");

    /* Priority routing. */
    CHECK(wubu_rendernode_priority(0) == 1, "priority 0 = normal");
    CHECK(wubu_rendernode_priority(50) == 2, "priority 50 = high");
    CHECK(wubu_rendernode_priority(150) == 3, "priority 150 = real-time");
    CHECK(wubu_rendernode_priority(-1) == 0, "priority -1 = invalid");

    /* Summary builds. */
    char out[160] = "";
    wubu_rendernode_summary(out, sizeof(out));
    CHECK(strstr(out, "rendernode[") != NULL, "summary has rendernode fragment");

    printf("\n=== RENDERNODE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
