/*
 * vbe.c — My Seed VBE Framebuffer Implementation (hosted)
 */

#include "vbe.h"
#include "memory.h"
#include <stdlib.h>
#include <string.h>

static VBEState g_vbe = {0};

int vbe_init(int width, int height) {
    g_vbe.width  = width;
    g_vbe.height = height;
    g_vbe.bpp    = 32;
    g_vbe.fb_size = (size_t)width * height * 4;

    g_vbe.fb   = (uint32_t *)mem_alloc(g_vbe.fb_size);
    g_vbe.back = (uint32_t *)mem_alloc(g_vbe.fb_size);
    if (!g_vbe.fb || !g_vbe.back) return -1;

    memset(g_vbe.fb, 0, g_vbe.fb_size);
    memset(g_vbe.back, 0, g_vbe.fb_size);
    return 0;
}

void vbe_shutdown(void) {
    if (g_vbe.fb)   mem_free(g_vbe.fb);
    if (g_vbe.back) mem_free(g_vbe.back);
    memset(&g_vbe, 0, sizeof(g_vbe));
}

VBEState *vbe_state(void) { return &g_vbe; }

void vbe_swap(void) {
    memcpy(g_vbe.fb, g_vbe.back, g_vbe.fb_size);
}

void vbe_set_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < g_vbe.width && y >= 0 && y < g_vbe.height)
        g_vbe.back[y * g_vbe.width + x] = color;
}

uint32_t vbe_get_pixel(int x, int y) {
    if (x >= 0 && x < g_vbe.width && y >= 0 && y < g_vbe.height)
        return g_vbe.back[y * g_vbe.width + x];
    return 0;
}

void vbe_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            vbe_set_pixel(x + dx, y + dy, color);
}

void vbe_hline(int x1, int x2, int y, uint32_t color) {
    if (x1 > x2) { int t=x1; x1=x2; x2=t; }
    for (int x = x1; x <= x2; x++) vbe_set_pixel(x, y, color);
}

void vbe_vline(int x, int y1, int y2, uint32_t color) {
    if (y1 > y2) { int t=y1; y1=y2; y2=t; }
    for (int y = y1; y <= y2; y++) vbe_set_pixel(x, y, color);
}

void vbe_rect(int x, int y, int w, int h, uint32_t color) {
    vbe_hline(x, x+w-1, y, color);
    vbe_hline(x, x+w-1, y+h-1, color);
    vbe_vline(x, y, y+h-1, color);
    vbe_vline(x+w-1, y, y+h-1, color);
}

void vbe_3d_raised(int x, int y, int w, int h) {
    vbe_hline(x, x+w-1, y, C_WIN_BORDER_LT);
    vbe_vline(x, y, y+h-1, C_WIN_BORDER_LT);
    vbe_hline(x+1, x+w-2, y+1, 0x00DFDFDF);
    vbe_vline(x+1, y+1, y+h-2, 0x00DFDFDF);
    vbe_hline(x+1, x+w-2, y+h-2, C_WIN_BORDER_DK);
    vbe_vline(x+w-2, y+1, y+h-2, C_WIN_BORDER_DK);
    vbe_hline(x, x+w-1, y+h-1, C_WIN_BORDER_DD);
    vbe_vline(x+w-1, y, y+h-1, C_WIN_BORDER_DD);
}

void vbe_3d_sunken(int x, int y, int w, int h) {
    vbe_hline(x, x+w-1, y, C_WIN_BORDER_DD);
    vbe_vline(x, y, y+h-1, C_WIN_BORDER_DD);
    vbe_hline(x+1, x+w-2, y+1, C_WIN_BORDER_DK);
    vbe_vline(x+1, y+1, y+h-2, C_WIN_BORDER_DK);
    vbe_hline(x+1, x+w-2, y+h-2, 0x00DFDFDF);
    vbe_vline(x+w-2, y+1, y+h-2, 0x00DFDFDF);
    vbe_hline(x, x+w-1, y+h-1, C_WIN_BORDER_LT);
    vbe_vline(x+w-1, y, y+h-1, C_WIN_BORDER_LT);
}

void vbe_clear(uint32_t color) {
    for (int i = 0; i < g_vbe.width * g_vbe.height; i++)
        g_vbe.back[i] = color;
}
