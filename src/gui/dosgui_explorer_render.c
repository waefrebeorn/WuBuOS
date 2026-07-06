/*
 * dosgui_explorer_render.c  --  Win98 Explorer render layer
 *
 * Extracted from dosgui_explorer.c — all drawing/rendering functions:
 *   - Tree view sidebar render (ex_draw_tree_node, ex_render_tree)
 *   - Breadcrumbs, toolbar, file list, preview, statusbar, context menu
 *   - Main render entry point (dosgui_explorer_render)
 *
 * Uses theme helpers + VBE framebuffer API.
 */

#include "dosgui_explorer_internal.h"
#include "dosgui_wm.h"
#include "../kernel/vbe.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* -- Tree node draw (recursive, local to this module) -------------- */

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

    int task_h = dosgui_taskbar_height();
    (void)task_h;
    int list_h = fb_h - y - ex_statusbar_h();

    /* Background */
    vbe_fill_rect(x, y, w, list_h, tc()->win_face);
    vbe_rect(x + w - 1, y, 1, list_h, tc()->border_dark);

    /* Tree header */
    vbe_fill_rect(x, y, w, ex_title_h(), tc()->win_title_inactive);
    vbe_draw_text(x + 8, y + (ex_title_h() - 8) / 2, "Folders", tc()->win_title_text, 1);

    int drawn_y = y + ex_title_h() + 2;

    if (ex->tree_root->first_child) {
        ExTreeNode *child = ex->tree_root->first_child;
        while (child) {
            ex_draw_tree_node(child, x, y, w, &drawn_y, 0, ex);
            child = child->next_sibling;
        }
    }
}

void ex_render_breadcrumbs(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
    (void)fb; (void)fb_w; (void)fb_h; (void)h;

    vbe_fill_rect(x, y, w, ex_breadcrumb_h(), tc()->win_face);
    vbe_hline(x, x + w, y + ex_breadcrumb_h() - 1, tc()->border_dark);

    int seg_x = x + 10;
    for (int i = 0; i < ex->breadcrumb_count; i++) {
        int seg_w = vbe_text_width(ex->breadcrumb_segments[i], 1) + 12;
        bool is_last = (i == ex->breadcrumb_count - 1);

        vbe_fill_rect(seg_x, y + 2, seg_w, ex_breadcrumb_h() - 4, is_last ? tc()->select_bg : tc()->btn_face);
        vbe_rect_rounded(seg_x, y + 2, seg_w, ex_breadcrumb_h() - 4, 3, is_last ? tc()->border_dark : tc()->border_light);
        vbe_draw_text(seg_x + 6, y + (ex_breadcrumb_h() - 8) / 2, ex->breadcrumb_segments[i], tc()->win_title_text, 1);

        seg_x += seg_w + 4;

        /* Separator */
        if (i < ex->breadcrumb_count - 1) {
            vbe_draw_text(seg_x, y + (ex_breadcrumb_h() - 8) / 2, ">", tc()->icon_text, 1);
            seg_x += 16;
        }
    }
}

void ex_render_toolbar(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
    (void)fb; (void)fb_w; (void)fb_h; (void)h;

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
}

