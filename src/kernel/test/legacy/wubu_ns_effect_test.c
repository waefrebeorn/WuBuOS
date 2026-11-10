/*
 * wubu_ns_effect_test.c -- the /n/effect subtree test.
 *
 * Asserts the file<->API routing:
 *   1. publish creates /n/effect/list + /n/effect/stack
 *   2. the list names every effect a program can pick
 *   3. writing a payload through the API adds to the SAME stack the surface
 *      applies, and refresh republishes it
 *   4. a bad payload is refused without changing the stack
 *   5. the stack actually changes pixels (the engine is linked, not stubbed)
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_effect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NSROOT "/tmp/ns_effect_test"
#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

static int read_file(const char *p, char *out, size_t cap)
{
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    return 1;
}

int main(void)
{
    printf("=== wubu_ns_effect_test (the /n/effect control subtree) ===\n");
    system("rm -rf " NSROOT);
    *(const char **)&g_ns_root = NSROOT;

    /* 1. publish */
    if (wubu_ns_publish_effect() != 0) FAIL("publish failed");
    char buf[2048];
    if (!read_file(NSROOT "/effect/list", buf, sizeof buf)) FAIL("no /n/effect/list");
    if (!read_file(NSROOT "/effect/stack", buf, sizeof buf)) FAIL("no /n/effect/stack");

    /* 2. the list names the effects */
    if (!strstr(buf, "tint") || !strstr(buf, "scanline")) {
        /* buf now holds the stack, so re-read the list */
        if (!read_file(NSROOT "/effect/list", buf, sizeof buf)) FAIL("re-read list");
    }
    if (!read_file(NSROOT "/effect/list", buf, sizeof buf)) FAIL("re-read list");
    if (!strstr(buf, "tint") || !strstr(buf, "vignette") ||
        !strstr(buf, "glow") || !strstr(buf, "scanline"))
        FAIL("list does not name every effect");

    /* 3. a payload adds to the live stack, and refresh republishes it */
    if (wubu_ns_effect_parse("tint 50 ff0000") != 0) FAIL("parse tint");
    const WubuEffectStack *s = wubu_ns_effect_stack();
    if (!s) FAIL("no live stack");
    if (wubu_effect_stack_count(s) != 1) FAIL("count after parse");
    const WubuEffect *e = wubu_effect_stack_at(s, 0);
    if (!e) FAIL("no entry");
    if (e->kind != WUBU_EFFECT_TINT) FAIL("kind");
    if (e->strength != 50) FAIL("strength");
    if (e->color != 0xFF0000) FAIL("colour");

    if (wubu_ns_effect_refresh() != 0) FAIL("refresh");
    if (!read_file(NSROOT "/effect/stack", buf, sizeof buf)) FAIL("re-read stack");
    if (!strstr(buf, "tint 50 ff0000")) FAIL("stack not republished");

    /* 4. a bad payload is refused and changes nothing */
    if (wubu_ns_effect_parse("bogus 50") == 0) FAIL("unknown name accepted");
    if (wubu_ns_effect_parse("tint") == 0) FAIL("missing strength accepted");
    if (wubu_effect_stack_count(s) != 1) FAIL("a bad payload changed the stack");

    /* strength is clamped, not wrapped */
    if (wubu_ns_effect_parse("glow 999 00ff00") != 0) FAIL("parse glow");
    e = wubu_effect_stack_at(s, 1);
    if (!e || e->strength != 100) FAIL("strength not clamped to 100");

    /* 5. clear empties it */
    if (wubu_ns_effect_parse("clear") != 0) FAIL("parse clear");
    if (wubu_effect_stack_count(s) != 0) FAIL("clear did not empty the stack");
    if (wubu_ns_effect_refresh() != 0) FAIL("refresh after clear");
    if (!read_file(NSROOT "/effect/stack", buf, sizeof buf)) FAIL("re-read stack 2");
    if (!strstr(buf, "(none)")) FAIL("cleared stack not republished");

    /* 6. the stack really changes pixels -- the engine is linked, not stubbed */
    if (wubu_ns_effect_parse("tint 50 ffffff") != 0) FAIL("parse tint 2");
    {
        uint32_t px[1] = { 0x00808080 };
        wubu_effect_apply(wubu_ns_effect_stack(), px, 1, 1);
        if (px[0] != 0x00BFBFBF) FAIL("the live stack does not affect pixels");
    }

    printf("=== wubu_ns_effect_test: all checks passed ===\n");
    return 0;
}
