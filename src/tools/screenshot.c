/*
 * wubu_screenshot.c — WuBuOS Screenshot/Snipping Tool Implementation
 *
 * PPM/BMP/PNG capture, GIF animation, in-OS snipping widget.
 */

#include "screenshot.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_wm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* ──────────────────────────────────────────────────────────────────
 *  PPM Writer (no dependencies)
 * ────────────────────────────────────────────────────────────────── */

static int write_ppm_internal(const char *path, const uint32_t *buf, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t c = buf[y * w + x];
            fputc((c >> 16) & 0xFF, f);  /* R */
            fputc((c >> 8) & 0xFF, f);   /* G */
            fputc(c & 0xFF, f);          /* B */
        }
    }
    fclose(f);
    return 0;
}

int wubu_write_ppm(const char *path, const uint32_t *buf, int w, int h) {
    return write_ppm_internal(path, buf, w, h);
}

/* ──────────────────────────────────────────────────────────────────
 *  BMP Writer (no dependencies)
 * ────────────────────────────────────────────────────────────────── */

#pragma pack(push, 1)
typedef struct {
    uint16_t type;
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} BmpFileHeader;

typedef struct {
    uint32_t header_size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_ppm;
    int32_t  y_ppm;
    uint32_t colors_used;
    uint32_t colors_important;
} BmpInfoHeader;
#pragma pack(pop)

static int write_bmp_internal(const char *path, const uint32_t *buf, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    /* Each row must be 4-byte aligned */
    int row_size = ((w * 3 + 3) / 4) * 4;
    int image_size = row_size * h;

    BmpFileHeader fh = {0};
    fh.type = 0x4D42;  /* 'BM' */
    fh.file_size = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader) + image_size;
    fh.offset = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader);

    BmpInfoHeader ih = {0};
    ih.header_size = sizeof(BmpInfoHeader);
    ih.width = w;
    ih.height = -h;  /* Negative = top-down */
    ih.planes = 1;
    ih.bpp = 24;
    ih.image_size = image_size;

    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);

    /* Write pixel data (bottom-up for BMP, but we use negative height so top-down) */
    uint8_t padding[row_size];
    memset(padding, 0, row_size);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t c = buf[y * w + x];
            fputc(c & 0xFF, f);        /* B */
            fputc((c >> 8) & 0xFF, f); /* G */
            fputc((c >> 16) & 0xFF, f); /* R */
        }
        int pad = row_size - w * 3;
        if (pad > 0) fwrite(padding, pad, 1, f);
    }

    fclose(f);
    return 0;
}

int wubu_write_bmp(const char *path, const uint32_t *buf, int w, int h) {
    return write_bmp_internal(path, buf, w, h);
}

/* ──────────────────────────────────────────────────────────────────
 *  PNG Writer (stb_image_write fallback)
 * ────────────────────────────────────────────────────────────────── */

#ifdef WUBU_USE_STB_IMAGE_WRITE
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

int wubu_write_png(const char *path, const uint32_t *buf, int w, int h) {
#ifdef WUBU_USE_STB_IMAGE_WRITE
    int ret = stbi_write_png(path, w, h, 4, buf, w * 4);
    return ret ? 0 : -1;
#else
    /* Fallback: write PPM and note it's not PNG */
    char ppm_path[512];
    snprintf(ppm_path, sizeof(ppm_path), "%s.ppm", path);
    write_ppm_internal(ppm_path, buf, w, h);
    return -1;
#endif
}

/* ──────────────────────────────────────────────────────────────────
 *  Screenshot Functions
 * ────────────────────────────────────────────────────────────────── */

int wubu_shot_fullscreen(const char *path, WubuShotFormat fmt) {
    VBEState *vbe = vbe_state();
    if (!vbe || !vbe->fb) return -1;

    uint32_t *copy = malloc(vbe->width * vbe->height * sizeof(uint32_t));
    if (!copy) return -1;
    memcpy(copy, vbe->fb, vbe->width * vbe->height * sizeof(uint32_t));

    int ret = -1;
    switch (fmt) {
        case SHOT_FMT_PPM:  ret = write_ppm_internal(path, copy, vbe->width, vbe->height); break;
        case SHOT_FMT_BMP:  ret = write_bmp_internal(path, copy, vbe->width, vbe->height); break;
        case SHOT_FMT_PNG:  ret = wubu_write_png(path, copy, vbe->width, vbe->height); break;
    }

    free(copy);
    return ret;
}

