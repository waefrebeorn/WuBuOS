/*
 * wubu_inject.c -- inject input into the workspace window, in C11.
 *
 * The hosted desktop runs inside a weston window on the host display. To
 * drive it from an agent, events must be injected at the display server:
 * weston receives them as ordinary X11 input and delivers them to the
 * focused Wayland surface, so the desktop behaves exactly as if a person
 * were using it.
 *
 * XTEST is used (libXtst), declared here and loaded with dlopen so the tool
 * builds with no XTest header or development package:
 *
 *   wubu_inject move X Y        move the pointer (screen coordinates)
 *   wubu_inject click [BUTTON]  press and release (default 1 = left)
 *   wubu_inject key  KEYSYM     press and release a key (e.g. Return, a, F4)
 *   wubu_inject type "TEXT"     type a string
 *   wubu_inject focus           report the window under the pointer
 *
 * C11, minimal includes, self-contained.
 */
#include "../egojar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <time.h>

/* The three XTEST entry points we use (libXtst ABI). */
typedef int (*fn_fake_motion)(Display *, int, int, int, unsigned long);
typedef int (*fn_fake_button)(Display *, unsigned int, int, unsigned long);
typedef int (*fn_fake_key)(Display *, unsigned int, int, unsigned long);

static fn_fake_motion p_motion;
static fn_fake_button p_button;
static fn_fake_key    p_key;

static int xtest_load(void)
{
    void *lib = dlopen("libXtst.so.6", RTLD_LAZY);
    if (!lib) lib = dlopen("libXtst.so", RTLD_LAZY);
    if (!lib) return -1;
    p_motion = (fn_fake_motion) dlsym(lib, "XTestFakeMotionEvent");
    p_button = (fn_fake_button) dlsym(lib, "XTestFakeButtonEvent");
    p_key    = (fn_fake_key)    dlsym(lib, "XTestFakeKeyEvent");
    return (p_motion && p_button && p_key) ? 0 : -1;
}

static void small_delay(void)
{
    struct timespec ts = {0, 4000000L};   /* 4 ms between press and release */
    nanosleep(&ts, NULL);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr,
                "usage: wubu_inject move X Y | click [BUTTON] | "
                "key KEYSYM | type TEXT | focus\n");
        return 2;
    }
    if (xtest_load() != 0) {
        fprintf(stderr, "wubu_inject: libXtst not available (%s)\n", dlerror());
        return 1;
    }

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "wubu_inject: cannot open the display\n");
        return 1;
    }
    int screen = DefaultScreen(dpy);

    if (strcmp(argv[1], "focus") == 0) {
        Window root = RootWindow(dpy, screen);
        Window child;
        int root_x, root_y, win_x, win_y;
        unsigned int mask;
        XQueryPointer(dpy, root, &root, &child, &root_x, &root_y,
                      &win_x, &win_y, &mask);
        printf("%d %d\n", root_x, root_y);
        XCloseDisplay(dpy);
        return 0;
    }

    if (strcmp(argv[1], "move") == 0 && argc >= 4) {
        int x = atoi(argv[2]);
        int y = atoi(argv[3]);
        p_motion(dpy, screen, x, y, 0);
        XFlush(dpy);
        XCloseDisplay(dpy);
        return 0;
    }

    if (strcmp(argv[1], "click") == 0) {
        unsigned int button = (argc >= 3) ? (unsigned int)atoi(argv[2]) : 1;
        p_button(dpy, button, 1, 0);
        XFlush(dpy);
        small_delay();
        p_button(dpy, button, 0, 0);
        XFlush(dpy);
        XCloseDisplay(dpy);
        return 0;
    }

    if (strcmp(argv[1], "key") == 0 && argc >= 3) {
        KeySym ks = XStringToKeysym(argv[2]);
        if (ks == NoSymbol) {
            fprintf(stderr, "wubu_inject: unknown keysym '%s'\n", argv[2]);
            XCloseDisplay(dpy);
            return 1;
        }
        KeyCode kc = XKeysymToKeycode(dpy, ks);
        p_key(dpy, (unsigned int)kc, 1, 0);
        XFlush(dpy);
        small_delay();
        p_key(dpy, (unsigned int)kc, 0, 0);
        XFlush(dpy);
        XCloseDisplay(dpy);
        return 0;
    }

    if (strcmp(argv[1], "type") == 0 && argc >= 3) {
        /* Type character by character on a US layout: uppercase letters and
         * the shifted punctuation row need Shift held. */
        static const char shifted[] = "~!@#$%^&*()_+{}|:\"<>?";
        KeyCode shift = XKeysymToKeycode(dpy, XK_Shift_L);
        for (const char *p = argv[2]; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (c == ' ') {
                KeyCode kc = XKeysymToKeycode(dpy, XK_space);
                p_key(dpy, (unsigned int)kc, 1, 0);
                XFlush(dpy);
                small_delay();
                p_key(dpy, (unsigned int)kc, 0, 0);
                XFlush(dpy);
                continue;
            }
            char one[2] = { (char)c, '\0' };
            KeySym ks = XStringToKeysym(one);
            if (ks == NoSymbol) continue;
            KeyCode kc = XKeysymToKeycode(dpy, ks);
            int need_shift = ((c >= 'A' && c <= 'Z') || strchr(shifted, (char)c) != NULL);
            if (need_shift && shift) p_key(dpy, (unsigned int)shift, 1, 0);
            p_key(dpy, (unsigned int)kc, 1, 0);
            XFlush(dpy);
            small_delay();
            p_key(dpy, (unsigned int)kc, 0, 0);
            if (need_shift && shift) p_key(dpy, (unsigned int)shift, 0, 0);
            XFlush(dpy);
        }
        XCloseDisplay(dpy);
        return 0;
    }

    fprintf(stderr, "wubu_inject: unknown command '%s'\n", argv[1]);
    XCloseDisplay(dpy);
    return 2;
}
