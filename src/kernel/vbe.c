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

/* The baked coverage table is DATA: no libm, no allocator, no host call. Including it
 * here is what lets one glue point upgrade every text call site. */
#include "../gui/fonts/wubu_font_text.h"

/* Bitmap glyphs were ~8px tall; the baked face has cap height 9 at 13px with ascent 15.
 * Drawing the baseline 14px below the old top keeps a title in the same optical band. */
#define AA_BASELINE_OFF 14

int vbe_text_baseline_off(void) { return AA_BASELINE_OFF; }
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#ifdef VBE_HOSTED
#  include <stdio.h>
#  include <fcntl.h>   /* for O_RDWR (O_CLOEXEC needs POSIX 2008+) */
#  include <unistd.h>
#  include <sys/ioctl.h>
#  include <linux/fb.h>
#  define VBE_ALLOC(size) calloc(1, size)
#  define VBE_FREE(ptr)  free(ptr)
#  define VBE_LOG(...)    fprintf(stderr, __VA_ARGS__)
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

/* -- Bochs VBE (DISPI) interface: 16-bit port pair 0x1CE/0x1CF -------- *
 * QEMU's std VGA and bochs-clone VBE shims expose linear-framebuffer
 * mode-set entirely through these two ports -- NO int 0x10 BIOS call is
 * needed, so it works from x86-64 long mode (the kernel cannot call the
 * real-mode VBIOS). Enabling the LFB here lets the bare-metal GUI present
 * to the actual panel instead of a dead RAM buffer. Pure-additive: if the
 * interface is absent it returns 0 and the kernel behaves exactly as
 * before (black screen). */
static inline void vbe_dispi_wr(uint16_t idx, uint16_t val) {
    __asm__ __volatile__("outw %w0, %1" :: "a"(idx), "Nd"((uint16_t)0x1CE));
    __asm__ __volatile__("outw %w0, %1" :: "a"(val), "Nd"((uint16_t)0x1CF));
}
static inline uint16_t vbe_dispi_rd(uint16_t idx) {
    __asm__ __volatile__("outw %w0, %1" :: "a"(idx), "Nd"((uint16_t)0x1CE));
    uint16_t v;
    __asm__ __volatile__("inw %1, %0" : "=a"(v) : "Nd"((uint16_t)0x1CF));
    return v;
}
#define VBE_DISPI_ID       0x0u
#define VBE_DISPI_XRES     0x1u
#define VBE_DISPI_YRES     0x2u
#define VBE_DISPI_BPP      0x3u
#define VBE_DISPI_ENABLE   0x4u
#define VBE_DISPI_LFBED    0x41u      /* ENABLE | LFB_ENABLED */
#define VBE_DISPI_CAP      0x0Bu
#define VBE_BOCHS_LFB_PHYS 0xE0000000ull  /* bochs VBE LFB (QEMU std vga) */

int vbe_bochs_present(void) {
    /* ID high bits identify Bochs VBE; the low nibble is the version
     * (0xB0C0=v1.x, 0xB0C5=current QEMU std vga). Mask the version. */
    return (vbe_dispi_rd(VBE_DISPI_ID) & 0xFFF0u) == 0xB0C0u;
}
uint16_t vbe_bochs_id(void) {
    return vbe_dispi_rd(VBE_DISPI_ID);
}
uint16_t vbe_dispi_read_reg(uint16_t idx) {
    return vbe_dispi_rd(idx);
}
/* Enable a linear framebuffer mode directly (no BIOS): set resolution +
 * depth then latch ENABLE|LFB. Returns the LFB physical base, or 0 if the
 * DISPI interface (or the mode) is unavailable. */
uint64_t vbe_bochs_enable(int w, int h, int bpp) {
    if (!vbe_bochs_present()) return 0;
    vbe_dispi_wr(VBE_DISPI_ENABLE, 0);         /* must disable first */
    vbe_dispi_wr(VBE_DISPI_XRES, (uint16_t)w);
    vbe_dispi_wr(VBE_DISPI_YRES, (uint16_t)h);
    vbe_dispi_wr(VBE_DISPI_BPP,  (uint16_t)bpp);
    vbe_dispi_wr(VBE_DISPI_ENABLE, VBE_DISPI_LFBED);  /* enable + linear */
    return VBE_BOCHS_LFB_PHYS;
}

