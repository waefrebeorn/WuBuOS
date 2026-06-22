/*
 * dosgui_startmenu.c  --  WuBuOS DosGui Start Menu Implementation
 *
 * Cell 402: THEMED Cascading Start Menu.
 * Win98-style: Start -> Programs -> {Accessories, WuBuOS, System}
 *              Accessories -> {Notepad, Paint, Calculator, Terminal}
 *              WuBuOS -> {Temple REPL, Package Manager, Container Manager}
 *              System -> {File Manager, Settings}
 *         -> Documents, Find, Help, Run, Shutdown
 * XP-style: Luna Start orb, sidebar, hover highlighting, rounded items
 */

#include "dosgui_startmenu.h"
#include "dosgui_desktop.h"
#include "dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

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

/* -- Submenus --------------------------------------------------- */

static SmSubmenu g_programs = {0};
static SmSubmenu g_accessories = {0};
static SmSubmenu g_wubuos = {0};
static SmSubmenu g_system = {0};

/* -- Menu State ------------------------------------------------- */

static int  g_open = 0;
static int  g_hovered = -1;       /* Index in main menu */
static int  g_submenu_open = -1;  /* Which submenu is expanded */
static int  g_sub_hovered = -1;   /* Hovered item in submenu */
static int  g_hover_submenu = -1; /* Which submenu mouse is over */

/* Main menu items */
typedef struct {
    char label[48];
    int  type; /* 0=app, 1=submenu, 2=separator, 3=shutdown */
    int  submenu_id;
    char app_name[48];
} MainMenuItem;

static MainMenuItem g_main_items[16];
static int g_main_count = 0;

/* -- Theme Helpers ---------------------------------------------- */

static const WubuThemeColors *tc(void) { return wubu_theme_colors(); }
static const WubuTheme *th(void) { return wubu_theme_get(); }

/* -- Layout Helpers ------------------------------------------- */

static inline int menu_item_h(void) { return th()->Luna_start_button ? 24 : 22; }
static inline int menu_w(void) { return th()->Luna_start_button ? 200 : 180; }
static inline int submenu_w(void) { return th()->Luna_start_button ? 180 : 160; }
static inline int menu_border(void) { return th()->Luna_start_button ? 2 : 2; }
static inline int sidebar_w(void) { return th()->Luna_start_button ? 48 : 0; }

/* -- Init ------------------------------------------------------- */

static void build_accessories(void) {
    g_accessories.item_count = 0;
    g_accessories.items[g_accessories.item_count++] = (SmMenuItem){
        SM_ITEM_APP, "Notepad", "Notepad", 0};
    g_accessories.items[g_accessories.item_count++] = (SmMenuItem){
        SM_ITEM_APP, "Paint", "Paint", 0};
    g_accessories.items[g_accessories.item_count++] = (SmMenuItem){
        SM_ITEM_APP, "Editor", "Editor", 0};
    g_accessories.items[g_accessories.item_count++] = (SmMenuItem){
        SM_ITEM_APP, "WuBu Canvas", "WuBu Canvas", 0};
    g_accessories.items[g_accessories.item_count++] = (SmMenuItem){
        SM_ITEM_APP, "FreeDoom", "FreeDoom", 0};
}

static void build_wubuos(void) {
    g_wubuos.item_count = 0;
    g_wubuos.items[g_wubuos.item_count++] = (SmMenuItem){
        SM_ITEM_APP, "HolyC Terminal", "HolyC Terminal", 0};
    g_wubuos.items[g_wubuos.item_count++] = (SmMenuItem){
        SM_ITEM_APP, "Temple REPL", "Temple REPL", 0};
    g_wubuos.items[g_wubuos.item_count++] = (SmMenuItem){
        SM_ITEM_APP, "Package Manager", "Package Manager", 0};
    g_wubuos.items[g_wubuos.item_count++] = (SmMenuItem){
        SM_ITEM_APP, "Container Manager", "Container Manager", 0};
}

static void build_system(void) {
    g_system.item_count = 0;
    g_system.items[g_system.item_count++] = (SmMenuItem){
        SM_ITEM_APP, "File Manager", "File Manager", 0};
    g_system.items[g_system.item_count++] = (SmMenuItem){
        SM_ITEM_APP, "Settings", "Settings", 0};
}

