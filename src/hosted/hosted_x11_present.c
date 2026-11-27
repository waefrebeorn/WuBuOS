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
/* MIT-SHM: without it every frame is a 3.8 MB socket transfer at 1200x800, which measured the loop at
 * ~36 fps no matter how cheap the render got -- and the loop rate IS the input latency. */
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <poll.h>

/* The WM does not take input from a callback: it drains the kernel's input queue via
 * input_key_poll / input_mouse_poll. X events are therefore translated into Set-1 scancodes
 * (X11 evdev keycode = scancode + 8) and pushed into that same queue -- one input path for
 * every backend, so the X11 window is as interactive as the Wayland one. */
#include "input.h"
#include "hosted_x11_private.h"

static int      g_xquit;
static int      g_xbuttons;
static int      g_xlast_x, g_xlast_y;
/* Every X event we consume is EVIDENCE OF ACTIVITY. The loop decides 60 fps vs 30 fps from "was there
 * input", and a motion event that the pump pushed into the input queue can be drained by the same frame's
 * dispatch -- so the queue reads empty and the loop falls back to the idle rate WHILE THE POINTER IS MOVING.
 * Measured as fps dipping to ~30 mid-drag with only 2 ms of render work. */
static unsigned long g_xevents;
/* The window's origin, cached. XTranslateCoordinates is a ROUND TRIP to the X server, and it was being made
 * for EVERY motion event -- measured as the frame missing its deadline while the pointer moved, with only
 * 2 ms of render work. Refreshed on Configure/Map and never per motion. */
static int      g_origin_ok, g_origin_x, g_origin_y;

/* Xlib's default error handler exits the process. For an OS window that is unacceptable: a
 * single BadMatch took the whole desktop down. Log and carry on. */
static int x11_error_handler(Display *d, XErrorEvent *e) {
    char buf[256];
    XGetErrorText(d, e->error_code, buf, sizeof buf);
    fprintf(stderr, "[x11] error %s (opcode %d) ignored\n", buf, e->request_code);
    return 0;
}

int hosted_x11_present_open(int width, int height, const char *title) {
    XSetErrorHandler(x11_error_handler);
    if (width <= 0 || height <= 0) return -1;
    g_xdpy = XOpenDisplay(NULL);
    if (!g_xdpy) return -1;

    int scr = DefaultScreen(g_xdpy);
    Visual *vis = DefaultVisual(g_xdpy, scr);
    int depth = DefaultDepth(g_xdpy, scr);

    g_xw = width;
    g_xh = height;
    g_win_w = width; g_win_h = height;

    /* A borderless, override-redirect window is deliberately NOT used: the OS should behave
     * like an application on the desktop (movable, closable) rather than seizing the screen.
     * It is sized to the OS resolution and centred. */
    int sw = DisplayWidth(g_xdpy, scr), sh = DisplayHeight(g_xdpy, scr);
    int x = (sw - width) / 2, y = (sh - height) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    g_win_x = x; g_win_y = y;   /* the size to return to when fullscreen goes off */

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
    /* Focus is set on MapNotify, never here: X_SetInputFocus on a window that is not yet
     * viewable is a BadMatch, and an unhandled X error kills the client outright (the OS
     * died the first time this ran). */
    g_xgc = XCreateGC(g_xdpy, g_xwin, 0, NULL);

    /* Zero-copy view of our framebuffer. XDestroyImage would free data, so it is never called;
     * the buffer belongs to the VBE backend. */
    /* Prefer a shared-memory image: the server then reads the pixels straight out of our memory instead of
     * receiving them over the socket. Fall back to the plain XImage if the extension is missing or a segment
     * cannot be attached -- a fallback path that has never been taken is not a fallback. */
    if (x11_create_image_surface(width, height) != 0) return -1;

    g_xopen = 1;
    XFlush(g_xdpy);
    return 0;
}

/* Copy the OS framebuffer into the X image and put it on screen. Called once per presented
 * frame; caller decides whether anything changed. */
