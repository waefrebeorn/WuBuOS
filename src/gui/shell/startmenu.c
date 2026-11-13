/*
 * startmenu.c  --  WuBuOS DosGui Start Menu Implementation
 *
 * Cell 402: THEMED Cascading Start Menu.
 * Win98-style: Start -> Programs -> {Accessories, WuBuOS, System}
 *              Accessories -> {Notepad, Paint, Calculator, Terminal}
 *              WuBuOS -> {Temple REPL, Package Manager, Container Manager}
 *              System -> {File Manager, Settings}
 *         -> Documents, Find, Help, Run, Shutdown
 * XP-style: Luna Start orb, sidebar, hover highlighting, rounded items
 */

#include "../../egojar.h"
#include "../../egojar_net.h"
#include <sys/socket.h>
#include "./startmenu_internal.h"
#include "./startmenu.h"
#include "../era_apps.h"
#include "./desktop.h"
#include "../wm.h"
#include "../wm_internal.h"
#include "../../apps/apps.h"
#include "../../kernel/vbe.h"
#include "../../gui/wubu_theme.h"
#include "../../gui/wubu_mime.h"
#include "../../gui/wubu_bonzi.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include "../../runtime/wubu_ftw.h"

/* Opening a file from search. Weak so headless targets need not carry an
 * editor; an absent one leaves such results inert there. */
__attribute__((weak)) DosGuiWindow *notepad_open(const char *filename);

/* Activate one search result.  Apps launch by name; files OPEN -- an entry that
 * looks like "notes.txt" must not be handed to the app registry. */
static void startmenu_activate(const SmProgramEntry *e) {
    if (!e) return;
    if (e->opens_file && e->executable[0]) {
        if (notepad_open) notepad_open(e->executable);
        return;
    }
    launch_app(e->name);
}


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


/* Main-menu + submenu construction moved to startmenu_db.c
 * (startmenu_build_main_menu / startmenu_build_submenus). */

/* -- State ------------------------------------------------------- */

