#ifndef WUBU_TOKENS_H
#define WUBU_TOKENS_H

#include "wubu_gui_metrics.h"

/*
 * wubu_tokens.h — Unified design tokens for CLI + GUI + themes.
 *
 * Single source of truth for visual identity (ADR-002 in both repos).
 * This is the wubuos (GUI) copy; wubuwizard has its own copy in
 * include/wubu_tokens.h. Both kept in sync via the token contract.
 *
 * Generated from research/066-ux-cohesion-research.md (Theme J, rank 3:
 * "unified design token file for CLI + GUI + themes").
 */

/* ---- Color palette (ARGB 0x00RRGGBB) ---- */

/* WuBu brand green (shared with wubuwizard: #00C853 = RGB(0,200,83)) */
#define WUBU_COLOR_ACCENT_GREEN  0x0000C853u

/* WuBu brand blue (used for links, secondary accents) */
#define WUBU_COLOR_ACCENT_BLUE   0x0066CCFFu

/* Neutral grays */
#define WUBU_COLOR_GRAY_900      0x001A1A1Au
#define WUBU_COLOR_GRAY_800      0x002A2A2Au
#define WUBU_COLOR_GRAY_700      0x003A3A3Au
#define WUBU_COLOR_GRAY_600      0x004A4A4Au
#define WUBU_COLOR_GRAY_500      0x006A6A6Au
#define WUBU_COLOR_GRAY_400      0x008A8A8Au
#define WUBU_COLOR_GRAY_300      0x00AAAAAAu
#define WUBU_COLOR_GRAY_200      0x00CCCCCCu
#define WUBU_COLOR_GRAY_100      0x00EEEEEEu

/* Semantic colors */
#define WUBU_COLOR_BG_WINDOW     0x00F0F0F0u
#define WUBU_COLOR_BG_CONTENT    0x00FFFFFFu
#define WUBU_COLOR_BORDER        0x00C0C0C0u
#define WUBU_COLOR_TEXT          0x00000000u
#define WUBU_COLOR_TEXT_DIM      0x00808080u

/* ---- Layout tokens --------------------------------------------------
 *
 * There was no layout scale here at all when the audit ran: every surface
 * invented its own margins (2, 4, 6, 7, 8, 10, 12, 20 appear independently
 * across the shell), which is why windows, menus and the taskbar disagree by a
 * pixel or two everywhere.  A spacing system only works if it has ONE owner,
 * so these are the numbers and nothing else may hardcode geometry.
 *
 * The grid is 4px.  Anything not on it needs a reason written down.
 */

/* The shared steps are the metric system's gaps (one decision); the 2/24/32
 * extras are token-specific and stay literal. */
#define WUBU_SPACE_2XS    2
#define WUBU_SPACE_XS     WUBU_METRIC_GAP_XS
#define WUBU_SPACE_SM     WUBU_METRIC_GAP_SM
#define WUBU_SPACE_MD     WUBU_METRIC_GAP_MD
#define WUBU_SPACE_LG     WUBU_METRIC_GAP_LG
#define WUBU_SPACE_XL     24
#define WUBU_SPACE_2XL    32

/* Corner radii.  Win11 uses 8 at 100% scale for windows and menus. */
#define WUBU_RADIUS_NONE  0
#define WUBU_RADIUS_XS    2
#define WUBU_RADIUS_SM    4
#define WUBU_RADIUS_MD    8
#define WUBU_RADIUS_LG    12

/* Borders. */
#define WUBU_BORDER_HAIRLINE  1
#define WUBU_BORDER_THIN      2

/* ---- Control metrics (Windows 11 at 100% scale) ---- */

#define WUBU_CONTROL_H     32   /* a standard button / list row */
#define WUBU_ICON_SM       16   /* title-bar and menu icon */
#define WUBU_ICON_MD       24   /* desktop icon glyph */
#define WUBU_ICON_LG       32

#define WUBU_TITLE_BAR_H   WUBU_METRIC_TITLEBAR_H /* window caption: canonical */
#define WUBU_TITLE_BTN_W   46   /* caption button width */
#define WUBU_TITLE_BTN_H   32   /* flush with the caption bar */
#define WUBU_TITLE_PAD     WUBU_SPACE_SM   /* icon and title inset */

