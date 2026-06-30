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

/* -- Safe String Macros (WUBU_SAFE_STRING) -------------------------- */

#define WUBU_STRCPY(dst, src, dst_size) \
    do { \
        if (dst_size > 0) { \
            strncpy((dst), (src), (dst_size) - 1); \
            (dst)[(dst_size) - 1] = '\0'; \
        } \
    } while (0)

#define WUBU_SNPRINTF(dst, dst_size, fmt, ...) \
    do { \
        if (dst_size > 0) { \
            snprintf((dst), (dst_size), (fmt), __VA_ARGS__); \
            (dst)[(dst_size) - 1] = '\0'; \
        } \
    } while (0)

#define WUBU_STRLCAT(dst, src, dst_size) \
    do { \
        size_t _dst_len = strlen(dst); \
        size_t _src_len = strlen(src); \
        if (_dst_len + _src_len + 1 <= dst_size) { \
            memcpy((dst) + _dst_len, (src), _src_len + 1); \
        } else if (_dst_len < dst_size) { \
            size_t _avail = (dst_size) - _dst_len - 1; \
            memcpy((dst) + _dst_len, (src), _avail); \
            (dst)[(dst_size) - 1] = '\0'; \
        } \
    } while (0)

/* Safe dirname wrapper - avoids modifying input string */
static inline void ex_dirname_safe(const char *path, char *out, size_t out_size) {
    if (!path || !out || out_size == 0) return;
    char copy[EX_MAX_PATH];
    strncpy(copy, path, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';
    char *d = dirname(copy);
    WUBU_STRCPY(out, d, out_size);
}

/* -- Global State ------------------------------------------------- */

ExExplorerState g_explorer = {0};

/* -- Forward Declarations ----------------------------------------- */

static void ex_render_tree(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h);
static void ex_render_breadcrumbs(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h);
static void ex_render_toolbar(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h);
static void ex_render_file_list(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h);
static void ex_render_preview(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h);
static void ex_render_statusbar(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h);
static void ex_render_context_menu(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h);

static const WubuThemeColors *tc(void) { return wubu_theme_colors(); }
static const WubuTheme *th(void) { return wubu_theme_get(); }
static int ex_row_h(void) { return th()->rounded_buttons ? 24 : EX_ROW_H; }
static int ex_tree_indent(void) { return th()->rounded_buttons ? 14 : EX_TREE_INDENT; }
static int ex_title_h(void) { return th()->rounded_buttons ? 24 : 22; }
static int ex_toolbar_h(void) { return EX_TOOLBAR_H; }
static int ex_breadcrumb_h(void) { return EX_BREADCRUMB_H; }
static int ex_statusbar_h(void) { return EX_STATUSBAR_H; }
static int ex_border_w(void) { return th()->rounded_buttons ? 3 : 2; }

static void ex_scan_directory(const char *path);
static void ex_populate_tree(ExTreeNode *node, const char *path);
static ExTreeNode *ex_tree_find(ExTreeNode *root, const char *path);
static void ex_tree_free(ExTreeNode *node);
static void ex_tree_scan(ExTreeNode *node);
static void ex_update_breadcrumbs(ExExplorerState *ex);
static void ex_add_to_history(ExExplorerState *ex, const char *path);
static void ex_sort_entries(ExExplorerState *ex);
static const char *ex_get_extension(const char *filename);
static const char *ex_file_type_str(ExEntryType type);
static void ex_get_file_info(const char *path, ExEntry *entry);
static int ex_file_compare(const void *a, const void *b);
static void ex_show_context_menu(ExExplorerState *ex, int mx, int my, int entry_idx);
static void ex_hide_context_menu(ExExplorerState *ex);
static void ex_handle_file_op(ExExplorerState *ex);
static void ex_worker_copy(const char *src, const char *dst, uint64_t *copied, uint64_t total);
static void ex_worker_move(const char *src, const char *dst);
static void ex_worker_delete(const char *path, bool permanent);

/* -- 9P/Styx File Operations (replacing local filesystem) ------------- */

static int ex_9p_stat(const char *path, struct stat *st) {
    /* Use Styx 9P stat via styxfs */
    extern int styxfs_stat(const char *path, struct stat *st);
    return styxfs_stat(path, st);
}

static int ex_9p_mkdir(const char *path, mode_t mode) {
    extern int styxfs_create(const char *path, int mode, int perm);
    return styxfs_create(path, 0x80000000 | 0x10000000, mode); /* DMODE | DMDIR */
}

static int ex_9p_unlink(const char *path) {
    extern int styxfs_remove(const char *path);
    return styxfs_remove(path);
}

static int ex_9p_rename(const char *oldpath, const char *newpath) {
    extern int styxfs_rename(const char *oldpath, const char *newpath);
    return styxfs_rename(oldpath, newpath);
}

static int ex_9p_open(const char *path, int flags) {
    extern int styxfs_open(const char *path, int flags);
    return styxfs_open(path, flags);
}

static ssize_t ex_9p_read(int fd, void *buf, size_t count) {
    extern ssize_t styxfs_read(int fd, void *buf, size_t count);
    return styxfs_read(fd, buf, count);
}

static ssize_t ex_9p_write(int fd, const void *buf, size_t count) {
    extern ssize_t styxfs_write(int fd, const void *buf, size_t count);
    return styxfs_write(fd, buf, count);
}

static int ex_9p_close(int fd) {
    extern int styxfs_close(int fd);
    return styxfs_close(fd);
}

static int ex_9p_readdir(const char *path, struct dirent ***entries) {
    extern int styxfs_readdir(const char *path, struct dirent ***entries);
    return styxfs_readdir(path, entries);
}

static DIR *ex_9p_opendir(const char *path) {
    extern DIR *styxfs_opendir(const char *path);
    return styxfs_opendir(path);
}

/* Global sort context for qsort */
static ExExplorerState *g_sort_ctx = NULL;

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

/* -- Zip Archives (Real Implementation using zlib + libzip dlopen) ---- */

/* ZIP local file header signature */
#define ZIP_LOCAL_FILE_HEADER_SIG   0x04034b50
#define ZIP_CENTRAL_DIR_HEADER_SIG  0x02014b50
#define ZIP_END_CENTRAL_DIR_SIG     0x06054b50

#pragma pack(push, 1)
typedef struct {
    uint32_t signature;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression_method;
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len;
    uint16_t extra_field_len;
} ZipLocalHeader;

typedef struct {
    uint32_t signature;
    uint16_t version_made_by;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression_method;
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len;
    uint16_t extra_field_len;
    uint16_t comment_len;
    uint16_t disk_num_start;
    uint16_t internal_attr;
    uint32_t external_attr;
    uint32_t local_header_offset;
} ZipCentralDirHeader;

typedef struct {
    uint32_t signature;
    uint16_t disk_number;
    uint16_t central_dir_disk;
    uint16_t central_dir_entries_this_disk;
    uint16_t central_dir_entries_total;
    uint32_t central_dir_size;
    uint32_t central_dir_offset;
    uint16_t comment_len;
} ZipEndCentralDir;
#pragma pack(pop)

/* Libzip function pointers (dlopen) */
typedef struct {
    void *handle;
    void *(*zip_open)(const char *, int, int *);
    void (*zip_close)(void *);
    void *(*zip_fopen_index)(void *, uint64_t, int);
    void (*zip_fclose)(void *);
    int64_t (*zip_fread)(void *, void *, uint64_t);
    int (*zip_stat_index)(void *, uint64_t, int, void *);
    int64_t (*zip_get_num_entries)(void *, int);
    const char *(*zip_get_name)(void *, uint64_t, int);
} LibzipFunctions;

static LibzipFunctions g_libzip = {0};

static bool ex_libzip_load(void) {
    if (g_libzip.handle) return true;
    g_libzip.handle = dlopen("libzip.so.4", RTLD_LAZY);
    if (!g_libzip.handle) return false;
    g_libzip.zip_open = dlsym(g_libzip.handle, "zip_open");
    g_libzip.zip_close = dlsym(g_libzip.handle, "zip_close");
    g_libzip.zip_fopen_index = dlsym(g_libzip.handle, "zip_fopen_index");
    g_libzip.zip_fclose = dlsym(g_libzip.handle, "zip_fclose");
    g_libzip.zip_fread = dlsym(g_libzip.handle, "zip_fread");
    g_libzip.zip_stat_index = dlsym(g_libzip.handle, "zip_stat_index");
    g_libzip.zip_get_num_entries = dlsym(g_libzip.handle, "zip_get_num_entries");
    g_libzip.zip_get_name = dlsym(g_libzip.handle, "zip_get_name");
    return g_libzip.zip_open != NULL;
}

static void ex_libzip_unload(void) {
    if (g_libzip.handle) {
        dlclose(g_libzip.handle);
        memset(&g_libzip, 0, sizeof(g_libzip));
    }
}

/* Zip entry cache for mounted archive */
typedef struct {
    char name[256];
    uint64_t uncompressed_size;
    uint64_t compressed_size;
    uint32_t crc32;
    uint16_t compression_method;
    time_t modified;
    int central_dir_index;
    bool is_directory;
} ExZipEntry;

static ExZipEntry g_zip_entries[EX_MAX_ZIP_ENTRIES];
static int g_zip_entry_count = 0;
static bool g_zip_entries_valid = false;

/* Read zip central directory using raw file I/O (no libzip headers needed) */
static int ex_zip_read_central_directory(const char *zip_path) {
    if (!zip_path) return -1;
    
    int fd = open(zip_path, O_RDONLY);
    if (fd < 0) return -1;
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return -1;
    }
    
    /* Read last 64KB to find End of Central Directory record */
    size_t search_size = st.st_size < 65536 ? st.st_size : 65536;
    uint8_t *buf = malloc(search_size);
    if (!buf) {
        close(fd);
        return -1;
    }
    
    off_t read_offset = st.st_size - search_size;
    if (lseek(fd, read_offset, SEEK_SET) < 0) {
        free(buf);
        close(fd);
        return -1;
    }
    
    if (read(fd, buf, search_size) != (ssize_t)search_size) {
        free(buf);
        close(fd);
        return -1;
    }
    
    /* Search for End of Central Directory signature */
    ZipEndCentralDir *ecdr = NULL;
    for (size_t i = 0; i + sizeof(ZipEndCentralDir) <= search_size; i++) {
        if (*(uint32_t*)(buf + i) == ZIP_END_CENTRAL_DIR_SIG) {
            ecdr = (ZipEndCentralDir*)(buf + i);
            break;
        }
    }
    
    if (!ecdr) {
        free(buf);
        close(fd);
        return -1;
    }
    
    /* Now read central directory entries */
    off_t central_dir_offset = ecdr->central_dir_offset;
    uint16_t entry_count = ecdr->central_dir_entries_total;
    
    if (lseek(fd, central_dir_offset, SEEK_SET) < 0) {
        free(buf);
        close(fd);
        return -1;
    }
    
    g_zip_entry_count = 0;
    
    for (int i = 0; i < entry_count && g_zip_entry_count < EX_MAX_ZIP_ENTRIES; i++) {
        ZipCentralDirHeader cdh;
        if (read(fd, &cdh, sizeof(cdh)) != sizeof(cdh)) break;
        
        if (cdh.signature != ZIP_CENTRAL_DIR_HEADER_SIG) break;
        
        /* Read filename */
        char filename[256];
        if (cdh.filename_len >= sizeof(filename)) cdh.filename_len = sizeof(filename) - 1;
        if (read(fd, filename, cdh.filename_len) != cdh.filename_len) break;
        filename[cdh.filename_len] = '\0';
        
        /* Skip extra field and comment */
        lseek(fd, cdh.extra_field_len + cdh.comment_len, SEEK_CUR);
        
        /* Store entry */
        ExZipEntry *ze = &g_zip_entries[g_zip_entry_count];
        WUBU_STRCPY(ze->name, filename, sizeof(ze->name));
        ze->uncompressed_size = cdh.uncompressed_size;
        ze->compressed_size = cdh.compressed_size;
        ze->crc32 = cdh.crc32;
        ze->compression_method = cdh.compression_method;
        ze->central_dir_index = i;
        ze->is_directory = (filename[cdh.filename_len - 1] == '/') || (cdh.uncompressed_size == 0 && cdh.compressed_size == 0);
        
        /* Convert DOS time to time_t */
        struct tm tm = {0};
        tm.tm_year = ((cdh.last_mod_date >> 9) & 0x7F) + 80;  /* Years since 1900 */
        tm.tm_mon = ((cdh.last_mod_date >> 5) & 0xF) - 1;      /* 0-11 */
        tm.tm_mday = cdh.last_mod_date & 0x1F;                  /* 1-31 */
        tm.tm_hour = (cdh.last_mod_time >> 11) & 0x1F;
        tm.tm_min = (cdh.last_mod_time >> 5) & 0x3F;
        tm.tm_sec = (cdh.last_mod_time & 0x1F) * 2;
        ze->modified = mktime(&tm);
        
        g_zip_entry_count++;
    }
    
    g_zip_entries_valid = (g_zip_entry_count > 0);
    
    free(buf);
    close(fd);
    return g_zip_entry_count;
}

