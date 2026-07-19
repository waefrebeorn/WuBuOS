/*
 * vbe.c  --  WuBuOS VBE Framebuffer Implementation
 *
 * Two modes:
 *   - Kernel mode (default): uses mem_alloc/mem_free from kernel memory.c
 *   - Hosted test mode (VBE_HOSTED defined): uses calloc/free for testing
 *
 * Fable Windowing Agent extensions:
 *   - 64-glyph 8x8 bitmap font (ASCII 32..95) from Mythos Fable
 *   - Clipped primitives, gradient fills, circle, shade
 *   - Software mouse cursor, Win98 title bar, close box
 */

#include "vbe.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* Choose allocator based on build mode */
#ifdef VBE_HOSTED
  #include <stdio.h>
  #define VBE_ALLOC(size) calloc(1, size)
  #define VBE_FREE(ptr)  free(ptr)
  #define VBE_LOG(...)    fprintf(stderr, __VA_ARGS__)
#else
  /* Freestanding bare-metal: NO libc stdio.  Route diagnostics through the
   * real serial klog (already proven working in kernel_main) instead of the
   * nonexistent fprintf/stderr, which would bind to garbage and triple-fault
   * the first time vbe_init runs on real hardware. */
  #include "memory.h"
  #include "klog.h"
  #define VBE_ALLOC(size) mem_alloc(size)
  #define VBE_FREE(ptr)  mem_free(ptr)
  #define VBE_LOG(...)    klog_printf(__VA_ARGS__)
#endif

static VBEState g_vbe = {0};

int vbe_init(int width, int height) {
    VBE_LOG("[DEBUG] vbe_init: width=%d, height=%d\n", width, height);
    g_vbe.width  = width;
    g_vbe.height = height;
    g_vbe.bpp    = 32;
    g_vbe.fb_size = (size_t)width * height * 4;
    VBE_LOG("[DEBUG] vbe_init: fb_size=%zu\n", g_vbe.fb_size);

    g_vbe.fb   = (uint32_t *)VBE_ALLOC(g_vbe.fb_size);
    g_vbe.back = (uint32_t *)VBE_ALLOC(g_vbe.fb_size);
    VBE_LOG("[DEBUG] vbe_init: fb=%p, back=%p\n", (void*)g_vbe.fb, (void*)g_vbe.back);
    if (!g_vbe.fb || !g_vbe.back) {
        VBE_LOG("[DEBUG] vbe_init: allocation failed!\n");
        return -1;
    }

    memset(g_vbe.fb, 0, g_vbe.fb_size);
    memset(g_vbe.back, 0, g_vbe.fb_size);
    return 0;
}

uint32_t *vbe_framebuffer(void) { return g_vbe.fb; }
int       vbe_width(void)       { return g_vbe.width; }
int       vbe_height(void)      { return g_vbe.height; }

