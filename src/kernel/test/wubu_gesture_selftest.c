/*
 * wubu_gesture_selftest.c -- verifies the multitouch gesture recogniser with
 * synthetic contact sequences (no touchpad required).
 */

#include "wubu_gesture.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while(0)

static int failures = 0;
static int passed = 0;

/* Feed a full touch-down / move / lift cycle for `n` contacts. */
static void gesture_begin(int n, int x0, int y0, int dx, int dy,
                          uint32_t t0) {
    for (int i = 0; i < n; i++)
        wubu_gesture_contact(i, x0 + i * dx, y0 + i * dy, true, t0);
}

static void gesture_move(int n, int x0, int y0, int dx, int dy,
                         uint32_t t) {
    for (int i = 0; i < n; i++)
        wubu_gesture_contact(i, x0 + i * dx, y0 + i * dy, true, t);
}

static void gesture_end(int n, int x0, int y0, int dx, int dy,
                        uint32_t t) {
    for (int i = 0; i < n; i++)
        wubu_gesture_contact(i, x0 + i * dx, y0 + i * dy, false, t);
}

/* One contact down, small move, up -> TAP. */
static void test_tap(void) {
    printf("-- tap\n");
    wubu_gesture_init();
    gesture_begin(1, 500, 500, 0, 0, 1000);
    gesture_move(1, 501, 500, 0, 0, 1050);
    gesture_end(1, 501, 500, 0, 0, 1100);

    WubuGesture g = WUBU_GESTURE_NONE;
    CHECK(wubu_gesture_poll(&g) == 1, "a gesture is reported");
    CHECK(g == WUBU_GESTURE_TAP, "recognised as a tap");
    CHECK(wubu_gesture_poll(&g) == 0, "no second gesture");
    CHECK(wubu_gesture_contact_count() == 0, "no contacts remain");
}

/* Two taps close in time and space -> DOUBLE_TAP. */
static void test_double_tap(void) {
    printf("-- double tap\n");
    wubu_gesture_init();
    gesture_begin(1, 500, 500, 0, 0, 1000);
    gesture_end(1, 500, 500, 0, 0, 1100);
    WubuGesture g = WUBU_GESTURE_NONE;
    wubu_gesture_poll(&g);
    CHECK(g == WUBU_GESTURE_TAP, "first contact is a tap");

    gesture_begin(1, 505, 502, 0, 0, 1200);
    gesture_end(1, 505, 502, 0, 0, 1300);
    CHECK(wubu_gesture_poll(&g) == 1, "second gesture reported");
    CHECK(g == WUBU_GESTURE_DOUBLE_TAP, "two quick taps are a double tap");
}

/* Two taps far apart in time are not a double tap. */
static void test_double_tap_window(void) {
    printf("-- double tap window\n");
    wubu_gesture_init();
    gesture_begin(1, 500, 500, 0, 0, 1000);
    gesture_end(1, 500, 500, 0, 0, 1100);
    WubuGesture g = WUBU_GESTURE_NONE;
    wubu_gesture_poll(&g);

    gesture_begin(1, 500, 500, 0, 0, 2000);   /* 900 ms later: too slow */
    gesture_end(1, 500, 500, 0, 0, 2100);
    wubu_gesture_poll(&g);
    CHECK(g == WUBU_GESTURE_TAP, "a slow second tap is just a tap");
}

/* Two contacts sweeping right -> SWIPE_RIGHT. */
static void test_swipe_right(void) {
    printf("-- swipe right\n");
    wubu_gesture_init();
    gesture_begin(2, 400, 500, 200, 0, 1000);
    gesture_move(2, 450, 500, 200, 0, 1050);
    gesture_end(2, 500, 500, 200, 0, 1100);

    WubuGesture g = WUBU_GESTURE_NONE;
    CHECK(wubu_gesture_poll(&g) == 1, "a gesture is reported");
    CHECK(g == WUBU_GESTURE_SWIPE_RIGHT, "recognised as a right swipe");
}

/* Two contacts sweeping up -> SWIPE_UP. */
static void test_swipe_up(void) {
    printf("-- swipe up\n");
    wubu_gesture_init();
    gesture_begin(2, 500, 700, 0, 100, 1000);
    gesture_move(2, 500, 650, 0, 100, 1050);
    gesture_end(2, 500, 600, 0, 100, 1100);

    WubuGesture g = WUBU_GESTURE_NONE;
    wubu_gesture_poll(&g);
    CHECK(g == WUBU_GESTURE_SWIPE_UP, "recognised as an up swipe");
}