static void build_main_menu(void) {
    g_main_count = 0;

    /* Programs (submenu) */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Programs", 1, 0, ""};

    /* Documents */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Documents", 0, -1, "File Manager"};

    /* Settings */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Settings", 0, -1, "Settings"};

    /* Separator */
    g_main_items[g_main_count++] = (MainMenuItem){
        "", 2, -1, ""};

    /* Find */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Find", 0, -1, "Find"};

    /* Help */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Help", 0, -1, "Help"};

    /* Run */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Run...", 0, -1, "Run"};

    /* Separator */
    g_main_items[g_main_count++] = (MainMenuItem){
        "", 2, -1, ""};

    /* Shutdown */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Shut Down", 3, -1, "Shutdown"};
}

void dosgui_startmenu_init(void) {
    build_accessories();
    build_wubuos();
    build_system();
    build_main_menu();
    g_open = 0;
    g_hovered = -1;
    g_submenu_open = -1;
    g_sub_hovered = -1;
    g_hover_submenu = -1;
    
    dosgui_startmenu_init_enhanced();
}

void dosgui_startmenu_shutdown(void) {
    g_open = 0;
}

/* -- State ------------------------------------------------------- */

int  dosgui_startmenu_is_open(void) { return g_open; }
void dosgui_startmenu_toggle(void) { g_open = !g_open; }
void dosgui_startmenu_open(void)   { g_open = 1; }
void dosgui_startmenu_close(void)  { g_open = 0; g_submenu_open = -1; }

/* -- Rendering --------------------------------------------------- */

#define MENU_X 4

static void draw_menu_item(int x, int y, int w, const char *label,
                            int hovered, int has_submenu, int type) {
    int mh = menu_item_h();
    uint32_t bg, fg;
    
    if (hovered) {
        bg = th()->Luna_start_button ? tc()->select_bg : 0x000080;
        fg = th()->Luna_start_button ? tc()->select_text : 0xFFFFFF;
    } else {
        bg = th()->Luna_start_button ? tc()->startmenu_bg : 0xC0C0C0;
        fg = th()->Luna_start_button ? tc()->startmenu_text : 0x000000;
    }
    
    if (type == 3) { /* Shutdown */
        bg = th()->Luna_start_button ? tc()->border_darkest : 0x800000;
        fg = th()->Luna_start_button ? 0xFF8080 : 0xFFFFFF;
    }

    if (th()->rounded_buttons) {
        vbe_fill_rect_rounded(x, y, w, mh, 2, bg);
        if (hovered) vbe_3d_raised_rounded_colors(x, y, w, mh, 2,
            tc()->border_light, tc()->border_face, tc()->border_dark, tc()->border_darkest);
    } else {
        vbe_fill_rect(x, y, w, mh, bg);
        if (hovered) vbe_3d_raised_colors(x, y, w, mh,
                                           tc()->border_light, tc()->border_face,
                                           tc()->border_dark, tc()->border_darkest);
    }

    vbe_draw_text(x + 6, y + (mh - 8) / 2, label, fg, 1);

    if (has_submenu) {
        /* Arrow indicator */
        int arrow_x = x + w - 16;
        int arrow_y = y + (mh - 8) / 2;
        vbe_draw_text(arrow_x, arrow_y, ">", fg, 1);
    }
}

static void draw_submenu(int x, int y, SmSubmenu *sub) {
    int mh = menu_item_h();
    int sw = submenu_w();
    int h = sub->item_count * mh + 4;
    int rad = th()->rounded_buttons ? 4 : 0;

    /* Submenu background with shadow */
    vbe_shade_rect(x + 2, y + 2, sw, h);
    
    if (rad > 0) {
        vbe_fill_rect_rounded(x, y, sw, h, rad, tc()->startmenu_bg);
        vbe_rect_rounded(x, y, sw, h, rad, tc()->border_dark);
    } else {
        vbe_fill_rect(x, y, sw, h, tc()->startmenu_bg);
        vbe_3d_raised_colors(x, y, sw, h,
                              tc()->border_light, tc()->border_face,
                              tc()->border_dark, tc()->border_darkest);
    }

    for (int i = 0; i < sub->item_count; i++) {
        SmMenuItem *item = &sub->items[i];
        int iy = y + 2 + i * mh;
        if (item->type == SM_ITEM_SEPARATOR) {
            int sep_y = iy + mh / 2;
            if (rad > 0) {
                vbe_hline(x + 8, x + sw - 8, sep_y, tc()->border_dark);
            } else {
                vbe_hline(x + 4, x + sw - 4, sep_y, 0x808080);
            }
        } else {
            bool hovered = (g_submenu_open >= 0 && g_sub_hovered == i && g_hover_submenu == g_submenu_open);
            uint32_t bg = hovered ? tc()->select_bg : tc()->startmenu_bg;
            uint32_t fg = hovered ? tc()->select_text : tc()->startmenu_text;
            
            if (rad > 0) {
                vbe_fill_rect_rounded(x + 2, iy, sw - 4, mh, 2, bg);
                if (hovered) vbe_3d_raised_rounded_colors(x + 2, iy, sw - 4, mh, 2,
                    tc()->border_light, tc()->border_face, tc()->border_dark, tc()->border_darkest);
            } else {
                vbe_fill_rect(x + 2, iy, sw - 4, mh, bg);
                if (hovered) vbe_3d_raised_colors(x + 2, iy, sw - 4, mh,
                                                   tc()->border_light, tc()->border_face,
                                                   tc()->border_dark, tc()->border_darkest);
            }
            vbe_draw_text(x + 8, iy + (mh - 8) / 2, item->label, fg, 1);
        }
    }
}

