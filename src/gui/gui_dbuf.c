/*
 * gui_dbuf.c — WuBuOS Double-Buffered GUI Renderer Implementation
 *
 * Cell 101: Flicker-free rendering with dirty rectangle tracking.
 */

#include "gui_dbuf.h"
#include <stdlib.h>
#include <string.h>

/* ── Simple 8x8 bitmap font for GUI text ──────────────────── */
/* Minimal 8x8 font: digits 0-9, A-Z, space, colon, dash, period, slash */
/* Each character is 8 bytes (8 rows of 8 bits) */

static const uint8_t font_8x8[40][8] = {
    /* 0 */ {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00},
    /* 1 */ {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00},
    /* 2 */ {0x3C,0x66,0x06,0x1C,0x30,0x60,0x7E,0x00},
    /* 3 */ {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00},
    /* 4 */ {0x0C,0x1C,0x2C,0x4C,0x7E,0x0C,0x0C,0x00},
    /* 5 */ {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00},
    /* 6 */ {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00},
    /* 7 */ {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00},
    /* 8 */ {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00},
    /* 9 */ {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00},
    /* A */ {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00},
    /* B */ {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00},
    /* C */ {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00},
    /* D */ {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00},
    /* E */ {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00},
    /* F */ {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00},
    /* G */ {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0x00},
    /* H */ {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00},
    /* I */ {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},
    /* J */ {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00},
    /* K */ {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00},
    /* L */ {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00},
    /* M */ {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00},
    /* N */ {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00},
    /* O */ {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    /* P */ {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00},
    /* Q */ {0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0x00},
    /* R */ {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00},
    /* S */ {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00},
    /* T */ {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    /* U */ {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    /* V */ {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00},
    /* W */ {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    /* X */ {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00},
    /* Y */ {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00},
    /* Z */ {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00},
    /* space */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* : */ {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00},
    /* - */ {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00},
    /* . */ {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
    /* / */ {0x02,0x06,0x0C,0x18,0x30,0x60,0x40,0x00},
};

static int char_to_font_idx(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    if (c >= 'a' && c <= 'Z') return c - 'a' + 10;
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    switch (c) {
        case ' ': return 36;
        case ':': return 37;
        case '-': return 38;
        case '.': return 39;
        case '/': return 40;
        default:  return 36; /* space for unknown */
    }
}

static void draw_char(gui_dbuf_t *db, int x, int y, char c, uint32_t color) {
    int idx = char_to_font_idx(c);
    if (idx < 0 || idx >= 40) return;
    const uint8_t *glyph = font_8x8[idx];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                gui_dbuf_pixel(db, x + col, y + row, color);
            }
        }
    }
}

/* ── Lifecycle ─────────────────────────────────────────────── */

int gui_dbuf_init(gui_dbuf_t *db, int width, int height) {
    memset(db, 0, sizeof(*db));
    db->width  = width;
    db->height = height;
    db->bpp    = 32;
    db->back   = (uint32_t *)calloc((size_t)width * height, sizeof(uint32_t));
    if (!db->back) return -1;
    return 0;
}

void gui_dbuf_shutdown(gui_dbuf_t *db) {
    if (!db) return;
    if (db->back) {
        free(db->back);
        db->back = NULL;
    }
}

/* ── Drawing Primitives ────────────────────────────────────── */

void gui_dbuf_clear(gui_dbuf_t *db, uint32_t color) {
    if (!db || !db->back) return;
    int total = db->width * db->height;
    for (int i = 0; i < total; i++) db->back[i] = color;
    gui_dbuf_mark_dirty(db, 0, 0, db->width, db->height);
}

void gui_dbuf_pixel(gui_dbuf_t *db, int x, int y, uint32_t color) {
    if (!db || !db->back) return;
    if (x < 0 || x >= db->width || y < 0 || y >= db->height) return;
    db->back[y * db->width + x] = color;
}

void gui_dbuf_hline(gui_dbuf_t *db, int x1, int x2, int y, uint32_t color) {
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    for (int x = x1; x <= x2; x++) gui_dbuf_pixel(db, x, y, color);
}

void gui_dbuf_vline(gui_dbuf_t *db, int x, int y1, int y2, uint32_t color) {
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    for (int y = y1; y <= y2; y++) gui_dbuf_pixel(db, x, y, color);
}

void gui_dbuf_rect(gui_dbuf_t *db, int x, int y, int w, int h, uint32_t color) {
    gui_dbuf_hline(db, x, x + w - 1, y, color);
    gui_dbuf_hline(db, x, x + w - 1, y + h - 1, color);
    gui_dbuf_vline(db, x, y, y + h - 1, color);
    gui_dbuf_vline(db, x + w - 1, y, y + h - 1, color);
}

void gui_dbuf_fill_rect(gui_dbuf_t *db, int x, int y, int w, int h, uint32_t color) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            gui_dbuf_pixel(db, x + dx, y + dy, color);
}

/* ── Win98-Style 3D Borders ────────────────────────────────── */

void gui_dbuf_border_raised(gui_dbuf_t *db, int x, int y, int w, int h) {
    uint32_t lt = 0x00FFFFFF;  /* white highlight */
    uint32_t dk = 0x00808080;  /* dark shadow */
    uint32_t dd = 0x00000000;  /* black */
    /* Top/left: light */
    gui_dbuf_hline(db, x, x+w-1, y, lt);
    gui_dbuf_vline(db, x, y, y+h-1, lt);
    /* Inner highlight */
    gui_dbuf_hline(db, x+1, x+w-2, y+1, 0x00DFDFDF);
    gui_dbuf_vline(db, x+1, y+1, y+h-2, 0x00DFDFDF);
    /* Bottom/right: dark */
    gui_dbuf_hline(db, x+1, x+w-2, y+h-2, dk);
    gui_dbuf_vline(db, x+w-2, y+1, y+h-2, dk);
    /* Outer edge: black */
    gui_dbuf_hline(db, x, x+w-1, y+h-1, dd);
    gui_dbuf_vline(db, x+w-1, y, y+h-1, dd);
}

void gui_dbuf_border_sunken(gui_dbuf_t *db, int x, int y, int w, int h) {
    uint32_t dk = 0x00808080;
    uint32_t lt = 0x00FFFFFF;
    /* Top/left: dark */
    gui_dbuf_hline(db, x, x+w-1, y, dk);
    gui_dbuf_vline(db, x, y, y+h-1, dk);
    /* Bottom/right: light */
    gui_dbuf_hline(db, x+1, x+w-2, y+h-2, 0x00DFDFDF);
    gui_dbuf_vline(db, x+w-2, y+1, y+h-2, 0x00DFDFDF);
    gui_dbuf_hline(db, x, x+w-1, y+h-1, lt);
    gui_dbuf_vline(db, x+w-1, y, y+h-1, lt);
}

void gui_dbuf_button(gui_dbuf_t *db, int x, int y, int w, int h,
                     const char *label, int pressed) {
    /* Fill background */
    gui_dbuf_fill_rect(db, x, y, w, h, 0x00C0C0C0);  /* Win98 gray */
    /* Border */
    if (pressed)
        gui_dbuf_border_sunken(db, x, y, w, h);
    else
        gui_dbuf_border_raised(db, x, y, w, h);
    /* Label */
    if (label) {
        int len = (int)strlen(label);
        int tx = x + (w - len * 8) / 2;
        int ty = y + (h - 8) / 2;
        for (int i = 0; i < len; i++) {
            draw_char(db, tx + i * 8, ty, label[i], 0x00000000);
        }
    }
    gui_dbuf_mark_dirty(db, x, y, w, h);
}

void gui_dbuf_window(gui_dbuf_t *db, int x, int y, int w, int h,
                     const char *title, int active) {
    /* Window face */
    gui_dbuf_fill_rect(db, x, y, w, h, 0x00C0C0C0);
    /* Outer border */
    gui_dbuf_border_raised(db, x, y, w, h);
    /* Title bar */
    uint32_t tb = active ? 0x00000080 : 0x00808080;  /* blue or gray */
    gui_dbuf_fill_rect(db, x+4, y+4, w-8, 18, tb);
    /* Title text */
    if (title) {
        int tx = x + 8;
        int ty = y + 7;
        for (int i = 0; title[i]; i++) {
            draw_char(db, tx + i * 8, ty, title[i], 0x00FFFFFF);
        }
    }
    /* Inner content area border */
    gui_dbuf_border_sunken(db, x+4, y+24, w-8, h-28);
    gui_dbuf_mark_dirty(db, x, y, w, h);
}

/* ── Dirty Rectangle Tracking ──────────────────────────────── */

void gui_dbuf_mark_dirty(gui_dbuf_t *db, int x, int y, int w, int h) {
    if (!db || db->dirty_count >= GUI_DBUF_DIRTY_MAX) return;
    /* Clip to screen */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > db->width)  w = db->width - x;
    if (y + h > db->height) h = db->height - y;
    if (w <= 0 || h <= 0) return;

    gui_rect_t *r = &db->dirty[db->dirty_count++];
    r->x = x; r->y = y; r->w = w; r->h = h;
}

