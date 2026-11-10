/*
 * wubu_slime.h -- the slime seam: screen edges that behave like a soft body.
 *
 * The idea (from shoji-bar-4's soft surfaces, pushed further): the screen EDGE
 * is not a wall, it is a soft, oozy seam.  Blobs -- app launchers, widgets,
 * window-rim islands -- can be pressed into it, where they are ABSORBED (they
 * shrink and sink into the edge), dragged along the rim, and pulled back out.
 * Everything jiggles on springs, so the seam feels wet rather than mechanical.
 *
 * The seam is a ring of blobs around the screen.  Each blob has a HOME on the
 * rim and a spring toward it; storing a blob deepens its rest position and
 * shrinks it, releasing it reverses both.  The spring solver (wubu_spring)
 * supplies the ooze.
 *
 * Pure C11, no platform: the same seam runs on Linux, Windows and mac.
 */
#ifndef WUBU_SLIME_H
#define WUBU_SLIME_H

#include <stdbool.h>
#include <stdint.h>

#include "wubu_spring.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Which edge of the screen a blob lives on. */
typedef enum {
    WUBU_EDGE_TOP = 0,
    WUBU_EDGE_RIGHT,
    WUBU_EDGE_BOTTOM,
    WUBU_EDGE_LEFT,
    WUBU_EDGE_COUNT
} WubuEdge;

typedef struct {
    WubuBlob blob;          /* physics body (position is rim-relative) */
    float    home_t;        /* 0..1 along its edge -- where it rests */
    float    radius_out;    /* radius when released                           */
    float    radius_in;     /* radius when absorbed into the seam             */
    float    depth_out;     /* distance from the edge when released           */
    float    depth_in;      /* distance from the edge when absorbed           */
    WubuEdge edge;
    bool     stored;        /* absorbed into the seam                         */
    bool     dragging;      /* under the pointer                              */
} WubuSlimeBlob;

typedef struct {
    WubuSlimeBlob *blobs;
    int            n, cap;
    float          screen_w, screen_h;
    float          margin;      /* seam thickness: how far in a blob may sink */

    /* Scratch for the solver: 2*n bodies (the blobs, then their pinned home
     * anchors) and n links.  Allocated once in init, so stepping allocates
     * nothing -- the seam runs at 60Hz for years. */
    WubuBlob       *scratch;
    WubuSpringLink *links;
} WubuSlime;

/*
 * Create a seam around a screen.  `margin` is the seam thickness; a blob can
 * sink up to its own radius into it.
 */
bool wubu_slime_init(WubuSlime *s, int cap, float screen_w, float screen_h,
                     float margin);
void wubu_slime_free(WubuSlime *s);

/*
 * Add a blob on an edge.  `home_t` is its rest position along the edge (0..1),
 * `out` its distance from the edge when released, `in` how deep it sinks when
 * absorbed, and `radius_out`/`radius_in` the two sizes.
 */
int  wubu_slime_add(WubuSlime *s, WubuEdge edge, float home_t,
                    float out, float in, float radius_out, float radius_in);

/* Absorb a blob into the seam (shrink + sink), or release it back out. */
void wubu_slime_store(WubuSlime *s, int i);
void wubu_slime_release(WubuSlime *s, int i);
void wubu_slime_toggle(WubuSlime *s, int i);

/* Drag a blob (it follows the pointer, constrained to the seam). */
void wubu_slime_drag(WubuSlime *s, int i, float x, float y);
void wubu_slime_drop(WubuSlime *s, int i);

/* Step the seam: springs toward home, constrained to the screen. */
void wubu_slime_step(WubuSlime *s, const WubuSpringWorld *w);

/* The blob under a point (for hit tests), or -1. */
int wubu_slime_hit(const WubuSlime *s, float x, float y);

/* Convert an edge + position-along-it + depth into screen coordinates. */
void wubu_slime_position(const WubuSlime *s, const WubuSlimeBlob *b,
                         float t, float depth, float *out_x, float *out_y);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_SLIME_H */