void dosgui_startmenu_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w;
    if (!g_open) return;

    const WubuTheme *t = th();
    int task_h = dosgui_taskbar_height();
    int mh = menu_item_h();
    int mw = menu_w();
    int sw = sidebar_w();
    int menu_y = fb_h - task_h - (g_main_count * mh + 4);

    /* Main menu background */
    int main_h = g_main_count * mh + 4;
    
    if (t->Luna_start_button) {
        /* XP Style: Sidebar on left */
        vbe_shade_rect(MENU_X + 2, menu_y + 2, mw, main_h);
        vbe_fill_rect_rounded(MENU_X, menu_y, mw, main_h, 4, tc()->startmenu_bg);
        vbe_rect_rounded(MENU_X, menu_y, mw, main_h, 4, tc()->border_dark);
        
        /* Sidebar */
        vbe_fill_rect_rounded(MENU_X, menu_y, sw, main_h, 4, tc()->startmenu_sidebar);
        /* Draw "WuBuOS" or Windows logo area in sidebar */
        vbe_draw_text(MENU_X + 4, menu_y + 6, "WuBuOS", 0xFFFFFF, 1);
        if (main_h > 80) {
            vbe_draw_text(MENU_X + 4, menu_y + main_h - 30, "Log off", 0xCCCCCC, 1);
            vbe_draw_text(MENU_X + 4, menu_y + main_h - 18, "Turn Off", 0xCCCCCC, 1);
        }
    } else {
        /* Win98 Style */
        vbe_fill_rect(MENU_X, menu_y, mw, main_h, tc()->startmenu_bg);
        vbe_3d_raised_colors(MENU_X, menu_y, mw, main_h,
                              tc()->border_light, tc()->border_face,
                              tc()->border_dark, tc()->border_darkest);
    }

    /* Main menu items */
    int content_x = MENU_X + sw + 4;
    int content_w = mw - sw - 8;

    for (int i = 0; i < g_main_count; i++) {
        MainMenuItem *mi = &g_main_items[i];
        int iy = menu_y + 2 + i * mh;

        if (mi->type == 2) {
            /* Separator */
            int sep_y = iy + mh / 2;
            if (t->rounded_buttons) {
                vbe_hline(content_x + 4, content_x + content_w - 4, sep_y, tc()->border_dark);
            } else {
                vbe_hline(MENU_X + 4, MENU_X + mw - 4, sep_y, 0x808080);
            }
        } else {
            bool hovered = (i == g_hovered);
            bool has_submenu = (mi->type == 1);
            draw_menu_item(content_x, iy, content_w, mi->label, hovered, has_submenu, mi->type);
        }
    }

    /* Submenu if open */
    if (g_submenu_open >= 0) {
        SmSubmenu *sub = NULL;
        switch (g_submenu_open) {
            case 0: sub = &g_programs; break;
            case 1: sub = &g_accessories; break;
            case 2: sub = &g_wubuos; break;
            case 3: sub = &g_system; break;
        }
        if (sub) {
            int sx = MENU_X + mw;
            int sy = menu_y + 2 + g_submenu_open * mh;
            draw_submenu(sx, sy, sub);
        }
    }
}

/* -- Input ------------------------------------------------------- */

