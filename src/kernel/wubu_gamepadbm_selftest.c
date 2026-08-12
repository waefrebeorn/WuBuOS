/*
 * wubu_gamepadbm_selftest.c -- verifies gamepad button map routing.
 */
#include "wubu_gamepadbm.h"
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
    printf("=== wubu_gamepadbm_selftest ===\n");

    wubu_gamepadbm_probe();

    int p = wubu_gamepadbm_present();
    CHECK(p == 0 || p == 1, "gamepadbm present is boolean");

    /* Button mapping. */
    CHECK(wubu_gamepadbm_map(0) == 0, "button 0 = A");
    CHECK(wubu_gamepadbm_map(3) == 3, "button 3 = Y");
    CHECK(wubu_gamepadbm_map(15) == 15, "button 15 = valid");
    CHECK(wubu_gamepadbm_map(16) == -1, "button 16 = invalid");
    CHECK(wubu_gamepadbm_map(-1) == -1, "button -1 = invalid");

    /* Button press detection. */
    CHECK(wubu_gamepadbm_is_pressed(1) == 1, "value 1 = pressed");
    CHECK(wubu_gamepadbm_is_pressed(0) == 0, "value 0 = not pressed");

    /* Summary builds. */
    char out[160] = "";
    wubu_gamepadbm_summary(out, sizeof(out));
    CHECK(strstr(out, "gamepadbm[") != NULL, "summary has gamepadbm fragment");

    printf("\n=== GAMEPADBM TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
