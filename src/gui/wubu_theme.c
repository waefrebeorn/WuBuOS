/*
 * wubu_theme.c  --  WuBuOS Theme Engine Implementation
 *
 * Cell 394: Runtime-switchable themes.
 * Win98 Classic, XP Luna Blue, XP Media Center Orange/Black.
 */
#include "wubu_theme.h"

#include <stdio.h>   /* the contrast audit must be able to NAME the pair it rejects */
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
        /* WAS MISSING: an unset field is 0, so every inactive Win98 window drew a BLACK
         * caption bar. The classic inactive caption is grey. */
        .win_title_inactive= RGB(0x6E,0x6E,0x6E),
        .win_title_text    = RGB(0xFF,0xFF,0xFF),   /* white on navy: 15.9:1 */
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
        .start_mark        = RGB(0x00,0x00,0x00),
        .startmenu_bg      = RGB(0xC0,0xC0,0xC0),
        .startmenu_sidebar = RGB(0x00,0x00,0x80),
        .startmenu_sidebar_grad_end = RGB(0x00,0x00,0x80),
        .startmenu_bg      = RGB(0xC0,0xC0,0xC0),
        /* MEASURED, not asserted: white on a silver menu is 1.82:1 -- the audit in
         * tools/gui_audit.py caught this. Black is 11.6:1. */
        .startmenu_text    = RGB(0x00,0x00,0x00),
        .startmenu_shutdown_bg = RGB(0x80,0x00,0x00),
        .startmenu_shutdown_fg = RGB(0xFF,0xFF,0xFF),
        .caption_close_hover = RGB(0xC4,0x2B,0x1C),
        .notif_read_bg = RGB(0xA8,0xA8,0xA8),
        .select_bg         = RGB(0x00,0x00,0x80),
        .select_text       = RGB(0xFF,0xFF,0xFF),
        .win_shadow        = 0x000000,   /* window drop-shadow */
        .scroll_track      = RGB(0xC0,0xC0,0xC0),
        .scroll_thumb      = RGB(0x66,0x66,0x66),
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
        .win_title_inactive= RGB(0x6E,0x6E,0x6E),   /* WCAG darker for inactive-title text */
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
        .start_mark        = RGB(0xFF,0xFF,0xFF),
        .startmenu_bg      = RGB(0xF8,0xF8,0xF8),
        .startmenu_sidebar = RGB(0x00,0x53,0x9E),   /* Blue sidebar */
        .startmenu_sidebar_grad_end = RGB(0x3A,0x7C,0x2C),  /* Luna green foot */
        .startmenu_hover   = RGB(0x31,0x6A,0xC5),
        .startmenu_text    = RGB(0x00,0x00,0x00),   /* WCAG AA: white on Luna blue sidebar (2.73->4.02) */
        .startmenu_shutdown_bg = RGB(0x00,0x00,0x00),
        .startmenu_shutdown_fg = RGB(0xFF,0x80,0x80),
        .caption_close_hover = RGB(0xC4,0x2B,0x1C),
        .notif_read_bg = RGB(0xEC,0xE9,0xD8),
        .select_bg         = RGB(0x31,0x6A,0xC5),
        .select_text       = RGB(0xFF,0xFF,0xFF),
        .win_shadow        = 0x14141E,   /* window drop-shadow */
        .scroll_track      = RGB(0xF0,0xF0,0xF0),
        .scroll_thumb      = RGB(0x82,0x82,0x82),
        .icon_text         = RGB(0xFF,0xFF,0xFF),
        .icon_text_shadow  = RGB(0x00,0x00,0x00),
        .icon_bg           = RGB(0x00,0x52,0x8A),
        .icon_border       = RGB(0x87,0xA2,0xB4),
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
        .win_title_text    = RGB(0x1A,0x1A,0x1A),
        .win_title_text_ina= RGB(0xFF,0xFF,0xFF),   /* WCAG AA */
        .border_light      = RGB(0x5A,0x5A,0x5A),
        .border_face       = RGB(0x2A,0x2A,0x2A),
        .border_dark       = RGB(0x1A,0x1A,0x1A),
        .border_darkest    = RGB(0x75,0x75,0x75),
        .btn_face          = RGB(0x3A,0x3A,0x3A),
        .btn_hover         = RGB(0xE8,0x6C,0x00),   /* Orange hover */
        .btn_pressed       = RGB(0xB0,0x54,0x00),
        .btn_text          = RGB(0xFF,0xFF,0xFF),
        .taskbar_bg        = RGB(0x1A,0x1A,0x1A),   /* Black taskbar */
        .taskbar_border    = RGB(0xE8,0x6C,0x00),   /* Orange accent */
        .start_btn_face    = RGB(0xE8,0x6C,0x00),   /* Orange Start */
        .start_btn_text    = RGB(0x00,0x00,0x00),
        .start_mark        = RGB(0xFF,0xFF,0xFF),
        .startmenu_bg      = RGB(0x2A,0x2A,0x2A),
        .startmenu_sidebar = RGB(0xE8,0x6C,0x00),
        .startmenu_sidebar_grad_end = RGB(0xE8,0x6C,0x00),
        .startmenu_hover   = RGB(0xE8,0x6C,0x00),
        .startmenu_text    = RGB(0xFF,0xFF,0xFF),
        .startmenu_shutdown_bg = RGB(0x00,0x00,0x00),
        .startmenu_shutdown_fg = RGB(0xFF,0x80,0x80),
        .caption_close_hover = RGB(0xC4,0x2B,0x1C),
        .notif_read_bg = RGB(0xEC,0xE9,0xD8),
        .select_bg         = RGB(0xE8,0x6C,0x00),
        .select_text       = RGB(0x00,0x00,0x00),
        .win_shadow        = 0x000000,   /* window drop-shadow */
        .scroll_track      = RGB(0x1A,0x1A,0x1A),
        .scroll_thumb      = RGB(0x68,0x68,0x68),
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
        .border_darkest    = RGB(0x80,0x80,0x80),
        .btn_face          = RGB(0x2A,0x4A,0x3A),
        .btn_hover         = RGB(0x00,0x80,0x50),
        .btn_pressed       = RGB(0x00,0x5A,0x30),
        .btn_text          = RGB(0xFF,0xFF,0xFF),
        .taskbar_bg        = RGB(0x0A,0x2A,0x1A),
        .taskbar_border    = RGB(0x00,0x80,0x50),
        .start_btn_face    = RGB(0x00,0x80,0x50),
        .start_btn_text    = RGB(0xFF,0xFF,0xFF),
        .start_mark        = RGB(0xFF,0xFF,0xFF),
        .startmenu_bg      = RGB(0x1A,0x3A,0x2A),
        .startmenu_sidebar = RGB(0x00,0x80,0x50),
        .startmenu_sidebar_grad_end = RGB(0x00,0x80,0x50),
        .startmenu_hover   = RGB(0x00,0x80,0x50),
        .startmenu_text    = RGB(0xFF,0xFF,0xFF),
        .startmenu_shutdown_bg = RGB(0x00,0x00,0x00),
        .startmenu_shutdown_fg = RGB(0xFF,0x80,0x80),
        .caption_close_hover = RGB(0xC4,0x2B,0x1C),
        .notif_read_bg = RGB(0xD0,0xD0,0xD0),
        .select_bg         = RGB(0x00,0x80,0x50),
        .select_text       = RGB(0xFF,0xFF,0xFF),
        .win_shadow        = 0x000000,   /* window drop-shadow */
        .scroll_track      = RGB(0x0A,0x2A,0x1A),
        .scroll_thumb      = RGB(0x46,0x7B,0x60),
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