void ex_render_file_list(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
    (void)fb; (void)fb_w; (void)fb_h;

    vbe_fill_rect(x, y, w, h, tc()->win_face);

    if (ex->view_mode == EX_VIEW_DETAILS) {
        /* Column headers */
        int col_x = x;
        for (int i = 0; i < ex->column_count; i++) {
            if (!ex->columns[i].visible) continue;
            vbe_fill_rect(col_x, y, ex->columns[i].width, ex_title_h(), tc()->win_title_inactive);
            vbe_3d_raised_colors(col_x, y, ex->columns[i].width, ex_title_h(),
                                tc()->border_light, tc()->border_face, tc()->border_dark, tc()->border_darkest);
            vbe_draw_text(col_x + 6, y + (ex_title_h() - 8) / 2, ex->columns[i].name, tc()->win_title_text, 1);
            col_x += ex->columns[i].width;
            vbe_vline(col_x - 1, y, y + h, tc()->border_dark);
        }

        /* Entries */
        int row_y = y + ex_title_h();
        int row_h = ex_row_h();
        int max_rows = (h - ex_title_h()) / row_h;

        for (int i = 0; i < ex->entry_count && i < max_rows; i++) {
            ExEntry *entry = &ex->entries[i];
            if (entry->hidden && !ex->show_hidden) continue;

            int col_x2 = x;
            for (int c = 0; c < ex->column_count; c++) {
                if (!ex->columns[c].visible) continue;
                int col_w = ex->columns[c].width;

                if (entry->is_selected || i == ex->focus_idx) {
                    vbe_fill_rect(col_x2, row_y, col_w, row_h, tc()->select_bg);
                }

                if (c == 0) { /* Name with icon */
                    vbe_fill_rect(col_x2 + 4, row_y + (row_h - 16) / 2, 16, 16, entry->icon_color);
                    vbe_rect(col_x2 + 4, row_y + (row_h - 16) / 2, 16, 16, tc()->icon_border);
                    vbe_draw_text(col_x2 + 24, row_y + (row_h - 8) / 2, entry->name, entry->is_selected ? tc()->select_text : tc()->icon_text, 1);
                } else if (c == 1) { /* Size */
                    char size_str[32];
                    dosgui_explorer_format_size(entry->size, size_str, sizeof(size_str));
                    vbe_draw_text(col_x2 + 6, row_y + (row_h - 8) / 2, size_str, entry->is_selected ? tc()->select_text : tc()->icon_text, 1);
                } else if (c == 2) { /* Type */
                    vbe_draw_text(col_x2 + 6, row_y + (row_h - 8) / 2, entry->type_str, entry->is_selected ? tc()->select_text : tc()->icon_text, 1);
                } else if (c == 3) { /* Date */
                    char date_str[64];
                    dosgui_explorer_format_time(entry->modified, date_str, sizeof(date_str));
                    vbe_draw_text(col_x2 + 6, row_y + (row_h - 8) / 2, date_str, entry->is_selected ? tc()->select_text : tc()->icon_text, 1);
                }

                col_x2 += col_w;
            }
            row_y += row_h;
        }
    } else if (ex->view_mode == EX_VIEW_ICONS) {
        /* Large icons grid */
        int icon_w = EX_ICON_SIZE;
        int icon_h = EX_ICON_SIZE;
        int cell_w = icon_w + 16;
        int cell_h = icon_h + 24;
        int cols = w / cell_w;
        if (cols < 1) cols = 1;
        int start_x = x + (w - cols * cell_w) / 2;
        int cur_x = start_x;
        int cur_y = y + 10;

        for (int i = 0; i < ex->entry_count; i++) {
            ExEntry *entry = &ex->entries[i];
            if (entry->hidden && !ex->show_hidden) continue;

            bool selected = entry->is_selected || i == ex->focus_idx;
            if (selected) {
                vbe_fill_rect(cur_x, cur_y, cell_w, cell_h, tc()->select_bg);
            }

            /* Icon */
            vbe_fill_rect(cur_x + 8, cur_y + 4, icon_w, icon_h, entry->icon_color);
            vbe_rect(cur_x + 8, cur_y + 4, icon_w, icon_h, tc()->icon_border);

            /* Name */
            vbe_draw_text(cur_x + (cell_w - vbe_text_width(entry->name, 1)) / 2,
                         cur_y + icon_h + 8, entry->name,
                         selected ? tc()->select_text : tc()->icon_text, 1);

            cur_x += cell_w;
            if (cur_x + cell_w > x + w - 10) {
                cur_x = start_x;
                cur_y += cell_h;
            }
        }
    } else if (ex->view_mode == EX_VIEW_LIST) {
        /* Small icons, single column */
        int row_h = 24;
        int row_y = y + 4;

        for (int i = 0; i < ex->entry_count; i++) {
            ExEntry *entry = &ex->entries[i];
            if (entry->hidden && !ex->show_hidden) continue;

            bool selected = entry->is_selected || i == ex->focus_idx;
            if (selected) {
                vbe_fill_rect(x + 2, row_y, w - 4, row_h, tc()->select_bg);
            }

            vbe_fill_rect(x + 4, row_y + (row_h - 16) / 2, 16, 16, entry->icon_color);
            vbe_rect(x + 4, row_y + (row_h - 16) / 2, 16, 16, tc()->icon_border);
            vbe_draw_text(x + 24, row_y + (row_h - 8) / 2, entry->name, selected ? tc()->select_text : tc()->icon_text, 1);

            row_y += row_h;
            if (row_y + row_h > y + h) break;
        }
    } else if (ex->view_mode == EX_VIEW_TILES) {
        /* Medium icons with metadata */
        int tile_w = 180;
        int tile_h = 90;
        int cols = w / tile_w;
        if (cols < 1) cols = 1;
        int cur_x = x + 10;
        int cur_y = y + 10;

        for (int i = 0; i < ex->entry_count; i++) {
            ExEntry *entry = &ex->entries[i];
            if (entry->hidden && !ex->show_hidden) continue;

            bool selected = entry->is_selected || i == ex->focus_idx;
            if (selected) {
                vbe_fill_rect(cur_x, cur_y, tile_w, tile_h, tc()->select_bg);
            }

            vbe_3d_raised_colors(cur_x, cur_y, tile_w, tile_h,
                                tc()->border_light, tc()->border_face, tc()->border_dark, tc()->border_darkest);

            vbe_fill_rect(cur_x + 8, cur_y + 8, 48, 48, entry->icon_color);
            vbe_rect(cur_x + 8, cur_y + 8, 48, 48, tc()->icon_border);

            vbe_draw_text(cur_x + 60, cur_y + 10, entry->name, selected ? tc()->select_text : tc()->icon_text, 1);

            char size_str[32];
            dosgui_explorer_format_size(entry->size, size_str, sizeof(size_str));
            vbe_draw_text(cur_x + 60, cur_y + 26, size_str, selected ? tc()->select_text : 0x808080, 1);

            char date_str[64];
            dosgui_explorer_format_time(entry->modified, date_str, sizeof(date_str));
            vbe_draw_text(cur_x + 60, cur_y + 38, date_str, selected ? tc()->select_text : 0x808080, 1);

            vbe_draw_text(cur_x + 60, cur_y + 50, entry->type_str, selected ? tc()->select_text : 0x808080, 1);

            cur_x += tile_w + 10;
            if (cur_x + tile_w > x + w - 10) {
                cur_x = x + 10;
                cur_y += tile_h + 10;
            }
        }
    }
}

