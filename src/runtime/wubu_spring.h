/*
 * wubu_spring.h -- soft-body spring physics for the slime seam.
 *
 * The slime seam is the screen edge behaving like a soft, oozy body: blobs
 * (apps, widgets, islands) pressed into it jiggle, settle, and can be dragged
 * along the rim and pulled back out.  This is the integrator that makes it feel
 * that way.
 *
 * Verlet integration (Jakobsen constraints) rather than force/acceleration:
 *   - unconditionally stable for the stiff, damped springs slime needs
 *   - no explicit velocity state to get out of sync
 *   - constraints are positional, so an over-stretched spring cannot explode
 *
 * Pure C11 over stdint/stdbool/math-free: no platform, no allocator, no state
 * beyond what the caller passes.  Works identically on Linux, Windows and mac.
 */
#ifndef WUBU_SPRING_H
#define WUBU_SPRING_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x, y;        /* position */
    float px, py;      /* previous position (verlet) */
    float radius;      /* collision/squish radius */
    float mass;        /* relative mass; <= 0 means immovable */
    bool  pinned;      /* a pinned blob is an anchor: it never moves */
} WubuBlob;

typedef struct {
    int   a, b;        /* blob indices */
    float rest;        /* rest length */
    float stiffness;   /* 0..1, fraction of the error corrected per step */
} WubuSpringLink;

typedef struct {
    float damping;     /* 0 = no loss (jiggles forever), 1 = critically damped */
    float gravity;     /* downward acceleration, px/step^2 */
    float dt;          /* fixed step; keep it CONSTANT for determinism */
    int   iterations;  /* constraint relaxation passes; more = stiffer */
} WubuSpringWorld;

/* A world tuned for ooze: soft, lossy, jiggly. */
WubuSpringWorld wubu_spring_ooze(void);
/* A world tuned for a firm rubber: quick settle, little jiggle. */
WubuSpringWorld wubu_spring_rubber(void);

/*
 * Advance the world one fixed step.
 *
 * Order per step: verlet integrate every free blob, apply gravity, then relax
 * the spring constraints `iterations` times.  Constraints move both endpoints
 * by mass ratio, so a heavy blob barely moves a light one -- which is what makes
 * an island feel anchored when a small blob pulls on it.
 */
void wubu_spring_step(WubuBlob *blobs, int n_blobs,
                      const WubuSpringLink *links, int n_links,
                      const WubuSpringWorld *w);

/*
 * Keep every blob inside a rect, squishing it against the edge instead of
 * letting it escape.  The seam lives here: a blob pressed into an edge flattens
 * and springs back rather than leaving the screen.
 *
 * `squish` is how far the blob may overlap the edge (its radius scaled), which
 * is what gives the ooze its flat-then-recover shape.
 */
void wubu_spring_constrain_rect(WubuBlob *blobs, int n_blobs,
                                float min_x, float min_y,
                                float max_x, float max_y,
                                float squish);

/* Distance between two blobs (for hit tests). */
float wubu_spring_dist(const WubuBlob *a, const WubuBlob *b);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_SPRING_H */
