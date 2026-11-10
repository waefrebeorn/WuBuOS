/* startmenu_internal.h -- Start menu internal header.
 * Shared globals + search/tree sub-module API. Public API + types in
 * ./startmenu.h. Globals are defined once in startmenu.c.
 */

#ifndef DOSGUI_STARTMENU_INTERNAL_H
#define DOSGUI_STARTMENU_INTERNAL_H

#include "./startmenu.h"
#include "../wubu_theme.h"

/* -- Shared state (defined in startmenu.c) ------------------ */
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

/* -- Search subsystem (startmenu_search.c) ------------------ */
void startmenu_search_init(void);
void startmenu_search_update(const char *query);
void startmenu_search_clear(void);
int  startmenu_search_get_results(SmProgramEntry ***out_results, int *out_count);
void startmenu_recent_add(const char *app_name);
int  startmenu_recent_get(SmProgramEntry **out_entries, int max);

/* -- Tree subsystem (startmenu_tree.c) ---------------------- */
SmTreeNode *tree_node_create(const char *label, int type);
void tree_add_child(SmTreeNode *parent, SmTreeNode *child);
void startmenu_tree_build(void);
SmTreeNode *startmenu_tree_get_root(void);
void startmenu_tree_toggle(SmTreeNode *node);
void startmenu_tree_render(uint32_t *fb, int fb_w, int fb_h, int x, int y);

/* -- Theme helpers (static inline — shared by all startmenu submodules) -- */
static inline const WubuThemeColors *tc(void) { return wubu_theme_colors(); }
static inline const WubuTheme *th(void) { return wubu_theme_get(); }

/* -- Safe string macros (shared by all startmenu submodules) ------ */
#ifndef WUBU_SAFE_STRING
#define WUBU_SAFE_STRING
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
#endif

/* -- Menu DB subsystem (startmenu_db.c) ------------------- */
/* Builds the static main-menu items + category submenus from the
 * MIME .desktop registry and the built-in app catalog. */
void startmenu_build_main_menu(void);
void startmenu_build_submenus(void);

#endif /* DOSGUI_STARTMENU_INTERNAL_H */