/* Diagonal motion must not be reported as a swipe (dominance rule). */
static void test_no_diagonal_swipe(void) {
    printf("-- no diagonal swipe\n");
    wubu_gesture_init();
    /* 45 degrees: neither axis dominates. */
    gesture_begin(2, 400, 700, 200, 0, 1000);
    gesture_end(2, 500, 600, 200, 0, 1100);

    WubuGesture g = WUBU_GESTURE_NONE;
    wubu_gesture_poll(&g);
    CHECK(g != WUBU_GESTURE_SWIPE_RIGHT && g != WUBU_GESTURE_SWIPE_UP,
          "a 45-degree drag is not a swipe");
}

/* Two contacts moving apart -> PINCH_OUT. */
static void test_pinch_out(void) {
    printf("-- pinch out\n");
    wubu_gesture_init();
    gesture_begin(2, 400, 500, 200, 0, 1000);   /* separation 200 */
    gesture_move(2, 350, 500, 300, 0, 1050);    /* separation 300 */
    gesture_end(2, 350, 500, 300, 0, 1100);

    WubuGesture g = WUBU_GESTURE_NONE;
    CHECK(wubu_gesture_poll(&g) == 1, "a gesture is reported");
    CHECK(g == WUBU_GESTURE_PINCH_OUT, "recognised as pinch out");
}

/* Two contacts moving together -> PINCH_IN. */
static void test_pinch_in(void) {
    printf("-- pinch in\n");
    wubu_gesture_init();
    gesture_begin(2, 400, 500, 200, 0, 1000);
    gesture_move(2, 450, 500, 100, 0, 1050);
    gesture_end(2, 450, 500, 100, 0, 1100);

    WubuGesture g = WUBU_GESTURE_NONE;
    wubu_gesture_poll(&g);
    CHECK(g == WUBU_GESTURE_PINCH_IN, "recognised as pinch in");
}

/* Two contacts rotating -> ROTATE_CW. */
static void test_rotate(void) {
    printf("-- rotate\n");
    wubu_gesture_init();
    /* Horizontal pair, then the right contact swings down: +90 degrees. */
    gesture_begin(2, 400, 500, 200, 0, 1000);
    gesture_move(2, 400, 500, 0, 200, 1050);
    gesture_end(2, 400, 500, 0, 200, 1100);

    WubuGesture g = WUBU_GESTURE_NONE;
    CHECK(wubu_gesture_poll(&g) == 1, "a gesture is reported");
    CHECK(g == WUBU_GESTURE_ROTATE_CW, "recognised as a clockwise rotation");
}

/* Live pinch scale tracks the separation while the gesture is in flight. */
static void test_live_pinch(void) {
    printf("-- live pinch scale\n");
    wubu_gesture_init();
    CHECK(wubu_gesture_pinch_scale() == 1.0f, "starts at 1.0");

    gesture_begin(2, 400, 500, 200, 0, 1000);
    gesture_move(2, 400, 500, 100, 0, 1050);    /* separation halves */
    float s = wubu_gesture_pinch_scale();
    CHECK(s > 0.4f && s < 0.6f, "live pinch tracks the separation");

    gesture_end(2, 400, 500, 100, 0, 1100);
    WubuGesture g = WUBU_GESTURE_NONE;
    wubu_gesture_poll(&g);
    CHECK(wubu_gesture_pinch_scale() == 1.0f, "scale resets after the lift");
}

/* A long, slow drag is not a tap. */
static void test_slow_drag_not_tap(void) {
    printf("-- slow drag is not a tap\n");
    wubu_gesture_init();
    gesture_begin(1, 500, 500, 0, 0, 1000);
    gesture_end(1, 500, 500, 0, 0, 2000);        /* 1000 ms: too slow */

    WubuGesture g = WUBU_GESTURE_NONE;
    wubu_gesture_poll(&g);
    CHECK(g == WUBU_GESTURE_NONE, "a slow press is not a tap");
}

int main(void)
{
    printf("=== wubu_gesture_selftest ===\n");

    test_tap();
    test_double_tap();
    test_double_tap_window();
    test_swipe_right();
    test_swipe_up();
    test_no_diagonal_swipe();
    test_pinch_out();
    test_pinch_in();
    test_rotate();
    test_live_pinch();
    test_slow_drag_not_tap();

    printf("\n=== wubu_gesture: %d passed, %d failed ===\n",
           passed, failures);
    return failures ? 1 : 0;
}
