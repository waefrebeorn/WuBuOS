/*
 * dosgui_wm_icon_glyphs.c -- Desktop icon glyph rendering (Chicago -> XP).
 *
 * The old desktop drew every icon as a flat colored box (icon_bg) with no
 * recognizable shape. That is the "mess": a desktop full of identical
 * rectangles. This module draws a recognizable 32x32 pixel-art glyph per
 * icon type (folder, app, file, drive, URL, shortcut), themed so it reads
 * correctly on both Win98 (silver) and XP Luna (blue) backgrounds.
 *
 * Self-contained: depends only on the public vbe_* pixel API and the theme
 * colors via tc().  No god-header reach-through.
 */

#include "dosgui_wm.h"
#include "dosgui_wm_internal.h"

#include <stdint.h>
#include <string.h>

/* Draw a filled rectangle clipped to the 32x32 icon cell. */
static void glyph_rect(int ox, int oy, int x, int y, int w, int h, uint32_t c) {
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= DOSGUI_ICON_SIZE) continue;
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= DOSGUI_ICON_SIZE) continue;
            vbe_set_pixel(ox + i, oy + j, c);
        }
    }
}

/* Draw a rounded-ish filled box (just an inset rect with lighter top edge). */
static void glyph_box(int ox, int oy, int x, int y, int w, int h,
                      uint32_t face, uint32_t light, uint32_t dark) {
    glyph_rect(ox, oy, x, y, w, h, face);
    glyph_rect(ox, oy, x, y, w, 1, light);          /* top highlight */
    glyph_rect(ox, oy, x, y + h - 1, w, 1, dark);   /* bottom shade */
    glyph_rect(ox, oy, x, y, 1, h, light);          /* left highlight */
    glyph_rect(ox, oy, x + w - 1, y, 1, h, dark);   /* right shade */
}

static void draw_folder(int ox, int oy, uint32_t* c) {
    /* Folder back (darker) + front tab (lighter) -- XP/Chicago folder look. */
    glyph_box(ox, oy, 4, 10, 24, 16, c[0], c[1], c[2]);      /* body */
    glyph_rect(ox, oy, 4, 10, 11, 4, c[1]);                  /* tab top */
    glyph_rect(ox, oy, 4, 9, 11, 2, c[1]);                   /* tab */
    glyph_rect(ox, oy, 5, 11, 22, 2, c[3]);                  /* gloss line */
}

static void draw_app(int ox, int oy, uint32_t* c) {
    /* Mini application window: title bar + body + 3 fake buttons. */
    glyph_box(ox, oy, 5, 6, 22, 20, c[0], c[1], c[2]);       /* window */
    glyph_rect(ox, oy, 5, 6, 22, 5, c[3]);                   /* title bar */
    glyph_rect(ox, oy, 7, 8, 3, 2, c[2]);                    /* title btn */
    glyph_rect(ox, oy, 12, 8, 3, 2, c[2]);
    glyph_rect(ox, oy, 17, 8, 3, 2, c[2]);
    glyph_rect(ox, oy, 8, 14, 16, 2, c[1]);                  /* content line */
    glyph_rect(ox, oy, 8, 18, 12, 2, c[2]);
}

static void draw_file(int ox, int oy, uint32_t* c) {
    /* Document page with a folded corner. */
    glyph_box(ox, oy, 8, 5, 16, 22, c[0], c[1], c[2]);       /* page */
    glyph_rect(ox, oy, 8, 5, 16, 4, c[3]);                   /* header band */
    glyph_rect(ox, oy, 19, 5, 5, 5, c[2]);                   /* folded corner */
    glyph_rect(ox, oy, 11, 12, 10, 1, c[2]);                 /* text lines */
    glyph_rect(ox, oy, 11, 16, 10, 1, c[2]);
    glyph_rect(ox, oy, 11, 20, 7, 1, c[2]);
}