void dosgui_startmenu_handle_click(int x, int y) {
    if (!g_open) return;

    int task_h = dosgui_taskbar_height();
    int mh = menu_item_h();
    int mw = menu_w();
    int sw = sidebar_w();
    int menu_y = dosgui_wm_screen_h() - task_h - (g_main_count * mh + 4);

    int content_x = MENU_X + sw + 4;
    (void)content_x;

    /* Check main menu bounds */
    if (x < MENU_X || x >= MENU_X + mw ||
        y < menu_y || y >= menu_y + g_main_count * mh + 4) {
        /* Check if click in submenu */
        if (g_submenu_open >= 0) {
            SmSubmenu *sub = NULL;
            switch (g_submenu_open) {
                case 0: sub = &g_programs; break;
                case 1: sub = &g_accessories; break;
                case 2: sub = &g_wubuos; break;
                case 3: sub = &g_system; break;
            }
            if (sub) {
                int sx = MENU_X + mw;
                int sy = menu_y + 2 + g_submenu_open * mh;
                int sh = sub->item_count * mh + 4;
                if (x >= sx && x < sx + submenu_w() && y >= sy && y < sy + sh) {
                    /* Click in submenu */
                    int idx = (y - sy - 2) / mh;
                    if (idx >= 0 && idx < sub->item_count) {
                        SmMenuItem *item = &sub->items[idx];
                        if (item->type == SM_ITEM_APP) {
                            dosgui_launch_app(item->app_name);
                            dosgui_startmenu_close();
                        }
                    }
                    return;
                }
            }
        }
        /* Clicked outside menu -- close */
        dosgui_startmenu_close();
        return;
    }

    /* Check if click in sidebar (XP) */
    if (th()->Luna_start_button && x < content_x) {
        /* Click in sidebar - handle logoff/shutdown */
        dosgui_startmenu_close();
        return;
    }

    int idx = (y - menu_y - 2) / mh;
    if (idx < 0 || idx >= g_main_count) return;

    MainMenuItem *mi = &g_main_items[idx];
    if (mi->type == 2) return; /* Separator */

    if (mi->type == 1) {
        /* Toggle submenu */
        g_submenu_open = (g_submenu_open == mi->submenu_id) ? -1 : mi->submenu_id;
    } else if (mi->type == 3) {
        /* Shutdown */
        dosgui_shutdown();
        dosgui_startmenu_close();
    } else {
        /* Launch app */
        dosgui_launch_app(mi->app_name);
        dosgui_startmenu_close();
    }
}

/* -- Hover tracking (called from WM mouse move) ----------------- */
void dosgui_startmenu_track_hover(int x, int y) {
    if (!g_open) {
        g_hovered = -1;
        g_sub_hovered = -1;
        g_hover_submenu = -1;
        return;
    }

    int task_h = dosgui_taskbar_height();
    int mh = menu_item_h();
    int mw = menu_w();
    int sw = sidebar_w();
    int menu_y = dosgui_wm_screen_h() - task_h - (g_main_count * mh + 4);

    int content_x = MENU_X + sw + 4;
    int content_w = mw - sw - 8;

    /* Reset hover */
    g_hovered = -1;
    g_sub_hovered = -1;
    g_hover_submenu = -1;

    /* Main menu hover */
    if (x >= content_x && x < content_x + content_w &&
        y >= menu_y && y < menu_y + g_main_count * mh + 4) {
        int idx = (y - menu_y - 2) / mh;
        if (idx >= 0 && idx < g_main_count && g_main_items[idx].type != 2) {
            g_hovered = idx;
        }
    }

    /* Submenu hover */
    if (g_submenu_open >= 0) {
        SmSubmenu *sub = NULL;
        switch (g_submenu_open) {
            case 0: sub = &g_programs; break;
            case 1: sub = &g_accessories; break;
            case 2: sub = &g_wubuos; break;
            case 3: sub = &g_system; break;
        }
        if (sub) {
            int sx = MENU_X + mw;
            int sy = menu_y + 2 + g_submenu_open * mh;
            int sh = sub->item_count * mh + 4;
            if (x >= sx && x < sx + submenu_w() && y >= sy && y < sy + sh) {
                g_hover_submenu = g_submenu_open;
                int idx = (y - sy - 2) / mh;
                if (idx >= 0 && idx < sub->item_count && sub->items[idx].type != SM_ITEM_SEPARATOR) {
                    g_sub_hovered = idx;
                }
            }
        }
    }
}

/* ==================================================================
 * Start Menu Enhancements Implementation -- Cell 402+
 * Search, Recently Used, Power Options, All Programs Tree
 * ================================================================== */

