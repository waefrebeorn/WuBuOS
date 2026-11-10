/*
 * wubu_slime_test.c -- the seam's behaviour against known answers.
 *
 * A blob must rest on its rim home, sink when absorbed, come back when
 * released, follow a drag along the rim, stay on screen, and never go NaN.
 */
#include "../runtime/wubu_slime.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_checks, g_fails;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

static void settle(WubuSlime *s, int steps) {
    WubuSpringWorld w = wubu_spring_ooze();
    for (int i = 0; i < steps; i++) wubu_slime_step(s, &w);
}

static int finite_all(const WubuSlime *s) {
    for (int i = 0; i < s->n; i++) {
        const WubuBlob *b = &s->blobs[i].blob;
        if (!isfinite(b->x) || !isfinite(b->y) || !isfinite(b->px) || !isfinite(b->py))
            return 0;
    }
    return 1;
}

static void test_rests_on_its_home(void) {
    WubuSlime s;
    CHECK(wubu_slime_init(&s, 8, 800.0f, 600.0f, 16.0f), "seam created");
    int i = wubu_slime_add(&s, WUBU_EDGE_TOP, 0.25f, 20.0f, 4.0f, 14.0f, 6.0f);
    CHECK(i == 0, "blob added");
    settle(&s, 300);

    /* home_t 0.25 on the top edge of an 800-wide screen, 20px out. */
    CHECK(fabsf(s.blobs[0].blob.x - 200.0f) < 2.0f, "settles at its x home");
    CHECK(fabsf(s.blobs[0].blob.y - 20.0f)  < 2.0f, "settles at its depth");
    CHECK(!s.blobs[0].stored, "starts released");
    wubu_slime_free(&s);
}

static void test_store_sinks_and_release_returns(void) {
    WubuSlime s;
    wubu_slime_init(&s, 8, 800.0f, 600.0f, 16.0f);
    wubu_slime_add(&s, WUBU_EDGE_LEFT, 0.5f, 20.0f, 6.0f, 14.0f, 6.0f);
    settle(&s, 200);

    float released_y = s.blobs[0].blob.y;
    float released_r = s.blobs[0].blob.radius;
    CHECK(fabsf(released_y - 300.0f) < 2.0f, "released at mid-left rim");
    CHECK(fabsf(released_r - 14.0f) < 0.5f, "released at full size");

    wubu_slime_store(&s, 0);
    settle(&s, 300);
    CHECK(fabsf(s.blobs[0].blob.x - 6.0f) < 2.0f, "stored sinks deeper into the seam");
    CHECK(s.blobs[0].blob.radius < 7.0f, "stored shrinks toward the absorbed size");

    wubu_slime_release(&s, 0);
    settle(&s, 300);
    CHECK(fabsf(s.blobs[0].blob.x - 20.0f) < 2.0f, "released comes back out");
    CHECK(s.blobs[0].blob.radius > 13.0f, "released regains its size");
}

/* Dragging along the rim must move the HOME, so a released blob stays where it
 * was put instead of snapping back. */
static void test_drag_moves_the_home(void) {
    WubuSlime s;
    wubu_slime_init(&s, 8, 800.0f, 600.0f, 16.0f);
    wubu_slime_add(&s, WUBU_EDGE_TOP, 0.1f, 20.0f, 4.0f, 14.0f, 6.0f);
    settle(&s, 200);

    wubu_slime_drag(&s, 0, 600.0f, 20.0f);
    wubu_slime_drop(&s, 0);
    settle(&s, 400);

    CHECK(fabsf(s.blobs[0].home_t - 0.75f) < 0.01f, "home slides with the drag");
    CHECK(fabsf(s.blobs[0].blob.x - 600.0f) < 4.0f, "released where it was put");
}

static void test_hit_testing(void) {
    WubuSlime s;
    wubu_slime_init(&s, 8, 800.0f, 600.0f, 16.0f);
    wubu_slime_add(&s, WUBU_EDGE_TOP, 0.5f, 20.0f, 4.0f, 14.0f, 6.0f);
    settle(&s, 100);

    int hit = wubu_slime_hit(&s, s.blobs[0].blob.x, s.blobs[0].blob.y);
    CHECK(hit == 0, "the blob's own centre hits it");
    CHECK(wubu_slime_hit(&s, 700.0f, 500.0f) == -1, "a far point misses");
}

static void test_every_edge_and_long_run(void) {
    WubuSlime s;
    wubu_slime_init(&s, 8, 800.0f, 600.0f, 16.0f);
    wubu_slime_add(&s, WUBU_EDGE_TOP,    0.2f, 20.0f, 4.0f, 12.0f, 5.0f);
    wubu_slime_add(&s, WUBU_EDGE_RIGHT,  0.4f, 20.0f, 4.0f, 12.0f, 5.0f);
    wubu_slime_add(&s, WUBU_EDGE_BOTTOM, 0.6f, 20.0f, 4.0f, 12.0f, 5.0f);
    wubu_slime_add(&s, WUBU_EDGE_LEFT,   0.8f, 20.0f, 4.0f, 12.0f, 5.0f);

    WubuSpringWorld w = wubu_spring_ooze();
    w.damping = 0.0f;                     /* hostile: no energy loss */
    for (int i = 0; i < 3000; i++) wubu_slime_step(&s, &w);

    CHECK(finite_all(&s), "four edges stay finite over 3000 undamped steps");

    /* Each blob must still be on (or very near) its own edge. */
    const WubuSlimeBlob *t = &s.blobs[0];
    CHECK(t->blob.y < 40.0f, "top blob stays near the top");
    const WubuSlimeBlob *r = &s.blobs[1];
    CHECK(r->blob.x > 760.0f, "right blob stays near the right");
    const WubuSlimeBlob *b = &s.blobs[2];
    CHECK(b->blob.y > 560.0f, "bottom blob stays near the bottom");
    const WubuSlimeBlob *l = &s.blobs[3];
    CHECK(l->blob.x < 40.0f, "left blob stays near the left");
}

static void test_capacity_and_bad_input(void) {
    WubuSlime s;
    CHECK(!wubu_slime_init(NULL, 4, 100.0f, 100.0f, 8.0f), "NULL seam refused");
    CHECK(!wubu_slime_init(&s, 0, 100.0f, 100.0f, 8.0f), "zero capacity refused");
    CHECK(!wubu_slime_init(&s, 4, 0.0f, 100.0f, 8.0f), "zero width refused");

    CHECK(wubu_slime_init(&s, 2, 800.0f, 600.0f, 16.0f), "seam created");
    CHECK(wubu_slime_add(&s, WUBU_EDGE_TOP, 0.5f, 10.0f, 2.0f, 8.0f, 3.0f) == 0, "first add");
    CHECK(wubu_slime_add(&s, WUBU_EDGE_TOP, 0.6f, 10.0f, 2.0f, 8.0f, 3.0f) == 1, "second add");
    CHECK(wubu_slime_add(&s, WUBU_EDGE_TOP, 0.7f, 10.0f, 2.0f, 8.0f, 3.0f) == -1,
          "beyond capacity refused");
    wubu_slime_free(&s);
}

int main(void) {
    printf("=== wubu slime seam ===\n");
    test_rests_on_its_home();
    test_store_sinks_and_release_returns();
    test_drag_moves_the_home();
    test_hit_testing();
    test_every_edge_and_long_run();
    test_capacity_and_bad_input();

    if (g_fails == 0) {
        printf("=== slime: %d checks, 0 failed ===\n", g_checks);
        return 0;
    }
    printf("=== slime: %d checks, %d failed ===\n", g_checks, g_fails);
    return 1;
}