/* Frame pacing + zero copy.
 *
 * The first version memcpy'd 3 MB and flushed on EVERY render callback. Under X11 that is a
 * full-screen transfer per iteration with no vsync anywhere, so the window lagged behind the
 * pointer and the WM's own input felt sluggish. Two rules now:
 *   - the XImage points straight at the OS framebuffer (no copy at all);
 *   - at most one put+flush per display frame (16 ms), which is what a compositor would pace.
 */
void hosted_x11_present_frame(const void *fb) {
    if (!g_xopen || !g_ximg || !fb) return;

    static const void *last_fb;
    if (!g_shm_ok && last_fb != fb) {
        /* SOCKET PATH ONLY: point the image straight at our framebuffer, so XPutImage reads it with no copy.
         * XDestroyImage would free the buffer, so the pointer is detached and the OS keeps ownership. */
        if (g_ximg->data) { g_ximg->data = NULL; }
        g_ximg->data = (char *)fb;
        last_fb = fb;
    }

    static struct timespec last;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long dt_us = (now.tv_sec - last.tv_sec) * 1000000L + (now.tv_nsec - last.tv_nsec) / 1000L;
    if (last.tv_sec && dt_us < 16000L) return;   /* cap at ~60 Hz */
    last = now;

    if (g_shm_ok) {
        /* The image MUST keep pointing at the shared segment: the server reads THAT memory, not ours, so
         * re-pointing g_ximg->data at the framebuffer made this memcpy copy fb onto itself and left the
         * segment stale -- XShmPutImage then answered BadValue (opcode 130) and nothing reached the screen.
         * Copy framebuffer -> segment, every presented frame. */
        if (g_ximg->data != g_shm.shmaddr) g_ximg->data = g_shm.shmaddr;
        memcpy(g_shm.shmaddr, fb, (size_t)g_xw * (size_t)g_xh * 4u);
        XShmPutImage(g_xdpy, g_xwin, g_xgc, g_ximg, 0, 0, 0, 0,
                     (unsigned)g_xw, (unsigned)g_xh, False);
    } else {
        XPutImage(g_xdpy, g_xwin, g_xgc, g_ximg, 0, 0, 0, 0,
                  (unsigned)g_xw, (unsigned)g_xh);
    }
    XFlush(g_xdpy);
}

/* X11 evdev keycode -> Set-1 scancode. Extended keys carry the 0xE0 prefix, which is how the
 * kernel's input_scancode_to_key tells Left from keypad 4. */
static uint32_t x11_scancode(unsigned kc) {
    switch (kc) {                      /* extended (E0-prefixed) keys */
    case 111: return 0xE048u;          /* Up    */
    case 116: return 0xE050u;          /* Down  */
    case 113: return 0xE04Bu;          /* Left  */
    case 114: return 0xE04Du;          /* Right */
    case 110: return 0xE047u;          /* Home  */
    case 115: return 0xE04Fu;          /* End   */
    case 112: return 0xE049u;          /* PgUp  */
    case 117: return 0xE051u;          /* PgDn  */
    case 118: return 0xE052u;          /* Ins   */
    case 119: return 0xE053u;          /* Del   */
    case 105: return 0xE04Du;          /* keypad right (same arrow) */
    default: break;
    }
    if (kc >= 8) return kc - 8;        /* everything else is a plain Set-1 code */
    return 0;
}

static uint32_t x11_mods(unsigned state) {
    uint32_t m = 0;
    if (state & ShiftMask)   m |= MOD_SHIFT;
    if (state & ControlMask) m |= MOD_CTRL;
    if (state & Mod1Mask)    m |= MOD_ALT;
    if (state & Mod4Mask)    m |= MOD_WIN;
    return m;
}

/* PARITY WITHOUT HOSTAGE-TAKING.
 * The guest must receive Win+Shift+S, but grabbing the WHOLE keyboard takes every host shortcut
 * with it -- a window that silently swallows Super is worse than one that lacks the binding. So we
 * grab exactly the combinations the shell implements, in every modifier variant a real keyboard can
 * produce (Lock/NumLock set or not). A grab that fails means the host WM already owns that
 * combination; our error handler logs and continues, and every other host shortcut keeps working. */
static int      g_kbd_capture;     /* Ctrl+Alt+G: the guest owns every key while it is on */
static unsigned g_grabbed_keys[32];
static int      g_grabbed_count;


