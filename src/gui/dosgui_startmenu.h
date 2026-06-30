/*
 * dosgui_startmenu.h  --  WuBuOS DosGui Start Menu
 *
 * Cell 402: Cascading start menu with program groups.
 * Win98-style: Start button → Programs → Accessories → etc.
 */

#ifndef WUBU_DOSGUI_STARTMENU_H
#define WUBU_DOSGUI_STARTMENU_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* -- Menu Entry Types ------------------------------------------- */

typedef enum {
    SM_ITEM_APP = 0,      /* Launches an app */
    SM_ITEM_SEPARATOR,    /* Visual separator line */
    SM_ITEM_SUBMENU,      /* Opens a submenu */
} SmItemType;

typedef struct {
    SmItemType type;
    char      label[48];
    char      app_name[48];   /* For SM_ITEM_APP: app to launch */
    int       submenu_id;     /* For SM_ITEM_SUBMENU: index into submenus */
} SmMenuItem;

typedef struct {
    char       name[32];
    SmMenuItem items[12];
    int        item_count;
} SmSubmenu;

/* Main menu items */
typedef struct {
    char label[48];
    int  type; /* 0=app, 1=submenu, 2=separator, 3=shutdown */
    int  submenu_id;
    char app_name[48];
} MainMenuItem;

/* Global state externs */
extern SmSubmenu g_programs;
extern SmSubmenu g_accessories;
extern SmSubmenu g_wubuos;
extern SmSubmenu g_system;

extern MainMenuItem g_main_items[16];
extern int g_main_count;

extern int g_open;
extern int g_hovered;
extern int g_submenu_open;
extern int g_sub_hovered;
extern int g_hover_submenu;

/* -- Lifecycle ---------------------------------------------------- */

void dosgui_startmenu_init(void);
void dosgui_startmenu_shutdown(void);

/* -- State ------------------------------------------------------- */

int  dosgui_startmenu_is_open(void);
void dosgui_startmenu_toggle(void);
void dosgui_startmenu_open(void);
void dosgui_startmenu_close(void);

/* -- Rendering --------------------------------------------------- */

void dosgui_startmenu_render(uint32_t *fb, int fb_w, int fb_h);

/* -- Input ------------------------------------------------------- */

void dosgui_startmenu_handle_click(int x, int y);

/* Hover tracking for submenu expansion */
void dosgui_startmenu_track_hover(int x, int y);

/* ==================================================================
 * Start Menu Enhancements -- Cell 402+
 * Search, Recently Used, Power Options, All Programs Tree
 * ================================================================== */

#define DOSGUI_MAX_PROGRAM_ENTRIES 64
#define DOSGUI_MAX_RECENT 10
#define DOSGUI_SEARCH_MAX_LEN 128

typedef enum {
    SM_CAT_ALL_PROGRAMS = 0,
    SM_CAT_RECENT = 1,
    SM_CAT_SEARCH_RESULTS = 2,
} SmCategory;

typedef struct {
    char name[48];
    char executable[128];
    char category[32];
    uint64_t last_run;
    int run_count;
    bool is_builtin;
} SmProgramEntry;

typedef struct {
    SmProgramEntry entries[DOSGUI_MAX_PROGRAM_ENTRIES];
    int count;
} SmProgramDB;

typedef struct {
    char query[DOSGUI_SEARCH_MAX_LEN];
    int cursor_pos;
    bool active;
    SmProgramEntry *results[DOSGUI_MAX_PROGRAM_ENTRIES];
    int result_count;
} SmSearchState;

void dosgui_startmenu_search_init(void);
void dosgui_startmenu_search_update(const char *query);
void dosgui_startmenu_search_clear(void);
int  dosgui_startmenu_search_get_results(SmProgramEntry ***out_results, int *out_count);

void dosgui_startmenu_recent_add(const char *app_name);
int  dosgui_startmenu_recent_get(SmProgramEntry **out_entries, int max);

typedef struct SmTreeNode {
    char label[48];
    int type;
    struct SmTreeNode *children;
    int child_count;
    int depth;
    bool expanded;
    SmProgramEntry *program;
} SmTreeNode;

void dosgui_startmenu_tree_build(void);
SmTreeNode *dosgui_startmenu_tree_get_root(void);
void dosgui_startmenu_tree_toggle(SmTreeNode *node);
void dosgui_startmenu_tree_render(uint32_t *fb, int fb_w, int fb_h, int x, int y);

typedef enum {
    PWR_SHUTDOWN = 0,
    PWR_RESTART = 1,
    PWR_LOGOFF = 2,
    PWR_SLEEP = 3,
    PWR_HIBERNATE = 4,
} PowerAction;

void dosgui_startmenu_power(PowerAction action);

void dosgui_startmenu_render_search_bar(uint32_t *fb, int fb_w, int fb_h, int menu_x, int menu_y, int w);
void dosgui_startmenu_render_recent(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int max_items);
void dosgui_startmenu_render_all_programs(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h);
void dosgui_startmenu_render_power_options(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w);

void dosgui_startmenu_handle_key(int key, uint32_t mods);
void dosgui_startmenu_handle_search_input(int key, uint32_t mods);

void dosgui_startmenu_build_programs_db(void);
void dosgui_startmenu_init_enhanced(void);

#endif /* WUBU_DOSGUI_STARTMENU_H */