/* -- Windows 11 (Fluent light) ------------------------------------
 *
 * The other presets are period pieces: Win98's bevels, XP's blue gradient and green
 * orb, Zune's chrome. This one is the modern target, and the numbers are Win11's:
 * surfaces #F3F3F3 (Mica) and #FFFFFF (cards), text #1A1A1A, one accent #0078D4,
 * hairline borders #E5E5E5, and NO 3D bevel anywhere -- Fluent has no bevels at all,
 * which is why the retro themes' border_light/dark must both collapse to the same
 * hairline here. Red is reserved for the close-button hover, exactly as Win11 does. */
[THEME_WIN11] = {
    .id = THEME_WIN11,
    .name = "Windows 11",
    .colors = {
        .desktop_bg        = RGB(0xF3,0xF3,0xF3),
        .win_face          = RGB(0xFF,0xFF,0xFF),
        .win_title_active  = RGB(0xF3,0xF3,0xF3),
        .win_title_inactive= RGB(0xF3,0xF3,0xF3),
        .win_title_text    = RGB(0x1A,0x1A,0x1A),
        .win_title_text_ina= RGB(0x60,0x60,0x60),   /* 5.7:1 on F3 */
        .border_light      = RGB(0xE5,0xE5,0xE5),
        .border_face       = RGB(0xE5,0xE5,0xE5),
        .border_dark       = RGB(0xE5,0xE5,0xE5),
        .border_darkest    = RGB(0x6E,0x6E,0x6E),
        .btn_face          = RGB(0xF3,0xF3,0xF3),
        .btn_hover         = RGB(0xE9,0xE9,0xE9),
        .btn_pressed       = RGB(0xDE,0xDE,0xDE),
        .btn_text          = RGB(0x1A,0x1A,0x1A),
        .taskbar_bg        = RGB(0xF3,0xF3,0xF3),
        .taskbar_border    = RGB(0xE5,0xE5,0xE5),
        .start_btn_face    = RGB(0xF3,0xF3,0xF3),
        .start_btn_text    = RGB(0x1A,0x1A,0x1A),
        .start_mark        = RGB(0x00,0x00,0x00),
        .startmenu_bg      = RGB(0xF3,0xF3,0xF3),
        .startmenu_sidebar = RGB(0xE9,0xE9,0xE9),
        .startmenu_sidebar_grad_end = RGB(0xE9,0xE9,0xE9),
        .startmenu_hover   = RGB(0xE0,0xE0,0xE0),
        .startmenu_text    = RGB(0x1A,0x1A,0x1A),
        .startmenu_shutdown_bg = RGB(0xC4,0x2B,0x1C),
        .startmenu_shutdown_fg = RGB(0xFF,0xFF,0xFF),
        .caption_close_hover = RGB(0xC4,0x2B,0x1C),
        .notif_read_bg = RGB(0xED,0xED,0xED),
        .select_bg         = RGB(0x00,0x78,0xD4),   /* the accent */
        .select_text       = RGB(0xFF,0xFF,0xFF),
        .win_shadow        = 0x000000,
        .scroll_track      = RGB(0xF3,0xF3,0xF3),
        .scroll_thumb      = RGB(0x82,0x82,0x82),
        .icon_text         = RGB(0x1A,0x1A,0x1A),
        .icon_text_shadow  = RGB(0xFF,0xFF,0xFF),
        .icon_bg           = RGB(0xFF,0xFF,0xFF),
        .icon_border       = RGB(0x89,0x89,0x89),
    },
    .title_gradient     = {RGB(0xF3,0xF3,0xF3), RGB(0xF3,0xF3,0xF3), false},
    .title_gradient_ina = {RGB(0xF3,0xF3,0xF3), RGB(0xF3,0xF3,0xF3), false},
    .rounded_buttons    = true,
    .gradient_title     = false,
    .Luna_start_button  = false,
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
        .win_title_text    = RGB(0x1A,0x1A,0x1A),
        .win_title_text_ina= RGB(0xFF,0xFF,0xFF),   /* WCAG AA */
        .border_light      = RGB(0x44,0x44,0x44),
        .border_face       = RGB(0x22,0x22,0x22),
        .border_dark       = RGB(0x11,0x11,0x11),
        .border_darkest    = RGB(0x71,0x71,0x71),
        .btn_face          = RGB(0x2A,0x2A,0x2A),
        .btn_hover         = RGB(0xE8,0x6C,0x00),   /* Orange hover */
        .btn_pressed       = RGB(0xB0,0x54,0x00),
        .btn_text          = RGB(0xFF,0xFF,0xFF),
        .taskbar_bg        = RGB(0x11,0x11,0x11),   /* Near-black taskbar */
        .taskbar_border    = RGB(0xE8,0x6C,0x00),   /* Orange accent */
        .start_btn_face    = RGB(0xE8,0x6C,0x00),   /* Orange Start */
        .start_btn_text    = RGB(0x1A,0x1A,0x1A),   /* dark on Zune orange = 5.45:1, the
                                                     * same choice the title bar already
                                                     * makes on this exact fill. White was
                                                     * 3.19:1 -- below AA for text. */
        .start_mark        = RGB(0xFF,0xFF,0xFF),
        .startmenu_bg      = RGB(0x1A,0x1A,0x1A),
        .startmenu_sidebar = RGB(0xE8,0x6C,0x00),
        .startmenu_sidebar_grad_end = RGB(0xE8,0x6C,0x00),
        .startmenu_hover   = RGB(0xE8,0x6C,0x00),
        .startmenu_text    = RGB(0xFF,0xFF,0xFF),
        .startmenu_shutdown_bg = RGB(0x00,0x00,0x00),
        .startmenu_shutdown_fg = RGB(0xFF,0x80,0x80),
        .caption_close_hover = RGB(0xC4,0x2B,0x1C),
        .notif_read_bg = RGB(0x30,0x30,0x30),
        .select_bg         = RGB(0xE8,0x6C,0x00),
        .select_text       = RGB(0x00,0x00,0x00),
        .win_shadow        = 0x000000,   /* window drop-shadow */
        .scroll_track      = RGB(0x11,0x11,0x11),
        .scroll_thumb      = RGB(0x63,0x63,0x63),
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
        /* CONTRAST LADDER, not three greys that happen to be dark. The old values put
         * the title (0x38) two steps from the face (0x2B) and the border (0x4A) almost
         * on top of the face, so a window had no readable edge and its controls were
         * invisible -- "bad edges" was largely this. Face sits between them and the
         * border is a real hairline that reads on both. */
        .desktop_bg        = RGB(0x17,0x17,0x17),
        .win_face          = RGB(0x24,0x24,0x24),
        .win_title_active  = RGB(0x32,0x32,0x32),
        .win_title_inactive= RGB(0x27,0x27,0x27),
        .win_title_text    = RGB(0xFF,0xFF,0xFF),
        .win_title_text_ina= RGB(0xC0,0xC0,0xC0),   /* 7.4:1 on 0x2B */
        .border_light      = RGB(0x4A,0x4A,0x4A),
        .border_face       = RGB(0x3A,0x3A,0x3A),   /* the visible hairline */
        .border_dark       = RGB(0x3A,0x3A,0x3A),   /* fluent: no bevel pair */
        .border_darkest    = RGB(0x92,0x92,0x92),
        .btn_face          = RGB(0x33,0x33,0x33),
        .btn_hover         = RGB(0x4C,0xC2,0xFF),   /* accent */
        .btn_pressed       = RGB(0x26,0x6B,0x8F),
        .btn_text          = RGB(0xFF,0xFF,0xFF),
        .taskbar_bg        = RGB(0x1F,0x1F,0x1F),
        .taskbar_border    = RGB(0x4C,0xC2,0xFF),
        .start_btn_face    = RGB(0x33,0x33,0x33),
        .start_btn_text    = RGB(0xFF,0xFF,0xFF),
        .start_mark        = RGB(0xFF,0xFF,0xFF),
        .startmenu_bg      = RGB(0x24,0x24,0x24),
        .startmenu_sidebar = RGB(0x2B,0x2B,0x2B),
        .startmenu_sidebar_grad_end = RGB(0x2B,0x2B,0x2B),
        .startmenu_hover   = RGB(0x4C,0xC2,0xFF),
        .startmenu_text    = RGB(0xFF,0xFF,0xFF),
        .startmenu_shutdown_bg = RGB(0x00,0x00,0x00),
        .startmenu_shutdown_fg = RGB(0xFF,0x80,0x80),
        .caption_close_hover = RGB(0xC4,0x2B,0x1C),
        .notif_read_bg = RGB(0x30,0x30,0x30),
        .select_bg         = RGB(0x4C,0xC2,0xFF),
        .select_text       = RGB(0x00,0x00,0x00),
        .win_shadow        = 0x000000,
        .scroll_track      = RGB(0x1A,0x1A,0x1A),
        .scroll_thumb      = RGB(0x68,0x68,0x68),
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

static WubuThemeId g_current = THEME_DARK;

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

static uint32_t live_text_on(uint32_t bg);   /* defined below; used by the seed */

static void live_seed_from_current(void) {
    g_live = g_themes[g_current].colors;
    /* Derive the body-text pair from the surface it will actually be drawn on, for EVERY
     * palette at once, instead of trusting seven hand-written pairs to agree with their faces. */
    g_live.body_text = live_text_on(g_live.win_face);
    {
        /* Dim body text: halfway between the readable ink and the face, which keeps a label
         * secondary without dropping it under the 4.5:1 floor on either polarity. */
        uint32_t t = g_live.body_text, f = g_live.win_face;
        int tr = (int)((t >> 16) & 0xFF), tg = (int)((t >> 8) & 0xFF), tb = (int)(t & 0xFF);
        int fr = (int)((f >> 16) & 0xFF), fg = (int)((f >> 8) & 0xFF), fb = (int)(f & 0xFF);
        int r = (tr * 3 + fr) / 4, g2 = (tg * 3 + fg) / 4, b = (tb * 3 + fb) / 4;
        g_live.body_text_ina = ((uint32_t)r << 16) | ((uint32_t)g2 << 8) | (uint32_t)b;
    }
}

/* Pick black or white text for a background: whichever actually CONTRASTS MORE.
 *
 * The old rule was "luminance > 0.55 -> black". Classic silver 0xC0C0C0 has a relative luminance
 * of 0.527, just under that threshold, so every Win98 surface was given WHITE ink -- the file
 * manager and the setup wizard rendered light-on-light. Comparing the two real contrast ratios
 * cannot make that mistake: silver gets 11.5:1 with black and 1.8:1 with white. */
static double live_rel_lum(uint32_t c) {
    double ch[3] = { (double)((c >> 16) & 0xFF), (double)((c >> 8) & 0xFF), (double)(c & 0xFF) };
    const double w[3] = { 0.2126, 0.7152, 0.0722 };
    double L = 0.0;
    for (int i = 0; i < 3; i++) {
        double v = ch[i] / 255.0;
        v = (v <= 0.03928) ? v / 12.92 : ((v + 0.055) / 1.055);
        L += w[i] * v * v * v;
    }
    return L;
}

static double live_contrast(uint32_t a, uint32_t b) {
    double la = live_rel_lum(a), lb = live_rel_lum(b);
    double hi = la > lb ? la : lb, lo = la > lb ? lb : la;
    return (hi + 0.05) / (lo + 0.05);
}

static uint32_t live_text_on(uint32_t bg) {
    double c_black = live_contrast(bg, 0x000000u);
    double c_white = live_contrast(bg, 0xFFFFFFu);
    return (c_black >= c_white) ? 0x000000u : 0xFFFFFFu;
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
    g_live.start_mark          = on_accent;

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
    /* Fluent light is the shipped look. The retro presets stay selectable -- they are
     * part of this OS's character -- but a fresh install should look like this decade. */
    g_current = THEME_DARK;   /* dark only: the shipped look */
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

bool wubu_theme_is_fluent(void) {
    WubuThemeId id = wubu_theme_current();
    return id == THEME_WIN11 || id == THEME_DARK;
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
/* A minimum ratio with no name attached is a verdict nobody can act on: the same lesson
 * gate_reachability learned when it reported "412 orphans" and could not say which target
 * appeared. Every failing pair prints the theme, the pair, the colours and the ratio. */
/* Two bars, because WCAG has two: body text needs 4.5:1 (1.4.3), UI components and
 * large text need 3.0:1 (1.4.11). One number for everything either fails readable text or
 * rejects perfectly good chrome. */
#define CHECKR(fg, bg, need) do { double cc = theme_contrast(fg, bg); \
                           if (cc < worst) worst = cc; \
                           if (cc < (need)) { \
                               worst = 0.0;   /* a violation is a failure, not a note */ \
                               fprintf(stderr, "[contrast] %-14s %-28s %06X on %06X = %.2f:1 (need %.1f)\n", \
                                       g_themes[t].name, #fg, (unsigned)(fg) & 0xFFFFFF, \
                                       (unsigned)(bg) & 0xFFFFFF, cc, (double)(need)); } } while (0)
#define CHECK(fg, bg)   CHECKR(fg, bg, 4.5)   /* text */
#define CHECKUI(fg, bg) CHECKR(fg, bg, 3.0)   /* UI component / large text */
    for (int t = 0; t < THEME_COUNT; t++) {
        const WubuThemeColors *c = &g_themes[t].colors;
        /* active title: text on title bar */
        CHECK(c->win_title_text,        c->win_title_active);
        /* inactive title: text on inactive title bar (text pair!) */
        CHECKUI(c->win_title_text_ina,  c->win_title_inactive);  /* inactive caption */
        /* buttons: text on face (text pair) */
        CHECK(c->btn_text,              c->btn_face);
        /* start button: text on face (text pair) */
        CHECK(c->start_btn_text,        c->start_btn_face);
        /* Start menu TEXT is drawn on startmenu_bg -- the modern launcher panel fills
         * with it (startmenu.c). It used to be drawn on the SIDEBAR, which is why this
         * pair existed; that text is btn_text now (the comment in startmenu.c records the
         * fix) and the sidebar band is a gradient fill with no text on it. Auditing
         * text-on-sidebar asserted a combination the renderer never produces: it reported
         * Win98 black-on-navy at 1.31:1 for a menu that draws no such thing. */
        CHECK(c->startmenu_text,        c->startmenu_bg);
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
