/*
 * wubu_rim.c -- see wubu_rim.h.
 *
 * The rim is a closed loop.  Perimeter is split into four legs:
 *   [0, w)           top
 *   [w, w+h)         right
 *   [w+h, 2w+h)      bottom
 *   [2w+h, 2w+2h)    left
 * and t is the distance along that loop divided by the perimeter.
 */
#include "wubu_rim.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

bool wubu_rim_init(WubuRim *r, int cap, float x, float y, float w, float h) {
    if (!r || cap <= 0 || w <= 0.0f || h <= 0.0f) return false;
    memset(r, 0, sizeof *r);

    r->islands = (WubuRimIsland *)calloc((size_t)cap, sizeof *r->islands);
    r->scratch = (WubuBlob *)calloc((size_t)cap * 2, sizeof *r->scratch);
    r->links   = (WubuSpringLink *)calloc((size_t)cap, sizeof *r->links);
    if (!r->islands || !r->scratch || !r->links) {
        wubu_rim_free(r);
        return false;
    }
    r->cap = cap;
    wubu_rim_set_rect(r, x, y, w, h);
    return true;
}

void wubu_rim_free(WubuRim *r) {
    if (!r) return;
    free(r->islands); r->islands = NULL;
    free(r->scratch); r->scratch = NULL;
    free(r->links);   r->links = NULL;
    r->n = r->cap = 0;
}

void wubu_rim_set_rect(WubuRim *r, float x, float y, float w, float h) {
    if (!r || w <= 0.0f || h <= 0.0f) return;
    r->x = x; r->y = y; r->w = w; r->h = h;
}

float wubu_rim_perimeter(const WubuRim *r) {
    return 2.0f * (r->w + r->h);
}

void wubu_rim_position(const WubuRim *r, float t, float *out_x, float *out_y) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float per = wubu_rim_perimeter(r);
    float s = t * per;

    float x = r->x, y = r->y, w = r->w, h = r->h;
    float px = x, py = y;

    if (s < w) {                       /* top, left -> right */
        px = x + s;  py = y;
    } else if (s < w + h) {            /* right, top -> bottom */
        px = x + w;  py = y + (s - w);
    } else if (s < 2.0f * w + h) {     /* bottom, right -> left */
        px = x + w - (s - w - h);  py = y + h;
    } else {                           /* left, bottom -> top */
        px = x;  py = y + h - (s - 2.0f * w - h);
    }
    if (out_x) *out_x = px;
    if (out_y) *out_y = py;
}

/* Squared distance from (px,py) to the nearest point of segment a-b. */
static float seg_nearest(float ax, float ay, float bx, float by,
                         float px, float py) {
    float dx = bx - ax, dy = by - ay;
    float len2 = dx * dx + dy * dy;
    float t = len2 > 0.0f ? ((px - ax) * dx + (py - ay) * dy) / len2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float ex = px - (ax + t * dx);
    float ey = py - (ay + t * dy);
    return ex * ex + ey * ey;
}

float wubu_rim_nearest_t(const WubuRim *r, float px, float py) {
    float x = r->x, y = r->y, w = r->w, h = r->h;
    float per = wubu_rim_perimeter(r);

    /* Leg start distances along the loop. */
    float starts[4] = { 0.0f, w, w + h, 2.0f * w + h };
    float ax[4] = { x,     x + w, x + w, x     };
    float ay[4] = { y,     y,     y + h, y + h };
    float bx[4] = { x + w, x + w, x,     x     };
    float by[4] = { y,     y + h, y + h, y     };

    float best = seg_nearest(ax[0], ay[0], bx[0], by[0], px, py);
    float best_t = 0.0f;

    for (int i = 0; i < 4; i++) {
        float dx = bx[i] - ax[i], dy = by[i] - ay[i];
        float len2 = dx * dx + dy * dy;
        float u = len2 > 0.0f ? ((px - ax[i]) * dx + (py - ay[i]) * dy) / len2 : 0.0f;
        if (u < 0.0f) u = 0.0f;
        if (u > 1.0f) u = 1.0f;
        float ex = px - (ax[i] + u * dx);
        float ey = py - (ay[i] + u * dy);
        float d2 = ex * ex + ey * ey;

        if (i == 0 || d2 < best) {
            best = d2;
            /* Distance along the loop = leg start + u * leg length. */
            float leg = sqrtf(len2);
            best_t = (starts[i] + u * leg) / per;
        }
    }
    return best_t;
}

int wubu_rim_add(WubuRim *r, float t, float radius) {
    if (!r || r->n >= r->cap) return -1;
    WubuRimIsland *is = &r->islands[r->n];
    memset(is, 0, sizeof *is);
    is->t = t;
    is->radius = radius;
    is->visible = true;

    float px, py;
    wubu_rim_position(r, t, &px, &py);
    is->blob.x = is->blob.px = px;
    is->blob.y = is->blob.py = py;
    is->blob.radius = radius;
    is->blob.mass = 1.0f;

    r->links[r->n].a = r->n;
    r->links[r->n].b = r->cap + r->n;   /* its home anchor */
    r->links[r->n].rest = 0.0f;
    r->links[r->n].stiffness = 0.45f;

    return r->n++;
}

void wubu_rim_show(WubuRim *r, int i, bool show) {
    if (!r || i < 0 || i >= r->n) return;
    r->islands[i].visible = show;
}

void wubu_rim_drag(WubuRim *r, int i, float px, float py) {
    if (!r || i < 0 || i >= r->n) return;
    WubuRimIsland *is = &r->islands[i];
    is->dragging = true;
    /* The island follows the pointer, and its home t becomes the nearest point
     * on the rim -- so when you let go it settles where you dropped it. */
    is->t = wubu_rim_nearest_t(r, px, py);
    is->blob.x = is->blob.px = px;
    is->blob.y = is->blob.py = py;
}

void wubu_rim_drop(WubuRim *r, int i) {
    if (!r || i < 0 || i >= r->n) return;
    r->islands[i].dragging = false;
}

void wubu_rim_step(WubuRim *r, const WubuSpringWorld *w) {
    int n = r->n;
    if (!r || n <= 0) return;

    for (int i = 0; i < n; i++) {
        WubuRimIsland *is = &r->islands[i];
        WubuBlob *body   = &r->scratch[i];
        WubuBlob *anchor = &r->scratch[r->cap + i];

        *body = is->blob;
        body->pinned = is->dragging || !is->visible;

        float hx, hy;
        wubu_rim_position(r, is->t, &hx, &hy);
        anchor->x = anchor->px = hx;
        anchor->y = anchor->py = hy;
        anchor->radius = 0.0f;
        anchor->mass = 0.0f;
        anchor->pinned = true;
    }

    wubu_spring_step(r->scratch, n + r->cap, r->links, n, w);

    for (int i = 0; i < n; i++) {
        WubuRimIsland *is = &r->islands[i];
        is->blob.x  = r->scratch[i].x;
        is->blob.y  = r->scratch[i].y;
        is->blob.px = r->scratch[i].px;
        is->blob.py = r->scratch[i].py;
    }
}

int wubu_rim_hit(const WubuRim *r, float px, float py) {
    if (!r) return -1;
    for (int i = 0; i < r->n; i++) {
        const WubuRimIsland *is = &r->islands[i];
        if (!is->visible) continue;
        float dx = px - is->blob.x;
        float dy = py - is->blob.y;
        float rad = is->blob.radius > 0.0f ? is->blob.radius : 1.0f;
        if (dx * dx + dy * dy <= rad * rad) return i;
    }
    return -1;
}
