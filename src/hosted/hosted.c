/*
 * hosted.c — WuBuOS Hosted Mode Launcher (Inferno emu-style)
 *
 * WuBuOS as a clickable Linux binary — the "blob OS".
 * Runs as a regular Linux program via X11 window.
 * Full OS environment: VBE framebuffer, kernel services,
 * Styx/9P namespace on Unix socket.
 *
 * Cell 200: ZealOS kernel runs in-process.
 *   - Kernel subsystems: mem_init, vbe_init, tasking
 *   - GUI shell: WM, desktop, taskbar, start menu
 *   - Input routing: X11 → WM → focused window
 *   - Render pipeline: desktop + windows + taskbar → vbe_swap → X11 blit
 *
 * Build: make hosted  →  src/hosted/wubu
 */
#include "hosted.h"
#include "../runtime/styx.h"
#include "../kernel/vbe.h"
#include "../kernel/memory.h"
#include "../kernel/tasking.h"
#include "../kernel/input.h"
#include "../gui/wm.h"
#include "../gui/startmenu.h"
#include "../apps/repl.h"
#include "../bridge/bridge.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

/* ── X11 type aliases ───────────────────────────────────────────── */

typedef Display    XDpy;
typedef Window     XWin;
typedef GC         XGc;

/* ── Forward Declarations ───────────────────────────────────────── */

static int handle_x11_event(hosted_state_t *state, XEvent *ev);
static int  handle_key(hosted_state_t *state, KeySym ks, int pressed);
static int  handle_mouse(hosted_state_t *state, int x, int y, int btn, int pressed);
static void repl_launch_callback(void);  /* Launch HolyC REPL */
static void input_dispatch(void);         /* Poll kernel queue -> WM */

/* ── In-memory filesystem for Styx namespace ────────────────────── */

#define STYXFS_MAX_FILES 64

typedef struct {
    char     name[256];
    uint8_t  qtype;
    uint64_t path;
    uint64_t length;
    uint8_t  data[8192];
    uint32_t data_len;
} styxfs_file_t;

static styxfs_file_t g_fs[STYXFS_MAX_FILES];
static int g_nfiles = 0;
static uint64_t g_next_path = 1;

/* Global hosted state for REPL callback access */
static hosted_state_t *g_hosted_state = NULL;

static int fs_add_dir(const char *name) {
    if (g_nfiles >= STYXFS_MAX_FILES) return -1;
    styxfs_file_t *f = &g_fs[g_nfiles++];
    strncpy(f->name, name, sizeof(f->name) - 1);
    f->qtype = STX_QTDIR;
    f->path = g_next_path++;
    f->length = 0;
    return 0;
}

static int fs_add_file(const char *name, const uint8_t *data, uint32_t len) {
    if (g_nfiles >= STYXFS_MAX_FILES) return -1;
    styxfs_file_t *f = &g_fs[g_nfiles++];
    strncpy(f->name, name, sizeof(f->name) - 1);
    f->qtype = STX_QTFILE;
    f->path = g_next_path++;
    f->length = len;
    if (data && len > 0) {
        uint32_t clen = len < sizeof(f->data) ? len : sizeof(f->data);
        memcpy(f->data, data, clen);
        f->data_len = clen;
    }
    return 0;
}

static void fs_reset(void) {
    g_nfiles = 0;
    g_next_path = 1;
}

/* ── Styx Server Callbacks ──────────────────────────────────────── */

static styx_fid_t *find_fid(styx_server_t *srv, uint32_t fid) {
    for (int i = 0; i < STYX_MAX_FIDS; i++)
        if (srv->fids[i].in_use && srv->fids[i].fid == fid)
            return &srv->fids[i];
    return NULL;
}

static int styx_attach_cb(styx_server_t *srv, uint32_t fid, const char *aname) {
    (void)aname;
    styx_fid_t *f = NULL;
    for (int i = 0; i < STYX_MAX_FIDS; i++) {
        if (!srv->fids[i].in_use) { f = &srv->fids[i]; break; }
    }
    if (!f) return -1;
    memset(f, 0, sizeof(*f));
    f->in_use = 1;
    f->fid = fid;
    f->qid.type = STX_QTDIR;
    f->qid.path = 0;
    f->qid.version = 1;
    return 0;
}