/* Populate explorer entries from zip cache */
static void ex_zip_populate_entries(void) {
    g_explorer.entry_count = 0;
    
    for (int i = 0; i < g_zip_entry_count && g_explorer.entry_count < EX_MAX_ENTRIES; i++) {
        ExZipEntry *ze = &g_zip_entries[i];
        
        ExEntry *entry = &g_explorer.entries[g_explorer.entry_count];
        WUBU_STRCPY(entry->name, ze->name, sizeof(entry->name));
        WUBU_SNPRINTF(entry->full_path, sizeof(entry->full_path), "%s/%s", g_explorer.current_zip_path, ze->name);
        
        if (ze->is_directory) {
            entry->type = EX_ENTRY_DIR;
        } else {
            entry->type = EX_ENTRY_FILE;
        }
        entry->size = ze->uncompressed_size;
        entry->modified = ze->modified;
        entry->created = ze->modified;
        entry->hidden = false;
        entry->readonly = true;  /* Zip entries are read-only in mount */
        entry->is_selected = false;
        
        /* Extract extension */
        const char *dot = strrchr(ze->name, '.');
        if (dot && !ze->is_directory) {
            WUBU_STRCPY(entry->extension, dot + 1, sizeof(entry->extension));
            for (char *p = entry->extension; *p; p++) *p = tolower(*p);
        } else {
            entry->extension[0] = '\0';
        }
        
        /* Store zip info */
        entry->zip_info.zip_index = ze->central_dir_index;
        WUBU_STRCPY(entry->zip_info.zip_path, g_explorer.current_zip_path, sizeof(entry->zip_info.zip_path));
        
        g_explorer.entry_count++;
    }
    
    /* Update breadcrumbs */
    ex_update_breadcrumbs(&g_explorer);
}

