/*
 * vbe.h  --  My Seed VBE Framebuffer Graphics
 *
 * Portable C framebuffer with VBE-style API.
 * Real kernel will use VBE BIOS modes; hosted test uses malloc'd buffer.
 *
 * Design: ZealOS/src/System/Gr/GrScreen.ZC
 *
 * Fable Windowing Agent extensions:
 *   - 64-glyph 8x8 bitmap font (ASCII 32..95)
 *   - Clipped primitives, gradient fills, circle, shade
 *   - Software mouse cursor
 *   - Win98 title bar + close box
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
    /* Scissor / clip rectangle (0,0,0,0 = disabled). */
    int       clip_x, clip_y, clip_w, clip_h;
    int       clip_enabled;
    /* --- Modular display backend (agostic framebuffer) --- */
    /* When a host provides a backing surface via vbe_set_backend(),
     * `back` points at that surface and vbe_swap() calls the backend's
     * present hook instead of memcpy. On bare metal the backend is NULL
     * and vbe_swap does a plain back->fb copy. This lets the same VBE
     * rendering code target DRM/KMS dumb buffers, Wayland SHM pools,
     * or a plain malloc'd buffer — resolution-agnostic. */
    void *backend;          /* opaque vbe_backend_t* (see below) */
} VBEState;

/* Init: allocate framebuffer of given size */
int  vbe_init(int width, int height);
void vbe_shutdown(void);

/* Access the allocated framebuffer (XRGB8888) and its dimensions. */
uint32_t *vbe_framebuffer(void);
int       vbe_width(void);
int       vbe_height(void);
/* Shutdown */
void vbe_shutdown(void);

/* Get state */
VBEState *vbe_state(void);

/* Swap front/back buffers (call each frame) */
void vbe_swap(void);

/* --- Modular display backend (resolution-agnostic) ----------------
 * A host-provided backing surface.  When set, `back` aliases the host's
 * surface (no separate allocation, no memcpy on swap).  The optional
 * `present` hook hands the completed frame to the host's compositor
 * (e.g. DRM page-flip or Wayland surface commit).
 *
 * Backends:
 *   VBE_BACKEND_MALLOC  — plain heap (bare-metal / test)
 *   VBE_BACKEND_DRM     — DRM/KMS dumb buffer + page flip
 *   VBE_BACKEND_WAYLAND — Wayland SHM surface (hosted mode)
 * The backend type is a hint; vbe_swap dispatches on the vtable.
 */
typedef enum {
    VBE_BACKEND_MALLOC = 0,
    VBE_BACKEND_DRM     = 1,
    VBE_BACKEND_WAYLAND = 2,
} vbe_backend_type_t;

typedef struct vbe_backend vbe_backend_t;
struct vbe_backend {
    vbe_backend_type_t type;
    void *host_ctx;              /* opaque host context, or NULL */
    void (*present)(vbe_backend_t *be);  /* flush frame to display */
    void (*destroy)(vbe_backend_t *be); /* free host resources */
};

/* Attach a host-provided back buffer (host owns the memory).
 * After this call vbe_framebuffer() returns `host_fb`; the VBE layer
 * renders directly into it every frame via vbe_swap(). */
int  vbe_set_backend(vbe_backend_type_t type,
                     uint32_t *host_fb, int w, int h,
                     vbe_backend_t *ops);

/* --- Resolution auto-detection --------------------------------------- */
/* Probe the connected display for its native mode.  Returns 0 and fills
 * `out_w/out_h` on success; returns -1 if the host cannot report a mode,
 * in which case the caller falls back to a safe default. */
int  vbe_probe_native_mode(int *out_w, int *out_h);

/* -- Drawing Primitives ------------------------------------------- */

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

/* Line drawing */
void vbe_line(int x1, int y1, int x2, int y2, uint32_t color);

/* Ellipse drawing */
void vbe_ellipse(int cx, int cy, int rx, int ry, uint32_t color);