static SmProgramDB g_program_db = {0};
static SmSearchState g_search = {0};
static SmProgramEntry g_recent[DOSGUI_MAX_RECENT] = {0};
static int g_recent_count = 0;
static SmTreeNode g_tree_root = {0};
static bool g_search_mode = false;

/* -- Program Database --------------------------------------------- */

void dosgui_startmenu_build_programs_db(void) {
    if (g_program_db.count > 0) return;
    
    SmProgramEntry *e;
    
    /* Accessories */
    e = &g_program_db.entries[g_program_db.count++];
    strcpy(e->name, "Notepad"); strcpy(e->executable, "Notepad");
    strcpy(e->category, "Accessories"); e->is_builtin = true;
    
    e = &g_program_db.entries[g_program_db.count++];
    strcpy(e->name, "Paint"); strcpy(e->executable, "Paint");
    strcpy(e->category, "Accessories"); e->is_builtin = true;
    
    e = &g_program_db.entries[g_program_db.count++];
    strcpy(e->name, "Editor"); strcpy(e->executable, "Editor");
    strcpy(e->category, "Accessories"); e->is_builtin = true;
    
    e = &g_program_db.entries[g_program_db.count++];
    strcpy(e->name, "WuBu Canvas"); strcpy(e->executable, "WuBu Canvas");
    strcpy(e->category, "Accessories"); e->is_builtin = true;
    
    e = &g_program_db.entries[g_program_db.count++];
    strcpy(e->name, "FreeDoom"); strcpy(e->executable, "FreeDoom");
    strcpy(e->category, "Accessories"); e->is_builtin = true;
    
    e = &g_program_db.entries[g_program_db.count++];
    strcpy(e->name, "Calculator"); strcpy(e->executable, "Calculator");
    strcpy(e->category, "Accessories"); e->is_builtin = true;
    
    /* WuBuOS */
    e = &g_program_db.entries[g_program_db.count++];
    strcpy(e->name, "HolyC Terminal"); strcpy(e->executable, "HolyC Terminal");
    strcpy(e->category, "WuBuOS"); e->is_builtin = true;
    
    e = &g_program_db.entries[g_program_db.count++];
    strcpy(e->name, "Temple REPL"); strcpy(e->executable, "Temple REPL");
    strcpy(e->category, "WuBuOS"); e->is_builtin = true;
    
    e = &g_program_db.entries[g_program_db.count++];
    strcpy(e->name, "Package Manager"); strcpy(e->executable, "Package Manager");
    strcpy(e->category, "WuBuOS"); e->is_builtin = true;
    
    e = &g_program_db.entries[g_program_db.count++];
    strcpy(e->name, "Container Manager"); strcpy(e->executable, "Container Manager");
    strcpy(e->category, "WuBuOS"); e->is_builtin = true;
    
    /* System */
    e = &g_program_db.entries[g_program_db.count++];
    strcpy(e->name, "File Manager"); strcpy(e->executable, "File Manager");
    strcpy(e->category, "System"); e->is_builtin = true;
    
    e = &g_program_db.entries[g_program_db.count++];
    strcpy(e->name, "Control Panel"); strcpy(e->executable, "Control Panel");
    strcpy(e->category, "System"); e->is_builtin = true;
    
    e = &g_program_db.entries[g_program_db.count++];
    strcpy(e->name, "Terminal"); strcpy(e->executable, "Terminal");
    strcpy(e->category, "System"); e->is_builtin = true;

    /* Scan /apps for .wubu manifests — adds discovered apps dynamically.
     * In hosted mode, /apps maps to <install_prefix>/apps/ on the host.
     * Each app gets its own entry derived from the manifest filename. */
    {
        const char *apps_dir = "/apps";
        DIR *dir = opendir(apps_dir);
        if (dir) {
            struct dirent *de;
            while ((de = readdir(dir)) != NULL) {
                if (de->d_name[0] == '.') continue;
                /* Check for .wubu manifest extension and regular file */
                size_t nlen = strlen(de->d_name);
                int is_wubu = (nlen > 5 &&
                    de->d_name[nlen-5] == '.' &&
                    de->d_name[nlen-4] == 'w' &&
                    de->d_name[nlen-3] == 'u' &&
                    de->d_name[nlen-2] == 'b' &&
                    de->d_name[nlen-1] == 'u');
                /* Use stat() to check for regular file (portable, no DT_REG) */
                char fpath[320];
                snprintf(fpath, sizeof(fpath), "%s/%s", apps_dir, de->d_name);
                struct stat fst;
                int is_reg = (stat(fpath, &fst) == 0 && S_ISREG(fst.st_mode));
                if ((is_wubu || is_reg) && g_program_db.count < DOSGUI_MAX_PROGRAM_ENTRIES) {
                    /* Check it's not already a built-in */
                    int is_dup = 0;
                    char app_name[48];
                    strncpy(app_name, de->d_name, sizeof(app_name) - 1);
                    app_name[sizeof(app_name) - 1] = '\0';
                    /* Strip .wubu extension for display name */
                    if (is_wubu) app_name[nlen - 5] = '\0';
                    for (int j = 0; j < g_program_db.count; j++) {
                        if (strcmp(g_program_db.entries[j].name, app_name) == 0) {
                            is_dup = 1; break;
                        }
                    }
                    if (!is_dup) {
                        e = &g_program_db.entries[g_program_db.count++];
                        strncpy(e->name, app_name, sizeof(e->name) - 1);
                        snprintf(e->executable, sizeof(e->executable), "%s/%s",
                                 apps_dir, de->d_name);
                        strcpy(e->category, "Apps");
                        e->is_builtin = false;
                        e->run_count = 0;
                        e->last_run = 0;
                    }
                }
            }
            closedir(dir);
        }
        /* Also try host-local apps dir relative to binary (WSL/dev mode) */
        if (g_program_db.count < DOSGUI_MAX_PROGRAM_ENTRIES) {
            const char *local_apps = "./apps";
            DIR *ldir = opendir(local_apps);
            if (ldir) {
                struct dirent *lde;
                while ((lde = readdir(ldir)) != NULL) {
                    if (lde->d_name[0] == '.') continue;
                    size_t ln = strlen(lde->d_name);
                    int lw = (ln > 5 &&
                        lde->d_name[ln-5] == '.' &&
                        lde->d_name[ln-4] == 'w' &&
                        lde->d_name[ln-3] == 'u' &&
                        lde->d_name[ln-2] == 'b' &&
                        lde->d_name[ln-1] == 'u');
                    /* Portable check via stat() instead of DT_REG */
                    char lfpath[320];
                    snprintf(lfpath, sizeof(lfpath), "%s/%s", local_apps, lde->d_name);
                    struct stat lst;
                    int lreg = (stat(lfpath, &lst) == 0 && S_ISREG(lst.st_mode));
                    if ((lw || lreg) &&
                        g_program_db.count < DOSGUI_MAX_PROGRAM_ENTRIES) {
                        char lname[48];
                        strncpy(lname, lde->d_name, sizeof(lname) - 1);
                        lname[sizeof(lname) - 1] = '\0';
                        if (lw) lname[ln - 5] = '\0';
                        int dup = 0;
                        for (int j = 0; j < g_program_db.count; j++) {
                            if (strcmp(g_program_db.entries[j].name, lname) == 0) {
                                dup = 1; break;
                            }
                        }
                        if (!dup) {
                            e = &g_program_db.entries[g_program_db.count++];
                            strncpy(e->name, lname, sizeof(e->name) - 1);
                            snprintf(e->executable, sizeof(e->executable),
                                     "%s/%s", local_apps, lde->d_name);
                            strcpy(e->category, "Apps");
                            e->is_builtin = false;
                        }
                    }
                }
                closedir(ldir);
            }
        }
    }
}

