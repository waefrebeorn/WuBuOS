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

#include "wubu_gui_metrics.h"
#include "../../egojar.h"
#include "../../egojar_net.h"
#include <sys/socket.h>
#include "./startmenu_internal.h"
#include "./startmenu.h"
#include "../era_apps.h"
#include "./shell_session.h"
#include "../wm.h"
#include "../explorer_internal.h"
#include "../wm_internal.h"
#include "../../apps/apps.h"
#include "../../kernel/vbe.h"
#include "../../gui/wubu_theme.h"
#include "../../gui/wubu_mime.h"
#include "../../gui/buddy_sprite.h"
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
    return WUBU_MENU_ROW_H;
}

static inline int submenu_w(void) {
    return WUBU_SUBMENU_W;
}

static inline int menu_w(void) {
    return WUBU_MENU_W;
}

static inline int sidebar_w(void) {
    return WUBU_START_SIDEBAR_W;
}

static inline int menu_border_w(void) {
    return WUBU_MENU_BORDER_W;
}

/* Chrome metrics come from the explorer owner (../explorer_internal.h);
 * this file used to hardcode 22/28/24/24 where the owner says 22/36/28/22,
 * so the shell band and the file manager disagreed. */



/* -- Global Submenu State ------------------------------------------ */

SmSubmenu g_programs = {0};
SmSubmenu g_accessories = {0};
SmSubmenu g_wubuos = {0};
SmSubmenu g_system = {0};

MainMenuItem g_main_items[16] = {0};
int g_main_count = 0;

int g_open = 0;
/* Where the Start mark was actually drawn, so a test measures the mark and not "some accent
 * pixels somewhere on the menu". Render and test share this one expression. */
static int g_logo_x, g_logo_y, g_logo_w, g_logo_h;
void startmenu_logo_rect(int *x, int *y, int *w, int *h) {
    if (x) *x = g_logo_x;
    if (y) *y = g_logo_y;
    if (w) *w = g_logo_w;
    if (h) *h = g_logo_h;
}
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

#define MENU_X WUBU_METRIC_MARGIN_XS

