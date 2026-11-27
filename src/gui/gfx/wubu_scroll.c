/*
 * wubu_scroll.c -- see the header for where the numbers come from.
 *
 * Integer only, no framebuffer, no globals: this is pure arithmetic so it can be tested without a display and
 * reused by every list surface.
 */
#include "wubu_scroll.h"

int wubu_scroll_max(const WubuScroll *s) {
    if (!s || s->view <= 0) return 0;
    int m = s->content - s->view;
    return m > 0 ? m : 0;
}

int wubu_scroll_clamp(WubuScroll *s) {
    if (!s) return 0;
    int m = wubu_scroll_max(s);
    if (s->offset > m) s->offset = m;
    if (s->offset < 0) s->offset = 0;
    return s->offset;
}

void wubu_scroll_thumb(const WubuScroll *s, int track, int *len, int *pos) {
    int l = track, p = 0;
    if (s && track > 0 && s->content > 0 && s->view < s->content) {
        l = (track * s->view) / s->content;
        if (l < WUBU_SCROLL_THUMB_MIN) l = WUBU_SCROLL_THUMB_MIN;   /* grabbable floor */
        if (l > track) l = track;
        int max = wubu_scroll_max(s);
        /* the thumb travels the track minus its own length, so the bottom of the thumb is the bottom */
        p = max > 0 ? ((track - l) * wubu_scroll_clamp((WubuScroll *)s)) / max : 0;
        if (p < 0) p = 0;
        if (p > track - l) p = track - l;
    }
    if (len) *len = l;
    if (pos) *pos = p;
}

int wubu_scroll_from_thumb_drag(const WubuScroll *s, int track, int y, int grab_dy) {
    if (!s || track <= 0) return 0;
    int l = 0, p0 = 0;
    wubu_scroll_thumb(s, track, &l, &p0);
    int travel = track - l;
    if (travel <= 0) return 0;
    int want_pos = y - grab_dy;            /* where the thumb's top should be now */
    if (want_pos < 0) want_pos = 0;
    if (want_pos > travel) want_pos = travel;
    int max = wubu_scroll_max(s);
    return (want_pos * max) / travel;
}

int wubu_scroll_wheel(WubuScroll *s, int notches, int row) {
    if (!s) return 0;
    s->offset += notches * row * WUBU_SCROLL_WHEEL_ROWS;
    return wubu_scroll_clamp(s);
}

int wubu_scroll_in_band(int x) { return x >= 0 && x < WUBU_SCROLL_BAND_W; }

int wubu_scroll_on_thumb(const WubuScroll *s, int track, int y) {
    int l = 0, p = 0;
    wubu_scroll_thumb(s, track, &l, &p);
    return y >= p && y < p + l;
}
