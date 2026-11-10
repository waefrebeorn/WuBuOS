/*
 * wm_slime.h -- the slime seam: screen edges that hold blobs like a soft body.
 *
 * The seam is the screen-edge half of the feature the rim islands belong to.
 * The edge is not a wall, it is a soft, oozy seam: a blob (a launcher, a
 * widget) pressed into it is ABSORBED -- it shrinks and sinks -- and can be
 * dragged along the rim and pulled back out.
 *
 * The module (wubu_slime) had a gate and no caller, same as wubu_rim did.  The
 * WM is now its owner: the seam wraps the screen, the WM renders it, the
 * pointer drags and stores blobs, and the taskbar's pins are mirrored into it.
 *
 * Pure C11, no platform.  The physics is wubu_slime's; this owns the WM glue.
 */
#ifndef WUBU_WM_SLIME_H
#define WUBU_WM_SLIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lifecycle.  init() is called from wm_init; capacity is fixed (8). */
void wm_slime_init(int screen_w, int screen_h);
void wm_slime_shutdown(void);

/*
 * Add a blob to an edge of the screen.  `edge` is 0=top 1=right 2=bottom
 * 3=left, `home_t` its rest position along that edge (0..1), `radius` its size
 * when released, `color` its face, and `exec` the launch command fired when a
 * stored blob is clicked (NULL for none).  Returns the blob index, or -1.
 */
int  wm_slime_add(int edge, float home_t, float radius, uint32_t color,
                  const char *exec);

void wm_slime_clear(void);
int  wm_slime_count(void);

/* Mirror the taskbar's pinned tasks into the seam (the real caller). */
void wm_slime_mirror_pins(void);

/* Advance the physics one frame and draw the blobs. */
void wm_slime_tick(void);
void wm_slime_render(uint32_t *fb, int fb_w, int fb_h);

/*
 * Pointer routing; called from wm_handle_mouse before window and taskbar
 * handling.  Drag moves a blob along its edge; a click stores a released blob
 * (absorb) or releases a stored one (and launches it).
 * Returns true when the event was consumed by a blob.
 */
bool wm_slime_handle_pointer(int x, int y, int btn, int kind);

/* Blob geometry, for tests and inspection. */
bool wm_slime_island_pos(int i, int *x, int *y, int *r);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_WM_SLIME_H */
