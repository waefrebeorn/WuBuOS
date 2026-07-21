/*
 * calc_test_support.c -- headless link support for the calculator unit test.
 *
 * The calculator's draw path (calc_draw / calc_draw_button) legitimately uses
 * the theme engine (tc()) and WM geometry helpers (title_bar_height /
 * border_width). The headless engine test never exercises the draw path, but
 * those symbols must still resolve at link time. Rather than pull the entire
 * window-manager stack into a unit test (entanglement), we provide minimal
 * standalone definitions of exactly the symbols the draw path needs.
 *
 * These mirror the trivial test stub pattern (calc_test_stub.c) that already
 * satisfies dosgui_wm_create() for this test. C11, minimal includes.
 */

#include "calc.h"
#include "../gui/dosgui_wm.h"
#include "../gui/wubu_theme.h"
#include <stdlib.h>

/* -- Minimal theme shim (returns a static default Win98-ish palette) ------ */

static uint32_t g_btn_face   = 0x00C0C0C0u;
static uint32_t g_btn_text   = 0x00000000u;
static uint32_t g_btn_press  = 0x00A0A0A0u;
static uint32_t g_border_lt  = 0x00FFFFFFu;
static uint32_t g_border_dk  = 0x00808080u;
static uint32_t g_border_dk2 = 0x00000000u;

const WubuThemeColors *tc(void) {
    /* A static struct; only the fields calc_draw() reads matter for the
     * headless link. Everything else stays zero-initialized. */
    static WubuThemeColors s;
    s.btn_face      = g_btn_face;
    s.btn_text      = g_btn_text;
    s.btn_pressed   = g_btn_press;
    s.border_light  = g_border_lt;
    s.border_dark   = g_border_dk;
    s.border_darkest= g_border_dk2;
    return &s;
}

/* -- WM geometry helpers (Win98 Classic values) ------------------------- */

int title_bar_height(void) { return 18; }
int border_width(void)      { return 2; }

/* -- Window-manager create stub (headless: no real WM needed) ----------- */

DosGuiWindow *dosgui_wm_create(int x, int y, int w, int h,
                               const char *title) {
    (void)x; (void)y; (void)w; (void)h; (void)title;
    return calloc(1, sizeof(DosGuiWindow));
}
