/*
 * gfx_surface.c -- ARGB8888 offscreen surfaces (one concern: storage).
 *
 * Design ref: rizonesoft/impossible-os@be8c286 src/kernel/gfx/gfx_core.c
 * (GPL-3.0-only). Ground-up WuBuOS C11 implementation; no upstream code
 * copied. Allocation is libc malloc here exactly like the GUI's wallpaper
 * decoder (src/gui/shell/wubu_wallpaper.c); a bare-metal port swaps the
 * allocator, not the logic.
 */
#include "gfx_internal.h"

#include <stdlib.h>
#include <string.h>

#define GUI_SURFACE_MAX_DIM 16384

GuiSurface *gui_surface_create(int w, int h)
{
    if (w <= 0 || h <= 0 || w > GUI_SURFACE_MAX_DIM || h > GUI_SURFACE_MAX_DIM)
        return NULL;
    GuiSurface *s = (GuiSurface *)malloc(sizeof *s);
    if (!s) return NULL;
    s->px = (uint32_t *)calloc((size_t)w * (size_t)h, sizeof(uint32_t));
    if (!s->px) { free(s); return NULL; }
    s->w = w;
    s->h = h;
    return s;
}

void gui_surface_destroy(GuiSurface *s)
{
    if (!s) return;
    free(s->px);
    free(s);
}

int gui_surface_width(const GuiSurface *s)  { return s ? s->w : 0; }
int gui_surface_height(const GuiSurface *s) { return s ? s->h : 0; }

uint32_t *gui_surface_pixels(GuiSurface *s) { return s ? s->px : NULL; }

void gui_surface_clear(GuiSurface *s, uint32_t argb)
{
    if (!s) return;
    size_t n = (size_t)s->w * (size_t)s->h;
    for (size_t i = 0; i < n; i++) s->px[i] = argb;
}

void gui_surface_fill_rect(GuiSurface *s, int x, int y, int w, int h,
                           uint32_t argb)
{
    if (!s || w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > s->w ? s->w : x + w;
    int y1 = y + h > s->h ? s->h : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    for (int row = y0; row < y1; row++) {
        uint32_t *p = s->px + (size_t)row * s->w + x0;
        for (int col = x0; col < x1; col++) *p++ = argb;
    }
}

uint32_t gui_surface_get(const GuiSurface *s, int x, int y)
{
    if (!s || x < 0 || y < 0 || x >= s->w || y >= s->h) return 0;
    return s->px[(size_t)y * s->w + x];
}

void gui_surface_set(GuiSurface *s, int x, int y, uint32_t argb)
{
    if (!s || x < 0 || y < 0 || x >= s->w || y >= s->h) return;
    s->px[(size_t)y * s->w + x] = argb;
}

void gui_surface_blend(GuiSurface *s, int x, int y, uint32_t src)
{
    if (!s || x < 0 || y < 0 || x >= s->w || y >= s->h) return;
    size_t i = (size_t)y * s->w + x;
    s->px[i] = gui_blend_pixel_argb(s->px[i], src);
}