/* Window chrome: the era drop-shadow (XP/Luna) and caption behaviour. */
#define WUBU_CHROME_SHADOW_OFF      4   /* shadow offset down-right */
#define WUBU_CHROME_SHADOW_ALPHA_SOFT 40 /* outer pass */
#define WUBU_CHROME_SHADOW_ALPHA_HARD 90 /* inner pass */

/* Modal dialogs (rename/delete/properties/shortcut). Chrome metrics come
 * from window_chrome (chrome_title_bar_height / chrome_border_width) —
 * dialogs are windows, not a second chrome owner. These are the client-
 * area layout constants only. */
#define WUBU_DIALOG_ORIGIN_X     300
#define WUBU_DIALOG_ORIGIN_Y     200
#define WUBU_DIALOG_RENAME_W     400
#define WUBU_DIALOG_RENAME_H     150
#define WUBU_DIALOG_DELETE_W     350
#define WUBU_DIALOG_DELETE_H     140
#define WUBU_DIALOG_PROPS_W      400
#define WUBU_DIALOG_PROPS_H      300
#define WUBU_DIALOG_SHORTCUT_W   400
#define WUBU_DIALOG_SHORTCUT_H   150
#define WUBU_DIALOG_PAD_X        10
#define WUBU_DIALOG_LABEL_DY     20
#define WUBU_DIALOG_INPUT_DY     50
#define WUBU_DIALOG_HINT_DY      40
#define WUBU_DIALOG_PROP_TOP     10
#define WUBU_DIALOG_PROP_PITCH   16
#define WUBU_DIALOG_PROP_HINT_DY 10
#define WUBU_DIALOG_SC_LBL1_DY   16
#define WUBU_DIALOG_SC_VAL1_DY   38
#define WUBU_DIALOG_SC_LBL2_DY   68
#define WUBU_DIALOG_SC_VAL2_DY   90
#define WUBU_DIALOG_SC_HINT_DY   120

/* Toast durations (ms): UX timing, one owner. */
#define WUBU_TOAST_MS_SHORT      1500
#define WUBU_TOAST_MS_DEFAULT    2000
#define WUBU_TOAST_MS_LONG       2500

/* Notification center panel. */
#define WUBU_NOTIF_PANEL_W       350
#define WUBU_NOTIF_HEADER_H      30
#define WUBU_NOTIF_HEADER_PAD    10
#define WUBU_NOTIF_LIST_TOP      40
#define WUBU_NOTIF_ROW_PITCH     60
#define WUBU_NOTIF_CARD_H        56
#define WUBU_NOTIF_URG_INSET     6
#define WUBU_NOTIF_URG_W         4
#define WUBU_NOTIF_URG_H         44
#define WUBU_NOTIF_TEXT_DX       14
#define WUBU_NOTIF_SUMMARY_DY    18
#define WUBU_NOTIF_BODY_DY       30
#define WUBU_NOTIF_TIME_RESERVE  60
#define WUBU_NOTIF_BOTTOM_PAD    10
#define WUBU_NOTIF_TOGGLE_W      30
#define WUBU_NOTIF_TOGGLE_H      22

/* Urgency status art (theme-independent semantics, like icon identity). */
#define WUBU_STATUS_OK           0x00FF00u /* glyph-art */
#define WUBU_STATUS_WARN         0xFFFF00u /* glyph-art */
#define WUBU_STATUS_CRIT         0xFF0000u /* glyph-art */

/* Win+W widgets board. */
#define WUBU_WIDGETS_PANEL_W     360
#define WUBU_WIDGETS_PANEL_X     WUBU_METRIC_MARGIN_SM
#define WUBU_WIDGETS_PANEL_Y     WUBU_METRIC_MARGIN_SM
#define WUBU_WIDGETS_EDGE_MARGIN 16   /* min desktop border around the panel */
#define WUBU_WIDGETS_PANEL_RADIUS 10
#define WUBU_WIDGETS_CARD_RADIUS 8
#define WUBU_WIDGETS_CARD_PAD    WUBU_METRIC_GAP_MD
#define WUBU_WIDGETS_CARD_GAP    WUBU_METRIC_GAP_MD
#define WUBU_WIDGETS_HEADER_DY   10
#define WUBU_WIDGETS_LIST_TOP    40
#define WUBU_WIDGETS_CARD_TITLE_DY 6
#define WUBU_WIDGETS_CLOCK_H     92
#define WUBU_WIDGETS_CLOCK_TIME_DY 22
#define WUBU_WIDGETS_CLOCK_DATE_DY 58
#define WUBU_WIDGETS_WEATHER_H   76
#define WUBU_WIDGETS_WEATHER_L1_DY 24
#define WUBU_WIDGETS_WEATHER_L2_DY 42
#define WUBU_WIDGETS_SYSTEM_H    112
#define WUBU_WIDGETS_SYSTEM_CPU_DY 24
#define WUBU_WIDGETS_SYSTEM_GPU_DY 40
#define WUBU_WIDGETS_SYSTEM_SSD_DY 56
#define WUBU_WIDGETS_SYSTEM_RAM_DY 72

