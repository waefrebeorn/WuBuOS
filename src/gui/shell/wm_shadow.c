/*
 * wm_shadow.c -- cached elevation shadow (one concern: the shadow cache).
 *
 * Design ref: rizonesoft/impossible-os@be8c286 docs/design/tokens.json
 * elevation.window_active = [offset_y 16, blur 32, alpha 94] and
 * src/kernel/gfx/gfx_effects.c gfx_drop_shadow (double box blur of a rounded
 * mask) (GPL-3.0-only). Ground-up WuBuOS C11 implementation; no upstream
 * code copied. Divergence: the mask is cached per window geometry and only
 * re-blurred when the geometry changes (upstream rebuilds per call); the
 * composite goes through gui_blit_to_vbe_alpha so the VBE clip holds.
 */
#include "wm_shadow.h"
#include "../gfx/gfx.h"
#include "../wubu_theme.h"
#include "wubu_gui_metrics.h"

#include <stddef.h>

/* elevation.window_active, verbatim facts from the reference tokens. */
#define SHADOW_OFFSET_Y 16
#define SHADOW_BLUR     32
#define SHADOW_ALPHA    94
/* Two box passes spread the mask 2*blur; the offset shifts it down, so the
 * pad absorbs both and the falloff is never clipped by the cache surface. */
#define SHADOW_PAD      (2 * SHADOW_BLUR + SHADOW_OFFSET_Y)
/* Degrade rather than allocate without bound on enormous windows. */
#define SHADOW_MAX_PIXELS (16u * 1024u * 1024u)

static GuiSurface *g_cache;
static int g_kx, g_ky, g_kw, g_kh;   /* geometry the cache was built for */
static unsigned g_blurs;

unsigned wm_shadow_blur_count(void) { return g_blurs; }

static void shadow_rebuild(int w, int h)
{
    gui_surface_destroy(g_cache);
    g_cache = NULL;
    if ((unsigned)(w + 2 * SHADOW_PAD) * (unsigned)(h + 2 * SHADOW_PAD)
        > SHADOW_MAX_PIXELS)
        return;
    g_cache = gui_surface_create(w + 2 * SHADOW_PAD, h + 2 * SHADOW_PAD);
    if (!g_cache) return;
    /* Black mask; the blur spreads its ALPHA, the rgb stays shadow-black. */
    gui_fill_rounded(g_cache, SHADOW_PAD, SHADOW_PAD, w, h,
                     WUBU_METRIC_RADIUS_WINDOW, 0xFF000000u);
    gui_blur_box(g_cache, 0, 0, gui_surface_width(g_cache),
                 gui_surface_height(g_cache), SHADOW_BLUR);
    gui_blur_box(g_cache, 0, 0, gui_surface_width(g_cache),
                 gui_surface_height(g_cache), SHADOW_BLUR);
    g_blurs++;
}

void wm_shadow_render(const DosGuiWindow *win)
{
    if (!win || !win->alive) return;
    if (wubu_theme_current() != THEME_DARK) return;
    if (wubu_theme_high_contrast()) return;
    if (win->w <= 0 || win->h <= 0) return;

    if (!g_cache || win->x != g_kx || win->y != g_ky ||
        win->w != g_kw || win->h != g_kh) {
        shadow_rebuild(win->w, win->h);
        g_kx = win->x; g_ky = win->y; g_kw = win->w; g_kh = win->h;
    }
    if (!g_cache) return;
    gui_blit_to_vbe_alpha(g_cache, win->x - SHADOW_PAD,
                          win->y - SHADOW_PAD + SHADOW_OFFSET_Y,
                          SHADOW_ALPHA);
}