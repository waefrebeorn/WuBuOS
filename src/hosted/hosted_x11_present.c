/*
 * hosted_x11_present.c -- present the WuBuOS framebuffer on the REAL display.
 *
 * The hosted binary had exactly one windowed path: a Wayland client. Under an X11 desktop
 * (Cinnamon here) that path cannot connect, so the only ways to look at the OS were a nested
 * compositor or a headless PPM dump -- a "split world" where what I inspected was not what the
 * screen showed. This module closes that split: it opens the actual X server, puts the OS in a
 * borderless window on the real screen, and blits the same framebuffer the renderer fills.
 *
 * The framebuffer is 0x00RRGGBB in host byte order, which on little-endian is B,G,R,X in
 * memory -- exactly ZPixmap depth-24 TrueColor with 32-bit stride, so XPutImage reads it
 * directly with no conversion and no copy.
 *
 * C11, no libm. Bounded: one expose/close pump per frame, no blocking calls.
 */

#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static Display *g_xdpy;
static Window   g_xwin;
static GC       g_xgc;
static XImage  *g_ximg;
static int      g_xw, g_xh;
static int      g_xopen;
static int      g_xquit;

int hosted_x11_present_open(int width, int height, const char *title) {
    if (width <= 0 || height <= 0) return -1;
    g_xdpy = XOpenDisplay(NULL);
    if (!g_xdpy) return -1;

    int scr = DefaultScreen(g_xdpy);
    Visual *vis = DefaultVisual(g_xdpy, scr);
    int depth = DefaultDepth(g_xdpy, scr);

    g_xw = width;
    g_xh = height;

    /* A borderless, override-redirect window is deliberately NOT used: the OS should behave
     * like an application on the desktop (movable, closable) rather than seizing the screen.
     * It is sized to the OS resolution and centred. */
    int sw = DisplayWidth(g_xdpy, scr), sh = DisplayHeight(g_xdpy, scr);
    int x = (sw - width) / 2, y = (sh - height) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.background_pixel = BlackPixel(g_xdpy, scr);
    attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                       ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                       StructureNotifyMask;

    g_xwin = XCreateWindow(g_xdpy, RootWindow(g_xdpy, scr), x, y,
                           (unsigned)width, (unsigned)height, 0, depth, InputOutput, vis,
                           CWBackPixel | CWEventMask, &attrs);
    if (!g_xwin) { XCloseDisplay(g_xdpy); g_xdpy = NULL; return -1; }

    XStoreName(g_xdpy, g_xwin, title ? title : "WuBuOS");

    /* Pin the geometry. A managed window gets maximised by the WM to the screen size, and the
     * OS then paints 1024x768 into a 1920-wide window -- the right half reads as dead black.
     * Fixed size hints make the WM keep the OS's own resolution. */
    XSizeHints *hints = XAllocSizeHints();
    if (hints) {
        hints->flags = PMinSize | PMaxSize | PBaseSize;
        hints->min_width = hints->max_width = hints->base_width = width;
        hints->min_height = hints->max_height = hints->base_height = height;
        XSetWMNormalHints(g_xdpy, g_xwin, hints);
        XFree(hints);
    }
    /* Closing the window means closing the OS, not orphaning the process. */
    Atom wm_delete = XInternAtom(g_xdpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_xdpy, g_xwin, &wm_delete, 1);

    XMapWindow(g_xdpy, g_xwin);
    g_xgc = XCreateGC(g_xdpy, g_xwin, 0, NULL);

    /* Zero-copy view of our framebuffer. XDestroyImage would free data, so it is never called;
     * the buffer belongs to the VBE backend. */
    char *data = (char *)calloc((size_t)width * (size_t)height, 4);
    if (!data) return -1;
    g_ximg = XCreateImage(g_xdpy, vis, (unsigned)depth, ZPixmap, 0, data,
                          (unsigned)width, (unsigned)height, 32, 0);
    if (!g_ximg) return -1;

    g_xopen = 1;
    XFlush(g_xdpy);
    return 0;
}

/* Copy the OS framebuffer into the X image and put it on screen. Called once per presented
 * frame; caller decides whether anything changed. */
void hosted_x11_present_frame(const void *fb) {
    if (!g_xopen || !g_ximg || !fb) return;
    size_t bytes = (size_t)g_xw * (size_t)g_xh * 4;
    memcpy(g_ximg->data, fb, bytes);
    XPutImage(g_xdpy, g_xwin, g_xgc, g_ximg, 0, 0, 0, 0,
              (unsigned)g_xw, (unsigned)g_xh);
    XFlush(g_xdpy);
}

/* Drain X events without blocking. Returns non-zero when the window was closed. */
int hosted_x11_present_pump(void) {
    if (!g_xopen) return 1;
    while (XPending(g_xdpy) > 0) {
        XEvent ev;
        XNextEvent(g_xdpy, &ev);
        switch (ev.type) {
        case Expose:
            if (g_ximg) {
                XPutImage(g_xdpy, g_xwin, g_xgc, g_ximg, 0, 0, 0, 0,
                          (unsigned)g_xw, (unsigned)g_xh);
            }
            break;
        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == XInternAtom(g_xdpy, "WM_DELETE_WINDOW", False))
                g_xquit = 1;
            break;
        case DestroyNotify:
            g_xquit = 1;
            break;
        case KeyPress: {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            if (ks == XK_Escape) g_xquit = 1;
            break;
        }
        default:
            break;
        }
    }
    return g_xquit;
}

int hosted_x11_present_active(void) { return g_xopen ? 1 : 0; }

void hosted_x11_present_close(void) {
    if (!g_xopen) return;
    if (g_ximg) { free(g_ximg->data); g_ximg->data = NULL; g_ximg = NULL; }
    if (g_xgc) { XFreeGC(g_xdpy, g_xgc); g_xgc = NULL; }
    if (g_xwin) { XDestroyWindow(g_xdpy, g_xwin); g_xwin = 0; }
    if (g_xdpy) { XCloseDisplay(g_xdpy); g_xdpy = NULL; }
    g_xopen = 0;
}
