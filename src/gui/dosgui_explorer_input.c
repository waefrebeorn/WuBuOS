/* dosgui_explorer_input.c -- Explorer input handling (keyboard + mouse)
 *
 * Extracted from dosgui_explorer.c (the monolith's two largest functions:
 * handle_key ~220 lines, handle_mouse ~195 lines).  Input is a distinct
 * concern from state/navigation/render; this module owns it.  It shares the
 * global explorer state + helpers via dosgui_explorer_internal.h.
 */

#include "dosgui_explorer_internal.h"
#include "dosgui_explorer.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_mime.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* -- Input Handling ----------------------------------------------- */



void dosgui_explorer_handle_key(uint32_t key, uint32_t mods) {
    /* Track modifier state for mouse handler */
    g_shift_pressed = (mods & 0x01) != 0;
    g_ctrl_pressed  = (mods & 0x02) != 0;

    if (g_explorer.context_menu_x >= 0) {
        ex_hide_context_menu(&g_explorer);
        return;
    }

    /* If find is active, handle find input */
    if (g_find_state.active) {
        if (key == 0x01) { /* Escape — close find */
            g_find_state.active = false;
            g_find_state.query_len = 0;
            g_find_state.query[0] = '\0';
            g_find_state.last_match = -1;
            snprintf(g_explorer.status_text, sizeof(g_explorer.status_text), "Find cancelled");
        } else if (key == 0x1C || key == 0xE01C) { /* Enter — find next */
            /* Search from last_match + 1 */
            int start = (g_find_state.last_match >= 0) ? g_find_state.last_match + 1 : 0;
            int found = -1;
            for (int i = start; i < g_explorer.entry_count; i++) {
                if (str_contains_nocase(g_explorer.entries[i].name, g_find_state.query)) {
                    found = i;
                    break;
                }
            }
            /* Wrap around if not found */
            if (found < 0 && start > 0) {
                for (int i = 0; i < start; i++) {
                    if (str_contains_nocase(g_explorer.entries[i].name, g_find_state.query)) {
                        found = i;
                        break;
                    }
                }
            }
            if (found >= 0) {
                g_find_state.last_match = found;
                g_explorer.focus_idx = found;
                g_explorer.anchor_idx = found;
                dosgui_explorer_clear_selection();
                dosgui_explorer_toggle_selection(found);
                /* Scroll to make visible */
                int row_h = ex_row_h();
                int list_h = g_explorer.preview_visible ?
                    (vbe_state()->height - ex_breadcrumb_h() - (g_explorer.toolbar_visible ? ex_toolbar_h() : 0) - ex_statusbar_h()) :
                    (vbe_state()->height - ex_breadcrumb_h() - (g_explorer.toolbar_visible ? ex_toolbar_h() : 0) - ex_statusbar_h());
                int visible_rows = list_h / row_h;
                if (g_explorer.focus_idx < g_explorer.list_scroll_y)
                    g_explorer.list_scroll_y = g_explorer.focus_idx;
                else if (g_explorer.focus_idx >= g_explorer.list_scroll_y + visible_rows)
                    g_explorer.list_scroll_y = g_explorer.focus_idx - visible_rows + 1;
                snprintf(g_explorer.status_text, sizeof(g_explorer.status_text),
                         "Found: %s", g_explorer.entries[found].name);
            } else {
                snprintf(g_explorer.status_text, sizeof(g_explorer.status_text),
                         "No match for: %s", g_find_state.query);
            }
        } else if (key == 0x0E) { /* Backspace */
            if (g_find_state.query_len > 0) {
                g_find_state.query[--g_find_state.query_len] = '\0';
                g_find_state.last_match = -1;
                snprintf(g_explorer.status_text, sizeof(g_explorer.status_text),
                         "Find: %s", g_find_state.query);
            }
        } else if (g_find_state.query_len < EX_FIND_MAX_LEN - 1) {
            /* Accept printable ASCII */
            char ch = 0;
            if (key >= 'a' && key <= 'z') ch = (char)key;
            else if (key >= 'A' && key <= 'Z') ch = (char)key;
            else if (key >= '0' && key <= '9') ch = (char)key;
            else if (key == 0x39) ch = ' ';       /* Space */
            else if (key == 0x0C) ch = '-';        /* Minus */
            else if (key == 0x0D) ch = '=';        /* Equals */
            else if (key == 0x1A) ch = '[';        /* Left bracket */
            else if (key == 0x1B) ch = ']';        /* Right bracket */
            else if (key == 0x27) ch = ';';        /* Semicolon */
            else if (key == 0x28) ch = '\'';       /* Apostrophe */
            else if (key == 0x29) ch = '`';        /* Grave */
            else if (key == 0x2B) ch = '\\';       /* Backslash */
            else if (key == 0x33) ch = ',';        /* Comma */
            else if (key == 0x34) ch = '.';        /* Period */
            else if (key == 0x35) ch = '/';        /* Slash */
            if (ch) {
                g_find_state.query[g_find_state.query_len++] = ch;
                g_find_state.query[g_find_state.query_len] = '\0';
                g_find_state.last_match = -1;
                snprintf(g_explorer.status_text, sizeof(g_explorer.status_text),
                         "Find: %s", g_find_state.query);
            }
        }
        return;
    }

    bool ctrl = mods & 0x04;  /* MOD_CTRL */
    bool shift = mods & 0x01; /* MOD_SHIFT */

    switch (key) {
        case 0xE048: /* Up arrow */
            if (g_explorer.focus_idx > 0) {
                g_explorer.focus_idx--;
                if (shift) dosgui_explorer_select_range(g_explorer.anchor_idx, g_explorer.focus_idx);
                else { dosgui_explorer_clear_selection(); dosgui_explorer_toggle_selection(g_explorer.focus_idx); g_explorer.anchor_idx = g_explorer.focus_idx; }
            }
            break;

        case 0xE050: /* Down arrow */
            if (g_explorer.focus_idx < g_explorer.entry_count - 1) {
                g_explorer.focus_idx++;
                if (shift) dosgui_explorer_select_range(g_explorer.anchor_idx, g_explorer.focus_idx);
                else { dosgui_explorer_clear_selection(); dosgui_explorer_toggle_selection(g_explorer.focus_idx); g_explorer.anchor_idx = g_explorer.focus_idx; }
            }
            break;

        case 0xE04B: /* Left arrow */
            if (g_explorer.view_mode == EX_VIEW_ICONS && g_explorer.focus_idx > 0) {
                g_explorer.focus_idx--;
                if (shift) dosgui_explorer_select_range(g_explorer.anchor_idx, g_explorer.focus_idx);
            }
            break;

        case 0xE04D: /* Right arrow */
            if (g_explorer.view_mode == EX_VIEW_ICONS && g_explorer.focus_idx < g_explorer.entry_count - 1) {
                g_explorer.focus_idx++;
                if (shift) dosgui_explorer_select_range(g_explorer.anchor_idx, g_explorer.focus_idx);
            }
            break;

        case '\r': /* Enter */
        case 0xE01C: /* Keypad Enter */
            if (g_explorer.focus_idx >= 0 && g_explorer.focus_idx < g_explorer.entry_count) {
                ExEntry *entry = &g_explorer.entries[g_explorer.focus_idx];
                if (entry->type == EX_ENTRY_DIR || entry->type == EX_ENTRY_DRIVE) {
                    dosgui_explorer_navigate(entry->full_path);
                } else {
                    /* Launch file with default app via MIME system */
                    if (wubu_mime_launch(entry->full_path, NULL) == 0) {
                        snprintf(g_explorer.status_text, sizeof(g_explorer.status_text),
                                 "Launched: %s", entry->name);
                    } else {
                        snprintf(g_explorer.status_text, sizeof(g_explorer.status_text),
                                 "Failed to launch: %s", entry->name);
                    }
                }
            }
            break;

        case 0xE04800: /* Backspace - go up */
        case 8:
            dosgui_explorer_go_up();
            break;

        case 'c':
        case 'C':
            if (ctrl) dosgui_explorer_copy();
            break;

        case 'x':
        case 'X':
            if (ctrl) dosgui_explorer_cut();
            break;

        case 'v':
        case 'V':
            if (ctrl) dosgui_explorer_paste();
            break;

        case 0xE053: /* Delete key */
            dosgui_explorer_delete(false);
            break;

        case 'f':
        case 'F':
            if (ctrl) {
                /* Activate find mode */
                g_find_state.active = true;
                g_find_state.query_len = 0;
                g_find_state.query[0] = '\0';
                g_find_state.last_match = -1;
                snprintf(g_explorer.status_text, sizeof(g_explorer.status_text),
                         "Find: type search query, Enter=find next, Esc=cancel");
            }
            break;

        case 0xE03B: /* F2 - rename */
            if (g_explorer.selection_count == 1) {
                dosgui_explorer_rename(g_explorer.selected_indices[0]);
            }
            break;

        case 0xE03C: /* F3 - find next */
            break;

        case 0xE03D: /* F4 - address bar focus */
            break;

        case 0xE03E: /* F5 - refresh */
            dosgui_explorer_refresh();
            break;

        case 'a':
        case 'A':
            if (ctrl) dosgui_explorer_select_all();
            break;

        case 'n':
        case 'N':
            if (ctrl && shift) dosgui_explorer_new_folder();
            else if (ctrl) dosgui_explorer_new_file("New Text Document.txt");
            break;

        case 9: /* Tab */
            /* Switch between tree, breadcrumbs, list, preview */
            break;

        default:
            break;
    }
}