static int styx_walk_cb(styx_server_t *srv, uint32_t fid, uint32_t newfid,
                         const char **wname, int nwname,
                         styx_qid_t *qids, int *nwqid) {
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;
    if (nwname == 0) {
        styx_fid_t *nf = NULL;
        for (int i = 0; i < STYX_MAX_FIDS; i++)
            if (!srv->fids[i].in_use) { nf = &srv->fids[i]; break; }
        if (!nf) return -1;
        *nf = *f; nf->fid = newfid;
        qids[0] = f->qid; *nwqid = 1;
        return 0;
    }
    styx_fid_t *nf = NULL;
    int walked = 0;
    for (int i = 0; i < nwname; i++) {
        styxfs_file_t *file = NULL;
        for (int j = 0; j < g_nfiles; j++)
            if (strcmp(g_fs[j].name, wname[i]) == 0) { file = &g_fs[j]; break; }
        if (!file) { *nwqid = walked; return walked > 0 ? 0 : -1; }
        qids[walked].type = file->qtype;
        qids[walked].path = file->path;
        qids[walked].version = 1;
        walked++;
        if (!nf) {
            for (int j = 0; j < STYX_MAX_FIDS; j++)
                if (!srv->fids[j].in_use) { nf = &srv->fids[j]; break; }
            if (!nf) return -1;
            *nf = *f; nf->fid = newfid;
        }
        nf->qid.type = file->qtype;
        nf->qid.path = file->path;
    }
    *nwqid = walked;
    return 0;
}

static int styx_open_cb(styx_server_t *srv, uint32_t fid, int mode, styx_qid_t *qid) {
    (void)mode;
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;
    *qid = f->qid;
    return 0;
}

static int styx_read_cb(styx_server_t *srv, uint32_t fid, uint64_t offset,
                         uint32_t count, uint8_t *data, uint32_t *nread) {
    (void)srv;
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;
    for (int i = 0; i < g_nfiles; i++) {
        if (g_fs[i].path == f->qid.path) {
            if (g_fs[i].qtype & STX_QTDIR) { *nread = 0; return 0; }
            if (offset >= g_fs[i].data_len) { *nread = 0; return 0; }
            uint32_t avail = g_fs[i].data_len - (uint32_t)offset;
            *nread = (count < avail) ? count : avail;
            if (*nread > 0) memcpy(data, g_fs[i].data + offset, *nread);
            return 0;
        }
    }
    return -1;
}

static int styx_stat_cb(styx_server_t *srv, uint32_t fid, styx_dir_t *dir) {
    styx_fid_t *f = find_fid(srv, fid);
    if (!f) return -1;
    memset(dir, 0, sizeof(*dir));
    dir->qid = f->qid;
    dir->mode = (f->qid.type & STX_QTDIR) ? STX_DMDIR : 0;
    dir->mode |= 0555;
    if (f->qid.path == 0) {
        strcpy(dir->name, "/");
    } else {
        for (int i = 0; i < g_nfiles; i++)
            if (g_fs[i].path == f->qid.path)
                { strncpy(dir->name, g_fs[i].name, STYX_MAX_FNAME - 1); break; }
    }
    strcpy(dir->uid, "wubu");
    strcpy(dir->gid, "wubu");
    return 0;
}

/* ── REPL Launch Callback ───────────────────────────────────────── */

static void repl_launch_callback(void) {
    /* Launch HolyC REPL — it creates its own window */
    if (g_hosted_state) {
        repl_start(g_hosted_state->width, g_hosted_state->height);
    }
}

/* ── Hosted Init ────────────────────────────────────────────────── */

