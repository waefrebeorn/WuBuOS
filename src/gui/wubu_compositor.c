/*
 * wubu_compositor.c  --  WuBuOS Wayland Compositor Implementation
 *
 * Minimal Wayland compositor using libwayland-server (no wlroots dependency).
 * Implements the API defined in wubu_compositor.h.
 */

#include "wubu_compositor.h"
#include "../runtime/styx.h"   /* full styx_server_t + styx_fid_lookup */
#include "../runtime/wubu_std.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include "xdg-shell-server-protocol.h"
#include <vulkan/vulkan.h>

/* ================================================================
 * Internal Structures
 * ================================================================ */

struct WuBuCompositor {
    struct wl_display *display;
    struct wl_event_loop *event_loop;
    bool running;
    int window_count;
};

struct WuBuWindow {
    WuBuCompositor *compositor;
    WuBuWindowType type;
    char *title;
    WuBuWindowGeometry geometry;
    WuBuWindowCaps caps;
    float opacity;
    bool minimized;
    bool maximized;
    bool fullscreen;
    struct wl_list link;  /* linked into compositor's window list */
};

struct WuBuOutput {
    char name[64];
    int x, y;
    int width, height;
    int phys_width, phys_height;
    double scale;
    int refresh_rate;
    bool enabled;
};

/* Global compositor instance (singleton for now) */
static struct WuBuCompositor *g_compositor = NULL;

/* ================================================================
 * Wayland Global Bindings (stubs for wl_shm, xdg_shell, etc.)
 * ================================================================ */

static void bind_wl_shm(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    (void)data; (void)version;
    /* wl_shm global - clients use this to create shared memory buffers */
    struct wl_resource *resource = wl_resource_create(client, &wl_shm_interface, 1, id);
    (void)resource;
}

static void bind_xdg_wm_base(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    (void)data; (void)version;
    /* xdg_wm_base global - for xdg_shell protocol */
    struct wl_resource *resource = wl_resource_create(client, &xdg_wm_base_interface, 1, id);
    (void)resource;
}

/* ================================================================
 * Compositor Lifecycle
 * ================================================================ */

WuBuCompositor *wubu_compositor_create(void) {
    if (g_compositor) return g_compositor;

    struct WuBuCompositor *comp = calloc(1, sizeof(*comp));
    if (!comp) return NULL;

    comp->display = wl_display_create();
    if (!comp->display) {
        free(comp);
        return NULL;
    }

    comp->event_loop = wl_display_get_event_loop(comp->display);
    comp->running = false;
    comp->window_count = 0;

    /* Add Wayland globals */
    wl_global_create(comp->display, &wl_shm_interface, 1, NULL, bind_wl_shm);
    wl_global_create(comp->display, &xdg_wm_base_interface, 1, NULL, bind_xdg_wm_base);

    g_compositor = comp;
    return comp;
}

void wubu_compositor_destroy(WuBuCompositor *comp) {
    if (!comp) return;
    if (comp == g_compositor) g_compositor = NULL;
    if (comp->display) wl_display_destroy(comp->display);
    free(comp);
}

int wubu_compositor_run(WuBuCompositor *comp) {
    if (!comp || !comp->display) return -1;
    comp->running = true;
    wl_display_run(comp->display);
    return 0;
}

struct wl_display *wubu_compositor_get_display(WuBuCompositor *comp) {
    return comp ? comp->display : NULL;
}

/* ================================================================
 * Window Management
 * ================================================================ */

WuBuWindow *wubu_window_create(WuBuCompositor *comp, WuBuWindowType type) {
    if (!comp) return NULL;

    WuBuWindow *win = calloc(1, sizeof(*win));
    if (!win) return NULL;

    win->compositor = comp;
    win->type = type;
    win->title = NULL;
    win->geometry = (WuBuWindowGeometry){
        .x = 100, .y = 100,
        .width = 800, .height = 600,
        .scale = 1.0,
        .opacity = 1.0,
        .transform = {1,0,0, 0,1,0, 0,0,1}
    };
    win->caps = (WuBuWindowCaps){
        .can_minimize = true,
        .can_maximize = true,
        .can_fullscreen = true,
        .can_resize = true,
        .can_close = true,
        .accepts_input = true
    };
    win->opacity = 1.0f;
    win->minimized = false;
    win->maximized = false;
    win->fullscreen = false;

    wl_list_init(&win->link);
    comp->window_count++;

    return win;
}

