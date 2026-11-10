/*
 * wubu_spring_test.c -- the soft-body solver against known behaviour.
 *
 * Every case checks a property with a known answer: a spring settles at its
 * rest length, a pinned anchor never moves, a mass ratio splits the correction,
 * a constrained blob stays inside its rect, and nothing ever goes NaN.
 */
#include "../runtime/wubu_spring.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_checks, g_fails;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

static int finite_blob(const WubuBlob *b) {
    return isfinite(b->x) && isfinite(b->y) && isfinite(b->px) && isfinite(b->py);
}

/* A free blob on a spring from a pinned anchor settles at the rest length. */
static void test_settles_at_rest_length(void) {
    WubuBlob blobs[2];
    memset(blobs, 0, sizeof blobs);
    blobs[0].x = blobs[0].px = 0.0f;   /* anchor */
    blobs[0].y = blobs[0].py = 0.0f;
    blobs[0].radius = 8.0f;
    blobs[0].mass = 0.0f;              /* immovable */
    blobs[0].pinned = true;

    blobs[1].x = blobs[1].px = 200.0f; /* stretched far past rest */
    blobs[1].y = blobs[1].py = 0.0f;
    blobs[1].radius = 8.0f;
    blobs[1].mass = 1.0f;

    WubuSpringLink link = { 0, 1, 40.0f, 0.5f };
    WubuSpringWorld w = wubu_spring_ooze();

    for (int i = 0; i < 240; i++)
        wubu_spring_step(blobs, 2, &link, 1, &w);

    float d = wubu_spring_dist(&blobs[0], &blobs[1]);
    CHECK(fabsf(d - 40.0f) < 2.0f, "settles at the rest length (within 2px)");
    CHECK(blobs[0].x == 0.0f && blobs[0].y == 0.0f, "the anchor never moves");
    CHECK(finite_blob(&blobs[1]), "no NaN after 240 steps");
}

/* A stretched spring must CONTRACT, not snap or explode. */
static void test_stretched_contract(void) {
    WubuBlob blobs[2];
    memset(blobs, 0, sizeof blobs);
    blobs[0].pinned = true; blobs[0].x = blobs[0].px = 0.0f;
    blobs[0].y = blobs[0].py = 0.0f; blobs[0].radius = 4.0f;
    blobs[1].x = blobs[1].px = 300.0f; blobs[1].y = blobs[1].py = 0.0f;
    blobs[1].radius = 4.0f; blobs[1].mass = 1.0f;

    WubuSpringLink link = { 0, 1, 50.0f, 0.8f };
    WubuSpringWorld w = wubu_spring_rubber();

    float before = wubu_spring_dist(&blobs[0], &blobs[1]);
    wubu_spring_step(blobs, 2, &link, 1, &w);
    float after = wubu_spring_dist(&blobs[0], &blobs[1]);
    CHECK(after < before, "one step contracts a stretched spring");
    CHECK(after > 49.0f, "one step does not overshoot past rest");
}

/* Equal masses split the correction evenly. */
static void test_mass_ratio(void) {
    WubuBlob blobs[2];
    memset(blobs, 0, sizeof blobs);
    blobs[0].x = blobs[0].px = 0.0f;  blobs[0].y = blobs[0].py = 0.0f;
    blobs[0].radius = 4.0f; blobs[0].mass = 1.0f;
    blobs[1].x = blobs[1].px = 200.0f; blobs[1].y = blobs[1].py = 0.0f;
    blobs[1].radius = 4.0f; blobs[1].mass = 1.0f;

    WubuSpringLink link = { 0, 1, 100.0f, 1.0f };
    WubuSpringWorld w = wubu_spring_ooze();

    wubu_spring_step(blobs, 2, &link, 1, &w);
    /* Equal masses: each moves half the error, so the midpoint is preserved. */
    float mid = (blobs[0].x + blobs[1].x) / 2.0f;
    CHECK(fabsf(mid - 100.0f) < 0.001f, "equal masses keep the midpoint");

    /* A 3:1 mass ratio moves the light blob three times as far. */
    WubuBlob b2[2];
    memset(b2, 0, sizeof b2);
    b2[0].x = b2[0].px = 0.0f;  b2[0].y = b2[0].py = 0.0f;
    b2[0].radius = 4.0f; b2[0].mass = 3.0f;
    b2[1].x = b2[1].px = 100.0f; b2[1].y = b2[1].py = 0.0f;
    b2[1].radius = 4.0f; b2[1].mass = 1.0f;
    WubuSpringLink l2 = { 0, 1, 40.0f, 1.0f };
    wubu_spring_step(b2, 2, &l2, 1, &w);
    float moved_heavy = fabsf(b2[0].x - 0.0f);
    float moved_light = fabsf(b2[1].x - 100.0f);
    CHECK(moved_light > moved_heavy * 2.0f,
          "the light blob moves further than the heavy one");
}

/* A blob pressed outside the rect is pushed back inside, with squish slack. */
static void test_constrain_rect(void) {
    WubuBlob b;
    memset(&b, 0, sizeof b);
    b.x = b.px = -50.0f; b.y = b.py = -50.0f;
    b.radius = 10.0f; b.mass = 1.0f;

    WubuBlob blobs[1] = { b };
    wubu_spring_constrain_rect(blobs, 1, 0.0f, 0.0f, 800.0f, 600.0f, 0.5f);
    CHECK(blobs[0].x >= 0.0f, "pushed back from the left edge");
    CHECK(blobs[0].y >= 0.0f, "pushed back from the top edge");
    CHECK(fabsf(blobs[0].x - 5.0f) < 0.001f,
          "squish lets it overlap by radius*0.5");

    blobs[0].x = blobs[0].px = 9000.0f;
    blobs[0].y = blobs[0].py = 9000.0f;
    wubu_spring_constrain_rect(blobs, 1, 0.0f, 0.0f, 800.0f, 600.0f, 0.0f);
    CHECK(blobs[0].x <= 800.0f && blobs[0].y <= 600.0f,
          "clamped to the far edges with zero squish");
}

