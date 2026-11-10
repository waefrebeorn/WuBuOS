/*
 * wm_rim.c -- see wm_rim.h.
 *
 * The rim wraps the FOCUSED window: every tick the rim rect is re-bound to
 * that window's frame, so a window that moves or resizes carries its islands
 * with it and their t (0..1 around the loop) stays valid.  With no focused
 * window the rim falls back to the screen rect, which is what lets the
 * taskbar's mirrored pins stay visible on the desktop.
 */
#include "wm_rim.h"

#include "../wm_internal.h"
#include "taskbar_tasks.h"
#include "../../runtime/wubu_rim.h"
#include "../../runtime/wubu_spring.h"

#include <string.h>

/* The app registry lives in src/apps; a test or minimal build may not link
 * it.  Weak, exactly as the taskbar's pinned buttons do it. */
__attribute__((weak)) void launch_app(const char *name);

#define WM_RIM_CAP       8
#define WM_RIM_EXEC_LEN  160
#define WM_RIM_CLICK_SLOP 4      /* px of movement still counted as a click */

typedef struct {
    uint32_t color;
    char     exec[WM_RIM_EXEC_LEN];
} WmRimMeta;

static WubuRim   g_rim;
static WmRimMeta g_meta[WM_RIM_CAP];
static bool      g_ready;
static int       g_screen_w, g_screen_h;

/* The island under the pointer, or -1. */
static int g_drag;
static int g_down_x, g_down_y;

/* -- geometry ------------------------------------------------------ */

void wm_rim_init(int screen_w, int screen_h) {
    g_screen_w = screen_w;
    g_screen_h = screen_h;
    memset(g_meta, 0, sizeof g_meta);
    g_drag = -1;
    g_ready = wubu_rim_init(&g_rim, WM_RIM_CAP, 0.0f, 0.0f,
                            (float)screen_w, (float)screen_h);
}

void wm_rim_shutdown(void) {
    if (!g_ready) return;
    wubu_rim_free(&g_rim);
    g_ready = false;
}

/* Re-bind the rim to the focused window (or the screen when there is none). */
static void bind_focused(void) {
    DosGuiWindow *w = wm_get_focused();
    if (w && w->alive && !(w->flags & DOSGUI_WIN_MINIMIZED))
        wubu_rim_set_rect(&g_rim, (float)w->x, (float)w->y,
                          (float)w->w, (float)w->h);
    else
        wubu_rim_set_rect(&g_rim, 0.0f, 0.0f,
                          (float)g_screen_w, (float)g_screen_h);
}

void wm_rim_rect(int *x, int *y, int *w, int *h) {
    if (x) *x = (int)g_rim.x;
    if (y) *y = (int)g_rim.y;
    if (w) *w = (int)g_rim.w;
    if (h) *h = (int)g_rim.h;
}

/* -- editing ------------------------------------------------------- */

int wm_rim_add(float t, float radius, uint32_t color, const char *exec) {
    if (!g_ready) return -1;
    int i = wubu_rim_add(&g_rim, t, radius);
    if (i < 0) return -1;
    g_meta[i].color = color;
    g_meta[i].exec[0] = '\0';
    if (exec) {
        strncpy(g_meta[i].exec, exec, sizeof g_meta[i].exec - 1);
        g_meta[i].exec[sizeof g_meta[i].exec - 1] = '\0';
    }
    bind_focused();
    return i;
}

void wm_rim_clear(void) {
    if (!g_ready) return;
    wubu_rim_free(&g_rim);
    memset(g_meta, 0, sizeof g_meta);
    g_drag = -1;
    wubu_rim_init(&g_rim, WM_RIM_CAP, 0.0f, 0.0f,
                  (float)g_screen_w, (float)g_screen_h);
}

int wm_rim_count(void) {
    return g_ready ? g_rim.field.n : 0;
}

/*
 * The real caller: mirror the taskbar's pinned tasks onto the rim.  A pinned
 * app is then reachable from the window you are working in, not only from
 * the taskbar -- the "island" half of the feature.  Spread evenly around the
 * loop so they do not pile up on one corner.
 */
void wm_rim_mirror_pins(void) {
    if (!g_ready) return;
    int n = wubu_taskbar_count();
    int shown = 0;
    for (int i = 0; i < n; i++) {
        const WubuTask *t = wubu_taskbar_get(i);
        if (!t || !t->pinned) continue;
        float slot = (float)(shown + 1) / (float)(n + 1);
        wm_rim_add(slot, 9.0f, 0x0080FF, t->exec[0] ? t->exec : t->title);
        shown++;
    }
}

/* -- per-frame ----------------------------------------------------- */

void wm_rim_tick(void) {
    if (!g_ready || g_rim.field.n == 0) return;
    bind_focused();
    WubuSpringWorld w = wubu_spring_ooze();
    wubu_rim_step(&g_rim, &w);
}

void wm_rim_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    if (!g_ready || g_rim.field.n == 0) return;

    /* Islands live on a window frame; never paint them over the taskbar. */
    int task_h = taskbar_height_dynamic();
    vbe_set_clip(0, 0, fb_w, fb_h - task_h);

    for (int i = 0; i < g_rim.field.n; i++) {
        const WubuRimIsland *is = &g_rim.islands[i];
        if (!is->visible) continue;
        int x = (int)is->blob.x;
        int y = (int)is->blob.y;
        int r = (int)is->blob.radius;
        if (r < 3) r = 3;

        uint32_t face = g_meta[i].color ? g_meta[i].color
                                        : tc()->win_title_active;
        vbe_fill_circle(x, y, r, face);
        vbe_ellipse(x, y, r, r, tc()->border_darkest);
        /* A small highlight so the blob reads as a raised bead, not a dot. */
        vbe_fill_circle(x - r / 3, y - r / 3, (r - 1) / 2, tc()->border_light);
    }

    vbe_reset_clip();
}

/* -- pointer ------------------------------------------------------- */

bool wm_rim_handle_pointer(int x, int y, int btn, int kind) {
    (void)btn;
    if (!g_ready || g_rim.field.n == 0) return false;

    if (kind == 1) {
        int i = wubu_rim_hit(&g_rim, (float)x, (float)y);
        if (i < 0) return false;
        g_drag = i;
        g_down_x = x;
        g_down_y = y;
        wubu_rim_drag(&g_rim, i, (float)x, (float)y);
        return true;
    }

    if (kind == 0 && g_drag >= 0) {
        wubu_rim_drag(&g_rim, g_drag, (float)x, (float)y);
        return true;
    }

    if (kind == 2 && g_drag >= 0) {
        int i = g_drag;
        g_drag = -1;
        wubu_rim_drop(&g_rim, i);
        /* A press-release that did not travel is a click: launch. */
        int dx = x - g_down_x;
        int dy = y - g_down_y;
        if (dx * dx + dy * dy <= WM_RIM_CLICK_SLOP * WM_RIM_CLICK_SLOP &&
            g_meta[i].exec[0] && launch_app) {
            launch_app(g_meta[i].exec);
        }
        return true;
    }

    return false;
}

bool wm_rim_island_pos(int i, int *x, int *y, int *r) {
    if (!g_ready || i < 0 || i >= g_rim.field.n) return false;
    const WubuBlob *b = &g_rim.islands[i].blob;
    if (x) *x = (int)b->x;
    if (y) *y = (int)b->y;
    if (r) *r = (int)b->radius;
    return true;
}
