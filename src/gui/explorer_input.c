/* explorer_input.c -- Explorer input handling (keyboard + mouse)
 *
 * Extracted from explorer.c (the monolith's two largest functions:
 * handle_key ~220 lines, handle_mouse ~195 lines).  Input is a distinct
 * concern from state/navigation/render; this module owns it.  It shares the
 * global explorer state + helpers via explorer_internal.h.
 */

#include "explorer_internal.h"
#include "explorer.h"
#include "../kernel/vbe.h"
#include "../kernel/input.h"   /* MOD_* -- the real modifier bits */
#include "../gui/wubu_mime.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* -- Input Handling ----------------------------------------------- */



void explorer_handle_key(uint32_t key, uint32_t mods) {
    /* Track modifier state for mouse handler */
    g_shift_pressed = (mods & 0x01) != 0;
    g_ctrl_pressed  = (mods & 0x02) != 0;

    /* PICK MODE first: Enter accepts, Esc cancels, typing fills the name field. Arrows and
     * navigation fall through to the normal handler so the picker navigates like Explorer. */
    if (explorer_pick_active()) {
        if (key == 0x1C || key == 0xE01C || key == '\r' || key == '\n') {
            explorer_pick_accept();
            return;
        }
        if (key == 0x01 || key == 27) {
            explorer_pick_cancel();
            return;
        }
        if (key == 8) { explorer_pick_typed_backspace(); return; }
        if (key >= 32 && key < 127) { explorer_pick_typed_char((char)key); return; }
    }

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
                explorer_clear_selection();
                explorer_toggle_selection(found);
                /* Scroll to make visible */
                int row_h = ex_row_h();
                /* Both branches were the same expression, copied. ex_content_top() owns
                 * the sum now, so the tab strip cannot be in one copy and missing from the
                 * other. */
                int list_h = vbe_state()->height - ex_content_top() - ex_statusbar_h();
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

    /* MOD_* from the platform (kernel/input.h): Ctrl is 0x02. This used to
     * be 0x04, which is MOD_ALT -- so every Ctrl+C/V/X/F in Explorer was
     * unbound and Alt did the work instead. */
    bool ctrl = (mods & MOD_CTRL) != 0;
    bool shift = (mods & MOD_SHIFT) != 0;

    switch (key) {
        case 0xE048: /* Up arrow */
            if (g_explorer.focus_idx > 0) {
                g_explorer.focus_idx--;
                if (shift) explorer_select_range(g_explorer.anchor_idx, g_explorer.focus_idx);
                else { explorer_clear_selection(); explorer_toggle_selection(g_explorer.focus_idx); g_explorer.anchor_idx = g_explorer.focus_idx; }
            }
            break;

        case 0xE050: /* Down arrow */
            if (g_explorer.focus_idx < g_explorer.entry_count - 1) {
                g_explorer.focus_idx++;
                if (shift) explorer_select_range(g_explorer.anchor_idx, g_explorer.focus_idx);
                else { explorer_clear_selection(); explorer_toggle_selection(g_explorer.focus_idx); g_explorer.anchor_idx = g_explorer.focus_idx; }
            }
            break;

        case 0xE04B: /* Left arrow */
            if (g_explorer.view_mode == EX_VIEW_ICONS && g_explorer.focus_idx > 0) {
                g_explorer.focus_idx--;
                if (shift) explorer_select_range(g_explorer.anchor_idx, g_explorer.focus_idx);
            }
            break;

        case 0xE04D: /* Right arrow */
            if (g_explorer.view_mode == EX_VIEW_ICONS && g_explorer.focus_idx < g_explorer.entry_count - 1) {
                g_explorer.focus_idx++;
                if (shift) explorer_select_range(g_explorer.anchor_idx, g_explorer.focus_idx);
            }
            break;

        case '\r': /* Enter */
        case 0xE01C: /* Keypad Enter */
            if (g_explorer.focus_idx >= 0 && g_explorer.focus_idx < g_explorer.entry_count) {
                ExEntry *entry = &g_explorer.entries[g_explorer.focus_idx];
                if (entry->type == EX_ENTRY_DIR || entry->type == EX_ENTRY_DRIVE) {
                    explorer_navigate(entry->full_path);
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
            explorer_go_up();
            break;

        case 'c':
        case 'C':
            if (ctrl) explorer_copy();
            break;

        case 'x':
        case 'X':
            if (ctrl) explorer_cut();
            break;

        case 'v':
        case 'V':
            if (ctrl) explorer_paste();
            break;

        /* Tabs (Win11): Ctrl+T opens one at the current folder, Ctrl+W closes the active
         * one, Ctrl+Tab / Ctrl+Shift+Tab walk them. */
        case 't':
        case 'T':
            if (ctrl) explorer_tab_open(g_explorer.current_path);
            break;

        case 'w':
        case 'W':
            if (ctrl) explorer_tab_close(explorer_tab_active());
            break;


        case 0xE053: /* Delete key */
            explorer_delete(false);
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
                explorer_rename(g_explorer.selected_indices[0]);
            }
            break;

        case 0xE03C: /* F3 - find next */
            break;

        case 0xE03D: /* F4 - address bar focus */
            break;

        case 0xE03E: /* F5 - refresh */
            explorer_refresh();
            break;

        case 'a':
        case 'A':
            if (ctrl) explorer_select_all();
            break;

        case 'n':
        case 'N':
            if (ctrl && shift) explorer_new_folder();
            else if (ctrl) explorer_new_file("New Text Document.txt");
            break;

        case 9: /* Tab */
            /* Ctrl+Tab / Ctrl+Shift+Tab walk the tabs (Win11). A bare Tab still means
             * "switch between tree, breadcrumbs, list, preview" -- not implemented yet, so
             * it deliberately does nothing rather than pretending. */
            if (ctrl && explorer_tab_count() > 1) {
                int n = explorer_tab_count();
                int step = g_shift_pressed ? -1 : 1;
                explorer_tab_switch((explorer_tab_active() + step + n) % n);
            }
            break;

        default:
            break;
    }
}

void explorer_handle_mouse(int x, int y, int btn, int kind) {
    /* kind: 0=move, 1=down, 2=up */
    ExExplorerState *ex = &g_explorer;
    
    /* Use shift state tracked from key handler */
    bool shift = g_shift_pressed;

    if (ex->context_menu_x >= 0) {
        if (kind == 1 && btn == 1) {
            /* Row first, then item -- the shared mapping decides which row is an item
             * at all, so this path carries no layout knowledge of its own. */
            int row = ex_ctx_row_at(x, y, ex->context_menu_x, ex->context_menu_y);
            int item = ex_ctx_item_at_row(row);
            ex_hide_context_menu(ex);

            switch (item) {
                case 0: explorer_open_entry(ex, ex->context_menu_entry); break;
                case 1: if (ex->context_menu_entry >= 0 &&
                            ex->context_menu_entry < ex->entry_count &&
                            ex_launch_with_wcode)
                            ex_launch_with_wcode(
                                ex->entries[ex->context_menu_entry].full_path);
                        break;
                case 2: explorer_cut();         break;
                case 3: explorer_copy();        break;
                case 4: explorer_paste();       break;
                case 5: explorer_delete(false); break;
                case 6: explorer_rename(ex->context_menu_entry); break;
                case 7: explorer_show_properties(ex->context_menu_entry); break;
                default: break;   /* outside the menu, or the separator */
            }
        }
        return;
    }

    /* Tab strip: click a tab to switch. Hit-testing goes through explorer_tab_rect(), the
     * same function that draws it. */
    int tab_y = ex_breadcrumb_h() + (ex->toolbar_visible ? ex_toolbar_h() : 0);
    if (y >= tab_y && y < tab_y + ex_tab_h() && ex->tab_count > 0) {
        if (kind == 1 && btn == 1) {
            for (int i = 0; i < ex->tab_count; i++) {
                int tx, tw;
                if (!explorer_tab_rect(i, vbe_state()->width, &tx, &tw)) continue;
                if (x >= tx && x < tx + tw) { explorer_tab_switch(i); break; }
            }
        }
        return;
    }

    {
        /* The address bar sits BELOW the toolbar, and its segment geometry comes from
         * ex_breadcrumb_segment_rect() -- the same function the renderer paints with. Testing
         * `y < breadcrumb height` here meant the top 28px of the window, where no chip is ever
         * drawn, so every click on "Computer" fell through to the file list. */
        int bar_y = ex->toolbar_visible ? ex_toolbar_h() : 0;
        if (y >= bar_y && y < bar_y + ex_breadcrumb_h()) {
            if (kind == 1 && btn == 1) {
                for (int i = 0; i < ex->breadcrumb_count; i++) {
                    int sx = 0, sy = 0, sw = 0;
                    if (!ex_breadcrumb_segment_rect(ex, i, 0, bar_y, &sx, &sy, &sw)) continue;
                    if (x >= sx && x < sx + sw) {
                        /* Segment 0 is the ROOT chip ("Computer") and stands for "/" itself.
                         * Appending it built "/Computer", a path that does not exist, so clicking
                         * the address bar navigated into nothing. Components start at 1. */
                        char path[EX_MAX_PATH] = "/";
                        for (int j = 1; j <= i; j++) {
                            strcat(path, ex->breadcrumb_segments[j]);
                            if (j < i) strcat(path, "/");
                        }
                        explorer_navigate(path);
                        break;
                    }
                }
            }
            return;
        }
    }

    int tree_x = 0;
    int tree_y = ex_content_top();
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
                                explorer_tree_collapse(node);
                            } else {
                                explorer_tree_expand(node);
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
                                if (child->expanded) explorer_tree_collapse(child);
                                else explorer_tree_expand(child);
                                break;
                            }
                            rel_y -= ex_row_h();
                            if (child->expanded && child->first_child) {
                                child = child->first_child;
                                continue;
                            }
                            /* Walk UP from the node we just finished, not from the NULL sibling we
                             * just found: `child->parent` inside `if (!child)` dereferenced NULL and
                             * segfaulted the whole desktop on a click past the last visible row. */
                            {
                                ExTreeNode *from = child;
                                child = child->next_sibling;
                                while (!child && from) {
                                    ExTreeNode *p = from->parent;
                                    if (!p) break;
                                    from = p;
                                    child = p->next_sibling;
                                }
                                if (!child) break;
                            }
                        }
                    }
                    break;
                }
                return;
            }

            /* Toolbar strip: this range is the TOOLBAR (below the address bar), not the
             * breadcrumbs -- the label was wrong, which is how the real breadcrumb hit-test ended up
             * testing the wrong rows. No toolbar buttons are wired here yet. */
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
                            explorer_clear_selection();
                            explorer_toggle_selection(idx);
                        }
                    }
                } else {
                    /* Details/List/Tiles - single column */
                    int idx = rel_y / row_h;
                    if (idx >= 0 && idx < ex->entry_count) {
                        ex->focus_idx = idx;
                        ex->anchor_idx = idx;
                        if (!shift) {
                            explorer_clear_selection();
                            explorer_toggle_selection(idx);
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
                        explorer_clear_selection();
                        explorer_toggle_selection(idx);
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
                explorer_select_range(start_idx, end_idx);
            }
        }
    }
}