/* -- Search ------------------------------------------------------- */

void dosgui_startmenu_search_init(void) {
    memset(&g_search, 0, sizeof(g_search));
    g_search.cursor_pos = 0;
}

void dosgui_startmenu_search_update(const char *query) {
    if (!query) {
        g_search.query[0] = '\0';
        g_search.cursor_pos = 0;
        g_search.result_count = 0;
        g_search.active = false;
        return;
    }
    
    strncpy(g_search.query, query, DOSGUI_SEARCH_MAX_LEN - 1);
    g_search.query[DOSGUI_SEARCH_MAX_LEN - 1] = '\0';
    g_search.cursor_pos = strlen(g_search.query);
    g_search.active = true;
    g_search.result_count = 0;
    
    if (g_search.query[0] == '\0') return;
    
    char lower_query[DOSGUI_SEARCH_MAX_LEN];
    for (int i = 0; g_search.query[i] && i < DOSGUI_SEARCH_MAX_LEN - 1; i++) {
        lower_query[i] = tolower(g_search.query[i]);
    }
    lower_query[strlen(lower_query)] = '\0';
    
    for (int i = 0; i < g_program_db.count && g_search.result_count < DOSGUI_MAX_PROGRAM_ENTRIES; i++) {
        SmProgramEntry *e = &g_program_db.entries[i];
        char lower_name[48];
        for (int j = 0; e->name[j]; j++) lower_name[j] = tolower(e->name[j]);
        lower_name[strlen(e->name)] = '\0';
        
        if (strstr(lower_name, lower_query)) {
            g_search.results[g_search.result_count++] = e;
        }
    }
}