static VBEState g_vbe = {0};

/* Physical base of the real scanout (0 while drawing into RAM). Recorded by
 * whoever attaches the live framebuffer: the compositor seam maps THIS. */
static uint64_t g_scanout_phys = 0;
/* The VMM is metal-only: hosted builds link vbe.c without it, so the mapping
 * seam is weak rather than a hard dependency. */
extern int wubu_vmm_map_user_page(uint64_t virt, uint64_t phys, uint32_t flags)
    __attribute__((weak));

int vbe_init(int width, int height) {
    VBE_LOG("[DEBUG] vbe_init: width=%d, height=%d\n", width, height);
    g_vbe.width  = width;
    g_vbe.height = height;
    g_vbe.bpp    = 32;
    g_vbe.fb_size = (size_t)width * height * 4;
    g_vbe.pitch  = (size_t)width * 4;   /* packed RAM rows */
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

/* -- Present / compositor seam ------------------------------------ */

void vbe_set_scanout_phys(uint64_t phys) { g_scanout_phys = phys; }
uint64_t vbe_scanout_phys(void) { return g_scanout_phys; }

/* Present from ANY buffer: a compositor's client buffers are packed
 * (width * 4) while the scanout may be padded, so the source stride is a
 * parameter and is never assumed to equal the scanout pitch -- one shared
 * stride silently shears the copy on a padded surface. */
void vbe_present_from(const uint32_t *src, size_t src_pitch,
                      int x, int y, int w, int h) {
    VBEState *v = &g_vbe;
    if (!v->fb || !src || v->fb == src) return;
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > v->width)  w = v->width - x;
    if (y + h > v->height) h = v->height - y;
    if (w <= 0 || h <= 0) return;
    for (int row = y; row < y + h; row++) {
        uint32_t *dst = (uint32_t *)((uint8_t *)v->fb + (size_t)row * v->pitch) + x;
        const uint32_t *s = (const uint32_t *)((const uint8_t *)src +
                                               (size_t)row * src_pitch) + x;
        for (int i = 0; i < w; i++) dst[i] = s[i];
    }
}

void vbe_present_rect(int x, int y, int w, int h) {
    VBEState *v = &g_vbe;
    if (!v->back) return;
    vbe_present_from(v->back, v->pitch, x, y, w, h);
}

int vbe_map_user(uint64_t virt, uint64_t *out_bytes) {
    VBEState *v = &g_vbe;
    if (!v->fb || !g_scanout_phys) return -1;   /* RAM/hosted: nothing to map */
    if (!wubu_vmm_map_user_page) return -1;     /* hosted link: no VMM */
    uint64_t bytes = (uint64_t)v->height * v->pitch;
    uint64_t pages = (bytes + 4095u) / 4096u;
    for (uint64_t i = 0; i < pages; i++)
        if (wubu_vmm_map_user_page(virt + i * 4096u,
                                   g_scanout_phys + i * 4096u, 7) != 0)
            return -1;
    if (out_bytes) *out_bytes = pages * 4096u;
    return 0;
}

/* -- Scissor / clip rectangle ------------------------------------- */
void vbe_set_clip(int x, int y, int w, int h) {
    g_vbe.clip_x = x; g_vbe.clip_y = y;
    g_vbe.clip_w = w; g_vbe.clip_h = h;
    g_vbe.clip_enabled = (w > 0 && h > 0);
}
void vbe_reset_clip(void) {
    g_vbe.clip_enabled = 0;
    g_vbe.clip_x = g_vbe.clip_y = g_vbe.clip_w = g_vbe.clip_h = 0;
}
void vbe_get_clip(int *x, int *y, int *w, int *h) {
    if (x) *x = g_vbe.clip_x;
    if (y) *y = g_vbe.clip_y;
    if (w) *w = g_vbe.clip_w;
    if (h) *h = g_vbe.clip_h;
}