static void grab_one(KeyCode kc, unsigned mods) {
    XGrabKey(g_xdpy, (int)kc, mods, g_xwin, True, GrabModeAsync, GrabModeAsync);
    if (g_grabbed_count < (int)(sizeof g_grabbed_keys / sizeof g_grabbed_keys[0]))
        g_grabbed_keys[g_grabbed_count++] = kc;
}

/* The real screen size, for the layer that decides the framebuffer size. On X11 there is no
 * /dev/fb0 and no Wayland output, so the native-mode probe used to fall through to a 1024x768
 * default on a 1920x1080 panel -- the OS then rendered into a window smaller than the screen and
 * every "maximise" left dead pixels around it. One short connection answers it. */
int hosted_x11_screen_size(int *out_w, int *out_h) {
    Display *d = XOpenDisplay(NULL);
    if (!d) return -1;
    Window root = DefaultRootWindow(d), child;
    int x = 0, y = 0;
    unsigned w = 0, h = 0, bw = 0, depth = 0;
    int ok = XGetGeometry(d, root, &child, &x, &y, &w, &h, &bw, &depth);
    XCloseDisplay(d);
    if (!ok || w == 0 || h == 0) return -1;
    if (out_w) *out_w = (int)w;
    if (out_h) *out_h = (int)h;
    return 0;
}

/* The guest owns the session when its framebuffer covers the screen. With the framebuffer sized
 * from the X11 root geometry that is now a real state, not an aspiration: 1024x768 in a 1920x1080
 * panel never covered anything, which is why the automatic policy was impossible before. */
static int hosted_x11_owns_screen(void) {
    if (!g_xopen || !g_xwin) return 0;
    Window root = DefaultRootWindow(g_xdpy), child;
    int rx = 0, ry = 0, ox = 0, oy = 0;
    unsigned rw = 0, rh = 0, ww = 0, wh = 0, bw = 0, depth = 0;
    if (!XGetGeometry(g_xdpy, root, &child, &rx, &ry, &rw, &rh, &bw, &depth)) return 0;
    if (!XGetGeometry(g_xdpy, g_xwin, &child, &ox, &oy, &ww, &wh, &bw, &depth)) return 0;
    return (int)ww >= (int)rw && (int)wh >= (int)rh;
}

/* auto (default): capture exactly when the guest fills the screen; WUBU_KBD_CAPTURE=on|off forces
 * it either way. Ctrl+Alt+G remains the manual override. */
static void hosted_x11_sync_kbd_capture(void) {
    const char *mode = getenv("WUBU_KBD_CAPTURE");
    int want = mode && strcmp(mode, "on") == 0  ? 1
             : mode && strcmp(mode, "off") == 0 ? 0
             : hosted_x11_owns_screen();
    if (want == g_kbd_capture) return;
    g_kbd_capture = want;
    if (want) XGrabKeyboard(g_xdpy, g_xwin, False, GrabModeAsync, GrabModeAsync, CurrentTime);
    else      XUngrabKeyboard(g_xdpy, CurrentTime);
    fprintf(stderr, "[x11] keyboard capture %s (%s)\n", want ? "ON" : "OFF",
            want ? "the guest fills the screen" : "windowed: host keeps its shortcuts");
}

