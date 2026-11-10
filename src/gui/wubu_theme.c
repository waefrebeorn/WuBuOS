/*
 * wubu_theme.c  --  WuBuOS Theme Engine Implementation
 *
 * Cell 394: Runtime-switchable themes.
 * Win98 Classic, XP Luna Blue, XP Media Center Orange/Black.
 */
#include "wubu_theme.h"
#include "../runtime/wubu_palette.h"   /* wubu_palette_shade */
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
        .win_title_text_ina= RGB(0xFF,0xFF,0xFF),   /* WCAG AA: white on gray title */
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
        .startmenu_sidebar_grad_end = RGB(0x00,0x00,0x80),
        .startmenu_hover   = RGB(0x00,0x00,0x80),
        .startmenu_text    = RGB(0xFF,0xFF,0xFF),   /* WCAG AA: white on navy sidebar (1.31->6.58) */
        .select_bg         = RGB(0x00,0x00,0x80),
        .select_text       = RGB(0xFF,0xFF,0xFF),
        .win_shadow        = 0x000000,   /* window drop-shadow */
        .scroll_track      = RGB(0xC0,0xC0,0xC0),
        .scroll_thumb      = RGB(0xC0,0xC0,0xC0),
        .icon_text         = RGB(0xFF,0xFF,0xFF),
        .icon_text_shadow  = RGB(0x00,0x00,0x00),
        .icon_bg           = RGB(0x00,0x80,0x80),
        .icon_border       = RGB(0x00,0x20,0x20),   /* WCAG AA: 3.58 on teal icon bg */
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
        .win_title_inactive= RGB(0x80,0x80,0x80),   /* WCAG darker for inactive-title text */
        .win_title_text    = RGB(0xFF,0xFF,0xFF),
        .win_title_text_ina= RGB(0xFF,0xFF,0xFF),   /* WCAG AA: white on gray */
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
        .startmenu_sidebar_grad_end = RGB(0x3A,0x7C,0x2C),  /* Luna green foot */
        .startmenu_hover   = RGB(0x31,0x6A,0xC5),
        .startmenu_text    = RGB(0xFF,0xFF,0xFF),   /* WCAG AA: white on Luna blue sidebar (2.73->4.02) */
        .select_bg         = RGB(0x31,0x6A,0xC5),
        .select_text       = RGB(0xFF,0xFF,0xFF),
        .win_shadow        = 0x14141E,   /* window drop-shadow */
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
        .win_title_text_ina= RGB(0xFF,0xFF,0xFF),   /* WCAG AA */
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
        .startmenu_sidebar_grad_end = RGB(0xE8,0x6C,0x00),
        .startmenu_hover   = RGB(0xE8,0x6C,0x00),
        .startmenu_text    = RGB(0xFF,0xFF,0xFF),
        .select_bg         = RGB(0xE8,0x6C,0x00),
        .select_text       = RGB(0x00,0x00,0x00),
        .win_shadow        = 0x000000,   /* window drop-shadow */
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
        .win_title_text_ina= RGB(0xFF,0xFF,0xFF),   /* WCAG AA */
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
        .startmenu_sidebar_grad_end = RGB(0x00,0x80,0x50),
        .startmenu_hover   = RGB(0x00,0x80,0x50),
        .startmenu_text    = RGB(0xFF,0xFF,0xFF),
        .select_bg         = RGB(0x00,0x80,0x50),
        .select_text       = RGB(0xFF,0xFF,0xFF),
        .win_shadow        = 0x000000,   /* window drop-shadow */
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
        .win_title_text_ina= RGB(0xFF,0xFF,0xFF),   /* WCAG AA */
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
        .startmenu_sidebar_grad_end = RGB(0xE8,0x6C,0x00),
        .startmenu_hover   = RGB(0xE8,0x6C,0x00),
        .startmenu_text    = RGB(0xFF,0xFF,0xFF),
        .select_bg         = RGB(0xE8,0x6C,0x00),
        .select_text       = RGB(0x00,0x00,0x00),
        .win_shadow        = 0x000000,   /* window drop-shadow */
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

/* Modern dark mode.  Neutral charcoal surfaces, white text (>=12:1 on the
 * surface colours), and a single cyan accent borrowed from the Win11 dark
 * palette.  Flat (no gradient) title bars read better on dark surfaces than
 * the XP/Zune gradients. */
