/*
 * wubuglott_geom.c -- 2D geometry for the articulatory model.
 * See wubuglott_geom.h. Slermed from chimera's anatomy/geom.rs.
 */
#include "wubuglott_geom.h"
#include "wubuglott_math.h"

#include <stdlib.h>

/* ---- polyline --------------------------------------------------------- */

void wg_polyline_init(wg_polyline *pl)
{
    pl->xs = NULL; pl->ys = NULL; pl->n = 0; pl->cap = 0;
}

void wg_polyline_free(wg_polyline *pl)
{
    free(pl->xs); free(pl->ys);
    pl->xs = NULL; pl->ys = NULL; pl->n = 0; pl->cap = 0;
}

void wg_polyline_clear(wg_polyline *pl) { pl->n = 0; }

static void polyline_grow(wg_polyline *pl, int need)
{
    if (need <= pl->cap) return;
    int cap = pl->cap ? pl->cap * 2 : 16;
    while (cap < need) cap *= 2;
    pl->xs = realloc(pl->xs, (size_t)cap * sizeof(double));
    pl->ys = realloc(pl->ys, (size_t)cap * sizeof(double));
    pl->cap = cap;
}

void wg_polyline_push(wg_polyline *pl, wg_v2 p)
{
    polyline_grow(pl, pl->n + 1);
    pl->xs[pl->n] = p.x;
    pl->ys[pl->n] = p.y;
    pl->n++;
}

void wg_polyline_append(wg_polyline *pl, const wg_v2 *pts, int n, int skip_first)
{
    int start = skip_first ? 1 : 0;
    if (start > n) start = n;
    for (int i = start; i < n; i++)
        wg_polyline_push(pl, pts[i]);
}

/* ---- ray intersections ------------------------------------------------ */

/* Where segment A->B crosses the line through o along d (as t along d), or
 * NaN. A segment crosses only if its ends lie on different sides of the
 * line, a point on the line counting as the positive side. */
static double crossing_t(double ox, double oy, double dx, double dy,
                         double ax, double ay, double bx, double by)
{
    int sa = (dx * (ay - oy) - dy * (ax - ox)) >= 0.0;
    int sb = (dx * (by - oy) - dy * (bx - ox)) >= 0.0;
    if (sa == sb)
        return NAN;
    double ex = bx - ax, ey = by - ay;
    double den = dx * ey - dy * ex;
    if (fabs(den) < 1e-15)
        return NAN;
    return ((ax - ox) * ey - (ay - oy) * ex) / den;
}

int wg_ray_polyline(double ox, double oy, double dx, double dy,
                    const wg_polyline *pl, double t_min, double t_max,
                    double *out, int out_cap)
{
    int n = 0;
    for (int i = 0; i + 1 < pl->n; i++) {
        double t = crossing_t(ox, oy, dx, dy, pl->xs[i], pl->ys[i],
                              pl->xs[i + 1], pl->ys[i + 1]);
        if (t >= t_min && t <= t_max && n < out_cap)
            out[n++] = t;
    }
    return n;
}

int wg_ray_crossings(double ox, double oy, double dx, double dy,
                     const wg_polyline *pl, double t_min, double t_max,
                     double *out_t, int *exit, int out_cap)
{
    int n = 0;
    for (int i = 0; i + 1 < pl->n; i++) {
        double t = crossing_t(ox, oy, dx, dy, pl->xs[i], pl->ys[i],
                              pl->xs[i + 1], pl->ys[i + 1]);
        if (t >= t_min && t <= t_max && n < out_cap) {
            out_t[n] = t;
            /* leaving the tissue (on the polyline's right) for the air */
            exit[n] = dx * (pl->ys[i + 1] - pl->ys[i])
                    - dy * (pl->xs[i + 1] - pl->xs[i]) < 0.0;
            n++;
        }
    }
    return n;
}

/* ---- curve samplers --------------------------------------------------- */

void wg_bezier3(wg_v2 p0, wg_v2 p1, wg_v2 p2, wg_v2 p3,
                int count, wg_v2 *out)
{
    for (int i = 0; i < count; i++) {
        double t = (double)i / (double)(count - 1);
        double u = 1.0 - t;
        double a = u * u * u;
        double b = 3.0 * u * u * t;
        double c = 3.0 * u * t * t;
        double d = t * t * t;
        out[i] = wg_v2_make(a * p0.x + b * p1.x + c * p2.x + d * p3.x,
                            a * p0.y + b * p1.y + c * p2.y + d * p3.y);
    }
}

void wg_bezier2(wg_v2 p0, wg_v2 p1, wg_v2 p2, int count, wg_v2 *out)
{
    for (int i = 0; i < count; i++) {
        double t = (double)i / (double)(count - 1);
        double u = 1.0 - t;
        out[i] = wg_v2_make(u * u * p0.x + 2.0 * u * t * p1.x + t * t * p2.x,
                            u * u * p0.y + 2.0 * u * t * p1.y + t * t * p2.y);
    }
}

void wg_arc(wg_v2 c, double r, double a0, double a1,
            int count, wg_v2 *out)
{
    for (int i = 0; i < count; i++)
        out[i] = wg_v2_polar(c, r, a0 + ((a1 - a0) * (double)i) / (double)(count - 1));
}

int wg_catmull_rom(const wg_v2 *pts, int n, int per, wg_v2 *out, int out_cap)
{
    if (n < 3) {
        int k = 0;
        for (int i = 0; i < n && k < out_cap; i++)
            out[k++] = pts[i];
        return k;
    }
    int k = 0;
    for (int i = 0; i + 1 < n; i++) {
        wg_v2 p0 = pts[i > 0 ? i - 1 : 0];
        wg_v2 p1 = pts[i];
        wg_v2 p2 = pts[i + 1];
        wg_v2 p3 = pts[(i + 2 < n) ? i + 2 : n - 1];
        for (int j = 0; j < per; j++) {
            if (k >= out_cap) return k;
            double t = (double)j / (double)per;
            double t2 = t * t, t3 = t2 * t;
            out[k++] = wg_v2_make(
                0.5 * (2.0 * p1.x + (-p0.x + p2.x) * t
                       + (2.0 * p0.x - 5.0 * p1.x + 4.0 * p2.x - p3.x) * t2
                       + (-p0.x + 3.0 * p1.x - 3.0 * p2.x + p3.x) * t3),
                0.5 * (2.0 * p1.y + (-p0.y + p2.y) * t
                       + (2.0 * p0.y - 5.0 * p1.y + 4.0 * p2.y - p3.y) * t2
                       + (-p0.y + 3.0 * p1.y - 3.0 * p2.y + p3.y) * t3));
        }
    }
    if (k < out_cap)
        out[k++] = pts[n - 1];
    return k;
}

int wg_tangent_angle(wg_v2 c, double r, wg_v2 p, int upper, double *out_angle)
{
    double d = wg_v2_dist(c, p);
    if (d <= r * 1.0001)
        return 0;
    double base = atan2(p.y - c.y, p.x - c.x);
    double off = acos(r / d);
    *out_angle = upper ? base + off : base - off;
    return 1;
}
