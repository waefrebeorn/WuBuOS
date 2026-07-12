/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "dosgui_explorer.h"
#include "dosgui_explorer_internal.h"
#include "wubu_theme.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

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
