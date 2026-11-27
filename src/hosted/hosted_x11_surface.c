/*
 * hosted_x11_surface.c -- what the window is PRESENTED onto: the XImage, the MIT-SHM segment, and the
 * fullscreen/resize requests.
 *
 * Split from hosted_x11_present.c (window + event loop) because both are real concerns and the combined file
 * had crossed the 600-line budget. The X window itself is never recreated here: a new window means a new
 * client id, lost focus and key grabs that must be redone.
 */
#include "hosted_x11_private.h"
#include "hosted_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

/* one definition for the state the private header declares: the X connection, the window, and the surface */
Display *g_xdpy;
Window   g_xwin;
GC       g_xgc;
XImage  *g_ximg;
int      g_xw, g_xh;
int      g_shm_ok;
XShmSegmentInfo g_shm;
int      g_xopen;

int g_fullscreen;
int g_win_w, g_win_h, g_win_x, g_win_y;
int g_pending_w, g_pending_h;

/* Release the image + segment. The fr: the socket path points the image straight at the OS framebuffer,
 * which this unit must never free (XDestroyImage would). */
void x11_release_image_surface(void) {
    if (g_shm_ok) {
        XShmDetach(g_xdpy, &g_shm);
        if (g_ximg) { g_ximg->data = NULL; XDestroyImage(g_ximg); g_ximg = NULL; }
        if (g_shm.shmaddr && g_shm.shmaddr != (char *)-1) shmdt(g_shm.shmaddr);
        g_shm.shmaddr = NULL;
    } else if (g_ximg) {
        g_ximg->data = NULL;
        XDestroyImage(g_ximg);
        g_ximg = NULL;
    }
    g_shm_ok = 0;
}

int x11_create_image_surface(int w, int h) {
    if (!g_xdpy || w <= 0 || h <= 0) return -1;
    Visual *vis = DefaultVisual(g_xdpy, DefaultScreen(g_xdpy));
    int depth = DefaultDepth(g_xdpy, DefaultScreen(g_xdpy));
    g_xw = w; g_xh = h;

    if (XShmQueryExtension(g_xdpy)) {
        g_ximg = XShmCreateImage(g_xdpy, vis, (unsigned)depth, ZPixmap, NULL, &g_shm,
                                 (unsigned)w, (unsigned)h);
        if (g_ximg) {
            size_t bytes = (size_t)w * (size_t)h * 4u;
            g_shm.shmid = shmget(IPC_PRIVATE, bytes, IPC_CREAT | 0600);
            if (g_shm.shmid >= 0) {
                g_shm.shmaddr = g_ximg->data = (char *)shmat(g_shm.shmid, NULL, 0);
                if (g_shm.shmaddr != (char *)-1) {
                    g_shm.readOnly = False;
                    if (XShmAttach(g_xdpy, &g_shm)) {
                        XSync(g_xdpy, False);
                        shmctl(g_shm.shmid, IPC_RMID, NULL);   /* the segment dies with us */
                        g_shm_ok = 1;
                    }
                }
            }
            if (!g_shm_ok) { fprintf(stderr, "[x11] MIT-SHM unavailable, using socket present\n"); }
        }
    }
    if (!g_shm_ok) {
        char *data = (char *)calloc((size_t)w * (size_t)h, 4);
        if (!data) return -1;
        g_ximg = XCreateImage(g_xdpy, vis, (unsigned)depth, ZPixmap, 0, data,
                              (unsigned)w, (unsigned)h, 32, 0);
        if (!g_ximg) return -1;
    }
    return 0;
}

/* Fullscreen: the OS was a fixed-size window (PMinSize == PMaxSize), which the WM must not resize, so a
 * "fullscreen" state could not exist at all -- `-f` was parsed and never used. Toggling now (a) asks the WM
 * through EWMH, (b) relaxes the size hints so a WM that ignores EWMH can still resize us, and (c) asks the
 * hosted loop to re-init the guest framebuffer at the new size, because a 1400x880 framebuffer stretched over
 * a 1920x1080 window is not usable -- it is a letterboxed session. */

