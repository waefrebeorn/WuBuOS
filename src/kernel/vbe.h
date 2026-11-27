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
    size_t    fb_size;   /* Pixel bytes: width * height * 4 (no row padding) */
    size_t    pitch;     /* Bytes per row (>= width*4). Firmware GOP surfaces
                          * may pad scan lines (PixelsPerScanLine > width);
                          * every row access must stride by THIS, not fb_size
                          * / height. Packed RAM buffers: pitch == width*4. */
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

/* --- Present / compositor seam ----------------------------------- */
void     vbe_set_scanout_phys(uint64_t phys);   /* 0 = RAM buffer / hosted */
uint64_t vbe_scanout_phys(void);
/* Present a damage rect from an arbitrary source buffer with its OWN stride. */
void vbe_present_from(const uint32_t *src, size_t src_pitch,
                      int x, int y, int w, int h);
void vbe_present_rect(int x, int y, int w, int h);   /* back buffer -> scanout */
/* Map the scanout into `virt` for ring 3 so a compositor can draw straight into
 * it. 0 on success (bytes written to *out_bytes); -1 for a RAM/hosted target. */
int  vbe_map_user(uint64_t virt, uint64_t *out_bytes);
void vbe_shutdown(void);

/* Access the allocated framebuffer (XRGB8888) and its dimensions. */
uint32_t *vbe_framebuffer(void);
void vbe_set_render_target(uint32_t *fb, int w, int h);
/* Same contract as vbe_set_render_target, but for surfaces whose rows are
 * padded (GOP/Limine framebuffers): pitch_bytes is the real byte distance
 * between scan lines and must be >= w*4. Returns without touching state if
 * the geometry is invalid. The plain wrapper assumes packed rows (w*4). */
void vbe_set_render_target_pitched(uint32_t *fb, int w, int h,
                                   uint64_t pitch_bytes);
int       vbe_width(void);
int       vbe_height(void);
/* Bochs VBE (DISPI, ports 0x1CE/0x1CF) -- direct LFB mode-set, no BIOS call.
 * present() returns 1 if the interface exists; enable(w,h,bpp) latches a
 * linear-framebuffer mode and returns the LFB physical base (0 on absence).
 * id() returns the raw DISPI ID word (0xB0C0 = present) for diagnostics;
 * read_reg() reads any DISPI register word. */
int      vbe_bochs_present(void);
uint16_t vbe_bochs_id(void);
uint16_t vbe_dispi_read_reg(uint16_t idx);
uint64_t vbe_bochs_enable(int w, int h, int bpp);
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
/* Scanline base for callers that already resolved their bounds. NULL when y
 * is off-surface. Lets a sprite/scanline loop write pixels inline instead of
 * paying the bounds+clip check per pixel through vbe_set_pixel. */
uint32_t *vbe_scanline(int y);
int  vbe_surface_width(void);
/* The buffer vbe_* writes, with its row stride in pixels (pitch/4) and the surface height. */
uint32_t *vbe_draw_surface(int *stride_px, int *height);

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
/* Lower-case glyphs, ASCII 'a'..'z'.  Separate from the table above because the
 * upper-case set stops at '_' (95); mixing them would shift every index. */
extern const uint8_t vbe_font_lower_8x8[26][8];
/* '`', '{', '|', '}', '~' -- the codes the two tables above do not cover. */
extern const uint8_t vbe_font_extra_8x8[5][8];
int vbe_font_extra_index(int ch);

/* Draw a single 8x8 font glyph (5px wide, 7px tall). scale=1 native.
 * The effective size is scale * vbe_text_scale(). */
void vbe_draw_char(int x, int y, char ch, uint32_t color, int scale);

/* Draw a string of glyphs. Advance = 6 * scale * text_scale per character. */
void vbe_draw_text(int x, int y, const char *s, uint32_t color, int scale);

/* Measure string width in pixels (count * 6 * scale * text_scale). */
int vbe_text_width(const char *s, int scale);
/* (ascent + descent) at the current text scale: the line box layout code must reserve. */
int vbe_line_height(void);

/* -- Global UI text scale (accessibility font scaling) -------------
 * Multiplies EVERY glyph drawn through this layer, so one call rescales
 * the whole UI coherently -- including vbe_text_width, which keeps layout
 * maths in sync with what is actually rendered. Set from the settings
 * daemon (a11y.font_scale) via wubu_settings_apply_live(). 1 = normal. */
void vbe_set_text_scale(int scale);
int  vbe_text_scale(void);

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

/* Blend `color` over one pixel with 0..255 coverage. Integer only, so a freestanding
 * rasteriser can antialias text and icons without libm. */
void vbe_blend_pixel_cov(int x, int y, uint32_t color, int cov);

/* Pixels between the `y` passed to vbe_draw_text() and the text baseline. Exposed so the
 * placement can be asserted instead of assumed: a title must keep its optical band. */
int vbe_text_baseline_off(void);

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
