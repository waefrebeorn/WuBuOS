/* explorer_input.c -- Explorer input handling (keyboard + mouse)
 *
 * Extracted from explorer.c (the monolith's two largest functions:
 * handle_key ~220 lines, handle_mouse ~195 lines).  Input is a distinct
 * concern from state/navigation/render; this module owns it.  It shares the
 * global explorer state + helpers via explorer_internal.h.
 */

#include "explorer_internal.h"
#include "gfx/wubu_scroll.h"    /* the same scroll arithmetic the Control Panel uses */
#include "explorer.h"
#include "../kernel/vbe.h"
#include "../kernel/input.h"   /* MOD_* -- the real modifier bits */
#include "../gui/wubu_mime.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* -- Input Handling ----------------------------------------------- */



/* One place that keeps the selection on screen. Windows scrolls a list when the keyboard moves the focus, and
 * NOT otherwise -- which is exactly why the Control Panel's clamp had to learn the same distinction. */
static void ex_ensure_focus_visible(void)
{
    if (g_explorer.list_view_h <= 0) return;
    int row = ex_row_h();
    if (row <= 0) return;
    /* the focus index is into the FILTERED view; translate, because that is what the list draws */
    int r = -1;
    for (int k = 0; k < ex_view_count(&g_explorer); k++)
        if (ex_view_entry(&g_explorer, k) == g_explorer.focus_idx) { r = k; break; }
    if (r < 0) return;
    WubuScroll s;
    s.view = g_explorer.list_view_h;
    s.content = ex_view_count(&g_explorer) * row;
    s.offset = g_explorer.list_scroll_y;
    int top = r * row, bot = top + row;
    if (top < s.offset) s.offset = top;
    if (bot > s.offset + s.view) s.offset = bot - s.view;
    g_explorer.list_scroll_y = wubu_scroll_clamp(&s);
}

