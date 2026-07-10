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

#include "dosgui_startmenu_internal.h"
#include "dosgui_startmenu.h"
#include "dosgui_desktop.h"
#include "dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "../gui/wubu_mime.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

/* -- Inline Layout Helpers ------------------------------------------ */

static inline int menu_item_h(void) {
    return 24;  /* EX_ROW_H equivalent */
}

static inline int submenu_w(void) {
    return 180;  /* EX_SUBMENU_W equivalent */
}

static inline int menu_w(void) {
    return 200;  /* EX_MENU_W equivalent */
}

static inline int sidebar_w(void) {
    return 48;  /* EX_SIDEBAR_W equivalent */
}

static inline int menu_border_w(void) {
    return 2;
}

static inline int ex_tree_indent(void) {
    return 14;
}

static inline int ex_title_h(void) {
    return 22;
}

static inline int ex_toolbar_h(void) {
    return 28;
}

static inline int ex_breadcrumb_h(void) {
    return 24;
}

static inline int ex_statusbar_h(void) {
    return 24;
}

static inline int ex_border_w(void) {
    return 2;
}



/* -- Global Submenu State ------------------------------------------ */

SmSubmenu g_programs = {0};
SmSubmenu g_accessories = {0};
SmSubmenu g_wubuos = {0};
SmSubmenu g_system = {0};

MainMenuItem g_main_items[16] = {0};
int g_main_count = 0;

int g_open = 0;
int g_hovered = -1;
int g_submenu_open = -1;
int g_sub_hovered = -1;
int g_hover_submenu = -1;

SmProgramDB g_program_db = {0};
SmSearchState g_search = {0};
SmProgramEntry g_recent[DOSGUI_MAX_RECENT] = {0};
int g_recent_count = 0;

SmTreeNode g_tree_nodes[32] = {0};
SmTreeNode g_tree_root = {0};
int g_tree_node_count = 0;
bool g_search_mode = false;


/* -- Safe String Macros (WUBU_SAFE_STRING) -------------------------- */

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

/* -- Dynamic Menu Building from .desktop files -------------------- */

/* Forward declarations for types (defined in header) */
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

/* Theme function forward declarations (from wubu_theme.h) */
extern const WubuThemeColors *wubu_theme_colors(void);
extern const WubuTheme *wubu_theme_get(void);


