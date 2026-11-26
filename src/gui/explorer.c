/*
 * explorer.c  --  WuBuOS File Manager (Win98/XP Explorer Shell)
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

#include "../egojar.h"
#include "../apps/guest_file.h"
#include "explorer.h"
#include "explorer_internal.h"
#include "wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <pwd.h>
#include <grp.h>
#include <limits.h>
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
 * Defined here; declared extern in explorer_internal.h for the input module. */
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

/* -- Lifecycle ---------------------------------------------------- */

int explorer_init(void) {
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

    /* Start at the GUEST ROOT ("/").
     *
     * This used to be getenv("HOME"), which on the hosted port is the real Linux
     * home directory -- so File Manager opened showing the host machine's files,
     * with no relation to anything WuBuOS itself had saved.  "/" now means the
     * guest sandbox (see styxfs_guest_root()), which is the only namespace apps
     * see. */
    strcpy(g_explorer.current_path, "/");
    /* Every explorer has at least one tab, like Windows 11: the strip is part of the
     * surface, not something the user has to create. */
    g_explorer.tab_count = 1;
    g_explorer.tab_active = 0;
    g_explorer.tabs[0].used = true;
    g_explorer.tabs[0].view_mode = g_explorer.view_mode;
    snprintf(g_explorer.tabs[0].path, sizeof(g_explorer.tabs[0].path), "/");

    ex_scan_directory(g_explorer.current_path);
    ex_populate_tree(g_explorer.tree_root, g_explorer.current_path);
    ex_update_breadcrumbs(&g_explorer);
    ex_add_to_history(&g_explorer, g_explorer.current_path);

    return 0;
}

