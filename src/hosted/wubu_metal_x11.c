/* wubu_metal_x11.c -- WuBuOS X11 display backend (extracted from wubu_metal.c).
 * Mirror of the original wubu_metal.c include set (proven to compile) plus the
 * extern globals this backend touches. C11, no god headers. */

#include "wubu_metal.h"
#include "wubu_metal_audio.h"
#include "../audio/wubu_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <dlfcn.h>

extern WubuDisplay g_display;
#ifdef WUBU_USE_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static Display *g_x11_dpy = NULL;
static Window   g_x11_win = 0;
static GC       g_x11_gc = 0;

int wubu_x11_init(int width, int height) {
    g_x11_dpy = XOpenDisplay(NULL);
    if (!g_x11_dpy) {
        fprintf(stderr, "XOpenDisplay failed\n");
        return -1;
    }

    int screen = DefaultScreen(g_x11_dpy);
    g_x11_win = XCreateSimpleWindow(g_x11_dpy, RootWindow(g_x11_dpy, screen),
                                     0, 0, width, height, 0,
                                     BlackPixel(g_x11_dpy, screen),
                                     BlackPixel(g_x11_dpy, screen));

    XSelectInput(g_x11_dpy, g_x11_win,
                 ExposureMask | KeyPressMask | KeyReleaseMask |
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                 StructureNotifyMask);

    XMapWindow(g_x11_dpy, g_x11_win);
    XFlush(g_x11_dpy);

    g_x11_gc = XCreateGC(g_x11_dpy, g_x11_win, 0, NULL);

    g_display.backend      = DISP_X11;
    g_display.width        = width;
    g_display.height       = height;
    g_display.x11_display  = g_x11_dpy;
    g_display.x11_window   = g_x11_win;
    g_display.x11_gc       = g_x11_gc;
    g_display.vbe_back     = calloc(width * height, sizeof(uint32_t));

    printf("[metal] X11 initialized: %dx%d\n", width, height);
    return 0;
}

void wubu_x11_shutdown(void) {
    if (g_x11_gc) XFreeGC(g_x11_dpy, g_x11_gc);
    if (g_x11_win) XDestroyWindow(g_x11_dpy, g_x11_win);
    if (g_x11_dpy) XCloseDisplay(g_x11_dpy);
    if (g_display.vbe_back) free(g_display.vbe_back);
    g_x11_dpy = NULL; g_x11_win = 0; g_x11_gc = 0; g_display.vbe_back = NULL;
}

void wubu_x11_flip(void) {
    if (g_x11_dpy && g_x11_win && g_display.vbe_back) {
        XImage *img = XCreateImage(g_x11_dpy, DefaultVisual(g_x11_dpy, DefaultScreen(g_x11_dpy)),
                                    24, ZPixmap, 0, (char*)g_display.vbe_back,
                                    g_display.width, g_display.height, 32, 0);
        if (img) {
            XPutImage(g_x11_dpy, g_x11_win, g_x11_gc, img, 0, 0, 0, 0, g_display.width, g_display.height);
            XFlush(g_x11_dpy);
            XDestroyImage(img);
        }
    }
}

int wubu_x11_set_mode(int width, int height, int refresh_hz) {
    (void)refresh_hz;
    if (g_x11_win && g_x11_dpy) {
        XResizeWindow(g_x11_dpy, g_x11_win, width, height);
        if (g_display.vbe_back) {
            free(g_display.vbe_back);
            g_display.vbe_back = calloc(width * height, sizeof(uint32_t));
        }
        g_display.width = width; g_display.height = height;
    }
    return 0;
}
#else
/* Try dlopen libX11 at runtime */
int wubu_x11_init(int width, int height) {
    void *x11_lib = dlopen("libX11.so.6", RTLD_LAZY);
    if (!x11_lib) {
        fprintf(stderr, "X11 not available (libX11 not found)\n");
        return -1;
    }
    dlclose(x11_lib);
    (void)width; (void)height;
    return -1;  /* Not implemented without X11 headers */
}
void wubu_x11_shutdown(void) {}
void wubu_x11_flip(void) {}
int wubu_x11_set_mode(int width, int height, int refresh_hz) {
    (void)width; (void)height; (void)refresh_hz;
    return -1;
}
#endif
