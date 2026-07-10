#include "wubu_screenshot.h"
#include "dosgui_wm.h"
#include "wubu_theme.h"
#include "wubu_notify.h"
#include "../hosted/hosted.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>
#include <math.h>
#include <zlib.h>

/* ============================================================
 * Internal State
 * ============================================================ */
static char g_screenshot_dir[PATH_MAX];
static wubu_region_selector_t *g_region_selector = NULL;
static wubu_sshot_t *g_region_result = NULL;
static void (*g_region_callback)(int, int, int, int, void*) = NULL;
static void *g_region_userdata = NULL;

/* ============================================================
 * Region Selector Implementation
 * ============================================================ */
struct wubu_region_selector {
    int screen_w, screen_h;
    int start_x, start_y;
    int current_x, current_y;
    bool dragging;
    void (*on_complete)(int x, int y, int w, int h, void *user);
    void *user_data;
    uint32_t *overlay_buffer;
};

wubu_region_selector_t *wubu_region_selector_begin(int screen_w, int screen_h,
                                                    void (*on_complete)(int x, int y, int w, int h, void *user),
                                                    void *user) {
    wubu_region_selector_t *sel = calloc(1, sizeof(wubu_region_selector_t));
    if (!sel) return NULL;
    sel->screen_w = screen_w;
    sel->screen_h = screen_h;
    sel->on_complete = on_complete;
    sel->user_data = user;
    sel->overlay_buffer = calloc(screen_w * screen_h, sizeof(uint32_t));
    return sel;
}

void wubu_region_selector_end(wubu_region_selector_t *sel) {
    if (sel) {
        free(sel->overlay_buffer);
        free(sel);
    }
}

void wubu_region_selector_mouse(wubu_region_selector_t *sel, int x, int y, bool down) {
    if (!sel) return;
    if (down) {
        if (!sel->dragging) {
            sel->start_x = sel->current_x = x;
            sel->start_y = sel->current_y = y;
            sel->dragging = true;
        }
    } else {
        if (sel->dragging) {
            sel->dragging = false;
            int x1 = sel->start_x < sel->current_x ? sel->start_x : sel->current_x;
            int y1 = sel->start_y < sel->current_y ? sel->start_y : sel->current_y;
            int x2 = sel->start_x > sel->current_x ? sel->start_x : sel->current_x;
            int y2 = sel->start_y > sel->current_y ? sel->start_y : sel->current_y;
            int w = x2 - x1;
            int h = y2 - y1;
            if (w > 10 && h > 10 && sel->on_complete) {
                sel->on_complete(x1, y1, w, h, sel->user_data);
            }
        }
    }
    if (sel->dragging) {
        sel->current_x = x;
        sel->current_y = y;
    }
}

static void draw_rect_simple(uint32_t *buf, int w, int h, int x1, int y1, int x2, int y2, uint32_t color, int thickness) {
    int left = x1 < x2 ? x1 : x2;
    int right = x1 > x2 ? x1 : x2;
    int top = y1 < y2 ? y1 : y2;
    int bottom = y1 > y2 ? y1 : y2;

    for (int t = 0; t < thickness; t++) {
        int l = left + t, r = right - t, tp = top + t, b = bottom - t;
        for (int x = l; x <= r; x++) {
            if (x >= 0 && x < w) {
                if (tp >= 0 && tp < h) buf[tp * w + x] = color;
                if (b >= 0 && b < h) buf[b * w + x] = color;
            }
        }
        for (int y = tp; y <= b; y++) {
            if (y >= 0 && y < h) {
                if (l >= 0 && l < w) buf[y * w + l] = color;
                if (r >= 0 && r < w) buf[y * w + r] = color;
            }
        }
    }
}