void vbe_shutdown(void) {
    if (g_vbe.fb)   VBE_FREE(g_vbe.fb);
    if (g_vbe.back) VBE_FREE(g_vbe.back);
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

/* 3D raised border — theme-agnostic: caller provides colors */
void vbe_3d_raised_colors(int x, int y, int w, int h,
                           uint32_t light, uint32_t face,
                           uint32_t dark, uint32_t darkest) {
    /* Top-left highlight */
    vbe_hline(x, x+w-1, y, light);
    vbe_vline(x, y, y+h-1, light);
    vbe_hline(x+1, x+w-2, y+1, face);
    vbe_vline(x+1, y+1, y+h-2, face);
    /* Bottom-right shadow */
    vbe_hline(x, x+w-1, y+h-1, darkest);
    vbe_vline(x+w-1, y, y+h-1, darkest);
    vbe_hline(x+1, x+w-2, y+h-2, dark);
    vbe_vline(x+w-2, y+1, y+h-2, dark);
}

/* 3D sunken border — theme-agnostic: caller provides colors */
void vbe_3d_sunken_colors(int x, int y, int w, int h,
                           uint32_t light, uint32_t face,
                           uint32_t dark, uint32_t darkest) {
    /* Top-left shadow */
    vbe_hline(x, x+w-1, y, darkest);
    vbe_vline(x, y, y+h-1, darkest);
    vbe_hline(x+1, x+w-2, y+1, dark);
    vbe_vline(x+1, y+1, y+h-2, dark);
    /* Bottom-right highlight */
    vbe_hline(x, x+w-1, y+h-1, light);
    vbe_vline(x+w-1, y, y+h-1, light);
    vbe_hline(x+1, x+w-2, y+h-2, face);
    vbe_vline(x+w-2, y+1, y+h-2, face);
}

/* Legacy Win98-compatible wrappers (for backward compatibility) */
void vbe_3d_raised(int x, int y, int w, int h) {
    vbe_3d_raised_colors(x, y, w, h, 0xFFFFFF, 0xDFDFDF, 0x808080, 0x000000);
}

void vbe_3d_sunken(int x, int y, int w, int h) {
    vbe_3d_sunken_colors(x, y, w, h, 0xFFFFFF, 0xDFDFDF, 0x808080, 0x000000);
}

void vbe_clear(uint32_t color) {
    for (int i = 0; i < g_vbe.width * g_vbe.height; i++)
        g_vbe.back[i] = color;
}

/* ================================================================
 * FABLE WINDOWING AGENT — 8x8 Bitmap Font
 * Ported from Mythos Fable (filipvabrousek/osdev)
 * 64 glyphs: ASCII 32 (space) through 95 (_)
 * ================================================================ */

const uint8_t vbe_font_8x8[64][8] = {
    /* ' ' */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* '!' */ {0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x20, 0},
    /* '"' */ {0x50, 0x50, 0x50, 0x00, 0x00, 0x00, 0x00, 0},
    /* '#' */ {0x50, 0x50, 0xF8, 0x50, 0xF8, 0x50, 0x50, 0},
    /* '$' */ {0x20, 0x78, 0xA0, 0x70, 0x28, 0xF0, 0x20, 0},
    /* '%' */ {0xC8, 0xC8, 0x10, 0x20, 0x40, 0x98, 0x98, 0},
    /* '&' */ {0x60, 0x90, 0xA0, 0x40, 0xA8, 0x90, 0x68, 0},
    /* ''' */ {0x20, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00, 0},
    /* '(' */ {0x10, 0x20, 0x40, 0x40, 0x40, 0x20, 0x10, 0},
    /* ')' */ {0x40, 0x20, 0x10, 0x10, 0x10, 0x20, 0x40, 0},
    /* '*' */ {0x00, 0x50, 0x20, 0xF8, 0x20, 0x50, 0x00, 0},
    /* '+' */ {0x00, 0x20, 0x20, 0xF8, 0x20, 0x20, 0x00, 0},
    /* ',' */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x10, 0x20},
    /* '-' */ {0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00, 0},
    /* '.' */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0},
    /* '/' */ {0x08, 0x08, 0x10, 0x20, 0x40, 0x80, 0x80, 0},
    /* '0' */ {0x70, 0x88, 0x98, 0xA8, 0xC8, 0x88, 0x70, 0},
    /* '1' */ {0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x70, 0},
    /* '2' */ {0x70, 0x88, 0x08, 0x30, 0x40, 0x80, 0xF8, 0},
    /* '3' */ {0xF0, 0x08, 0x08, 0x70, 0x08, 0x08, 0xF0, 0},
    /* '4' */ {0x10, 0x30, 0x50, 0x90, 0xF8, 0x10, 0x10, 0},
    /* '5' */ {0xF8, 0x80, 0xF0, 0x08, 0x08, 0x88, 0x70, 0},
    /* '6' */ {0x30, 0x40, 0x80, 0xF0, 0x88, 0x88, 0x70, 0},
    /* '7' */ {0xF8, 0x08, 0x10, 0x20, 0x40, 0x40, 0x40, 0},
    /* '8' */ {0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70, 0},
    /* '9' */ {0x70, 0x88, 0x88, 0x78, 0x08, 0x10, 0x60, 0},
    /* ':' */ {0x00, 0x60, 0x60, 0x00, 0x60, 0x60, 0x00, 0},
    /* ';' */ {0x00, 0x60, 0x60, 0x00, 0x60, 0x20, 0x40, 0},
    /* '<' */ {0x10, 0x20, 0x40, 0x80, 0x40, 0x20, 0x10, 0},
    /* '=' */ {0x00, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0x00, 0},
    /* '>' */ {0x40, 0x20, 0x10, 0x08, 0x10, 0x20, 0x40, 0},
    /* '?' */ {0x70, 0x88, 0x08, 0x10, 0x20, 0x00, 0x20, 0},
    /* '@' */ {0x70, 0x88, 0xB8, 0xA8, 0xB0, 0x80, 0x78, 0},
    /* 'A' */ {0x70, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88, 0},
    /* 'B' */ {0xF0, 0x88, 0x88, 0xF0, 0x88, 0x88, 0xF0, 0},
    /* 'C' */ {0x70, 0x88, 0x80, 0x80, 0x80, 0x88, 0x70, 0},
    /* 'D' */ {0xF0, 0x88, 0x88, 0x88, 0x88, 0x88, 0xF0, 0},
    /* 'E' */ {0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0xF8, 0},
    /* 'F' */ {0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0x80, 0},
    /* 'G' */ {0x70, 0x88, 0x80, 0xB8, 0x88, 0x88, 0x78, 0},
    /* 'H' */ {0x88, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88, 0},
    /* 'I' */ {0x70, 0x20, 0x20, 0x20, 0x20, 0x20, 0x70, 0},
    /* 'J' */ {0x38, 0x10, 0x10, 0x10, 0x10, 0x90, 0x60, 0},
    /* 'K' */ {0x88, 0x90, 0xA0, 0xC0, 0xA0, 0x90, 0x88, 0},
    /* 'L' */ {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xF8, 0},
    /* 'M' */ {0x88, 0xD8, 0xA8, 0xA8, 0x88, 0x88, 0x88, 0},
    /* 'N' */ {0x88, 0xC8, 0xA8, 0x98, 0x88, 0x88, 0x88, 0},
    /* 'O' */ {0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0},
    /* 'P' */ {0xF0, 0x88, 0x88, 0xF0, 0x80, 0x80, 0x80, 0},
    /* 'Q' */ {0x70, 0x88, 0x88, 0x88, 0xA8, 0x90, 0x68, 0},
    /* 'R' */ {0xF0, 0x88, 0x88, 0xF0, 0xA0, 0x90, 0x88, 0},
    /* 'S' */ {0x78, 0x80, 0x80, 0x70, 0x08, 0x08, 0xF0, 0},
    /* 'T' */ {0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0},
    /* 'U' */ {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0},
    /* 'V' */ {0x88, 0x88, 0x88, 0x88, 0x50, 0x50, 0x20, 0},
    /* 'W' */ {0x88, 0x88, 0x88, 0xA8, 0xA8, 0xA8, 0x50, 0},
    /* 'X' */ {0x88, 0x88, 0x50, 0x20, 0x50, 0x88, 0x88, 0},
    /* 'Y' */ {0x88, 0x88, 0x50, 0x20, 0x20, 0x20, 0x20, 0},
    /* 'Z' */ {0xF8, 0x08, 0x10, 0x20, 0x40, 0x80, 0xF8, 0},
    /* '[' */ {0x70, 0x40, 0x40, 0x40, 0x40, 0x40, 0x70, 0},
    /* '\' */ {0x80, 0x80, 0x40, 0x20, 0x10, 0x08, 0x08, 0},
    /* ']' */ {0x70, 0x10, 0x10, 0x10, 0x10, 0x10, 0x70, 0},
    /* '^' */ {0x20, 0x50, 0x88, 0x00, 0x00, 0x00, 0x00, 0},
    /* '_' */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8},
};

