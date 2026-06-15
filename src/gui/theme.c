/*
 * theme.c  --  My Seed Win98 Classic Theme (98.css in C)
 */
#include "../kernel/vbe.h"

/* Draw Win98 title bar at (x,y+3) of width w, with given title text */
void theme_title_bar(int x, int y, int w, uint32_t color) {
    vbe_fill_rect(x, y, w, 20, color);
}

/* Draw 3D button (raised or pressed) */
void theme_button(int x, int y, int w, int h, int pressed) {
    if (pressed) vbe_3d_sunken(x, y, w, h);
    else         vbe_3d_raised(x, y, w, h);
}

/* Draw checkbox (Win98 style) */
void theme_checkbox(int x, int y, int checked) {
    vbe_3d_sunken(x, y, 13, 13);
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
    vbe_3d_sunken(x, y, w, h);
}

/* Draw scroll bar thumb */
void theme_scroll_thumb(int x, int y, int w, int h) {
    vbe_fill_rect(x, y, w, h, C_WIN_FACE);
    vbe_3d_raised(x, y, w, h);
}