void vbe_swap(void) {
    VBEState *v = vbe_state();
    vbe_backend_t *be = (vbe_backend_t *)v->backend;
    if (be && be->present) {
        /* Host owns the buffer; just flush via the backend's present hook. */
        be->present(be);
        return;
    }
    /* No backend: plain back-buffer copy (bare-metal / test).  When the
     * backend mapped fb and back to the same host surface there is nothing
     * to copy -- and a self-memcpy of a 3MB buffer every frame is pure waste. */
    if (v->fb == v->back) return;
    if (v->pitch == (size_t)v->width * 4) {
        memcpy(v->fb, v->back, v->fb_size);
    } else {
        /* Padded rows (GOP surface): copy scan line by scan line so the
         * padding bytes are never treated as pixels. */
        for (int y = 0; y < v->height; y++)
            memcpy((uint8_t *)v->fb + (size_t)y * v->pitch,
                   (uint8_t *)v->back + (size_t)y * v->pitch,
                   (size_t)v->width * 4);
    }
}

/* --- Modular display backend -------------------------------- */

int vbe_set_backend(vbe_backend_type_t type,
                    uint32_t *host_fb, int w, int h,
                    vbe_backend_t *ops) {
    VBEState *v = vbe_state();
    if (!host_fb) return -1;
    /* If already initialized with its own malloc buffers, free them —
     * the host surface replaces them as the render target. */
    if (v->fb && v->fb != host_fb)  VBE_FREE(v->fb);
    if (v->back && v->back != host_fb) VBE_FREE(v->back);
    v->fb   = host_fb;
    v->back = host_fb;   /* render directly into the host surface */
    v->width  = w;
    v->height = h;
    v->bpp    = 32;
    v->fb_size = (size_t)w * h * 4;
    v->pitch  = (size_t)w * 4;
    v->backend = ops;
    if (ops) ops->type = type;
    VBE_LOG("[vbe] backend attached: type=%d %dx%d fb=%p\n", type, w, h, host_fb);
    return 0;
}

/* Retarget the render surface WITHOUT freeing the previous one.
 *
 * The hosted compositor reads the client's SHM buffer on its own schedule
 * (commit, expose, frame callback).  A single shared buffer means the
 * compositor can catch the client mid-render, and because the renderer fills
 * the desktop background row-by-row from the top, that reads as a dark bar
 * across the top of the screen that comes and goes.  The client therefore
 * renders into one SHM buffer while the compositor holds the other; this
 * retargets the render surface each frame.  Unlike vbe_set_backend it never
 * frees -- the buffers belong to the SHM pool. */
void vbe_set_render_target(uint32_t *fb, int w, int h)
{
    vbe_set_render_target_pitched(fb, w, h, (uint64_t)w * 4);
}

/* Pitched variant: the surface's real row stride (GOP PixelsPerScanLine*4)
 * may exceed w*4. Every primitive below strides rows by g_vbe.pitch, so a
 * padded firmware framebuffer renders correctly instead of shearing. */
void vbe_set_render_target_pitched(uint32_t *fb, int w, int h,
                                   uint64_t pitch_bytes)
{
    VBEState *v = vbe_state();
    if (!fb || w <= 0 || h <= 0) return;
    if (pitch_bytes < (uint64_t)w * 4) return;   /* rows would overlap */
    v->fb   = fb;
    v->back = fb;   /* render directly into the host surface */
    v->width  = w;
    v->height = h;
    v->bpp    = 32;
    v->fb_size = (size_t)w * h * 4;
    v->pitch  = (size_t)pitch_bytes;
}

/* Row base for pixel (x,y): stride by the REAL pitch, never width*4. */
static inline uint32_t *vbe_row(int y)
{
    return (uint32_t *)((uint8_t *)g_vbe.back + (size_t)y * g_vbe.pitch);
}

