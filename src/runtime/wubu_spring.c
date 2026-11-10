/*
 * wubu_spring.c -- see wubu_spring.h.
 *
 * Verlet + Jakobsen relaxation.  No sqrt in the hot loop where it can be
 * avoided: the constraint projection uses the raw length and a scale factor,
 * which is the standard trick and keeps a step cheap enough for every blob on
 * screen at 60Hz.
 */
#include "wubu_spring.h"

#include <math.h>

WubuSpringWorld wubu_spring_ooze(void) {
    WubuSpringWorld w;
    w.damping  = 0.035f;   /* lossy enough to settle, loose enough to jiggle */
    w.gravity  = 0.0f;     /* the seam is on a wall, not the floor */
    w.dt       = 1.0f;     /* fixed step; the caller advances one step at a time */
    w.iterations = 6;      /* soft: low stiffness per pass, many passes */
    return w;
}

WubuSpringWorld wubu_spring_rubber(void) {
    WubuSpringWorld w;
    w.damping  = 0.12f;
    w.gravity  = 0.0f;
    w.dt       = 1.0f;
    w.iterations = 12;
    return w;
}

void wubu_spring_step(WubuBlob *blobs, int n_blobs,
                      const WubuSpringLink *links, int n_links,
                      const WubuSpringWorld *w) {
    if (!blobs || n_blobs <= 0) return;

    /* -- 1. verlet integrate -------------------------------------------------
     * x' = x + (x - x_prev) * (1 - damping) + a * dt^2
     * Pinned blobs are anchors: their position is never integrated. */
    float keep = 1.0f - w->damping;
    float dt2  = w->dt * w->dt;
    for (int i = 0; i < n_blobs; i++) {
        WubuBlob *b = &blobs[i];
        if (b->pinned || b->mass <= 0.0f) { b->px = b->x; b->py = b->y; continue; }

        float nx = b->x + (b->x - b->px) * keep;
        float ny = b->y + (b->y - b->py) * keep + w->gravity * dt2;
        b->px = b->x;
        b->py = b->y;
        b->x  = nx;
        b->y  = ny;
    }

    /* -- 2. relax constraints ------------------------------------------------
     * Each pass moves both endpoints toward the rest length, weighted by mass.
     * More passes = stiffer.  This is what makes a springy rim feel soft rather
     * than rigid. */
    for (int it = 0; it < w->iterations; it++) {
        for (int i = 0; i < n_links; i++) {
            const WubuSpringLink *l = &links[i];
            if (l->a < 0 || l->b < 0 || l->a >= n_blobs || l->b >= n_blobs) continue;
            if (l->a == l->b) continue;

            WubuBlob *a = &blobs[l->a];
            WubuBlob *b = &blobs[l->b];

            float dx = b->x - a->x;
            float dy = b->y - a->y;
            float dist2 = dx * dx + dy * dy;
            if (dist2 < 1e-9f) continue;          /* coincident: nothing to project */

            float dist = sqrtf(dist2);
            float err  = (dist - l->rest) / dist; /* fraction of the error */

            /* INVERSE-mass weights.  A light blob must move MORE than a heavy
             * one -- weighting by mass directly (the easy mistake) makes a heavy
             * island drag a light blob around, which reads as wrong.  An
             * immovable endpoint takes none of the correction. */
            float wa = (a->pinned || a->mass <= 0.0f) ? 0.0f : 1.0f / a->mass;
            float wb = (b->pinned || b->mass <= 0.0f) ? 0.0f : 1.0f / b->mass;
            float sum = wa + wb;
            if (sum <= 0.0f) continue;             /* both anchored: rigid link */

            float k = l->stiffness;
            float fa = k * err * (wa / sum);
            float fb = k * err * (wb / sum);

            a->x += dx * fa;
            a->y += dy * fa;
            b->x -= dx * fb;
            b->y -= dy * fb;
        }
    }
}

void wubu_spring_constrain_rect(WubuBlob *blobs, int n_blobs,
                                float min_x, float min_y,
                                float max_x, float max_y,
                                float squish) {
    if (!blobs || n_blobs <= 0) return;
    if (squish < 0.0f) squish = 0.0f;
    if (squish > 1.0f) squish = 1.0f;

    for (int i = 0; i < n_blobs; i++) {
        WubuBlob *b = &blobs[i];
        if (b->pinned || b->mass <= 0.0f) continue;

        /* A blob pressed into an edge may overlap by `squish` of its radius --
         * that overlap is the flat-then-recover shape of the ooze. */
        float slack = b->radius * squish;

        if (b->x < min_x + slack) b->x = min_x + slack;
        if (b->x > max_x - slack) b->x = max_x - slack;
        if (b->y < min_y + slack) b->y = min_y + slack;
        if (b->y > max_y - slack) b->y = max_y - slack;
    }
}

float wubu_spring_dist(const WubuBlob *a, const WubuBlob *b) {
    if (!a || !b) return 0.0f;
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    return sqrtf(dx * dx + dy * dy);
}
