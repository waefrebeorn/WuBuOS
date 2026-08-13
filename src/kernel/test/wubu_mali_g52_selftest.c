/*
 * wubu_mali_g52_selftest.c -- verifies ARM Mali G52 routing.
 */
#include "wubu_mali_g52.h"
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
    printf("=== wubu_mali_g52_selftest ===\n");

    wubu_mali_g52_probe();

    int p = wubu_mali_g52_present();
    CHECK(p == 0 || p == 1, "mali_g52 present is boolean");

    /* Panfrost routing. */
    CHECK(wubu_mali_g52_uses_panfrost(1) == 1, "panfrost available = uses panfrost");
    CHECK(wubu_mali_g52_uses_panfrost(0) == 0, "no panfrost = not used");

    /* OpenGL ES support. */
    CHECK(wubu_mali_g52_opengl_es(32) == 1, "GLES 3.2 supported on G52");
    CHECK(wubu_mali_g52_opengl_es(40) == 0, "GLES 4.0 not supported on G52");

    /* Summary builds. */
    char out[160] = "";
    wubu_mali_g52_summary(out, sizeof(out));
    CHECK(strstr(out, "mali_g52[") != NULL, "summary has mali_g52 fragment");

    printf("\n=== MALI_G52 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