#define GLYPH_ADVANCE 6

void vbe_draw_char(int x, int y, char ch, uint32_t color, int scale) {
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    if (ch < 32 || ch > 95) ch = 32;
    const uint8_t *glyph = vbe_font_8x8[ch - 32];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                if (scale == 1)
                    vbe_set_pixel(x + col, y + row, color);
                else
                    vbe_fill_rect(x + col * scale, y + row * scale,
                                  scale, scale, color);
            }
        }
    }
}

void vbe_draw_text(int x, int y, const char *s, uint32_t color, int scale) {
    for (; *s; s++) {
        vbe_draw_char(x, y, *s, color, scale);
        x += GLYPH_ADVANCE * scale;
    }
}

int vbe_text_width(const char *s, int scale) {
    int n = 0;
    while (s[n]) n++;
    return n * GLYPH_ADVANCE * scale;
}

/* ================================================================
 * FABLE WINDOWING AGENT — Extended Primitives
 * ================================================================ */

int vbe_fill_rect_clip(int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > g_vbe.width)  w = g_vbe.width - x;
    if (y + h > g_vbe.height) h = g_vbe.height - y;
    if (w <= 0 || h <= 0) return 0;
    vbe_fill_rect(x, y, w, h, color);
    return 1;
}

void vbe_shade_rect(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > g_vbe.width)  w = g_vbe.width - x;
    if (y + h > g_vbe.height) h = g_vbe.height - y;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int px = x + col, py = y + row;
            if (px >= 0 && px < g_vbe.width && py >= 0 && py < g_vbe.height)
                g_vbe.back[py * g_vbe.width + px] =
                    (g_vbe.back[py * g_vbe.width + px] >> 1) & 0x7F7F7F;
        }
    }
}