int vbe_probe_native_mode(int *out_w, int *out_h) {
    /* Try framebuffer ioctl (linux/fb.h) for the real display resolution.
     * This works on bare-metal Linux with fbdev, and as a fallback on
     * systems without DRM.  If unavailable, report failure so the caller
     * can use a safe default. */
#ifdef VBE_HOSTED
    int fd = open("/dev/fb0", O_RDWR);
    if (fd >= 0) {
        struct fb_var_screeninfo vinfo;
        if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == 0) {
            close(fd);
            if (out_w) *out_w = vinfo.xres;
            if (out_h) *out_h = vinfo.yres;
            VBE_LOG("[vbe] native mode via fbdev: %dx%d\n", vinfo.xres, vinfo.yres);
            return 0;
        }
        close(fd);
    }
    /* Try Wayland compositor for the native output mode. */
    const char *xdg = getenv("WAYLAND_DISPLAY");
    if (xdg) {
        VBE_LOG("[vbe] Wayland display '%s' active; using compositor mode\n", xdg);
        /* The compositor provides the surface size; the host layer sets it
         * via vbe_set_backend before the first render.  Report success so
         * the caller doesn't clamp to a fallback. */
        if (out_w) *out_w = 0;
        if (out_h) *out_h = 0;
        return 0;
    }
# else
    (void)out_w; (void)out_h;
#endif
    return -1;
}

void vbe_set_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= g_vbe.width || y < 0 || y >= g_vbe.height) return;
    if (g_vbe.clip_enabled) {
        if (x < g_vbe.clip_x || x >= g_vbe.clip_x + g_vbe.clip_w ||
            y < g_vbe.clip_y || y >= g_vbe.clip_y + g_vbe.clip_h)
            return;
    }
    vbe_row(y)[x] = color;
}

uint32_t *vbe_scanline(int y) {
    if (y < 0 || y >= g_vbe.height || !g_vbe.back) return 0;
    return vbe_row(y);
}

int vbe_surface_width(void) { return g_vbe.width; }

/* The surface the vbe_* calls actually write: vbe_row() indexes g_vbe.back with g_vbe.pitch, NOT
 * g_vbe.fb. A renderer that indexes a framebuffer pointer directly (rather than calling vbe_*) must use
 * THIS, or its pixels land in a buffer nothing presents -- which is exactly how the Control Panel window
 * opened with chrome and an empty body. stride_px is pitch/4, which is NOT always width. */
uint32_t *vbe_draw_surface(int *stride_px, int *height)
{
    if (stride_px) *stride_px = (int)(g_vbe.pitch / 4);
    if (height) *height = g_vbe.height;
    return g_vbe.back;
}

uint32_t vbe_get_pixel(int x, int y) {
    if (x >= 0 && x < g_vbe.width && y >= 0 && y < g_vbe.height)
        return vbe_row(y)[x];
    return 0;
}

/* Fill one row span [x, x+w) with `color`, resolving the clip rectangle ONCE
 * and writing two pixels per 64-bit store. The per-pixel helper costs a bounds
 * check plus a call per pixel: a full-surface clear spent ~8M emulated steps
 * that way, which is a third of the whole boot under WEMU. */
static void vbe_fill_span(int x, int y, int w, uint32_t color)
{
    if (w <= 0 || y < 0 || y >= g_vbe.height) return;
    int x1 = x, x2 = x + w;
    if (x1 < 0) x1 = 0;
    if (x2 > g_vbe.width) x2 = g_vbe.width;
    if (g_vbe.clip_enabled) {
        if (y < g_vbe.clip_y || y >= g_vbe.clip_y + g_vbe.clip_h) return;
        if (x1 < g_vbe.clip_x) x1 = g_vbe.clip_x;
        if (x2 > g_vbe.clip_x + g_vbe.clip_w) x2 = g_vbe.clip_x + g_vbe.clip_w;
    }
    if (x1 >= x2) return;
    uint32_t *row = vbe_row(y) + x1;
    int n = x2 - x1;
    /* One `rep stosl` per row. On metal that is ERMS (one instruction, the
     * core streams it); under WEMU it is one decoded instruction with an
     * internal loop. The four-store 64-bit loop this replaced measured 35.9%
     * of the entire boot (1.96M of 5.45M guest steps) on a 1024x768 clear.
     * 4-byte stores never fuse into the 16-byte SSE moves a device window
     * rejects. */
    unsigned long cnt = (unsigned long)n;
    __asm__ volatile ("rep stosl"
                      : "+D"(row), "+c"(cnt)
                      : "a"(color)
                      : "memory");
    (void)row;
}