static void draw_drive(int ox, int oy, uint32_t* c) {
    /* Drive: rounded rectangle with a slot + LED. */
    glyph_box(ox, oy, 6, 11, 20, 12, c[0], c[1], c[2]);      /* body */
    glyph_rect(ox, oy, 9, 15, 12, 2, c[2]);                  /* slot */
    glyph_rect(ox, oy, 22, 13, 2, 2, c[3]);                  /* LED */
}

static void draw_url(int ox, int oy, uint32_t* c) {
    /* Globe: circle with meridian/parallels. */
    glyph_box(ox, oy, 8, 7, 16, 16, c[0], c[1], c[2]);       /* disc */
    for (int i = 9; i < 23; i++)                             /* vertical meridian */
        vbe_set_pixel(ox + 15, oy + i, c[2]);
    glyph_rect(ox, oy, 8, 14, 16, 1, c[2]);                  /* equator */
    glyph_rect(ox, oy, 11, 7, 1, 16, c[3]);                  /* longitude */
    glyph_rect(ox, oy, 19, 7, 1, 16, c[3]);
}

/* Small white arrow overlay used to mark shortcuts. */
static void draw_shortcut_arrow(int ox, int oy) {
    uint32_t w = 0x00FFFFFF, k = 0x00000000;
    int ax = 18, ay = 16;
    for (int i = 0; i < 9; i++) {                 /* shaft */
        vbe_set_pixel(ox + ax + i, oy + ay - i, w);
        vbe_set_pixel(ox + ax + i, oy + ay - i + 1, k);
    }
    glyph_rect(ox, oy, ax + 7, ay - 9, 5, 5, w);  /* arrowhead */
    glyph_rect(ox, oy, ax + 7, ay - 9, 5, 1, w);
    glyph_rect(ox, oy, ax + 7, ay - 5, 5, 1, w);
}

/* Public: draw the glyph for an icon type at origin (ox, oy) using up to
 * 4 theme colors: c[0]=face c[1]=light c[2]=dark c[3]=accent. */
void dosgui_wm_draw_icon_glyph(DeskIconType type, int ox, int oy,
                               uint32_t c0, uint32_t c1,
                               uint32_t c2, uint32_t c3) {
    uint32_t c[4] = { c0, c1, c2, c3 };
    switch (type) {
        case DESK_ICON_FOLDER:   draw_folder(ox, oy, c); break;
        case DESK_ICON_APP:      draw_app(ox, oy, c);   break;
        case DESK_ICON_FILE:     draw_file(ox, oy, c);  break;
        case DESK_ICON_DRIVE:    draw_drive(ox, oy, c); break;
        case DESK_ICON_URL:      draw_url(ox, oy, c);   break;
        case DESK_ICON_SHORTCUT:
        default:
            draw_file(ox, oy, c);
            draw_shortcut_arrow(ox, oy);
            break;
    }
}

/* Selection highlight: a translucent navy box with a 1px focus rect, drawn
 * behind/around the selected icon (XP active-selection look). */
void dosgui_wm_draw_icon_selection(int ox, int oy) {
    /* Navy translucent fill (0x80 navy over whatever is below). */
    uint32_t sel = 0x80300080;  /* ABGR: navy, ~50% */
    glyph_rect(ox, oy, 0, 0, DOSGUI_ICON_SIZE, DOSGUI_ICON_SIZE, sel);
    /* Dashed-look focus outline (solid 1px is fine at this scale). */
    uint32_t f = 0x00FFFFFF;
    glyph_rect(ox, oy, 0, 0, DOSGUI_ICON_SIZE, 1, f);
    glyph_rect(ox, oy, 0, DOSGUI_ICON_SIZE - 1, DOSGUI_ICON_SIZE, 1, f);
    glyph_rect(ox, oy, 0, 0, 1, DOSGUI_ICON_SIZE, f);
    glyph_rect(ox, oy, DOSGUI_ICON_SIZE - 1, 0, 1, DOSGUI_ICON_SIZE, f);
}
