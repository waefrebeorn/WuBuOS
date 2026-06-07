/*
 * vbe.h — My Seed VBE Framebuffer Graphics
 *
 * Portable C framebuffer with VBE-style API.
 * Real kernel will use VBE BIOS modes; hosted test uses malloc'd buffer.
 *
 * Design: ZealOS/src/System/Gr/GrScreen.ZC
 */

#ifndef MYSEED_VBE_H
#define MYSEED_VBE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t *fb;        /* Framebuffer pixels (XRGB8888) */
    uint32_t *back;      /* Back buffer (double-buffer) */
    int       width;
    int       height;
    int       bpp;       /* Bits per pixel (always 32) */
    size_t    fb_size;   /* Total bytes: width * height * 4 */
} VBEState;

/* Init: allocate framebuffer of given size */
int  vbe_init(int width, int height);

/* Shutdown */
void vbe_shutdown(void);

/* Get state */
VBEState *vbe_state(void);

/* Swap front/back buffers (call each frame) */
void vbe_swap(void);

/* ── Drawing Primitives ─────────────────────────────────────────── */

void vbe_set_pixel(int x, int y, uint32_t color);
uint32_t vbe_get_pixel(int x, int y);

/* Filled rectangle */
void vbe_fill_rect(int x, int y, int w, int h, uint32_t color);

/* Horizontal line */
void vbe_hline(int x1, int x2, int y, uint32_t color);

/* Vertical line */
void vbe_vline(int x, int y1, int y2, uint32_t color);

/* Rectangle outline */
void vbe_rect(int x, int y, int w, int h, uint32_t color);

/* 3D raised border (Win98 style) */
void vbe_3d_raised(int x, int y, int w, int h);

/* 3D sunken border */
void vbe_3d_sunken(int x, int y, int w, int h);

/* Fill entire screen */
void vbe_clear(uint32_t color);

/* ── Win98 Classic Colors ───────────────────────────────────────── */

#define C_WIN_DESKTOP   0x00808080
#define C_WIN_FACE      0x00C0C0C0
#define C_WIN_TITLE     0x00000080
#define C_WIN_TITLE_INA 0x00808080
#define C_WIN_TITLE_FG  0x00FFFFFF
#define C_WIN_BORDER_LT 0x00FFFFFF
#define C_WIN_BORDER_DK 0x00808080
#define C_WIN_BORDER_DD 0x00000000
#define C_WIN_TEXT       0x00000000
#define C_WIN_HILITE    0x00000080

#endif /* MYSEED_VBE_H */