#define WUBU_TASKBAR_H     WUBU_METRIC_TASKBAR_H   /* taskbar: canonical */

/* Modern (Win11-class) taskbar. Indicator measurements from the reference
 * shell spec (3px pill, 2px above the button bottom, 6 idle / 16 focused). */
#define WUBU_TASKBAR_BTN_MIN_W        44
#define WUBU_TASKBAR_BTN_MAX_W        160
#define WUBU_TASKBAR_SHELL_W          34
#define WUBU_TASKBAR_CLOCK_W          64
#define WUBU_TASKBAR_TILE_INSET       8   /* bar height minus tile height */
#define WUBU_TASKBAR_INDICATOR_H      3
#define WUBU_TASKBAR_INDICATOR_W_IDLE 6
#define WUBU_TASKBAR_INDICATOR_W_FOCUS 16
#define WUBU_TASKBAR_INDICATOR_GAP    2
#define WUBU_TASKBAR_INDICATOR_IDLE   0x9A9A9Au /* running-unfocused pill */

/* Classic (Win98/XP) taskbar: era-accurate values, named so the surface
 * holds no geometry literals. Bevels stay — they ARE the era look. */
#define WUBU_TASKBAR_CLASSIC_BTN_H     22
#define WUBU_TASKBAR_CLASSIC_BTN_MIN_W 120
#define WUBU_TASKBAR_CLASSIC_BTN_MAX_W 200
#define WUBU_TASKBAR_CLASSIC_BTN_GAP   3   /* off-grid: Win98 button pitch */
#define WUBU_TASKBAR_CLASSIC_BTN_PAD   24  /* title padding either side */
#define WUBU_TASKBAR_CLASSIC_TEXT_RESERVE 26 /* padding + icon gap */
#define WUBU_TASKBAR_CLASSIC_START_W   60
#define WUBU_TASKBAR_CLASSIC_START_W_LUNA 54
#define WUBU_TASKBAR_CLASSIC_LUNA_EXTRA 20 /* Luna orb wider than its label */
#define WUBU_TASKBAR_CLASSIC_LUNA_H    24
#define WUBU_TASKBAR_CLASSIC_PIN_W     120
#define WUBU_TASKBAR_CLASSIC_PIN_MARK  6   /* pin marker square */
#define WUBU_TASKBAR_CLASSIC_CLOCK_H   20
#define WUBU_TASKBAR_CLASSIC_CLOCK_PAD 3   /* sunken well inset */
#define WUBU_TASKBAR_CLASSIC_EDGE_PAD  10  /* screen-edge margins */
#define WUBU_TASKBAR_CLASSIC_CLOCK_FLOOR 68 /* right of the Start button */
#define WUBU_TASKBAR_TRAY_ICON         16  /* glyph inside a tray tile */
#define WUBU_TASKBAR_FLAG_SIZE         14  /* the 4-pane Start emblem */
#define WUBU_TASKBAR_BADGE     12
#define WUBU_TASKBAR_BADGE_BG  0xFF0000u /* era notification red */
#define WUBU_TASKBAR_BADGE_FG  0xFFFFFFu
#define WUBU_TASKBAR_CLASSIC_PAGER_W   14
#define WUBU_TASKBAR_CLASSIC_PAGER_H   16
#define WUBU_TASKBAR_CLASSIC_PAGER_PITCH 16
#define WUBU_TASKBAR_CLASSIC_PAGER_RESERVE 150
#define WUBU_MENU_PAD      WUBU_SPACE_MD   /* menu content inset */
#define WUBU_MENU_ROW_H    WUBU_METRIC_ROW_H /* menu row = canonical row height
                                              * (was a stale 26 here while the
                                              * surface drew 24 — two truths) */