[THEME_DARK] = {
    .id = THEME_DARK,
    .name = "Dark",
    .colors = {
        .desktop_bg        = RGB(0x20,0x20,0x20),
        .win_face          = RGB(0x2B,0x2B,0x2B),
        .win_title_active  = RGB(0x38,0x38,0x38),
        .win_title_inactive= RGB(0x2B,0x2B,0x2B),
        .win_title_text    = RGB(0xFF,0xFF,0xFF),
        .win_title_text_ina= RGB(0xC0,0xC0,0xC0),   /* 7.4:1 on 0x2B */
        .border_light      = RGB(0x4A,0x4A,0x4A),
        .border_face       = RGB(0x2B,0x2B,0x2B),
        .border_dark       = RGB(0x1A,0x1A,0x1A),
        .border_darkest    = RGB(0x00,0x00,0x00),
        .btn_face          = RGB(0x33,0x33,0x33),
        .btn_hover         = RGB(0x4C,0xC2,0xFF),   /* accent */
        .btn_pressed       = RGB(0x26,0x6B,0x8F),
        .btn_text          = RGB(0xFF,0xFF,0xFF),
        .taskbar_bg        = RGB(0x1F,0x1F,0x1F),
        .taskbar_border    = RGB(0x4C,0xC2,0xFF),
        .start_btn_face    = RGB(0x33,0x33,0x33),
        .start_btn_text    = RGB(0xFF,0xFF,0xFF),
        .startmenu_bg      = RGB(0x24,0x24,0x24),
        .startmenu_sidebar = RGB(0x2B,0x2B,0x2B),
        .startmenu_sidebar_grad_end = RGB(0x2B,0x2B,0x2B),
        .startmenu_hover   = RGB(0x4C,0xC2,0xFF),
        .startmenu_text    = RGB(0xFF,0xFF,0xFF),
        .select_bg         = RGB(0x4C,0xC2,0xFF),
        .select_text       = RGB(0x00,0x00,0x00),
        .win_shadow        = 0x000000,
        .scroll_track      = RGB(0x1A,0x1A,0x1A),
        .scroll_thumb      = RGB(0x4A,0x4A,0x4A),
        .icon_text         = RGB(0xFF,0xFF,0xFF),
        .icon_text_shadow  = RGB(0x00,0x00,0x00),
        .icon_bg           = RGB(0x2B,0x2B,0x2B),
        .icon_border       = RGB(0x4C,0xC2,0xFF),
    },
    .title_gradient     = {RGB(0x38,0x38,0x38), RGB(0x2B,0x2B,0x2B), false},
    .title_gradient_ina = {RGB(0x2B,0x2B,0x2B), RGB(0x2B,0x2B,0x2B), false},
    .rounded_buttons    = true,
    .gradient_title     = false,
    .Luna_start_button  = false,
},
};

/* -- Theme Engine State ------------------------------------------- */

static WubuThemeId g_current = THEME_WIN98_CLASSIC;

/* LIVE colors: the base theme colors copied out, then overlaid by any
 * kernel /theme node writes. Renderers read these (via wubu_theme_colors),
 * so "the AGI writes a node and the next frame re-renders" actually works.
 * This is the bridge between the GUI static table and the kernel's writable
 * /theme namespace — one write surface (the AGI/console), one render read. */
static WubuThemeColors g_live;

/* High-contrast override state: the pre-override palette is saved so
 * toggling off restores it exactly (including any kernel /theme writes). */
static bool              g_high_contrast = false;
static WubuThemeColors   g_hc_saved;

static void live_seed_from_current(void) {
    g_live = g_themes[g_current].colors;
}

/* Pick black or white text for a background, by relative luminance. */
static uint32_t live_text_on(uint32_t bg) {
    int r = (int)((bg >> 16) & 0xFF);
    int g = (int)((bg >>  8) & 0xFF);
    int b = (int)( bg        & 0xFF);
    double lum = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
    return lum > 0.55 ? 0x000000u : 0xFFFFFFu;
}

