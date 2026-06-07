/*
 * gui_dbuf_test.c — Test Suite for Double-Buffered GUI Renderer
 *
 * Cell 101: Tests double buffering, drawing primitives,
 * Win98 borders, dirty rect tracking, and flip.
 */

#include "gui_dbuf.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name) printf("  TEST %-45s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

static int count_pixels(gui_dbuf_t *db, uint32_t color) {
    int count = 0;
    for (int i = 0; i < db->width * db->height; i++)
        if (db->back[i] == color) count++;
    return count;
}

/* ── Lifecycle Tests ───────────────────────────────────────── */

static void test_init(void) {
    TEST("gui_dbuf init");
    gui_dbuf_t db;
    int rc = gui_dbuf_init(&db, 320, 200);
    CHECK(rc == 0, "init should succeed");
    CHECK(db.back != NULL, "back buffer should be allocated");
    CHECK(db.width == 320, "width should match");
    CHECK(db.height == 200, "height should match");
    gui_dbuf_shutdown(&db);
    PASS();
}

static void test_clear(void) {
    TEST("clear fills entire buffer");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 64, 48);
    gui_dbuf_clear(&db, 0x00FF0000);
    int red = count_pixels(&db, 0x00FF0000);
    CHECK(red == 64 * 48, "all pixels should be red");
    CHECK(db.dirty_count == 1, "should mark full screen dirty");
    gui_dbuf_shutdown(&db);
    PASS();
}

/* ── Drawing Primitive Tests ───────────────────────────────── */

static void test_pixel(void) {
    TEST("pixel sets color");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 320, 200);
    gui_dbuf_clear(&db, 0x00000000);
    gui_dbuf_pixel(&db, 10, 20, 0x00FF0000);
    CHECK(db.back[20 * 320 + 10] == 0x00FF0000, "pixel should be red");
    gui_dbuf_shutdown(&db);
    PASS();
}

static void test_pixel_clipping(void) {
    TEST("pixel clips to bounds");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 100, 100);
    gui_dbuf_clear(&db, 0x00000000);
    gui_dbuf_pixel(&db, -1, 50, 0x00FF0000);   /* negative x */
    gui_dbuf_pixel(&db, 50, -1, 0x00FF0000);   /* negative y */
    gui_dbuf_pixel(&db, 100, 50, 0x00FF0000);  /* x at width */
    gui_dbuf_pixel(&db, 50, 100, 0x00FF0000);  /* y at height */
    int red = count_pixels(&db, 0x00FF0000);
    CHECK(red == 0, "no out-of-bounds pixels should be set");
    gui_dbuf_shutdown(&db);
    PASS();
}

static void test_fill_rect(void) {
    TEST("fill_rect counts pixels correctly");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 100, 100);
    gui_dbuf_clear(&db, 0x00000000);
    gui_dbuf_fill_rect(&db, 10, 20, 30, 40, 0x0000FF00);
    int green = count_pixels(&db, 0x0000FF00);
    CHECK(green == 30 * 40, "should fill exactly w*h pixels");
    gui_dbuf_shutdown(&db);
    PASS();
}

/* ── Dirty Rectangle Tests ─────────────────────────────────── */

static void test_dirty_tracking(void) {
    TEST("dirty rects tracked per draw call");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 320, 200);
    gui_dbuf_clear(&db, 0x00C0C0C0);
    gui_dbuf_clear_dirty(&db);  /* reset after clear */

    gui_dbuf_button(&db, 10, 10, 80, 25, "OK", 0);
    CHECK(db.dirty_count == 1, "button should mark 1 dirty rect");
    CHECK(db.dirty[0].x == 10, "dirty x should match button x");
    CHECK(db.dirty[0].y == 10, "dirty y should match button y");
    CHECK(db.dirty[0].w == 80, "dirty w should match button w");
    CHECK(db.dirty[0].h == 25, "dirty h should match button h");

    gui_dbuf_shutdown(&db);
    PASS();
}