int  startmenu_is_open(void) { return g_open; }
void startmenu_toggle(void) { g_open = !g_open; if (g_open) g_search.active = true; }
void startmenu_open(void)   {
    g_open = 1;
    g_search.active = true;
    /* Files appear here as they are created, so index them on every open rather
     * than once at boot -- a document saved five seconds ago should be findable
     * five seconds later. */
    extern void startmenu_index_files(void);
    startmenu_index_files();
}
void startmenu_close(void)  { g_open = 0; g_submenu_open = -1; startmenu_search_clear(); }

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
    int label_x = x + 6;
    if (type == 4) {
        /* Checkbox: [x] when companion enabled, [ ] when disabled. */
        vbe_fill_rect(label_x, y + (mh - 8) / 2, 10, 8, 0xFFFFFF);
        vbe_rect(label_x, y + (mh - 8) / 2, 10, 8, 0x000000);
        if (wubu_bonzi_is_enabled())
            vbe_fill_rect(label_x + 2, y + (mh - 8) / 2 + 2, 6, 4, 0x000000);
        label_x += 16;
    }
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
        vbe_draw_text(label_x, y + (mh - 8) / 2, truncated, fg, 1);
    } else {
        vbe_draw_text(label_x, y + (mh - 8) / 2, label, fg, 1);
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

/* -- Windows 11-class Start menu ---------------------------------- */
/*
 * The dark theme ships the Win11 shell: a centred, rounded panel with a
 * search bar, a Pinned app grid, a Recommended list and a user/power
 * footer.  Geometry lives in startmenu_win11_layout() so the renderer and
 * the pointer path can never disagree about where a tile is.
 */

typedef struct {
    int px, py, pw, ph;
    int search_x, search_y, search_w, search_h;
    int grid_x, grid_y, tile, gap, cols, rows;
    int rec_label_y;
    int rec_x, rec_y, rec_item_w, rec_item_h, rec_cols, rec_rows;
    int foot_y;
    int power_x, power_y, power_w, power_h;
} SmWin11;

/* Pinned apps, in display order.  name = launch key; the icon type and
 * colour mirror the app registry so the tile matches the desktop icon. */
typedef struct {
    const char *name;
    int         icon_type;
    uint32_t    icon_color;
} Win11Pin;

static const Win11Pin g_win11_pins[] = {
    { "Notepad",      DESK_ICON_NOTEPAD,    0x00AAAA00 },
    { "Paint",        DESK_ICON_PAINT,      0x000080FF },
    { "Calculator",   DESK_ICON_CALCULATOR, 0x00FF8000 },
    { "Terminal",     DESK_ICON_TERMINAL,   0x00000000 },
    { "File Manager", DESK_ICON_EXPLORER,   0x000080FF },
    { "Settings",     DESK_ICON_SETTINGS,   0x00808080 },
    { "Editor",       DESK_ICON_COUNT + 0,  0x008080FF },
    { "WuBu Canvas",  DESK_ICON_COUNT + 1,  0x000080FF },
    { "HolyC Term",   DESK_ICON_COUNT + 3,  0x00800080 },
    { "DOS Box",      DESK_ICON_COUNT + 4,  0x0000C000 },
    { "EDR Activity", DESK_ICON_COUNT + 5,  0x00C00000 },
    { "Bonzi Buddy",  DESK_ICON_COUNT + 6,  0x00A030A0 },
};
#define WIN11_PIN_COUNT ((int)(sizeof(g_win11_pins) / sizeof(g_win11_pins[0])))

static void startmenu_win11_layout(int fb_w, int fb_h, SmWin11 *L) {
    memset(L, 0, sizeof *L);
    int th = taskbar_height();
    L->pw = 520; L->ph = 560;
    if (L->pw > fb_w - 24) L->pw = fb_w - 24;
    L->px = (fb_w - L->pw) / 2;
    L->py = fb_h - th - L->ph - 12;
    if (L->py < 8) L->py = 8;

    L->search_x = L->px + 16; L->search_y = L->py + 12;
    L->search_w = L->pw - 32; L->search_h = 28;

    L->cols = 6; L->tile = 72; L->gap = 8; L->rows = 2;
    int grid_w = L->cols * L->tile + (L->cols - 1) * L->gap;
    L->grid_x = L->px + (L->pw - grid_w) / 2;
    L->grid_y = L->py + 72;

    L->rec_label_y = L->grid_y + L->rows * L->tile +
                     (L->rows - 1) * L->gap + 14;
    L->rec_cols = 2; L->rec_rows = 3;
    L->rec_item_w = (L->pw - 32 - 8) / 2;
    L->rec_item_h = 44;
    L->rec_x = L->px + 16;
    L->rec_y = L->rec_label_y + 16;

    L->foot_y = L->py + L->ph - 44;
    L->power_w = 36; L->power_h = 28;
    L->power_x = L->px + L->pw - 16 - L->power_w;
    L->power_y = L->foot_y + 8;
}

static void startmenu_render_win11(uint32_t *fb, int fb_w, int fb_h) {
    const WubuThemeColors *c = tc();
    SmWin11 L;
    startmenu_win11_layout(fb_w, fb_h, &L);

    vbe_fill_rect_rounded(L.px, L.py, L.pw, L.ph, 10, c->startmenu_bg);
    vbe_rect_rounded(L.px, L.py, L.pw, L.ph, 10, c->border_light);

    /* Search bar. */
    vbe_fill_rect_rounded(L.search_x, L.search_y, L.search_w, L.search_h,
                          6, c->btn_face);
    vbe_rect_rounded(L.search_x, L.search_y, L.search_w, L.search_h,
                     6, c->border_light);
    if (g_search.active && g_search.query[0]) {
        char disp[DOSGUI_SEARCH_MAX_LEN + 2];
        snprintf(disp, sizeof(disp), "%s|", g_search.query);
        vbe_draw_text(L.search_x + 10, L.search_y + 8, disp,
                      c->startmenu_text, 1);
    } else {
        vbe_draw_text(L.search_x + 10, L.search_y + 8, "Type here to search",
                      c->startmenu_text, 1);
    }

    /* While a query is typed the results replace the pinned grid, exactly
     * as Windows 11 does. */
    SmProgramEntry **results = NULL;
    int nres = 0;
    bool searching = (g_search.active && g_search.query[0] &&
                      startmenu_search_get_results(&results, &nres) == 0);
    if (searching) {
        int ry = L.search_y + L.search_h + 14;
        vbe_draw_text(L.px + 16, ry, nres ? "Best match" : "No results",
                      c->startmenu_text, 1);
        ry += 18;
        int shown = nres < 8 ? nres : 8;
        for (int i = 0; i < shown; i++) {
            int rx = L.px + 16;
            vbe_fill_rect_rounded(rx, ry, L.pw - 32, 40, 6, c->btn_face);
            vbe_rect_rounded(rx, ry, L.pw - 32, 40, 6, c->border_light);
            vbe_draw_text(rx + 12, ry + 16, results[i]->name,
                          c->startmenu_text, 1);
            ry += 46;
        }
    } else {

    /* Pinned grid. */
    vbe_draw_text(L.px + 16, L.grid_y - 16, "Pinned", c->startmenu_text, 1);
    for (int i = 0; i < WIN11_PIN_COUNT; i++) {
        const Win11Pin *d = &g_win11_pins[i];
        int col = i % L.cols, row = i / L.cols;
        int tx = L.grid_x + col * (L.tile + L.gap);
        int ty = L.grid_y + row * (L.tile + L.gap);
        vbe_fill_rect_rounded(tx, ty, L.tile, L.tile, 8, c->btn_face);
        vbe_rect_rounded(tx, ty, L.tile, L.tile, 8, c->border_light);
        wm_draw_icon_glyph(d->icon_type, tx + 20, ty + 6,
                           d->icon_color ? d->icon_color : c->btn_hover,
                           c->border_light, c->border_dark, c->btn_hover);
        vbe_draw_text(tx + 2, ty + 46, d->name, c->startmenu_text, 1);
    }

    /* Recommended: recent programs, two columns. */
    vbe_draw_text(L.px + 16, L.rec_label_y, "Recommended",
                  c->startmenu_text, 1);
    int n = g_recent_count < 6 ? g_recent_count : 6;
    for (int i = 0; i < n; i++) {
        int col = i % L.rec_cols, row = i / L.rec_cols;
        int ix = L.rec_x + col * (L.rec_item_w + 8);
        int iy = L.rec_y + row * (L.rec_item_h + 4);
        vbe_fill_rect_rounded(ix, iy, L.rec_item_w, L.rec_item_h, 6,
                              c->btn_face);
        vbe_rect_rounded(ix, iy, L.rec_item_w, L.rec_item_h, 6,
                         c->border_light);
        vbe_draw_text(ix + 8, iy + 16, g_recent[i].name,
                      c->startmenu_text, 1);
    }
    }   /* !searching */

    /* Footer: user chip + power. */
    vbe_fill_rect_rounded(L.px + 16, L.foot_y + 4, 32, 32, 16, c->btn_hover);
    vbe_draw_text(L.px + 56, L.foot_y + 14, "WuBu User", c->startmenu_text, 1);
    vbe_fill_rect_rounded(L.power_x, L.power_y, L.power_w, L.power_h, 6,
                          c->btn_face);
    vbe_rect_rounded(L.power_x, L.power_y, L.power_w, L.power_h, 6,
                     c->border_light);
    vbe_draw_text(L.power_x + 8, L.power_y + 8, "Power", c->startmenu_text, 1);
}

static void startmenu_win11_click(int x, int y) {
    SmWin11 L;
    startmenu_win11_layout(wm_screen_w(), wm_screen_h(), &L);

    /* A press outside the panel dismisses it (Win11 behaviour). */
    if (x < L.px || x >= L.px + L.pw || y < L.py || y >= L.py + L.ph) {
        startmenu_close();
        return;
    }
    /* While a query is typed, the results are the only clickable rows. */
    if (g_search.active && g_search.query[0]) {
        SmProgramEntry **results = NULL;
        int nres = 0;
        if (startmenu_search_get_results(&results, &nres) == 0) {
            int ry = L.search_y + L.search_h + 14 + 18;
            int shown = nres < 8 ? nres : 8;
            for (int i = 0; i < shown; i++) {
                if (x >= L.px + 16 && x < L.px + L.pw - 16 &&
                    y >= ry && y < ry + 40) {
                    startmenu_activate(results[i]);
                    startmenu_close();
                    return;
                }
                ry += 46;
            }
        }
        return;
    }

    /* Pinned grid. */
    for (int i = 0; i < WIN11_PIN_COUNT; i++) {
        int col = i % L.cols, row = i / L.cols;
        int tx = L.grid_x + col * (L.tile + L.gap);
        int ty = L.grid_y + row * (L.tile + L.gap);
        if (x >= tx && x < tx + L.tile && y >= ty && y < ty + L.tile) {
            launch_app(g_win11_pins[i].name);
            startmenu_close();
            return;
        }
    }
    /* Recommended list. */
    int n = g_recent_count < 6 ? g_recent_count : 6;
    for (int i = 0; i < n; i++) {
        int col = i % L.rec_cols, row = i / L.rec_cols;
        int ix = L.rec_x + col * (L.rec_item_w + 8);
        int iy = L.rec_y + row * (L.rec_item_h + 4);
        if (x >= ix && x < ix + L.rec_item_w &&
            y >= iy && y < iy + L.rec_item_h) {
            launch_app(g_recent[i].name);
            startmenu_close();
            return;
        }
    }
}

void startmenu_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w;
    if (!g_open) return;

    if (wubu_theme_current() == THEME_DARK) {
        startmenu_render_win11(fb, fb_w, fb_h);
        return;
    }

    const WubuTheme *t = th();
    int task_h = taskbar_height();
    int mh = menu_item_h();
    int mw = menu_w();
    int sw = sidebar_w();
    int menu_y = fb_h - task_h - (g_main_count * mh + 4);

    /* Main menu background */
    int main_h = g_main_count * mh + 4;
    
    if (t->Luna_start_button) {
        /* XP Style: blue panel + gradient sidebar (the iconic Luna look). */
        vbe_shade_rect(MENU_X + 2, menu_y + 2, mw, main_h);
        vbe_fill_rect_rounded(MENU_X, menu_y, mw, main_h, 4, tc()->startmenu_bg);
        vbe_rect_rounded(MENU_X, menu_y, mw, main_h, 4, tc()->border_dark);

        /* Sidebar: vertical gradient (blue top -> theme foot colour). This is
         * the signature XP chrome; flat on non-gradient themes. */
        vbe_vgradient(MENU_X, menu_y, sw, main_h,
                      tc()->startmenu_sidebar, tc()->startmenu_sidebar_grad_end);
        vbe_rect_rounded(MENU_X, menu_y, sw, main_h, 4, tc()->border_dark);

        /* Header banner: a small 4-pane orb logo + bold "WuBuOS" wordmark. */
        int hdr = 26;
        vbe_vgradient(MENU_X + 4, menu_y + 4, sw - 8, hdr - 4,
                      tc()->startmenu_sidebar, tc()->startmenu_sidebar_grad_end);
        /* Orb: four coloured quadrants (Windows-flag style) inside a 16x16 box. */
        int ox = MENU_X + 8, oy = menu_y + 8;
        vbe_fill_rect(ox,      oy,      7, 7, 0xF24C3E); /* red TL */
        vbe_fill_rect(ox + 8,  oy,      7, 7, 0x6FCF3C); /* green TR */
        vbe_fill_rect(ox,      oy + 8,  7, 7, 0x39A0EC); /* blue BL */
        vbe_fill_rect(ox + 8,  oy + 8,  7, 7, 0xFFC90E); /* yellow BR */
        vbe_draw_text(MENU_X + 8, menu_y + hdr + 2, "WuBuOS", 0xFFFFFF, 1);

        /* Footer: Log off / Turn Off (as in XP). */
        if (main_h > 80) {
            vbe_draw_text(MENU_X + 4, menu_y + main_h - 30, "Log off", 0xDDDDDD, 1);
            vbe_draw_text(MENU_X + 4, menu_y + main_h - 18, "Turn Off", 0xDDDDDD, 1);
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

void startmenu_handle_click(int x, int y) {
    if (!g_open) return;

    if (wubu_theme_current() == THEME_DARK) {
        startmenu_win11_click(x, y);
        return;
    }

    int task_h = taskbar_height();
    int mh = menu_item_h();
    int mw = menu_w();
    int sw = sidebar_w();
    int menu_y = wm_screen_h() - task_h - (g_main_count * mh + 4);

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
                            launch_app(item->app_name);
                            startmenu_close();
                        }
                    }
                    return;
                }
            }
        }
        /* Clicked outside menu -- close */
        startmenu_close();
        return;
    }

    /* Check if click in sidebar (XP) */
    if (th()->Luna_start_button && x < content_x) {
        /* Click in sidebar - handle logoff/shutdown */
        startmenu_close();
        return;
    }

    int idx = (y - menu_y - 2) / mh;
    if (idx < 0 || idx >= g_main_count) return;

    MainMenuItem *mi = &g_main_items[idx];
    if (mi->type == 2) return; /* Separator */
    if (mi->type == 4) {
        /* Toggle companion (Bonzi Buddy) on/off. */
        wubu_bonzi_set_enabled(!wubu_bonzi_is_enabled());
        startmenu_close();
        return;
    }
    if (mi->type == 1) {
        /* Toggle submenu */
        g_submenu_open = (g_submenu_open == mi->submenu_id) ? -1 : mi->submenu_id;
    } else if (mi->type == 3) {
        /* Shutdown */
        wubu_shutdown();
        startmenu_close();
    } else {
        /* Launch app */
        launch_app(mi->app_name);
        startmenu_close();
    }
}

