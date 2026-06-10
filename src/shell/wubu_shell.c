/*
 * wubu_shell.c — WuBuOS Unified GUI Shell Runner
 *
 * Cell 207: Integration — runs the Win98 GUI shell (WM, desktop, taskbar,
 * start menu, REPL) on any platform backend (X11, DRM/KMS, Wayland, VBE).
 * Uses the unified display/input APIs from wubu_metal.h.
 *
 * This is the core integration point: same shell, same binary, all platforms.
 */

#include "wubu_shell.h"
#include "wubu_metal.h"

#include "../kernel/vbe.h"
#include "../kernel/memory.h"
#include "../kernel/tasking.h"
#include "../kernel/interrupt.h"
#include "../kernel/input.h"
#include "../kernel/wubu_gaad.h"
#include "../gui/wm.h"
#include "../gui/desktop.c"
#include "../gui/taskbar.c"
#include "../gui/theme.c"
#include "../gui/startmenu.h"
#include "../gui/gui_dbuf.h"
#include "../apps/repl.h"
#include "../bridge/bridge.h"
#include "../runtime/styx.h"
#include "../runtime/wubu_host_exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

/* ── Styx In-Memory Filesystem (copied from hosted.c) ────────────── */

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

/* ── Styx Server Callbacks (for namespace server) ────────────────── */

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