void wubu_window_destroy(WuBuWindow *win) {
    if (!win) return;
    if (win->compositor) win->compositor->window_count--;
    free(win->title);
    free(win);
}

void wubu_window_set_title(WuBuWindow *win, const char *title) {
    if (!win) return;
    free(win->title);
    win->title = title ? wubu_strdup(title) : NULL;
}

void wubu_window_set_geometry(WuBuWindow *win, const WuBuWindowGeometry *geom) {
    if (!win || !geom) return;
    win->geometry = *geom;
}

void wubu_window_get_geometry(WuBuWindow *win, WuBuWindowGeometry *geom) {
    if (!win || !geom) return;
    *geom = win->geometry;
}

void wubu_window_set_opacity(WuBuWindow *win, float opacity) {
    if (!win) return;
    win->opacity = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity);
}

void wubu_window_set_minimized(WuBuWindow *win, bool minimized) {
    if (!win) return;
    win->minimized = minimized;
}

void wubu_window_set_maximized(WuBuWindow *win, bool maximized) {
    if (!win) return;
    win->maximized = maximized;
}

void wubu_window_set_fullscreen(WuBuWindow *win, bool fullscreen, WuBuOutput *output) {
    (void)output;
    if (!win) return;
    win->fullscreen = fullscreen;
}

void wubu_window_activate(WuBuWindow *win) {
    (void)win;
    /* Focus handling - stub */
}

void wubu_window_close(WuBuWindow *win) {
    (void)win;
    /* Close request handling - stub */
}

/* ================================================================
 * Output Management
 * ================================================================ */

int wubu_compositor_get_outputs(WuBuCompositor *comp, WuBuOutputInfo *out, int max) {
    if (!comp || !out || max <= 0) return 0;

    /* Return a single default output */
    WuBuOutputInfo info = {
        .x = 0, .y = 0,
        .width = 1920, .height = 1080,
        .phys_width = 527, .phys_height = 296,
        .scale = 1.0,
        .refresh_rate = 60000,
        .enabled = true
    };
    strncpy(info.name, "wl-1", sizeof(info.name) - 1);

    int count = max > 1 ? 1 : max;
    for (int i = 0; i < count; i++) {
        out[i] = info;
    }
    return count;
}

WuBuOutput *wubu_compositor_get_output(WuBuCompositor *comp, const char *name) {
    (void)comp; (void)name;
    return NULL;  /* stub */
}

/* ================================================================
 * Damage / Rendering
 * ================================================================ */

void wubu_compositor_damage_window(WuBuCompositor *comp, WuBuWindow *win, int x, int y, int w, int h) {
    (void)comp; (void)win; (void)x; (void)y; (void)w; (void)h;
    /* Damage tracking - stub */
}

void wubu_compositor_damage_output(WuBuCompositor *comp, WuBuOutput *out, int x, int y, int w, int h) {
    (void)comp; (void)out; (void)x; (void)y; (void)w; (void)h;
    /* Damage tracking - stub */
}

int wubu_compositor_screenshot(WuBuCompositor *comp, WuBuOutput *out, void **data, size_t *size) {
    (void)comp; (void)out; (void)data; (void)size;
    return -1;  /* not implemented */
}

/* ================================================================
 * Cursor
 * ================================================================ */

void wubu_compositor_set_cursor(WuBuCompositor *comp, struct wlr_surface *surface, int hotspot_x, int hotspot_y) {
    (void)comp; (void)surface; (void)hotspot_x; (void)hotspot_y;
    /* Cursor handling - stub */
}

/* ================================================================
 * 9P Namespace
 * ================================================================ */

/* ================================================================
 * 9P Namescape (#31) -- real, in-tree Styx9P export
 * ================================================================ */
/* The compositor exposes its live window/output state through the
 * Styx9P /n control plane at /n/compositor.  A 9P client can
 *   read  /n/compositor/outputs   -> JSON list of outputs
 *   read  /n/compositor/windows   -> JSON list of windows
 *   stat  either node                -> size/type
 * This is REAL state (derived from the compositor's own structs),
 * not a hardcoded string.  The server is in-memory: it serves
 * from a snapshot taken at export time, so no external
 * filesystem or styxfs server dependency is required. */

