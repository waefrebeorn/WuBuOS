/*
 * theme.c  --  My Seed Win98 Classic Theme (98.css in C) (LEGACY)
 */
#include "../kernel/vbe_legacy.h"

/* Legacy Win98 color constants (local to legacy theme) */
#define C_WIN_FACE      0x00C0C0C0
#define C_WIN_BORDER_DD 0x00000000

/* Draw Win98 title bar at (x,y+3) of width w, with given title text */
void theme_title_bar(int x, int y, int w, uint32_t color) {
    vbe_fill_rect(x, y, w, 20, color);
}

/* Draw 3D button (raised or pressed) */
void theme_button(int x, int y, int w, int h, int pressed) {
    if (pressed) vbe_3d_sunken_colors(x, y, w, h,
                                       0xFFFFFF, 0xDFDFDF, 0x808080, 0x000000);
    else         vbe_3d_raised_colors(x, y, w, h,
                                       0xFFFFFF, 0xDFDFDF, 0x808080, 0x000000);
}

/* Draw checkbox (Win98 style) */
void theme_checkbox(int x, int y, int checked) {
    vbe_3d_sunken_colors(x, y, 13, 13,
                          0xFFFFFF, 0xDFDFDF, 0x808080, 0x000000);
    if (checked) {
        /* Draw checkmark with lines */
        vbe_vline(x+3, y+5, y+10, C_WIN_BORDER_DD);
        vbe_hline(x+3, x+6, y+10, C_WIN_BORDER_DD);
        vbe_vline(x+7, y+3, y+7, C_WIN_BORDER_DD);
    }
}

/* Draw scroll bar track */
void theme_scroll_track(int x, int y, int w, int h) {
    vbe_fill_rect(x, y, w, h, C_WIN_FACE);
    vbe_3d_sunken_colors(x, y, w, h,
                          0xFFFFFF, 0xDFDFDF, 0x808080, 0x000000);
}

/* Draw scroll bar thumb */
void theme_scroll_thumb(int x, int y, int w, int h) {
    vbe_fill_rect(x, y, w, h, C_WIN_FACE);
    vbe_3d_raised_colors(x, y, w, h,
                          0xFFFFFF, 0xDFDFDF, 0x808080, 0x000000);
}