static int styx_open_cb(styx_server_t *srv, uint32_t fid, uint32_t mode, styx_qid_t *qid) {
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

/* ── Shell State ─────────────────────────────────────────────────── */

typedef struct {
    int width;
    int height;
    bool running;
    bool is_temple_mode;
    int styx_fd;
} WubuShell;

static WubuShell g_shell = {0};

/* ── Styx Namespace Setup ────────────────────────────────────────── */

static void shell_init_styx(WubuShell *sh) {
    /* Build Styx namespace: /wubu, /dev, /prog, /cons */
    fs_add_dir("wubu");
    fs_add_dir("dev");
    fs_add_dir("prog");
    fs_add_file("cons", (const uint8_t*)"WuBuOS — Styx namespace on unified shell\n", 40);

    uint8_t demo_wubu[64];
    memset(demo_wubu, 0, sizeof(demo_wubu));
    memcpy(demo_wubu, "WUBU!\0\1\2", 8);
    demo_wubu[8] = 1;
    fs_add_file("hello.wubu", demo_wubu, sizeof(demo_wubu));

    /* Unix socket for Styx namespace */
    char sock_path[128];
    snprintf(sock_path, sizeof(sock_path), "/tmp/wubu-styx-%d.sock", getpid());
    struct sockaddr_un addr;
    sh->styx_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sh->styx_fd >= 0) {
        unlink(sock_path);
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
        if (bind(sh->styx_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            listen(sh->styx_fd, 5);
            fprintf(stderr, "Styx: %s\n", sock_path);
        } else {
            close(sh->styx_fd); sh->styx_fd = -1;
        }
    }
}

/* ── REPL Launch Callback ────────────────────────────────────────── */

static void shell_repl_launch(void) {
    repl_start(g_shell.width, g_shell.height);
}

/* ── Render Win98 Desktop to VBE Back Buffer ─────────────────────── */

extern void desktop_draw(int screen_w, int screen_h, int taskbar_h);
extern void taskbar_draw(int screen_w, int screen_h);
extern int  taskbar_height(void);

static void shell_render(WubuShell *sh) {
    WubuDisplay *disp = wubu_disp_state();

    /* 1. Desktop background + icons */
    int tb_h = taskbar_height();
    desktop_draw(sh->width, sh->height, tb_h);

    /* 2. Windows (WM renders all visible windows) */
    wm_render(NULL, sh->width, sh->height);

    /* 3. Start menu (if open) */
    if (startmenu_is_open()) {
        startmenu_draw();
    }

    /* 4. Taskbar (always on top) */
    taskbar_draw(sh->width, sh->height);

    /* 5. Swap VBE front/back buffers */
    vbe_swap();

    /* 6. Copy VBE front buffer to display backend framebuffer */
    VBEState *vs = vbe_state();
    if (vs && vs->fb && disp->vbe_back) {
        memcpy(disp->vbe_back, vs->fb,
               (size_t)sh->width * sh->height * 4);
    }
}

/* ── Main Shell Event Loop ───────────────────────────────────────── */

int wubu_shell_run(int width, int height) {
    g_shell.width = width;
    g_shell.height = height;
    g_shell.running = true;
    g_shell.is_temple_mode = false;

    /* Initialize kernel subsystems */
    mem_init(1024 * 1024);
    vbe_init(width, height);
    input_init();

    /* Initialize GUI shell */
    wm_init(width, height);
    startmenu_init();
    taskbar_init();

    /* Register start menu entries */
    startmenu_add_entry("Programs", SM_PROGRAM, NULL);
    startmenu_add_entry("Temple REPL", SM_SYSTEM, shell_repl_launch);
    startmenu_add_entry("Separator", SM_SEPARATOR, NULL);
    startmenu_add_entry("Shut Down", SM_SYSTEM, NULL);

    /* Create default desktop window */
    wm_create_window(100, 80, 400, 300, "TempleOS HolyC");

    fprintf(stderr, "WuBuOS: kernel + GUI shell initialized\n");

    /* Initialize Styx namespace */
    shell_init_styx(&g_shell);

    /* Initialize display backend (wubu_metal unified API) */
    if (wubu_disp_init(width, height) != 0) {
        fprintf(stderr, "Display init failed\n");
        return -1;
    }

    /* Main event loop */
    struct timespec last;
    clock_gettime(CLOCK_MONOTONIC, &last);
    const long frame_ns = 1000000000L / 30;  /* 30 FPS */

    while (g_shell.running) {
        WubuDisplay *disp = wubu_disp_state();

        /* Poll input events */
        wubu_input_poll();  /* Unified input poll (evdev/X11) */

        /* Dispatch kernel input queue to WM (Cell 202) */
        KeyEvent kev;
        while (input_key_poll(&kev)) {
            wm_handle_key(kev.keycode, kev.modifiers);
        }
        MouseEvent mev;
        while (input_mouse_poll(&mev)) {
            wm_handle_mouse(mev.x, mev.y, mev.buttons, mev.buttons ? 0 : 2);
        }

        /* Check for mode toggle (Ctrl+Alt+T) */
        BridgeMode mode = bridge_get_mode();
        if (mode == MODE_TEMPLE && !g_shell.is_temple_mode) {
            g_shell.is_temple_mode = true;
            fprintf(stderr, "Switching to Temple mode\n");
        } else if (mode == MODE_GUI && g_shell.is_temple_mode) {
            g_shell.is_temple_mode = false;
            fprintf(stderr, "Switching to GUI mode\n");
        }

        /* Render */
        if (g_shell.is_temple_mode) {
            /* Temple REPL mode: black background */
            if (disp->vbe_back) {
                for (int i = 0; i < width * height; i++)
                    disp->vbe_back[i] = 0x00000000;
            }
        } else {
            /* GUI mode: full Win98 desktop */
            shell_render(&g_shell);
        }

        /* Flip to display backend */
        wubu_disp_flip();

        /* Frame rate limiting */
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

/* ── Shell Shutdown ──────────────────────────────────────────────── */

void wubu_shell_shutdown(void) {
    fprintf(stderr, "WuBuOS shell shutdown...\n");

    /* GUI shell shutdown */
    wm_shutdown();
    vbe_shutdown();
    input_shutdown();

    /* Styx cleanup */
    if (g_shell.styx_fd >= 0) {
        close(g_shell.styx_fd);
        char p[128];
        snprintf(p, sizeof(p), "/tmp/wubu-styx-%d.sock", getpid());
        unlink(p);
    }

    /* Display backend shutdown (unified API) */
    wubu_disp_shutdown();

    memset(&g_shell, 0, sizeof(g_shell));
}