static void test_dirty_max(void) {
    TEST("dirty rects cap at max");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 320, 200);
    gui_dbuf_clear(&db, 0x00000000);
    gui_dbuf_clear_dirty(&db);

    for (int i = 0; i < GUI_DBUF_DIRTY_MAX + 10; i++) {
        gui_dbuf_mark_dirty(&db, i * 2, 0, 1, 1);
    }
    CHECK(db.dirty_count == GUI_DBUF_DIRTY_MAX, "should cap at max");

    gui_dbuf_shutdown(&db);
    PASS();
}

static void test_dirty_clear(void) {
    TEST("clear_dirty resets count");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 320, 200);
    gui_dbuf_clear(&db, 0x00000000);
    gui_dbuf_clear_dirty(&db);
    gui_dbuf_mark_dirty(&db, 0, 0, 10, 10);
    gui_dbuf_mark_dirty(&db, 20, 20, 10, 10);
    CHECK(db.dirty_count == 2, "should have 2 dirty rects");
    gui_dbuf_clear_dirty(&db);
    CHECK(db.dirty_count == 0, "should be 0 after clear");
    gui_dbuf_shutdown(&db);
    PASS();
}

static void test_dirty_clipping(void) {
    TEST("dirty rects clip to screen bounds");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 100, 100);
    gui_dbuf_clear(&db, 0x00000000);
    gui_dbuf_clear_dirty(&db);

    gui_dbuf_mark_dirty(&db, -10, -10, 50, 50);  /* negative origin */
    CHECK(db.dirty_count == 1, "should add 1 clipped rect");
    CHECK(db.dirty[0].x == 0, "x should be clipped to 0");
    CHECK(db.dirty[0].y == 0, "y should be clipped to 0");
    CHECK(db.dirty[0].w == 40, "w should be clipped");

    gui_dbuf_mark_dirty(&db, 90, 90, 50, 50);  /* extends past edge */
    CHECK(db.dirty[1].w == 10, "w should clip to screen width");
    CHECK(db.dirty[1].h == 10, "h should clip to screen height");

    gui_dbuf_shutdown(&db);
    PASS();
}

/* ── Flip Tests ────────────────────────────────────────────── */

static void test_flip(void) {
    TEST("flip counts pixels and clears dirty");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 320, 200);
    gui_dbuf_clear(&db, 0x00000000);
    gui_dbuf_clear_dirty(&db);

    gui_dbuf_fill_rect(&db, 10, 10, 50, 50, 0x00FF0000);
    gui_dbuf_mark_dirty(&db, 10, 10, 50, 50);

    uint32_t pixels = gui_dbuf_flip(&db);
    CHECK(pixels == 50 * 50, "should copy dirty region pixels");
    CHECK(db.dirty_count == 0, "dirty should be cleared after flip");
    CHECK(db.frames == 1, "frame count should be 1");
    CHECK(db.flips == 1, "flip count should be 1");
    CHECK(db.pixels_copied == 50 * 50, "pixels_copied should match");

    gui_dbuf_shutdown(&db);
    PASS();
}

static void test_flip_empty_dirty(void) {
    TEST("flip with no dirty rects copies nothing");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 320, 200);
    gui_dbuf_clear(&db, 0x00FF0000);
    gui_dbuf_clear_dirty(&db);

    uint32_t pixels = gui_dbuf_flip(&db);
    CHECK(pixels == 0, "should copy 0 pixels when no dirty rects");
    CHECK(db.frames == 1, "frame should still increment");

    gui_dbuf_shutdown(&db);
    PASS();
}

/* ── Win98 Widget Tests ────────────────────────────────────── */

static void test_button_raised(void) {
    TEST("button drawn with raised border");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 320, 200);
    gui_dbuf_clear(&db, 0x00C0C0C0);

    gui_dbuf_button(&db, 50, 50, 80, 25, "OK", 0);
    /* Check that the button area has non-background pixels (border) */
    int changed = 0;
    for (int y = 50; y < 75; y++)
        for (int x = 50; x < 130; x++)
            if (db.back[y * 320 + x] != 0x00C0C0C0) changed++;
    CHECK(changed > 0, "button should draw border pixels");

    /* Verify background is Win98 gray */
    CHECK(db.back[62 * 320 + 90] == 0x00C0C0C0, "interior should be gray");

    gui_dbuf_shutdown(&db);
    PASS();
}