static uint32_t lerp_color(uint32_t a, uint32_t b, int num, int den) {
    int ar = (int)((a >> 16) & 0xFF), ag = (int)((a >> 8) & 0xFF), ab = (int)(a & 0xFF);
    int br = (int)((b >> 16) & 0xFF), bg = (int)((b >> 8) & 0xFF), bb = (int)(b & 0xFF);
    int r = ar + (br - ar) * num / den;
    int g = ag + (bg - ag) * num / den;
    int bl = ab + (bb - ab) * num / den;
    return (uint32_t)((r << 16) | (g << 8) | bl);
}

void vbe_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    int den = h > 1 ? h - 1 : 1;
    for (int row = 0; row < h; row++) {
        uint32_t c = lerp_color(top, bottom, row, den);
        vbe_fill_rect(x, y + row, w, 1, c);
    }
}

void vbe_blend_rect(int x, int y, int w, int h, uint32_t color, int alpha) {
    if (alpha <= 0) return;
    if (alpha > 255) alpha = 255;
    for (int dy = 0; dy < h; dy++) {
        int py = y + dy;
        if (py < 0 || py >= g_vbe.height) continue;
        for (int dx = 0; dx < w; dx++) {
            int px = x + dx;
            if (px < 0 || px >= g_vbe.width) continue;
            uint32_t dst = g_vbe.back[py * g_vbe.width + px];
            g_vbe.back[py * g_vbe.width + px] = lerp_color(dst, color, alpha, 255);
        }
    }
}

void vbe_hgradient(int x, int y, int w, int h, uint32_t left, uint32_t right) {
    int den = w > 1 ? w - 1 : 1;
    for (int col = 0; col < w; col++) {
        uint32_t c = lerp_color(left, right, col, den);
        vbe_fill_rect(x + col, y, 1, h, c);
    }
}

void vbe_fill_circle(int cx, int cy, int r, uint32_t color) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r)
                vbe_set_pixel(cx + dx, cy + dy, color);
}

void vbe_draw_cursor(int mx, int my) {
    static const char *shape[] = {
        "X",
        "XX",
        "X.X",
        "X..X",
        "X...X",
        "X....X",
        "X.....X",
        "X......X",
        "X.......X",
        "X........X",
        "X.....XXXXX",
        "X..X..X",
        "X.X X..X",
        "XX  X..X",
        "X    X..X",
        "     X..X",
        "      X..X",
        "       XX",
    };
    for (int row = 0; row < 18; row++) {
        for (int col = 0; shape[row][col]; col++) {
            char c = shape[row][col];
            if (c == 'X')      vbe_set_pixel(mx + col, my + row, 0x000000);
            else if (c == '.') vbe_set_pixel(mx + col, my + row, 0xFFFFFF);
        }
    }
}