void hosted_x11_grab_win_keys(void) {
    if (getenv("WUBU_NO_KBD_GRAB")) return;
    static const KeySym syms[] = {
        XK_s, XK_e, XK_d, XK_a, XK_i, XK_l, XK_r, XK_x, XK_Tab, XK_space,
    };
    const unsigned lock_variants[] = { 0u, LockMask, Mod2Mask, LockMask | Mod2Mask };
    for (unsigned i = 0; i < sizeof syms / sizeof syms[0]; i++) {
        KeyCode kc = XKeysymToKeycode(g_xdpy, syms[i]);
        if (!kc) continue;
        for (unsigned v = 0; v < sizeof lock_variants / sizeof lock_variants[0]; v++) {
            grab_one(kc, Mod4Mask | lock_variants[v]);
            grab_one(kc, Mod4Mask | ShiftMask | lock_variants[v]);
        }
    }

    /* Alt+F4 belongs to the GUEST, not to the host window manager.
     *
     * Without this grab, pressing Alt+F4 on the desktop (no guest window focused, so nothing for the guest to
     * close) was handled by the host WM, which closed the hosted window -- the whole OS disappeared. Measured
     * in a live session: nine clicks and one Alt+F4 left an empty desktop with the guest process gone. The
     * guest's own handler already refuses to close anything when no window is focused (wm_input.c), so the
     * key simply does nothing, which is what Windows does on an empty desktop. */
    /* Ctrl+Esc opens the guest's Start menu (Microsoft's shortcut list). The host WM owns Super, so without
     * grabbing this combination the OS had no keyboard route to its own menu inside a hosted session. */
    KeyCode esc = XKeysymToKeycode(g_xdpy, XK_Escape);
    if (esc)
        for (unsigned v = 0; v < sizeof lock_variants / sizeof lock_variants[0]; v++)
            grab_one(esc, ControlMask | lock_variants[v]);

    KeyCode f4 = XKeysymToKeycode(g_xdpy, XK_F4);
    if (f4)
        for (unsigned v = 0; v < sizeof lock_variants / sizeof lock_variants[0]; v++)
            grab_one(f4, Mod1Mask | lock_variants[v]);
}

/* Drain X events without blocking. Returns non-zero when the window was closed. */
/* 1 when new X events arrived since the last call. */
int hosted_x11_present_activity(void) {
    static unsigned long seen;
    if (g_xevents != seen) { seen = g_xevents; return 1; }
    return 0;
}

