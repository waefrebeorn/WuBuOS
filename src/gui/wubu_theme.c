/*
 * wubu_theme.c  --  WuBuOS Theme Engine Implementation
 *
 * Cell 394: Runtime-switchable themes.
 * Win98 Classic, XP Luna Blue, XP Media Center Orange/Black.
 */
#include "wubu_theme.h"
#include <string.h>
#include <stdbool.h>

/* -- XRGB8888 Helpers --------------------------------------------- */
#define RGB(r,g,b) ((uint32_t)((r)<<16 | (g)<<8 | (b)))

/* -- Predefined Theme Data ---------------------------------------- */

static const WubuTheme g_themes[THEME_COUNT] = {

/* -- Win98 Classic ----------------------------------------------- */
[THEME_WIN98_CLASSIC] = {
    .id = THEME_WIN98_CLASSIC,
    .name = "Win98 Classic",
    .colors = {
        .desktop_bg        = RGB(0x00,0x80,0x80),   /* Teal */
        .win_face          = RGB(0xC0,0xC0,0xC0),   /* Silver */
        .win_title_active  = RGB(0x00,0x00,0x80),   /* Navy */
        .win_title_inactive= RGB(0x80,0x80,0x80),   /* Gray */
        .win_title_text    = RGB(0xFF,0xFF,0xFF),   /* White */
        .win_title_text_ina= RGB(0xC0,0xC0,0xC0),   /* Silver */
        .border_light      = RGB(0xFF,0xFF,0xFF),   /* White */
        .border_face       = RGB(0xC0,0xC0,0xC0),   /* Silver */
        .border_dark       = RGB(0x80,0x80,0x80),   /* Gray */
        .border_darkest    = RGB(0x00,0x00,0x00),   /* Black */
        .btn_face          = RGB(0xC0,0xC0,0xC0),
        .btn_hover         = RGB(0xD4,0xD4,0xD4),
        .btn_pressed       = RGB(0xA0,0xA0,0xA0),
        .btn_text          = RGB(0x00,0x00,0x00),
        .taskbar_bg        = RGB(0xC0,0xC0,0xC0),
        .taskbar_border    = RGB(0x80,0x80,0x80),
        .start_btn_face    = RGB(0xC0,0xC0,0xC0),
        .start_btn_text    = RGB(0x00,0x00,0x00),
        .startmenu_bg      = RGB(0xC0,0xC0,0xC0),
        .startmenu_sidebar = RGB(0x00,0x00,0x80),
        .startmenu_hover   = RGB(0x00,0x00,0x80),
        .startmenu_text    = RGB(0x00,0x00,0x00),
        .select_bg         = RGB(0x00,0x00,0x80),
        .select_text       = RGB(0xFF,0xFF,0xFF),
        .scroll_track      = RGB(0xC0,0xC0,0xC0),
        .scroll_thumb      = RGB(0xC0,0xC0,0xC0),
        .icon_text         = RGB(0xFF,0xFF,0xFF),
        .icon_text_shadow  = RGB(0x00,0x00,0x00),
        .icon_bg           = RGB(0x00,0x80,0x80),
        .icon_border       = RGB(0x00,0x40,0x40),
    },
    .title_gradient     = {0, 0, false},
    .title_gradient_ina = {0, 0, false},
    .rounded_buttons    = false,
    .gradient_title     = false,
    .Luna_start_button  = false,
},

/* -- XP Luna Blue ------------------------------------------------ */
[THEME_XP_LUNA_BLUE] = {
    .id = THEME_XP_LUNA_BLUE,
    .name = "XP Luna Blue",
    .colors = {
        .desktop_bg        = RGB(0x00,0x52,0x8A),   /* XP Bliss blue */
        .win_face          = RGB(0xE8,0xE8,0xE8),   /* Lighter face */
        .win_title_active  = RGB(0x00,0x53,0x9E),   /* Luna blue */
        .win_title_inactive= RGB(0xB0,0xB0,0xB0),   /* Muted */
        .win_title_text    = RGB(0xFF,0xFF,0xFF),
        .win_title_text_ina= RGB(0xD8,0xD8,0xD8),
        .border_light      = RGB(0xFF,0xFF,0xFF),
        .border_face       = RGB(0xE8,0xE8,0xE8),
        .border_dark       = RGB(0x7B,0x7B,0x7B),
        .border_darkest    = RGB(0x00,0x00,0x00),
        .btn_face          = RGB(0xEE,0xEE,0xEE),
        .btn_hover         = RGB(0xF4,0xF4,0xF4),
        .btn_pressed       = RGB(0xCC,0xCC,0xCC),
        .btn_text          = RGB(0x00,0x00,0x00),
        .taskbar_bg        = RGB(0x31,0x6A,0xC5),   /* XP blue taskbar */
        .taskbar_border    = RGB(0x1B,0x4D,0x8E),
        .start_btn_face    = RGB(0x3A,0x7C,0x2C),   /* Green Start orb */
        .start_btn_text    = RGB(0xFF,0xFF,0xFF),
        .startmenu_bg      = RGB(0xF8,0xF8,0xF8),
        .startmenu_sidebar = RGB(0x00,0x53,0x9E),   /* Blue sidebar */
        .startmenu_hover   = RGB(0x31,0x6A,0xC5),
        .startmenu_text    = RGB(0x00,0x00,0x00),
        .select_bg         = RGB(0x31,0x6A,0xC5),
        .select_text       = RGB(0xFF,0xFF,0xFF),
        .scroll_track      = RGB(0xF0,0xF0,0xF0),
        .scroll_thumb      = RGB(0xC8,0xC8,0xC8),
        .icon_text         = RGB(0xFF,0xFF,0xFF),
        .icon_text_shadow  = RGB(0x00,0x00,0x00),
        .icon_bg           = RGB(0x00,0x52,0x8A),
        .icon_border       = RGB(0x00,0x3A,0x60),
    },
    .title_gradient     = {RGB(0x00,0x53,0x9E), RGB(0x00,0x99,0xCC), true},
    .title_gradient_ina = {RGB(0xB0,0xB0,0xB0), RGB(0xD8,0xD8,0xD8), true},
    .rounded_buttons    = true,
    .gradient_title     = true,
    .Luna_start_button  = true,
},

/* -- XP Media Center Orange/Black -------------------------------- */
[THEME_XP_MEDIA_ORANGE] = {
    .id = THEME_XP_MEDIA_ORANGE,
    .name = "XP Media Orange",
    .colors = {
        .desktop_bg        = RGB(0x1A,0x1A,0x1A),   /* Near-black */
        .win_face          = RGB(0x2A,0x2A,0x2A),   /* Dark gray */
        .win_title_active  = RGB(0xE8,0x6C,0x00),   /* MC orange */
        .win_title_inactive= RGB(0x4A,0x4A,0x4A),
        .win_title_text    = RGB(0xFF,0xFF,0xFF),
        .win_title_text_ina= RGB(0x8A,0x8A,0x8A),
        .border_light      = RGB(0x5A,0x5A,0x5A),
        .border_face       = RGB(0x2A,0x2A,0x2A),
        .border_dark       = RGB(0x1A,0x1A,0x1A),
        .border_darkest    = RGB(0x00,0x00,0x00),
        .btn_face          = RGB(0x3A,0x3A,0x3A),
        .btn_hover         = RGB(0xE8,0x6C,0x00),   /* Orange hover */
        .btn_pressed       = RGB(0xB0,0x54,0x00),
        .btn_text          = RGB(0xFF,0xFF,0xFF),
        .taskbar_bg        = RGB(0x1A,0x1A,0x1A),   /* Black taskbar */
        .taskbar_border    = RGB(0xE8,0x6C,0x00),   /* Orange accent */
        .start_btn_face    = RGB(0xE8,0x6C,0x00),   /* Orange Start */
        .start_btn_text    = RGB(0x00,0x00,0x00),
        .startmenu_bg      = RGB(0x2A,0x2A,0x2A),
        .startmenu_sidebar = RGB(0xE8,0x6C,0x00),
        .startmenu_hover   = RGB(0xE8,0x6C,0x00),
        .startmenu_text    = RGB(0xFF,0xFF,0xFF),
        .select_bg         = RGB(0xE8,0x6C,0x00),
        .select_text       = RGB(0x00,0x00,0x00),
        .scroll_track      = RGB(0x1A,0x1A,0x1A),
        .scroll_thumb      = RGB(0x4A,0x4A,0x4A),
        .icon_text         = RGB(0xFF,0xFF,0xFF),
        .icon_text_shadow  = RGB(0x00,0x00,0x00),
        .icon_bg           = RGB(0x2A,0x2A,0x2A),
        .icon_border       = RGB(0xE8,0x6C,0x00),
    },
    .title_gradient     = {RGB(0xE8,0x6C,0x00), RGB(0xFF,0x99,0x33), true},
    .title_gradient_ina = {RGB(0x4A,0x4A,0x4A), RGB(0x3A,0x3A,0x3A), true},
    .rounded_buttons    = true,
    .gradient_title     = true,
    .Luna_start_button  = true,
},

/* -- WuBu Custom ------------------------------------------------- */
[THEME_WUBU_CUSTOM] = {
    .id = THEME_WUBU_CUSTOM,
    .name = "WuBu Green",
    .colors = {
        .desktop_bg        = RGB(0x0A,0x2A,0x1A),   /* Dark green */
        .win_face          = RGB(0x1A,0x3A,0x2A),   /* Green-tint */
        .win_title_active  = RGB(0x00,0x80,0x50),   /* WuBu green */
        .win_title_inactive= RGB(0x2A,0x3A,0x30),
        .win_title_text    = RGB(0xFF,0xFF,0xFF),
        .win_title_text_ina= RGB(0x8A,0x9A,0x8A),
        .border_light      = RGB(0x3A,0x5A,0x4A),
        .border_face       = RGB(0x1A,0x3A,0x2A),
        .border_dark       = RGB(0x0A,0x1A,0x0A),
        .border_darkest    = RGB(0x00,0x00,0x00),
        .btn_face          = RGB(0x2A,0x4A,0x3A),
        .btn_hover         = RGB(0x00,0x80,0x50),
        .btn_pressed       = RGB(0x00,0x5A,0x30),
        .btn_text          = RGB(0xFF,0xFF,0xFF),
        .taskbar_bg        = RGB(0x0A,0x2A,0x1A),
        .taskbar_border    = RGB(0x00,0x80,0x50),
        .start_btn_face    = RGB(0x00,0x80,0x50),
        .start_btn_text    = RGB(0xFF,0xFF,0xFF),
        .startmenu_bg      = RGB(0x1A,0x3A,0x2A),
        .startmenu_sidebar = RGB(0x00,0x80,0x50),
        .startmenu_hover   = RGB(0x00,0x80,0x50),
        .startmenu_text    = RGB(0xFF,0xFF,0xFF),
        .select_bg         = RGB(0x00,0x80,0x50),
        .select_text       = RGB(0xFF,0xFF,0xFF),
        .scroll_track      = RGB(0x0A,0x2A,0x1A),
        .scroll_thumb      = RGB(0x2A,0x4A,0x3A),
        .icon_text         = RGB(0xFF,0xFF,0xFF),
        .icon_text_shadow  = RGB(0x00,0x00,0x00),
        .icon_bg           = RGB(0x00,0x60,0x30),
        .icon_border       = RGB(0x00,0x80,0x50),
    },
    .title_gradient     = {RGB(0x00,0x80,0x50), RGB(0x00,0xC0,0x80), true},
    .title_gradient_ina = {RGB(0x2A,0x3A,0x30), RGB(0x1A,0x2A,0x1A), true},
    .rounded_buttons    = true,
    .gradient_title     = true,
    .Luna_start_button  = true,
},

/* -- Zune ---------------------------------------------------------- */
[THEME_ZUNE] = {
    .id = THEME_ZUNE,
    .name = "Zune",
    .colors = {
        .desktop_bg        = RGB(0x1A,0x1A,0x1A),   /* Near-black */
        .win_face          = RGB(0x22,0x22,0x22),   /* Dark gray */
        .win_title_active  = RGB(0xE8,0x6C,0x00),   /* Zune orange */
        .win_title_inactive= RGB(0x33,0x33,0x33),
        .win_title_text    = RGB(0xFF,0xFF,0xFF),
        .win_title_text_ina= RGB(0x88,0x88,0x88),
        .border_light      = RGB(0x44,0x44,0x44),
        .border_face       = RGB(0x22,0x22,0x22),
        .border_dark       = RGB(0x11,0x11,0x11),
        .border_darkest    = RGB(0x00,0x00,0x00),
        .btn_face          = RGB(0x2A,0x2A,0x2A),
        .btn_hover         = RGB(0xE8,0x6C,0x00),   /* Orange hover */
        .btn_pressed       = RGB(0xB0,0x54,0x00),
        .btn_text          = RGB(0xFF,0xFF,0xFF),
        .taskbar_bg        = RGB(0x11,0x11,0x11),   /* Near-black taskbar */
        .taskbar_border    = RGB(0xE8,0x6C,0x00),   /* Orange accent */
        .start_btn_face    = RGB(0xE8,0x6C,0x00),   /* Orange Start */
        .start_btn_text    = RGB(0xFF,0xFF,0xFF),
        .startmenu_bg      = RGB(0x1A,0x1A,0x1A),
        .startmenu_sidebar = RGB(0xE8,0x6C,0x00),
        .startmenu_hover   = RGB(0xE8,0x6C,0x00),
        .startmenu_text    = RGB(0xFF,0xFF,0xFF),
        .select_bg         = RGB(0xE8,0x6C,0x00),
        .select_text       = RGB(0x00,0x00,0x00),
        .scroll_track      = RGB(0x11,0x11,0x11),
        .scroll_thumb      = RGB(0x33,0x33,0x33),
        .icon_text         = RGB(0xFF,0xFF,0xFF),
        .icon_text_shadow  = RGB(0x00,0x00,0x00),
        .icon_bg           = RGB(0x22,0x22,0x22),
        .icon_border       = RGB(0xE8,0x6C,0x00),
    },
    .title_gradient     = {RGB(0xE8,0x6C,0x00), RGB(0xFF,0x99,0x33), true},
    .title_gradient_ina = {RGB(0x33,0x33,0x33), RGB(0x22,0x22,0x22), true},
    .rounded_buttons    = true,
    .gradient_title     = true,
    .Luna_start_button  = true,
},
};

/* -- Theme Engine State ------------------------------------------- */

static WubuThemeId g_current = THEME_WIN98_CLASSIC;

int wubu_theme_init(void) {
    g_current = THEME_WIN98_CLASSIC;
    return 0;
}

void wubu_theme_shutdown(void) {
    /* Nothing dynamic to clean up */
}

WubuThemeId wubu_theme_current(void) {
    return g_current;
}

void wubu_theme_set(WubuThemeId id) {
    if (id >= 0 && id < THEME_COUNT)
        g_current = id;
}

const WubuThemeColors *wubu_theme_colors(void) {
    return &g_themes[g_current].colors;
}

const WubuTheme *wubu_theme_get(void) {
    return &g_themes[g_current];
}

void wubu_theme_cycle(void) {
    g_current = (WubuThemeId)((g_current + 1) % THEME_COUNT);
}

const char *wubu_theme_name(WubuThemeId id) {
    if (id >= 0 && id < THEME_COUNT)
        return g_themes[id].name;
    return "Unknown";
}
