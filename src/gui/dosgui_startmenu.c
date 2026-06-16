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
        if (hovered) vbe_3d_raised(x, y, w, mh);
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
        vbe_3d_raised(x, y, sw, h);
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
                if (hovered) vbe_3d_raised(x + 2, iy, sw - 4, mh);
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
        vbe_3d_raised(MENU_X, menu_y, mw, main_h);
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