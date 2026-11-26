/*
 * explorer_toolbar.c -- the strip above the list: Back/Forward/Up/Refresh/New Folder, the view label
 * and the search field. Split out of explorer_render.c when that file crossed the 600-line budget:
 * the toolbar is its own concern, and the search field it hosts lives in explorer_search.c.
 *
 * C11.
 */
#include "wubu_gui_metrics.h"
#include "wubu_tokens.h"
#include "explorer_internal.h"
#include "wm.h"
#include "../kernel/vbe.h"
#include <stdio.h>
#include <string.h>

/* The toolbar's buttons, as geometry: the renderer and the click handler both ask THIS, because a
 * painted button nobody can press is the defect this strip shipped with ("no toolbar buttons are wired
 * here yet" was a comment, not a promise). */
#define EX_TB_BTN_W 70
#define EX_TB_PITCH 76
#define EX_TB_LEFT  10

int ex_toolbar_button_count(const ExExplorerState *ex)
{
    return (ex && ex->library >= 0) ? 6 : 5;      /* a library adds "Add folders" */
}

const char *ex_toolbar_button_label(const ExExplorerState *ex, int i)
{
    static const char *btns[] = {"Back", "Forward", "Up", "Refresh", "New Folder", "Add folders"};
    if (i < 0 || i >= ex_toolbar_button_count(ex)) return NULL;
    return btns[i];
}

bool ex_toolbar_button_rect(const ExExplorerState *ex, int i, int x, int y,
                            int *bx, int *by, int *bw, int *bh)
{
    if (i < 0 || i >= ex_toolbar_button_count(ex)) return false;
    if (bx) *bx = x + EX_TB_LEFT + i * EX_TB_PITCH;
    if (by) *by = y + 4;
    if (bw) *bw = EX_TB_BTN_W;
    if (bh) *bh = ex_toolbar_h() - 8;
    return true;
}

void ex_render_toolbar(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
    (void)fb; (void)fb_w; (void)fb_h; (void)h;
    /* Remember the width this strip was drawn at: the search field is right-anchored, and the click
     * handler must use the same width the drawing did (it has no window geometry of its own). */
    ex->toolbar_w = w;

    vbe_fill_rect(x, y, w, ex_toolbar_h(), tc()->taskbar_bg);
    vbe_hline(x, x + w, y + ex_toolbar_h() - 1, tc()->taskbar_border);

    int n_btns = ex_toolbar_button_count(ex);
    for (int i = 0; i < n_btns; i++) {
        int bx, by, bw, bh;
        if (!ex_toolbar_button_rect(ex, i, x, y, &bx, &by, &bw, &bh)) continue;
        const char *label = ex_toolbar_button_label(ex, i);
        vbe_fill_rect(bx, by, bw, bh, tc()->btn_face);
        vbe_3d_raised_colors(bx, by, bw, bh,
                            tc()->border_light, tc()->border_face, tc()->border_dark, tc()->border_darkest);
        vbe_draw_text(bx + (bw - vbe_text_width(label, 1)) / 2, by + (bh - 8) / 2, label, tc()->btn_text, 1);
    }

    /* View mode dropdown indicator */
    const char *view_names[] = {"Details", "Icons", "List", "Tiles"};
    char view_label[64];
    snprintf(view_label, sizeof(view_label), "View: %s", view_names[ex->view_mode]);
    vbe_draw_text(x + w - 180, y + (ex_toolbar_h() - 8) / 2, view_label, tc()->btn_text, 1);

    /* The field itself is drawn by the module that owns it (explorer_search.c). */
    ex_render_search(ex, x, y, w, ex_toolbar_h());
}