int wubu_shot_region(const char *path, int x, int y, int w, int h, WubuShotFormat fmt) {
    VBEState *vbe = vbe_state();
    if (!vbe || !vbe->fb) return -1;

    /* Clamp to framebuffer */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > vbe->width)  w = vbe->width - x;
    if (y + h > vbe->height) h = vbe->height - y;
    if (w <= 0 || h <= 0) return -1;

    uint32_t *copy = malloc(w * h * sizeof(uint32_t));
    if (!copy) return -1;

    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            copy[dy * w + dx] = vbe->fb[(y + dy) * vbe->width + (x + dx)];
        }
    }

    int ret = -1;
    switch (fmt) {
        case SHOT_FMT_PPM:  ret = write_ppm_internal(path, copy, w, h); break;
        case SHOT_FMT_BMP:  ret = write_bmp_internal(path, copy, w, h); break;
        case SHOT_FMT_PNG:  ret = wubu_write_png(path, copy, w, h); break;
    }

    free(copy);
    return ret;
}

extern WubuWM g_wm;

int wubu_shot_window(const char *path, int win_id, WubuShotFormat fmt) {
    WubuWin *win = wubu_wm_find(win_id);
    if (!win) return -1;
    return wubu_shot_region(path, win->x, win->y, win->w, win->h, fmt);
}

/* ──────────────────────────────────────────────────────────────────
 *  GIF Recorder (simple frame accumulation)
 * ────────────────────────────────────────────────────────────────── */

static WubuGifRecorder g_gif = {0};

int wubu_gif_start(const char *path, int w, int h, int delay_ms, int max_frames) {
    if (g_gif.active) return -1;

    strncpy(g_gif.output_path, path, sizeof(g_gif.output_path) - 1);
    g_gif.frame_w = w;
    g_gif.frame_h = h;
    g_gif.delay_ms = delay_ms;
    g_gif.max_frames = max_frames;
    g_gif.frame_count = 0;
    g_gif.frame_idx = 0;

    size_t total_pixels = (size_t)w * h * max_frames;
    g_gif.frame_buffer = calloc(total_pixels, sizeof(uint32_t));
    if (!g_gif.frame_buffer) return -1;

    g_gif.active = true;
    return 0;
}

int wubu_gif_add_frame(int x, int y, int w, int h) {
    if (!g_gif.active) return -1;
    if (g_gif.frame_count >= g_gif.max_frames) return -1;

    VBEState *vbe = vbe_state();
    if (!vbe || !vbe->fb) return -1;

    /* Clamp */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > vbe->width)  w = vbe->width - x;
    if (y + h > vbe->height) h = vbe->height - y;
    if (w <= 0 || h <= 0) return -1;

    uint32_t *dst = &g_gif.frame_buffer[g_gif.frame_idx * g_gif.frame_w * g_gif.frame_h];

    /* Clear frame */
    for (int i = 0; i < g_gif.frame_w * g_gif.frame_h; i++) dst[i] = 0;

    /* Copy captured region into frame (centered) */
    int cx = (g_gif.frame_w - w) / 2;
    int cy = (g_gif.frame_h - h) / 2;
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            dst[(cy + dy) * g_gif.frame_w + (cx + dx)] = vbe->fb[(y + dy) * vbe->width + (x + dx)];
        }
    }

    g_gif.frame_count++;
    g_gif.frame_idx = g_gif.frame_count;
    return 0;
}

int wubu_gif_stop(void) {
    if (!g_gif.active) return -1;
    if (g_gif.frame_count == 0) {
        free(g_gif.frame_buffer);
        g_gif.active = false;
        return 0;
    }

    /* Simple GIF would need encoder - for now just save frames as PPM sequence */
    for (int i = 0; i < g_gif.frame_count; i++) {
        char frame_path[512];
        snprintf(frame_path, sizeof(frame_path), "%s.frame%03d.ppm", g_gif.output_path, i);
        uint32_t *frame = &g_gif.frame_buffer[i * g_gif.frame_w * g_gif.frame_h];
        write_ppm_internal(frame_path, frame, g_gif.frame_w, g_gif.frame_h);
    }

    free(g_gif.frame_buffer);
    g_gif.frame_buffer = NULL;
    g_gif.active = false;
    return 0;
}

/* ──────────────────────────────────────────────────────────────────
 *  Snipping Tool Widget
 * ────────────────────────────────────────────────────────────────── */

static WubuSnipTool g_snip = {0};

int wubu_snip_tool_init(void) {
    memset(&g_snip, 0, sizeof(g_snip));
    g_snip.mode = SNIP_MODE_RECTANGLE;
    g_snip.overlay_buffer = calloc(1920 * 1080, sizeof(uint32_t));
    return g_snip.overlay_buffer ? 0 : -1;
}

