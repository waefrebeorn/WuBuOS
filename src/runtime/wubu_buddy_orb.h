/* wubu_buddy_orb.h -- wubu_buddy: a live shader-ready metaball orb.
 *
 * The buddy is not a bitmap and not a hand-modelled mesh: it is a small set of
 * metaballs whose positions and radii evolve continuously, evaluated by the
 * classic field  f(p) = sum r_i^2 / |p - c_i|^2 .  The isosurface f == ISO is
 * what the renderer draws, so the FORM IS THE STATE -- a living AGI is allowed
 * to change shape, and this one does.
 *
 * Opaque type, C11, no bitmaps, no external state. */
#ifndef WUBU_BUDDY_ORB_H
#define WUBU_BUDDY_ORB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WUBU_BUDDY_ORB_MAX_BLOBS 16
#define WUBU_BUDDY_ISO          1.0f     /* isosurface of the metaball field */

typedef struct wubu_buddy_orb wubu_buddy_orb_t;

/* Create an orb with `n` blobs (clamped to WUBU_BUDDY_ORB_MAX_BLOBS). */
wubu_buddy_orb_t *wubu_buddy_orb_create(int n);

void wubu_buddy_orb_destroy(wubu_buddy_orb_t *orb);

/* Advance the form.  `t` is seconds; the shape is a pure function of t, so the
 * same t always yields the same body (deterministic, reproducible). */
void wubu_buddy_orb_set_time(wubu_buddy_orb_t *orb, float t);

int  wubu_buddy_orb_blob_count(const wubu_buddy_orb_t *orb);

/* Blob i centre (xyz) and radius. */
void wubu_buddy_orb_get_blob(const wubu_buddy_orb_t *orb, int i,
                             float out_xyz[3], float *out_r);

/* Field value at a point; > WUBU_BUDDY_ISO means inside the surface. */
float wubu_buddy_orb_field(const wubu_buddy_orb_t *orb, float x, float y, float z);

/* The render space: axis-aligned box that contains the whole orb with margin.
 * Everything the renderer may draw lives in here. */
void wubu_buddy_orb_render_box(const wubu_buddy_orb_t *orb,
                               float out_min[3], float out_max[3]);

/* Cheap seed -> 0..1, used for per-blob phase offsets. Deterministic. */
float wubu_buddy_hash01(uint32_t seed);

#ifdef __cplusplus
}
#endif
#endif /* WUBU_BUDDY_ORB_H */