static void explorer_handle_key_impl(uint32_t key, uint32_t mods) {
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

    /* The search field owns keystrokes while it is focused. It is a TEXT FIELD: typing "n" must filter
     * the list, not create a folder, and Backspace must edit the query, not navigate up. */
    if (g_explorer.search_active) {
        if (key == 0x01 || key == 27) {                 /* Esc: clear and give the list back */
            ex_search_clear(&g_explorer);
            return;
        }
        if (key == 0x1C || key == 0xE01C || key == '\r' || key == '\n') {
            g_explorer.search_active = false;           /* Enter keeps the filter, returns to the list */
            return;
        }
        if (key == 8) {
            size_t n = strlen(g_explorer.search);
            if (n) {
                g_explorer.search[n - 1] = '\0';
                ex_search_set(&g_explorer, g_explorer.search);
            }
            return;
        }
        if (key >= 32 && key < 127) {
            size_t n = strlen(g_explorer.search);
            if (n + 1 < sizeof g_explorer.search) {
                g_explorer.search[n] = (char)key;
                g_explorer.search[n + 1] = '\0';
                ex_search_set(&g_explorer, g_explorer.search);
            }
            return;
        }
        /* Arrows still move through the filtered list; anything else is swallowed so a focused field
         * never triggers a shortcut by accident. */
        if (!(key == 0xE048 || key == 0xE050 || key == 0xE04B || key == 0xE04D)) return;
    }

    if (g_explorer.context_menu_x >= 0) {
        ex_hide_context_menu(&g_explorer);
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
                /* Ctrl+F focuses the toolbar search field -- the Windows behaviour: the list filters
                 * as you type. The old find-next modal is gone from this key because two search
                 * mechanisms is two answers to one question. */
                g_explorer.search_active = true;
                snprintf(g_explorer.status_text, sizeof(g_explorer.status_text),
                         "Search: type to filter, Enter=apply, Esc=clear");
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

void explorer_handle_key(uint32_t key, uint32_t mods) {
    explorer_handle_key_impl(key, mods);
    ex_ensure_focus_visible();     /* Windows scrolls the list to the keyboard's focus, not on paint */
}

void explorer_handle_mouse(int x, int y, int btn, int kind) {
    /* WHEEL over the TREE pane scrolls the tree; over the list it scrolls the list. */
    if ((btn == 4 || btn == 5) && x < g_explorer.tree_w && g_explorer.tree_view_h > 0) {
        WubuScroll ts;
        ts.view = g_explorer.tree_view_h;
        ts.content = g_explorer.tree_content_h;   /* measured by the renderer, not assumed */
        ts.offset = g_explorer.tree_scroll_y;
        g_explorer.tree_scroll_y = wubu_scroll_wheel(&ts, btn == 4 ? -1 : +1, ex_row_h());
        return;
    }
    /* WHEEL: three rows per notch, the same rule as every other list in the OS. */
    if ((btn == 4 || btn == 5) && g_explorer.list_view_h > 0) {
        int row = ex_row_h();
        WubuScroll s;
        s.view = g_explorer.list_view_h;
        s.content = ex_view_count(&g_explorer) * row;
        s.offset = g_explorer.list_scroll_y;
        g_explorer.list_scroll_y = wubu_scroll_wheel(&s, btn == 4 ? -1 : +1, row);
        return;
    }
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
                case 1: {
                    /* "Open With..." must ASK. It used to launch W-Code unconditionally, which is a
                     * different promise: the menu said choose-an-app and the code chose one. The
                     * resolver owns the choice; W-Code stays the fallback when nothing can be offered. */
                    extern bool open_with_open(const char *path) __attribute__((weak));
                    if (ex->context_menu_entry >= 0 && ex->context_menu_entry < ex->entry_count) {
                        const char *p = ex->entries[ex->context_menu_entry].full_path;
                        if (open_with_open && open_with_open(p)) break;
                        if (ex_launch_with_wcode) ex_launch_with_wcode(p);
                    }
                    break;
                }
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
            /* The library shortcuts own the top of the nav pane; a click there opens the library. */
            if (x >= tree_x && x < tree_x + tree_w && y >= tree_y && y < tree_y + list_h) {
                int lib = ex_hit_library_shortcut(ex, tree_y, x, y);
                if (lib >= 0) { explorer_open_library(lib); return; }
            }

            /* Check tree view first (below the library block) */
            if (x >= tree_x && x < tree_x + tree_w &&
                y >= tree_y + ex_library_block_h() && y < tree_y + list_h) {
                /* Tree view click - find node at position */
                ExTreeNode *node = ex->tree_root;
                int rel_y = y - tree_y - ex_library_block_h() - 4;
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

            /* Toolbar strip: this range is the TOOLBAR (below the address bar), not the breadcrumbs.
             * The buttons were painted and never wired; they now hit-test through
             * ex_toolbar_button_rect(), the same function the renderer paints them with. */
            if (y >= 0 && y < ex_toolbar_h()) {
                /* The search field first: its box comes from ex_search_rect(). */
                if (kind == 1 && btn == 1) {
                    int sx, sy, sw, sh;
                    if (ex_search_rect(ex, 0, 0, ex->toolbar_w, ex_toolbar_h(), &sx, &sy, &sw, &sh) &&
                        x >= sx && x < sx + sw && y >= sy && y < sy + sh) {
                        ex->search_active = true;
                        return;
                    }
                }
                if (kind == 1 && btn == 1) {
                    int n_btns = ex_toolbar_button_count(ex);
                    for (int i = 0; i < n_btns; i++) {
                        int bx, by, bw, bh;
                        if (!ex_toolbar_button_rect(ex, i, 0, 0, &bx, &by, &bw, &bh)) continue;
                        if (x < bx || x >= bx + bw || y < by || y >= by + bh) continue;
                        switch (i) {
                        case 0: explorer_go_back(); return;
                        case 1: explorer_go_forward(); return;
                        case 2: explorer_go_up(); return;
                        case 3: explorer_refresh(); return;
                        case 4: explorer_new_folder(); return;
                        case 5: {
                            /* "Add folders" belongs to the library card, not to Explorer: weak so a
                             * WM-less test graph still links. */
                            extern __attribute__((weak)) bool add_to_library_open(int, int, int);
                            if (add_to_library_open)
                                (void)add_to_library_open(ex->library, vbe_state()->width,
                                                          vbe_state()->height);
                            return;
                        }
                        default: return;
                        }
                    }
                }
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
                    /* row*cols+col is a VISIBLE position; the entry index comes from the same
                     * filtered view the renderer drew. */
                    int idx = ex_view_entry(ex, row * cols + col);
                    if (idx >= 0) {
                        ex->focus_idx = idx;
                        ex->anchor_idx = idx;
                        explorer_update_preview(idx);   /* the pane follows the click */
                        if (!shift) {
                            explorer_clear_selection();
                            explorer_toggle_selection(idx);
                        }
                    }
                } else {
                    /* Details/List/Tiles - single column: visible row -> entry through the view. */
                    int idx = ex_view_entry(ex, rel_y / row_h);
                    if (idx >= 0) {
                        ex->focus_idx = idx;
                        ex->anchor_idx = idx;
                        explorer_update_preview(idx);   /* the pane follows the click */
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
