/*
 * wubu_palette_test.c -- the wallpaper-seed extractor against known images.
 *
 * Each case is a synthetic image whose answer is known BY CONSTRUCTION, so a
 * wrong extractor cannot hide behind "it depends on the photo".
 */
#include "../runtime/wubu_palette.h"

#include <stdio.h>
#include <string.h>

static int g_checks, g_fails;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

static uint32_t rgb(uint32_t r, uint32_t g, uint32_t b) {
    return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

/* Fill a w*h buffer with a flat colour. */
static void fill(uint32_t *px, int w, int h, uint32_t c) {
    for (int i = 0; i < w * h; i++) px[i] = c;
}

/* Flat colour: no hue, so no accent can be claimed. */
static void test_flat_has_no_accent(void) {
    enum { W = 32, H = 32 };
    uint32_t px[W * H];
    fill(px, W, H, rgb(200, 30, 30));

    WubuPalette p;
    memset(&p, 0, sizeof p);
    bool ok = wubu_palette_extract(px, W, H, 1, &p);
    CHECK(ok, "a flat COLOURED image still has a hue");
    CHECK(p.base == rgb(200, 30, 30), "base is the flat colour, averaged exactly");
    CHECK(p.accent == p.base, "a single hue is its own accent");
    CHECK(p.vibrant == 1, "vibrant flag is set for a real hue");
}

static void test_grey_is_degenerate(void) {
    enum { W = 16, H = 16 };
    uint32_t px[W * H];
    fill(px, W, H, rgb(128, 128, 128));

    WubuPalette p;
    memset(&p, 0, sizeof p);
    bool ok = wubu_palette_extract(px, W, H, 1, &p);
    CHECK(!ok, "a grey image is degenerate");
    CHECK(p.accent == p.base, "grey accent falls back to base");
}

/* Mostly a dark blue field with a bright yellow block: the yellow is the
 * accent, the blue is the base. */
static void test_accent_from_a_minority_block(void) {
    enum { W = 64, H = 64 };
    uint32_t px[W * H];
    fill(px, W, H, rgb(20, 30, 90));            /* ~90% of the image */

    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 20; x++)
            px[y * W + x] = rgb(240, 200, 40);  /* ~10% */

    WubuPalette p;
    memset(&p, 0, sizeof p);
    bool ok = wubu_palette_extract(px, W, H, 1, &p);
    CHECK(ok, "a vivid minority yields an accent");
    CHECK(p.vibrant == 1, "vibrant flag is set");
    CHECK(p.base == rgb(20, 30, 90), "base is the dominant field");
    CHECK(p.accent == rgb(240, 200, 40), "accent is the vivid block");
}

/* A single vivid pixel must NOT restyle the desktop: presence matters. */
static void test_one_vivid_pixel_is_ignored(void) {
    enum { W = 64, H = 64 };
    uint32_t px[W * H];
    fill(px, W, H, rgb(40, 40, 40));
    px[0] = rgb(255, 0, 255);                   /* one magenta pixel */

    WubuPalette p;
    memset(&p, 0, sizeof p);
    bool ok = wubu_palette_extract(px, W, H, 1, &p);
    CHECK(!ok, "one vivid pixel is below the presence threshold");
}

/* Stepping must not change the answer on a flat image. */
static void test_step_is_stable(void) {
    enum { W = 64, H = 64 };
    uint32_t px[W * H];
    fill(px, W, H, rgb(10, 120, 60));
    px[5 * W + 5] = rgb(250, 240, 30);
    for (int i = 0; i < W * H; i += 7) px[i] = rgb(250, 240, 30);

    WubuPalette a, b;
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    wubu_palette_extract(px, W, H, 1, &a);
    wubu_palette_extract(px, W, H, 4, &b);
    CHECK(a.accent == b.accent, "accent is stable across sampling steps");
}

static void test_degenerate_inputs(void) {
    WubuPalette p;
    CHECK(!wubu_palette_extract(NULL, 4, 4, 1, &p), "NULL pixels refused");
    uint32_t px[16];
    fill(px, 4, 4, rgb(1, 2, 3));
    CHECK(!wubu_palette_extract(px, 0, 4, 1, &p), "zero width refused");
    CHECK(!wubu_palette_extract(px, 4, 4, 0, &p), "zero step refused");
}

static void test_shade(void) {
    CHECK(wubu_palette_shade(rgb(128, 128, 128), 0) == rgb(128, 128, 128),
          "shade 0 is identity");
    CHECK(wubu_palette_shade(rgb(0, 0, 0), 100) == rgb(255, 255, 255),
          "shade +100 reaches white");
    CHECK(wubu_palette_shade(rgb(255, 255, 255), -100) == rgb(0, 0, 0),
          "shade -100 reaches black");
    /* Clamped, not wrapped. */
    CHECK(wubu_palette_shade(rgb(200, 200, 200), 500) == rgb(255, 255, 255),
          "shade clamps at +100");
    CHECK(wubu_palette_shade(rgb(200, 200, 200), -500) == rgb(0, 0, 0),
          "shade clamps at -100");
}

int main(void) {
    printf("=== wubu palette (wallpaper seed) ===\n");
    test_flat_has_no_accent();
    test_grey_is_degenerate();
    test_accent_from_a_minority_block();
    test_one_vivid_pixel_is_ignored();
    test_step_is_stable();
    test_degenerate_inputs();
    test_shade();

    if (g_fails == 0) {
        printf("=== palette: %d checks, 0 failed ===\n", g_checks);
        return 0;
    }
    printf("=== palette: %d checks, %d failed ===\n", g_checks, g_fails);
    return 1;
}
