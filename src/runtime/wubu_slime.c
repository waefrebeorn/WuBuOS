/*
 * wubu_slime.c -- see wubu_slime.h.
 *
 * Each blob is tied by a spring to a PINNED ANCHOR sitting at its home on the
 * rim.  The spring's rest length is zero, so the blob wants to be exactly at
 * home; the stiffness decides how lazily it follows.  Storing a blob moves the
 * anchor deeper into the seam and shrinks the blob, releasing reverses both.
 */
#include "wubu_slime.h"

#include <stdlib.h>
#include <string.h>

bool wubu_slime_init(WubuSlime *s, int cap, float screen_w, float screen_h,
                     float margin) {
    if (!s || cap <= 0 || screen_w <= 0.0f || screen_h <= 0.0f) return false;
    memset(s, 0, sizeof *s);

    s->blobs = (WubuSlimeBlob *)calloc((size_t)cap, sizeof *s->blobs);
    s->scratch = (WubuBlob *)calloc((size_t)cap * 2, sizeof *s->scratch);
    s->links = (WubuSpringLink *)calloc((size_t)cap, sizeof *s->links);
    if (!s->blobs || !s->scratch || !s->links) {
        wubu_slime_free(s);
        return false;
    }
    s->cap = cap;
    s->screen_w = screen_w;
    s->screen_h = screen_h;
    s->margin = margin > 0.0f ? margin : 8.0f;
    return true;
}

void wubu_slime_free(WubuSlime *s) {
    if (!s) return;
    free(s->blobs);   s->blobs = NULL;
    free(s->scratch); s->scratch = NULL;
    free(s->links);   s->links = NULL;
    s->n = s->cap = 0;
}

void wubu_slime_position(const WubuSlime *s, const WubuSlimeBlob *b,
                         float t, float depth, float *out_x, float *out_y) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float x = 0.0f, y = 0.0f;
    switch (b->edge) {
        case WUBU_EDGE_TOP:    x = t * s->screen_w;  y = depth; break;
        case WUBU_EDGE_BOTTOM: x = t * s->screen_w;  y = s->screen_h - depth; break;
        case WUBU_EDGE_LEFT:   x = depth;            y = t * s->screen_h; break;
        case WUBU_EDGE_RIGHT:  x = s->screen_w - depth; y = t * s->screen_h; break;
        default: break;
    }
    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
}

int wubu_slime_add(WubuSlime *s, WubuEdge edge, float home_t,
                   float out, float in, float radius_out, float radius_in) {
    if (!s || s->n >= s->cap || (int)edge < 0 || (int)edge >= WUBU_EDGE_COUNT)
        return -1;

    WubuSlimeBlob *b = &s->blobs[s->n];
    memset(b, 0, sizeof *b);
    b->edge = edge;
    b->home_t = home_t;
    b->depth_out = out;
    b->depth_in = in;
    b->radius_out = radius_out;
    b->radius_in = radius_in;
    b->stored = false;

    float x, y;
    wubu_slime_position(s, b, home_t, out, &x, &y);
    b->blob.x = b->blob.px = x;
    b->blob.y = b->blob.py = y;
    b->blob.radius = radius_out;
    b->blob.mass = 1.0f;
    b->blob.pinned = false;

    s->links[s->n].a = s->n;
    s->links[s->n].b = s->cap + s->n;   /* the anchor, placed at home */
    s->links[s->n].rest = 0.0f;
    s->links[s->n].stiffness = 0.4f;

    return s->n++;
}

static void slime_home(const WubuSlime *s, const WubuSlimeBlob *b,
                       float *hx, float *hy) {
    float depth = b->stored ? b->depth_in : b->depth_out;
    wubu_slime_position(s, b, b->home_t, depth, hx, hy);
}

void wubu_slime_store(WubuSlime *s, int i) {
    if (!s || i < 0 || i >= s->n) return;
    s->blobs[i].stored = true;
}

void wubu_slime_release(WubuSlime *s, int i) {
    if (!s || i < 0 || i >= s->n) return;
    s->blobs[i].stored = false;
}

void wubu_slime_toggle(WubuSlime *s, int i) {
    if (!s || i < 0 || i >= s->n) return;
    s->blobs[i].stored = !s->blobs[i].stored;
}

void wubu_slime_drag(WubuSlime *s, int i, float x, float y) {
    if (!s || i < 0 || i >= s->n) return;
    WubuSlimeBlob *b = &s->blobs[i];
    b->dragging = true;
    /* The blob follows the pointer... */
    b->blob.x = b->blob.px = x;
    b->blob.y = b->blob.py = y;
    /* ...but its home slides along the rim toward the pointer, so releasing
     * it leaves it where you put it rather than snapping back. */
    float t;
    switch (b->edge) {
        case WUBU_EDGE_TOP:
        case WUBU_EDGE_BOTTOM: t = x / s->screen_w; break;
        case WUBU_EDGE_LEFT:
        case WUBU_EDGE_RIGHT:  t = y / s->screen_h; break;
        default:               t = b->home_t; break;
    }
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    b->home_t = t;
}

void wubu_slime_drop(WubuSlime *s, int i) {
    if (!s || i < 0 || i >= s->n) return;
    s->blobs[i].dragging = false;
}

void wubu_slime_step(WubuSlime *s, const WubuSpringWorld *w) {
    int n = s->n;
    if (!s || n <= 0) return;

    for (int i = 0; i < n; i++) {
        WubuSlimeBlob *b = &s->blobs[i];
        WubuBlob *body = &s->scratch[i];
        WubuBlob *anchor = &s->scratch[s->cap + i];

        *body = b->blob;
        /* Dragging pins the blob under the pointer; otherwise it is free to
         * spring.  The anchor is ALWAYS pinned -- it is the home. */
        body->pinned = b->dragging;

        float hx, hy;
        slime_home(s, b, &hx, &hy);
        anchor->x = anchor->px = hx;
        anchor->y = anchor->py = hy;
        anchor->radius = 0.0f;
        anchor->mass = 0.0f;
        anchor->pinned = true;

        /* Stored blobs shrink toward their absorbed size. */
        float target = b->stored ? b->radius_in : b->radius_out;
        b->blob.radius += (target - b->blob.radius) * 0.25f;
    }

    /* Bodies are [0, n) the blobs and [cap, cap+n) their anchors, so the
     * solver sees n + cap bodies. */
    wubu_spring_step(s->scratch, n + s->cap, s->links, n, w);

    for (int i = 0; i < n; i++) {
        WubuSlimeBlob *b = &s->blobs[i];
        b->blob.x  = s->scratch[i].x;
        b->blob.y  = s->scratch[i].y;
        b->blob.px = s->scratch[i].px;
        b->blob.py = s->scratch[i].py;
    }
}

int wubu_slime_hit(const WubuSlime *s, float x, float y) {
    if (!s) return -1;
    for (int i = 0; i < s->n; i++) {
        const WubuSlimeBlob *b = &s->blobs[i];
        float dx = x - b->blob.x;
        float dy = y - b->blob.y;
        float r = b->blob.radius > 0.0f ? b->blob.radius : 1.0f;
        if (dx * dx + dy * dy <= r * r) return i;
    }
    return -1;
}
