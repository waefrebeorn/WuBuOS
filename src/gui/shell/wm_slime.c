/*
 * wm_slime.c -- see wm_slime.h.
 *
 * The seam wraps the SCREEN (unlike wm_rim, which wraps the focused window).
 * Every tick the blobs spring toward their home on the edge, so a blob pressed
 * into the seam jiggles and settles rather than sitting there rigidly.
 */
#include "wm_slime.h"

#include "../wm_internal.h"
#include "taskbar_tasks.h"
#include "../../runtime/wubu_slime.h"
#include "../../runtime/wubu_spring.h"

#include <string.h>

/* The app registry lives in src/apps; a test or minimal build may not link
 * it.  Weak, exactly as the taskbar's pinned buttons do it. */
__attribute__((weak)) void launch_app(const char *name);

#define WM_SLIME_CAP       8
#define WM_SLIME_EXEC_LEN  160
#define WM_SLIME_CLICK_SLOP 4      /* px of movement still counted as a click */

/* Depth from the edge when released, and how deep a stored blob sinks. */
#define WM_SLIME_OUT       20.0f
#define WM_SLIME_IN         7.0f

typedef struct {
    uint32_t color;
    char     exec[WM_SLIME_EXEC_LEN];
} WmSlimeMeta;

static WubuSlime   g_slime;
static WmSlimeMeta g_meta[WM_SLIME_CAP];
static bool        g_ready;
static int         g_screen_w, g_screen_h;

/* The blob under an active press, or -1. */
static int g_drag;
static int g_down_x, g_down_y;

/* -- lifecycle ----------------------------------------------------- */

void wm_slime_init(int screen_w, int screen_h) {
    g_screen_w = screen_w;
    g_screen_h = screen_h;
    memset(g_meta, 0, sizeof g_meta);
    g_drag = -1;
    g_ready = wubu_slime_init(&g_slime, WM_SLIME_CAP,
                              (float)screen_w, (float)screen_h, 16.0f);
}

void wm_slime_shutdown(void) {
    if (!g_ready) return;
    wubu_slime_free(&g_slime);
    g_ready = false;
}

/* -- editing ------------------------------------------------------- */

int wm_slime_add(int edge, float home_t, float radius, uint32_t color,
                 const char *exec) {
    if (!g_ready) return -1;
    int i = wubu_slime_add(&g_slime, (WubuEdge)edge, home_t,
                           WM_SLIME_OUT, WM_SLIME_IN,
                           radius, radius * 0.45f);
    if (i < 0) return -1;
    g_meta[i].color = color;
    g_meta[i].exec[0] = '\0';
    if (exec) {
        strncpy(g_meta[i].exec, exec, sizeof g_meta[i].exec - 1);
        g_meta[i].exec[sizeof g_meta[i].exec - 1] = '\0';
    }
    return i;
}

void wm_slime_clear(void) {
    if (!g_ready) return;
    wubu_slime_free(&g_slime);
    memset(g_meta, 0, sizeof g_meta);
    g_drag = -1;
    wubu_slime_init(&g_slime, WM_SLIME_CAP, g_screen_w, g_screen_h, 16.0f);
}

int wm_slime_count(void) {
    return g_ready ? g_slime.field.n : 0;
}

/*
 * The real caller: mirror the taskbar's pinned tasks into the TOP edge of the
 * seam.  The bottom edge is behind the taskbar and the left/right edges are
 * where windows live, so the top edge is the one surface that is always
 * visible.  The blobs start RELEASED (out), so they read as a strip of beads
 * along the top of the screen.
 */
void wm_slime_mirror_pins(void) {
    if (!g_ready) return;
    int n = wubu_taskbar_count();
    int shown = 0;
    for (int i = 0; i < n; i++) {
        const WubuTask *t = wubu_taskbar_get(i);
        if (!t || !t->pinned) continue;
        float slot = (float)(shown + 1) / (float)(n + 1);
        wm_slime_add(0 /* top */, slot, 10.0f, 0x00C05050,
                     t->exec[0] ? t->exec : t->title);
        shown++;
    }
}

/* -- per-frame ----------------------------------------------------- */

void wm_slime_tick(void) {
    if (!g_ready || g_slime.field.n == 0) return;
    WubuSpringWorld w = wubu_spring_ooze();
    wubu_slime_step(&g_slime, &w);
}

void wm_slime_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    if (!g_ready || g_slime.field.n == 0) return;

    for (int i = 0; i < g_slime.field.n; i++) {
        const WubuSlimeBlob *b = &g_slime.blobs[i];
        int x = (int)b->blob.x;
        int y = (int)b->blob.y;
        int r = (int)b->blob.radius;
        if (r < 3) r = 3;

        uint32_t face = g_meta[i].color ? g_meta[i].color
                                        : tc()->win_title_active;
        vbe_fill_circle(x, y, r, face);
        vbe_ellipse(x, y, r, r, tc()->border_darkest);
        vbe_fill_circle(x - r / 3, y - r / 3, (r - 1) / 2, tc()->border_light);
    }
}

/* -- pointer ------------------------------------------------------- */

bool wm_slime_handle_pointer(int x, int y, int btn, int kind) {
    (void)btn;
    if (!g_ready || g_slime.field.n == 0) return false;

    if (kind == 1) {
        int i = wubu_slime_hit(&g_slime, (float)x, (float)y);
        if (i < 0) return false;
        g_drag = i;
        g_down_x = x;
        g_down_y = y;
        wubu_slime_drag(&g_slime, i, (float)x, (float)y);
        return true;
    }

    if (kind == 0 && g_drag >= 0) {
        wubu_slime_drag(&g_slime, g_drag, (float)x, (float)y);
        return true;
    }

    if (kind == 2 && g_drag >= 0) {
        int i = g_drag;
        g_drag = -1;
        wubu_slime_drop(&g_slime, i);

        int dx = x - g_down_x;
        int dy = y - g_down_y;
        if (dx * dx + dy * dy > WM_SLIME_CLICK_SLOP * WM_SLIME_CLICK_SLOP)
            return true;                       /* a drag, not a click */

        if (g_slime.blobs[i].stored) {
            /* A stored blob is pulled back out -- and launches. */
            wubu_slime_release(&g_slime, i);
            if (g_meta[i].exec[0] && launch_app) launch_app(g_meta[i].exec);
        } else {
            wubu_slime_store(&g_slime, i);     /* absorb it into the seam */
        }
        return true;
    }

    return false;
}

bool wm_slime_island_pos(int i, int *x, int *y, int *r) {
    if (!g_ready || i < 0 || i >= g_slime.field.n) return false;
    const WubuBlob *b = &g_slime.blobs[i].blob;
    if (x) *x = (int)b->x;
    if (y) *y = (int)b->y;
    if (r) *r = (int)b->radius;
    return true;
}