/* ================================================================
 * Rounded Rectangle Primitives (for XP Classic chrome)
 * ================================================================ */

static bool vbe_point_in_circle(int x, int y, int cx, int cy, int r) {
    int dx = x - cx;
    int dy = y - cy;
    return dx * dx + dy * dy <= r * r;
}

void vbe_fill_rect_rounded(int x, int y, int w, int h, int radius, uint32_t color) {
    if (radius <= 0) { vbe_fill_rect(x, y, w, h, color); return; }
    if (radius > w/2) radius = w/2;
    if (radius > h/2) radius = h/2;

    /* Top-left corner */
    for (int dy = 0; dy < radius; dy++) {
        for (int dx = 0; dx < radius; dx++) {
            if (!vbe_point_in_circle(x + dx, y + dy, x + radius - 1, y + radius - 1, radius))
                vbe_set_pixel(x + dx, y + dy, color);
        }
    }
    /* Top-right corner */
    for (int dy = 0; dy < radius; dy++) {
        for (int dx = 0; dx < radius; dx++) {
            if (!vbe_point_in_circle(x + w - radius + dx, y + dy, x + w - radius, y + radius - 1, radius))
                vbe_set_pixel(x + w - radius + dx, y + dy, color);
        }
    }
    /* Bottom-left corner */
    for (int dy = 0; dy < radius; dy++) {
        for (int dx = 0; dx < radius; dx++) {
            if (!vbe_point_in_circle(x + dx, y + h - radius + dy, x + radius - 1, y + h - radius, radius))
                vbe_set_pixel(x + dx, y + h - radius + dy, color);
        }
    }
    /* Bottom-right corner */
    for (int dy = 0; dy < radius; dy++) {
        for (int dx = 0; dx < radius; dx++) {
            if (!vbe_point_in_circle(x + w - radius + dx, y + h - radius + dy, x + w - radius, y + h - radius, radius))
                vbe_set_pixel(x + w - radius + dx, y + h - radius + dy, color);
        }
    }

    /* Center horizontal strips */
    vbe_fill_rect(x + radius, y, w - 2*radius, h, color);
    vbe_fill_rect(x, y + radius, radius, h - 2*radius, color);
    vbe_fill_rect(x + w - radius, y + radius, radius, h - 2*radius, color);
}

void vbe_rect_rounded(int x, int y, int w, int h, int radius, uint32_t color) {
    if (radius <= 0) { vbe_rect(x, y, w, h, color); return; }
    if (radius > w/2) radius = w/2;
    if (radius > h/2) radius = h/2;

    /* Top edge */
    vbe_hline(x + radius, x + w - radius - 1, y, color);
    /* Bottom edge */
    vbe_hline(x + radius, x + w - radius - 1, y + h - 1, color);
    /* Left edge */
    vbe_vline(x, y + radius, y + h - radius - 1, color);
    /* Right edge */
    vbe_vline(x + w - 1, y + radius, y + h - radius - 1, color);

    /* Four corner arcs (midpoint circle algorithm for outline) */
    (void)radius;
    /* Approximate quarter circles using 45-degree step */
    int r = radius;
    for (int i = 0; i <= r; i++) {
        int dx = (int)((r - i) * 0.70710678118);
        int dy = (int)(i * 0.70710678118);
        /* TL */
        vbe_set_pixel(x + radius - 1 - dx, y + radius - 1 - dy, color);
        vbe_set_pixel(x + radius - 1 - dy, y + radius - 1 - dx, color);
        /* TR */
        vbe_set_pixel(x + w - radius + dx, y + radius - 1 - dy, color);
        vbe_set_pixel(x + w - radius + dy, y + radius - 1 - dx, color);
        /* BL */
        vbe_set_pixel(x + radius - 1 - dx, y + h - radius + dy, color);
        vbe_set_pixel(x + radius - 1 - dy, y + h - radius + dx, color);
        /* BR */
        vbe_set_pixel(x + w - radius + dx, y + h - radius + dy, color);
        vbe_set_pixel(x + w - radius + dy, y + h - radius + dx, color);
    }
}