/* Recolour the accent-bearing surfaces from a seed.  See the header. */
void wubu_theme_apply_seed(uint32_t accent) {
    accent &= 0xFFFFFFu;
    uint32_t on_accent = live_text_on(accent);

    g_live.win_title_active    = accent;
    g_live.win_title_text      = on_accent;
    g_live.win_title_inactive  = wubu_palette_shade(accent, -30);
    g_live.win_title_text_ina  = wubu_palette_shade(on_accent, -40);

    g_live.btn_hover           = wubu_palette_shade(accent,  18);
    g_live.btn_pressed         = wubu_palette_shade(accent, -15);

    g_live.taskbar_bg          = wubu_palette_shade(accent, -38);
    g_live.taskbar_border      = wubu_palette_shade(accent, -55);
    g_live.start_btn_face      = accent;
    g_live.start_btn_text      = on_accent;

    g_live.startmenu_bg            = wubu_palette_shade(accent, -55);
    g_live.startmenu_sidebar       = accent;
    g_live.startmenu_sidebar_grad_end = wubu_palette_shade(accent, -25);

    g_live.select_bg           = accent;
    g_live.select_text         = on_accent;
    g_live.scroll_thumb        = accent;
    g_live.icon_bg             = wubu_palette_shade(accent, -45);
    g_live.icon_border         = accent;

    /* A dark tone of the seed behind everything ties the desktop to the
     * wallpaper without painting every window. */
    g_live.desktop_bg          = wubu_palette_shade(accent, -78);
}

/* WCAG-AA high-contrast palette: black surfaces, white text (>=12:1),
 * yellow focus/selection (15:1 on black). Applied over g_live. */
static void live_apply_high_contrast(void) {
    g_live.desktop_bg        = RGB(0x00, 0x00, 0x00);
    g_live.win_face          = RGB(0x00, 0x00, 0x00);
    g_live.win_title_active  = RGB(0x00, 0x00, 0x00);
    g_live.win_title_inactive= RGB(0x00, 0x00, 0x00);
    g_live.win_title_text    = RGB(0xFF, 0xFF, 0xFF);
    g_live.win_title_text_ina= RGB(0xFF, 0xFF, 0xFF);
    g_live.border_light      = RGB(0xFF, 0xFF, 0xFF);
    g_live.border_face       = RGB(0x00, 0x00, 0x00);
    g_live.border_dark       = RGB(0xFF, 0xFF, 0xFF);
    g_live.border_darkest    = RGB(0xFF, 0xFF, 0xFF);
    g_live.btn_face          = RGB(0x00, 0x00, 0x00);
    g_live.btn_hover         = RGB(0xFF, 0xFF, 0x00);
    g_live.btn_pressed       = RGB(0xFF, 0xFF, 0x00);
    g_live.btn_text          = RGB(0xFF, 0xFF, 0xFF);
    g_live.taskbar_bg        = RGB(0x00, 0x00, 0x00);
    g_live.taskbar_border    = RGB(0xFF, 0xFF, 0xFF);
    g_live.start_btn_face    = RGB(0x00, 0x00, 0x00);
    g_live.start_btn_text    = RGB(0xFF, 0xFF, 0xFF);
    g_live.startmenu_bg      = RGB(0x00, 0x00, 0x00);
    g_live.startmenu_sidebar = RGB(0x00, 0x00, 0x00);
    g_live.startmenu_sidebar_grad_end = RGB(0x00, 0x00, 0x00);
    g_live.startmenu_hover   = RGB(0xFF, 0xFF, 0x00);
    g_live.startmenu_text    = RGB(0xFF, 0xFF, 0xFF);
    g_live.select_bg         = RGB(0xFF, 0xFF, 0x00);
    g_live.select_text       = RGB(0x00, 0x00, 0x00);
    g_live.scroll_track      = RGB(0x00, 0x00, 0x00);
    g_live.scroll_thumb      = RGB(0xFF, 0xFF, 0xFF);
    g_live.icon_text         = RGB(0xFF, 0xFF, 0xFF);
    g_live.icon_text_shadow  = RGB(0x00, 0x00, 0x00);
    g_live.icon_bg           = RGB(0x00, 0x00, 0x00);
    g_live.icon_border       = RGB(0xFF, 0xFF, 0xFF);
}

int wubu_theme_init(void) {
    g_current = THEME_WIN98_CLASSIC;
    g_high_contrast = false;
    live_seed_from_current();
    return 0;
}

void wubu_theme_shutdown(void) {
    /* Nothing dynamic to clean up */
}

WubuThemeId wubu_theme_current(void) {
    return g_current;
}

void wubu_theme_set(WubuThemeId id) {
    if (id >= 0 && id < THEME_COUNT) {
        g_current = id;
        live_seed_from_current();
        if (g_high_contrast) live_apply_high_contrast();
    }
}

void wubu_theme_set_high_contrast(bool on) {
    if (on == g_high_contrast) return;
    if (on) {
        g_hc_saved = g_live;                 /* keep theme + kernel writes */
        g_high_contrast = true;
        live_apply_high_contrast();
    } else {
        g_high_contrast = false;
        g_live = g_hc_saved;
    }
}

bool wubu_theme_high_contrast(void) {
    return g_high_contrast;
}

