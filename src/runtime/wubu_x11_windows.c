/* wubu_x11_windows.c -- see wubu_x11_windows.h. */
#include "wubu_x11_windows.h"

#include <string.h>

void wubu_x11_windows_init(WubuX11Windows *t) {
    memset(t, 0, sizeof *t);
}

WubuX11Window *wubu_x11_window_find(WubuX11Windows *t, uint32_t id) {
    for (int i = 0; i < WUBU_X11_MAX_WINDOWS; i++)
        if (t->win[i].in_use && t->win[i].id == id) return &t->win[i];
    return NULL;
}

int wubu_x11_window_create(WubuX11Windows *t, uint32_t id,
                           int x, int y, int w, int h) {
    if (!t || id == 0) return -1;
    if (w <= 0 || h <= 0) return -2;
    if ((long)w * (long)h > WUBU_X11_MAX_PIXELS) return -3;   /* bounded pool */
    if (wubu_x11_window_find(t, id)) return -4;               /* id already used */
    for (int i = 0; i < WUBU_X11_MAX_WINDOWS; i++) {
        WubuX11Window *win = &t->win[i];
        if (win->in_use) continue;
        memset(win, 0, sizeof *win);
        win->in_use = 1;
        win->id = id;
        win->x = x; win->y = y; win->w = w; win->h = h;
        wubu_x11_surface_init(&win->surface, win->pixels, w, h);
        wubu_x11_surface_clear(&win->surface, 0, 0, 0);
        t->count++;
        return 0;
    }
    return -5;                                                /* table full */
}

int wubu_x11_window_destroy(WubuX11Windows *t, uint32_t id) {
    WubuX11Window *win = wubu_x11_window_find(t, id);
    if (!win) return -1;
    memset(win, 0, sizeof *win);
    if (t->count > 0) t->count--;
    return 0;
}

int wubu_x11_window_map(WubuX11Windows *t, uint32_t id, int mapped) {
    WubuX11Window *win = wubu_x11_window_find(t, id);
    if (!win) return -1;
    win->mapped = mapped ? 1 : 0;
    return 0;
}

int wubu_x11_window_put_image(WubuX11Windows *t, uint32_t id, int x, int y,
                              int w, int h, const uint8_t *data, size_t len,
                              int lsb, int depth) {
    WubuX11Window *win = wubu_x11_window_find(t, id);
    if (!win) return -1;
    return wubu_x11_surface_put_image(&win->surface, x, y, w, h, data, len, lsb, depth);
}

int wubu_x11_window_present(WubuX11Windows *t, uint32_t id,
                            uint8_t *out, size_t cap, int out_w, int out_h) {
    WubuX11Window *win = wubu_x11_window_find(t, id);
    if (!win) return -1;
    if (!out || out_w <= 0 || out_h <= 0) return -2;
    if (cap < (size_t)out_w * (size_t)out_h * 4u) return -3;  /* never truncate */
    memset(out, 0, (size_t)out_w * (size_t)out_h * 4u);

    int copy_w = win->w < out_w ? win->w : out_w;
    int copy_h = win->h < out_h ? win->h : out_h;
    for (int row = 0; row < copy_h; row++)
        memcpy(out + (size_t)row * out_w * 4u,
               win->pixels + (size_t)row * win->w * 4u,
               (size_t)copy_w * 4u);
    wubu_x11_surface_clear_damage(&win->surface);             /* presented */
    return 0;
}