void ex_render_preview(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
    (void)fb; (void)fb_w; (void)fb_h;

    if (!ex->preview_visible) return;

    vbe_fill_rect(x, y, w, h, tc()->win_face);
    vbe_vline(x, y, y + h, tc()->border_dark);

    /* Preview header */
    vbe_fill_rect(x, y, w, ex_title_h(), tc()->win_title_inactive);
    vbe_draw_text(x + 8, y + (ex_title_h() - 8) / 2, "Preview", tc()->win_title_text, 1);

    int content_y = y + ex_title_h() + 4;
    int content_h = h - ex_title_h() - 4;

    if (ex->preview.type == EX_PREVIEW_NONE) {
        vbe_draw_text(x + 10, content_y + 10, "Select a file to preview", 0x808080, 1);
    } else if (ex->preview.type == EX_PREVIEW_TEXT) {
        int line_h = 12;
        int lines = content_h / line_h;
        for (int i = 0; i < ex->preview.text_lines && i < lines; i++) {
            char *line = ex->preview.text_buffer;
            for (int j = 0; j < i; j++) {
                line = strchr(line, '\n');
                if (!line) break;
                line++;
                if (!*line) break;
            }
            if (line) {
                char *end = strchr(line, '\n');
                int len = end ? (end - line) : strlen(line);
                if (len > 80) len = 80;
                char buf[128];
                strncpy(buf, line, len);
                buf[len] = '\0';
                vbe_draw_text(x + 10, content_y + i * line_h, buf, tc()->win_title_text, 1);
            }
        }
    } else if (ex->preview.type == EX_PREVIEW_IMAGE) {
        /* Render image pixels with nearest-neighbor scaling to fit preview area */
        if (ex->preview.img_pixels && ex->preview.img_w > 0 && ex->preview.img_h > 0) {
            int avail_w = w - 20;
            int avail_h = content_h - 40;
            if (avail_w < 1) avail_w = 1;
            if (avail_h < 1) avail_h = 1;

            /* Compute scale to fit while preserving aspect ratio */
            int out_w, out_h;
            int scale_w = ex->preview.img_w / avail_w;
            int scale_h = ex->preview.img_h / avail_h;
            int scale_n = scale_w > scale_h ? scale_w : scale_h;
            if (scale_n < 1) scale_n = 1;
            out_w = ex->preview.img_w / scale_n;
            out_h = ex->preview.img_h / scale_n;
            if (out_w > avail_w) out_w = avail_w;
            if (out_h > avail_h) out_h = avail_h;
            if (out_w < 1) out_w = 1;
            if (out_h < 1) out_h = 1;

            /* Center in preview area */
            int ox = x + 10 + (avail_w - out_w) / 2;
            int oy = content_y + 10 + (avail_h - out_h) / 2;

            /* Nearest-neighbor blit */
            uint32_t *src = ex->preview.img_pixels;
            for (int py = 0; py < out_h; py++) {
                int src_y = (py * ex->preview.img_h) / out_h;
                if (src_y >= ex->preview.img_h) src_y = ex->preview.img_h - 1;
                for (int px = 0; px < out_w; px++) {
                    int src_x = (px * ex->preview.img_w) / out_w;
                    if (src_x >= ex->preview.img_w) src_x = ex->preview.img_w - 1;
                    uint32_t pixel = src[src_y * ex->preview.img_w + src_x];
                    uint32_t rgb = pixel & 0x00FFFFFF;
                    vbe_set_pixel(ox + px, oy + py, rgb);
                }
            }

            /* Draw border around image */
            vbe_rect(ox - 1, oy - 1, out_w + 2, out_h + 2, 0x00808080);

            char dims[64];
            snprintf(dims, sizeof(dims), "%dx%d (shown %dx%d)", ex->preview.img_w, ex->preview.img_h, out_w, out_h);
            vbe_draw_text(x + 10, content_y + content_h - 14, dims, 0x808080, 1);
        } else {
            vbe_draw_text(x + 10, content_y + 10, "[Image Preview - No Data]", 0x808080, 1);
            char dims[64];
            snprintf(dims, sizeof(dims), "%dx%d pixels", ex->preview.img_w, ex->preview.img_h);
            vbe_draw_text(x + 10, content_y + 30, dims, 0x808080, 1);
        }
    } else if (ex->preview.type == EX_PREVIEW_METADATA) {
        char size_str[32];
        dosgui_explorer_format_size(ex->preview.file_size, size_str, sizeof(size_str));

        vbe_draw_text(x + 10, content_y + 10, "File Size:", 0x808080, 1);
        vbe_draw_text(x + 100, content_y + 10, size_str, tc()->win_title_text, 1);

        char date_str[64];
        dosgui_explorer_format_time(ex->preview.modified, date_str, sizeof(date_str));
        vbe_draw_text(x + 10, content_y + 30, "Modified:", 0x808080, 1);
        vbe_draw_text(x + 100, content_y + 30, date_str, tc()->win_title_text, 1);

        vbe_draw_text(x + 10, content_y + 50, "Type:", 0x808080, 1);
        vbe_draw_text(x + 100, content_y + 50, ex->preview.mime_type, tc()->win_title_text, 1);
    } else if (ex->preview.type == EX_PREVIEW_BINARY) {
        vbe_draw_text(x + 10, content_y + 10, "[Binary File - No Preview]", 0x808080, 1);
    }
}

