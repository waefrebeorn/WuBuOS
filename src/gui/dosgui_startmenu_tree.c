/* dosgui_startmenu_tree.c -- Start menu program-tree (All Programs).
 *
 * Self-contained subsystem extracted from dosgui_startmenu.c. Operates on the
 * shared g_tree_* state (extern in dosgui_startmenu_internal.h). Minimal includes.
 */

#include "wubu_theme.h"
#include "dosgui_startmenu_internal.h"
#include <stdlib.h>
#include <string.h>

SmTreeNode *tree_node_create(const char *label, int type) {
    if (g_tree_node_count >= 32) return NULL;
    SmTreeNode *n = &g_tree_nodes[g_tree_node_count++];
    memset(n, 0, sizeof(SmTreeNode));
    strncpy(n->label, label, sizeof(n->label) - 1);
    n->type = type;
    n->expanded = (type == 0);
    return n;
}

void tree_add_child(SmTreeNode *parent, SmTreeNode *child) {
    if (parent->child_count < 8) {
        if (!parent->children) parent->children = child;
        parent->child_count++;
    }
}

void dosgui_startmenu_tree_build(void) {
    g_tree_node_count = 0;
    memset(&g_tree_root, 0, sizeof(g_tree_root));
    strncpy(g_tree_root.label, "All Programs", sizeof(g_tree_root.label) - 1);
    g_tree_root.type = 0;
    g_tree_root.expanded = true;
    g_tree_root.depth = 0;
    
    for (int i = 0; i < g_program_db.count; i++) {
        SmProgramEntry *e = &g_program_db.entries[i];
        
        SmTreeNode *cat = NULL;
        for (int j = 0; j < g_tree_root.child_count; j++) {
            if (strcmp(g_tree_root.children[j].label, e->category) == 0) {
                cat = &g_tree_root.children[j];
                break;
            }
        }
        if (!cat) {
            cat = tree_node_create(e->category, 0);
            if (cat && cat != &g_tree_root) {
                if (!g_tree_root.children) g_tree_root.children = &g_tree_nodes[1];
                g_tree_root.child_count++;
            }
        }
        
        if (cat) {
            SmTreeNode *prog = tree_node_create(e->name, 1);
            if (prog) {
                prog->program = e;
                prog->depth = cat->depth + 1;
                if (!cat->children) cat->children = prog;
                cat->child_count++;
            }
        }
    }
}

SmTreeNode *dosgui_startmenu_tree_get_root(void) {
    return &g_tree_root;
}

void dosgui_startmenu_tree_toggle(SmTreeNode *node) {
    if (node && node->type == 0) {
        node->expanded = !node->expanded;
    }
}

void dosgui_startmenu_tree_render(uint32_t *fb, int fb_w, int fb_h, int x, int y) {
    (void)fb; (void)fb_w; (void)fb_h;
    
    const SmTreeNode *node = &g_tree_root;
    int item_h = 22;
    int indent = 20;
    int ty = y;
    
    if (!node->children) return;
    
    for (int i = 0; i < node->child_count; i++) {
        SmTreeNode *cat = &node->children[i];
        if (cat->type != 0) continue;
        
        vbe_draw_text(x + indent, ty, cat->label, wubu_theme_colors()->win_title_text, 1);
        ty += item_h;
        
        if (cat->expanded && cat->children) {
            for (int j = 0; j < cat->child_count; j++) {
                SmTreeNode *prog = &cat->children[j];
                if (prog->type == 1 && prog->program) {
                    vbe_draw_text(x + indent * 2, ty, prog->program->name, wubu_theme_colors()->startmenu_text, 1);
                    ty += item_h;
                }
            }
        }
        ty += 4;
    }
}
