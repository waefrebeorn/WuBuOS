/*
 * wubu_rim_test.c -- the rim loop and the islands that travel around it.
 */
#include "../runtime/wubu_rim.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_checks, g_fails;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

static void settle(WubuRim *r, int steps) {
    WubuSpringWorld w = wubu_spring_ooze();
    for (int i = 0; i < steps; i++) wubu_rim_step(r, &w);
}

/* The loop must close: t=0 and t=1 are the same point, and the corners sit
 * at the leg boundaries (the legs are NOT equal unless w == h). */
static void test_loop_is_closed(void) {
    WubuRim r;
    CHECK(wubu_rim_init(&r, 8, 0.0f, 0.0f, 400.0f, 200.0f), "rim created");

    float per = wubu_rim_perimeter(&r);
    float t_tr = 400.0f / per;              /* end of the top leg      */
    float t_br = (400.0f + 200.0f) / per;   /* end of the right leg    */
    float t_bl = (2.0f * 400.0f + 200.0f) / per;  /* end of the bottom leg */

    float x0, y0, x1, y1;
    wubu_rim_position(&r, 0.0f, &x0, &y0);
    wubu_rim_position(&r, 1.0f, &x1, &y1);
    CHECK(x0 == x1 && y0 == y1, "t=0 and t=1 are the same point");
    CHECK(x0 == 0.0f && y0 == 0.0f, "the loop starts at the top-left");

    float x, y;
    wubu_rim_position(&r, t_tr, &x, &y);
    CHECK(fabsf(x - 400.0f) < 0.001f && fabsf(y - 0.0f) < 0.001f,
          "the top leg ends at the top-right corner");
    wubu_rim_position(&r, t_br, &x, &y);
    CHECK(fabsf(x - 400.0f) < 0.001f && fabsf(y - 200.0f) < 0.001f,
          "the right leg ends at the bottom-right corner");
    wubu_rim_position(&r, t_bl, &x, &y);
    CHECK(fabsf(x - 0.0f) < 0.001f && fabsf(y - 200.0f) < 0.001f,
          "the bottom leg ends at the bottom-left corner");
    wubu_rim_free(&r);
}

/* The nearest-t map is the inverse of position on the rim itself. */
static void test_nearest_t_round_trips(void) {
    WubuRim r;
    wubu_rim_init(&r, 8, 0.0f, 0.0f, 400.0f, 200.0f);

    /* t=0 and t=1 are the SAME point, so the inverse map returns 0 for both --
     * check the interior of the loop, then the seam separately. */
    for (int k = 0; k < 8; k++) {
        float t = (float)k / 8.0f;
        float x, y;
        wubu_rim_position(&r, t, &x, &y);
        float back = wubu_rim_nearest_t(&r, x, y);
        CHECK(fabsf(back - t) < 0.02f, "nearest_t inverts position on the rim");
    }
    {
        float x, y;
        wubu_rim_position(&r, 1.0f, &x, &y);
        float back = wubu_rim_nearest_t(&r, x, y);
        CHECK(back < 0.02f || back > 0.98f,
              "the seam point maps to either end of the loop");
    }

    /* A point clearly inside the rect snaps to the nearest edge: (200,10) is
     * 200px along the top leg of a 1200px perimeter. */
    float t = wubu_rim_nearest_t(&r, 200.0f, 10.0f);
    CHECK(fabsf(t - (200.0f / 1200.0f)) < 0.02f,
          "an interior point near the top maps to its leg distance");
    wubu_rim_free(&r);
}

static void test_island_rests_and_settles(void) {
    WubuRim r;
    wubu_rim_init(&r, 8, 0.0f, 0.0f, 400.0f, 200.0f);
    int i = wubu_rim_add(&r, 400.0f / 1200.0f, 10.0f);   /* top-right corner */
    CHECK(i == 0, "island added");
    settle(&r, 300);

    CHECK(fabsf(r.islands[0].blob.x - 400.0f) < 2.0f, "settles at the corner x");
    CHECK(fabsf(r.islands[0].blob.y - 0.0f)   < 2.0f, "settles at the corner y");
    wubu_rim_free(&r);
}

/* Dragging around the rim must move the island's HOME, so it stays where it is
 * put instead of springing back to its old spot. */
static void test_drag_travels_the_rim(void) {
    WubuRim r;
    wubu_rim_init(&r, 8, 0.0f, 0.0f, 400.0f, 200.0f);
    wubu_rim_add(&r, 0.0f, 10.0f);                /* top-left */
    settle(&r, 200);

    wubu_rim_drag(&r, 0, 400.0f, 100.0f);         /* drag down the right edge */
    wubu_rim_drop(&r, 0);
    settle(&r, 400);

    CHECK(fabsf(r.islands[0].t - (500.0f / 1200.0f)) < 0.03f,
          "the island travelled down the right leg");
    CHECK(fabsf(r.islands[0].blob.x - 400.0f) < 3.0f, "and rests on the right edge");
    CHECK(fabsf(r.islands[0].blob.y - 100.0f) < 3.0f, "at the dragged height");
}