void vbe_3d_raised_rounded_colors(int x, int y, int w, int h, int radius,
                                   uint32_t light, uint32_t face,
                                   uint32_t dark, uint32_t darkest) {
    /* Top-left highlight */
    vbe_hline(x + radius, x + w - radius - 1, y, light);
    vbe_vline(x, y + radius, y + h - radius - 1, light);
    vbe_hline(x + radius, x + w - radius - 1, y + 1, face);
    vbe_vline(x + 1, y + radius, y + h - radius - 1, face);
    /* Bottom-right shadow */
    vbe_hline(x + radius, x + w - radius - 1, y + h - 1, darkest);
    vbe_vline(x + w - 1, y + radius, y + h - radius - 1, darkest);
    vbe_hline(x + radius + 1, x + w - radius - 1, y + h - 2, dark);
    vbe_vline(x + w - 2, y + radius + 1, y + h - radius - 1, dark);
}

void vbe_3d_sunken_rounded_colors(int x, int y, int w, int h, int radius,
                                   uint32_t light, uint32_t face,
                                   uint32_t dark, uint32_t darkest) {
    /* Top-left shadow */
    vbe_hline(x + radius, x + w - radius - 1, y, darkest);
    vbe_vline(x, y + radius, y + h - radius - 1, darkest);
    vbe_hline(x + radius, x + w - radius - 1, y + 1, dark);
    vbe_vline(x + 1, y + radius, y + h - radius - 1, dark);
    /* Bottom-right highlight */
    vbe_hline(x + radius, x + w - radius - 1, y + h - 1, light);
    vbe_vline(x + w - 1, y + radius, y + h - radius - 1, light);
    vbe_hline(x + radius + 1, x + w - radius - 1, y + h - 2, face);
    vbe_vline(x + w - 2, y + radius + 1, y + h - radius - 1, face);
}



void vbe_title_bar(int x, int y, int w, int h, int active) {
    if (active)
        vbe_hgradient(x, y, w, h, 0x0A2A6A, 0x3A6EA5);
    else
        vbe_fill_rect(x, y, w, h, 0x7A7A7A);
}

void vbe_close_box(int x, int y, int active) {
    vbe_fill_rect(x, y, 14, 12, active ? 0xC0392B : 0x909090);
    vbe_rect(x, y, 14, 12, 0x000000);
    vbe_draw_text(x + 5, y + 2, "X", 0xFFFFFF, 1);
}

/* Line drawing using Bresenham's algorithm */
void vbe_line(int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        vbe_set_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

/* Ellipse drawing using midpoint algorithm */
void vbe_ellipse(int cx, int cy, int rx, int ry, uint32_t color) {
    if (rx <= 0 || ry <= 0) return;
    
    int x = 0;
    int y = ry;
    int rx2 = rx * rx;
    int ry2 = ry * ry;
    int two_rx2 = 2 * rx2;
    int two_ry2 = 2 * ry2;
    
    /* Region 1 */
    int p = (int)(ry2 - rx2 * ry + 0.25 * rx2);
    int px = 0;
    int py = two_rx2 * y;
    
    while (px < py) {
        vbe_set_pixel(cx + x, cy + y, color);
        vbe_set_pixel(cx - x, cy + y, color);
        vbe_set_pixel(cx + x, cy - y, color);
        vbe_set_pixel(cx - x, cy - y, color);
        
        x++;
        px += two_ry2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            y--;
            py -= two_rx2;
            p += ry2 + px - py;
        }
    }
    
    /* Region 2 */
    p = (int)(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    
    while (y >= 0) {
        vbe_set_pixel(cx + x, cy + y, color);
        vbe_set_pixel(cx - x, cy + y, color);
        vbe_set_pixel(cx + x, cy - y, color);
        vbe_set_pixel(cx - x, cy - y, color);
        
        y--;
        py -= two_rx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            x++;
            px += two_ry2;
            p += rx2 - py + px;
        }
    }
}