static void test_button_sunken(void) {
    TEST("button drawn with sunken border when pressed");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 320, 200);
    gui_dbuf_clear(&db, 0x00C0C0C0);

    gui_dbuf_button(&db, 50, 50, 80, 25, "OK", 1);  /* pressed = 1 */

    /* Sunken border should have dark pixels at top-left */
    int border_pixels = 0;
    for (int y = 50; y < 53; y++)
        for (int x = 50; x < 53; x++)
            if (db.back[y * 320 + x] == 0x00808080) border_pixels++;
    CHECK(border_pixels > 0, "sunken border should have dark top-left");

    gui_dbuf_shutdown(&db);
    PASS();
}

static void test_window(void) {
    TEST("window drawn with title bar and content area");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 320, 200);
    gui_dbuf_clear(&db, 0x00808080);  /* desktop gray */

    gui_dbuf_window(&db, 30, 30, 200, 120, "Test", 1);

    /* Title bar should be blue (active) */
    int title_pixels = 0;
    for (int y = 34; y < 52; y++)
        for (int x = 34; x < 226; x++)
            if (db.back[y * 320 + x] == 0x00000080) title_pixels++;
    CHECK(title_pixels > 0, "title bar should be blue");

    /* Content area should be gray */
    CHECK(db.back[100 * 320 + 100] == 0x00C0C0C0, "content area should be gray");

    gui_dbuf_shutdown(&db);
    PASS();
}

static void test_window_inactive(void) {
    TEST("inactive window has gray title bar");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 320, 200);
    gui_dbuf_clear(&db, 0x00808080);

    gui_dbuf_window(&db, 30, 30, 200, 120, "Test", 0);

    int gray_title = 0;
    for (int y = 34; y < 52; y++)
        for (int x = 34; x < 226; x++)
            if (db.back[y * 320 + x] == 0x00808080) gray_title++;
    CHECK(gray_title > 0, "inactive title bar should be gray");

    gui_dbuf_shutdown(&db);
    PASS();
}

/* ── Query Tests ───────────────────────────────────────────── */

static void test_dimensions(void) {
    TEST("dimension queries");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 640, 480);
    CHECK(gui_dbuf_width(&db) == 640, "width");
    CHECK(gui_dbuf_height(&db) == 480, "height");
    CHECK(gui_dbuf_width(NULL) == 0, "NULL width");
    CHECK(gui_dbuf_height(NULL) == 0, "NULL height");
    gui_dbuf_shutdown(&db);
    PASS();
}

static void test_frame_count(void) {
    TEST("frame count increments on flip");
    gui_dbuf_t db;
    gui_dbuf_init(&db, 100, 100);
    gui_dbuf_clear(&db, 0x00000000);

    gui_dbuf_flip(&db);
    gui_dbuf_flip(&db);
    gui_dbuf_flip(&db);
    CHECK(gui_dbuf_frames(&db) == 3, "should be 3 frames");
    CHECK(gui_dbuf_frames(NULL) == 0, "NULL frames");

    gui_dbuf_shutdown(&db);
    PASS();
}

/* ── Main ──────────────────────────────────────────────────── */

int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  WuBuOS Double-Buffered GUI Test Suite             ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    test_init();
    test_clear();
    test_pixel();
    test_pixel_clipping();
    test_fill_rect();
    test_dirty_tracking();
    test_dirty_max();
    test_dirty_clear();
    test_dirty_clipping();
    test_flip();
    test_flip_empty_dirty();
    test_button_raised();
    test_button_sunken();
    test_window();
    test_window_inactive();
    test_dimensions();
    test_frame_count();

    printf("\n══════════════════════════════════════════════════\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("══════════════════════════════════════════════════\n");

    return g_fail > 0 ? 1 : 0;
}
