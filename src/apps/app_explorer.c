/*
 * app_explorer.c  --  WuBuOS File Manager: in-shell Explorer binding
 *
 * Binds the real, Win98/XP-class dosgui_explorer engine
 * (dosgui_explorer.c + ex_render_*) to a DosGuiWindow. The engine already
 * implements tree sidebar, breadcrumbs, toolbar, details/icons/list/tiles views,
 * preview pane, status bar, context menus, copy/cut/paste/delete/rename,
 * multi-select, column sort, zip browsing and drive enumeration. This file
 * only wires it to the window: on_draw fans out to the ex_render_* calls,
 * on_mouse / on_key forward to the engine's handlers.
 *
 * C11, minimal includes, self-contained. Content is drawn via the global
 * vbe_* backbuffer at absolute coordinates (same pattern as notepad/canvas).
 */

#include "dosgui_apps.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_explorer.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdlib.h>
#include <string.h>

/* -- Init ---------------------------------------------------------- */

static int g_explorer_ready = 0;

void app_explorer_init(void) {
    if (g_explorer_ready) return;
    dosgui_explorer_init();
    g_explorer_ready = 1;
}

/* -- Draw (on_draw callback) -------------------------------------- */

void app_explorer_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    if (!g_explorer_ready) app_explorer_init();
    ExExplorerState *ex = dosgui_explorer_state();
    if (!ex) return;

    int wx = win->x, wy = win->y, ww = win->w, wh = win->h;

    int ty = wy + EX_TOOLBAR_H + EX_BREADCRUMB_H;
    int th = wh - EX_TOOLBAR_H - EX_BREADCRUMB_H - EX_STATUSBAR_H;

    int bx = wx + 2 + ex->tree_w + 2;          /* file-list left */
    int bw = ww - (bx - wx) - (ex->preview_visible ? ex->preview_w : 0) - 4;
    int px = bx + bw + 2;                        /* preview left */
    int pw = ex->preview_visible ? ex->preview_w : 0;

    /* Toolbar + breadcrumbs span the full width. */
    ex_render_toolbar(ex, fb, fb_w, fb_h, wx, wy, ww, EX_TOOLBAR_H);
    ex_render_breadcrumbs(ex, fb, fb_w, fb_h, wx, wy + EX_TOOLBAR_H, ww, EX_BREADCRUMB_H);

    /* Tree sidebar. */
    ex_render_tree(ex, fb, fb_w, fb_h, wx + 2, ty, ex->tree_w, th);

    /* File list (center). */
    ex_render_file_list(ex, fb, fb_w, fb_h, bx, ty, bw, th);

    /* Preview pane (right). */
    if (ex->preview_visible)
        ex_render_preview(ex, fb, fb_w, fb_h, px, ty, pw, th);

    /* Status bar (bottom). */
    ex_render_statusbar(ex, fb, fb_w, fb_h, wx, wy + wh - EX_STATUSBAR_H, ww, EX_STATUSBAR_H);

    /* Context menu (topmost). */
    if (ex->context_menu_x >= 0)
        ex_render_context_menu(ex, fb, fb_w, fb_h);
}

/* -- Input forwards ------------------------------------------------ */

void app_explorer_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    (void)win;
    dosgui_explorer_handle_mouse(x, y, btn, kind);
}

void app_explorer_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)win;
    dosgui_explorer_handle_key(key, mods);
}
