/* wubu_x11_surface.h -- a guest's pixels, before they are anybody's window.
 *
 * The bridge has three layers and this is the middle one:
 *   wire (requests) -> SURFACE (pixels + damage) -> presentation (DosGuiWindow).
 *
 * Keeping the surface separate from the window is what lets a guest be hosted
 * without being granted a compositor: the surface is a plain buffer the bridge
 * owns, and the window is ours. The DOS box already presents `uint8_t *fb` RGBA
 * through dos_window; this produces exactly that shape, so nothing has to be
 * taught a second way to draw.
 */
#ifndef WUBU_X11_SURFACE_H
#define WUBU_X11_SURFACE_H

#include <stddef.h>
#include <stdint.h>

typedef struct WubuX11Surface {
    uint8_t *pixels;      /* RGBA, w*h*4, owned by the caller */
    int      w, h;
    int      dirty_x0, dirty_y0, dirty_x1, dirty_y1;   /* inclusive-exclusive */
} WubuX11Surface;

void wubu_x11_surface_init(WubuX11Surface *s, uint8_t *pixels, int w, int h);
void wubu_x11_surface_clear(WubuX11Surface *s, uint8_t r, uint8_t g, uint8_t b);
void wubu_x11_surface_mark_all(WubuX11Surface *s);
int  wubu_x11_surface_dirty(const WubuX11Surface *s);
/* Presentation consumes the damage: call this after a frame has been copied out,
 * or the next frame redraws everything forever. */
void wubu_x11_surface_clear_damage(WubuX11Surface *s);

/* PutImage, ZPixmap, 32 bits per pixel, in the client's byte order.
 *
 * The wire gives us a rectangle of CARD32 pixels (or bytes for depth 8); this
 * converts to the RGBA the compositor wants and grows the damage rect. Returns 0,
 * or negative when the rectangle does not fit the surface -- refusing is better
 * than drawing half a window and calling it correct. */
int wubu_x11_surface_put_image(WubuX11Surface *s, int x, int y, int w, int h,
                               const uint8_t *data, size_t len, int lsb, int depth);

#endif