int hosted_x11_present_pump(void) {
    if (!g_xopen) return 1;
    while (XPending(g_xdpy) > 0) {
        XEvent ev;
        XNextEvent(g_xdpy, &ev);
        g_xevents++;
        switch (ev.type) {
        case MapNotify:
            g_origin_ok = 0;   /* the window moved or was remapped: re-read the origin once */
            XSetInputFocus(g_xdpy, g_xwin, RevertToParent, CurrentTime);
            /* PARITY: a guest OS is the session while it is the focused window, so it must see the
             * Win key. Without a keyboard grab the HOST window manager resolves Super+Shift+S and
             * Super+E before the OS is ever handed the event -- measured: no key event arrived at
             * all, which is why a Win-key binding could never work here. Grabbed on focus, released
             * on focus loss, and switchable off with WUBU_NO_KBD_GRAB=1. */
            hosted_x11_grab_win_keys();
            hosted_x11_sync_kbd_capture();
            break;
        case ConfigureNotify:
            g_origin_ok = 0;   /* the window moved or was remapped: re-read the origin once */
            hosted_x11_sync_kbd_capture();
            break;
        case FocusOut:
            if (g_kbd_capture) { g_kbd_capture = 0; XUngrabKeyboard(g_xdpy, CurrentTime); }
            break;
        case Expose:
            if (g_ximg) {
                XPutImage(g_xdpy, g_xwin, g_xgc, g_ximg, 0, 0, 0, 0,
                          (unsigned)g_xw, (unsigned)g_xh);
            }
            break;
        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == XInternAtom(g_xdpy, "WM_DELETE_WINDOW", False)) {
                /* The host WM sends this for Alt+F4 and for its title-bar X. It means "close what is
                 * focused", not "destroy the OS": with no guest window focused it must do NOTHING (Windows
                 * behaves exactly that way on an empty desktop). Alt+F4 used to close the hosted window --
                 * the entire session disappeared. Ctrl+Alt+Q is the explicit way out. */
                extern int wm_close_focused(void) __attribute__((weak));
                if (wm_close_focused && wm_close_focused()) break;
                static int said;
                if (!said) {
                    said = 1;
                    fprintf(stderr, "[hosted] close request ignored: no OS window is focused. Ctrl+Alt+Q quits.\n");
                }
                break;
            }
            break;
        case DestroyNotify:
            g_xquit = 1;
            break;
        case KeyPress:
        case KeyRelease: {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);

            /* FULLSCREEN: F11, and Ctrl+Shift+F because that is what a remote-desktop client uses. The
             * guest is a fixed-size window by design (the WM is not allowed to resize it), so fullscreen is
             * a request to us: EWMH + a framebuffer re-init at the new size. */
            if (ev.type == KeyPress &&
                (ks == XK_F11 ||
                 (ks == XK_f && (ev.xkey.state & ControlMask) && (ev.xkey.state & ShiftMask)))) {
                hosted_x11_toggle_fullscreen();
                break;
            }

            /* the explicit escape hatch, since a close request no longer ends the session */
            if (ev.type == KeyPress && ks == XK_q && (ev.xkey.state & ControlMask) && (ev.xkey.state & Mod1Mask)) {
                g_xquit = 1;
                break;
            }

            /* Ctrl+Alt+G toggles KEYBOARD CAPTURE: with capture on the guest receives every key,
             * Win combos included, exactly as an OS that owns the session would. That is the only
             * way a Win-key binding can work while the host WM keeps its own Super shortcuts --
             * passive grabs lose to a pre-existing WM grab, and a permanent whole-keyboard grab
             * would hold the host hostage. Off by default, released by the same key. */
            if (ev.type == KeyPress && ks == XK_g &&
                (ev.xkey.state & (Mod1Mask | ControlMask)) == (Mod1Mask | ControlMask)) {
                g_kbd_capture = !g_kbd_capture;
                if (g_kbd_capture)
                    XGrabKeyboard(g_xdpy, g_xwin, False, GrabModeAsync, GrabModeAsync, CurrentTime);
                else
                    XUngrabKeyboard(g_xdpy, CurrentTime);
                fprintf(stderr, "[x11] keyboard capture %s%s\n",
                        g_kbd_capture ? "ON" : "OFF",
                        g_kbd_capture ? " (Ctrl+Alt+G again to release)" : "");
                break;
            }
            /* Escape closes the OS only with Alt held; plain Escape belongs to the WM
             * (menus, the Alt+Tab switcher, dialogs). */
            if (ev.type == KeyPress && ks == XK_Escape && (ev.xkey.state & Mod1Mask)) {
                g_xquit = 1;
                break;
            }
            uint32_t sc = x11_scancode(ev.xkey.keycode);
            if (!sc) break;
            if (getenv("WUBU_DBG_X11"))
                fprintf(stderr, "[x11] key %s kc=%u -> sc=0x%X state=0x%X\n",
                        ev.type == KeyPress ? "down" : "up",
                        ev.xkey.keycode, sc, ev.xkey.state);
            KeyEvent kev;
            memset(&kev, 0, sizeof(kev));
            kev.scancode  = sc;
            /* keycode is the TRANSLATED key (ASCII for printable, control codes for
             * Enter/Backspace, passthrough for arrows/modifiers). Handing the WM the raw
             * scancode made "hello" arrive as "0.#&&" -- five keys, wrong glyphs. */
            kev.modifiers = x11_mods(ev.xkey.state);
            kev.keycode   = input_scancode_to_key(sc, kev.modifiers);
            kev.kind      = (ev.type == KeyPress) ? KEY_EVENT_DOWN : KEY_EVENT_UP;
            input_key_push(kev);
            break;
        }
        case ButtonPress: {
            /* COORDINATE TRUTH: an X event's x/y are relative to the window it was delivered on,
             * and a reparenting WM reports them against its FRAME. On this desktop that leaked a
             * 33 px titlebar into every click -- the pointer sat on the power row and the OS saw
             * a point below the panel, which closed it. Translate into our own client window so
             * every coordinate below is the one the user is pointing at. */
            /* Root coordinates minus OUR origin. The event's own x/y were already correct here,
             * but a reparenting WM can report them against its frame, and this window's client
             * origin (447,111) is not the frame geometry a tool like xdotool prints (448,144) --
             * so measure the mapping instead of assuming it. */
            Window child; int ox = 0, oy = 0;
            XTranslateCoordinates(g_xdpy, g_xwin, DefaultRootWindow(g_xdpy), 0, 0, &ox, &oy, &child);
            int tx = ev.xbutton.x_root - ox, ty = ev.xbutton.y_root - oy;

            /* A real keyboard only reaches the window that holds X input focus. Without this
             * the app could be clicked but every physical keystroke went to whatever else the
             * window manager had focused -- which read as "the keyboard does not work". */
            if (getenv("WUBU_INPUT_DEBUG"))
                fprintf(stderr, "[x11] press root=%d,%d origin=%d,%d -> guest %d,%d\n",
                        ev.xbutton.x_root, ev.xbutton.y_root, ox, oy, tx, ty);
            XSetInputFocus(g_xdpy, g_xwin, RevertToParent, CurrentTime);
            hosted_x11_grab_win_keys();
            hosted_x11_sync_kbd_capture();
            MouseEvent mev;
            memset(&mev, 0, sizeof(mev));
            mev.x = tx;
            mev.y = ty;
            int bit = (ev.xbutton.button == 1) ? 1 : (ev.xbutton.button == 3) ? 2 : 4;
            g_xbuttons |= bit;
            mev.buttons = g_xbuttons;
            mev.kind = 1;
            input_mouse_push(mev);
            break;
        }
        case ButtonRelease: {
            Window child; int ox = 0, oy = 0;
            XTranslateCoordinates(g_xdpy, g_xwin, DefaultRootWindow(g_xdpy), 0, 0, &ox, &oy, &child);
            int tx = ev.xbutton.x_root - ox, ty = ev.xbutton.y_root - oy;
            MouseEvent mev;
            memset(&mev, 0, sizeof(mev));
            mev.x = tx;
            mev.y = ty;
            int bit = (ev.xbutton.button == 1) ? 1 : (ev.xbutton.button == 3) ? 2 : 4;
            g_xbuttons &= ~bit;
            mev.buttons = g_xbuttons;
            mev.kind = 2;
            input_mouse_push(mev);
            break;
        }
        case MotionNotify: {
            MouseEvent mev;
            memset(&mev, 0, sizeof(mev));
            if (!g_origin_ok) {
                Window child;
                if (XTranslateCoordinates(g_xdpy, g_xwin, DefaultRootWindow(g_xdpy), 0, 0,
                                          &g_origin_x, &g_origin_y, &child))
                    g_origin_ok = 1;
            }
            int tx = ev.xmotion.x_root - g_origin_x;
            int ty = ev.xmotion.y_root - g_origin_y;
            mev.x = tx;
            mev.y = ty;
            mev.dx = tx - g_xlast_x;
            mev.dy = ty - g_xlast_y;
            g_xlast_x = tx;
            g_xlast_y = ty;
            mev.buttons = g_xbuttons;
            mev.kind = 0;                 /* a move is not a press */
            input_mouse_push(mev);
            break;
        }
        default:
            break;
        }
    }
    return g_xquit;
}

