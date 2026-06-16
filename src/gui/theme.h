/*
 * theme.h  --  WuBuOS GUI Theme System
 *
 * WinXP Classic / Luna / Win2000 / Win98 / High Contrast themes.
 * Pure C11, no dependencies. All colors ARGB8888.
 */

#ifndef WUBUOS_THEME_H
#define WUBUOS_THEME_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Theme Identifiers
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    THEME_XP_CLASSIC = 0,   /* Windows XP Classic (Win2000 look) */
    THEME_XP_LUNA_BLUE,     /* Windows XP Luna (blue) */
    THEME_XP_LUNA_SILVER,   /* Windows XP Luna (silver) */
    THEME_XP_LUNA_OLIVE,    /* Windows XP Luna (olive green) */
    THEME_WIN2000,          /* Windows 2000 */
    THEME_WIN98,            /* Windows 98 */
    THEME_HIGH_CONTRAST,    /* High Contrast #1 (white on black) */
    THEME_COUNT
} ThemeId;

/* ═══════════════════════════════════════════════════════════════════
 * Color Roles (semantic, mapped per-theme)
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    /* Window frame */
    COLOR_WIN_FRAME_ACTIVE,
    COLOR_WIN_FRAME_INACTIVE,
    COLOR_WIN_TITLE_ACTIVE,
    COLOR_WIN_TITLE_INACTIVE,
    COLOR_WIN_TITLE_TEXT_ACTIVE,
    COLOR_WIN_TITLE_TEXT_INACTIVE,
    COLOR_WIN_CLIENT,           /* Window background */
    COLOR_WIN_BORDER_LIGHT,     /* 3D raised light */
    COLOR_WIN_BORDER_HILIGHT,   /* 3D raised highlight */
    COLOR_WIN_BORDER_SHADOW,    /* 3D sunken shadow */
    COLOR_WIN_BORDER_DKSHADOW,  /* 3D sunken dark shadow */

    /* Buttons / Controls */
    COLOR_BTN_FACE,             /* Button background */
    COLOR_BTN_HILIGHT,          /* Button highlight (top-left) */
    COLOR_BTN_LIGHT,            /* Button light (inner top-left) */
    COLOR_BTN_SHADOW,           /* Button shadow (bottom-right) */
    COLOR_BTN_DKSHADOW,         /* Button dark shadow (outer bottom-right) */
    COLOR_BTN_TEXT,             /* Button text */
    COLOR_BTN_HOT,              /* Button hot-track (hover) */
    COLOR_BTN_PRESSED,          /* Button pressed */

    /* Input fields */
    COLOR_WINDOW,               /* Edit/list background */
    COLOR_WINDOW_TEXT,          /* Edit/list text */
    COLOR_WINDOW_FRAME,         /* Edit/list border */
    COLOR_HIGHLIGHT,            /* Selection background */
    COLOR_HIGHLIGHT_TEXT,       /* Selection text */
    COLOR_HOTLIGHT,             /* Hot-track text (links) */

    /* Desktop / Taskbar */
    COLOR_DESKTOP,              /* Desktop background */
    COLOR_TASKBAR,              /* Taskbar background */
    COLOR_TASKBAR_TEXT,         /* Taskbar clock/text */
    COLOR_START_BTN,            /* Start button face */
    COLOR_START_BTN_HOT,        /* Start button hover */
    COLOR_START_BTN_PRESSED,    /* Start button pressed */
    COLOR_QUICKLAUNCH,          /* Quick Launch background */

    /* Menu */
    COLOR_MENU,                 /* Menu background */
    COLOR_MENU_TEXT,            /* Menu text */
    COLOR_MENU_HILIGHT,         /* Menu selection background */
    COLOR_MENU_HILIGHT_TEXT,    /* Menu selection text */
    COLOR_MENU_BAR,             /* Menu bar background */
    COLOR_MENU_BORDER,          /* Menu border */

    /* Tooltip */
    COLOR_TOOLTIP,              /* Tooltip background */
    COLOR_TOOLTIP_TEXT,         /* Tooltip text */

    /* Scrollbar */
    COLOR_SCROLLBAR,            /* Scrollbar trough */
    COLOR_SCROLLBAR_THUMB,      /* Scrollbar thumb */
    COLOR_SCROLLBAR_ARROW,      /* Scrollbar arrows */

    /* Focus */
    COLOR_FOCUS_RECT,           /* Focus rectangle (dotted) */

    /* Disabled */
    COLOR_GRAYTEXT,             /* Disabled text */

    COLOR_COUNT
} ColorRole;

/* ═══════════════════════════════════════════════════════════════════
 * System Metrics (themeable sizes)
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    METRIC_BORDER_WIDTH,        /* Window frame border */
    METRIC_CAPTION_HEIGHT,      /* Title bar height */
    METRIC_SMCAPTION_HEIGHT,    /* Small caption (tool window) */
    METRIC_MENU_HEIGHT,         /* Menu bar item height */
    METRIC_SM_MENU_HEIGHT,      /* Popup menu item height */
    METRIC_BUTTON_WIDTH,        /* Min button width */
    METRIC_BUTTON_HEIGHT,       /* Standard button height */
    METRIC_SCROLL_WIDTH,        /* Scrollbar width */
    METRIC_ICON_WIDTH,          /* Desktop icon width */
    METRIC_ICON_HEIGHT,         /* Desktop icon height */
    METRIC_ICON_SPACING_X,      /* Desktop icon grid X */
    METRIC_ICON_SPACING_Y,      /* Desktop icon grid Y */
    METRIC_ICON_LABEL_WRAP,     /* Icon label max width */
    METRIC_EDGE_THICKNESS,      /* 3D edge thickness */
    METRIC_TASKBAR_HEIGHT,      /* Taskbar height */
    METRIC_START_BUTTON_WIDTH,  /* Start button width */
    METRIC_TOOLTIP_PADDING,     /* Tooltip internal padding */

    METRIC_COUNT
} MetricId;