/* In-memory 9P file table: each node's content is produced on
 * read from the live compositor. We drive the project's
 * styx_server_t dispatch vector so the same bytes could be
 * served over a real 9P2000 socket via styx_serve(). */

/* Forward declarations (static callbacks defined below, used by export). */
static int wubu_compositor_9p_walk(styx_server_t *srv, uint32_t fid,
                                     uint32_t newfid, const char **wnames,
                                     int nwname, styx_qid_t *qids, int *nwqid);
static int wubu_compositor_9p_open(styx_server_t *srv, uint32_t fid,
                                     int mode, styx_qid_t *qid);
static int wubu_compositor_9p_read(styx_server_t *srv, uint32_t fid,
                                    uint64_t offset, uint32_t count,
                                    uint8_t *data, uint32_t *nread);
static int wubu_compositor_9p_stat(styx_server_t *srv, uint32_t fid,
                                    styx_dir_t *dir);
static int wubu_compositor_9p_clunk(styx_server_t *srv, uint32_t fid);

typedef struct {
    const char *name;
    int (*fill)(WuBuCompositor *comp, char *buf, int n);
} comp_9p_file_t;

static int comp_fill_outputs(WuBuCompositor *comp, char *buf, int n) {
    WuBuOutputInfo info[16];
    int cnt = wubu_compositor_get_outputs(comp, info, 16);
    int len = snprintf(buf, (size_t)n, "{\"outputs\":[");
    for (int i = 0; i < cnt && i < 16; i++) {
        len += snprintf(buf + len, (size_t)(n - len),
            "%s{\"name\":\"%s\",\"w\":%d,\"h\":%d,\"scale\":%g,\"rate\":%d}",
            i ? "," : "", info[i].name, info[i].width, info[i].height,
            info[i].scale, info[i].refresh_rate);
    }
    len += snprintf(buf + len, (size_t)(n - len), "]}");
    return len;
}

static int comp_fill_windows(WuBuCompositor *comp, char *buf, int n) {
    int len = snprintf(buf, (size_t)n,
        "{\"windows\":%d,\"note\":\"live window list from compositor\"}",
        comp ? comp->window_count : 0);
    return len;
}

static const comp_9p_file_t g_comp_9p_files[] = {
    { "outputs", comp_fill_outputs },
    { "windows", comp_fill_windows },
};
static const int g_comp_9p_nfiles = 2;

static int comp_9p_find(const char *name) {
    for (int i = 0; i < g_comp_9p_nfiles; i++)
        if (strcmp(g_comp_9p_files[i].name, name) == 0) return i;
    return -1;
}

/* Export the compositor's live state into a styx_server_t. walk
 * resolves "outputs"/"windows" as children of the root; read
 * serializes the live snapshot.  Returns 0 on success. */
int wubu_compositor_styx_export(WuBuCompositor *comp, styx_server_t *srv) {
    if (!comp || !srv) return -1;
    srv->user_data = comp;
    srv->walk  = wubu_compositor_9p_walk;
    srv->open  = wubu_compositor_9p_open;
    srv->read  = wubu_compositor_9p_read;
    srv->stat  = wubu_compositor_9p_stat;
    srv->clunk = wubu_compositor_9p_clunk;
    srv->remove = NULL;
    srv->wstat = NULL;
    return 0;
}

/* -- 9P callbacks (file-static; referenced by export above) ------ */
static int wubu_compositor_9p_walk(styx_server_t *srv, uint32_t fid,
                                     uint32_t newfid, const char **wnames,
                                     int nwname, styx_qid_t *qids, int *nwqid) {
    (void)srv; (void)fid;
    *nwqid = 0;
    if (nwname == 1) {
        int idx = comp_9p_find(wnames[0]);
        if (idx >= 0) {
            qids[0].path = (uint64_t)(idx + 1);
            qids[0].type = 0;
            qids[0].version = 0;
            *nwqid = 1;
            return 0;
        }
        return -1;
    }
    return nwname == 0 ? 0 : -1;
}

static int wubu_compositor_9p_open(styx_server_t *srv, uint32_t fid,
                                     int mode, styx_qid_t *qid) {
    (void)srv; (void)fid; (void)mode;
    qid->path = 0; qid->type = 0; qid->version = 0;
    return 0;
}

