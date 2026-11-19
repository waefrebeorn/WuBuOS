/*
 * gui_gfx.h -- WuBuOS GUI compositing foundation (public API).
 *
 * Design ref: rizonesoft/impossible-os@be8c286 src/kernel/gfx/gfx_core.c,
 * gfx_blend.c, gfx_blur.c (GPL-3.0-only). Ground-up WuBuOS C11
 * implementation; no upstream code copied.
 *
 * ARGB8888 offscreen surfaces + alpha compositing onto the VBE back buffer.
 * The VBE layer (src/kernel/vbe.h) remains the ONE screen-drawing owner:
 * gui_blit_to_vbe* write through vbe_set_pixel/vbe_get_pixel so clipping and
 * the render/present boundary stay intact.
 *
 * Deliberate divergences from the reference:
 *  - Surfaces store STRAIGHT alpha (not premultiplied). The blend does the
 *    multiply; this matches the XRGB8888 world the shell already lives in.
 *  - Box blur CLAMPS to edge pixels instead of shrinking the sample window,
 *    so acrylic borders do not darken.
 *
 * Colour layout: 0xAARRGGBB (alpha high byte, blue low byte).
 */
#ifndef GUI_GFX_H
#define GUI_GFX_H

#include <stdint.h>

#define GUI_ARGB(a, r, g, b) \
    ((uint32_t)(((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | \
                ((uint32_t)(g) << 8) | (uint32_t)(b)))
#define GUI_A(c) (((c) >> 24) & 0xFFu)
#define GUI_R(c) (((c) >> 16) & 0xFFu)
#define GUI_G(c) (((c) >> 8) & 0xFFu)
#define GUI_B(c) ((c) & 0xFFu)

/* Opaque offscreen surface, stride == width. */
typedef struct GuiSurface GuiSurface;

/* Create/destroy. Returns NULL on bad dims or allocation failure. */
GuiSurface *gui_surface_create(int w, int h);
void        gui_surface_destroy(GuiSurface *s);

int         gui_surface_width(const GuiSurface *s);
int         gui_surface_height(const GuiSurface *s);
/* Direct pixel access (ARGB8888, row-major, stride == width). NULL if s NULL. */
uint32_t   *gui_surface_pixels(GuiSurface *s);

void        gui_surface_clear(GuiSurface *s, uint32_t argb);
void        gui_surface_fill_rect(GuiSurface *s, int x, int y, int w, int h,
                                  uint32_t argb);
/* Bounds-checked accessors; out-of-bounds get returns 0, set is a no-op. */
uint32_t    gui_surface_get(const GuiSurface *s, int x, int y);
void        gui_surface_set(GuiSurface *s, int x, int y, uint32_t argb);

/* Anti-aliased filled rounded rectangle (integer 4x4 supersampled corner
 * coverage). radius is clamped to half the smallest dimension. */
void gui_fill_rounded(GuiSurface *s, int x, int y, int w, int h,
                      int radius, uint32_t argb);

/* Separable box blur over a region, clamp-to-edge, O(w*h) per pass.
 * radius 0 or an empty/clipped-out region is a no-op. */
void gui_blur_box(GuiSurface *s, int x, int y, int w, int h, int radius);
/* Three box passes approximate a gaussian (the reference's acrylic trick). */
void gui_blur_gauss_approx(GuiSurface *s, int x, int y, int w, int h,
                           int radius);

/* Composite a surface onto the VBE back buffer at (dx,dy), src-over
 * per-pixel alpha. Honours the active VBE clip. */
void gui_blit_to_vbe(const GuiSurface *s, int dx, int dy);
/* Same, with an extra global alpha (0 = invisible, 255 = unchanged). */
void gui_blit_to_vbe_alpha(const GuiSurface *s, int dx, int dy, uint8_t alpha);

#endif /* GUI_GFX_H */