void vbe_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int dy = 0; dy < h; dy++)
        vbe_fill_span(x, y + dy, w, color);
}

void vbe_hline(int x1, int x2, int y, uint32_t color) {
    if (x1 > x2) { int t=x1; x1=x2; x2=t; }
    vbe_fill_span(x1, y, x2 - x1 + 1, color);
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
    /* Packed target with no clip: the whole surface is one contiguous run, so
     * one `rep stosl` beats one row-fill per scanline. */
    if (g_vbe.back && !g_vbe.clip_enabled &&
        g_vbe.pitch == (size_t)g_vbe.width * 4) {
        uint32_t *p = (uint32_t *)g_vbe.back;
        unsigned long n = (unsigned long)g_vbe.width * (unsigned long)g_vbe.height;
        __asm__ volatile ("rep stosl"
                          : "+D"(p), "+c"(n)
                          : "a"(color)
                          : "memory");
        (void)n;
        return;
    }
    for (int y = 0; y < g_vbe.height; y++)
        vbe_fill_span(0, y, g_vbe.width, color);
}

/* ================================================================
 * FABLE WINDOWING AGENT — 8x8 Bitmap Font
 * Ported from Mythos Fable (filipvabrousek/osdev)
 * 64 glyphs: ASCII 32 (space) through 95 (_)
 * ================================================================ */


#define GLYPH_ADVANCE 6

/* Global UI text scale -- see vbe.h. 1 = native 8x8 glyphs. */
static int g_text_scale = 1;

void vbe_set_text_scale(int scale) {
    g_text_scale = (scale >= 1) ? scale : 1;
}

int vbe_text_scale(void) {
    return g_text_scale;
}

void vbe_draw_char(int x, int y, char ch, uint32_t color, int scale) {
    const uint8_t *glyph;

    /* Lower case is drawn from its own table now.  The old code folded a-z up
     * to A-Z, which made every string in the OS render in capitals -- source
     * strings included, not just typed input. */
    if (ch >= 'a' && ch <= 'z') {
        glyph = vbe_font_lower_8x8[(unsigned char)ch - 'a'];
    } else if (vbe_font_extra_index((unsigned char)ch) >= 0) {
        glyph = vbe_font_extra_8x8[vbe_font_extra_index((unsigned char)ch)];
    } else if (ch >= 32 && ch <= 95) {
        glyph = vbe_font_8x8[(unsigned char)ch - 32];
    } else {
        ch = '?';                       /* unrepresentable: a VISIBLE placeholder, not a silent space */
        glyph = vbe_font_8x8[0];
    }
    int s = scale * g_text_scale;
    if (s < 1) s = 1;
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                if (s == 1)
                    vbe_set_pixel(x + col, y + row, color);
                else
                    vbe_fill_rect(x + col * s, y + row * s,
                                  s, s, color);
            }
        }
    }
}

/* ---- antialiased text from the baked face ----------------------------------------
 * Every glyph in the product came from a 1-bit bitmap table, which is why text reads
 * blocky at any size. The coverage table is baked from a real TrueType face by
 * tools/font_bake.py; this glue upgrades all callers at once instead of asking 300 sites
 * to opt in. Bytes outside the baked range (or a non-unit text scale) fall back to the
 * bitmap path: a glyph we cannot draw must never silently vanish. */
/* Per CHARACTER, not per string: if a line contains one symbol the face cannot draw,
 * falling back for the whole line would make an otherwise clean title blocky again. The
 * chars outside the baked range keep the bitmap glyph at the same pen. */
/* Decode one UTF-8 character. Latin-1 (U+00A0..U+00FF) is what the baked face carries, so the two-byte
 * forms C2 xx / C3 xx are decoded properly; anything wider becomes '?' -- a VISIBLE placeholder, because a
 * silent space is how "café" and "—" became blanks in every string the OS drew. */
static int utf8_next(const unsigned char **p) {
    unsigned char c = **p;
    if (c < 0x80) { (*p)++; return c; }
    if ((c == 0xC2 || c == 0xC3) && (*p)[1] >= 0x80 && (*p)[1] <= 0xBF) {
        int cp = ((c & 0x1F) << 6) | ((*p)[1] & 0x3F);
        *p += 2;
        return cp;
    }
    (*p)++;
    return '?';
}