void explorer_shutdown(void) {
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

/* -- Pick mode ---------------------------------------------------------------
 * Explorer IS the file picker. The first version of the picker grew its own file list, which is
 * a second copy of a component this OS already has: same directory walk, same selection, same
 * navigation, twice the bugs. Callers now put the REAL Explorer into pick mode and read the
 * selection back through a completion callback.
 */
typedef struct {
    int      active;
    int      save_mode;
    char     typed[256];
    int      typed_len;
    char     result[EX_MAX_PATH + 64];
    void   (*cb)(const char *path, void *user);
    void    *user;
} ExPickState;

static ExPickState g_pick;

/* What the user is pointing at: the selected entry's full path, or the directory being shown.
 * The Print dialog and anything else that acts on "the current file" asks this, so there is one
 * answer to that question in the whole shell. */
const char *explorer_selected_path(void) {
    if (g_explorer.entry_count > 0 && g_explorer.selected_indices[0] >= 0)
        return g_explorer.entries[g_explorer.selected_indices[0]].full_path;
    return g_explorer.current_path;
}

int explorer_pick_active(void) { return g_pick.active; }
int  explorer_pick_save_mode(void) { return g_pick.save_mode; }
const char *explorer_pick_typed(void) { return g_pick.typed; }

void explorer_pick_typed_char(char c) {
    if (!g_pick.active) return;
    if (g_pick.typed_len < (int)sizeof g_pick.typed - 1) {
        g_pick.typed[g_pick.typed_len++] = c;
        g_pick.typed[g_pick.typed_len] = '\0';
    }
}
void explorer_pick_typed_backspace(void) {
    if (g_pick.active && g_pick.typed_len > 0)
        g_pick.typed[--g_pick.typed_len] = '\0';
}

/* Accept the current selection (or the typed name) and report it. */
int explorer_pick_accept(void) {
    if (!g_pick.active) return 0;
    const char *base = explorer_current_path();
    if (g_pick.typed_len) {
        snprintf(g_pick.result, sizeof g_pick.result, "%s/%s", base, g_pick.typed);
    } else if (g_explorer.entry_count > 0 && g_explorer.selected_indices[0] >= 0) {
        ExEntry *e = &g_explorer.entries[g_explorer.selected_indices[0]];
        /* The guest entries carry a root-relative path, so joining it to "/" produced "//cons".
         * Normalise to exactly one leading slash: a caller must be able to open what we hand it. */
        const char *p = e->full_path;
        while (p[0] == '/' && p[1] == '/') p++;
        if (p[0] != '/')
            snprintf(g_pick.result, sizeof g_pick.result, "%s/%s", base, p);
        else
            snprintf(g_pick.result, sizeof g_pick.result, "%s", p);
        if (e->type == EX_ENTRY_DIR || e->type == EX_ENTRY_DRIVE) {
            explorer_navigate(g_pick.result);   /* a folder is navigation, not a result */
            return 0;
        }
    } else {
        snprintf(g_explorer.status_text, sizeof g_explorer.status_text, "Pick something first");
        return 0;
    }
    if (g_pick.cb) g_pick.cb(g_pick.result, g_pick.user);
    g_pick.active = 0;
    g_pick.typed_len = 0;
    g_pick.typed[0] = '\0';
    return 1;
}

void explorer_pick_cancel(void) {
    g_pick.active = 0;
    g_pick.typed_len = 0;
    g_pick.typed[0] = '\0';
}

int explorer_open_pick(const char *start_dir, int save_mode,
                       void (*cb)(const char *path, void *user), void *user) {
    /* explorer_init() is what builds the columns, the tree and the path history; without it the
     * window opens on uninitialised state and lists NOTHING. It was only ever called from the
     * app registry, so a caller that reached Explorer directly got an empty pane. */
    if (g_explorer.column_count == 0) explorer_init();
    explorer_show();
    if (!explorer_is_open()) return -1;
    g_pick.active = 1;
    g_pick.save_mode = save_mode ? 1 : 0;
    g_pick.cb = cb;
    g_pick.user = user;
    g_pick.typed_len = 0;
    g_pick.typed[0] = '\0';
    /* Explorer lists the GUEST filesystem (guest_file_children), not the host's POSIX tree, so a
     * host path like /home/wubu is simply not there. Fall back to the guest root instead of
     * opening a pane that can never fill. */
    {
        const char *dir = (start_dir && *start_dir) ? start_dir : NULL;
        if (!dir || !guest_file_is_dir(dir)) dir = "/";
        explorer_navigate(dir);
    }
    snprintf(g_explorer.status_text, sizeof g_explorer.status_text,
             save_mode ? "Save As: type a name, Enter to accept, Esc to cancel"
                       : "Open: select a file, Enter to open, Esc to cancel");
    return 0;
}

/* Adapters: the explorer engine's handlers take no window, the WM's callbacks do. */
static void ex_win_key(DosGuiWindow *w, uint32_t key, uint32_t mods) {
    (void)w; explorer_handle_key(key, mods);
}
static void ex_win_mouse(DosGuiWindow *w, int x, int y, int btn, int kind) {
    (void)w;
    /* The WM reports window-relative coordinates INCLUDING the caption; the explorer layout speaks
     * content-relative (its first row starts under the caption). Without this subtraction every
     * click landed one caption-height too high -- it selected the row above the one under the
     * pointer, which reads as "the file manager is inaccurate" rather than as an offset bug. */
    int oy = w->content_w > 0 ? (w->content_y - w->y) : 0;   /* content origin inside the window */
    explorer_handle_mouse(x, y - oy, btn, kind);
}

void explorer_show(void) {
    if (!explorer_is_open()) {
        DosGuiWindow *win = wm_create(100, 100, 900, 600, "File Manager");
        if (win) {
            /* The renderer and the input handlers live in the app binding (app_explorer_draw /
             * explorer_handle_*). This window used to leave all three NULL -- "custom rendering
             * in WM" -- so a window opened by anything other than the app registry drew an
             * EMPTY pane with no status bar and took no keys. Weak so a narrow link (the WM
             * tests, which have no app layer) still builds. */
            extern void app_explorer_draw(DosGuiWindow *, uint32_t *, int, int)
                __attribute__((weak));
            win->user_data = &g_explorer;
            win->on_draw = app_explorer_draw;
            win->on_key = ex_win_key;
            win->on_mouse = ex_win_mouse;
            g_explorer.win_id = win->id;
        }
    }
}

void explorer_hide(void) {
    if (explorer_is_open()) {
        DosGuiWindow *win = wm_find_by_id(g_explorer.win_id);
        if (win) {
            wm_destroy(win);
        }
        g_explorer.win_id = 0;
    }
    ex_hide_context_menu(&g_explorer);
}

bool explorer_is_open(void) {
    return g_explorer.win_id > 0 && wm_find_by_id(g_explorer.win_id) != NULL;
}

void explorer_toggle(void) {
    if (explorer_is_open()) {
        explorer_hide();
    } else {
        explorer_show();
    }
}

/* -- Navigation --------------------------------------------------- */

void explorer_navigate(const char *path) {
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

void explorer_go_up(void) {
    char parent[EX_MAX_PATH];
    ex_dirname_safe(g_explorer.current_path, parent, EX_MAX_PATH);
    if (strcmp(parent, g_explorer.current_path) != 0) {
        explorer_navigate(parent);
    } else if (strcmp(g_explorer.current_path, "/") != 0) {
        explorer_navigate("/");
    }
}

void explorer_go_back(void) {
    if (g_explorer.history_pos > 0) {
        g_explorer.history_pos--;
        strncpy(g_explorer.current_path, g_explorer.history[g_explorer.history_pos], EX_MAX_PATH - 1);
        explorer_navigate(g_explorer.current_path);
    }
}

void explorer_go_forward(void) {
    if (g_explorer.history_pos < 31 && g_explorer.history[g_explorer.history_pos + 1][0]) {
        g_explorer.history_pos++;
        strncpy(g_explorer.current_path, g_explorer.history[g_explorer.history_pos], EX_MAX_PATH - 1);
        explorer_navigate(g_explorer.current_path);
    }
}

void explorer_refresh(void) {
    explorer_navigate(g_explorer.current_path);
}

/* -- Tree View ---------------------------------------------------- */

void explorer_tree_expand(ExTreeNode *node) {
    if (!node || node->expanded) return;
    node->expanded = true;
    if (!node->scanned) {
        ex_tree_scan(node);
    }
}

void explorer_tree_collapse(ExTreeNode *node) {
    if (!node || !node->expanded) return;
    node->expanded = false;
}

void explorer_tree_select(ExTreeNode *node) {
    if (!node) return;
    g_explorer.tree_selected = node;
    explorer_navigate(node->path);
}

ExTreeNode *explorer_tree_find(const char *path) {
    return g_explorer.tree_root ? ex_tree_find(g_explorer.tree_root, path) : NULL;
}

void explorer_tree_scan(ExTreeNode *node) {
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

void explorer_select_all(void) {
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

void explorer_clear_selection(void) {
    for (int i = 0; i < g_explorer.selection_count; i++) {
        int idx = g_explorer.selected_indices[i];
        if (idx >= 0 && idx < g_explorer.entry_count) {
            g_explorer.entries[idx].is_selected = false;
        }
    }
    g_explorer.selection_count = 0;
    g_explorer.anchor_idx = -1;
}

void explorer_toggle_selection(int idx) {
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

void explorer_select_range(int start, int end) {
    if (start > end) { int t = start; start = end; end = t; }
    for (int i = start; i <= end && i < g_explorer.entry_count; i++) {
        if (!g_explorer.entries[i].hidden || g_explorer.show_hidden) {
            explorer_toggle_selection(i);
        }
    }
}

bool explorer_is_selected(int idx) {
    if (idx < 0 || idx >= g_explorer.entry_count) return false;
    return g_explorer.entries[idx].is_selected;
}

/* -- View --------------------------------------------------------- */

void explorer_set_view_mode(ExViewMode mode) {
    if (mode >= 0 && mode < 4) {
        g_explorer.view_mode = mode;
    }
}

void explorer_set_sort(ExSortColumn col, bool ascending) {
    if (col >= 0 && col < EX_SORT_COUNT) {
        g_explorer.sort_column = col;
        g_explorer.sort_ascending = ascending;
        ex_sort_entries(&g_explorer);
    }
}

void explorer_toggle_hidden(void) {
    g_explorer.show_hidden = !g_explorer.show_hidden;
    ex_scan_directory(g_explorer.current_path);
}

void explorer_toggle_extensions(void) {
    g_explorer.show_extensions = !g_explorer.show_extensions;
}

void explorer_toggle_preview(void) {
    g_explorer.preview_visible = !g_explorer.preview_visible;
}

void explorer_toggle_toolbar(void) {
    g_explorer.toolbar_visible = !g_explorer.toolbar_visible;
}

/* -- File Operations ---------------------------------------------- */


/* -- Properties --------------------------------------------------- */

void explorer_show_properties(int idx) {
    if (idx < 0 || idx >= g_explorer.entry_count) return;
    ExEntry *entry = &g_explorer.entries[idx];
    if (entry->hidden && !g_explorer.show_hidden) return;

    char msg[512];
    explorer_format_size(entry->size, msg, sizeof(msg));
    snprintf(g_explorer.status_text, sizeof(g_explorer.status_text),
             "Properties: %s (%s)", entry->name, msg);
    /* In real implementation: show modal dialog */
}

/* Input handling moved to explorer_input.c
 * (explorer_handle_key / explorer_handle_mouse). */

/* -- State Accessors ---------------------------------------------- */

/* -- Tabs -------------------------------------------------------------- */

int ex_tab_h(void) { return EX_TAB_H; }

int ex_content_top(void) {
    return ex_breadcrumb_h() + (g_explorer.toolbar_visible ? ex_toolbar_h() : 0) + ex_tab_h();
}

bool explorer_tab_rect(int i, int fb_w, int *x, int *w) {
    int n = g_explorer.tab_count > 0 ? g_explorer.tab_count : 1;
    if (i < 0 || i >= n) return false;
    int tw = fb_w / n;
    if (tw > 220) tw = 220;
    if (tw < 60)  tw = 60;
    int x0 = 0;
    /* Centre the group when the tabs do not fill the width, like the Win11 strip. */
    if (tw * n < fb_w) x0 = (fb_w - tw * n) / 2;
    if (x) *x = x0 + i * tw;
    if (w) *w = tw;
    return true;
}

int explorer_tab_count(void) { return g_explorer.tab_count; }
int explorer_tab_active(void) { return g_explorer.tab_active; }

const char *explorer_tab_path(int i) {
    if (i < 0 || i >= g_explorer.tab_count) return NULL;
    return g_explorer.tabs[i].path;
}

/* The active tab IS the live state; copy it in before switching away or closing. */
static void ex_tab_save_active(void) {
    int i = g_explorer.tab_active;
    if (i < 0 || i >= g_explorer.tab_count) return;
    ExTab *t = &g_explorer.tabs[i];
    snprintf(t->path, sizeof(t->path), "%s", g_explorer.current_path);
    t->view_mode = g_explorer.view_mode;
    t->sort_column = g_explorer.sort_column;
    t->sort_ascending = g_explorer.sort_ascending;
    t->used = true;
}

int explorer_tab_open(const char *path) {
    if (g_explorer.tab_count >= EX_MAX_TABS) return -1;
    if (g_explorer.tab_count == 0) {
        /* First tab: adopt whatever the explorer is already showing. */
        g_explorer.tab_active = 0;
        g_explorer.tab_count = 1;
        g_explorer.tabs[0].used = true;
        snprintf(g_explorer.tabs[0].path, sizeof(g_explorer.tabs[0].path), "%s",
                 g_explorer.current_path[0] ? g_explorer.current_path : (path ? path : "/"));
    } else {
        ex_tab_save_active();
    }
    int idx = g_explorer.tab_count;
    ExTab *t = &g_explorer.tabs[idx];
    memset(t, 0, sizeof(*t));
    t->used = true;
    snprintf(t->path, sizeof(t->path), "%s", path && path[0] ? path : g_explorer.current_path);
    t->view_mode = g_explorer.view_mode;
    t->sort_column = g_explorer.sort_column;
    t->sort_ascending = g_explorer.sort_ascending;
    g_explorer.tab_count++;
    g_explorer.tab_active = idx;
    explorer_navigate(t->path);
    return idx;
}

bool explorer_tab_switch(int i) {
    if (i < 0 || i >= g_explorer.tab_count || i == g_explorer.tab_active) return false;
    ex_tab_save_active();
    g_explorer.tab_active = i;
    ExTab *t = &g_explorer.tabs[i];
    g_explorer.view_mode = t->view_mode;
    g_explorer.sort_column = t->sort_column;
    g_explorer.sort_ascending = t->sort_ascending;
    explorer_navigate(t->path);
    return true;
}

/* Closing the last tab does not leave an explorer with no view: it keeps the tab and just
 * resets it, which is what Windows does when you close the only tab (the window closes; here
 * the panel stays and the tab remains, because the panel is the product surface). */
bool explorer_tab_close(int i) {
    if (i < 0 || i >= g_explorer.tab_count) return false;
    if (g_explorer.tab_count == 1) return false;
    for (int k = i; k < g_explorer.tab_count - 1; k++) g_explorer.tabs[k] = g_explorer.tabs[k + 1];
    g_explorer.tab_count--;
    if (g_explorer.tab_active >= g_explorer.tab_count)
        g_explorer.tab_active = g_explorer.tab_count - 1;
    ExTab *t = &g_explorer.tabs[g_explorer.tab_active];
    g_explorer.view_mode = t->view_mode;
    explorer_navigate(t->path);
    return true;
}

ExExplorerState *explorer_state(void) {
    return &g_explorer;
}

const char *explorer_current_path(void) {
    return g_explorer.current_path;
}

/* -- Drive/Volume Enumeration ------------------------------------- */

/* -- Internal Implementation -------------------------------------- */

typedef struct {
    ExExplorerState *ex;
} ScanCtx;

static bool scan_one(const char *name, bool is_dir, uint32_t size, void *user) {
    ScanCtx *ctx = (ScanCtx *)user;
    ExExplorerState *ex = ctx->ex;

    if (!ex->show_hidden && name[0] == '.') return true;

    ExEntry *entry = &ex->entries[ex->entry_count];
    {   /* Guest paths arrive as "/" at the root, so "%s/%s" produced "//cons". Cosmetic to look
         * at, but the result of a pick is handed to openers as a path. */
        size_t pl = strlen(ex->current_path);
        int need_sep = !(pl > 0 && ex->current_path[pl - 1] == '/');
        snprintf(entry->full_path, sizeof(entry->full_path), "%s%s%s",
                 ex->current_path, need_sep ? "/" : "", name);
    }
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->type = is_dir ? EX_ENTRY_DIR : EX_ENTRY_FILE;
    entry->size = is_dir ? 0 : (uint64_t)size;

    ex_get_file_info(entry->full_path, entry);

    ex->status_file_count++;
    ex->status_total_size += entry->size;
    if (ex->entry_count + 1 < EX_MAX_ENTRIES) ex->entry_count++;
    return true;
}

void ex_scan_directory(const char *path) {
    ExExplorerState *ex = &g_explorer;
    ex->entry_count = 0;
    ex->status_file_count = 0;
    ex->status_total_size = 0;

    if (!guest_file_is_dir(path)) {
        snprintf(ex->status_text, sizeof(ex->status_text),
                 "Cannot open: %s", path);
        return;
    }

    ScanCtx ctx = { ex };
    int n = guest_file_children(path, scan_one, &ctx);
    if (n < 0) {
        snprintf(ex->status_text, sizeof(ex->status_text),
                 "Cannot open: %s", path);
        return;
    }

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

void explorer_update_drive_list(void) {
    /* Refresh tree root with new drive list */
    if (g_explorer.tree_root) {
        ex_tree_free(g_explorer.tree_root);
        g_explorer.tree_root = NULL;
    }
    ex_populate_tree(g_explorer.tree_root, g_explorer.current_path);
}