#define WUBU_MENU_W        200   /* classic Start main panel width */
#define WUBU_SUBMENU_W     180   /* classic cascading submenu width */
#define WUBU_MENU_BORDER_W WUBU_BORDER_THIN

/* Menu checkbox: theme-independent control art (white box, dark mark reads
 * on every menu surface). Named so no surface re-invents the literals. */
#define WUBU_MENU_CHECK_BG     0xFFFFFFu
#define WUBU_MENU_CHECK_FG     0x000000u
#define WUBU_MENU_CHECK_W      10
#define WUBU_MENU_CHECK_H      8
#define WUBU_MENU_CHECK_MARK_W 6
#define WUBU_MENU_CHECK_MARK_H 4

/* Classic Start sidebar (XP-era banner). */
#define WUBU_START_SIDEBAR_W        48
#define WUBU_START_SIDEBAR_HDR_H    26
#define WUBU_START_SIDEBAR_TEXT_DIM 0xDDDDDDu
#define WUBU_START_LOGOFF_DY        30   /* footer text offsets from menu bottom */
#define WUBU_START_POWEROFF_DY      18

/* Win11-class Start panel: structural values. Pre-parity measurements —
 * Tier 1.2 of docs/compendium/04-roadmap/gui-slerm-impossible-os.md moves
 * them to the reference layout HERE, in one place, renderer + click path
 * both consuming them. */
#define WUBU_START_W            520
#define WUBU_START_H            560
#define WUBU_START_SEARCH_H     28
#define WUBU_START_TILE         72
#define WUBU_START_COLS         6
#define WUBU_START_ROWS         2
#define WUBU_START_GRID_DY      72   /* panel top -> pinned grid */
#define WUBU_START_REC_ITEM_H   44
#define WUBU_START_REC_DY       14   /* grid bottom -> Recommended label */
#define WUBU_START_FOOT_H       44
#define WUBU_START_POWER_W      36
#define WUBU_START_POWER_H      28
#define WUBU_START_RESULT_H     40   /* search result row */
#define WUBU_START_RESULT_PITCH 46   /* row + gap */
#define WUBU_START_SEARCH_PAD_X 10   /* placeholder/query text inset */
#define WUBU_START_SEARCH_QUERY_X 80 /* query text after the Search label */
#define WUBU_START_RESULT_DY    14   /* search bar -> results label */
#define WUBU_START_RESULT_LABEL_DY 18 /* results label -> first row */
#define WUBU_START_TILE_ICON_DY 6    /* tile top -> icon */
#define WUBU_START_TILE_LABEL_DY 46  /* tile top -> label */
#define WUBU_START_FOOT_MIN_H   80   /* min panel height for the XP footer */
#define WUBU_MENU_RECENT_ROW_H  22   /* classic Recently Used row */
#define WUBU_MENU_RECENT_HDR_DY 20   /* Recently Used heading -> first row */
#define WUBU_MENU_ITEM_PAD_X    6    /* item label inset (off-grid: era look) */
#define WUBU_MENU_ITEM_TEXT_RESERVE 20 /* arrow + padding reserved from width */

/* Type sizes (the VBE draws at a fixed 8px cell; these are cap heights used
 * for vertical centring, not point sizes). */
#define WUBU_FONT_CAP_H    8
#define WUBU_FONT_BODY_H   8

/* ---- Legacy banner layout (shared with CLI banner) ---- */

#define WUBU_TOKEN_BANNER_WIDTH  64
#define WUBU_TOKEN_SECTION_PAD   WUBU_SPACE_XS
#define WUBU_TOKEN_LINE_WIDTH    WUBU_TOKEN_BANNER_WIDTH

/* ---- ANSI escape codes (CLI only, shared with wubu_banner.h) ---- */

#define WUBU_ANSI_GREEN   "\x1b[38;2;200;83;255m"
#define WUBU_ANSI_BOLD    "\x1b[1m"
#define WUBU_ANSI_RESET   "\x1b[0m"

#endif /* WUBU_TOKENS_H */
