/*
 * wubu_effect_test.c -- the effect engine's gate.
 *
 * The passes are the part that must be exactly right: they run per pixel in
 * the draw path, so each test pins a known input to a known output rather than
 * just asserting "something changed".
 */
#include "../runtime/wubu_effect.h"
#include "gui_dbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks, g_fails;

#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

static uint32_t rgb(uint32_t r, uint32_t g, uint32_t b) {
    return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

static void test_registry(void) {
    CHECK(wubu_effect_kind_count() == WUBU_EFFECT_COUNT, "kind count");
    CHECK(strcmp(wubu_effect_name(WUBU_EFFECT_TINT), "tint") == 0, "tint name");
    CHECK(strcmp(wubu_effect_name(WUBU_EFFECT_VIGNETTE), "vignette") == 0, "vignette name");
    CHECK(strcmp(wubu_effect_name(WUBU_EFFECT_GLOW), "glow") == 0, "glow name");
    CHECK(strcmp(wubu_effect_name(WUBU_EFFECT_SCANLINE), "scanline") == 0, "scanline name");
    CHECK(wubu_effect_name(WUBU_EFFECT_NONE) == NULL, "none has no effect name");
    CHECK(wubu_effect_name((WubuEffectKind)999) == NULL, "out-of-range name is NULL");
    CHECK(strcmp(wubu_effect_name_at(1), "tint") == 0, "name_at 1");
    CHECK(wubu_effect_name_at(0) == NULL, "name_at 0 is NULL");
    CHECK(wubu_effect_kind_at(3) == WUBU_EFFECT_GLOW, "kind_at 3");
    CHECK(wubu_effect_kind_at(-1) == WUBU_EFFECT_NONE, "kind_at -1 is NONE");
}

static void test_stack_editing(void) {
    WubuEffectStack *s = wubu_effect_stack_new();
    CHECK(s != NULL, "stack allocates");
    if (!s) return;

    CHECK(wubu_effect_stack_count(s) == 0, "new stack is empty");
    CHECK(wubu_effect_stack_at(s, 0) == NULL, "empty stack has no element 0");

    CHECK(wubu_effect_stack_add(s, WUBU_EFFECT_TINT, 0x112233, 40), "add works");
    CHECK(wubu_effect_stack_count(s) == 1, "count after add");
    const WubuEffect *e = wubu_effect_stack_at(s, 0);
    CHECK(e != NULL, "at(0)");
    if (e) {
        CHECK(e->kind == WUBU_EFFECT_TINT, "kind stored");
        CHECK(e->color == 0x112233, "colour stored");
        CHECK(e->strength == 40, "strength stored");
    }

    /* Rejections. */
    CHECK(!wubu_effect_stack_add(s, WUBU_EFFECT_NONE, 0, 10), "NONE is rejected");
    CHECK(!wubu_effect_stack_add(s, (WubuEffectKind)999, 0, 10), "bad kind rejected");
    CHECK(wubu_effect_stack_count(s) == 1, "rejected adds do not grow the stack");

    /* Clamping. */
    CHECK(wubu_effect_stack_add(s, WUBU_EFFECT_GLOW, 0xFFFFFF00, 999), "add clamps");
    e = wubu_effect_stack_at(s, 1);
    CHECK(e && e->strength == 100, "strength clamped high");
    CHECK(e && e->color == 0xFFFF00, "colour masked to 24 bits");
    CHECK(wubu_effect_stack_add(s, WUBU_EFFECT_GLOW, 0, -5), "add clamps low");
    e = wubu_effect_stack_at(s, 2);
    CHECK(e && e->strength == 0, "strength clamped low");

    /* Removal preserves order. */
    CHECK(wubu_effect_stack_add(s, WUBU_EFFECT_SCANLINE, 0, 50), "third effect");
    CHECK(wubu_effect_stack_remove(s, 1), "remove middle");
    CHECK(wubu_effect_stack_count(s) == 3, "count after remove");
    e = wubu_effect_stack_at(s, 2);
    CHECK(e && e->kind == WUBU_EFFECT_SCANLINE, "removal preserved order");
    CHECK(!wubu_effect_stack_remove(s, 99), "out-of-range remove rejected");
    CHECK(!wubu_effect_stack_remove(s, -1), "negative remove rejected");

    /* The cap. */
    wubu_effect_stack_clear(s);
    CHECK(wubu_effect_stack_count(s) == 0, "clear empties");
    for (int i = 0; i < 8; i++)
        CHECK(wubu_effect_stack_add(s, WUBU_EFFECT_TINT, 0, 10), "fill to the cap");
    CHECK(!wubu_effect_stack_add(s, WUBU_EFFECT_TINT, 0, 10), "ninth add rejected");
    CHECK(wubu_effect_stack_count(s) == 8, "cap holds at eight");

    wubu_effect_stack_free(s);
}

static void test_passes(void) {
    /* --- tint: a known pixel moves toward the tint by the exact amount. --- */
    {
        uint32_t px[1] = { rgb(128, 128, 128) };
        WubuEffectStack *s = wubu_effect_stack_new();
        wubu_effect_stack_add(s, WUBU_EFFECT_TINT, rgb(255, 255, 255), 50);
        wubu_effect_apply(s, px, 1, 1);
        /* 128 + (255-128)*50/100 = 128 + 63 = 191 */
        CHECK(px[0] == rgb(191, 191, 191), "tint moves 128 toward 255 by half");
        wubu_effect_stack_free(s);
    }

    /* --- tint at 0 strength is a no-op. --- */
    {
        uint32_t px[1] = { rgb(10, 20, 30) };
        WubuEffectStack *s = wubu_effect_stack_new();
        wubu_effect_stack_add(s, WUBU_EFFECT_TINT, rgb(255, 0, 0), 0);
        wubu_effect_apply(s, px, 1, 1);
        CHECK(px[0] == rgb(10, 20, 30), "zero strength is a no-op");
        wubu_effect_stack_free(s);
    }

    /* --- vignette: the centre is untouched, the corner darkens. --- */
    {
        enum { W = 8, H = 8 };
        uint32_t px[W * H];
        for (int i = 0; i < W * H; i++) px[i] = rgb(200, 200, 200);
        WubuEffectStack *s = wubu_effect_stack_new();
        wubu_effect_stack_add(s, WUBU_EFFECT_VIGNETTE, 0, 50);
        wubu_effect_apply(s, px, W, H);
        CHECK(px[(H / 2) * W + (W / 2)] == rgb(200, 200, 200),
              "vignette leaves the centre alone");
        CHECK(px[0] == rgb(100, 100, 100),
              "vignette halves the corner at 50 percent");
        wubu_effect_stack_free(s);
    }

    /* --- glow: black stays black, a bright pixel lifts toward the colour. --- */
    {
        uint32_t dark[1]  = { rgb(0, 0, 0) };
        uint32_t bright[1] = { rgb(200, 200, 200) };
        WubuEffectStack *s = wubu_effect_stack_new();
        wubu_effect_stack_add(s, WUBU_EFFECT_GLOW, rgb(0, 0, 255), 50);
        wubu_effect_apply(s, dark, 1, 1);
        wubu_effect_apply(s, bright, 1, 1);
        CHECK(dark[0] == rgb(0, 0, 0), "glow leaves black alone");
        /* lum=200, k=50*200/255=39, b = 200 + 255*39/100 = 299 -> 255 */
        CHECK(bright[0] == rgb(200, 200, 255), "glow lifts the blue channel");
        wubu_effect_stack_free(s);
    }

    /* --- scanline: row 0 survives, row 1 is dimmed. --- */
    {
        enum { W = 4, H = 2 };
        uint32_t px[W * H];
        for (int i = 0; i < W * H; i++) px[i] = rgb(200, 200, 200);
        WubuEffectStack *s = wubu_effect_stack_new();
        wubu_effect_stack_add(s, WUBU_EFFECT_SCANLINE, 0, 50);
        wubu_effect_apply(s, px, W, H);
        CHECK(px[0] == rgb(200, 200, 200), "scanline keeps row 0");
        CHECK(px[W] == rgb(100, 100, 100), "scanline halves row 1");
        wubu_effect_stack_free(s);
    }

    /* --- an empty or NULL stack changes nothing. --- */
    {
        uint32_t a[4] = { rgb(1, 2, 3), rgb(4, 5, 6), rgb(7, 8, 9), rgb(10, 11, 12) };
        uint32_t b[4];
        memcpy(b, a, sizeof a);
        wubu_effect_apply(NULL, a, 2, 2);
        CHECK(memcmp(a, b, sizeof a) == 0, "NULL stack is a no-op");

        WubuEffectStack *s = wubu_effect_stack_new();
        wubu_effect_apply(s, a, 2, 2);
        CHECK(memcmp(a, b, sizeof a) == 0, "empty stack is a no-op");
        wubu_effect_stack_free(s);
    }

    /* --- order matters, and both orders are deterministic. --- */
    {
        enum { N = 4 };
        uint32_t a[N], b[N], base[N];
        for (int i = 0; i < N; i++) base[i] = rgb(60, 120, 200);

        memcpy(a, base, sizeof a);
        memcpy(b, base, sizeof b);

        WubuEffectStack *s1 = wubu_effect_stack_new();
        wubu_effect_stack_add(s1, WUBU_EFFECT_TINT, rgb(255, 0, 0), 40);
        wubu_effect_stack_add(s1, WUBU_EFFECT_VIGNETTE, 0, 40);
        wubu_effect_apply(s1, a, N, 1);
        wubu_effect_stack_free(s1);

        WubuEffectStack *s2 = wubu_effect_stack_new();
        wubu_effect_stack_add(s2, WUBU_EFFECT_VIGNETTE, 0, 40);
        wubu_effect_stack_add(s2, WUBU_EFFECT_TINT, rgb(255, 0, 0), 40);
        wubu_effect_apply(s2, b, N, 1);
        wubu_effect_stack_free(s2);

        CHECK(memcmp(a, b, sizeof a) != 0, "order changes the result");
        CHECK(memcmp(a, base, sizeof a) != 0, "the stack changed the pixels");
    }

    /* --- degenerate surfaces are refused, not crashed. --- */
    {
        uint32_t px[1] = { rgb(9, 9, 9) };
        WubuEffectStack *s = wubu_effect_stack_new();
        wubu_effect_stack_add(s, WUBU_EFFECT_TINT, 0xFFFFFF, 50);
        wubu_effect_apply(s, px, 0, 0);
        wubu_effect_apply(s, px, 1, 0);
        wubu_effect_apply(s, NULL, 1, 1);
        CHECK(px[0] == rgb(9, 9, 9), "degenerate surfaces are untouched");
        wubu_effect_stack_free(s);
    }
}

static void test_dbuf_integration(void) {
    gui_dbuf_t db;
    CHECK(gui_dbuf_init(&db, 16, 16) == 0, "dbuf init");
    if (db.back == NULL) { CHECK(false, "dbuf has a surface"); return; }

    /* No stack attached: applying is a no-op. */
    uint32_t before = db.back[0];
    gui_dbuf_effect_apply(&db);
    CHECK(db.back[0] == before, "no stack leaves the surface alone");

    /* Attach a stack, apply, and the surface changes. */
    WubuEffectStack *s = wubu_effect_stack_new();
    CHECK(s != NULL, "effect stack allocates");
    if (s) {
        wubu_effect_stack_add(s, WUBU_EFFECT_VIGNETTE, 0, 50);
        for (int i = 0; i < 16 * 16; i++) db.back[i] = rgb(200, 200, 200);
        /* The dbuf owns its stack: hand ours over through the accessor. */
        wubu_effect_stack_free(db.effect);
        db.effect = s;
        gui_dbuf_effect_apply(&db);
        CHECK(db.back[0] == rgb(100, 100, 100),
              "vignette through the dbuf darkens the corner");
        CHECK(db.back[(16 / 2) * 16 + (16 / 2)] == rgb(200, 200, 200),
              "vignette through the dbuf spares the centre");
    }

    /* The lazy accessor creates a usable stack. */
    wubu_effect_stack_clear(gui_dbuf_effect(&db));
    CHECK(wubu_effect_stack_count(gui_dbuf_effect(&db)) == 0,
          "gui_dbuf_effect returns the same stack");

    gui_dbuf_shutdown(&db);
    CHECK(db.effect == NULL, "shutdown releases the stack");
    CHECK(db.back == NULL, "shutdown releases the surface");
}

int main(void) {
    printf("=== effect engine ===\n");
    test_registry();
    test_stack_editing();
    test_passes();
    test_dbuf_integration();
    printf("=== effect: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