/* -- Hover tracking (called from WM mouse move) ----------------- */
void startmenu_track_hover(int x, int y) {
    if (!g_open) {
        g_hovered = -1;
        g_sub_hovered = -1;
        g_hover_submenu = -1;
        return;
    }

    int task_h = taskbar_height();
    int mh = menu_item_h();
    int mw = menu_w();
    int sw = sidebar_w();
    int menu_y = wm_screen_h() - task_h - (g_main_count * mh + 4);

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

void startmenu_build_programs_db(void) {
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

    e = &g_program_db.entries[g_program_db.count++];
    strcpy(e->name, "EDR Activity"); strcpy(e->executable, "EDR Activity");
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

void startmenu_render_search_bar(uint32_t *fb, int fb_w, int fb_h, int menu_x, int menu_y, int w) {
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

void startmenu_render_recent(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int max_items) {
    (void)fb; (void)fb_w; (void)fb_h;
    
    SmProgramEntry *entries[DOSGUI_MAX_RECENT];
    int count = startmenu_recent_get(entries, max_items);
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

void startmenu_render_all_programs(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h) {
    (void)fb; (void)fb_w; (void)fb_h; (void)w; (void)h;
    startmenu_tree_render(fb, fb_w, fb_h, x, y);
}


/* -- Input Handling ----------------------------------------------- */

void startmenu_handle_key(int key, uint32_t mods) {
    (void)mods;
    if (!g_open) return;
    
    if (key == 27) {
        if (g_search.active) {
            startmenu_search_clear();
        } else {
            startmenu_close();
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
            startmenu_search_update(g_search.query);
        }
    }
    else if (key == 13 && g_search.active) {
        if (g_search.result_count > 0) {
            launch_app(g_search.results[0]->name);
            startmenu_close();
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
                    launch_app(g_accessories.items[g_sub_hovered].app_name);
                    startmenu_close();
                }
                else if (submenu_id >= 0 && submenu_id == 1 && g_sub_hovered < g_wubuos.item_count) {
                    launch_app(g_wubuos.items[g_sub_hovered].app_name);
                    startmenu_close();
                }
                else if (submenu_id >= 0 && submenu_id == 2 && g_sub_hovered < g_system.item_count) {
                    if (g_sub_hovered == g_system.item_count - 1) { /* Shutdown */
                        wubu_shutdown();
                    } else {
                        launch_app(g_system.items[g_sub_hovered].app_name);
                    }
                    startmenu_close();
                }
            }
            else if (g_hovered >= 0 && g_hovered < g_main_count) {
                int type = g_main_items[g_hovered].type;
                if (type == 0) { /* App */
                    launch_app(g_main_items[g_hovered].app_name);
                    startmenu_close();
                }
                else if (type == 3) { /* Shutdown */
                    wubu_shutdown();
                    startmenu_close();
                }
                else if (type == 1) { /* Submenu */
                    g_submenu_open = g_hovered;
                    g_sub_hovered = 0;
                }
            }
        }
    }
}

void startmenu_handle_search_input(int key, uint32_t mods) {
    (void)mods;
    if (!g_search.active) return;
    
    if (key >= 32 && key < 127 && g_search.cursor_pos < DOSGUI_SEARCH_MAX_LEN - 1) {
        g_search.query[g_search.cursor_pos++] = (char)key;
        g_search.query[g_search.cursor_pos] = '\0';
        startmenu_search_update(g_search.query);
    }
}

void startmenu_init(void) {
    startmenu_init_enhanced();
}

void startmenu_init_enhanced(void) {
    startmenu_build_main_menu();
    startmenu_build_submenus();
    startmenu_build_programs_db();
    startmenu_search_init();
    startmenu_tree_build();
    /* Register the "one app per computing era" set, tagging each with its VSL
     * syscall personality so the shell can prove every OS personality is
     * reachable as a first-class process. (CP/M 0xC0, Mac 0xB0, etc.) */
    era_apps_register();
}

void startmenu_shutdown(void) {
    wubu_shutdown();
}

void startmenu_recent_add(const char *app_name);