bool dosgui_explorer_mount_zip(const char *zip_path) {
    if (!zip_path) return false;
    
    /* Try libzip first if available */
    if (ex_libzip_load()) {
        int err = 0;
        void *archive = g_libzip.zip_open(zip_path, 0, &err);
        if (archive) {
            /* Success with libzip */
            WUBU_STRCPY(g_explorer.current_zip_path, zip_path, sizeof(g_explorer.current_zip_path));
            g_explorer.in_zip_archive = true;
            
            /* Use libzip to populate entries */
            int64_t num_entries = g_libzip.zip_get_num_entries(archive, 0);
            g_zip_entry_count = 0;
            
            for (int64_t i = 0; i < num_entries && g_zip_entry_count < EX_MAX_ZIP_ENTRIES; i++) {
                const char *name = g_libzip.zip_get_name(archive, i, 0);
                if (!name) continue;
                
                ExZipEntry *ze = &g_zip_entries[g_zip_entry_count];
                WUBU_STRCPY(ze->name, name, sizeof(ze->name));
                ze->central_dir_index = (int)i;
                ze->is_directory = (name[strlen(name) - 1] == '/');
                g_zip_entry_count++;
            }
            
            g_libzip.zip_close(archive);
            g_zip_entries_valid = true;
            ex_zip_populate_entries();
            return true;
        }
    }
    
    /* Fallback: parse zip manually using zlib structures */
    int count = ex_zip_read_central_directory(zip_path);
    if (count > 0) {
        WUBU_STRCPY(g_explorer.current_zip_path, zip_path, sizeof(g_explorer.current_zip_path));
        g_explorer.in_zip_archive = true;
        ex_zip_populate_entries();
        return true;
    }
    
    return false;
}