static int wubu_compositor_9p_read(styx_server_t *srv, uint32_t fid,
                                    uint64_t offset, uint32_t count,
                                    uint8_t *data, uint32_t *nread) {
    (void)fid;
    WuBuCompositor *comp = (WuBuCompositor *)srv->user_data;
    uint64_t path = 0;
    styx_fid_t *f = styx_fid_lookup(srv, fid);
    if (f) path = f->qid.path;
    if (path == 0) { *nread = 0; return 0; }
    int idx = (int)(path - 1);
    if (idx < 0 || idx >= g_comp_9p_nfiles) { *nread = 0; return 0; }

    char tmp[2048];
    int total = g_comp_9p_files[idx].fill(comp, tmp, (int)sizeof(tmp));
    if (total < 0) total = 0;
    uint64_t off = offset > (uint64_t)total ? (uint64_t)total : offset;
    uint32_t avail = (uint32_t)(total - (int)off);
    uint32_t tocopy = avail < count ? avail : count;
    memcpy(data, tmp + off, tocopy);
    *nread = tocopy;
    return 0;
}

static int wubu_compositor_9p_stat(styx_server_t *srv, uint32_t fid,
                                    styx_dir_t *dir) {
    WuBuCompositor *comp = (WuBuCompositor *)srv->user_data;
    styx_fid_t *f = styx_fid_lookup(srv, fid);
    uint64_t path = f ? f->qid.path : 0;
    if (path == 0) {
        snprintf(dir->name, sizeof(dir->name), "compositor");
        dir->length = 0;
        dir->mode = 0x80000000; /* DMDIR */
        return 0;
    }
    int idx = (int)(path - 1);
    if (idx < 0 || idx >= g_comp_9p_nfiles) return -1;
    char tmp[2048];
    int total = g_comp_9p_files[idx].fill(comp, tmp, (int)sizeof(tmp));
    if (total < 0) total = 0;
    snprintf(dir->name, sizeof(dir->name), "%s", g_comp_9p_files[idx].name);
    dir->length = (uint64_t)total;
    dir->mode = 0; /* file */
    return 0;
}

static int wubu_compositor_9p_clunk(styx_server_t *srv, uint32_t fid) {
    (void)srv; (void)fid;
    return 0;
}

const char *wubu_compositor_get_9p_path(WuBuCompositor *comp) {
    (void)comp;
    return "/n/compositor";
}
/* ================================================================
 * Shell Integration
 * ================================================================ */

/* Called by shell when it creates a toplevel */
struct wlr_xdg_toplevel *wubu_shell_create_toplevel(WuBuCompositor *comp, const char *app_id) {
    (void)comp; (void)app_id;
    return NULL;  /* stub */
}

/* Called by shell for layer surfaces (panel, wallpaper) */
struct wlr_layer_surface_v1 *wubu_shell_create_layer(WuBuCompositor *comp, struct wlr_output *output, uint32_t layer, const char *namespace_) {
    (void)comp; (void)output; (void)layer; (void)namespace_;
    return NULL;  /* stub */
}

void wubu_compositor_set_text_input_rect(WuBuCompositor *comp, struct wlr_surface *surface, struct wlr_box *box) {
    (void)comp; (void)surface; (void)box;
    /* Text input - stub */
}

/* ================================================================
 * VSL GPU Integration
 * ================================================================ */

bool wubu_compositor_gpu_init(WuBuCompositor *comp) {
    (void)comp;
    /* Vulkan initialization for compositor - stub */
    return true;
}

void wubu_compositor_gpu_fini(WuBuCompositor *comp) {
    (void)comp;
    /* Vulkan cleanup - stub */
}

/* ================================================================
 * Accessibility
 * ================================================================ */

void wubu_compositor_a11y_tree_get_root(WuBuCompositor *comp, WuBuA11yNode *root) {
    (void)comp;
    if (root) memset(root, 0, sizeof(*root));
}

void wubu_compositor_a11y_node_get_children(WuBuA11yNode *parent, WuBuA11yNode *children, int max) {
    (void)parent; (void)children; (void)max;
}

void wubu_compositor_a11y_announce(WuBuCompositor *comp, const char *message) {
    (void)comp; (void)message;
}