int hosted_init(hosted_state_t *state, int argc, char **argv) {
    g_hosted_state = state;  /* Set global for REPL callback */
    memset(state, 0, sizeof(*state));
    state->width = HOSTED_DEFAULT_W;
    state->height = HOSTED_DEFAULT_H;
    state->mode = HMODE_GUI;
    state->running = true;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-w") == 0 && i + 2 < argc) {
            state->width = atoi(argv[++i]); state->height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0) state->mode = HMODE_TEMPLE;
        else if (strcmp(argv[i], "-c") == 0) state->mode = HMODE_CONSOLE;
        else if (strcmp(argv[i], "-h") == 0) state->mode = HMODE_HEADLESS;
        else if (strcmp(argv[i], "-f") == 0) state->fullscreen = true;
    }
    
    state->depth = 32;
    state->fb_pitch = state->width * 4;
    state->framebuffer = (uint32_t*)calloc((size_t)state->width * state->height, 4);
    if (!state->framebuffer) { fprintf(stderr, "OOM\n"); return -1; }
    
    /* ── Kernel subsystem init (Cell 200: ZealOS in-process) ────── */
    mem_init(1024 * 1024);
    vbe_init(state->width, state->height);
    input_init();

    /* ── GUI shell init (WM + start menu) ────────────────────────── */
    wm_init(state->width, state->height);
    startmenu_init();
    
    /* Register start menu entries */
    startmenu_add_entry("Programs", SM_PROGRAM, NULL);
    startmenu_add_entry("Temple REPL", SM_SYSTEM, repl_launch_callback);
    startmenu_add_entry("Separator", SM_SEPARATOR, NULL);
    startmenu_add_entry("Shut Down", SM_SYSTEM, NULL);
    
    /* Create default desktop windows */
    wm_create_window(100, 80, 400, 300, "TempleOS HolyC");
    
    fprintf(stderr, "WuBuOS: kernel + GUI shell initialized\n");
    
    /* Init X11 (skip if headless) */
    if (state->mode != HMODE_HEADLESS) {
        XDpy *dpy = XOpenDisplay(NULL);
        if (!dpy) {
            fprintf(stderr, "No X display. Use -h for headless.\n");
            free(state->framebuffer);
            return -1;
        }
        
        int scr = DefaultScreen(dpy);
        XWin root = RootWindow(dpy, scr);
        
        XSetWindowAttributes attrs;
        attrs.background_pixel = BlackPixel(dpy, scr);
        attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                           ButtonPressMask | ButtonReleaseMask |
                           PointerMotionMask | StructureNotifyMask;
        
        XWin win = XCreateWindow(dpy, root, 0, 0,
                                  state->width, state->height, 0,
                                  CopyFromParent, InputOutput,
                                  CopyFromParent, CWBackPixel | CWEventMask,
                                  &attrs);
        XStoreName(dpy, win, HOSTED_WIN_TITLE);
        XGc gc = XCreateGC(dpy, win, 0, NULL);
        XImage *img = XCreateImage(dpy, DefaultVisual(dpy, scr),
                                    24, ZPixmap, 0,
                                    (char*)state->framebuffer,
                                    state->width, state->height, 32,
                                    state->fb_pitch);
        XMapWindow(dpy, win);
        XFlush(dpy);
        
        /* Enable close button via WM_DELETE_WINDOW */
        Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(dpy, win, &wm_delete, 1);
        
        state->display_ptr = (void*)dpy;
        state->window_ptr = (void*)(uintptr_t)win;
        state->gc_ptr = (void*)(uintptr_t)gc;
        state->ximage_ptr = (void*)img;
        state->x11_fd = XConnectionNumber(dpy);
        
        fprintf(stderr, "WuBuOS: %dx%d window\n", state->width, state->height);
    }
    
    /* Build Styx namespace */
    fs_add_dir("wubu");
    fs_add_dir("dev");
    fs_add_dir("prog");
    fs_add_file("cons", (const uint8_t*)"WuBuOS blob OS — Styx namespace\n", 33);
    
    uint8_t demo_wubu[64];
    memset(demo_wubu, 0, sizeof(demo_wubu));
    memcpy(demo_wubu, "WUBU!\0\1\2", 8);
    demo_wubu[8] = 1;
    fs_add_file("hello.wubu", demo_wubu, sizeof(demo_wubu));
    
    /* Unix socket for Styx namespace */
    char sock_path[128];
    snprintf(sock_path, sizeof(sock_path), "/tmp/wubu-styx-%d.sock", getpid());
    struct sockaddr_un addr;
    state->styx_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (state->styx_fd >= 0) {
        unlink(sock_path);
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
        if (bind(state->styx_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            listen(state->styx_fd, 5);
            fprintf(stderr, "Styx: %s\n", sock_path);
        } else {
            close(state->styx_fd); state->styx_fd = -1;
        }
    }
    
    return 0;
}

/* ── Render the full Win98 desktop to VBE back buffer ──────────── */