void gui_dbuf_clear_dirty(gui_dbuf_t *db) {
    if (!db) return;
    db->dirty_count = 0;
}

/* ── Flip ──────────────────────────────────────────────────── */

uint32_t gui_dbuf_flip(gui_dbuf_t *db) {
    if (!db || !db->back) return 0;

    uint32_t pixels = 0;
    for (int i = 0; i < db->dirty_count; i++) {
        gui_rect_t *r = &db->dirty[i];
        for (int y = r->y; y < r->y + r->h; y++) {
            for (int x = r->x; x < r->x + r->w; x++) {
                /* In the real kernel, this would write to VBE front buffer.
                 * In hosted mode, we just count pixels. */
                pixels++;
            }
        }
    }

    db->pixels_copied += pixels;
    db->flips++;
    db->frames++;
    gui_dbuf_clear_dirty(db);
    return pixels;
}

/* ── Query ─────────────────────────────────────────────────── */

int      gui_dbuf_width(const gui_dbuf_t *db) { return db ? db->width : 0; }
int      gui_dbuf_height(const gui_dbuf_t *db) { return db ? db->height : 0; }
uint64_t gui_dbuf_frames(const gui_dbuf_t *db) { return db ? db->frames : 0; }
int      gui_dbuf_dirty_count(const gui_dbuf_t *db) { return db ? db->dirty_count : 0; }
