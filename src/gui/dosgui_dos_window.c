/*
 * dosgui_dos_window.c -- Render a WuBuOS 16-bit DOS process in a desktop
 * window. The window blits the guest's captured VGA framebuffer (RGBA from
 * wubu_dos_proc_frame_capture) and forwards keyboard/mouse input back into the
 * guest via the proc's control channel.
 *
 * Self-contained GUI leaf: depends only on dosgui_wm.h + wubu_dos_proc.h.
 * No god-header. C11.
 */
#include "dosgui_wm.h"
#include "wubu_dos_proc.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>     /* snprintf */

typedef struct {
    WubuDosProc *proc;
    uint8_t     *fb;       /* latest RGBA frame */
    int          fb_w, fb_h;
    char         styx_path[256];
} DosWindowCtx;

/* Blit an RGBA framebuffer region into the window at (x,y,w,h), nearest-neighbor
 * scaled to fit the destination rectangle. */
static void blit_rgba(uint32_t *dst, int dst_w, int dst_h,
                      const uint8_t *src, int src_w, int src_h,
                      int dx, int dy, int dw, int dh) {
    if (!src || src_w <= 0 || src_h <= 0 || dw <= 0 || dh <= 0) return;
    for (int y = 0; y < dh; y++) {
        int sy = (y * src_h) / dh;
        if (sy >= src_h) sy = src_h - 1;
        for (int x = 0; x < dw; x++) {
            int sx = (x * src_w) / dw;
            if (sx >= src_w) sx = src_w - 1;
            int px = dx + x, py = dy + y;
            if (px < 0 || py < 0 || px >= dst_w || py >= dst_h) continue;
            const uint8_t *p = src + (sy * src_w + sx) * 4;
            dst[py * dst_w + px] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                                    ((uint32_t)p[2] << 16) | ((uint32_t)255 << 24);
        }
    }
}

static void dos_win_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    DosWindowCtx *ctx = (DosWindowCtx *)win->user_data;
    if (!ctx) return;

    /* Pull a fresh frame (this also keeps the Styx `screen` node fresh if the
     * process was mounted). */
    int w = 0, h = 0;
    if (ctx->styx_path[0])
        wubu_dos_proc_frame_to_styx(ctx->proc, "/tmp/wubu_styx", ctx->fb, &w, &h);
    else
        wubu_dos_proc_frame_capture(ctx->proc, ctx->fb, &w, &h);
    ctx->fb_w = w; ctx->fb_h = h;

    /* Clear client area (bounds-guarded against the destination fb). */
    for (int y = 0; y < win->h; y++) {
        int py = win->y + y;
        if (py < 0 || py >= fb_h) continue;
        for (int x = 0; x < win->w; x++) {
            int px = win->x + x;
            if (px < 0 || px >= fb_w) continue;
            fb[py * fb_w + px] = 0x00102030u;
        }
    }

    if (w > 0 && h > 0)
        blit_rgba(fb, fb_w, fb_h, ctx->fb, w, h,
                  win->x + 2, win->y + DOSGUI_TITLE_H,
                  win->w - 4, win->h - DOSGUI_TITLE_H - 2);
}

static void dos_win_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)mods;
    DosWindowCtx *ctx = (DosWindowCtx *)win->user_data;
    if (!ctx) return;
    char kbuf[8];
    /* Map common WuBuOS key codes to QEMU qcodes. */
    const char *qc = NULL;
    switch (key) {
        case 0x0D: qc = "ret"; break;
        case 0x1B: qc = "esc"; break;
        case 0x09: qc = "tab"; break;
        case 0x08: qc = "backspace"; break;
        case 0x20: qc = "spc"; break;
        case 0x7F: qc = "delete"; break;
        default:
            if (key >= 'a' && key <= 'z') { snprintf(kbuf, sizeof(kbuf), "%c", (char)key); qc = kbuf; }
            else if (key >= 'A' && key <= 'Z') { snprintf(kbuf, sizeof(kbuf), "%c", (char)(key - 'A' + 'a')); qc = kbuf; }
            else if (key >= '0' && key <= '9') { snprintf(kbuf, sizeof(kbuf), "%c", (char)key); qc = kbuf; }
            break;
    }
    if (qc) wubu_dos_proc_send_key(ctx->proc, qc);
}

static void dos_win_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    DosWindowCtx *ctx = (DosWindowCtx *)win->user_data;
    if (!ctx) return;
    /* Translate to guest coordinates and inject via QMP send-key for click. */
    if (kind == 2 && btn == 1) wubu_dos_proc_send_key(ctx->proc, "ret");
    (void)x; (void)y;
}

/*
 * Spawn a desktop window hosting the given DOS process.
 * Returns the WM window (the caller drives frames by calling dosgui_wm_render,
 * which invokes dos_win_draw). If `mount_styx` is set, the process is also
 * mounted into the Styx namespace at /tmp/wubu_styx.
 */
DosGuiWindow *dosgui_dos_window_spawn(WubuDosProc *proc, int mount_styx) {
    if (!proc) return NULL;
    DosWindowCtx *ctx = (DosWindowCtx *)calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->proc = proc;
    ctx->fb = (uint8_t *)malloc(WUBU_DOS_FB_MAX_BYTES);
    if (!ctx->fb) { free(ctx); return NULL; }
    if (mount_styx) {
        const char *p = wubu_dos_proc_styx_mount(proc, "/tmp/wubu_styx");
        snprintf(ctx->styx_path, sizeof(ctx->styx_path), "%s", p ? p : "");
    }

    DosGuiWindow *win = dosgui_wm_create(60, 60, 360, 280, "DOS");
    if (!win) { free(ctx->fb); free(ctx); return NULL; }
    win->user_data = ctx;
    win->on_draw = dos_win_draw;
    win->on_key  = dos_win_key;
    win->on_mouse = dos_win_mouse;
    return win;
}

/* Tear down the window's context (does NOT kill the DOS proc, and does NOT
 * free the window — the caller owns the window and frees it via
 * dosgui_wm_destroy). Freeing `win` here would double-free with the caller. */
void dosgui_dos_window_close(DosGuiWindow *win) {
    if (!win) return;
    DosWindowCtx *ctx = (DosWindowCtx *)win->user_data;
    if (ctx) { free(ctx->fb); free(ctx); win->user_data = NULL; }
}