static void test_rect_move_keeps_t(void) {
    WubuRim r;
    wubu_rim_init(&r, 8, 0.0f, 0.0f, 400.0f, 200.0f);
    wubu_rim_add(&r, 0.25f, 10.0f);
    settle(&r, 100);

    /* The window moves and resizes under the island; t stays valid. */
    wubu_rim_set_rect(&r, 50.0f, 60.0f, 800.0f, 400.0f);
    settle(&r, 400);

    float ex, ey;
    wubu_rim_position(&r, 0.25f, &ex, &ey);
    CHECK(fabsf(r.islands[0].t - 0.25f) < 0.001f, "t survives a rect change");
    CHECK(fabsf(r.islands[0].blob.x - ex) < 3.0f, "island follows the new rect");
    CHECK(fabsf(r.islands[0].blob.y - ey) < 3.0f, "and the new corner");
}

static void test_hit_and_visibility(void) {
    WubuRim r;
    wubu_rim_init(&r, 8, 0.0f, 0.0f, 400.0f, 200.0f);
    wubu_rim_add(&r, 400.0f / 1200.0f, 12.0f);
    settle(&r, 100);

    CHECK(wubu_rim_hit(&r, 400.0f, 0.0f) == 0, "hit at the island");
    CHECK(wubu_rim_hit(&r, 0.0f, 0.0f) == -1, "miss elsewhere");

    wubu_rim_show(&r, 0, false);
    CHECK(wubu_rim_hit(&r, 400.0f, 0.0f) == -1, "a hidden island cannot be hit");
    CHECK(!r.islands[0].visible, "hidden flag recorded");
}

static void test_long_run_stays_finite(void) {
    WubuRim r;
    wubu_rim_init(&r, 8, 0.0f, 0.0f, 400.0f, 200.0f);
    for (int k = 0; k < 6; k++)
        wubu_rim_add(&r, (float)k / 6.0f, 8.0f);

    WubuSpringWorld w = wubu_spring_ooze();
    w.damping = 0.0f;                     /* hostile: no energy loss */
    for (int s = 0; s < 3000; s++) wubu_rim_step(&r, &w);

    int ok = 1;
    for (int i = 0; i < r.n; i++) {
        const WubuBlob *b = &r.islands[i].blob;
        if (!isfinite(b->x) || !isfinite(b->y)) ok = 0;
    }
    CHECK(ok, "six islands stay finite over 3000 undamped steps");

    /* Every island must still be ON the rim (within its radius). */
    int on_rim = 1;
    for (int i = 0; i < r.n; i++) {
        float ex, ey;
        wubu_rim_position(&r, r.islands[i].t, &ex, &ey);
        if (wubu_spring_dist(&r.islands[i].blob, &(WubuBlob){ .x = ex, .y = ey }) > 4.0f)
            on_rim = 0;
    }
    CHECK(on_rim, "every island stays on the rim");
}

static void test_bad_input(void) {
    WubuRim r;
    CHECK(!wubu_rim_init(NULL, 4, 0, 0, 10, 10), "NULL rim refused");
    CHECK(!wubu_rim_init(&r, 0, 0, 0, 10, 10), "zero capacity refused");
    CHECK(!wubu_rim_init(&r, 4, 0, 0, 0, 10), "zero width refused");
    CHECK(wubu_rim_init(&r, 2, 0, 0, 100, 100), "rim created");
    CHECK(wubu_rim_add(&r, 0.5f, 5.0f) == 0, "first add");
    CHECK(wubu_rim_add(&r, 0.6f, 5.0f) == 1, "second add");
    CHECK(wubu_rim_add(&r, 0.7f, 5.0f) == -1, "beyond capacity refused");
    wubu_rim_free(&r);
}

int main(void) {
    printf("=== wubu rim islands ===\n");
    test_loop_is_closed();
    test_nearest_t_round_trips();
    test_island_rests_and_settles();
    test_drag_travels_the_rim();
    test_rect_move_keeps_t();
    test_hit_and_visibility();
    test_long_run_stays_finite();
    test_bad_input();

    if (g_fails == 0) {
        printf("=== rim: %d checks, 0 failed ===\n", g_checks);
        return 0;
    }
    printf("=== rim: %d checks, %d failed ===\n", g_checks, g_fails);
    return 1;
}
