/*
 * wubuglott_geom.h -- 2D geometry for the articulatory model.
 *
 * Slermed from chimera-vocal-synth, crates/chimera/src/anatomy/geom.rs.
 * Builders write into a caller's buffer (cleared first) so per-control-step
 * geometry allocates nothing once warm.
 *
 * C11. Header-only for the scalar helpers; the polyline and the builders
 * that need state live in wubuglott_geom.c.
 */
#ifndef WUBUGLOTT_GEOM_H
#define WUBUGLOTT_GEOM_H

#include <math.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { double x, y; } wg_v2;

static inline wg_v2 wg_v2_make(double x, double y) { wg_v2 r; r.x = x; r.y = y; return r; }
static inline wg_v2 wg_v2_add(wg_v2 a, wg_v2 b)  { return wg_v2_make(a.x + b.x, a.y + b.y); }
static inline wg_v2 wg_v2_sub(wg_v2 a, wg_v2 b)  { return wg_v2_make(a.x - b.x, a.y - b.y); }
static inline wg_v2 wg_v2_scale(wg_v2 a, double s){ return wg_v2_make(a.x * s, a.y * s); }
static inline double wg_v2_len(wg_v2 a)           { return hypot(a.x, a.y); }
static inline double wg_v2_dist(wg_v2 a, wg_v2 b) { return hypot(a.x - b.x, a.y - b.y); }
static inline wg_v2 wg_v2_norm(wg_v2 a)
{
    double l = hypot(a.x, a.y);
    if (l == 0.0 || isnan(l)) l = 1.0;
    return wg_v2_make(a.x / l, a.y / l);
}
static inline wg_v2 wg_v2_lerp(wg_v2 a, wg_v2 b, double t)
{
    return wg_v2_make(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}
static inline wg_v2 wg_v2_polar(wg_v2 c, double r, double ang)
{
    return wg_v2_make(c.x + r * cos(ang), c.y + r * sin(ang));
}
static inline wg_v2 wg_v2_rotate_about(wg_v2 p, wg_v2 c, double ang)
{
    double s = sin(ang), co = cos(ang);
    double dx = p.x - c.x, dy = p.y - c.y;
    return wg_v2_make(c.x + dx * co - dy * s, c.y + dx * s + dy * co);
}

/* Flat polyline for fast intersection. */
typedef struct {
    double *xs, *ys;
    int     n, cap;
} wg_polyline;

void wg_polyline_init(wg_polyline *pl);
void wg_polyline_free(wg_polyline *pl);
void wg_polyline_clear(wg_polyline *pl);
void wg_polyline_push(wg_polyline *pl, wg_v2 p);
void wg_polyline_append(wg_polyline *pl, const wg_v2 *pts, int n, int skip_first);

static inline wg_v2 wg_polyline_point(const wg_polyline *pl, int i)
{
    return wg_v2_make(pl->xs[i], pl->ys[i]);
}

/* All intersections of the ray o + t*d (t in [t_min, t_max]) with a
 * polyline. Writes t into `out` and returns the count. */
int wg_ray_polyline(double ox, double oy, double dx, double dy,
                    const wg_polyline *pl, double t_min, double t_max,
                    double *out, int out_cap);

/* Like wg_ray_polyline, also flagging for each crossing whether the ray
 * leaves the tissue (the polyline's right-hand side) for the air. */
int wg_ray_crossings(double ox, double oy, double dx, double dy,
                     const wg_polyline *pl, double t_min, double t_max,
                     double *out_t, int *exit, int out_cap);

/* Sample a cubic Bezier into `count` points (both ends included). */
void wg_bezier3(wg_v2 p0, wg_v2 p1, wg_v2 p2, wg_v2 p3,
                int count, wg_v2 *out);
/* Sample a quadratic Bezier. */
void wg_bezier2(wg_v2 p0, wg_v2 p1, wg_v2 p2,
                int count, wg_v2 *out);
/* Arc from a0 to a1 (radians, either direction), `count` points. */
void wg_arc(wg_v2 c, double r, double a0, double a1,
            int count, wg_v2 *out);

/* Catmull-Rom spline through `n` points (uniform), `per` samples per
 * segment. Returns the number of points written (n-1)*per + 1. */
int wg_catmull_rom(const wg_v2 *pts, int n, int per, wg_v2 *out, int out_cap);

/* Upper (or lower) tangent point from an external point p to circle (c, r):
 * its angle on the circle, or 0 when there is none (sets *ok to 0). */
int wg_tangent_angle(wg_v2 c, double r, wg_v2 p, int upper, double *out_angle);

#ifdef __cplusplus
}
#endif

#endif /* WUBUGLOTT_GEOM_H */
