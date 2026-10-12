/* dosgui_explorer_input.c -- Explorer input handling (keyboard + mouse)
 *
 * Extracted from dosgui_explorer.c (the monolith's two largest functions:
 * handle_key ~220 lines, handle_mouse ~195 lines).  Input is a distinct
 * concern from state/navigation/render; this module owns it.  It shares the
 * global explorer state + helpers via dosgui_explorer_internal.h.
 */

#include "dosgui_explorer_internal.h"
#include "dosgui_explorer.h"
#include "dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_mime.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* Monotonic milliseconds for double-click detection (XP ~500ms window). */
static uint64_t ex_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

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

    /* Window-aware origin: when hosted in a WM window (win_id set), the
     * explorer is drawn at the window's x/y; hit-testing must match. When
     * driven directly (tests / fullscreen), origin is 0,0. */
    int origin_x = 0, origin_y = 0;
    if (ex->win_id > 0) {
        DosGuiWindow *ew = dosgui_wm_find_by_id(ex->win_id);
        if (ew) { origin_x = ew->x; origin_y = ew->y; }
    }

    if (ex->context_menu_x >= 0) {
        if (kind == 1 && btn == 1) { /* Left click outside closes context menu */
            ex_hide_context_menu(ex);
        }
        return;
    }

    if (y < origin_y + ex_breadcrumb_h()) {
        /* Click in breadcrumbs area - handle segment clicks */
        if (kind == 1 && btn == 1) {
            int seg_x = origin_x + 10;
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

    /* Window-aware geometry: match app_explorer_draw's windowed layout.
     * Extent is the window's w/h when hosted, else the screen. */
    int screen_w = vbe_state()->width, screen_h = vbe_state()->height;
    if (ex->win_id > 0) {
        DosGuiWindow *ew = dosgui_wm_find_by_id(ex->win_id);
        if (ew) { screen_w = ew->w; screen_h = ew->h; }
    }
    int tree_x = origin_x + 2;
    int tree_y = origin_y + ex_breadcrumb_h() + (ex->toolbar_visible ? ex_toolbar_h() : 0);
    int tree_w = ex->tree_w;
    int list_x = tree_x + tree_w;
    int list_y = tree_y;
    int list_w = screen_w - (list_x - origin_x) - (ex->preview_visible ? ex->preview_w : 0) - 4;
    int list_h = screen_h - (tree_y - origin_y) - ex_statusbar_h();
    int preview_x = list_x + list_w;

    if (kind == 0) { /* Mouse move */
        if (ex->sb_dragging) {
            /* Map cursor Y -> list_scroll_y (inverse thumb geometry). */
            int content_y = list_y + ex_title_h();
            int content_h = list_h - ex_title_h();
            int row_h = ex_row_h();
            int vis = content_h / row_h;
            if (vis < 1) vis = 1;
            int btn = 14;
            int track_top = content_y + btn;
            int track_h = content_h - 2 * btn;
            int thumb_h = track_h * vis / ex->entry_count;
            if (thumb_h < 8) thumb_h = 8;
            if (thumb_h > track_h) thumb_h = track_h;
            int span = track_h - thumb_h;
            int total = ex->entry_count - vis;
            if (span > 0 && total > 0) {
                int pos = (y - ex->sb_drag_off - track_top);
                if (pos < 0) pos = 0;
                if (pos > span) pos = span;
                ex->list_scroll_y = total * pos / span;
            }
            return;
        }
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

            /* Check toolbar buttons (window-aware band) */
            if (ex->toolbar_visible &&
                y >= origin_y && y < origin_y + ex_toolbar_h()) {
                /* Buttons laid out as ex_render_toolbar draws them. */
                const char *btns[] = {"Back", "Forward", "Up", "Refresh", "New Folder"};
                int btn_x = origin_x + 10;
                int btn_w = 70, btn_stride = 76;
                int btn_y = origin_y + 4, btn_h = ex_toolbar_h() - 8;
                for (int i = 0; i < 5; i++) {
                    if (x >= btn_x && x < btn_x + btn_w &&
                        y >= btn_y && y < btn_y + btn_h) {
                        switch (i) {
                            case 0: dosgui_explorer_go_back(); break;
                            case 1: dosgui_explorer_go_forward(); break;
                            case 2: dosgui_explorer_go_up(); break;
                            case 3: dosgui_explorer_refresh(); break;
                            case 4: dosgui_explorer_new_folder(); break;
                        }
                        return;
                    }
                    btn_x += btn_stride;
                }
                return;
            }

            /* Check file list */
            if (x >= list_x && x < list_x + list_w && y >= list_y && y < list_y + list_h) {
                int rel_y = y - list_y - 4;
                int row_h = ex_row_h();

                /* Scrollbar column (rightmost 14px, only when it overflows). */
                int content_y = list_y + ex_title_h();
                int content_h = list_h - ex_title_h();
                int vis = content_h / row_h;
                if (ex->entry_count > vis) {
                    int sb_x = list_x + list_w - 14;
                    if (x >= sb_x && y >= content_y && y < content_y + content_h) {
                        int btn = 14;                      /* arrow button size */
                        int track_top = content_y + btn;
                        int track_h = content_h - 2 * btn;
                        int thumb_h = track_h * vis / ex->entry_count;
                        if (thumb_h < 8) thumb_h = 8;
                        if (thumb_h > track_h) thumb_h = track_h;
                        int span = (track_h - thumb_h);
                        int thumb_y = track_top;
                        if (ex->entry_count > vis && span > 0)
                            thumb_y = track_top + span * ex->list_scroll_y / (ex->entry_count - vis);
                        if (y < track_top) {
                            /* Up arrow: one row up. */
                            dosgui_explorer_scroll(-1);
                        } else if (y >= track_top + track_h) {
                            /* Down arrow: one row down. */
                            dosgui_explorer_scroll(1);
                        } else if (y >= thumb_y && y < thumb_y + thumb_h) {
                            /* Grab the thumb to drag. */
                            ex->sb_dragging = true;
                            ex->sb_drag_off = y - thumb_y;
                        } else if (y < thumb_y) {
                            /* Track above thumb: page up. */
                            dosgui_explorer_scroll(-vis);
                        } else {
                            /* Track below thumb: page down. */
                            dosgui_explorer_scroll(vis);
                        }
                        return;
                    }
                }

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
                    /* Details/List/Tiles - single column. The visible row
                     * maps to entry list_scroll_y + row. */
                    int idx = ex->list_scroll_y + rel_y / row_h;
                    if (idx >= 0 && idx < ex->entry_count) {
                        ex->focus_idx = idx;
                        ex->anchor_idx = idx;
                        if (!shift) {
                            dosgui_explorer_clear_selection();
                            dosgui_explorer_toggle_selection(idx);
                        }
                        /* XP parity: double-click (same entry, ~500ms) opens
                         * it — navigate into a folder/drive, MIME-launch a file. */
                        uint64_t now = ex_now_ms();
                        bool dbl = (ex->last_click_idx == idx &&
                                    now - ex->last_click_ms < 500);
                        if (dbl) {
                            ExEntry *e = &ex->entries[idx];
                            if (e->type == EX_ENTRY_DIR || e->type == EX_ENTRY_DRIVE) {
                                dosgui_explorer_navigate(e->full_path);
                            } else if (wubu_mime_launch(e->full_path, NULL) == 0) {
                                snprintf(ex->status_text, sizeof(ex->status_text),
                                         "Launched: %s", e->name);
                            } else {
                                snprintf(ex->status_text, sizeof(ex->status_text),
                                         "Failed to launch: %s", e->name);
                            }
                            ex->last_click_idx = -1;
                            ex->last_click_ms = 0;
                        } else {
                            ex->last_click_idx = idx;
                            ex->last_click_ms = now;
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
        if (ex->sb_dragging) {
            ex->sb_dragging = false;
            ex->sb_drag_off = 0;
            return;
        }
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