static void set_size_hints(int w, int h) {
    XSizeHints *hints = XAllocSizeHints();
    if (!hints) return;
    hints->flags = PMinSize | PMaxSize | PBaseSize;
    hints->min_width = hints->max_width = hints->base_width = w;
    hints->min_height = hints->max_height = hints->base_height = h;
    XSetWMNormalHints(g_xdpy, g_xwin, hints);
    XFree(hints);
}

static void ewmh_fullscreen(int on) {
    Atom state = XInternAtom(g_xdpy, "_NET_WM_STATE", False);
    Atom full  = XInternAtom(g_xdpy, "_NET_WM_STATE_FULLSCREEN", False);
    XEvent e;
    memset(&e, 0, sizeof e);
    e.type = ClientMessage;
    e.xclient.window = g_xwin;
    e.xclient.message_type = state;
    e.xclient.format = 32;
    e.xclient.data.l[0] = on ? 1 : 0;      /* _NET_WM_STATE_ADD / _REMOVE */
    e.xclient.data.l[1] = (long)full;
    e.xclient.data.l[2] = 0;
    e.xclient.data.l[3] = 1;               /* source: application */
    XSendEvent(g_xdpy, DefaultRootWindow(g_xdpy), False,
               SubstructureNotifyMask | SubstructureRedirectMask, &e);
}

int hosted_x11_toggle_fullscreen(void) {
    if (!g_xdpy || !g_xwin) return -1;
    int sw = DisplayWidth(g_xdpy, DefaultScreen(g_xdpy));
    int sh = DisplayHeight(g_xdpy, DefaultScreen(g_xdpy));
    g_fullscreen = !g_fullscreen;
    if (g_fullscreen) {
        /* relax the pin BEFORE asking, or the WM cannot honour the state */
        XSizeHints *h = XAllocSizeHints();
        if (h) {
            h->flags = PMinSize;
            h->min_width = 320; h->min_height = 240;
            XSetWMNormalHints(g_xdpy, g_xwin, h);
            XFree(h);
        }
        ewmh_fullscreen(1);
        XMoveResizeWindow(g_xdpy, g_xwin, 0, 0, (unsigned)sw, (unsigned)sh);
        g_pending_w = sw; g_pending_h = sh;
        fprintf(stderr, "[hosted] fullscreen ON (%dx%d)\n", sw, sh);
    } else {
        ewmh_fullscreen(0);
        XMoveResizeWindow(g_xdpy, g_xwin, g_win_x, g_win_y, (unsigned)g_win_w, (unsigned)g_win_h);
        set_size_hints(g_win_w, g_win_h);
        g_pending_w = g_win_w; g_pending_h = g_win_h;
        fprintf(stderr, "[hosted] fullscreen OFF (%dx%d)\n", g_win_w, g_win_h);
    }
    XFlush(g_xdpy);
    return g_fullscreen;
}

int hosted_x11_fullscreen_on(void) { return g_fullscreen; }

/* Rebuild the presentation surface at a new size WITHOUT recreating the X window: a fresh window means a new
 * client id, lost input focus and key grabs that must be redone -- measured when the first fullscreen attempt
 * closed and re-opened the window. */
int hosted_x11_resize_surface(int w, int h) {
    if (!g_xdpy || !g_xwin || w <= 0 || h <= 0) return -1;
    x11_release_image_surface();
    if (x11_create_image_surface(w, h) != 0) return -1;
    XFlush(g_xdpy);
    return 0;
}

/* The hosted loop asks once per frame: 1 means the surface changed size and the guest must re-init. */
int hosted_x11_take_resize(int *w, int *h) {
    if (g_pending_w <= 0 || g_pending_h <= 0) return 0;
    if (w) *w = g_pending_w;
    if (h) *h = g_pending_h;
    g_pending_w = g_pending_h = 0;
    return 1;
}