void wubu_region_selector_render(wubu_region_selector_t *sel, uint32_t *fb, int fb_w, int fb_h) {
    if (!sel || !fb || !sel->dragging) return;
    if (fb_w != sel->screen_w || fb_h != sel->screen_h) return;

    int x1 = sel->start_x < sel->current_x ? sel->start_x : sel->current_x;
    int y1 = sel->start_y < sel->current_y ? sel->start_y : sel->current_y;
    int x2 = sel->start_x > sel->current_x ? sel->start_x : sel->current_x;
    int y2 = sel->start_y > sel->current_y ? sel->start_y : sel->current_y;

    const WubuThemeColors *tc = wubu_theme_colors();

    /* Dim the screen */
    for (int y = 0; y < fb_h; y++) {
        for (int x = 0; x < fb_w; x++) {
            uint32_t p = fb[y * fb_w + x];
            uint8_t a = p >> 24;
            uint8_t r = (p >> 16) & 0xFF;
            uint8_t g = (p >> 8) & 0xFF;
            uint8_t b = p & 0xFF;
            r = (r * 3) / 4;
            g = (g * 3) / 4;
            b = (b * 3) / 4;
            fb[y * fb_w + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    /* Brighten selection region */
    for (int y = y1; y <= y2 && y < fb_h; y++) {
        for (int x = x1; x <= x2 && x < fb_w; x++) {
            uint32_t p = fb[y * fb_w + x];
            uint8_t a = p >> 24;
            uint8_t r = (p >> 16) & 0xFF;
            uint8_t g = (p >> 8) & 0xFF;
            uint8_t b = p & 0xFF;
            r = r < 255 - 64 ? r + 64 : 255;
            fb[y * fb_w + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    /* Draw selection border */
    uint32_t border_color = tc->border_face | 0xFF000000;
    draw_rect_simple(fb, fb_w, fb_h, x1, y1, x2, y2, border_color, 2);
}

/* ============================================================
 * Screenshot Directory
 * ============================================================ */
const char *wubu_screenshot_get_dir(void) {
    if (g_screenshot_dir[0]) return g_screenshot_dir;

    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(g_screenshot_dir, sizeof(g_screenshot_dir), "%s/Pictures/WuBuOS", home);

    struct stat st;
    if (stat(g_screenshot_dir, &st) != 0) {
        mkdir(g_screenshot_dir, 0755);
    }
    return g_screenshot_dir;
}

/* ============================================================
 * Filename Generation
 * ============================================================ */
void wubu_screenshot_gen_filename(char *buf, size_t n) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, n, "Screenshot_%Y-%m-%d_%H-%M-%S.png", tm);
}

/* ============================================================
 * Pixel Capture (Wayland SHM readback)
 * ============================================================ */
extern wayland_state_t g_wl;

static wubu_sshot_t *capture_framebuffer(int x, int y, int w, int h) {
    if (!g_wl.shm_buffer || !g_wl.shm_data) return NULL;

    int fb_w = g_wl.width;
    int fb_h = g_wl.height;
    uint32_t *src = (uint32_t*)g_wl.shm_data;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fb_w) w = fb_w - x;
    if (y + h > fb_h) h = fb_h - y;
    if (w <= 0 || h <= 0) return NULL;

    wubu_sshot_t *sshot = calloc(1, sizeof(wubu_sshot_t));
    if (!sshot) return NULL;

    sshot->width = w;
    sshot->height = h;
    sshot->stride = w * 4;
    sshot->pixels = malloc(w * h * 4);
    if (!sshot->pixels) {
        free(sshot);
        return NULL;
    }

    for (int row = 0; row < h; row++) {
        memcpy(&sshot->pixels[row * w],
               &src[(y + row) * fb_w + x],
               w * 4);
    }
    return sshot;
}

/* ============================================================
 * Window Capture (find focused window)
 * ============================================================ */
static wubu_sshot_t *capture_focused_window(void) {
    extern DosGuiWindow *dosgui_wm_get_focused(void);
    DosGuiWindow *win = dosgui_wm_get_focused();
    if (!win) return capture_framebuffer(0, 0, 0, 0); /* fallback */

    return capture_framebuffer(win->x, win->y, win->w, win->h);
}

/* ============================================================
 * Public Capture API
 * ============================================================ */
wubu_sshot_t *wubu_screenshot_capture(wubu_sshot_mode_t mode) {
    switch (mode) {
        case WUBU_SSHOT_FULL:
            return capture_framebuffer(0, 0, 0, 0);
        case WUBU_SSHOT_WINDOW:
            return capture_focused_window();
        case WUBU_SSHOT_REGION:
            return NULL;
    }
    return NULL;
}

void wubu_screenshot_free(wubu_sshot_t *sshot) {
    if (!sshot) return;
    wubu_screenshot_clear_annotations(sshot);
    free(sshot->pixels);
    free(sshot);
}

/* ============================================================
 * Annotation API
 * ============================================================ */
void wubu_screenshot_add_annotation(wubu_sshot_t *sshot,
                                     wubu_annot_tool_t tool,
                                     int x1, int y1, int x2, int y2,
                                     const char *text,
                                     const wubu_annot_style_t *style) {
    if (!sshot) return;

    wubu_annotation_t *ann = calloc(1, sizeof(wubu_annotation_t));
    if (!ann) return;

    ann->tool = tool;
    ann->x1 = x1; ann->y1 = y1; ann->x2 = x2; ann->y2 = y2;
    if (text) ann->text = strdup(text);
    if (style) ann->style = *style;
    else {
        ann->style.color = 0xFFFF0000;
        ann->style.thickness = 2;
    }

    ann->next = sshot->annotations;
    sshot->annotations = ann;
    sshot->dirty = true;
}

void wubu_screenshot_clear_annotations(wubu_sshot_t *sshot) {
    if (!sshot) return;
    wubu_annotation_t *ann = sshot->annotations;
    while (ann) {
        wubu_annotation_t *next = ann->next;
        free(ann->text);
        free(ann);
        ann = next;
    }
    sshot->annotations = NULL;
    sshot->dirty = false;
}

/* Simple line drawing (Bresenham) */
static void draw_line(uint32_t *buf, int w, int h, int x1, int y1, int x2, int y2, uint32_t color, int thickness) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;

    for (int t = 0; t < thickness; t++) {
        int cx = x1, cy = y1;
        while (true) {
            if (cx >= 0 && cx < w && cy >= 0 && cy < h) buf[cy * w + cx] = color;
            if (cx == x2 && cy == y2) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; cx += sx; }
            if (e2 <= dx) { err += dx; cy += sy; }
        }
        if (thickness > 1) {
            if (abs(x2 - x1) > abs(y2 - y1)) y1++; else x1++;
        }
    }
}

/* Rectangle outline */
static void draw_rect(uint32_t *buf, int w, int h, int x1, int y1, int x2, int y2, uint32_t color, int thickness, uint32_t fill) {
    int left = x1 < x2 ? x1 : x2;
    int right = x1 > x2 ? x1 : x2;
    int top = y1 < y2 ? y1 : y2;
    int bottom = y1 > y2 ? y1 : y2;

    if (fill) {
        for (int y = top + thickness; y < bottom - thickness; y++) {
            for (int x = left + thickness; x < right - thickness; x++) {
                if (x >= 0 && x < w && y >= 0 && y < h) buf[y * w + x] = fill;
            }
        }
    }

    for (int t = 0; t < thickness; t++) {
        int l = left + t, r = right - t, tp = top + t, b = bottom - t;
        for (int x = l; x <= r; x++) {
            if (x >= 0 && x < w) {
                if (tp >= 0 && tp < h) buf[tp * w + x] = color;
                if (b >= 0 && b < h) buf[b * w + x] = color;
            }
        }
        for (int y = tp; y <= b; y++) {
            if (y >= 0 && y < h) {
                if (l >= 0 && l < w) buf[y * w + l] = color;
                if (r >= 0 && r < w) buf[y * w + r] = color;
            }
        }
    }
}

/* Ellipse (midpoint algorithm) */
static void plot_ellipse_points(uint32_t *buf, int w, int h, int cx, int cy, int x, int y, uint32_t color) {
    if (cx + x >= 0 && cx + x < w && cy + y >= 0 && cy + y < h) buf[(cy + y) * w + cx + x] = color;
    if (cx - x >= 0 && cx - x < w && cy + y >= 0 && cy + y < h) buf[(cy + y) * w + cx - x] = color;
    if (cx + x >= 0 && cx + x < w && cy - y >= 0 && cy - y < h) buf[(cy - y) * w + cx + x] = color;
    if (cx - x >= 0 && cx - x < w && cy - y >= 0 && cy - y < h) buf[(cy - y) * w + cx - x] = color;
}

static void draw_ellipse(uint32_t *buf, int w, int h, int x1, int y1, int x2, int y2, uint32_t color, int thickness, uint32_t fill) {
    int cx = (x1 + x2) / 2;
    int cy = (y1 + y2) / 2;
    int rx = abs(x2 - x1) / 2;
    int ry = abs(y2 - y1) / 2;
    if (rx == 0 || ry == 0) return;

    for (int t = 0; t < thickness; t++) {
        int x = 0, y = ry + t;
        int rx2 = (rx + t) * (rx + t);
        int ry2 = (ry + t) * (ry + t);
        int two_rx2 = 2 * rx2;
        int two_ry2 = 2 * ry2;
        int px = 0, py = two_rx2 * y;

        int p = ry2 - rx2 * ry + rx2 / 4;
        while (px < py) {
            plot_ellipse_points(buf, w, h, cx, cy, x, y, color);
            x++; px += two_ry2;
            if (p < 0) p += ry2 + px;
            else { y--; py -= two_rx2; p += ry2 + px - py; }
        }
        p = (int)(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
        while (y >= 0) {
            plot_ellipse_points(buf, w, h, cx, cy, x, y, color);
            y--; py -= two_rx2;
            if (p > 0) p += rx2 - py;
            else { x++; px += two_ry2; p += rx2 - py + px; }
        }
    }

    if (fill) {
        for (int dy = -ry; dy <= ry; dy++) {
            double ratio = (double)dy * dy / (ry * ry);
            if (ratio > 1.0) continue;
            int dx = (int)(rx * sqrt(1.0 - ratio));
            for (int x = -dx; x <= dx; x++) {
                int px = cx + x, py = cy + dy;
                if (px >= 0 && px < w && py >= 0 && py < h) buf[py * w + px] = fill;
            }
        }
    }
}

/* Arrow */
static void draw_arrow(uint32_t *buf, int w, int h, int x1, int y1, int x2, int y2, uint32_t color, int thickness) {
    draw_line(buf, w, h, x1, y1, x2, y2, color, thickness);
    double angle = atan2((double)y1 - y2, (double)x1 - x2);
    int head_len = 15 + thickness * 3;
    int ax1 = x2 + (int)(head_len * cos(angle + 0.5));
    int ay1 = y2 + (int)(head_len * sin(angle + 0.5));
    int ax2 = x2 + (int)(head_len * cos(angle - 0.5));
    int ay2 = y2 + (int)(head_len * sin(angle - 0.5));
    draw_line(buf, w, h, x2, y2, ax1, ay1, color, thickness);
    draw_line(buf, w, h, x2, y2, ax2, ay2, color, thickness);
}

void wubu_screenshot_render_annotations(wubu_sshot_t *sshot) {
    if (!sshot || !sshot->pixels || !sshot->annotations) return;

    wubu_annotation_t *ann = sshot->annotations;
    while (ann) {
        wubu_annot_style_t *st = &ann->style;
        uint32_t color = st->color | 0xFF000000;
        uint32_t fill = st->fill_color ? (st->fill_color | 0xFF000000) : 0;

        switch (ann->tool) {
            case WUBU_ANNOT_RECT:
                draw_rect(sshot->pixels, sshot->width, sshot->height,
                          ann->x1, ann->y1, ann->x2, ann->y2,
                          color, st->thickness, fill);
                break;
            case WUBU_ANNOT_ELLIPSE:
                draw_ellipse(sshot->pixels, sshot->width, sshot->height,
                             ann->x1, ann->y1, ann->x2, ann->y2,
                             color, st->thickness, fill);
                break;
            case WUBU_ANNOT_ARROW:
                draw_arrow(sshot->pixels, sshot->width, sshot->height,
                           ann->x1, ann->y1, ann->x2, ann->y2,
                           color, st->thickness);
                break;
            case WUBU_ANNOT_FREEHAND:
                draw_line(sshot->pixels, sshot->width, sshot->height,
                          ann->x1, ann->y1, ann->x2, ann->y2,
                          color, st->thickness);
                break;
            case WUBU_ANNOT_HIGHLIGHT:
                draw_rect(sshot->pixels, sshot->width, sshot->height,
                          ann->x1, ann->y1, ann->x2, ann->y2,
                          0xFFFFFF00, 1, 0x80FFFF00);
                break;
            case WUBU_ANNOT_TEXT:
                break;
            default: break;
        }
        ann = ann->next;
    }
    sshot->dirty = false;
}

/* ============================================================
 * PNG Save (simplified - writes valid PNG)
 * ============================================================ */

/* Real clipboard buffer (owned by this module). */
static uint8_t *g_clipboard_png = NULL;
static size_t   g_clipboard_len = 0;

bool wubu_screenshot_to_clipboard(wubu_sshot_t *sshot) {
    if (!sshot || !sshot->pixels) return false;
    if (sshot->dirty) wubu_screenshot_render_annotations(sshot);

    uint8_t *png = NULL;
    size_t len = png_encode_rgba(sshot->pixels, sshot->width, sshot->height, &png);
    if (len == 0 || !png) return false;

    free(g_clipboard_png);
    g_clipboard_png = png;
    g_clipboard_len = len;
    return true;
}

/* Accessor for tests / paste handlers: returns clipboard bytes + length. */
const uint8_t *wubu_screenshot_clipboard_data(size_t *out_len) {
    if (out_len) *out_len = g_clipboard_len;
    return g_clipboard_png;
}

/* ============================================================
 * Region Selection Callback
 * ============================================================ */
static void region_complete_cb(int x, int y, int w, int h, void *user) {
    (void)user;
    wubu_sshot_t *sshot = capture_framebuffer(x, y, w, h);
    g_region_result = sshot;
    if (g_region_callback) g_region_callback(x, y, w, h, g_region_userdata);
}

void wubu_screenshot_handle_printscr(void) {
    wubu_sshot_t *sshot = wubu_screenshot_capture(WUBU_SSHOT_FULL);
    if (sshot) {
        char path[PATH_MAX];
        if (wubu_screenshot_save_auto(sshot, path, sizeof(path))) {
            wubu_notify_simple("WuBuOS", "Screenshot Saved", path, NULL, NOTIFY_URGENCY_LOW, 5000);
            wubu_screenshot_to_clipboard(sshot);
        }
        wubu_screenshot_free(sshot);
    }
}

void wubu_screenshot_handle_alt_printscr(void) {
    wubu_sshot_t *sshot = wubu_screenshot_capture(WUBU_SSHOT_WINDOW);
    if (sshot) {
        char path[PATH_MAX];
        if (wubu_screenshot_save_auto(sshot, path, sizeof(path))) {
            wubu_notify_simple("WuBuOS", "Window Screenshot Saved", path, NULL, NOTIFY_URGENCY_LOW, 5000);
            wubu_screenshot_to_clipboard(sshot);
        }
        wubu_screenshot_free(sshot);
    }
}

void wubu_screenshot_handle_shift_printscr(void) {
    extern wayland_state_t g_wl;
    if (!g_wl.shm_buffer) return;

    if (g_region_selector) {
        wubu_region_selector_end(g_region_selector);
        g_region_selector = NULL;
        return;
    }

    g_region_selector = wubu_region_selector_begin(g_wl.width, g_wl.height,
                                                    region_complete_cb, NULL);
}

void wubu_screenshot_update_region_selector(int x, int y, bool down) {
    if (g_region_selector) {
        wubu_region_selector_mouse(g_region_selector, x, y, down);
    }
}

void wubu_screenshot_render_region_selector(uint32_t *fb, int w, int h) {
    if (g_region_selector) {
        wubu_region_selector_render(g_region_selector, fb, w, h);
    }
}

bool wubu_screenshot_has_active_region_selector(void) {
    return g_region_selector != NULL;
}

/* ============================================================
 * Init/Shutdown
 * ============================================================ */
void wubu_screenshot_init(void) {
    g_screenshot_dir[0] = 0;
    wubu_screenshot_get_dir();
}

void wubu_screenshot_shutdown(void) {
    if (g_region_selector) {
        wubu_region_selector_end(g_region_selector);
        g_region_selector = NULL;
    }
    if (g_region_result) {
        wubu_screenshot_free(g_region_result);
        g_region_result = NULL;
    }
}