/* ═══════════════════════════════════════════════════════════════════
 * Theme Definition Structure
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t colors[COLOR_COUNT];
    int      metrics[METRIC_COUNT];
    const char *name;
    const char *description;
    bool     uses_gradients;    /* Luna themes use gradient title bars */
    bool     uses_rounded;      /* Luna themes have rounded corners */
} Theme;

/* ═══════════════════════════════════════════════════════════════════
 * Theme API
 * ═══════════════════════════════════════════════════════════════════ */

/* Initialize theme system (loads defaults) */
void theme_init(void);

/* Set active theme by ID */
void theme_set(ThemeId id);

/* Get active theme ID */
ThemeId theme_get(void);

/* Get active theme pointer */
const Theme *theme_current(void);

/* Get color by role (with current theme) */
uint32_t theme_color(ColorRole role);

/* Get metric by ID (with current theme) */
int theme_metric(MetricId id);

/* Get color by role from specific theme */
uint32_t theme_color_ex(const Theme *t, ColorRole role);

/* Get metric by ID from specific theme */
int theme_metric_ex(const Theme *t, MetricId id);

/* Blend two colors (alpha over) */
uint32_t theme_blend(uint32_t fg, uint32_t bg, uint8_t alpha);

/* Darken/lighten a color */
uint32_t theme_darken(uint32_t color, int percent);
uint32_t theme_lighten(uint32_t color, int percent);

/* Draw 3D raised edge with current theme colors */
void theme_draw_raised(int x, int y, int w, int h, int thickness);

/* Draw 3D sunken edge with current theme colors */
void theme_draw_sunken(int x, int y, int w, int h, int thickness);

/* Draw XP-style button (supports normal/hot/pressed/disabled) */
typedef enum {
    BTN_STATE_NORMAL = 0,
    BTN_STATE_HOT,
    BTN_STATE_PRESSED,
    BTN_STATE_DISABLED,
    BTN_STATE_DEFAULT,    /* Default button (pulsing/heavy border) */
} BtnState;

void theme_draw_button(int x, int y, int w, int h,
                       const char *text, BtnState state,
                       bool focused, bool default_btn);

/* Draw XP-style checkbox */
typedef enum {
    CHK_UNCHECKED = 0,
    CHK_CHECKED,
    CHK_MIXED,        /* Indeterminate */
} ChkState;

void theme_draw_checkbox(int x, int y, int size,
                         ChkState state, BtnState btn_state);

/* Draw XP-style radio button */
void theme_draw_radio(int x, int y, int size,
                      bool checked, BtnState btn_state);

/* Draw scrollbar (vertical or horizontal) */
typedef enum {
    SB_VERT = 0,
    SB_HORZ,
} SbOrient;

typedef struct {
    int track_x, track_y, track_w, track_h;
    int thumb_x, thumb_y, thumb_w, thumb_h;
    int btn_up_x, btn_up_y, btn_up_w, btn_up_h;
    int btn_down_x, btn_down_y, btn_down_w, btn_down_h;
    int page_up_x, page_up_y, page_up_w, page_up_h;
    int page_down_x, page_down_y, page_down_w, page_down_h;
    bool show_up, show_down;
    bool thumb_hot, thumb_pressed;
    bool up_hot, up_pressed, down_hot, down_pressed;
} ScrollbarLayout;

void theme_calc_scrollbar_layout(SbOrient orient,
                                  int x, int y, int w, int h,
                                  int total, int page, int pos,
                                  ScrollbarLayout *out);

void theme_draw_scrollbar(const ScrollbarLayout *layout);

/* Draw menu item */
typedef enum {
    MENU_ITEM_NORMAL = 0,
    MENU_ITEM_SELECTED,
    MENU_ITEM_DISABLED,
    MENU_ITEM_SEPARATOR,
    MENU_ITEM_SUBMENU,    /* Has submenu arrow */
} MenuItemState;

void theme_draw_menu_item(int x, int y, int w, int h,
                          const char *text, const char *shortcut,
                          bool has_check, bool checked,
                          MenuItemState state);

/* Draw tooltip */
void theme_draw_tooltip(int x, int y, int w, int h, const char *text);

/* Draw focus rectangle (dotted) */
void theme_draw_focus_rect(int x, int y, int w, int h);

/* Draw XP-style window close button */
typedef enum {
    CAP_BTN_CLOSE = 0,
    CAP_BTN_MAXIMIZE,
    CAP_BTN_MINIMIZE,
    CAP_BTN_HELP,
    CAP_BTN_RESTORE,
} CapBtnId;

typedef enum {
    CAP_BTN_NORMAL = 0,
    CAP_BTN_HOT,
    CAP_BTN_PRESSED,
    CAP_BTN_DISABLED,
} CapBtnState;

void theme_draw_caption_button(int x, int y, int w, int h,
                               CapBtnId id, CapBtnState state,
                               bool active_window);

/* Draw Start button (XP style: green orb for Luna, rectangular for Classic) */
void theme_draw_start_button(int x, int y, int w, int h,
                             BtnState state);

/* Gradient fill (vertical) for Luna title bars */
void theme_gradient_v(int x, int y, int w, int h,
                      uint32_t top, uint32_t bottom);

/* System metrics helpers */
int theme_cx_screen(void);
int theme_cy_screen(void);

/* Font metrics (uses VBE 8x8 font scaled) */
int theme_font_height(int scale);
int theme_font_width(int scale, const char *text);

/* Debug: dump current theme */
void theme_dump(void);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOS_THEME_H */