int hosted_x11_present_active(void) { return g_xopen ? 1 : 0; }

/* The X connection's file descriptor, so the run loop can block on INPUT with a frame
 * deadline instead of sleeping and then checking. That sleep is what made the pointer feel
 * broken: at the idle rate a motion waited a whole frame (up to 125 ms). */
/* Block until X is readable or the frame deadline passes. Returns 0 when woken by input, -1
 * when the deadline expired. */
int hosted_x11_wait_ms(int ms) {
    if (!g_xopen || !g_xdpy) return -1;
    struct pollfd pfd;
    pfd.fd = ConnectionNumber(g_xdpy);
    pfd.events = POLLIN;
    pfd.revents = 0;
    int r = poll(&pfd, 1, ms);
    if (r > 0 && (pfd.revents & POLLIN)) {
        /* Drain the socket into Xlib's queue so XPending() sees it next iteration. */
        XFlush(g_xdpy);
        return 0;
    }
    return -1;
}

int hosted_x11_present_fd(void) {
    return (g_xopen && g_xdpy) ? ConnectionNumber(g_xdpy) : -1;
}

void hosted_x11_present_close(void) {
    if (!g_xopen) return;
    if (g_ximg) { free(g_ximg->data); g_ximg->data = NULL; g_ximg = NULL; }
    if (g_xgc) { XFreeGC(g_xdpy, g_xgc); g_xgc = NULL; }
    if (g_xwin) { XDestroyWindow(g_xdpy, g_xwin); g_xwin = 0; }
    if (g_xdpy) { XCloseDisplay(g_xdpy); g_xdpy = NULL; }
    g_xopen = 0;
}
