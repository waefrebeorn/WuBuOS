/*
 * gfx_internal.h -- shared leaf helpers for the gui/gfx family.
 * Not a public API; only gui_gfx sibling modules include this.
 */
#ifndef GUI_GFX_INTERNAL_H
#define GUI_GFX_INTERNAL_H

#include "gfx.h"

struct GuiSurface {
    uint32_t *px;
    int w, h;
};

/* Exact integer divide-by-255 for n in [0, 65535]:
 * (n + 1 + (n >> 8)) >> 8. One owner for the blend rounding. */
static inline uint32_t gui_div255(uint32_t n)
{
    return (n + 1u + (n >> 8)) >> 8;
}

/* Src-over blend of straight-alpha `src` onto straight-alpha `*dst`.
 * The ONE per-pixel blend owner (gfx_blend.c exposes it to siblings). */
uint32_t gui_blend_pixel_argb(uint32_t dst, uint32_t src);

/* Blend `src` onto surface pixel (x,y), bounds-checked. */
void gui_surface_blend(GuiSurface *s, int x, int y, uint32_t src);

#endif /* GUI_GFX_INTERNAL_H */