void dosgui_startmenu_search_clear(void) {
    g_search.query[0] = '\0';
    g_search.cursor_pos = 0;
    g_search.result_count = 0;
    g_search.active = false;
}

int dosgui_startmenu_search_get_results(SmProgramEntry ***out_results, int *out_count) {
    if (!g_search.active) return -1;
    if (out_results) *out_results = g_search.results;
    if (out_count) *out_count = g_search.result_count;
    return 0;
}

/* -- Recently Used ------------------------------------------------ */

void dosgui_startmenu_recent_add(const char *app_name) {
    if (!app_name) return;
    
    for (int i = 0; i < g_recent_count; i++) {
        if (strcmp(g_recent[i].name, app_name) == 0) {
            SmProgramEntry temp = g_recent[i];
            memmove(&g_recent[1], &g_recent[0], i * sizeof(SmProgramEntry));
            g_recent[0] = temp;
            temp.last_run = time(NULL);
            temp.run_count++;
            return;
        }
    }
    
    for (int i = 0; i < g_program_db.count; i++) {
        if (strcmp(g_program_db.entries[i].name, app_name) == 0) {
            if (g_recent_count < DOSGUI_MAX_RECENT) {
                g_recent[g_recent_count++] = g_program_db.entries[i];
            } else {
                memmove(&g_recent[1], &g_recent[0], (DOSGUI_MAX_RECENT - 1) * sizeof(SmProgramEntry));
            }
            g_recent[0] = g_program_db.entries[i];
            g_recent[0].last_run = time(NULL);
            g_recent[0].run_count++;
            break;
        }
    }
}

int dosgui_startmenu_recent_get(SmProgramEntry **out_entries, int max) {
    int count = g_recent_count < max ? g_recent_count : max;
    for (int i = 0; i < count; i++) {
        out_entries[i] = &g_recent[i];
    }
    return count;
}

/* -- All Programs Tree -------------------------------------------- */

static SmTreeNode g_tree_nodes[32];
static int g_tree_node_count = 0;

static SmTreeNode *tree_node_create(const char *label, int type) {
    if (g_tree_node_count >= 32) return NULL;
    SmTreeNode *n = &g_tree_nodes[g_tree_node_count++];
    memset(n, 0, sizeof(SmTreeNode));
    strncpy(n->label, label, sizeof(n->label) - 1);
    n->type = type;
    n->expanded = (type == 0);
    return n;
}

static void tree_add_child(SmTreeNode *parent, SmTreeNode *child) {
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
        
        vbe_draw_text(x + indent, ty, cat->label, tc()->win_title_text, 1);
        ty += item_h;
        
        if (cat->expanded && cat->children) {
            for (int j = 0; j < cat->child_count; j++) {
                SmTreeNode *prog = &cat->children[j];
                if (prog->type == 1 && prog->program) {
                    vbe_draw_text(x + indent * 2, ty, prog->program->name, tc()->startmenu_text, 1);
                    ty += item_h;
                }
            }
        }
        ty += 4;
    }
}

/* -- Power Options ------------------------------------------------ */

void dosgui_startmenu_power(PowerAction action) {
    switch (action) {
        case PWR_SHUTDOWN:
            dosgui_shutdown();
            break;
        case PWR_RESTART:
            dosgui_shutdown();
            break;
        case PWR_LOGOFF:
            break;
        case PWR_SLEEP:
            break;
        case PWR_HIBERNATE:
            break;
    }
    dosgui_startmenu_close();
}

/* -- Enhanced Rendering ------------------------------------------- */

