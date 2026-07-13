#include "hosted.h"
#include "hosted_internal.h"
#include "wayland_state.h"
#include "../kernel/vbe.h"
#include "../kernel/input.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_desktop.h"
#include "../gui/dosgui_startmenu.h"
#include "../gui/wubu_screenshot.h"
#include "../gui/wubu_theme.h"
#include "../bridge/bridge.h"
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "xdg-shell-client.header"
#include "primary-selection-client.header"

/* === hosted_wayland_shm.c -- SHM buffer pool + frame blit === */
/* SHM double-buffered pool state (owned by this module). */
shm_buffer_t    g_shm_bufs[SHM_BUFFERS];
int             g_cur_buf = 0;

/* g_hosted_state is defined in hosted.c; read here for frame dimensions. */
extern hosted_state_t *g_hosted_state;
static void shm_create_shared_memory(size_t size, int *fd, void **addr);
/* shm_buffer_create / shm_buffer_destroy are defined non-static below and
 * declared in hosted_internal.h for cross-module use (surface resize). */

static void shm_create_shared_memory(size_t size, int *fd, void **addr) {
    char template[] = "/tmp/wubu-shm-XXXXXX";
    *fd = mkstemp(template);
    if (*fd < 0) { perror("mkstemp"); return; }
    unlink(template);
    ftruncate(*fd, (off_t)size);
    *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
    if (*addr == MAP_FAILED) { perror("mmap"); close(*fd); *fd = -1; }
}

void shm_buffer_create(shm_buffer_t *buf, int w, int h) {
    memset(buf, 0, sizeof(*buf));
    buf->width = w;
    buf->height = h;
    buf->stride = w * 4;
    size_t size = (size_t)buf->stride * h;

    shm_create_shared_memory(size, &buf->fd, (void**)&buf->pixels);

    struct wl_shm_pool *pool = wl_shm_create_pool(g_wl.shm, buf->fd, (int32_t)size);
    buf->wl_buf = wl_shm_pool_create_buffer(pool, 0, (int32_t)w, (int32_t)h,
                                             (int32_t)buf->stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
}

void shm_buffer_destroy(shm_buffer_t *buf) {
    if (buf->wl_buf) { wl_buffer_destroy(buf->wl_buf); buf->wl_buf = NULL; }
    if (buf->pixels) { munmap(buf->pixels, (size_t)buf->stride * buf->height); buf->pixels = NULL; }
    if (buf->fd >= 0) { close(buf->fd); buf->fd = -1; }
}
/* Create the double-buffered SHM pool and wire it into g_wl. */
void wl_shm_init(hosted_state_t *state) {
    shm_buffer_create(&g_shm_bufs[0], state->width, state->height);
    shm_buffer_create(&g_shm_bufs[1], state->width, state->height);
    g_wl.width  = state->width;
    g_wl.height = state->height;
    g_wl.shm_buffer = g_shm_bufs[0].wl_buf;
    g_wl.shm_data   = g_shm_bufs[0].pixels;
}

/* Destroy the SHM pool + the wl_shm global. */
void wl_shm_term(void) {
    for (int i = 0; i < SHM_BUFFERS; i++) shm_buffer_destroy(&g_shm_bufs[i]);
    if (g_wl.shm) { wl_shm_destroy(g_wl.shm); g_wl.shm = NULL; }
}

/* Blit the VBE back-buffer into the current SHM buffer + commit.
 * Declared in hosted_internal.h and called by the hosted.c render loop. */
void hosted_wl_frame_render(void) {
    if (!g_wl.surface || !g_wl.compositor) return;
    shm_buffer_t *buf = &g_shm_bufs[g_cur_buf];
    if (!buf->pixels) return;

    VBEState *vs = vbe_state();
    if (vs && vs->fb) {
        memcpy(buf->pixels, vs->fb,
               (size_t)g_hosted_state->width * g_hosted_state->height * 4);
    }

    if (wubu_screenshot_has_active_region_selector()) {
        wubu_screenshot_render_region_selector(buf->pixels, buf->width, buf->height);
    }

    wl_surface_attach(g_wl.surface, buf->wl_buf, 0, 0);
    wl_surface_damage_buffer(g_wl.surface, 0, 0, buf->width, buf->height);
    wl_surface_commit(g_wl.surface);

    g_cur_buf = 1 - g_cur_buf;
}
