/* gui_blit.h -- the one blit every guest presenter uses.
 *
 * The DOS box taught this tree how to present a guest: an RGBA frame scaled to fit
 * a window rectangle. When the X11 bridge needed the same thing, the honest move was
 * to share the function rather than write a second nearest-neighbour loop -- a second
 * copy is a second set of off-by-one bugs, and the copies drift the moment one of
 * them learns about clipping or alpha.
 *
 * `dst` is an RGBA surface as uint32 in 0x00RRGGBB byte order (the shape both the
 * VBE back buffer and DosGuiWindow use).
 */
#ifndef WUBU_GUI_BLIT_H
#define WUBU_GUI_BLIT_H

#include <stdint.h>

/* Scale `src` (src_w x src_h RGBA bytes) into (dx,dy,dw,dh) of `dst` (dst_w x dst_h
 * uint32 pixels), nearest-neighbour, bounds-guarded in both directions. Returns the
 * number of pixels written, which is 0 for an empty or degenerate rectangle. */
int wubu_blit_rgba(uint32_t *dst, int dst_w, int dst_h,
                   const uint8_t *src, int src_w, int src_h,
                   int dx, int dy, int dw, int dh);

#endif
