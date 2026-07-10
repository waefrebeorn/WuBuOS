/* dosgui_startmenu_internal.h -- Start menu internal header.
 * Shared globals + search/tree sub-module API. Public API + types in
 * dosgui_startmenu.h. Globals are defined once in dosgui_startmenu.c.
 */

#ifndef DOSGUI_STARTMENU_INTERNAL_H
#define DOSGUI_STARTMENU_INTERNAL_H

#include "dosgui_startmenu.h"

/* -- Shared state (defined in dosgui_startmenu.c) ------------------ */
extern int g_open;
extern int g_submenu_open;
extern SmProgramDB g_program_db;
extern SmSearchState g_search;
extern SmProgramEntry g_recent[DOSGUI_MAX_RECENT];
extern int g_recent_count;
extern SmTreeNode g_tree_nodes[32];
extern SmTreeNode g_tree_root;
extern int g_tree_node_count;
extern bool g_search_mode;

/* -- Search subsystem (dosgui_startmenu_search.c) ------------------ */
void dosgui_startmenu_search_init(void);
void dosgui_startmenu_search_update(const char *query);
void dosgui_startmenu_search_clear(void);
int  dosgui_startmenu_search_get_results(SmProgramEntry ***out_results, int *out_count);
void dosgui_startmenu_recent_add(const char *app_name);
int  dosgui_startmenu_recent_get(SmProgramEntry **out_entries, int max);

/* -- Tree subsystem (dosgui_startmenu_tree.c) ---------------------- */
SmTreeNode *tree_node_create(const char *label, int type);
void tree_add_child(SmTreeNode *parent, SmTreeNode *child);
void dosgui_startmenu_tree_build(void);
SmTreeNode *dosgui_startmenu_tree_get_root(void);
void dosgui_startmenu_tree_toggle(SmTreeNode *node);
void dosgui_startmenu_tree_render(uint32_t *fb, int fb_w, int fb_h, int x, int y);

#endif /* DOSGUI_STARTMENU_INTERNAL_H */
