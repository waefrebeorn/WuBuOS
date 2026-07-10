/* dosgui_explorer_tree.c -- Tree view subsystem + shared entry helpers.
 *
 * Owns directory-tree population/find/free and the pure entry helpers
 * (extension, case-insensitive compare, sort, file compare) used across the
 * explorer. Declared non-static in dosgui_explorer_internal.h so every
 * submodule links the SAME implementation (no duplicated copies). Shared state
 * via g_explorer (extern); backend via public ex_9p_*. Minimal includes.
 */

#include "dosgui_explorer_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *ex_get_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

int ex_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a - ((*b >= 'A' && *b <= 'Z') ? *b + 32 : *b);
}

void ex_sort_entries(ExExplorerState *ex) {
    g_sort_ctx = ex;
    qsort(ex->entries, ex->entry_count, sizeof(ExEntry), ex_file_compare);
    g_sort_ctx = NULL;
}

int ex_file_compare(const void *a, const void *b) {
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

void ex_populate_tree(ExTreeNode *node, const char *path) {
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

ExTreeNode *ex_tree_find(ExTreeNode *root, const char *path) {
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

void ex_tree_free(ExTreeNode *node) {
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