void dosgui_startmenu_render_search_bar(uint32_t *fb, int fb_w, int fb_h, int menu_x, int menu_y, int w) {
    (void)fb; (void)fb_w; (void)fb_h;
    
    if (!g_search.active && !g_search_mode) return;
    
    int bar_h = 28;
    int bar_y = menu_y - bar_h - 2;
    int bar_w = w - 8;
    
    vbe_fill_rect_rounded(menu_x + 4, bar_y, bar_w, bar_h, 4, tc()->win_face);
    vbe_3d_sunken_rounded_colors(menu_x + 4, bar_y, bar_w, bar_h, 4,
                                  tc()->border_light, tc()->border_face,
                                  tc()->border_dark, tc()->border_darkest);
    
    vbe_draw_text(menu_x + 10, bar_y + (bar_h - 8) / 2, "Search", tc()->icon_text_shadow, 1);
    
    char display[DOSGUI_SEARCH_MAX_LEN + 8];
    snprintf(display, sizeof(display), "%s%c", g_search.query, 
             (g_search.active && (time(NULL) * 2) % 2) ? '_' : ' ');
    vbe_draw_text(menu_x + 80, bar_y + (bar_h - 8) / 2, display, tc()->win_title_text, 1);
}

void dosgui_startmenu_render_recent(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int max_items) {
    (void)fb; (void)fb_w; (void)fb_h;
    
    SmProgramEntry *entries[DOSGUI_MAX_RECENT];
    int count = dosgui_startmenu_recent_get(entries, max_items);
    if (count == 0) return;
    
    int item_h = 22;
    int ty = y;
    
    vbe_draw_text(x, ty, "Recently Used", tc()->win_title_text, 1);
    ty += 20;
    
    for (int i = 0; i < count; i++) {
        SmProgramEntry *e = entries[i];
        vbe_fill_rect(x, ty, w, item_h, tc()->startmenu_bg);
        vbe_draw_text(x + 8, ty + (item_h - 8) / 2, e->name, tc()->startmenu_text, 1);
        ty += item_h;
    }
}

void dosgui_startmenu_render_all_programs(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
    (void)fb; (void)fb_w; (void)fb_h; (void)w; (void)h;
    dosgui_startmenu_tree_render(fb, fb_w, fb_h, x, y);
}

void dosgui_startmenu_render_power_options(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w) {
    (void)fb; (void)fb_w; (void)fb_h;
    
    const char *labels[] = { "Shut Down", "Restart", "Log Off", "Sleep", "Hibernate" };
    int count = sizeof(labels) / sizeof(labels[0]);
    int item_h = 28;
    int ty = y;
    
    for (int i = 0; i < count; i++) {
        vbe_fill_rect_rounded(x, ty, w, item_h, 4, tc()->btn_face);
        vbe_3d_sunken_rounded_colors(x, ty, w, item_h, 4,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
        vbe_draw_text(x + 10, ty + (item_h - 8) / 2, labels[i], tc()->btn_text, 1);
        ty += item_h + 4;
    }
}

/* -- Input Handling ----------------------------------------------- */

void dosgui_startmenu_handle_key(int key, uint32_t mods) {
    (void)mods;
    if (!g_open) return;
    
    if (key == 27) {
        if (g_search.active) {
            dosgui_startmenu_search_clear();
        } else {
            dosgui_startmenu_close();
        }
    }
    else if (key == '\t' && g_open) {
        g_search_mode = !g_search_mode;
        if (g_search_mode) {
            g_search.active = true;
        }
    }
    else if (key == 8 && g_search.active) {
        if (g_search.cursor_pos > 0) {
            g_search.query[--g_search.cursor_pos] = '\0';
            dosgui_startmenu_search_update(g_search.query);
        }
    }
    else if (key == 13 && g_search.active) {
        if (g_search.result_count > 0) {
            dosgui_launch_app(g_search.results[0]->name);
            dosgui_startmenu_close();
            g_search_mode = false;
        }
    }
}

void dosgui_startmenu_handle_search_input(int key, uint32_t mods) {
    (void)mods;
    if (!g_search.active) return;
    
    if (key >= 32 && key < 127 && g_search.cursor_pos < DOSGUI_SEARCH_MAX_LEN - 1) {
        g_search.query[g_search.cursor_pos++] = (char)key;
        g_search.query[g_search.cursor_pos] = '\0';
        dosgui_startmenu_search_update(g_search.query);
    }
}

void dosgui_startmenu_init_enhanced(void) {
    dosgui_startmenu_build_programs_db();
    dosgui_startmenu_search_init();
    dosgui_startmenu_tree_build();
}

void dosgui_startmenu_recent_add(const char *app_name);