/* 3D raised border — theme-agnostic: caller provides colors */
void vbe_3d_raised_colors(int x, int y, int w, int h,
                           uint32_t light, uint32_t face,
                           uint32_t dark, uint32_t darkest);

/* 3D sunken border — theme-agnostic: caller provides colors */
void vbe_3d_sunken_colors(int x, int y, int w, int h,
                           uint32_t light, uint32_t face,
                           uint32_t dark, uint32_t darkest);

/* Fill entire screen */
void vbe_clear(uint32_t color);

/* -- Fable Windowing Agent: 8x8 Bitmap Font ---------------------- */
/* 64 glyphs, ASCII 32..95. 8 bytes per glyph, bit 7 = leftmost. */
/* Ported from Mythos Fable (filipvabrousek/osdev). */

extern const uint8_t vbe_font_8x8[64][8];

/* Draw a single 8x8 font glyph (5px wide, 7px tall). scale=1 native. */
void vbe_draw_char(int x, int y, char ch, uint32_t color, int scale);

/* Draw a string of glyphs. Advance = 6 * scale per character. */
void vbe_draw_text(int x, int y, const char *s, uint32_t color, int scale);

/* Measure string width in pixels (count * 6 * scale). */
int  vbe_text_width(const char *s, int scale);

/* -- Fable Windowing Agent: Extended Primitives ------------------ */

/* Clipped fill_rect — clips to screen, returns 0 if fully invisible. */
int  vbe_fill_rect_clip(int x, int y, int w, int h, uint32_t color);

/* Shade rectangle: darken existing pixels by 50%. */
void vbe_shade_rect(int x, int y, int w, int h);

/* Vertical gradient fill. */
void vbe_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom);

/* Alpha-blended rectangle: blend `color` over the existing pixels using the
 * given alpha (0 = unchanged, 255 = fully color). Used for soft drop-shadows. */
void vbe_blend_rect(int x, int y, int w, int h, uint32_t color, int alpha);

/* Horizontal gradient fill. */
void vbe_hgradient(int x, int y, int w, int h, uint32_t left, uint32_t right);

/* Filled circle. */
void vbe_fill_circle(int cx, int cy, int r, uint32_t color);

/* Rounded rectangle (for XP-style buttons/chrome) */
void vbe_fill_rect_rounded(int x, int y, int w, int h, int radius, uint32_t color);
void vbe_rect_rounded(int x, int y, int w, int h, int radius, uint32_t color);

/* Theme-agnostic 3D rounded (caller provides colors) */
void vbe_3d_raised_rounded_colors(int x, int y, int w, int h, int radius,
                                   uint32_t light, uint32_t face,
                                   uint32_t dark, uint32_t darkest);
void vbe_3d_sunken_rounded_colors(int x, int y, int w, int h, int radius,
                                   uint32_t light, uint32_t face,
                                   uint32_t dark, uint32_t darkest);

/* Fable-style software mouse cursor (18-row arrow with outline). */
void vbe_draw_cursor(int mx, int my);

/* -- Clipping (scissor) rectangle --------------------------------- */
/* Sets a scissor rect; all subsequent pixel writes are confined to it.
 * Pass x=y=w=h=0 (or call vbe_reset_clip) to disable. Used by the WM to
 * guarantee a window's on_draw content can never bleed outside its frame. */
void vbe_set_clip(int x, int y, int w, int h);
void vbe_reset_clip(void);
void vbe_get_clip(int *x, int *y, int *w, int *h);

/* Win98 gradient title bar (active: navy→blue, inactive: gray flat). */
void vbe_title_bar(int x, int y, int w, int h, int active);

/* Close box (red X button, 14×12). */
void vbe_close_box(int x, int y, int active);

/* -- Win98 Classic Colors ----------------------------------------- */
/* REMOVED: VBE is theme-agnostic. Colors provided by caller (GUI layer). */

#endif /* MYSEED_VBE_H */
