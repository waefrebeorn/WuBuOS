/* wubu_scroll_test.c -- the arithmetic every list depends on, checked without a display. */
#include "wubu_scroll.h"

#include <stdio.h>

static int g_fail;

#define TEST(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); g_fail++; } \
    else printf("  ok   %s\n", msg); } while (0)

int main(void) {
    printf("=== wubu_scroll ===\n");

    /* everything fits -> nothing to scroll, full track, no thumb travel */
    WubuScroll f = { .content = 200, .view = 300, .offset = 0 };
    TEST(wubu_scroll_max(&f) == 0, "content shorter than the view has no scroll range");
    int len = 0, pos = 0;
    wubu_scroll_thumb(&f, 400, &len, &pos);
    TEST(len == 400 && pos == 0, "the thumb fills the track when nothing can scroll");

    /* a list twice the viewport */
    WubuScroll s = { .content = 800, .view = 400, .offset = 0 };
    TEST(wubu_scroll_max(&s) == 400, "max offset is content minus view");
    wubu_scroll_thumb(&s, 400, &len, &pos);
    TEST(len == 200, "thumb is half the track when content is twice the view");
    TEST(pos == 0, "thumb starts at the top at offset 0");

    s.offset = 400;                       /* bottom */
    wubu_scroll_thumb(&s, 400, &len, &pos);
    TEST(pos == 200, "at the bottom the thumb sits at track minus its length (its END touches the end)");

    s.offset = 9999;
    TEST(wubu_scroll_clamp(&s) == 400, "an out-of-range offset clamps to the bottom");

    /* wheel: three rows per notch, clamped at both ends */
    s.offset = 0;
    TEST(wubu_scroll_wheel(&s, 1, 26) == 78, "one notch scrolls three rows (Windows default)");
    TEST(wubu_scroll_wheel(&s, -5, 26) == 0, "scrolling up past the top stops at 0");
    TEST(wubu_scroll_wheel(&s, 99, 26) == 400, "scrolling down past the end stops at max");

    /* the grabbable floor: a very long list must still leave a thumb you can hit */
    WubuScroll huge = { .content = 100000, .view = 400, .offset = 0 };
    wubu_scroll_thumb(&huge, 400, &len, &pos);
    TEST(len >= WUBU_SCROLL_THUMB_MIN, "a 100x list still leaves a grabbable thumb, not a hairline");

    /* drag: grabbing the thumb and moving down by the full travel must reach the bottom */
    WubuScroll d = { .content = 800, .view = 400, .offset = 0 };
    wubu_scroll_thumb(&d, 400, &len, &pos);
    int grab = 5;                                     /* pressed 5 px into the thumb */
    int bottom = wubu_scroll_from_thumb_drag(&d, 400, 400 - 1, grab);
    TEST(bottom > 390, "dragging the thumb to the bottom of the track reaches the end of the list");
    int top = wubu_scroll_from_thumb_drag(&d, 400, 0, grab);
    TEST(top == 0, "dragging it back to the top reaches offset 0");

    /* the band is Microsoft's 16 px overlay, and the thumb test tells a grab from a track jump */
    TEST(wubu_scroll_in_band(0) && wubu_scroll_in_band(15) && !wubu_scroll_in_band(16),
         "the hit band is 16 px, and only 16");
    WubuScroll t = { .content = 800, .view = 400, .offset = 200 };
    TEST(wubu_scroll_on_thumb(&t, 400, 100), "a press on the thumb is a grab");
    TEST(!wubu_scroll_on_thumb(&t, 400, 300), "a press outside the thumb is a track jump");

    printf("=== wubu_scroll: %d failed ===\n", g_fail);
    return g_fail ? 1 : 0;
}
