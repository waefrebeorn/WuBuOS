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

void ex_render_toolbar(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
    (void)fb; (void)fb_w; (void)fb_h; (void)h;
    /* Remember the width this strip was drawn at: the search field is right-anchored, and the click
     * handler must use the same width the drawing did (it has no window geometry of its own). */
    ex->toolbar_w = w;

    vbe_fill_rect(x, y, w, ex_toolbar_h(), tc()->taskbar_bg);
    vbe_hline(x, x + w, y + ex_toolbar_h() - 1, tc()->taskbar_border);

    const char *btns[] = {"Back", "Forward", "Up", "Refresh", "New Folder"};
    int btn_w = 70;
    int btn_x = x + 10;

    for (int i = 0; i < 5; i++) {
        vbe_fill_rect(btn_x, y + 4, btn_w, ex_toolbar_h() - 8, tc()->btn_face);
        vbe_3d_raised_colors(btn_x, y + 4, btn_w, ex_toolbar_h() - 8,
                            tc()->border_light, tc()->border_face, tc()->border_dark, tc()->border_darkest);
        vbe_draw_text(btn_x + (btn_w - vbe_text_width(btns[i], 1)) / 2, y + 4 + (ex_toolbar_h() - 8 - 8) / 2, btns[i], tc()->btn_text, 1);
        btn_x += btn_w + 6;
    }

    /* View mode dropdown indicator */
    const char *view_names[] = {"Details", "Icons", "List", "Tiles"};
    char view_label[64];
    snprintf(view_label, sizeof(view_label), "View: %s", view_names[ex->view_mode]);
    vbe_draw_text(x + w - 180, y + (ex_toolbar_h() - 8) / 2, view_label, tc()->btn_text, 1);

    /* The field itself is drawn by the module that owns it (explorer_search.c). */
    ex_render_search(ex, x, y, w, ex_toolbar_h());
}