static int aa_char_ok(unsigned char c) {
    return g_text_scale == 1 && c >= WUBU_FONT_TEXT_FIRST && c <= WUBU_FONT_TEXT_LAST;
}

static const WubuGlyphAA *aa_glyph(unsigned char c) {
    return &g_font_glyphs[c - WUBU_FONT_TEXT_FIRST];
}

static int aa_advance(const WubuGlyphAA *g) {
    return (g->w > 0 ? g->w : (WUBU_FONT_TEXT_PX / 4)) + 1;
}

static int aa_text_width(const char *s) {
    int w = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        int cp = utf8_next(&p);
        unsigned char c = (cp >= 0 && cp <= 0xFF) ? (unsigned char)cp : (unsigned char)'?';
        w += aa_char_ok(c) ? aa_advance(aa_glyph(c)) : GLYPH_ADVANCE * g_text_scale;
    }
    return w;
}

static void aa_draw_text(int x, int y, const char *s, uint32_t color) {
    /* y is the TOP of a bitmap-font line; the golden cap box starts AA_BASELINE_OFF below
     * it, so a title drawn the old way keeps its optical position. */
    const int baseline = y + AA_BASELINE_OFF;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        int cp = utf8_next(&p);
        unsigned char c = (cp >= 0 && cp <= 0xFF) ? (unsigned char)cp : (unsigned char)'?';
        if (!aa_char_ok(c)) {          /* one glyph the face lacks: bitmap, same line */
            vbe_draw_char(x, y, (char)c, color, 1);
            x += GLYPH_ADVANCE * g_text_scale;
            continue;
        }
        const WubuGlyphAA *g = aa_glyph(c);
        if (g->cov) {
            for (int gy = 0; gy < g->h; gy++) {
                const unsigned char *row = g->cov + (size_t)gy * (size_t)g->w;
                for (int gx = 0; gx < g->w; gx++) {
                    int cov = row[gx];
                    if (cov) vbe_blend_pixel_cov(x + g->xoff + gx,
                                                 baseline + g->yoff + gy, color, cov);
                }
            }
        }
        x += aa_advance(g);
    }
}

void vbe_draw_text(int x, int y, const char *s, uint32_t color, int scale) {
    if (scale == 1 && g_text_scale == 1) { aa_draw_text(x, y, s, color); return; }
    int adv = GLYPH_ADVANCE * scale * g_text_scale;
    for (; *s; s++) {
        vbe_draw_char(x, y, *s, color, scale);
        x += adv;
    }
}

