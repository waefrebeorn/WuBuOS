/*
 * dosgui_startmenu.c  --  WuBuOS DosGui Start Menu Implementation
 *
 * Cell 402: Win98-style cascading start menu.
 * Start → Programs → {Accessories, WuBuOS, System}
 *              Accessories → {Notepad, Paint, Calculator, Terminal}
 *              WuBuOS → {Temple REPL, Package Manager, Container Manager}
 *              System → {File Manager, Settings}
 *         → Documents, Find, Help, Run, Shutdown
 */

#include "dosgui_startmenu.h"
#include "dosgui_desktop.h"
#include "dosgui_wm.h"

extern void dosgui_tick(void);
#include "../kernel/vbe.h"
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

/* Main menu items */
typedef struct {
    char label[48];
    int  type; /* 0=app, 1=submenu, 2=separator */
    int  submenu_id;
    char app_name[48];
} MainMenuItem;

static MainMenuItem g_main_items[12];
static int g_main_count = 0;

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
        "Shut Down", 0, -1, "Shutdown"};
}

void dosgui_startmenu_init(void) {
    build_accessories();
    build_wubuos();
    build_system();
    build_main_menu();
    g_open = 0;
    g_hovered = -1;
    g_submenu_open = -1;
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

#define MENU_X         4
#define MENU_ITEM_H    22
#define MENU_W         180
#define SUBMENU_W      160
#define MENU_BORDER    2

static void draw_menu_item(int x, int y, int w, const char *label,
                            int hovered, int has_submenu) {
    uint32_t bg = hovered ? 0x000080 : 0xC0C0C0;
    vbe_fill_rect(x, y, w, MENU_ITEM_H, bg);
    vbe_draw_text(x + 6, y + 7, label,
                  hovered ? 0xFFFFFF : 0x000000, 1);
    if (has_submenu) {
        /* Arrow indicator */
        vbe_draw_text(x + w - 14, y + 7, ">", 0x000000, 1);
    }
}

static void draw_submenu(int x, int y, SmSubmenu *sub) {
    int h = sub->item_count * MENU_ITEM_H + 4;
    vbe_fill_rect(x, y, SUBMENU_W, h, 0xC0C0C0);
    vbe_3d_raised(x, y, SUBMENU_W, h);

    for (int i = 0; i < sub->item_count; i++) {
        SmMenuItem *item = &sub->items[i];
        int iy = y + 2 + i * MENU_ITEM_H;
        if (item->type == SM_ITEM_SEPARATOR) {
            vbe_hline(x + 4, x + SUBMENU_W - 4, iy + 10, 0x808080);
        } else {
            draw_menu_item(x + 2, iy, SUBMENU_W - 4, item->label, 0, 0);
        }
    }
}

void dosgui_startmenu_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    if (!g_open) return;

    int task_h = dosgui_taskbar_height();
    int menu_y = fb_h - task_h - (g_main_count * MENU_ITEM_H + 4);

    /* Main menu */
    vbe_fill_rect(MENU_X, menu_y, MENU_W,
                  g_main_count * MENU_ITEM_H + 4, 0xC0C0C0);
    vbe_3d_raised(MENU_X, menu_y, MENU_W,
                  g_main_count * MENU_ITEM_H + 4);

    for (int i = 0; i < g_main_count; i++) {
        MainMenuItem *mi = &g_main_items[i];
        int iy = menu_y + 2 + i * MENU_ITEM_H;

        if (mi->type == 2) {
            /* Separator */
            vbe_hline(MENU_X + 4, MENU_X + MENU_W - 4, iy + 10, 0x808080);
        } else {
            draw_menu_item(MENU_X + 2, iy, MENU_W - 4,
                           mi->label, i == g_hovered, mi->type == 1);
        }
    }

    /* Submenu if open */
    if (g_submenu_open == 0) {
        /* Programs submenu */
        int sx = MENU_X + MENU_W;
        int sy = menu_y + 2;
        draw_submenu(sx, sy, &g_programs);
    }
}

/* -- Input ------------------------------------------------------- */

void dosgui_startmenu_handle_click(int x, int y) {
    if (!g_open) return;

    int task_h = dosgui_taskbar_height();
    int menu_y = dosgui_wm_screen_h() - task_h -
                 (g_main_count * MENU_ITEM_H + 4);

    /* Check main menu bounds */
    if (x < MENU_X || x >= MENU_X + MENU_W ||
        y < menu_y || y >= menu_y + g_main_count * MENU_ITEM_H + 4) {
        /* Clicked outside menu — close */
        dosgui_startmenu_close();
        return;
    }

    int idx = (y - menu_y - 2) / MENU_ITEM_H;
    if (idx < 0 || idx >= g_main_count) return;

    MainMenuItem *mi = &g_main_items[idx];
    if (mi->type == 2) return; /* Separator */

    if (mi->type == 1) {
        /* Toggle submenu */
        g_submenu_open = (g_submenu_open == mi->submenu_id) ? -1 : mi->submenu_id;
    } else {
        /* Launch app */
        dosgui_launch_app(mi->app_name);
        dosgui_startmenu_close();
    }
}
