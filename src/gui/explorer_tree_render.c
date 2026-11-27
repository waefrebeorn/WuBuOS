/*
 * explorer_tree_render.c -- the folder TREE pane: its nodes, its scroll offset and its thumb.
 *
 * Split out of explorer_render.c (which had crossed the 600-line budget) along a real seam: the list pane and
 * the tree pane are different concerns that only share the theme and the explorer state.
 */
#include "explorer_internal.h"
#include "gfx/wubu_scroll.h"
#include "wubu_tokens.h"
#include "wm.h"
#include "../kernel/vbe.h"

static void ex_draw_tree_node(ExTreeNode *node, int x, int y, int w, int *drawn_y, int depth, ExExplorerState *ex) {
    if (!node) return;

    int mh = ex_row_h();
    int indent = ex_tree_indent();
    int node_x = x + 4 + depth * indent;
    int node_w = w - 4 - depth * indent;

    if (node == ex->tree_selected) {
        vbe_fill_rect(node_x, *drawn_y, node_w, mh, tc()->select_bg);
    }

    /* Expand/collapse indicator for non-leaf nodes */
    if (node->first_child || node->is_drive) {
        if (node->expanded) {
            vbe_draw_text(node_x, *drawn_y + (mh - 8) / 2, "\x1b[B", tc()->icon_text, 1); /* Down arrow */
        } else {
            vbe_draw_text(node_x, *drawn_y + (mh - 8) / 2, "\x1b[C", tc()->icon_text, 1); /* Right arrow */
        }
    }

    /* Icon */
    vbe_fill_rect(node_x + 16, *drawn_y + (mh - 16) / 2, 16, 16, node->icon_color);
    vbe_rect(node_x + 16, *drawn_y + (mh - 16) / 2, 16, 16, tc()->icon_border);

    /* Name */
    vbe_draw_text(node_x + 36, *drawn_y + (mh - 8) / 2, node->display_name, tc()->icon_text, 1);

    *drawn_y += mh;

    if (node->expanded && node->first_child) {
        ExTreeNode *child = node->first_child;
        while (child) {
            ex_draw_tree_node(child, x, y, w, drawn_y, depth + 1, ex);
            child = child->next_sibling;
        }
    }
}

/* -- Render sub-sections ------------------------------------------- */

void ex_render_tree(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
    (void)fb; (void)fb_w; (void)fb_h;

    if (!ex->tree_root) return;

    int task_h = taskbar_height();
    (void)task_h;
    int list_h = fb_h - y - ex_statusbar_h();

    /* Background */
    vbe_fill_rect(x, y, w, list_h, tc()->win_face);
    vbe_rect(x + w - 1, y, 1, list_h, tc()->border_dark);

    /* Tree header */
    vbe_fill_rect(x, y, w, ex_title_h(), tc()->win_title_inactive);
    vbe_draw_text(x + 8, y + (ex_title_h() - 8) / 2, "Folders", tc()->body_text, 1);

    /* The tree pane scrolls with the SAME offset model as the list: `tree_scroll_y` was declared and never
     * read, so a deep folder tree simply ran off the bottom of the pane. */
    ex->tree_view_h = list_h - ex_title_h();
    int drawn_y = y + ex_title_h() + 2 - ex->tree_scroll_y;

    if (ex->tree_root->first_child) {
        ExTreeNode *child = ex->tree_root->first_child;
        while (child) {
            ex_draw_tree_node(child, x, y, w, &drawn_y, 0, ex);
            child = child->next_sibling;
        }
    }

    /* what the tree ACTUALLY drew: the wheel's range comes from this, not from a node count guess */
    ex->tree_content_h = drawn_y + ex->tree_scroll_y - (y + ex_title_h() + 2);
    {
        WubuScroll s = { .content = ex->tree_content_h,
                         .view = ex->tree_view_h, .offset = ex->tree_scroll_y };
        if (wubu_scroll_max(&s) > 0) {
            int tl = 0, tp = 0;
            wubu_scroll_thumb(&s, ex->tree_view_h, &tl, &tp);
            vbe_fill_rect(x + w - WUBU_SCROLL_THUMB_W - WUBU_SPACE_XS, y + ex_title_h() + WUBU_SPACE_XS + tp,
                          WUBU_SCROLL_THUMB_W, tl, tc()->scroll_thumb);
        }
    }
}