int vbe_text_width(const char *s, int scale) {
    if (scale == 1 && g_text_scale == 1) return aa_text_width(s);
    int n = 0;
    while (s[n]) n++;
    return n * GLYPH_ADVANCE * scale * g_text_scale;
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
            if (px >= 0 && px < g_vbe.width && py >= 0 && py < g_vbe.height) {
                uint32_t *rp = vbe_row(py);
                rp[px] = (rp[px] >> 1) & 0x7F7F7F;
            }
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
            uint32_t *row = vbe_row(py);
            row[px] = lerp_color(row[px], color, alpha, 255);
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

/* Integer square root. vbe.c is FREESTANDING: __builtin_sqrt compiled to a call to
 * sqrt(), which the bridge and screenshot gates do not link (-lm absent), so the whole
 * suite went red on my antialiasing commit. A kernel-side rasteriser has no business
 * pulling libm in for a corner arc anyway. */
static uint32_t vbe_isqrt(uint32_t n) {
    uint32_t x = n, y = (n + 1u) / 2u;
    if (n == 0) return 0;
    while (y < x) { x = y; y = (n / y + y) / 2u; }
    return x;
}

/* Integer coverage blend, public: text and icons carry 0..255 coverage and must not
 * pull float (or libm) into a freestanding rasteriser. The float path below stays for
 * the arc rasteriser, which already computes its own coverage. */
void vbe_blend_pixel_cov(int x, int y, uint32_t color, int cov) {
    if (cov <= 0) return;
    if (cov > 255) cov = 255;
    uint32_t dst = vbe_get_pixel(x, y);
    int a = cov, ia = 255 - cov;
    int r = (((color >> 16) & 0xFF) * a + ((dst >> 16) & 0xFF) * ia) / 255;
    int g = (((color >> 8)  & 0xFF) * a + ((dst >> 8)  & 0xFF) * ia) / 255;
    int b = (( color        & 0xFF) * a + ( dst        & 0xFF) * ia) / 255;
    vbe_set_pixel(x, y, ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
}

static void vbe_blend_pixel(int x, int y, uint32_t color, float a) {
    if (x < 0 || y < 0) return;
    uint32_t dst = vbe_get_pixel(x, y);
    int sr = (int)((color >> 16) & 0xff), sg = (int)((color >> 8) & 0xff), sb = (int)(color & 0xff);
    int dr = (int)((dst >> 16) & 0xff), dg = (int)((dst >> 8) & 0xff), db = (int)(dst & 0xff);
    int r = (int)((float)sr * a + (float)dr * (1.0f - a));
    int g = (int)((float)sg * a + (float)dg * (1.0f - a));
    int b = (int)((float)sb * a + (float)db * (1.0f - a));
    vbe_set_pixel(x, y, ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
}

/* ANTIALIASED ROUNDED FILL -- the one owner of "a rounded rectangle".
 *
 * The previous version was 1-bit (a staircase at every radius) AND inverted: outside
 * every corner arc it painted the FILL COLOUR over the background, so a rounded button
 * carried a coloured square halo. Both defects were inherited by every surface that
 * used it, which is why the whole desktop looked boxy while individual widgets were
 * being patched one at a time.
 *
 * Coverage blending, per pixel: a pixel is filled in proportion to how much of it lies
 * inside the arc, and pixels outside contribute nothing at all. Straight edges are
 * filled by plain rects, so only the four radius boxes pay the per-pixel cost. */
static void vbe_fill_rounded_aa_impl(int x, int y, int w, int h, int radius, uint32_t color) {
    if (radius <= 0) { vbe_fill_rect(x, y, w, h, color); return; }
    if (radius > w/2) radius = w/2;
    if (radius > h/2) radius = h/2;

    /* Straight interior: centre band + the two side bands, no per-pixel math. */
    vbe_fill_rect(x + radius, y, w - 2 * radius, h, color);
    vbe_fill_rect(x, y + radius, radius, h - 2 * radius, color);
    vbe_fill_rect(x + w - radius, y + radius, radius, h - 2 * radius, color);

    /* Four corner squares, coverage-blended against whatever is already there. */
    /* All in HALF-PIXEL units: no floats, no libm. The arc centre is (rad, rad) in
     * pixels, i.e. (2*rad, 2*rad) here; a pixel centre (dx,dy) is (2dx+1, 2dy+1). */
    const int r2 = 2 * radius;
    for (int dy = 0; dy < radius; dy++) {
        for (int dx = 0; dx < radius; dx++) {
            int px2 = 2 * dx + 1, py2 = 2 * dy + 1;
            int ax = r2 - px2, ay = r2 - py2;
            uint32_t dhalf = vbe_isqrt((uint32_t)(ax * ax + ay * ay));  /* half-pixels */
            int cov2 = (int)(2 * radius) - (int)dhalf + 1;               /* half-pixel cov */
            if (cov2 <= 0) continue;
            if (cov2 > 2) cov2 = 2;
            float cov = (float)cov2 * 0.5f;
            vbe_blend_pixel(x + dx,             y + dy,             color, cov);
            vbe_blend_pixel(x + w - 1 - dx,     y + dy,             color, cov);
            vbe_blend_pixel(x + dx,             y + h - 1 - dy,     color, cov);
            vbe_blend_pixel(x + w - 1 - dx,     y + h - 1 - dy,     color, cov);
        }
    }
    return;

    /* Top-left corner (legacy path, unreachable) */
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

/* Public owner, unchanged signature: every caller in the tree gets the antialiased
 * implementation without being touched. */
void vbe_fill_rect_rounded(int x, int y, int w, int h, int radius, uint32_t color) {
    vbe_fill_rounded_aa_impl(x, y, w, h, radius, color);
}