void wubu_snip_tool_shutdown(void) {
    if (g_snip.overlay_buffer) {
        free(g_snip.overlay_buffer);
        g_snip.overlay_buffer = NULL;
    }
}

void wubu_snip_tool_activate(WubuSnipMode mode) {
    g_snip.mode = mode;
    g_snip.active = true;
    g_snip.selecting = false;
}

void wubu_snip_tool_deactivate(void) {
    g_snip.active = false;
    g_snip.selecting = false;
}

void wubu_snip_tool_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!g_snip.active || !g_snip.selecting) return;

    /* Draw selection rectangle overlay */
    int x1 = g_snip.start_x < g_snip.end_x ? g_snip.start_x : g_snip.end_x;
    int y1 = g_snip.start_y < g_snip.end_y ? g_snip.start_y : g_snip.end_y;
    int x2 = g_snip.start_x > g_snip.end_x ? g_snip.start_x : g_snip.end_x;
    int y2 = g_snip.start_y > g_snip.end_y ? g_snip.start_y : g_snip.end_y;
    int w = x2 - x1;
    int h = y2 - y1;

    if (w <= 1 || h <= 1) return;

    /* Draw marching ants border */
    uint32_t ant_color = 0xFFFF0000;
    static int ant_offset = 0;

    for (int i = x1; i < x2; i += 4) {
        int pos = (i + ant_offset) % 8;
        fb[y1 * fb_w + i] = (pos < 4) ? ant_color : fb[y1 * fb_w + i];
        fb[y2 * fb_w + i] = (pos < 4) ? ant_color : fb[y2 * fb_w + i];
    }
    for (int i = y1; i < y2; i += 4) {
        int pos = (i + ant_offset) % 8;
        fb[i * fb_w + x1] = (pos < 4) ? ant_color : fb[i * fb_w + x1];
        fb[i * fb_w + x2] = (pos < 4) ? ant_color : fb[i * fb_w + x2];
    }

    /* Dim outside area */
    uint32_t dim = 0x80000000;
    for (int y = 0; y < y1; y++) {
        for (int x = 0; x < fb_w; x++) fb[y * fb_w + x] = (fb[y * fb_w + x] & 0x00FFFFFF) | dim;
    }
    for (int y = y2; y < fb_h; y++) {
        for (int x = 0; x < fb_w; x++) fb[y * fb_w + x] = (fb[y * fb_w + x] & 0x00FFFFFF) | dim;
    }
    for (int y = y1; y < y2; y++) {
        for (int x = 0; x < x1; x++) fb[y * fb_w + x] = (fb[y * fb_w + x] & 0x00FFFFFF) | dim;
        for (int x = x2; x < fb_w; x++) fb[y * fb_w + x] = (fb[y * fb_w + x] & 0x00FFFFFF) | dim;
    }

    ant_offset = (ant_offset + 1) % 8;
}

bool wubu_snip_tool_handle_mouse(int x, int y, int btn, int kind) {
    if (!g_snip.active) return false;

    if (kind == 1) {  /* Mouse down */
        g_snip.selecting = true;
        g_snip.start_x = x;
        g_snip.start_y = y;
        g_snip.end_x = x;
        g_snip.end_y = y;
        return true;
    } else if (kind == 0 && g_snip.selecting) {  /* Mouse move */
        g_snip.end_x = x;
        g_snip.end_y = y;
        return true;
    } else if (kind == 2 && g_snip.selecting) {  /* Mouse up */
        g_snip.selecting = false;
        g_snip.end_x = x;
        g_snip.end_y = y;
        return true;
    }
    return false;
}

int wubu_snip_tool_save(const char *path, WubuShotFormat fmt) {
    if (!g_snip.active) return -1;

    int x1 = g_snip.start_x < g_snip.end_x ? g_snip.start_x : g_snip.end_x;
    int y1 = g_snip.start_y < g_snip.end_y ? g_snip.start_y : g_snip.end_y;
    int x2 = g_snip.start_x > g_snip.end_x ? g_snip.start_x : g_snip.end_x;
    int y2 = g_snip.start_y > g_snip.end_y ? g_snip.start_y : g_snip.end_y;
    int w = x2 - x1;
    int h = y2 - y1;

    if (w <= 1 || h <= 1) return -1;

    g_snip.active = false;
    return wubu_shot_region(path, x1, y1, w, h, fmt);
}

/* Test accessors */
WubuSnipTool *wubu_snip_tool_state(void) { return &g_snip; }
WubuGifRecorder *wubu_gif_recorder_state(void) { return &g_gif; }