void dosgui_explorer_handle_mouse(int x, int y, int btn, int kind) {
    /* kind: 0=move, 1=down, 2=up */
    ExExplorerState *ex = &g_explorer;
    
    /* Use shift state tracked from key handler */
    bool shift = g_shift_pressed;

    if (ex->context_menu_x >= 0) {
        if (kind == 1 && btn == 1) { /* Left click outside closes context menu */
            ex_hide_context_menu(ex);
        }
        return;
    }

    if (y < ex_breadcrumb_h()) {
        /* Click in breadcrumbs area - handle segment clicks */
        if (kind == 1 && btn == 1) {
            int seg_x = 10;
            for (int i = 0; i < ex->breadcrumb_count; i++) {
                int seg_w = vbe_text_width(ex->breadcrumb_segments[i], 1) + 12;
                if (x >= seg_x && x < seg_x + seg_w) {
                    /* Navigate to this breadcrumb segment */
                    char path[EX_MAX_PATH] = "/";
                    for (int j = 0; j <= i; j++) {
                        if (j > 0) {
                            strcat(path, "/");
                        }
                        strcat(path, ex->breadcrumb_segments[j]);
                    }
                    dosgui_explorer_navigate(path);
                    break;
                }
                seg_x += seg_w + 4;
            }
        }
        return;
    }

    int tree_x = 0;
    int tree_y = ex_breadcrumb_h() + (ex->toolbar_visible ? ex_toolbar_h() : 0);
    int tree_w = ex->tree_w;
    int list_x = tree_x + tree_w;
    int list_y = tree_y;
    int list_w = ex->preview_visible ? (vbe_state()->width - tree_w - ex->preview_w) : (vbe_state()->width - tree_w);
    int list_h = vbe_state()->height - tree_y - ex_statusbar_h();
    int preview_x = list_x + list_w;

    if (kind == 0) { /* Mouse move */
        if (ex->rubber_band.active) {
            ex->rubber_band.end_x = x;
            ex->rubber_band.end_y = y;
        }
        return;
    }

    if (kind == 1) { /* Mouse down */
        if (btn == 1) { /* Left click */
            /* Check tree view first */
            if (x >= tree_x && x < tree_x + tree_w && y >= tree_y && y < tree_y + list_h) {
                /* Tree view click - find node at position */
                ExTreeNode *node = ex->tree_root;
                int rel_y = y - tree_y - 4;
                while (node && rel_y >= 0) {
                    int node_h = ex_row_h();
                    if (node->is_drive || node->parent == ex->tree_root) {
                        if (rel_y < node_h) {
                            if (node->expanded) {
                                dosgui_explorer_tree_collapse(node);
                            } else {
                                dosgui_explorer_tree_expand(node);
                            }
                            break;
                        }
                        rel_y -= node_h;
                    }
                    /* Recurse into children */
                    if (node->expanded && node->first_child) {
                        ExTreeNode *child = node->first_child;
                        while (child) {
                            if (rel_y < ex_row_h()) {
                                if (child->expanded) dosgui_explorer_tree_collapse(child);
                                else dosgui_explorer_tree_expand(child);
                                break;
                            }
                            rel_y -= ex_row_h();
                            if (child->expanded && child->first_child) {
                                child = child->first_child;
                                continue;
                            }
                            child = child->next_sibling;
                            if (!child) {
                                ExTreeNode *p = child->parent;
                                while (p && !p->next_sibling) p = p->parent;
                                if (p) child = p->next_sibling;
                                else break;
                            }
                        }
                    }
                    break;
                }
                return;
            }

            /* Check breadcrumbs */
            if (y >= ex_breadcrumb_h() && y < ex_breadcrumb_h() + ex_toolbar_h()) {
                /* Toolbar buttons */
                return;
            }

            /* Check file list */
            if (x >= list_x && x < list_x + list_w && y >= list_y && y < list_y + list_h) {
                int rel_y = y - list_y - 4;
                int row_h = ex_row_h();

                if (ex->view_mode == EX_VIEW_ICONS) {
                    /* Grid layout for icons */
                    int cols = list_w / (EX_ICON_SIZE + 16);
                    if (cols < 1) cols = 1;
                    int row = rel_y / (EX_ICON_SIZE + 20);
                    int col = (x - list_x) / (EX_ICON_SIZE + 16);
                    int idx = row * cols + col;
                    if (idx >= 0 && idx < ex->entry_count) {
                        ex->focus_idx = idx;
                        ex->anchor_idx = idx;
                        if (!shift) {
                            dosgui_explorer_clear_selection();
                            dosgui_explorer_toggle_selection(idx);
                        }
                    }
                } else {
                    /* Details/List/Tiles - single column */
                    int idx = rel_y / row_h;
                    if (idx >= 0 && idx < ex->entry_count) {
                        ex->focus_idx = idx;
                        ex->anchor_idx = idx;
                        if (!shift) {
                            dosgui_explorer_clear_selection();
                            dosgui_explorer_toggle_selection(idx);
                        }
                    }
                }
                return;
            }

            /* Check preview pane */
            if (ex->preview_visible && x >= preview_x && x < vbe_state()->width) {
                return;
            }

            /* Start rubber band selection on empty space */
            if (x >= list_x && x < list_x + list_w && y >= list_y && y < list_y + list_h) {
                ex->rubber_band.active = true;
                ex->rubber_band.start_x = x;
                ex->rubber_band.start_y = y;
                ex->rubber_band.end_x = x;
                ex->rubber_band.end_y = y;
            }
        } else if (btn == 3) { /* Right click - context menu */
            if (x >= list_x && x < list_x + list_w && y >= list_y && y < list_y + list_h) {
                int rel_y = y - list_y - 4;
                int row_h = ex_row_h();
                int idx = rel_y / row_h;
                if (idx >= 0 && idx < ex->entry_count) {
                    if (!ex->entries[idx].is_selected) {
                        dosgui_explorer_clear_selection();
                        dosgui_explorer_toggle_selection(idx);
                    }
                    ex_show_context_menu(ex, x, y, idx);
                } else {
                    ex_show_context_menu(ex, x, y, -1);
                }
            }
        }
    } else if (kind == 2) { /* Mouse up */
        if (ex->rubber_band.active) {
            ex->rubber_band.active = false;
            /* Process rubber band selection */
            int min_x = ex->rubber_band.start_x < ex->rubber_band.end_x ? ex->rubber_band.start_x : ex->rubber_band.end_x;
            int max_x = ex->rubber_band.start_x > ex->rubber_band.end_x ? ex->rubber_band.start_x : ex->rubber_band.end_x;
            int min_y = ex->rubber_band.start_y < ex->rubber_band.end_y ? ex->rubber_band.start_y : ex->rubber_band.end_y;
            int max_y = ex->rubber_band.start_y > ex->rubber_band.end_y ? ex->rubber_band.start_y : ex->rubber_band.end_y;

            if (ex->view_mode == EX_VIEW_ICONS) {
                /* Grid selection */
            } else {
                int row_h = ex_row_h();
                int start_idx = (min_y - list_y - 4) / row_h;
                int end_idx = (max_y - list_y - 4) / row_h;
                if (start_idx < 0) start_idx = 0;
                if (end_idx >= ex->entry_count) end_idx = ex->entry_count - 1;
                dosgui_explorer_select_range(start_idx, end_idx);
            }
        }
    }
}