static void build_main_menu_from_desktop(void) {
    g_main_count = 0;
    
    /* Programs submenu (index 0) */
    g_main_items[g_main_count++] = (MainMenuItem){
        "Programs", 1, 0, ""};
    
    /* Documents -> File Manager */
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

/* Build submenus from MIME desktop entries by category */
static void build_submenus_from_desktop(void) {
    MimeSystem *mime = wubu_mime_state();
    
    /* Reset submenu counts */
    g_programs.item_count = 0;
    g_accessories.item_count = 0;
    g_wubuos.item_count = 0;
    g_system.item_count = 0;
    
    /* Category mapping */
    struct { const char *cat; SmSubmenu *sub; } cats[] = {
        {"Accessories", &g_accessories},
        {"WuBuOS", &g_wubuos},
        {"System", &g_system},
        {"Development", &g_programs},
        {"Graphics", &g_programs},
        {"Internet", &g_programs},
        {"Office", &g_programs},
        {"AudioVideo", &g_programs},
        {"Game", &g_programs},
        {"Apps", &g_programs},
    };
    
    for (int i = 0; i < mime->desktop_entry_count; i++) {
        DesktopEntry *de = &mime->desktop_entries[i];
        if (de->hidden || de->no_display) continue;
        if (strcmp(de->type, "Application") != 0) continue;
        if (de->exec[0] == '\0') continue;
        
        /* Find matching category */
        SmSubmenu *target = &g_programs; /* default */
        for (size_t c = 0; c < sizeof(cats)/sizeof(cats[0]); c++) {
            if (strstr(de->categories, cats[c].cat)) {
                target = cats[c].sub;
                break;
            }
        }
        
        if (target->item_count < 12) {
            target->items[target->item_count++] = (SmMenuItem){
                SM_ITEM_APP, "", "", 0};
            strncpy(target->items[target->item_count - 1].label, de->name, 47);
            strncpy(target->items[target->item_count - 1].app_name, de->name, 47);
        }
    }
    
    /* WuBuOS - ensure core apps present */
    for (int i = 0; i < 4; i++) {
        SmSubmenu *sub = &g_wubuos;
        const char *label = "", *app_name = "";
        switch (i) {
            case 0: label = "HolyC Terminal"; app_name = "HolyC Terminal"; break;
            case 1: label = "Temple REPL"; app_name = "Temple REPL"; break;
            case 2: label = "Package Manager"; app_name = "Package Manager"; break;
            case 3: label = "Container Manager"; app_name = "Container Manager"; break;
        }
        for (int j = 0; j < sub->item_count; j++) {
            if (strcmp(sub->items[j].label, label) == 0) goto next_wubuos;
        }
        if (sub->item_count < 12) {
            sub->items[sub->item_count++] = (SmMenuItem){
                SM_ITEM_APP, "", "", 0};
            strncpy(sub->items[sub->item_count - 1].label, label, 47);
            strncpy(sub->items[sub->item_count - 1].app_name, app_name, 47);
        }
        next_wubuos:;
    }
    
    /* System */
    for (int i = 0; i < 2; i++) {
        SmSubmenu *sub = &g_system;
        const char *label = "", *app_name = "";
        switch (i) {
            case 0: label = "File Manager"; app_name = "File Manager"; break;
            case 1: label = "Settings"; app_name = "Settings"; break;
        }
        for (int j = 0; j < sub->item_count; j++) {
            if (strcmp(sub->items[j].label, label) == 0) goto next_system;
        }
        if (sub->item_count < 12) {
            sub->items[sub->item_count++] = (SmMenuItem){
                SM_ITEM_APP, "", "", 0};
            strncpy(sub->items[sub->item_count - 1].label, label, 47);
            strncpy(sub->items[sub->item_count - 1].app_name, app_name, 47);
        }
        next_system:;
    }
    
    /* Accessories */
    for (int i = 0; i < 6; i++) {
        SmSubmenu *sub = &g_accessories;
        const char *label = "", *app_name = "";
        switch (i) {
            case 0: label = "Notepad"; app_name = "Notepad"; break;
            case 1: label = "Paint"; app_name = "Paint"; break;
            case 2: label = "Editor"; app_name = "Editor"; break;
            case 3: label = "WuBu Canvas"; app_name = "WuBu Canvas"; break;
            case 4: label = "FreeDoom"; app_name = "FreeDoom"; break;
            case 5: label = "Calculator"; app_name = "Calculator"; break;
        }
        for (int j = 0; j < sub->item_count; j++) {
            if (strcmp(sub->items[j].label, label) == 0) goto next_accessories;
        }
        if (sub->item_count < 12) {
            sub->items[sub->item_count++] = (SmMenuItem){
                SM_ITEM_APP, "", "", 0};
            strncpy(sub->items[sub->item_count - 1].label, label, 47);
            strncpy(sub->items[sub->item_count - 1].app_name, app_name, 47);
        }
        next_accessories:;
    }
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
    
    /* Draw truncated label with ellipsis if too wide */
    int max_text_w = w - 20; /* Reserve space for arrow and padding */
    if (has_submenu) max_text_w -= 16;
    int text_w = vbe_text_width(label, 1);
    if (text_w > max_text_w) {
        char truncated[64];
        int len = strlen(label);
        strncpy(truncated, label, len);
        truncated[len] = '\0';
        while (len > 0 && vbe_text_width(truncated, 1) > max_text_w - 6) {
            len--;
            truncated[len] = '\0';
        }
        if (len > 0) {
            strncpy(truncated + len, "...", 3);
            truncated[len + 3] = '\0';
        } else {
            strcpy(truncated, "...");
        }
        vbe_draw_text(x + 6, y + (mh - 8) / 2, truncated, fg, 1);
    } else {
        vbe_draw_text(x + 6, y + (mh - 8) / 2, label, fg, 1);
    }
    
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


/* -- All Programs Tree -------------------------------------------- */

SmTreeNode g_tree_nodes[32];


/* -- Power Options ------------------------------------------------ */


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
    /* Arrow key navigation for main menu */
    else if (g_open && !g_search.active) {
        if (key == 0xE048) { /* Up arrow */
            if (g_hovered > 0) {
                g_hovered--;
                while (g_hovered >= 0 && g_main_items[g_hovered].type == 2) {
                    g_hovered--;
                }
            }
        }
        else if (key == 0xE050) { /* Down arrow */
            if (g_hovered < g_main_count - 1) {
                g_hovered++;
                while (g_hovered < g_main_count && g_main_items[g_hovered].type == 2) {
                    g_hovered++;
                }
            }
        }
        else if (key == 0xE04D) { /* Right arrow - open submenu */
            if (g_hovered >= 0 && g_hovered < g_main_count && g_main_items[g_hovered].type == 1) {
                g_submenu_open = g_hovered;
                g_sub_hovered = 0;
            }
        }
        else if (key == 0xE04B) { /* Left arrow - close submenu */
            if (g_submenu_open >= 0) {
                g_submenu_open = -1;
            }
        }
        else if (key == 13) { /* Enter - activate */
            if (g_submenu_open >= 0) {
                /* Activate submenu item */
                int submenu_id = g_main_items[g_hovered].submenu_id;
                if (submenu_id >= 0 && submenu_id == 0 && g_sub_hovered < g_accessories.item_count) {
                    dosgui_launch_app(g_accessories.items[g_sub_hovered].app_name);
                    dosgui_startmenu_close();
                }
                else if (submenu_id >= 0 && submenu_id == 1 && g_sub_hovered < g_wubuos.item_count) {
                    dosgui_launch_app(g_wubuos.items[g_sub_hovered].app_name);
                    dosgui_startmenu_close();
                }
                else if (submenu_id >= 0 && submenu_id == 2 && g_sub_hovered < g_system.item_count) {
                    if (g_sub_hovered == g_system.item_count - 1) { /* Shutdown */
                        dosgui_shutdown();
                    } else {
                        dosgui_launch_app(g_system.items[g_sub_hovered].app_name);
                    }
                    dosgui_startmenu_close();
                }
            }
            else if (g_hovered >= 0 && g_hovered < g_main_count) {
                int type = g_main_items[g_hovered].type;
                if (type == 0) { /* App */
                    dosgui_launch_app(g_main_items[g_hovered].app_name);
                    dosgui_startmenu_close();
                }
                else if (type == 3) { /* Shutdown */
                    dosgui_shutdown();
                    dosgui_startmenu_close();
                }
                else if (type == 1) { /* Submenu */
                    g_submenu_open = g_hovered;
                    g_sub_hovered = 0;
                }
            }
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

void dosgui_startmenu_init(void) {
    dosgui_startmenu_init_enhanced();
}

void dosgui_startmenu_init_enhanced(void) {
    dosgui_startmenu_build_programs_db();
    dosgui_startmenu_search_init();
    dosgui_startmenu_tree_build();
}

void dosgui_startmenu_shutdown(void) {
    dosgui_shutdown();
}

void dosgui_startmenu_recent_add(const char *app_name);