/* Forward decl for desktop_draw and taskbar_draw (in gui/) */
extern void desktop_draw(int screen_w, int screen_h, int taskbar_h);
extern void taskbar_draw(int screen_w, int screen_h);
extern int  taskbar_height(void);

static void render_desktop(hosted_state_t *state) {
    /* 1. Desktop background + icons */
    int tb_h = taskbar_height();
    desktop_draw(state->width, state->height, tb_h);
    
    /* 2. Windows (WM renders all visible windows) */
    wm_render(NULL, state->width, state->height);
    
    /* 3. Start menu (if open) */
    if (startmenu_is_open()) {
        startmenu_draw();
    }
    
    /* 4. Taskbar (always on top) */
    taskbar_draw(state->width, state->height);
}

/* ── Main Event Loop ────────────────────────────────────────────── */

int hosted_run(hosted_state_t *state) {
    fprintf(stderr, "WuBuOS running. Mode: %s\n",
            state->mode == HMODE_GUI ? "GUI" :
            state->mode == HMODE_TEMPLE ? "Temple" :
            state->mode == HMODE_CONSOLE ? "Console" : "Headless");
    
    struct timespec last;
    clock_gettime(CLOCK_MONOTONIC, &last);
    const long frame_ns = 1000000000L / 30;
    
    while (state->running) {
        XDpy *dpy = (XDpy*)state->display_ptr;
        if (dpy) {
            while (XPending(dpy)) {
                XEvent ev;
                XNextEvent(dpy, &ev);
                handle_x11_event(state, &ev);
            }
        }
        
        /* Cell 202: Dispatch input from kernel queue to WM */
        input_dispatch();
        
        if (state->framebuffer) {
            /* ── Render Win98 desktop to VBE back buffer ─── */
            if (state->mode == HMODE_GUI) {
                render_desktop(state);
                vbe_swap();
                
                /* Copy VBE front buffer to X11 framebuffer */
                VBEState *vs = vbe_state();
                if (vs && vs->fb) {
                    memcpy(state->framebuffer, vs->fb,
                           (size_t)state->width * state->height * 4);
                }
            } else if (state->mode == HMODE_TEMPLE) {
                /* Temple REPL mode: black background */
                for (int i = 0; i < state->width * state->height; i++)
                    state->framebuffer[i] = 0x00000000;
            } else {
                /* Console/Headless: gray */
                for (int i = 0; i < state->width * state->height; i++)
                    state->framebuffer[i] = 0x00808080;
            }
        }
        
        if (dpy && state->window_ptr && state->gc_ptr) {
            XWin win = (XWin)(uintptr_t)state->window_ptr;
            XGc gc = (XGc)(uintptr_t)state->gc_ptr;
            XPutImage(dpy, win, gc, (XImage*)state->ximage_ptr,
                      0, 0, 0, 0, state->width, state->height);
            XFlush(dpy);
        }
        
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - last.tv_sec) * 1000000000L +
                       (now.tv_nsec - last.tv_nsec);
        if (elapsed < frame_ns) {
            struct timespec slp = {0, frame_ns - elapsed};
            nanosleep(&slp, NULL);
        }
        clock_gettime(CLOCK_MONOTONIC, &last);
    }
    return 0;
}

/* ── Shutdown ───────────────────────────────────────────────────── */

void hosted_shutdown(hosted_state_t *state) {
    fprintf(stderr, "WuBuOS shutdown...\n");
    
    /* GUI shell shutdown */
    wm_shutdown();
    vbe_shutdown();
    input_shutdown();
    
    if (state->styx_fd >= 0) {
        close(state->styx_fd);
        char p[128];
        snprintf(p, sizeof(p), "/tmp/wubu-styx-%d.sock", getpid());
        unlink(p);
    }
    XDpy *dpy = (XDpy*)state->display_ptr;
    if (dpy) {
        XImage *img = (XImage*)state->ximage_ptr;
        if (img) { img->data = NULL; XDestroyImage(img); }
        XWin win = (XWin)(uintptr_t)state->window_ptr;
        XGc gc = (XGc)(uintptr_t)state->gc_ptr;
        if (gc) XFreeGC(dpy, gc);
        if (win) XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
    }
    if (state->framebuffer) free(state->framebuffer);
    memset(state, 0, sizeof(*state));
}

