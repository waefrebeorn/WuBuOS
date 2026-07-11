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

static void ex_scan_directory(const char *path);
static void ex_tree_scan(ExTreeNode *node);
void ex_update_breadcrumbs(ExExplorerState *ex);
static void ex_add_to_history(ExExplorerState *ex, const char *path);
static const char *ex_file_type_str(ExEntryType type);
static void ex_show_context_menu(ExExplorerState *ex, int mx, int my, int entry_idx);
static void ex_hide_context_menu(ExExplorerState *ex);

/* Global sort context for qsort */
ExExplorerState *g_sort_ctx = NULL;

/* Shift key state tracker — updated from key handler, read from mouse handler */
static bool g_shift_pressed = false;
static bool g_ctrl_pressed  = false;

/* -- Case-insensitive substring search (portable, no GNU extensions) --- */
static int str_contains_nocase(const char *haystack, const char *needle) {
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

/* -- Find State --------------------------------------------------- */
#define EX_FIND_MAX_LEN 256
static struct {
    bool    active;
    char    query[EX_FIND_MAX_LEN];
    int     query_len;
    int     last_match;     /* index of last match, -1 = none */
    bool    case_sensitive;
} g_find_state = {0};

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

static void ex_tree_scan(ExTreeNode *node) {
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

void dosgui_explorer_copy(void) {
    if (g_explorer.selection_count == 0) return;
    g_explorer.clipboard_op = EX_OP_COPY;
    g_explorer.clipboard_count = 0;
    for (int i = 0; i < g_explorer.selection_count && i < EX_MAX_SELECTION; i++) {
        int idx = g_explorer.selected_indices[i];
        if (idx >= 0 && idx < g_explorer.entry_count) {
            strncpy(g_explorer.clipboard_paths[g_explorer.clipboard_count],
                    g_explorer.entries[idx].full_path, EX_MAX_PATH - 1);
            g_explorer.clipboard_count++;
        }
    }
    snprintf(g_explorer.status_text, sizeof(g_explorer.status_text),
             "Copied %d item(s) to clipboard", g_explorer.clipboard_count);
}

void dosgui_explorer_cut(void) {
    if (g_explorer.selection_count == 0) return;
    g_explorer.clipboard_op = EX_OP_MOVE;
    g_explorer.clipboard_count = 0;
    for (int i = 0; i < g_explorer.selection_count && i < EX_MAX_SELECTION; i++) {
        int idx = g_explorer.selected_indices[i];
        if (idx >= 0 && idx < g_explorer.entry_count) {
            strncpy(g_explorer.clipboard_paths[g_explorer.clipboard_count],
                    g_explorer.entries[idx].full_path, EX_MAX_PATH - 1);
            g_explorer.clipboard_count++;
        }
    }
    snprintf(g_explorer.status_text, sizeof(g_explorer.status_text),
             "Cut %d item(s) to clipboard", g_explorer.clipboard_count);
}

void dosgui_explorer_paste(void) {
    if (g_explorer.clipboard_count == 0) return;

    g_explorer.file_op.type = g_explorer.clipboard_op;
    g_explorer.file_op.count = g_explorer.clipboard_count;
    g_explorer.file_op.current_idx = 0;
    g_explorer.file_op.total_bytes = 0;
    g_explorer.file_op.copied_bytes = 0;
    g_explorer.file_op.in_progress = true;
    g_explorer.file_op.error = false;
    strncpy(g_explorer.file_op.dest_path, g_explorer.current_path, EX_MAX_PATH - 1);

    for (int i = 0; i < g_explorer.clipboard_count; i++) {
        strncpy(g_explorer.file_op.paths[i], g_explorer.clipboard_paths[i], EX_MAX_PATH - 1);
        struct stat st;
        if (ex_9p_stat(g_explorer.file_op.paths[i], &st) == 0) {
            g_explorer.file_op.total_bytes += st.st_size;
        }
    }

    /* Start async worker (simplified - in real implementation would fork) */
    ex_handle_file_op(&g_explorer);
}

void dosgui_explorer_delete(bool permanent) {
    if (g_explorer.selection_count == 0) return;

    g_explorer.file_op.type = EX_OP_DELETE;
    g_explorer.file_op.count = g_explorer.selection_count;
    g_explorer.file_op.current_idx = 0;
    g_explorer.file_op.in_progress = true;
    g_explorer.file_op.error = false;

    for (int i = 0; i < g_explorer.selection_count && i < EX_MAX_SELECTION; i++) {
        int idx = g_explorer.selected_indices[i];
        if (idx >= 0 && idx < g_explorer.entry_count) {
            strncpy(g_explorer.file_op.paths[i], g_explorer.entries[idx].full_path, EX_MAX_PATH - 1);
        }
    }

    ex_handle_file_op(&g_explorer);
    dosgui_explorer_clear_selection();
}

void dosgui_explorer_rename(int idx) {
    if (idx < 0 || idx >= g_explorer.entry_count) return;
    ExEntry *entry = &g_explorer.entries[idx];
    if (entry->hidden && !g_explorer.show_hidden) return;

    /* Rename using an inline edit - for now, append _renamed to demonstrate */
    char new_path[EX_MAX_PATH];
    const char *old_name = entry->name;
    const char *dot = strrchr(old_name, '.');
    
    if (dot) {
        snprintf(new_path, sizeof(new_path), "%s/%.*s_renamed%s", 
                 g_explorer.current_path, (int)(dot - old_name), old_name, dot);
    } else {
        snprintf(new_path, sizeof(new_path), "%s/%s_renamed", g_explorer.current_path, old_name);
    }
    
    if (rename(entry->full_path, new_path) == 0) {
        dosgui_explorer_refresh();
        WUBU_SNPRINTF(g_explorer.status_text, sizeof(g_explorer.status_text),
                 "Renamed: %s", old_name);
    } else {
        WUBU_SNPRINTF(g_explorer.status_text, sizeof(g_explorer.status_text),
                 "Rename failed: %s", strerror(errno));
    }
}

void dosgui_explorer_new_folder(void) {
    char name[256] = "New Folder";
    char path[EX_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", g_explorer.current_path, name);

    /* Find unique name - use mkdir with EEXIST check instead of access() to avoid TOCTOU */
    int counter = 1;
    while (mkdir(path, 0755) != 0) {
        if (errno != EEXIST) {
            WUBU_SNPRINTF(g_explorer.status_text, sizeof(g_explorer.status_text),
                     "Failed to create folder: %s", strerror(errno));
            return;
        }
        WUBU_SNPRINTF(name, sizeof(name), "New Folder (%d)", counter++);
        WUBU_SNPRINTF(path, sizeof(path), "%s/%s", g_explorer.current_path, name);
    }

    dosgui_explorer_refresh();
    WUBU_SNPRINTF(g_explorer.status_text, sizeof(g_explorer.status_text),
             "Created folder: %s", name);
}

void dosgui_explorer_new_file(const char *template_name) {
    if (!template_name) template_name = "New File";
    char name[256];
    char path[EX_MAX_PATH];
    WUBU_SNPRINTF(name, sizeof(name), "%s", template_name);
    WUBU_SNPRINTF(path, sizeof(path), "%s/%s", g_explorer.current_path, name);

    int counter = 1;
    /* Use open with O_EXCL to avoid TOCTOU race */
    while (1) {
        int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0644);
        if (fd >= 0) {
            close(fd);
            dosgui_explorer_refresh();
            WUBU_SNPRINTF(g_explorer.status_text, sizeof(g_explorer.status_text),
                     "Created file: %s", name);
            return;
        }
        if (errno != EEXIST) {
            WUBU_SNPRINTF(g_explorer.status_text, sizeof(g_explorer.status_text),
                     "Failed to create file: %s", strerror(errno));
            return;
        }
        char *dot = strrchr(name, '.');
        if (dot) {
            *dot = '\0';
            WUBU_SNPRINTF(name, sizeof(name), "%s (%d)%s", name, counter++, dot);
        } else {
            WUBU_SNPRINTF(name, sizeof(name), "%s (%d)", name, counter++);
        }
        WUBU_SNPRINTF(path, sizeof(path), "%s/%s", g_explorer.current_path, name);
    }
}

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
                    extern int wubu_mime_launch(const char *file_path, const char *handler_id);
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

/* -- State Accessors ---------------------------------------------- */

ExExplorerState *dosgui_explorer_state(void) {
    return &g_explorer;
}

const char *dosgui_explorer_current_path(void) {
    return g_explorer.current_path;
}

/* -- Drive/Volume Enumeration ------------------------------------- */

/* -- Internal Implementation -------------------------------------- */

static void ex_scan_directory(const char *path) {
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

static void ex_add_to_history(ExExplorerState *ex, const char *path) {
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

static void ex_show_context_menu(ExExplorerState *ex, int mx, int my, int entry_idx) {
    ex->context_menu_x = mx;
    ex->context_menu_y = my;
    ex->context_menu_entry = entry_idx;
}

static void ex_hide_context_menu(ExExplorerState *ex) {
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