void ex_render_statusbar(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
    (void)fb; (void)fb_w; (void)fb_h; (void)x; (void)h;

    int sb_y = fb_h - ex_statusbar_h();

    vbe_fill_rect(0, sb_y, fb_w, ex_statusbar_h(), tc()->taskbar_bg);
    vbe_hline(0, fb_w, sb_y, tc()->taskbar_border);

    /* Left: status text */
    vbe_draw_text(10, sb_y + (ex_statusbar_h() - 8) / 2, ex->status_text[0] ? ex->status_text : "Ready", tc()->win_title_text, 1);

    /* Center: item count and total size */
    char count_str[128];
    if (ex->selection_count > 0) {
        snprintf(count_str, sizeof(count_str), "%d of %d items selected, %lu bytes",
                 ex->selection_count, ex->status_file_count, (unsigned long)ex->status_total_size);
    } else {
        snprintf(count_str, sizeof(count_str), "%d items, %lu bytes", ex->status_file_count, (unsigned long)ex->status_total_size);
    }
    int count_x = (fb_w - vbe_text_width(count_str, 1)) / 2;
    vbe_draw_text(count_x, sb_y + (ex_statusbar_h() - 8) / 2, count_str, tc()->win_title_text, 1);

    /* Right: view mode */
    const char *view_names[] = {"Details", "Icons", "List", "Tiles"};
    char view_str[64];
    snprintf(view_str, sizeof(view_str), "%s View", view_names[ex->view_mode]);
    vbe_draw_text(fb_w - 10 - vbe_text_width(view_str, 1), sb_y + (ex_statusbar_h() - 8) / 2, view_str, tc()->win_title_text, 1);
}

