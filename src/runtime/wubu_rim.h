/*
 * wubu_rim.h -- islands that live on a window's rim and travel around it.
 *
 * The rim is a closed loop: top edge, right edge, bottom edge, left edge.  An
 * island is a blob pinned to that loop by a spring; its position is a
 * NORMALIZED DISTANCE ALONG THE PERIMETER, so "move it around the rim" is just
 * a number changing, and dragging it means following the pointer's nearest
 * point on the loop.
 *
 * The springs are what make it feel alive rather than mechanical: release an
 * island and it jiggles back toward where you dropped it instead of snapping.
 *
 * Built on wubu_spring; pure C11, no platform.
 */
#ifndef WUBU_RIM_H
#define WUBU_RIM_H

#include <stdbool.h>
#include <stdint.h>

#include "wubu_spring.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    WubuBlob blob;        /* physics body at the island's current point */
    float    t;           /* normalized distance along the rim, 0..1 */
    float    radius;
    bool     dragging;
    bool     visible;     /* an island can be dismissed without freeing it */
} WubuRimIsland;

typedef struct {
    WubuRimIsland *islands;
    int            n, cap;
    float          x, y, w, h;    /* the window rect the rim wraps */

    /* Scratch for the solver (see wubu_slime). */
    WubuBlob       *scratch;
    WubuSpringLink *links;
} WubuRim;

/* A rim around a window rect. */
bool wubu_rim_init(WubuRim *r, int cap, float x, float y, float w, float h);
void wubu_rim_free(WubuRim *r);

/* The rect can move/resize under the islands; their t stays valid. */
void wubu_rim_set_rect(WubuRim *r, float x, float y, float w, float h);

/* Screen point on the rim at normalized distance t (0..1). */
void wubu_rim_position(const WubuRim *r, float t, float *out_x, float *out_y);

/* The rim's perimeter (2*(w+h)). Needed to convert a pixel distance along the
 * loop into a t and back. */
float wubu_rim_perimeter(const WubuRim *r);

/*
 * The normalized distance of the nearest point on the rim to (px, py).
 * This is what turns a pointer drag into rim motion.
 */
float wubu_rim_nearest_t(const WubuRim *r, float px, float py);

/* Add an island at rim position t. Returns its index, or -1. */
int  wubu_rim_add(WubuRim *r, float t, float radius);

void wubu_rim_show(WubuRim *r, int i, bool show);
void wubu_rim_drag(WubuRim *r, int i, float px, float py);
void wubu_rim_drop(WubuRim *r, int i);

/* Step the islands: springs toward their home, on the rim, jiggling. */
void wubu_rim_step(WubuRim *r, const WubuSpringWorld *w);

/* Which island is under a point, or -1. */
int  wubu_rim_hit(const WubuRim *r, float px, float py);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_RIM_H */
