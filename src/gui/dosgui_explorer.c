/*
 * dosgui_explorer.c  --  WuBuOS File Manager (Win98/XP Explorer Shell)
 *
 * Phase 5: Full-featured file manager with:
 *   - Tree view sidebar (folder hierarchy, expand/collapse)
 *   - Breadcrumbs address bar (clickable path segments)
 *   - File list view (Details, Icons, List, Tiles)
 *   - Preview pane (text, images, metadata)
 *   - File operations (Copy, Move, Delete, Rename, New Folder)
 *   - Context menus (Open, Properties, Cut, Copy, Paste, Delete, New)
 *   - Zip archive mount/browse (via libzip integration)
 *   - Multi-select (Shift/Ctrl click, rubber band)
 *   - Keyboard shortcuts (F2 rename, Delete, Ctrl+C/V/X, Enter open)
 *   - Column sorting (Name, Size, Type, Date Modified)
 *   - Drive/Volume list (from Styx/9P namespace)
 */

#include "dosgui_explorer.h"
#include "dosgui_explorer_internal.h"
#include "dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <stdint.h>
#include <zlib.h>
#include <stdlib.h>

/* Safe dirname wrapper - avoids modifying input string */
/* -- Global State ------------------------------------------------- */

ExExplorerState g_explorer = {0};

/* -- Forward Declarations ----------------------------------------- */

void ex_scan_directory(const char *path);
void ex_tree_scan(ExTreeNode *node);
void ex_update_breadcrumbs(ExExplorerState *ex);
void ex_add_to_history(ExExplorerState *ex, const char *path);
const char *ex_file_type_str(ExEntryType type);
void ex_show_context_menu(ExExplorerState *ex, int mx, int my, int entry_idx);
void ex_hide_context_menu(ExExplorerState *ex);

/* Global sort context for qsort */
ExExplorerState *g_sort_ctx = NULL;

/* Shift key state tracker — updated from key handler, read from mouse handler.
 * Defined here; declared extern in dosgui_explorer_internal.h for the input module. */
bool g_shift_pressed = false;
bool g_ctrl_pressed  = false;