void ex_render_context_menu(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;

    if (ex->context_menu_x < 0) return;

    const char *items[] = {"Open", "Open With...", "Cut", "Copy", "Paste", "Delete", "Rename", "Properties"};
    const int n_items = 8;
    int item_h = 24;
    int menu_w = 180;
    int menu_h = n_items * item_h + 4;
    int menu_x = ex->context_menu_x;
    int menu_y = ex->context_menu_y;

    /* Ensure on screen */
    if (menu_x + menu_w > fb_w) menu_x = fb_w - menu_w;
    if (menu_y + menu_h > fb_h - ex_statusbar_h()) menu_y = fb_h - ex_statusbar_h() - menu_h;

    /* Shadow */
    vbe_shade_rect(menu_x + 2, menu_y + 2, menu_w, menu_h);

    /* Background */
    if (th()->rounded_buttons) {
        vbe_fill_rect_rounded(menu_x, menu_y, menu_w, menu_h, 4, tc()->startmenu_bg);
        vbe_rect_rounded(menu_x, menu_y, menu_w, menu_h, 4, tc()->border_dark);
    } else {
        vbe_fill_rect(menu_x, menu_y, menu_w, menu_h, tc()->startmenu_bg);
        vbe_3d_raised_colors(menu_x, menu_y, menu_w, menu_h,
                            tc()->border_light, tc()->border_face, tc()->border_dark, tc()->border_darkest);
    }

    for (int i = 0; i < n_items; i++) {
        int iy = menu_y + 2 + i * item_h;
        /* Separator after Paste */
        if (i == 5) {
            vbe_hline(menu_x + 8, menu_x + menu_w - 8, iy + item_h / 2, tc()->border_dark);
            continue;
        }
        vbe_draw_text(menu_x + 12, iy + (item_h - 8) / 2, items[i], tc()->startmenu_text, 1);
    }
}

/* -- Main render entry point --------------------------------------- */

void dosgui_explorer_render(uint32_t *fb, int fb_w, int fb_h) {
    ExExplorerState *ex = &g_explorer;
    if (!dosgui_explorer_is_open()) return;

    int tree_x = 0;
    int tree_y = ex_breadcrumb_h() + (ex->toolbar_visible ? ex_toolbar_h() : 0);
    int tree_w = ex->tree_w;
    int list_x = tree_x + tree_w;
    int list_y = tree_y;
    int list_w = ex->preview_visible ? (fb_w - tree_w - ex->preview_w) : (fb_w - tree_w);
    int list_h = fb_h - tree_y - ex_statusbar_h();
    int preview_x = list_x + list_w;

    ex_render_breadcrumbs(ex, fb, fb_w, fb_h, tree_x, 0, fb_w, ex_breadcrumb_h());

    if (ex->toolbar_visible) {
        ex_render_toolbar(ex, fb, fb_w, fb_h, tree_x, ex_breadcrumb_h(), fb_w, ex_toolbar_h());
    }

    ex_render_tree(ex, fb, fb_w, fb_h, tree_x, tree_y, tree_w, list_h);
    ex_render_file_list(ex, fb, fb_w, fb_h, list_x, list_y, list_w, list_h);

    if (ex->preview_visible) {
        ex_render_preview(ex, fb, fb_w, fb_h, preview_x, list_y, ex->preview_w, list_h);
    }

    ex_render_statusbar(ex, fb, fb_w, fb_h, 0, fb_h - ex_statusbar_h(), fb_w, ex_statusbar_h());
    ex_render_context_menu(ex, fb, fb_w, fb_h);
}
