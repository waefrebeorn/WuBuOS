/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "../egojar.h"
#include "explorer.h"
#include "explorer_internal.h"
#include "wubu_theme.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* Context-menu geometry lives in explorer_internal.h so renderer, hit-test and
 * tests share one definition -- it used to sit here and drift from the renderer. */
/* Map a click position to a MENU ROW index (0..8), or -1 when the press landed on
 * nothing actionable -- outside the menu entirely, or on the separator row.
 *
 * Row layout MUST mirror ex_render_context_menu:
 *     row 0..4  -> Open, Open With..., Cut, Copy, Paste
 *     row 5     -> separator
 *     row 6..8  -> Delete, Rename, Properties
 * The renderer INSERTS the separator rather than overwriting an item, so Delete is
 * visible and every label has its own row. */
/* Display ROW occupied by menu item `item` (0..7).
 *
 * The separator sits BEFORE Delete (item 5), so every item from Delete downward is one
 * row lower than its array index. This is the ONLY place that shift is expressed: the
 * renderer asks it where to draw, and the hit-test answers with the inverse, so a menu
 * cannot become unclickable because two files disagreed about the offset. */
int ex_ctx_item_row(int item)
{
    if (item < 0 || item >= EX_CTX_ITEMS) return -1;
    /* The separator TAKES row 5, so items from Delete onward shift down one.
     * Using > here instead of >= would put Delete back on top of the separator. */
    return item + (item >= EX_CTX_SEPARATOR_ROW ? 1 : 0);
}

/* Inverse of ex_ctx_item_row(): which item lives at display row `row`, or -1 when the
 * row is the separator or outside the menu. */
int ex_ctx_item_at_row(int row)
{
    if (row < 0 || row >= EX_CTX_ROWS) return -1;
    if (row == EX_CTX_SEPARATOR_ROW) return -1;
    int item = row - (row > EX_CTX_SEPARATOR_ROW ? 1 : 0);
    return (item >= 0 && item < EX_CTX_ITEMS) ? item : -1;
}

int ex_ctx_row_at(int mx, int my, int menu_x, int menu_y)
{
    if (menu_x < 0) return -1;
    if (mx < menu_x || mx >= menu_x + EX_CTX_MENU_W) return -1;
    int rel = my - menu_y - 2;
    if (rel < 0) return -1;
    int row = rel / EX_CTX_ITEM_H;
    return (row >= EX_CTX_ROWS) ? -1 : row;
}

/* Open the selected entry: directories navigate, files go to their handler.
 *
 * This did not exist before -- selecting a file left the user with no way to act on it,
 * because nothing in the explorer opened anything. */
void explorer_open_entry(ExExplorerState *ex, int idx)
{
    if (!ex || idx < 0 || idx >= ex->entry_count) return;

    const ExEntry *e = &ex->entries[idx];

    if (e->type == EX_ENTRY_DIR || e->type == EX_ENTRY_ZIP ||
        e->type == EX_ENTRY_DRIVE || e->type == EX_ENTRY_MOUNT) {
        explorer_navigate(e->full_path);
        return;
    }

    /* Files need a handler. The one we have is W-CODE (the IDE owns source and .wet
     * applet files); everything else is honestly left alone rather than opened with a
     * guess that would silently mislead the user. */
    if (ex_launch_with_wcode &&
        (strcmp(e->extension, "wet") == 0 ||
         strcmp(e->extension, "c")   == 0 ||
         strcmp(e->extension, "h")   == 0 ||
         strcmp(e->extension, "hc")  == 0)) {
        ex_launch_with_wcode(e->full_path);
    }
}

void explorer_copy(void) {
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

void explorer_cut(void) {
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

void explorer_paste(void) {
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

void explorer_delete(bool permanent) {
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
    explorer_clear_selection();
}

void explorer_rename(int idx) {
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
        explorer_refresh();
        WUBU_SNPRINTF(g_explorer.status_text, sizeof(g_explorer.status_text),
                 "Renamed: %s", old_name);
    } else {
        WUBU_SNPRINTF(g_explorer.status_text, sizeof(g_explorer.status_text),
                 "Rename failed: %s", strerror(errno));
    }
}

void explorer_new_folder(void) {
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

    explorer_refresh();
    WUBU_SNPRINTF(g_explorer.status_text, sizeof(g_explorer.status_text),
             "Created folder: %s", name);
}

void explorer_new_file(const char *template_name) {
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
            explorer_refresh();
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