void hosted_blit(hosted_state_t *state) {
    if (!state->display_ptr || !state->window_ptr || !state->gc_ptr) return;
    XDpy *dpy = (XDpy*)state->display_ptr;
    XWin win = (XWin)(uintptr_t)state->window_ptr;
    XGc gc = (XGc)(uintptr_t)state->gc_ptr;
    XPutImage(dpy, win, gc, (XImage*)state->ximage_ptr,
              0, 0, 0, 0, state->width, state->height);
    XFlush(dpy);
}

void hosted_set_mode(hosted_state_t *state, hosted_mode_t mode) {
    state->mode = mode;
    fprintf(stderr, "Mode: %s\n", mode == HMODE_GUI ? "GUI" :
            mode == HMODE_TEMPLE ? "Temple" : "Other");
}

/* ── Event Handlers ─────────────────────────────────────────────── */

static int handle_x11_event(hosted_state_t *state, XEvent *ev) {
    switch (ev->type) {
    case Expose:
        if (ev->xexpose.count == 0) hosted_blit(state);
        break;
    case KeyPress:
        handle_key(state, XLookupKeysym(&ev->xkey, 0), 1);
        break;
    case KeyRelease:
        handle_key(state, XLookupKeysym(&ev->xkey, 0), 0);
        break;
    case ButtonPress:
        handle_mouse(state, ev->xbutton.x, ev->xbutton.y, ev->xbutton.button, 1);
        break;
    case ButtonRelease:
        handle_mouse(state, ev->xbutton.x, ev->xbutton.y, ev->xbutton.button, 0);
        break;
    case MotionNotify:
        handle_mouse(state, ev->xmotion.x, ev->xmotion.y, 0, 0);
        break;
    case ClientMessage:
        /* WM_DELETE_WINDOW */
        state->running = false;
        break;
    case DestroyNotify:
        state->running = false;
        break;
    case ConfigureNotify:
        state->width = ev->xconfigure.width;
        state->height = ev->xconfigure.height;
        break;
    }
    return 0;
}

static int handle_key(hosted_state_t *state, KeySym ks, int pressed) {
    uint32_t wu_key = 0;
    switch (ks) {
    case XK_Return:    wu_key = 0x1C; break;
    case XK_Escape:    wu_key = 0x01; break;
    case XK_BackSpace: wu_key = 0x0E; break;
    case XK_Tab:       wu_key = 0x0F; break;
    case XK_Control_L: wu_key = 0x1D; break;
    case XK_Shift_L:   wu_key = 0x2A; break;
    case XK_Alt_L:     wu_key = 0x38; break;
    case XK_space:     wu_key = 0x39; break;
    case XK_Left:      wu_key = 0xE04B; break;
    case XK_Up:        wu_key = 0xE048; break;
    case XK_Right:     wu_key = 0xE04D; break;
    case XK_Down:      wu_key = 0xE050; break;
    case XK_t:
        /* Ctrl+Alt+T: DOS flip — toggle GUI ↔ Temple mode */
        if (state->key_map[0x1D] && state->key_map[0x38]) {
            bridge_toggle_mode();
            hosted_set_mode(state, bridge_get_mode() == MODE_TEMPLE
                                    ? HMODE_TEMPLE : HMODE_GUI);
        }
        wu_key = 0x14; /* 't' scancode */
        break;
    default:
        if (ks >= XK_a && ks <= XK_z) wu_key = 0x1E + (uint32_t)(ks - XK_a);
        else if (ks >= XK_0 && ks <= XK_9) wu_key = 0x0B + (uint32_t)(ks - XK_0);
        else if (ks >= XK_F1 && ks <= XK_F12) wu_key = 0x3B + (uint32_t)(ks - XK_F1);
        break;
    }

    /* Push to kernel input queue (Cell 202: unified input dispatch) */
    if (wu_key) {
        KeyEvent ev = {0};
        ev.scancode = wu_key;
        ev.keycode = wu_key;
        ev.kind = pressed ? KEY_EVENT_DOWN : KEY_EVENT_UP;
        uint32_t mods = 0;
        if (state->key_map[0x1D]) mods |= MOD_CTRL;
        if (state->key_map[0x2A]) mods |= MOD_SHIFT;
        if (state->key_map[0x38]) mods |= MOD_ALT;
        ev.modifiers = mods;
        input_key_push(ev);
    }

    /* Track modifier key state for Ctrl+Alt+T detection */
    if (ks == XK_Control_L || ks == XK_Control_R)
        state->key_map[0x1D] = pressed;
    if (ks == XK_Alt_L || ks == XK_Alt_R)
        state->key_map[0x38] = pressed;
    if (ks == XK_Shift_L || ks == XK_Shift_R)
        state->key_map[0x2A] = pressed;
    (void)state;
    return wu_key;
}