void dosgui_explorer_unmount_zip(void) {
    g_explorer.in_zip_archive = false;
    g_explorer.current_zip_path[0] = '\0';
    g_zip_entries_valid = false;
    g_zip_entry_count = 0;
    dosgui_explorer_go_up();
}

bool dosgui_explorer_is_in_zip(void) {
    return g_explorer.in_zip_archive;
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

/* -- Rendering ---------------------------------------------------- */

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

static void ex_render_tree(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
    (void)fb; (void)fb_w; (void)fb_h;

    if (!ex->tree_root) return;

    int task_h = dosgui_taskbar_height();
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

static void ex_render_breadcrumbs(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
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

static void ex_render_toolbar(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
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

static void ex_render_file_list(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
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

static void ex_render_preview(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
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

static void ex_render_statusbar(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
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

static void ex_render_context_menu(ExExplorerState *ex, uint32_t *fb, int fb_w, int fb_h) {
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

/* -- State Accessors ---------------------------------------------- */

ExExplorerState *dosgui_explorer_state(void) {
    return &g_explorer;
}

const char *dosgui_explorer_current_path(void) {
    return g_explorer.current_path;
}

/* -- Helpers ------------------------------------------------------ */

const char *dosgui_explorer_type_str(ExEntryType type) {
    static const char *names[] = {
        "Unknown", "File", "Folder", "Link", "Drive", "Archive", "Mount", "Special"
    };
    if (type >= 0 && type < 8) return names[type];
    return "Unknown";
}

const char *dosgui_explorer_view_mode_name(ExViewMode mode) {
    static const char *names[] = {"Details", "Icons", "List", "Tiles"};
    if (mode >= 0 && mode < 4) return names[mode];
    return "Unknown";
}

uint32_t dosgui_explorer_type_color(ExEntryType type) {
    static const uint32_t colors[] = {
        0x808080,  /* Unknown - gray */
        0xFFFFFF,  /* File - white */
        0xFFD700,  /* Folder - gold */
        0x00FFFF,  /* Link - cyan */
        0x00FF00,  /* Drive - green */
        0xFF8000,  /* Zip - orange */
        0x8000FF,  /* Mount - purple */
        0xFF00FF   /* Special - magenta */
    };
    if (type >= 0 && type < 8) return colors[type];
    return 0x808080;
}

void dosgui_explorer_format_size(uint64_t bytes, char *buf, int buf_size) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = (double)bytes;
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    if (unit == 0) {
        snprintf(buf, buf_size, "%.0f %s", size, units[unit]);
    } else {
        snprintf(buf, buf_size, "%.1f %s", size, units[unit]);
    }
}

void dosgui_explorer_format_time(time_t t, char *buf, int buf_size) {
    struct tm *tm = localtime(&t);
    if (tm) {
        strftime(buf, buf_size, "%Y-%m-%d %H:%M", tm);
    } else {
        snprintf(buf, buf_size, "Unknown");
    }
}

/* -- Drive/Volume Enumeration ------------------------------------- */

int dosgui_explorer_enumerate_drives(char paths[][EX_MAX_PATH], char labels[][64], int max) {
    int count = 0;

    /* Add root filesystem */
    if (count < max) {
        strcpy(paths[count], "/");
        strcpy(labels[count], "Root Filesystem");
        count++;
    }

    /* On Linux, check /mnt for mounted volumes */
    DIR *mnt = ex_9p_opendir("/mnt");
    if (mnt) {
        struct dirent *ent;
        while ((ent = readdir(mnt)) && count < max) {
            if (ent->d_name[0] == '.') continue;

            char path[EX_MAX_PATH];
            snprintf(path, sizeof(path), "/mnt/%s", ent->d_name);

            struct stat st;
            if (ex_9p_stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                strncpy(paths[count], path, EX_MAX_PATH - 1);
                strncpy(labels[count], ent->d_name, 63);
                count++;
            }
        }
        closedir(mnt);
    }

    /* Check /media for removable media */
    DIR *media = ex_9p_opendir("/media");
    if (media) {
        struct dirent *ent;
        while ((ent = readdir(media)) && count < max) {
            if (ent->d_name[0] == '.') continue;

            char path[EX_MAX_PATH];
            snprintf(path, sizeof(path), "/media/%s", ent->d_name);

            struct stat st;
            if (ex_9p_stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                strncpy(paths[count], path, EX_MAX_PATH - 1);
                strncpy(labels[count], ent->d_name, 63);
                count++;
            }
        }
        closedir(media);
    }

    return count;
}

void dosgui_explorer_update_drive_list(void) {
    /* Refresh tree root with new drive list */
    if (g_explorer.tree_root) {
        ex_tree_free(g_explorer.tree_root);
        g_explorer.tree_root = NULL;
    }
    ex_populate_tree(g_explorer.tree_root, g_explorer.current_path);
}

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

static void ex_get_file_info(const char *path, ExEntry *entry) {
    struct stat st;
    if (ex_9p_stat(path, &st) != 0) {
        entry->type = EX_ENTRY_UNKNOWN;
        entry->size = 0;
        entry->modified = 0;
        entry->created = 0;
        strcpy(entry->type_str, "Unknown");
        entry->icon_color = dosgui_explorer_type_color(EX_ENTRY_UNKNOWN);
        return;
    }

    entry->size = st.st_size;
    entry->modified = st.st_mtime;
    entry->created = st.st_ctime;
    entry->hidden = (entry->name[0] == '.');
    entry->readonly = !(st.st_mode & S_IWUSR);
    entry->icon_color = 0xFFFFFF;

    if (S_ISDIR(st.st_mode)) {
        entry->type = EX_ENTRY_DIR;
        strcpy(entry->type_str, "File Folder");
        entry->icon_color = 0xFFD700; /* Gold */
    } else if (S_ISLNK(st.st_mode)) {
        entry->type = EX_ENTRY_SYMLINK;
        strcpy(entry->type_str, "Shortcut");
        entry->icon_color = 0x00FFFF; /* Cyan */
    } else if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) {
        entry->type = EX_ENTRY_SPECIAL;
        strcpy(entry->type_str, "Device");
        entry->icon_color = 0x8000FF; /* Purple */
    } else {
        entry->type = EX_ENTRY_FILE;
        const char *ext = ex_get_extension(entry->name);
        strncpy(entry->extension, ext, sizeof(entry->extension) - 1);

        /* Determine type by extension */
        if (strcmp(ext, "zip") == 0 || strcmp(ext, "tar") == 0 ||
            strcmp(ext, "gz") == 0 || strcmp(ext, "bz2") == 0 ||
            strcmp(ext, "xz") == 0 || strcmp(ext, "7z") == 0) {
            entry->type = EX_ENTRY_ZIP;
            strcpy(entry->type_str, "Compressed Archive");
            entry->icon_color = 0xFF8000; /* Orange */
        } else if (strcmp(ext, "txt") == 0 || strcmp(ext, "md") == 0 ||
                   strcmp(ext, "log") == 0 || strcmp(ext, "ini") == 0 ||
                   strcmp(ext, "cfg") == 0 || strcmp(ext, "conf") == 0) {
            strcpy(entry->type_str, "Text Document");
            entry->icon_color = 0xFFFFFF;
        } else if (strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0 ||
                   strcmp(ext, "cpp") == 0 || strcmp(ext, "hpp") == 0 ||
                   strcmp(ext, "py") == 0 || strcmp(ext, "js") == 0 ||
                   strcmp(ext, "sh") == 0 || strcmp(ext, "rs") == 0) {
            strcpy(entry->type_str, "Source Code");
            entry->icon_color = 0x00FF00;
        } else if (strcmp(ext, "png") == 0 || strcmp(ext, "jpg") == 0 ||
                   strcmp(ext, "jpeg") == 0 || strcmp(ext, "gif") == 0 ||
                   strcmp(ext, "bmp") == 0 || strcmp(ext, "webp") == 0) {
            strcpy(entry->type_str, "Image");
            entry->icon_color = 0xFF00FF;
        } else if (strcmp(ext, "mp3") == 0 || strcmp(ext, "wav") == 0 ||
                   strcmp(ext, "flac") == 0 || strcmp(ext, "ogg") == 0) {
            strcpy(entry->type_str, "Audio");
            entry->icon_color = 0x00FFFF;
        } else if (strcmp(ext, "mp4") == 0 || strcmp(ext, "mkv") == 0 ||
                   strcmp(ext, "avi") == 0 || strcmp(ext, "mov") == 0) {
            strcpy(entry->type_str, "Video");
            entry->icon_color = 0xFFFF00;
        } else if (strcmp(ext, "exe") == 0 || strcmp(ext, "dll") == 0 ||
                   strcmp(ext, "so") == 0 || strcmp(ext, "dylib") == 0) {
            strcpy(entry->type_str, "Executable");
            entry->icon_color = 0xFF0000;
        } else if (strcmp(ext, "pdf") == 0) {
            strcpy(entry->type_str, "PDF Document");
            entry->icon_color = 0xFF0000;
        } else if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0 ||
                   strcmp(ext, "css") == 0) {
            strcpy(entry->type_str, "Web Document");
            entry->icon_color = 0x0000FF;
        } else {
            snprintf(entry->type_str, sizeof(entry->type_str), "%s File",
                     ext[0] ? ext : "Unknown");
            entry->icon_color = 0xFFFFFF;
        }
    }

    /* ex_format_time(entry->modified, entry->type_str + strlen(entry->type_str), 0); // Not used, just formatting */
}

static const char *ex_get_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

/* Static case-insensitive string compare */
static int ex_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a - ((*b >= 'A' && *b <= 'Z') ? *b + 32 : *b);
}

static void ex_sort_entries(ExExplorerState *ex) {
    g_sort_ctx = ex;
    qsort(ex->entries, ex->entry_count, sizeof(ExEntry), ex_file_compare);
    g_sort_ctx = NULL;
}

static int ex_file_compare(const void *a, const void *b) {
    ExExplorerState *ex = g_sort_ctx;
    const ExEntry *ea = (const ExEntry *)a;
    const ExEntry *eb = (const ExEntry *)b;

    /* Folders first */
    bool da = (ea->type == EX_ENTRY_DIR || ea->type == EX_ENTRY_DRIVE);
    bool db = (eb->type == EX_ENTRY_DIR || eb->type == EX_ENTRY_DRIVE);
    if (da != db) return da ? -1 : 1;

    int cmp = 0;
    switch (ex->sort_column) {
        case EX_SORT_NAME:
            cmp = ex_strcasecmp(ea->name, eb->name);
            break;
        case EX_SORT_SIZE:
            if (ea->size < eb->size) cmp = -1;
            else if (ea->size > eb->size) cmp = 1;
            else cmp = 0;
            break;
        case EX_SORT_TYPE:
            cmp = ex_strcasecmp(ea->type_str, eb->type_str);
            break;
        case EX_SORT_DATE:
            if (ea->modified < eb->modified) cmp = -1;
            else if (ea->modified > eb->modified) cmp = 1;
            else cmp = 0;
            break;
        default:
            cmp = 0;
            break;
    }

    return ex->sort_ascending ? cmp : -cmp;
}

static void ex_populate_tree(ExTreeNode *node, const char *path) {
    /* Simplified: just add drives and current path hierarchy */
    ExExplorerState *ex = &g_explorer;

    if (!ex->tree_root) {
        ex->tree_root = (ExTreeNode *)calloc(1, sizeof(ExTreeNode));
        if (!ex->tree_root) return;

        strcpy(ex->tree_root->path, "/");
        strcpy(ex->tree_root->display_name, "Computer");
        ex->tree_root->is_drive = true;
        ex->tree_root->expanded = true;
        ex->tree_root->icon_color = 0x00FF00;
        ex->tree_root->scanned = true;
    }

    /* Add drive nodes */
    char drive_paths[16][EX_MAX_PATH];
    char drive_labels[16][64];
    int drive_count = dosgui_explorer_enumerate_drives(drive_paths, drive_labels, 16);

    for (int i = 0; i < drive_count; i++) {
        ExTreeNode *drive = (ExTreeNode *)calloc(1, sizeof(ExTreeNode));
        if (!drive) continue;

        strncpy(drive->path, drive_paths[i], EX_MAX_PATH - 1);
        strncpy(drive->display_name, drive_labels[i], 255);
        drive->parent = ex->tree_root;
        drive->is_drive = true;
        drive->expanded = false;
        drive->icon_color = 0x00FF00;

        /* Add to root's children */
        if (!ex->tree_root->first_child) {
            ex->tree_root->first_child = drive;
            ex->tree_root->children = drive;
        } else {
            ExTreeNode *sibling = ex->tree_root->first_child;
            while (sibling->next_sibling) sibling = sibling->next_sibling;
            sibling->next_sibling = drive;
        }
        ex->tree_root->child_count++;
    }

    /* Add current directory hierarchy */
    char *path_copy = strdup(path);
    char *components[64];
    int comp_count = 0;
    char *tok = strtok(path_copy, "/");
    while (tok && comp_count < 64) {
        components[comp_count++] = tok;
        tok = strtok(NULL, "/");
    }

    ExTreeNode *parent = ex->tree_root->first_child; /* Root -> drive */
    char build_path[EX_MAX_PATH] = "/";

    for (int i = 0; i < comp_count; i++) {
        if (i == 0) { /* First component is drive name */
            /* Find matching drive */
            ExTreeNode *drive = ex->tree_root->first_child;
            while (drive && strcmp(drive->display_name, components[i]) != 0) {
                drive = drive->next_sibling;
            }
            if (drive) {
                parent = drive;
                snprintf(build_path, sizeof(build_path), "/%s", components[i]);
            }
            continue;
        }

        strcat(build_path, "/");
        strcat(build_path, components[i]);

        /* Check if child exists */
        ExTreeNode *child = parent->first_child;
        ExTreeNode *found = NULL;
        while (child) {
            if (strcmp(child->display_name, components[i]) == 0) {
                found = child;
                break;
            }
            child = child->next_sibling;
        }

        if (!found) {
            found = (ExTreeNode *)calloc(1, sizeof(ExTreeNode));
            if (found) {
                strncpy(found->path, build_path, EX_MAX_PATH - 1);
                strncpy(found->display_name, components[i], 255);
                found->parent = parent;
                found->expanded = (i == comp_count - 1);
                found->icon_color = 0xFFD700;

                if (!parent->first_child) {
                    parent->first_child = found;
                    parent->children = found;
                } else {
                    ExTreeNode *sib = parent->first_child;
                    while (sib->next_sibling) sib = sib->next_sibling;
                    sib->next_sibling = found;
                }
                parent->child_count++;
            }
        }

        if (found) parent = found;
    }

    free(path_copy);
}

static ExTreeNode *ex_tree_find(ExTreeNode *root, const char *path) {
    if (!root) return NULL;
    if (strcmp(root->path, path) == 0) return root;

    ExTreeNode *child = root->first_child;
    while (child) {
        ExTreeNode *found = ex_tree_find(child, path);
        if (found) return found;
        child = child->next_sibling;
    }
    return NULL;
}

static void ex_tree_free(ExTreeNode *node) {
    if (!node) return;
    ExTreeNode *child = node->first_child;
    while (child) {
        ExTreeNode *next = child->next_sibling;
        ex_tree_free(child);
        child = next;
    }
    /* Also free children list if different from first_child to avoid leaks */
    if (node->children != node->first_child) {
        child = node->children;
        while (child) {
            ExTreeNode *next = child->next_sibling;
            if (child != node->first_child) {
                ex_tree_free(child);
            }
            child = next;
        }
    }
    free(node);
}

static void ex_update_breadcrumbs(ExExplorerState *ex) {
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

static void ex_worker_copy(const char *src, const char *dst, uint64_t *copied, uint64_t total) {
    (void)total;
    struct stat st;
    if (ex_9p_stat(src, &st) != 0) return;
    
    if (S_ISDIR(st.st_mode)) {
        /* Directory copy - recurse */
        struct dirent **entries;
        int count = ex_9p_readdir(src, &entries);
        if (count >= 0) {
            ex_9p_mkdir(dst, st.st_mode & 0777);
            for (int i = 0; i < count; i++) {
                if (strcmp(entries[i]->d_name, ".") == 0 || strcmp(entries[i]->d_name, "..") == 0) {
                    free(entries[i]);
                    continue;
                }
                char child_src[EX_MAX_PATH];
                char child_dst[EX_MAX_PATH];
                snprintf(child_src, sizeof(child_src), "%s/%s", src, entries[i]->d_name);
                snprintf(child_dst, sizeof(child_dst), "%s/%s", dst, entries[i]->d_name);
                ex_worker_copy(child_src, child_dst, copied, 0);
                free(entries[i]);
            }
            free(entries);
        }
    } else {
        /* File copy */
        int fdi = ex_9p_open(src, O_RDONLY);
        int fdo = ex_9p_open(dst, O_WRONLY | O_CREAT | O_TRUNC);
        if (fdi >= 0 && fdo >= 0) {
            char buf[8192];
            ssize_t n;
            while ((n = ex_9p_read(fdi, buf, sizeof(buf))) > 0) {
                ex_9p_write(fdo, buf, n);
                if (copied) *copied += n;
            }
            ex_9p_close(fdi);
            ex_9p_close(fdo);
        } else {
            if (fdi >= 0) ex_9p_close(fdi);
            if (fdo >= 0) ex_9p_close(fdo);
        }
    }
}

static void ex_worker_move(const char *src, const char *dst) {
    /* Try rename first (same filesystem) */
    if (ex_9p_rename(src, dst) == 0) return;
    
    /* Cross-filesystem: copy then delete */
    uint64_t copied = 0;
    ex_worker_copy(src, dst, &copied, 0);
    ex_9p_unlink(src);
}

static void ex_worker_delete(const char *path, bool permanent) {
    (void)permanent;
    struct stat st;
    if (ex_9p_stat(path, &st) != 0) return;
    
    if (S_ISDIR(st.st_mode)) {
        /* Recursive directory delete */
        struct dirent **entries;
        int count = ex_9p_readdir(path, &entries);
        if (count >= 0) {
            for (int i = 0; i < count; i++) {
                if (strcmp(entries[i]->d_name, ".") == 0 || strcmp(entries[i]->d_name, "..") == 0) {
                    free(entries[i]);
                    continue;
                }
                char child[EX_MAX_PATH];
                snprintf(child, sizeof(child), "%s/%s", path, entries[i]->d_name);
                ex_worker_delete(child, permanent);
                free(entries[i]);
            }
            free(entries);
        }
        ex_9p_unlink(path);
    } else {
        ex_9p_unlink(path);
    }
}

static void ex_handle_file_op(ExExplorerState *ex) {
    /* Use worker functions with 9P/Styx */
    for (int i = 0; i < ex->file_op.count; i++) {
        if (ex->file_op.error) break;

        const char *src = ex->file_op.paths[i];
        char dst[EX_MAX_PATH];
        const char *base = strrchr(src, '/');
        base = base ? base + 1 : src;

        snprintf(dst, sizeof(dst), "%s/%s", ex->file_op.dest_path, base);

        if (ex->file_op.type == EX_OP_COPY) {
            uint64_t copied = 0;
            ex_worker_copy(src, dst, &copied, ex->file_op.total_bytes);
            ex->file_op.copied_bytes += copied;
            snprintf(ex->status_text, sizeof(ex->status_text),
                     "Copied: %s", base);
        } else if (ex->file_op.type == EX_OP_MOVE) {
            ex_worker_move(src, dst);
            snprintf(ex->status_text, sizeof(ex->status_text),
                     "Moved: %s", base);
        } else if (ex->file_op.type == EX_OP_DELETE) {
            ex_worker_delete(src, false);
            snprintf(ex->status_text, sizeof(ex->status_text),
                     "Deleted: %s", base);
        }
    }

    ex->file_op.in_progress = false;
    ex->file_op.current_idx = ex->file_op.count;

    /* Refresh view */
    dosgui_explorer_refresh();
}

/* -- Preview Update ----------------------------------------------- */

void dosgui_explorer_update_preview(int idx) {
    if (idx < 0 || idx >= g_explorer.entry_count) {
        g_explorer.preview.type = EX_PREVIEW_NONE;
        return;
    }

    ExEntry *entry = &g_explorer.entries[idx];
    if (entry->hidden && !g_explorer.show_hidden) {
        g_explorer.preview.type = EX_PREVIEW_NONE;
        return;
    }

    strncpy(g_explorer.preview.path, entry->full_path, EX_MAX_PATH - 1);
    g_explorer.preview.file_size = entry->size;
    g_explorer.preview.modified = entry->modified;
    strncpy(g_explorer.preview.mime_type, entry->type_str, 63);

    if (entry->type == EX_ENTRY_DIR) {
        g_explorer.preview.type = EX_PREVIEW_METADATA;
    } else {
        const char *ext = ex_get_extension(entry->name);
        if (strcmp(ext, "txt") == 0 || strcmp(ext, "md") == 0 ||
            strcmp(ext, "log") == 0 || strcmp(ext, "c") == 0 ||
            strcmp(ext, "h") == 0 || strcmp(ext, "cpp") == 0 ||
            strcmp(ext, "hpp") == 0 || strcmp(ext, "py") == 0 ||
            strcmp(ext, "js") == 0 || strcmp(ext, "sh") == 0 ||
            strcmp(ext, "rs") == 0 || strcmp(ext, "json") == 0 ||
            strcmp(ext, "xml") == 0 || strcmp(ext, "html") == 0 ||
            strcmp(ext, "css") == 0) {
            g_explorer.preview.type = EX_PREVIEW_TEXT;

            int fd = ex_9p_open(entry->full_path, O_RDONLY);
            if (fd >= 0) {
                ssize_t n = ex_9p_read(fd, g_explorer.preview.text_buffer, sizeof(g_explorer.preview.text_buffer) - 1);
                if (n > 0) {
                    g_explorer.preview.text_buffer[n] = '\0';
                    g_explorer.preview.text_lines = 0;
                    for (int i = 0; i < n; i++) {
                        if (g_explorer.preview.text_buffer[i] == '\n') g_explorer.preview.text_lines++;
                    }
                }
                ex_9p_close(fd);
            }
        } else if (strcmp(ext, "png") == 0 || strcmp(ext, "jpg") == 0 ||
                   strcmp(ext, "jpeg") == 0 || strcmp(ext, "gif") == 0 ||
                   strcmp(ext, "bmp") == 0 || strcmp(ext, "webp") == 0) {
            g_explorer.preview.type = EX_PREVIEW_IMAGE;
            g_explorer.preview.img_w = 0;
            g_explorer.preview.img_h = 0;
            /* Would use stb_image here */
        } else {
            g_explorer.preview.type = EX_PREVIEW_METADATA;
        }
    }
}
