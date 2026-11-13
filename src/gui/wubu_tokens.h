#ifndef WUBU_TOKENS_H
#define WUBU_TOKENS_H

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

#define WUBU_SPACE_2XS    2
#define WUBU_SPACE_XS     4
#define WUBU_SPACE_SM     8
#define WUBU_SPACE_MD     12
#define WUBU_SPACE_LG     16
#define WUBU_SPACE_XL     24
#define WUBU_SPACE_2XL    32

/* Corner radii.  Win11 uses 8 at 100% scale for windows and menus. */
#define WUBU_RADIUS_NONE  0
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

#define WUBU_TITLE_BAR_H   32   /* Win11 caption height */
#define WUBU_TITLE_BTN_W   46   /* caption button width */
#define WUBU_TITLE_BTN_H   32   /* flush with the caption bar */
#define WUBU_TITLE_PAD     WUBU_SPACE_SM   /* icon and title inset */

#define WUBU_TASKBAR_H     48   /* Win11 taskbar height */
#define WUBU_MENU_PAD      WUBU_SPACE_MD   /* menu content inset */
#define WUBU_MENU_ROW_H    26

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
