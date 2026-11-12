/*
 * wubuglott_geom_test.c -- behavior gate for the articulatory geometry.
 *
 * Not assert-nothing: checks the spline interpolates its control points,
 * the ray/polyline crossings land where a hand-computed crossing is,
 * the Bezier endpoints are exact, and the tangent point exists only outside
 * the circle.
 */
#include "../../egojar.h"

#include "wubuglott_geom.h"
#include "wubuglott_math.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int g_fail;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); g_fail++; } } while (0)

int main(void)
{
    /* ---- 1. vector basics ------------------------------------------ */
    CHECK(wg_v2_dist(wg_v2_make(0, 0), wg_v2_make(3, 4)) == 5.0,
          "3-4-5 distance");
    wg_v2 n = wg_v2_norm(wg_v2_make(0, 0));
    CHECK(n.x == 0.0 && n.y == 0.0, "a zero vector normalises to zero, not NaN");
    CHECK(wg_v2_len(wg_v2_norm(wg_v2_make(3, 4))) == 1.0,
          "a normalised vector has length 1");

    /* ---- 2. catmull_rom interpolates its control points ------------- */
    {
        wg_v2 pts[4] = { wg_v2_make(0, 0), wg_v2_make(1, 2),
                         wg_v2_make(3, 1), wg_v2_make(4, 0) };
        wg_v2 out[64];
        int k = wg_catmull_rom(pts, 4, 8, out, 64);
        CHECK(k == (4 - 1) * 8 + 1, "the spline writes (n-1)*per + 1 points");
        CHECK(out[0].x == 0.0 && out[0].y == 0.0, "the spline starts at the first point");
        CHECK(out[k - 1].x == 4.0 && out[k - 1].y == 0.0,
              "the spline ends at the last point");
        /* the interior control points are hit at the segment boundaries */
        CHECK(fabs(out[8].x - 1.0) < 1e-12 && fabs(out[8].y - 2.0) < 1e-12,
              "the spline passes through the second control point");
        CHECK(fabs(out[16].x - 3.0) < 1e-12 && fabs(out[16].y - 1.0) < 1e-12,
              "the spline passes through the third control point");
        /* Catmull-Rom INTERPOLATES but may overshoot the polygon a little
         * (it is not a Bezier). Check it stays within a sane margin. */
        int bounded = 1;
        for (int i = 0; i < k; i++)
            if (out[i].x < -0.6 || out[i].x > 4.6
                || out[i].y < -0.6 || out[i].y > 2.6)
                bounded = 0;
        CHECK(bounded, "the spline stays near the control polygon");
    }

    /* ---- 3. a short polyline is copied verbatim --------------------- */
    {
        wg_v2 pts[2] = { wg_v2_make(1, 1), wg_v2_make(2, 2) };
        wg_v2 out[8];
        int k = wg_catmull_rom(pts, 2, 4, out, 8);
        CHECK(k == 2, "fewer than 3 points returns them unchanged");
    }

    /* ---- 4. ray/polyline crossings --------------------------------- */
    {
        /* A square from (0,0) to (10,10). A ray from (-5,5) along +x must
         * cross twice: entering at x=0 and leaving at x=10. */
        /* Wound CLOCKWISE, the way the airway walls are (the air is on the
         * polyline's right), so the exit flags mean what they say. */
        wg_polyline pl;
        wg_polyline_init(&pl);
        wg_polyline_push(&pl, wg_v2_make(0, 0));
        wg_polyline_push(&pl, wg_v2_make(0, 10));
        wg_polyline_push(&pl, wg_v2_make(10, 10));
        wg_polyline_push(&pl, wg_v2_make(10, 0));
        wg_polyline_push(&pl, wg_v2_make(0, 0));

        double t[8];
        int k = wg_ray_polyline(-5, 5, 1, 0, &pl, 0.0, 100.0, t, 8);
        CHECK(k == 2, "a ray through a square crosses twice");
        /* the order follows the polyline's segments, not t -- compare as a set */
        int has5 = 0, has15 = 0;
        for (int i = 0; i < k; i++) {
            if (fabs(t[i] - 5.0) < 1e-9) has5 = 1;
            if (fabs(t[i] - 15.0) < 1e-9) has15 = 1;
        }
        CHECK(has5, "the ray crosses the left wall (t=5)");
        CHECK(has15, "the ray crosses the right wall (t=15)");

        /* the exit flags: entering is not an exit, leaving is */
        double tt[8];
        int ex[8];
        int k2 = wg_ray_crossings(-5, 5, 1, 0, &pl, 0.0, 100.0, tt, ex, 8);
        CHECK(k2 == 2, "ray_crossings finds both crossings");
        /* the right wall is segment 2 (found first) and is the exit */
        CHECK(ex[0] == 0, "entering the tissue is not an exit");
        CHECK(ex[1] == 1, "leaving the tissue is an exit");

        /* a ray that misses entirely */
        int k3 = wg_ray_polyline(-5, 50, 1, 0, &pl, 0.0, 100.0, t, 8);
        CHECK(k3 == 0, "a ray that misses finds no crossings");
        wg_polyline_free(&pl);
    }

    /* ---- 5. Bezier endpoints are exact ------------------------------ */
    {
        wg_v2 p0 = wg_v2_make(1, 1), p1 = wg_v2_make(2, 4),
              p2 = wg_v2_make(5, 3), p3 = wg_v2_make(6, 1);
        wg_v2 out[16];
        wg_bezier3(p0, p1, p2, p3, 16, out);
        CHECK(out[0].x == 1.0 && out[0].y == 1.0, "bezier3 starts at p0");
        CHECK(out[15].x == 6.0 && out[15].y == 1.0, "bezier3 ends at p3");

        wg_bezier2(p0, p1, p3, 16, out);
        CHECK(out[0].x == 1.0 && out[0].y == 1.0, "bezier2 starts at p0");
        CHECK(out[15].x == 6.0 && out[15].y == 1.0, "bezier2 ends at p2");
    }

    /* ---- 6. arc sweeps the right angle ------------------------------ */
    {
        wg_v2 out[9];
        wg_arc(wg_v2_make(0, 0), 2.0, 0.0, WG_PI / 2, 9, out);
        CHECK(fabs(out[0].x - 2.0) < 1e-12 && fabs(out[0].y) < 1e-12,
              "the arc starts at angle 0");
        CHECK(fabs(out[8].x) < 1e-12 && fabs(out[8].y - 2.0) < 1e-12,
              "the arc ends at angle pi/2");
    }

    /* ---- 7. tangent point exists only outside the circle ------------ */
    {
        double ang;
        CHECK(wg_tangent_angle(wg_v2_make(0, 0), 1.0, wg_v2_make(2, 0), 1, &ang) == 1,
              "a point outside the circle has a tangent");
        CHECK(fabs(ang - WG_PI / 3.0) < 1e-9,
              "the upper tangent from (2,0) to a unit circle is at 60 degrees");
        CHECK(wg_tangent_angle(wg_v2_make(0, 0), 1.0, wg_v2_make(0.5, 0), 1, &ang) == 0,
              "a point inside the circle has no tangent");
    }

    /* ---- 8. rotate_about -------------------------------------------- */
    {
        wg_v2 p = wg_v2_rotate_about(wg_v2_make(1, 0), wg_v2_make(0, 0), WG_PI / 2);
        CHECK(fabs(p.x) < 1e-12 && fabs(p.y - 1.0) < 1e-12,
              "a quarter turn about the origin sends (1,0) to (0,1)");
    }

    if (g_fail == 0) {
        printf("wubuglott_geom_test: all checks passed\n");
        return 0;
    }
    printf("wubuglott_geom_test: %d FAILED\n", g_fail);
    return 1;
}
