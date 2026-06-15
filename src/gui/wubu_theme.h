/*
 * wubu_theme.h  --  WuBuOS Theme Engine
 *
 * Cell 394: Multi-theme support  --  Win98 Classic, XP Luna Blue,
 * XP Media Center Orange/Black, and custom.
 *
 * Every theme defines a complete set of colors + drawing callbacks.
 * The theme engine switches at runtime (no recompile).
 *
 * Themes:
 *   - WIN98_CLASSIC:    Teal desktop, 3D raised/sunken borders, navy title
 *   - XP_LUNA_BLUE:     Blue gradient title, rounded buttons, Luna feel
 *   - XP_MEDIA_ORANGE:  Orange/black, Media Center special edition
 *   - WUBU_CUSTOM:      Green-tinted, WuBuOS branded
 */
#ifndef WUBU_THEME_H
#define WUBU_THEME_H

#include <stdint.h>
#include <stdbool.h>

/* -- Theme IDs ---------------------------------------------------- */

typedef enum {
    THEME_WIN98_CLASSIC   = 0,
    THEME_XP_LUNA_BLUE   = 1,
    THEME_XP_MEDIA_ORANGE = 2,
    THEME_WUBU_CUSTOM     = 3,
    THEME_COUNT           = 4,
} WubuThemeId;

/* -- Theme Color Set ---------------------------------------------- */

typedef struct {
    /* Desktop */
    uint32_t desktop_bg;

    /* Window chrome */
    uint32_t win_face;           /* Window body background */
    uint32_t win_title_active;   /* Active window title bar */
    uint32_t win_title_inactive; /* Inactive window title bar */
    uint32_t win_title_text;     /* Title text color */
    uint32_t win_title_text_ina; /* Inactive title text */

    /* Borders */
    uint32_t border_light;       /* 3D highlight (top-left) */
    uint32_t border_face;        /* 3D face */
    uint32_t border_dark;        /* 3D shadow (bottom-right) */
    uint32_t border_darkest;     /* 3D outer shadow */

    /* Buttons */
    uint32_t btn_face;
    uint32_t btn_hover;
    uint32_t btn_pressed;
    uint32_t btn_text;

    /* Taskbar */
    uint32_t taskbar_bg;
    uint32_t taskbar_border;
    uint32_t start_btn_face;
    uint32_t start_btn_text;

    /* Start menu */
    uint32_t startmenu_bg;
    uint32_t startmenu_sidebar;
    uint32_t startmenu_hover;
    uint32_t startmenu_text;

    /* Selection / highlights */
    uint32_t select_bg;
    uint32_t select_text;

    /* Scrollbar */
    uint32_t scroll_track;
    uint32_t scroll_thumb;

    /* Desktop icons */
    uint32_t icon_text;
    uint32_t icon_text_shadow;
} WubuThemeColors;

/* -- Theme Gradient (for XP-style title bars) --------------------- */

typedef struct {
    uint32_t color_start;
    uint32_t color_end;
    bool     active;       /* Gradient only for active window? */
} WubuThemeGradient;

/* -- Complete Theme ----------------------------------------------- */

typedef struct {
    WubuThemeId       id;
    char              name[32];
    WubuThemeColors   colors;
    WubuThemeGradient title_gradient;     /* XP: blue gradient */
    WubuThemeGradient title_gradient_ina; /* XP: blue gradient inactive */
    bool              rounded_buttons;    /* XP: rounded vs 98: square */
    bool              gradient_title;     /* XP: gradient vs 98: flat */
    bool              Luna_start_button;  /* XP: green Start orb */
} WubuTheme;

/* -- Theme Engine API --------------------------------------------- */

/* Initialize theme engine (default: Win98 Classic) */
int  wubu_theme_init(void);
void wubu_theme_shutdown(void);

/* Get/set current theme */
WubuThemeId wubu_theme_current(void);
void wubu_theme_set(WubuThemeId id);

/* Get current theme colors (for rendering) */
const WubuThemeColors *wubu_theme_colors(void);

/* Get full theme struct */
const WubuTheme *wubu_theme_get(void);

/* Cycle to next theme */
void wubu_theme_cycle(void);

/* Get theme name */
const char *wubu_theme_name(WubuThemeId id);

#endif /* WUBU_THEME_H */