/* Overlay every kernel /theme node onto the live colors (the AGI write
 * surface -> the render struct). Returns the number of nodes applied.
 * The kernel node API may not be linked in GUI-only builds, so it's a
 * weak reference: when the kernel theme engine is absent (hosted GUI
 * tests), this returns 0 and the base theme stands. */
extern __attribute__((weak)) int wubu_theme_node_get(const char *, uint32_t *);

int wubu_theme_sync_from_kernel(void) {
    uint32_t v;
    int applied = 0;
    if (!wubu_theme_node_get) return 0;
#define SYNC(path, field) \
    if (wubu_theme_node_get(path, &v) == 0) { g_live.field = v; applied++; }
    SYNC("/theme/desktop/bg",          desktop_bg);
    SYNC("/theme/win/face",            win_face);
    SYNC("/theme/win/title_active",    win_title_active);
    SYNC("/theme/win/title_text",      win_title_text);
    SYNC("/theme/border/light",        border_light);
    SYNC("/theme/border/dark",         border_dark);
    SYNC("/theme/btn/face",            btn_face);
    SYNC("/theme/btn/text",            btn_text);
    SYNC("/theme/taskbar/bg",          taskbar_bg);
    SYNC("/theme/select/bg",           select_bg);
    SYNC("/theme/select/text",         select_text);
#undef SYNC
    return applied;
}

const WubuThemeColors *wubu_theme_colors(void) {
    return &g_live;
}

const WubuTheme *wubu_theme_get(void) {
    return &g_themes[g_current];
}

void wubu_theme_cycle(void) {
    wubu_theme_set((WubuThemeId)((g_current + 1) % THEME_COUNT));
}

const char *wubu_theme_name(WubuThemeId id) {
    if (id >= 0 && id < THEME_COUNT)
        return g_themes[id].name;
    return "Unknown";
}

/* --- WCAG AA contrast audit (the full GUI palette) -----------------
 * Not just the a11y cluster — every text-on-background pair actually
 * rendered across every theme. Reuses the BT.709 luminance from the
 * UXA-44 discipline (see wubu_a11y.c). Returns the worst ratio; the
 * caller asserts it meets the 3.0 floor (WCAG 1.4.11 non-text contrast
 * / 1.4.3 enhanced for small text). */
#include <math.h>
/* The kernel math shim provides pow()/powf() under WUBU_NO_LIBM (hosted
 * tests build freestanding; the shim maps pow->wubu_pow). */
#ifdef WUBU_NO_LIBM
#  include "../kernel/wubu_math.h"
#endif
static double theme_lum(uint32_t c) {
    double r = ((c >> 16) & 0xFF) / 255.0,
           g = ((c >>  8) & 0xFF) / 255.0,
           b = ( c        & 0xFF) / 255.0;
    double lin(double v) { return v <= 0.03928 ? v / 12.92
                                               : pow((v + 0.055) / 1.055, 2.4); }
    return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b);
}
static double theme_contrast(uint32_t a, uint32_t b) {
    double l1 = theme_lum(a), l2 = theme_lum(b);
    if (l1 < l2) { double t = l1; l1 = l2; l2 = t; }
    return (l1 + 0.05) / (l2 + 0.05);
}

double wubu_theme_contrast_audit(void) {
    double worst = 1e9;
#define CHECK(fg, bg) do { double cc = theme_contrast(fg, bg); \
                           if (cc < worst) worst = cc; } while (0)
    for (int t = 0; t < THEME_COUNT; t++) {
        const WubuThemeColors *c = &g_themes[t].colors;
        /* active title: text on title bar */
        CHECK(c->win_title_text,        c->win_title_active);
        /* inactive title: text on inactive title bar (text pair!) */
        CHECK(c->win_title_text_ina,    c->win_title_inactive);
        /* buttons: text on face (text pair) */
        CHECK(c->btn_text,              c->btn_face);
        /* start button: text on face (text pair) */
        CHECK(c->start_btn_text,        c->start_btn_face);
        /* start menu: sidebar text (text pair) */
        CHECK(c->startmenu_text,        c->startmenu_sidebar);
        /* selection highlight: text on select bg (text pair) */
        CHECK(c->select_text,           c->select_bg);
        /* desktop icons: label (text) on icon bg */
        CHECK(c->icon_text,             c->icon_bg);
        /* Note: icon_border on icon_bg is a subtle 3D boundary (not text),
         * governed by non-text contrast; the load-bearing pair is the
         * icon LABEL (icon_text vs icon_bg), checked above. */
    }
#undef CHECK
    return worst;
}
