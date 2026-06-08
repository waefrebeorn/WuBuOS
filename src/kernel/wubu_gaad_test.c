/*
 * wubu_gaad_test.c — GAAD: Golden Aspect Adaptive Decomposition
 *
 * Cell 393: Tests for the universal resolution translator.
 */
#include "wubu_gaad.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int pass = 0, fail = 0;

#define TEST(name) printf("  TEST Cell393: %-55s", name)
#define PASS() do { pass++; printf("✅\n"); } while(0)
#define FAIL(msg) do { fail++; printf("❌ %s\n", msg); } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ── Golden Subdivision ─────────────────────────────────────────── */

static void test_decompose_640x480(void) {
    TEST("decompose 640×480 (TempleOS resolution)");
    WubuGaadDecomp d;
    wubu_gaad_decompose(640, 480, 4, &d);
    CHECK(d.n_regions > 0, "should produce regions");
    CHECK(d.frame_w == 640, "frame_w should be 640");
    CHECK(d.frame_h == 480, "frame_h should be 480");
    /* First region should be a square 480×480 */
    CHECK(d.regions[0].w == 480, "first region should be 480 wide");
    CHECK(d.regions[0].h == 480, "first region should be 480 tall");
    CHECK(d.regions[0].kind == WUBU_GAAD_SQUARE, "first should be square");
    PASS();
}

static void test_decompose_1920x1080(void) {
    TEST("decompose 1920×1080 (modern resolution)");
    WubuGaadDecomp d;
    wubu_gaad_decompose(1920, 1080, 4, &d);
    CHECK(d.n_regions > 0, "should produce regions");
    /* First: square 1080×1080, then 840×1080 recursively */
    CHECK(d.regions[0].w == 1080, "first square should be 1080 wide");
    CHECK(d.regions[0].h == 1080, "first square should be 1080 tall");
    PASS();
}

static void test_decompose_square(void) {
    TEST("decompose 512×512 (square = single region)");
    WubuGaadDecomp d;
    wubu_gaad_decompose(512, 512, 4, &d);
    CHECK(d.n_regions == 1, "square should be single region");
    CHECK(d.regions[0].kind == WUBU_GAAD_SQUARE, "should be square kind");
    PASS();
}

static void test_decompose_golden_rect(void) {
    TEST("decompose golden rectangle (φ:1)");
    int w = 1618, h = 1000;  /* Approximately φ:1 */
    WubuGaadDecomp d;
    wubu_gaad_decompose(w, h, 4, &d);
    CHECK(d.n_regions >= 2, "golden rect should subdivide into 2+");
    CHECK(d.regions[0].w == 1000, "first square should be 1000×1000");
    PASS();
}

static void test_decompose_portrait(void) {
    TEST("decompose portrait 1080×1920");
    WubuGaadDecomp d;
    wubu_gaad_decompose(1080, 1920, 4, &d);
    CHECK(d.n_regions > 0, "should produce regions");
    /* Portrait: first square is 1080×1080, then 1080×840 below */
    CHECK(d.regions[0].w == 1080, "first square should be 1080 wide");
    PASS();
}

/* ── Snap Finding ───────────────────────────────────────────────── */

static void test_snap_center(void) {
    TEST("snap: window at center finds nearest region");
    WubuGaadDecomp d;
    wubu_gaad_decompose(1920, 1080, 4, &d);
    int idx = wubu_gaad_find_snap(&d, 800, 400, 320, 240, 500);
    CHECK(idx >= 0, "should find a snap target");
    PASS();
}

static void test_snap_too_far(void) {
    TEST("snap: window too far returns -1");
    WubuGaadDecomp d;
    wubu_gaad_decompose(1920, 1080, 4, &d);
    int idx = wubu_gaad_find_snap(&d, 5000, 5000, 100, 100, 10);
    CHECK(idx == -1, "should return -1 when too far");
    PASS();
}

/* ── Feng Shui ──────────────────────────────────────────────────── */

static void test_feng_shui_build(void) {
    TEST("feng shui: build 1920×1080 cardinal mirrors");
    WubuFengShui fs;
    wubu_gaad_feng_shui_build(1920, 1080, &fs);
    /* North regions should be at y=0 */
    CHECK(fs.north[0].y == 0, "north[0] should be at y=0");
    /* All regions should be snap targets */
    CHECK(fs.north[0].is_snap_target, "north should be snap target");
    CHECK(fs.center.is_snap_target, "center should be snap target");
    /* Cardinal directions */
    CHECK(fs.north[0].cardinal == 0, "north cardinal = 0");
    CHECK(fs.east[0].cardinal == 1, "east cardinal = 1");
    CHECK(fs.south[0].cardinal == 2, "south cardinal = 2");
    CHECK(fs.west[0].cardinal == 3, "west cardinal = 3");
    CHECK(fs.center.cardinal == -1, "center cardinal = -1");
    PASS();
}

static void test_feng_shui_snap(void) {
    TEST("feng shui: snap window to nearest region");
    WubuFengShui fs;
    wubu_gaad_feng_shui_build(1920, 1080, &fs);
    /* Window at top-left should snap to north or west */
    int ox, oy, ow, oh;
    bool snapped = wubu_gaad_feng_shui_snap(&fs, 10, 10, 400, 300, 500,
                                             &ox, &oy, &ow, &oh);
    CHECK(snapped, "should snap to a region");
    PASS();
}

static void test_feng_shui_asymmetry(void) {
    TEST("feng shui: N/S mirror asymmetry (feng shui)");
    WubuFengShui fs;
    wubu_gaad_feng_shui_build(1920, 1080, &fs);
    /* North[0] should be wider than south[0] (φ² vs 1) */
    CHECK(fs.north[0].w > fs.south[0].w,
          "north[0] should be wider than south[0] (commanding position)");
    PASS();
}