static void draw_menu_item(int x, int y, int w, const char *label,
                            int hovered, int has_submenu, int type) {
    int mh = menu_item_h();
    uint32_t bg, fg;

    /* The theme engine already answers for the CURRENT theme. The old
     * Luna/classic ternaries duplicated the classic palette as literals,
     * bypassing /theme writes, dynamic colour and high-contrast. Item text
     * is btn_text: the old code used startmenu_text (the SIDEBAR colour),
     * which is white-on-light under XP — unreadable. */
    if (hovered) {
        bg = tc()->select_bg;
        fg = tc()->select_text;
    } else {
        bg = tc()->startmenu_bg;
        fg = tc()->btn_text;
    }

    if (type == 3) { /* Shutdown */
        bg = tc()->startmenu_shutdown_bg;
        fg = tc()->startmenu_shutdown_fg;
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
    int label_x = x + WUBU_MENU_ITEM_PAD_X;
    if (type == 4) {
        /* Checkbox: [x] when companion enabled, [ ] when disabled. */
        int cy = y + (mh - WUBU_MENU_CHECK_H) / 2;
        vbe_fill_rect(label_x, cy, WUBU_MENU_CHECK_W, WUBU_MENU_CHECK_H,
                      WUBU_MENU_CHECK_BG);
        vbe_rect(label_x, cy, WUBU_MENU_CHECK_W, WUBU_MENU_CHECK_H,
                 WUBU_MENU_CHECK_FG);
        if (wubu_bonzi_is_enabled())
            vbe_fill_rect(label_x + WUBU_SPACE_2XS, cy + WUBU_SPACE_2XS,
                          WUBU_MENU_CHECK_MARK_W, WUBU_MENU_CHECK_MARK_H,
                          WUBU_MENU_CHECK_FG);
        label_x += WUBU_SPACE_LG;
    }
    int max_text_w = w - WUBU_MENU_ITEM_TEXT_RESERVE;
    if (has_submenu) max_text_w -= WUBU_SPACE_LG;
    int text_w = vbe_text_width(label, 1);
    if (text_w > max_text_w) {
        char truncated[64];
        int len = strlen(label);
        strncpy(truncated, label, len);
        truncated[len] = '\0';
        while (len > 0 && vbe_text_width(truncated, 1) > max_text_w - vbe_text_width("...", 1)) {
            len--;
            truncated[len] = '\0';
        }
        if (len > 0) {
            strncpy(truncated + len, "...", 3);
            truncated[len + 3] = '\0';
        } else {
            strcpy(truncated, "...");
        }
        vbe_draw_text(label_x, y + (mh - WUBU_FONT_INK_H) / 2, truncated, fg, 1);
    } else {
        vbe_draw_text(label_x, y + (mh - WUBU_FONT_INK_H) / 2, label, fg, 1);
    }
    
    if (has_submenu) {
        /* Arrow indicator */
        int arrow_x = x + w - WUBU_SPACE_LG;
        int arrow_y = y + (mh - WUBU_FONT_INK_H) / 2;
        vbe_draw_text(arrow_x, arrow_y, ">", fg, 1);
    }
}

static void draw_submenu(int x, int y, SmSubmenu *sub) {
    int mh = menu_item_h();
    int sw = submenu_w();
    int h = sub->item_count * mh + WUBU_SPACE_XS;
    int rad = th()->rounded_buttons ? WUBU_METRIC_RADIUS_CONTROL : 0;

    /* Submenu background with shadow */
    vbe_shade_rect(x + WUBU_SPACE_2XS, y + WUBU_SPACE_2XS, sw, h);
    
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
        int iy = y + WUBU_SPACE_2XS + i * mh;
        if (item->type == SM_ITEM_SEPARATOR) {
            int sep_y = iy + mh / 2;
            int inset = rad > 0 ? WUBU_SPACE_SM : WUBU_SPACE_XS;
            vbe_hline(x + inset, x + sw - inset, sep_y, tc()->border_dark);
        } else {
            bool hovered = (g_submenu_open >= 0 && g_sub_hovered == i && g_hover_submenu == g_submenu_open);
            uint32_t bg = hovered ? tc()->select_bg : tc()->startmenu_bg;
            uint32_t fg = hovered ? tc()->select_text : tc()->btn_text;
            
            if (rad > 0) {
                vbe_fill_rect_rounded(x + WUBU_SPACE_2XS, iy, sw - 2 * WUBU_SPACE_2XS, mh, WUBU_RADIUS_XS, bg);
                if (hovered) vbe_3d_raised_rounded_colors(x + WUBU_SPACE_2XS, iy, sw - 2 * WUBU_SPACE_2XS, mh, WUBU_RADIUS_XS,
                    tc()->border_light, tc()->border_face, tc()->border_dark, tc()->border_darkest);
            } else {
                vbe_fill_rect(x + WUBU_SPACE_2XS, iy, sw - 2 * WUBU_SPACE_2XS, mh, bg);
                if (hovered) vbe_3d_raised_colors(x + WUBU_SPACE_2XS, iy, sw - 2 * WUBU_SPACE_2XS, mh,
                                                   tc()->border_light, tc()->border_face,
                                                   tc()->border_dark, tc()->border_darkest);
            }
            vbe_draw_text(x + WUBU_SPACE_SM, iy + (mh - WUBU_FONT_INK_H) / 2, item->label, fg, 1);
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
    { "Editor",       DESK_ICON_COUNT + 0,  0x008080FF }, /* icon-identity */
    { "WuBu Canvas",  DESK_ICON_COUNT + 1,  0x000080FF }, /* icon-identity */
    { "HolyC Term",   DESK_ICON_COUNT + 3,  0x00800080 }, /* icon-identity */
    { "DOS Box",      DESK_ICON_COUNT + 4,  0x0000C000 }, /* icon-identity */
    { "EDR Activity", DESK_ICON_COUNT + 5,  0x00C00000 }, /* icon-identity */
    { "Bonzi Buddy",  DESK_ICON_COUNT + 6,  0x00A030A0 }, /* icon-identity */
};
#define WIN11_PIN_COUNT ((int)(sizeof(g_win11_pins) / sizeof(g_win11_pins[0])))

static void startmenu_win11_layout(int fb_w, int fb_h, SmWin11 *L) {
    memset(L, 0, sizeof *L);
    int th = taskbar_height();
    L->pw = WUBU_START_W; L->ph = WUBU_START_H;
    if (L->pw > fb_w - WUBU_SPACE_XL) L->pw = fb_w - WUBU_SPACE_XL;
    L->px = (fb_w - L->pw) / 2;
    L->py = fb_h - th - L->ph - WUBU_SPACE_MD;
    if (L->py < WUBU_SPACE_SM) L->py = WUBU_SPACE_SM;

    L->search_x = L->px + WUBU_SPACE_LG; L->search_y = L->py + WUBU_SPACE_MD;
    L->search_w = L->pw - 2 * WUBU_SPACE_LG; L->search_h = WUBU_START_SEARCH_H;

    L->cols = WUBU_START_COLS; L->tile = WUBU_START_TILE;
    L->gap = WUBU_SPACE_SM; L->rows = WUBU_START_ROWS;
    int grid_w = L->cols * L->tile + (L->cols - 1) * L->gap;
    L->grid_x = L->px + (L->pw - grid_w) / 2;
    L->grid_y = L->py + WUBU_START_GRID_DY;

    L->rec_label_y = L->grid_y + L->rows * L->tile +
                     (L->rows - 1) * L->gap + WUBU_START_REC_DY;
    L->rec_cols = 2; L->rec_rows = 3;
    L->rec_item_w = (L->pw - 2 * WUBU_SPACE_LG - WUBU_SPACE_SM) / 2;
    L->rec_item_h = WUBU_START_REC_ITEM_H;
    L->rec_x = L->px + WUBU_SPACE_LG;
    L->rec_y = L->rec_label_y + WUBU_SPACE_LG;

    L->foot_y = L->py + L->ph - WUBU_START_FOOT_H;
    L->power_w = WUBU_START_POWER_W; L->power_h = WUBU_START_POWER_H;
    L->power_x = L->px + L->pw - WUBU_SPACE_LG - L->power_w;
    L->power_y = L->foot_y + WUBU_METRIC_GAP_SM;
}

static void startmenu_render_win11(uint32_t *fb, int fb_w, int fb_h) {
    const WubuThemeColors *c = tc();
    SmWin11 L;
    startmenu_win11_layout(fb_w, fb_h, &L);

    vbe_fill_rect_rounded(L.px, L.py, L.pw, L.ph, WUBU_METRIC_RADIUS_WINDOW, c->startmenu_bg);
    vbe_rect_rounded(L.px, L.py, L.pw, L.ph, WUBU_METRIC_RADIUS_WINDOW, c->border_light);

    /* Search bar. */
    vbe_fill_rect_rounded(L.search_x, L.search_y, L.search_w, L.search_h,
                          WUBU_METRIC_RADIUS_CONTROL, c->btn_face);
    vbe_rect_rounded(L.search_x, L.search_y, L.search_w, L.search_h,
                     WUBU_METRIC_RADIUS_CONTROL, c->border_light);
    if (g_search.active && g_search.query[0]) {
        char disp[DOSGUI_SEARCH_MAX_LEN + 2];
        snprintf(disp, sizeof(disp), "%s|", g_search.query);
        vbe_draw_text(L.search_x + WUBU_START_SEARCH_PAD_X,
                      L.search_y + WUBU_SPACE_SM, disp, c->startmenu_text, 1);
    } else {
        vbe_draw_text(L.search_x + WUBU_START_SEARCH_PAD_X,
                      L.search_y + WUBU_SPACE_SM, "Type here to search",
                      c->startmenu_text, 1);
    }

    /* While a query is typed the results replace the pinned grid, exactly
     * as Windows 11 does. */
    SmProgramEntry **results = NULL;
    int nres = 0;
    bool searching = (g_search.active && g_search.query[0] &&
                      startmenu_search_get_results(&results, &nres) == 0);
    if (searching) {
        int ry = L.search_y + L.search_h + WUBU_START_RESULT_DY;
        vbe_draw_text(L.px + WUBU_SPACE_LG, ry,
                      nres ? "Best match" : "No results", c->startmenu_text, 1);
        ry += WUBU_START_RESULT_LABEL_DY;
        int shown = nres < 8 ? nres : 8;
        for (int i = 0; i < shown; i++) {
            int rx = L.px + WUBU_SPACE_LG;
            vbe_fill_rect_rounded(rx, ry, L.pw - 2 * WUBU_SPACE_LG,
                                  WUBU_START_RESULT_H, WUBU_METRIC_RADIUS_CONTROL, c->btn_face);
            vbe_rect_rounded(rx, ry, L.pw - 2 * WUBU_SPACE_LG,
                             WUBU_START_RESULT_H, WUBU_METRIC_RADIUS_CONTROL, c->border_light);
            vbe_draw_text(rx + WUBU_SPACE_MD, ry + WUBU_SPACE_LG, results[i]->name,
                          c->startmenu_text, 1);
            ry += WUBU_START_RESULT_PITCH;
        }
    } else {

    /* Pinned grid. */
    vbe_draw_text(L.px + WUBU_SPACE_LG, L.grid_y - WUBU_SPACE_LG,
                  "Pinned", c->startmenu_text, 1);
    for (int i = 0; i < WIN11_PIN_COUNT; i++) {
        const Win11Pin *d = &g_win11_pins[i];
        int col = i % L.cols, row = i / L.cols;
        int tx = L.grid_x + col * (L.tile + L.gap);
        int ty = L.grid_y + row * (L.tile + L.gap);
        vbe_fill_rect_rounded(tx, ty, L.tile, L.tile, WUBU_METRIC_RADIUS_WINDOW, c->btn_face);
        vbe_rect_rounded(tx, ty, L.tile, L.tile, WUBU_METRIC_RADIUS_WINDOW, c->border_light);
        wm_draw_icon_glyph(d->icon_type, tx + (L.tile - WUBU_ICON_LG) / 2,
                           ty + WUBU_START_TILE_ICON_DY,
                           d->icon_color ? d->icon_color : c->btn_hover,
                           c->border_light, c->border_dark, c->btn_hover);
        vbe_draw_text(tx + WUBU_SPACE_2XS, ty + WUBU_START_TILE_LABEL_DY,
                      d->name, c->startmenu_text, 1);
    }

    /* Recommended: recent programs, two columns. */
    vbe_draw_text(L.px + WUBU_SPACE_LG, L.rec_label_y, "Recommended",
                  c->startmenu_text, 1);
    int n = g_recent_count < 6 ? g_recent_count : 6;
    for (int i = 0; i < n; i++) {
        int col = i % L.rec_cols, row = i / L.rec_cols;
        int ix = L.rec_x + col * (L.rec_item_w + WUBU_SPACE_SM);
        int iy = L.rec_y + row * (L.rec_item_h + WUBU_SPACE_XS);
        vbe_fill_rect_rounded(ix, iy, L.rec_item_w, L.rec_item_h,
                              WUBU_METRIC_RADIUS_CONTROL, c->btn_face);
        vbe_rect_rounded(ix, iy, L.rec_item_w, L.rec_item_h,
                         WUBU_METRIC_RADIUS_CONTROL, c->border_light);
        vbe_draw_text(ix + WUBU_SPACE_SM, iy + WUBU_SPACE_LG, g_recent[i].name,
                      c->startmenu_text, 1);
    }
    }   /* !searching */

    /* Footer: user chip + power. */
    int av = WUBU_SPACE_2XL;
    vbe_fill_rect_rounded(L.px + WUBU_SPACE_LG, L.foot_y + WUBU_SPACE_XS,
                          av, av, av / 2, c->btn_hover);
    vbe_draw_text(L.px + WUBU_SPACE_LG + av + WUBU_SPACE_SM,
                  L.foot_y + WUBU_SPACE_XS + (av - WUBU_FONT_INK_H) / 2,
                  "WuBu User", c->startmenu_text, 1);
    vbe_fill_rect_rounded(L.power_x, L.power_y, L.power_w, L.power_h,
                          WUBU_METRIC_RADIUS_CONTROL, c->btn_face);
    vbe_rect_rounded(L.power_x, L.power_y, L.power_w, L.power_h,
                     WUBU_METRIC_RADIUS_CONTROL, c->border_light);
    vbe_draw_text(L.power_x + WUBU_METRIC_GAP_SM, L.power_y + WUBU_METRIC_GAP_SM, "Power", c->startmenu_text, 1);
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
            int ry = L.search_y + L.search_h + WUBU_START_RESULT_DY +
                     WUBU_START_RESULT_LABEL_DY;
            int shown = nres < 8 ? nres : 8;
            for (int i = 0; i < shown; i++) {
                if (x >= L.px + WUBU_SPACE_LG &&
                    x < L.px + L.pw - WUBU_SPACE_LG &&
                    y >= ry && y < ry + WUBU_START_RESULT_H) {
                    startmenu_activate(results[i]);
                    startmenu_close();
                    return;
                }
                ry += WUBU_START_RESULT_PITCH;
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
        int ix = L.rec_x + col * (L.rec_item_w + WUBU_SPACE_SM);
        int iy = L.rec_y + row * (L.rec_item_h + WUBU_SPACE_XS);
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
    int menu_y = fb_h - task_h - (g_main_count * mh + WUBU_SPACE_XS);

    /* Main menu background */
    int main_h = g_main_count * mh + WUBU_SPACE_XS;
    
    if (t->Luna_start_button) {
        /* XP Style: blue panel + gradient sidebar (the iconic Luna look). */
        vbe_shade_rect(MENU_X + WUBU_SPACE_2XS, menu_y + WUBU_SPACE_2XS, mw, main_h);
        vbe_fill_rect_rounded(MENU_X, menu_y, mw, main_h, WUBU_METRIC_RADIUS_CONTROL, tc()->startmenu_bg);
        vbe_rect_rounded(MENU_X, menu_y, mw, main_h, WUBU_METRIC_RADIUS_CONTROL, tc()->border_dark);

        /* Sidebar: vertical gradient (blue top -> theme foot colour). This is
         * the signature XP chrome; flat on non-gradient themes. */
        vbe_vgradient(MENU_X, menu_y, sw, main_h,
                      tc()->startmenu_sidebar, tc()->startmenu_sidebar_grad_end);
        vbe_rect_rounded(MENU_X, menu_y, sw, main_h, WUBU_METRIC_RADIUS_CONTROL, tc()->border_dark);

        /* Header banner: a small 4-pane orb logo + bold "WuBuOS" wordmark. */
        int hdr = WUBU_START_SIDEBAR_HDR_H;
        vbe_vgradient(MENU_X + WUBU_SPACE_XS, menu_y + WUBU_SPACE_XS,
                      sw - 2 * WUBU_SPACE_XS, hdr - WUBU_SPACE_XS,
                      tc()->startmenu_sidebar, tc()->startmenu_sidebar_grad_end);
        /* Orb: four coloured quadrants (Windows-flag style) inside a 16x16
         * box. The quadrant colours are brand-art, not theme surface. */
        int ox = MENU_X + WUBU_SPACE_SM, oy = menu_y + WUBU_SPACE_SM;
        g_logo_x = ox; g_logo_y = oy; g_logo_w = 16; g_logo_h = 16;
        /* Four panes of ONE accent. This was Microsoft's red/green/blue/yellow flag -- a
         * borrowed OS logo, the same defect as the Win95 flag that used to sit on Start. */
        for (int pane = 0; pane < 4; pane++) {
            vbe_fill_rect(ox + (pane & 1) * 8, oy + (pane >> 1) * 8, 7, 7, tc()->select_bg);
        }
        vbe_draw_text(MENU_X + WUBU_SPACE_SM, menu_y + hdr + WUBU_SPACE_2XS,
                      "WuBuOS", tc()->startmenu_text, 1);

        /* Footer: Log off / Turn Off (as in XP). */
        if (main_h > WUBU_START_FOOT_MIN_H) {
            vbe_draw_text(MENU_X + WUBU_SPACE_XS, menu_y + main_h - WUBU_START_LOGOFF_DY,
                          "Log off", WUBU_START_SIDEBAR_TEXT_DIM, 1);
            vbe_draw_text(MENU_X + WUBU_SPACE_XS, menu_y + main_h - WUBU_START_POWEROFF_DY,
                          "Turn Off", WUBU_START_SIDEBAR_TEXT_DIM, 1);
        }
    } else {
        /* Win98 Style */
        vbe_fill_rect(MENU_X, menu_y, mw, main_h, tc()->startmenu_bg);
        vbe_3d_raised_colors(MENU_X, menu_y, mw, main_h,
                              tc()->border_light, tc()->border_face,
                              tc()->border_dark, tc()->border_darkest);
    }

    /* Main menu items */
    int content_x = MENU_X + sw + WUBU_SPACE_XS;
    int content_w = mw - sw - 2 * WUBU_SPACE_XS;

    for (int i = 0; i < g_main_count; i++) {
        MainMenuItem *mi = &g_main_items[i];
        int iy = menu_y + WUBU_SPACE_2XS + i * mh;

        if (mi->type == 2) {
            /* Separator */
            int sep_y = iy + mh / 2;
            int sx0 = t->rounded_buttons ? content_x : MENU_X;
            int sx1 = t->rounded_buttons ? content_x + content_w : MENU_X + mw;
            vbe_hline(sx0 + WUBU_SPACE_XS, sx1 - WUBU_SPACE_XS, sep_y, tc()->border_dark);
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
            int sy = menu_y + WUBU_SPACE_2XS + g_submenu_open * mh;
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
    int menu_y = wm_screen_h() - task_h - (g_main_count * mh + WUBU_SPACE_XS);

    int content_x = MENU_X + sw + WUBU_SPACE_XS;
    (void)content_x;

    /* Check main menu bounds */
    if (x < MENU_X || x >= MENU_X + mw ||
        y < menu_y || y >= menu_y + g_main_count * mh + WUBU_SPACE_XS) {
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
                int sy = menu_y + WUBU_SPACE_2XS + g_submenu_open * mh;
                int sh = sub->item_count * mh + WUBU_SPACE_XS;
                if (x >= sx && x < sx + submenu_w() && y >= sy && y < sy + sh) {
                    /* Click in submenu */
                    int idx = (y - sy - WUBU_SPACE_2XS) / mh;
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

    int idx = (y - menu_y - WUBU_SPACE_2XS) / mh;
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
    int menu_y = wm_screen_h() - task_h - (g_main_count * mh + WUBU_SPACE_XS);

    int content_x = MENU_X + sw + WUBU_SPACE_XS;
    int content_w = mw - sw - 2 * WUBU_SPACE_XS;

    /* Reset hover */
    g_hovered = -1;
    g_sub_hovered = -1;
    g_hover_submenu = -1;

    /* Main menu hover */
    if (x >= content_x && x < content_x + content_w &&
        y >= menu_y && y < menu_y + g_main_count * mh + WUBU_SPACE_XS) {
        int idx = (y - menu_y - WUBU_SPACE_2XS) / mh;
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
            int sy = menu_y + WUBU_SPACE_2XS + g_submenu_open * mh;
            int sh = sub->item_count * mh + WUBU_SPACE_XS;
            if (x >= sx && x < sx + submenu_w() && y >= sy && y < sy + sh) {
                g_hover_submenu = g_submenu_open;
                int idx = (y - sy - WUBU_SPACE_2XS) / mh;
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
    
    int bar_h = WUBU_START_SEARCH_H;
    int bar_y = menu_y - bar_h - WUBU_SPACE_2XS;
    int bar_w = w - WUBU_SPACE_SM;
    
    vbe_fill_rect_rounded(menu_x + WUBU_SPACE_XS, bar_y, bar_w, bar_h,
                          WUBU_METRIC_RADIUS_CONTROL, tc()->win_face);
    vbe_3d_sunken_rounded_colors(menu_x + WUBU_SPACE_XS, bar_y, bar_w, bar_h,
                                  WUBU_METRIC_RADIUS_CONTROL,
                                  tc()->border_light, tc()->border_face,
                                  tc()->border_dark, tc()->border_darkest);
    
    vbe_draw_text(menu_x + WUBU_START_SEARCH_PAD_X,
                  bar_y + (bar_h - WUBU_FONT_INK_H) / 2,
                  "Search", tc()->icon_text_shadow, 1);
    
    char display[DOSGUI_SEARCH_MAX_LEN + 8];
    snprintf(display, sizeof(display), "%s%c", g_search.query, 
             (g_search.active && (time(NULL) * 2) % 2) ? '_' : ' ');
    vbe_draw_text(menu_x + WUBU_START_SEARCH_QUERY_X,
                  bar_y + (bar_h - WUBU_FONT_INK_H) / 2,
                  display, tc()->win_title_text, 1);
}

void startmenu_render_recent(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int max_items) {
    (void)fb; (void)fb_w; (void)fb_h;
    
    SmProgramEntry *entries[DOSGUI_MAX_RECENT];
    int count = startmenu_recent_get(entries, max_items);
    if (count == 0) return;
    
    int item_h = WUBU_MENU_RECENT_ROW_H;
    int ty = y;
    
    vbe_draw_text(x, ty, "Recently Used", tc()->win_title_text, 1);
    ty += WUBU_MENU_RECENT_HDR_DY;
    
    for (int i = 0; i < count; i++) {
        SmProgramEntry *e = entries[i];
        vbe_fill_rect(x, ty, w, item_h, tc()->startmenu_bg);
        vbe_draw_text(x + WUBU_SPACE_SM, ty + (item_h - WUBU_FONT_INK_H) / 2,
                      e->name, tc()->startmenu_text, 1);
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


