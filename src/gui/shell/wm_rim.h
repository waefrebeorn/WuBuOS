/*
 * wm_rim.h -- islands on the focused window's rim.
 *
 * The rim-island surface (wubu_rim) was a runtime module with a gate and no
 * caller: it was linked into RT_OBJS and nothing outside its own test could
 * reach it.  This module is the WM-side owner that makes it a real desktop
 * surface -- the islands live on the focused window's frame, they are drawn
 * by the WM renderer, and the pointer can grab and drag them around the rim.
 *
 * A caller adds an island with a launch command; clicking it launches that
 * app through the same weak launch_app path the taskbar's pinned buttons use.
 * The taskbar's pins are mirrored here, so a pinned app is reachable from the
 * window you are working in and not only from the taskbar.
 *
 * Pure C11, no platform.  The physics is wubu_rim's; this owns the WM glue.
 */
#ifndef WUBU_WM_RIM_H
#define WUBU_WM_RIM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lifecycle.  init() is called from wm_init; capacity is fixed (8). */
void wm_rim_init(int screen_w, int screen_h);
void wm_rim_shutdown(void);

/*
 * Add an island to the focused window's rim.  `t` is 0..1 around the loop
 * (0 = top-left, going clockwise), `radius` its size in pixels, `color` its
 * face, and `exec` the launch command fired on click (NULL for none).
 * Returns the island index, or -1 when full / not initialised.
 */
int  wm_rim_add(float t, float radius, uint32_t color, const char *exec);

void wm_rim_clear(void);
int  wm_rim_count(void);

/* Mirror the taskbar's pinned tasks onto the rim (the real caller). */
void wm_rim_mirror_pins(void);

/* Advance the physics one frame and draw the islands. */
void wm_rim_tick(void);
void wm_rim_render(uint32_t *fb, int fb_w, int fb_h);

/*
 * Pointer routing; called from wm_handle_mouse before window and taskbar
 * handling.  Returns true when the event was consumed by an island.
 */
bool wm_rim_handle_pointer(int x, int y, int btn, int kind);

/* Island geometry, for tests and inspection. */
bool wm_rim_island_pos(int i, int *x, int *y, int *r);
void wm_rim_rect(int *x, int *y, int *w, int *h);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_WM_RIM_H */