static int handle_mouse(hosted_state_t *state, int x, int y, int btn, int pressed) {
    int dx = x - state->mouse_x;
    int dy = y - state->mouse_y;
    state->mouse_x = x;
    state->mouse_y = y;
    if (btn) state->mouse_buttons = btn;

    /* Push to kernel input queue (Cell 202: unified input dispatch) */
    if (btn && btn != 0) {
        MouseEvent ev = {0};
        ev.dx = dx;
        ev.dy = dy;
        ev.buttons = btn == 1 ? 1 : (btn == 3 ? 2 : 4); /* Left=1, Right=2, Middle=4 */
        ev.scroll = 0;
        input_mouse_push(ev);
    } else if (btn == 0) {
        /* Motion event - only push if position changed */
        if (dx != 0 || dy != 0) {
            MouseEvent ev = {0};
            ev.dx = dx;
            ev.dy = dy;
            ev.buttons = state->mouse_buttons;
            ev.scroll = 0;
            input_mouse_push(ev);
        }
    }

    /* Start menu click handling (keep for GUI) */
    if (btn == 1 && pressed && startmenu_is_open()) {
        if (startmenu_is_inside(x, y)) {
            int idx = startmenu_handle_mouse(x, y);
            if (idx >= 0) startmenu_click(idx);
        } else {
            startmenu_close();
        }
    }

    /* Start button click (bottom-left corner of taskbar) */
    if (btn == 1 && pressed && !startmenu_is_open()) {
        if (x >= 4 && x <= 64 && y >= state->height - 28 + 3 && y <= state->height - 3) {
            startmenu_open(0, state->height - taskbar_height() - startmenu_get_height());
        }
    }

    (void)pressed;
    return 0;
}

/* Poll kernel input queue and dispatch to WM (Cell 202) */
static void input_dispatch(void) {
    KeyEvent kev;
    while (input_key_poll(&kev)) {
        wm_handle_key(kev.keycode, kev.modifiers);
    }
    MouseEvent mev;
    while (input_mouse_poll(&mev)) {
        /* Queue already stores absolute position in x,y */
        wm_handle_mouse(mev.x, mev.y, mev.buttons, mev.buttons ? 0 : 2);
    }
}

/* ── Styx Namespace API ─────────────────────────────────────────── */

int hosted_styx_init(hosted_state_t *state, const char *socket_path) {
    (void)socket_path;
    /* Styx server callbacks are used when accept() handles connections */
    styx_server_t srv;
    styx_init(&srv);
    srv.attach = styx_attach_cb;
    srv.walk = styx_walk_cb;
    srv.open = styx_open_cb;
    srv.read = styx_read_cb;
    srv.stat = styx_stat_cb;
    (void)state;
    return 0;
}

int hosted_styx_register_wubu(hosted_state_t *state,
                               const char *name,
                               const uint8_t *data, uint32_t size) {
    (void)state;
    return fs_add_file(name, data, size);
}

/* Expose fs_reset for tests */
void hosted_fs_reset(void) { fs_reset(); }

/* ── Query API for behavioral tests ────────────────────────────── */

/* Check if kernel subsystems are initialized */
int hosted_kernel_ready(void) {
    VBEState *vs = vbe_state();
    return (vs && vs->fb && vs->back && vs->width > 0) ? 1 : 0;
}

/* Check if WM has windows */
int hosted_wm_has_windows(void) {
    return wm_window_count() > 0 ? 1 : 0;
}

/* ── Main ───────────────────────────────────────────────────────── */

#ifndef WUBU_HOSTED_TEST
int main(int argc, char **argv) {
    hosted_state_t state;
    if (hosted_init(&state, argc, argv) != 0) return 1;
    int ret = hosted_run(&state);
    hosted_shutdown(&state);
    return ret;
}
#endif