/* Long runs must stay finite: a stiff spring with no damping is the classic
 * way to make a verlet solver explode. */
static void test_no_explosion(void) {
    enum { N = 8 };
    WubuBlob blobs[N];
    memset(blobs, 0, sizeof blobs);
    for (int i = 0; i < N; i++) {
        blobs[i].x = blobs[i].px = (float)(i * 30);
        blobs[i].y = blobs[i].py = 100.0f;
        blobs[i].radius = 6.0f;
        blobs[i].mass = 1.0f;
    }
    blobs[0].pinned = true;

    WubuSpringLink links[N - 1];
    for (int i = 0; i < N - 1; i++) {
        links[i].a = i; links[i].b = i + 1;
        links[i].rest = 20.0f; links[i].stiffness = 1.0f;
    }

    WubuSpringWorld w = wubu_spring_ooze();
    w.damping = 0.0f;                    /* the hostile case: no energy loss */
    w.iterations = 20;

    for (int s = 0; s < 2000; s++)
        wubu_spring_step(blobs, N, links, N - 1, &w);

    int ok = 1;
    for (int i = 0; i < N; i++) if (!finite_blob(&blobs[i])) ok = 0;
    CHECK(ok, "an undamped stiff chain stays finite over 2000 steps");

    /* And it must still be roughly at rest, not vibrating to infinity. */
    float spread = 0.0f;
    for (int i = 1; i < N; i++) {
        float d = wubu_spring_dist(&blobs[i - 1], &blobs[i]);
        spread += fabsf(d - 20.0f);
    }
    CHECK(spread < 60.0f, "the chain settles near its rest lengths");
}

/* Determinism: the same world stepped twice gives the same answer. */
static void test_deterministic(void) {
    WubuBlob a[3], b[3];
    memset(a, 0, sizeof a);
    memset(b, 0, sizeof b);
    for (int i = 0; i < 3; i++) {
        a[i].x = a[i].px = (float)(i * 25);
        a[i].y = a[i].py = 50.0f + (float)i;
        a[i].radius = 6.0f;
        a[i].mass = 1.0f + (float)i;
        b[i] = a[i];
    }
    a[0].pinned = b[0].pinned = true;

    WubuSpringLink la[2], lb[2];
    for (int i = 0; i < 2; i++) {
        la[i] = (WubuSpringLink){ i, i + 1, 30.0f, 0.6f };
        lb[i] = la[i];
    }
    WubuSpringWorld w = wubu_spring_ooze();
    for (int s = 0; s < 120; s++) {
        wubu_spring_step(a, 3, la, 2, &w);
        wubu_spring_step(b, 3, lb, 2, &w);
    }
    CHECK(memcmp(a, b, sizeof a) == 0, "identical worlds step identically");
}

/* The shared anchored-spring scaffold: a free body settles on its anchor, a
 * pinned body never moves, capacity is enforced, and free clears everything. */
static void test_field(void) {
    WubuSpringField f;
    CHECK(wubu_spring_field_init(&f, 2), "field created");
    CHECK(f.n == 0 && f.cap == 2, "field starts empty");
    CHECK(!wubu_spring_field_init(NULL, 4), "NULL field refused");
    CHECK(!wubu_spring_field_init(&f, 0), "zero capacity refused");

    CHECK(wubu_spring_field_add(&f, 0.5f) == 0, "first body added");
    CHECK(wubu_spring_field_add(&f, 0.5f) == 1, "second body added");
    CHECK(wubu_spring_field_add(&f, 0.5f) == -1, "third refused when full");

    WubuBlob bodies[2];
    memset(bodies, 0, sizeof bodies);
    bodies[0].x = bodies[0].px = 100.0f;      /* far from its anchor */
    bodies[0].y = bodies[0].py = 0.0f;
    bodies[0].mass = 1.0f;
    bodies[1].x = bodies[1].px = 0.0f;
    bodies[1].y = bodies[1].py = 0.0f;
    bodies[1].mass = 1.0f;

    f.ax[0] = 0.0f;  f.ay[0] = 0.0f;  f.pinned[0] = false;
    f.ax[1] = 50.0f; f.ay[1] = 0.0f;  f.pinned[1] = true;   /* held in place */

    WubuSpringWorld w = wubu_spring_ooze();
    for (int i = 0; i < 300; i++)
        wubu_spring_field_step(&f, &w, bodies, 2, sizeof(WubuBlob));

    CHECK(fabsf(bodies[0].x) < 2.0f, "a free body settles on its anchor");
    CHECK(bodies[1].x == 0.0f && bodies[1].y == 0.0f,
          "a pinned body never moves");
    CHECK(isfinite(bodies[0].x) && isfinite(bodies[1].x),
          "no NaN after 300 steps");

    wubu_spring_field_free(&f);
    CHECK(f.n == 0 && f.cap == 0 && f.scratch == NULL, "field freed");
}

int main(void) {
    printf("=== wubu spring (soft-body solver) ===\n");
    test_settles_at_rest_length();
    test_stretched_contract();
    test_mass_ratio();
    test_constrain_rect();
    test_no_explosion();
    test_deterministic();
    test_field();

    if (g_fails == 0) {
        printf("=== spring: %d checks, 0 failed ===\n", g_checks);
        return 0;
    }
    printf("=== spring: %d checks, %d failed ===\n", g_checks, g_fails);
    return 1;
}