/* -- Case-insensitive substring search (portable, no GNU extensions) --- */
int str_contains_nocase(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return 1;
    int hlen = strlen(haystack);
    int nlen = strlen(needle);
    for (int i = 0; i <= hlen - nlen; i++) {
        int match = 1;
        for (int j = 0; j < nlen; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

/* -- Find State (defined here; declared extern in internal header) ----- */
ExFindState g_find_state = {0};

/* -- Lifecycle ---------------------------------------------------- */

int dosgui_explorer_init(void) {
    memset(&g_explorer, 0, sizeof(g_explorer));

    /* Context menu starts hidden (memset sets 0, but 0 >= 0 triggers the check) */
    g_explorer.context_menu_x = -1;
    g_explorer.context_menu_y = -1;
    g_explorer.context_menu_entry = -1;

    /* Initialize default columns */
    g_explorer.columns[0] = (ExColumn){"Name", EX_COLUMN_NAME_W, 80, EX_SORT_NAME, true, true};
    g_explorer.columns[1] = (ExColumn){"Size", EX_COLUMN_SIZE_W, 50, EX_SORT_SIZE, true, true};
    g_explorer.columns[2] = (ExColumn){"Type", EX_COLUMN_TYPE_W, 60, EX_SORT_TYPE, true, true};
    g_explorer.columns[3] = (ExColumn){"Date Modified", EX_COLUMN_DATE_W, 80, EX_SORT_DATE, true, true};
    g_explorer.column_count = 4;

    /* Default view mode */
    g_explorer.view_mode = EX_VIEW_DETAILS;
    g_explorer.sort_column = EX_SORT_NAME;
    g_explorer.sort_ascending = true;
    g_explorer.show_hidden = false;
    g_explorer.show_extensions = true;
    g_explorer.single_click_open = false;
    g_explorer.toolbar_visible = true;
    g_explorer.preview_visible = true;
    g_explorer.preview_w = EX_PREVIEW_W;
    g_explorer.tree_w = 220;

    /* Start at home directory */
    const char *home = getenv("HOME");
    if (home) {
        strncpy(g_explorer.current_path, home, EX_MAX_PATH - 1);
    } else {
        strcpy(g_explorer.current_path, "/");
    }

    ex_scan_directory(g_explorer.current_path);
    ex_populate_tree(g_explorer.tree_root, g_explorer.current_path);
    ex_update_breadcrumbs(&g_explorer);
    ex_add_to_history(&g_explorer, g_explorer.current_path);

    return 0;
}

void dosgui_explorer_shutdown(void) {
    if (g_explorer.tree_root) {
        ex_tree_free(g_explorer.tree_root);
        g_explorer.tree_root = NULL;
    }
    if (g_explorer.preview.img_pixels) {
        free(g_explorer.preview.img_pixels);
        g_explorer.preview.img_pixels = NULL;
    }
    memset(&g_explorer, 0, sizeof(g_explorer));
}

/* -- Window Management -------------------------------------------- */

void dosgui_explorer_show(void) {
    if (!dosgui_explorer_is_open()) {
        DosGuiWindow *win = dosgui_wm_create(100, 100, 900, 600, "File Manager");
        if (win) {
            static ExExplorerState *ex_ref = &g_explorer;
            win->user_data = ex_ref;
            win->on_draw = NULL; /* Custom rendering in WM */
            win->on_key = NULL;
            win->on_mouse = NULL;
            g_explorer.win_id = win->id;
        }
    }
}

void dosgui_explorer_hide(void) {
    if (dosgui_explorer_is_open()) {
        DosGuiWindow *win = dosgui_wm_find_by_id(g_explorer.win_id);
        if (win) {
            dosgui_wm_destroy(win);
        }
        g_explorer.win_id = 0;
    }
    ex_hide_context_menu(&g_explorer);
}

bool dosgui_explorer_is_open(void) {
    return g_explorer.win_id > 0 && dosgui_wm_find_by_id(g_explorer.win_id) != NULL;
}

void dosgui_explorer_toggle(void) {
    if (dosgui_explorer_is_open()) {
        dosgui_explorer_hide();
    } else {
        dosgui_explorer_show();
    }
}

/* -- Navigation --------------------------------------------------- */

void dosgui_explorer_navigate(const char *path) {
    if (!path) return;

    char resolved[EX_MAX_PATH];
    if (realpath(path, resolved)) {
        strncpy(g_explorer.current_path, resolved, EX_MAX_PATH - 1);
    } else {
        strncpy(g_explorer.current_path, path, EX_MAX_PATH - 1);
    }

    ex_scan_directory(g_explorer.current_path);
    if (g_explorer.tree_selected) {
        ex_tree_free(g_explorer.tree_root);
        g_explorer.tree_root = NULL;
    }
    ex_populate_tree(g_explorer.tree_root, g_explorer.current_path);
    ex_update_breadcrumbs(&g_explorer);
    ex_add_to_history(&g_explorer, g_explorer.current_path);

    /* Update tree selection */
    if (g_explorer.tree_root) {
        ExTreeNode *node = ex_tree_find(g_explorer.tree_root, g_explorer.current_path);
        if (node) g_explorer.tree_selected = node;
    }

    /* Update preview */
    g_explorer.preview.type = EX_PREVIEW_NONE;
    g_explorer.preview.path[0] = '\0';
}

void dosgui_explorer_go_up(void) {
    char parent[EX_MAX_PATH];
    ex_dirname_safe(g_explorer.current_path, parent, EX_MAX_PATH);
    if (strcmp(parent, g_explorer.current_path) != 0) {
        dosgui_explorer_navigate(parent);
    } else if (strcmp(g_explorer.current_path, "/") != 0) {
        dosgui_explorer_navigate("/");
    }
}

void dosgui_explorer_go_back(void) {
    if (g_explorer.history_pos > 0) {
        g_explorer.history_pos--;
        strncpy(g_explorer.current_path, g_explorer.history[g_explorer.history_pos], EX_MAX_PATH - 1);
        dosgui_explorer_navigate(g_explorer.current_path);
    }
}

void dosgui_explorer_go_forward(void) {
    if (g_explorer.history_pos < 31 && g_explorer.history[g_explorer.history_pos + 1][0]) {
        g_explorer.history_pos++;
        strncpy(g_explorer.current_path, g_explorer.history[g_explorer.history_pos], EX_MAX_PATH - 1);
        dosgui_explorer_navigate(g_explorer.current_path);
    }
}

void dosgui_explorer_refresh(void) {
    dosgui_explorer_navigate(g_explorer.current_path);
}

/* -- Tree View ---------------------------------------------------- */

void dosgui_explorer_tree_expand(ExTreeNode *node) {
    if (!node || node->expanded) return;
    node->expanded = true;
    if (!node->scanned) {
        ex_tree_scan(node);
    }
}

void dosgui_explorer_tree_collapse(ExTreeNode *node) {
    if (!node || !node->expanded) return;
    node->expanded = false;
}

void dosgui_explorer_tree_select(ExTreeNode *node) {
    if (!node) return;
    g_explorer.tree_selected = node;
    dosgui_explorer_navigate(node->path);
}

ExTreeNode *dosgui_explorer_tree_find(const char *path) {
    return g_explorer.tree_root ? ex_tree_find(g_explorer.tree_root, path) : NULL;
}

void dosgui_explorer_tree_scan(ExTreeNode *node) {
    if (!node || node->scanned) return;
    ex_populate_tree(node, node->path);
    node->scanned = true;
}

void ex_tree_scan(ExTreeNode *node) {
    if (!node || node->scanned) return;
    ex_populate_tree(node, node->path);
    node->scanned = true;
}

/* -- Selection ---------------------------------------------------- */

void dosgui_explorer_select_all(void) {
    g_explorer.selection_count = 0;
    for (int i = 0; i < g_explorer.entry_count && i < EX_MAX_SELECTION; i++) {
        if (!g_explorer.entries[i].hidden || g_explorer.show_hidden) {
            g_explorer.selected_indices[g_explorer.selection_count++] = i;
            g_explorer.entries[i].is_selected = true;
        }
    }
    g_explorer.anchor_idx = 0;
    g_explorer.focus_idx = 0;
}

void dosgui_explorer_clear_selection(void) {
    for (int i = 0; i < g_explorer.selection_count; i++) {
        int idx = g_explorer.selected_indices[i];
        if (idx >= 0 && idx < g_explorer.entry_count) {
            g_explorer.entries[idx].is_selected = false;
        }
    }
    g_explorer.selection_count = 0;
    g_explorer.anchor_idx = -1;
}

void dosgui_explorer_toggle_selection(int idx) {
    if (idx < 0 || idx >= g_explorer.entry_count) return;
    ExEntry *entry = &g_explorer.entries[idx];
    if (entry->hidden && !g_explorer.show_hidden) return;

    if (entry->is_selected) {
        entry->is_selected = false;
        for (int i = 0; i < g_explorer.selection_count; i++) {
            if (g_explorer.selected_indices[i] == idx) {
                for (int j = i; j < g_explorer.selection_count - 1; j++) {
                    g_explorer.selected_indices[j] = g_explorer.selected_indices[j + 1];
                }
                g_explorer.selection_count--;
                break;
            }
        }
    } else {
        entry->is_selected = true;
        if (g_explorer.selection_count < EX_MAX_SELECTION) {
            g_explorer.selected_indices[g_explorer.selection_count++] = idx;
        }
    }
}

void dosgui_explorer_select_range(int start, int end) {
    if (start > end) { int t = start; start = end; end = t; }
    for (int i = start; i <= end && i < g_explorer.entry_count; i++) {
        if (!g_explorer.entries[i].hidden || g_explorer.show_hidden) {
            dosgui_explorer_toggle_selection(i);
        }
    }
}

bool dosgui_explorer_is_selected(int idx) {
    if (idx < 0 || idx >= g_explorer.entry_count) return false;
    return g_explorer.entries[idx].is_selected;
}

/* -- View --------------------------------------------------------- */

void dosgui_explorer_set_view_mode(ExViewMode mode) {
    if (mode >= 0 && mode < 4) {
        g_explorer.view_mode = mode;
    }
}

void dosgui_explorer_set_sort(ExSortColumn col, bool ascending) {
    if (col >= 0 && col < EX_SORT_COUNT) {
        g_explorer.sort_column = col;
        g_explorer.sort_ascending = ascending;
        ex_sort_entries(&g_explorer);
    }
}

void dosgui_explorer_toggle_hidden(void) {
    g_explorer.show_hidden = !g_explorer.show_hidden;
    ex_scan_directory(g_explorer.current_path);
}

void dosgui_explorer_toggle_extensions(void) {
    g_explorer.show_extensions = !g_explorer.show_extensions;
}

void dosgui_explorer_toggle_preview(void) {
    g_explorer.preview_visible = !g_explorer.preview_visible;
}

void dosgui_explorer_toggle_toolbar(void) {
    g_explorer.toolbar_visible = !g_explorer.toolbar_visible;
}

/* -- File Operations ---------------------------------------------- */


/* -- Properties --------------------------------------------------- */

void dosgui_explorer_show_properties(int idx) {
    if (idx < 0 || idx >= g_explorer.entry_count) return;
    ExEntry *entry = &g_explorer.entries[idx];
    if (entry->hidden && !g_explorer.show_hidden) return;

    char msg[512];
    dosgui_explorer_format_size(entry->size, msg, sizeof(msg));
    snprintf(g_explorer.status_text, sizeof(g_explorer.status_text),
             "Properties: %s (%s)", entry->name, msg);
    /* In real implementation: show modal dialog */
}

/* Input handling moved to dosgui_explorer_input.c
 * (dosgui_explorer_handle_key / dosgui_explorer_handle_mouse). */

/* -- State Accessors ---------------------------------------------- */

ExExplorerState *dosgui_explorer_state(void) {
    return &g_explorer;
}

const char *dosgui_explorer_current_path(void) {
    return g_explorer.current_path;
}

/* -- Drive/Volume Enumeration ------------------------------------- */

/* -- Internal Implementation -------------------------------------- */

void ex_scan_directory(const char *path) {
    ExExplorerState *ex = &g_explorer;
    ex->entry_count = 0;
    ex->status_file_count = 0;
    ex->status_total_size = 0;

    DIR *dir = ex_9p_opendir(path);
    if (!dir) {
        snprintf(ex->status_text, sizeof(ex->status_text), "Cannot open: %s", strerror(errno));
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) && ex->entry_count < EX_MAX_ENTRIES) {
        if (!ex->show_hidden && ent->d_name[0] == '.') continue;

        ExEntry *entry = &ex->entries[ex->entry_count];
        snprintf(entry->full_path, sizeof(entry->full_path), "%s/%s", path, ent->d_name);
        strncpy(entry->name, ent->d_name, sizeof(entry->name) - 1);

        ex_get_file_info(entry->full_path, entry);

        ex->status_file_count++;
        ex->status_total_size += entry->size;

        ex->entry_count++;
    }
    closedir(dir);

    ex_sort_entries(ex);
    ex->focus_idx = 0;
    ex->anchor_idx = 0;
    ex->selection_count = 0;
}

/* Static case-insensitive string compare */

void ex_update_breadcrumbs(ExExplorerState *ex) {
    ex->breadcrumb_count = 0;

    if (strcmp(ex->current_path, "/") == 0) {
        strcpy(ex->breadcrumb_segments[0], "Computer");
        ex->breadcrumb_count = 1;
        return;
    }

    char *path_copy = strdup(ex->current_path);
    char *components[64];
    int comp_count = 0;

    char *tok = strtok(path_copy, "/");
    while (tok && comp_count < 64) {
        components[comp_count++] = tok;
        tok = strtok(NULL, "/");
    }

    /* First is "Computer", then path components */
    if (comp_count > 0) {
        strncpy(ex->breadcrumb_segments[0], "Computer", 255);
        ex->breadcrumb_count = 1;
        for (int i = 0; i < comp_count; i++) {
            if (ex->breadcrumb_count < 32) {
                strncpy(ex->breadcrumb_segments[ex->breadcrumb_count], components[i], 255);
                ex->breadcrumb_count++;
            }
        }
    }

    free(path_copy);
}

void ex_add_to_history(ExExplorerState *ex, const char *path) {
    /* Truncate forward history */
    for (int i = ex->history_pos + 1; i < 32; i++) {
        ex->history[i][0] = '\0';
    }

    if (ex->history_pos < 31) {
        ex->history_pos++;
    } else {
        /* Shift history */
        for (int i = 0; i < 31; i++) {
            strcpy(ex->history[i], ex->history[i + 1]);
        }
        ex->history_pos = 31;
    }
    strncpy(ex->history[ex->history_pos], path, EX_MAX_PATH - 1);
}

void ex_show_context_menu(ExExplorerState *ex, int mx, int my, int entry_idx) {
    ex->context_menu_x = mx;
    ex->context_menu_y = my;
    ex->context_menu_entry = entry_idx;
}

void ex_hide_context_menu(ExExplorerState *ex) {
    ex->context_menu_x = -1;
    ex->context_menu_y = -1;
    ex->context_menu_entry = -1;
}

/* -- Preview Update ----------------------------------------------- */

void dosgui_explorer_update_drive_list(void) {
    /* Refresh tree root with new drive list */
    if (g_explorer.tree_root) {
        ex_tree_free(g_explorer.tree_root);
        g_explorer.tree_root = NULL;
    }
    ex_populate_tree(g_explorer.tree_root, g_explorer.current_path);
}