/* ── Resolution Translation ─────────────────────────────────────── */

static void test_translate_templeos_to_hd(void) {
    TEST("translate: TempleOS 640×480 → 1920×1080");
    WubuGaadTranslate t;
    wubu_gaad_translate_init(640, 480, 1920, 1080, 3, &t);
    CHECK(t.src_decomp.n_regions > 0, "source should have regions");
    CHECK(t.dst_decomp.n_regions > 0, "target should have regions");

    /* Top-left corner should map exactly */
    int dx, dy;
    wubu_gaad_translate_pixel(&t, 0, 0, &dx, &dy);
    CHECK(dx == 0 && dy == 0, "origin should map to origin");

    /* A pixel in the center of the first GAAD square.
     * Source square: 480×480 at (0,0). Target square: 1080×1080.
     * Pixel (240,240) is center of source square → (540,540) in target */
    wubu_gaad_translate_pixel(&t, 240, 240, &dx, &dy);
    CHECK(dx == 540 && dy == 540, "center of first square maps to center of target square");

    /* Region correspondence preserves GAAD geometry,
     * not simple linear scaling. This IS the feature. */
    PASS();
}

static void test_translate_inverse(void) {
    TEST("translate: inverse round-trip");
    WubuGaadTranslate t;
    wubu_gaad_translate_init(640, 480, 1920, 1080, 3, &t);

    /* Forward then inverse should approximately recover */
    int dx, dy, sx, sy;
    wubu_gaad_translate_pixel(&t, 100, 100, &dx, &dy);
    wubu_gaad_translate_inverse(&t, dx, dy, &sx, &sy);
    CHECK(abs(sx - 100) < 20, "round-trip x should be close (±20)");
    CHECK(abs(sy - 100) < 20, "round-trip y should be close (±20)");
    PASS();
}

static void test_translate_same_res(void) {
    TEST("translate: same resolution = identity");
    WubuGaadTranslate t;
    wubu_gaad_translate_init(1920, 1080, 1920, 1080, 3, &t);

    int dx, dy;
    wubu_gaad_translate_pixel(&t, 500, 300, &dx, &dy);
    CHECK(abs(dx - 500) < 5, "same res should be near-identity x");
    CHECK(abs(dy - 300) < 5, "same res should be near-identity y");
    PASS();
}

static void test_region_scale(void) {
    TEST("translate: region scale factor");
    WubuGaadTranslate t;
    wubu_gaad_translate_init(640, 480, 1920, 1080, 3, &t);

    double scale = wubu_gaad_region_scale(&t, 0);
    CHECK(scale > 1.0, "scale should be > 1 (target is larger)");
    PASS();
}

/* ── Pure C Math ────────────────────────────────────────────────── */

static void test_isqrt(void) {
    TEST("wubu_isqrt correctness");
    CHECK(wubu_isqrt(0) == 0, "sqrt(0) = 0");
    CHECK(wubu_isqrt(1) == 1, "sqrt(1) = 1");
    CHECK(wubu_isqrt(4) == 2, "sqrt(4) = 2");
    CHECK(wubu_isqrt(9) == 3, "sqrt(9) = 3");
    CHECK(wubu_isqrt(16) == 4, "sqrt(16) = 4");
    CHECK(wubu_isqrt(100) == 10, "sqrt(100) = 10");
    CHECK(wubu_isqrt(144) == 12, "sqrt(144) = 12");
    PASS();
}

static void test_phi_pow(void) {
    TEST("phi^n via recurrence");
    CHECK(wubu_phi_pow(0) > 0.999 && wubu_phi_pow(0) < 1.001,
          "φ^0 = 1");
    CHECK(wubu_phi_pow(1) > 1.617 && wubu_phi_pow(1) < 1.619,
          "φ^1 ≈ 1.618");
    CHECK(wubu_phi_pow(2) > 2.617 && wubu_phi_pow(2) < 2.619,
          "φ^2 ≈ 2.618");
    CHECK(wubu_phi_pow(3) > 4.235 && wubu_phi_pow(3) < 4.237,
          "φ^3 ≈ 4.236");
    PASS();
}

static void test_clamp(void) {
    TEST("wubu_clamp");
    CHECK(wubu_clamp(5, 0, 10) == 5, "5 in [0,10] = 5");
    CHECK(wubu_clamp(-1, 0, 10) == 0, "-1 clamped to 0");
    CHECK(wubu_clamp(15, 0, 10) == 10, "15 clamped to 10");
    PASS();
}

/* ── Main ───────────────────────────────────────────────────────── */

int main(void) {
    printf("\n── GAAD Golden Subdivision (Cell 393) ──\n\n");
    test_decompose_640x480();
    test_decompose_1920x1080();
    test_decompose_square();
    test_decompose_golden_rect();
    test_decompose_portrait();

    printf("\n── GAAD Snap (Cell 393) ──\n\n");
    test_snap_center();
    test_snap_too_far();

    printf("\n── GAAD Feng Shui (Cell 393) ──\n\n");
    test_feng_shui_build();
    test_feng_shui_snap();
    test_feng_shui_asymmetry();

    printf("\n── GAAD Resolution Translation (Cell 393) ──\n\n");
    test_translate_templeos_to_hd();
    test_translate_inverse();
    test_translate_same_res();
    test_region_scale();

    printf("\n── GAAD Pure C Math (Cell 393) ──\n\n");
    test_isqrt();
    test_phi_pow();
    test_clamp();

    printf("\n══════════════════════════════════════════════════\n");
    printf("  Results: %d/%d passed, %d failed\n",
           pass, pass + fail, fail);
    printf("══════════════════════════════════════════════════\n");
    return fail > 0 ? 1 : 0;
}
