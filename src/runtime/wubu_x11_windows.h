/* wubu_x11_windows.h -- the resources a client draws into.
 *
 * A surface with no id is a framebuffer nobody can address. This is the table
 * that turns a client's CreateWindow into something PutImage can name, and it is
 * deliberately small and fixed: the ONLY windows that exist are the ones a guest
 * asked for, each with a bounded buffer from a static pool. No allocator, no
 * growth, no way for a guest to exhaust the host -- the same discipline the
 * kernel-side pools use.
 *
 * Presentation is a copy out: the compositor asks for a window's pixels and gets
 * RGBA in the shape dos_window already blits. The window never draws itself.
 */
#ifndef WUBU_X11_WINDOWS_H
#define WUBU_X11_WINDOWS_H

#include <stddef.h>
#include <stdint.h>

#include "wubu_x11_surface.h"

#define WUBU_X11_MAX_WINDOWS 32
#define WUBU_X11_MAX_PIXELS  (1024 * 768)     /* per window, bounded on purpose */

typedef struct WubuX11Window {
    uint32_t id;
    int      in_use;
    int      mapped;
    int      x, y, w, h;
    WubuX11Surface surface;
    uint8_t  pixels[WUBU_X11_MAX_PIXELS * 4];
} WubuX11Window;

typedef struct WubuX11Windows {
    WubuX11Window win[WUBU_X11_MAX_WINDOWS];
    int count;
} WubuX11Windows;

void wubu_x11_windows_init(WubuX11Windows *t);

/* Create a window. Returns 0, or negative when the id is taken, the geometry is
 * impossible, or the table/pool is full. A refusal is reported, never rounded. */
int wubu_x11_window_create(WubuX11Windows *t, uint32_t id,
                           int x, int y, int w, int h);
int wubu_x11_window_destroy(WubuX11Windows *t, uint32_t id);
int wubu_x11_window_map(WubuX11Windows *t, uint32_t id, int mapped);
WubuX11Window *wubu_x11_window_find(WubuX11Windows *t, uint32_t id);

/* PutImage into a window, reporting an unknown id as such. */
int wubu_x11_window_put_image(WubuX11Windows *t, uint32_t id, int x, int y,
                              int w, int h, const uint8_t *data, size_t len,
                              int lsb, int depth);

/* Copy a mapped window's pixels out, top-left anchored. Returns 0 when `cap` is
 * too small, so a caller cannot be silently handed a truncated frame. */
int wubu_x11_window_present(WubuX11Windows *t, uint32_t id,
                            uint8_t *out, size_t cap, int out_w, int out_h);

#endif
