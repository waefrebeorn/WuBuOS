#ifndef WUBU_TOKENS_H
#define WUBU_TOKENS_H

/*
 * wubu_tokens.h — Unified design tokens for CLI + GUI + themes.
 *
 * Single source of truth for visual identity (ADR-002 in both repos).
 * This is the wubunos (GUI) copy; wubuwizard has its own copy in
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

/* ---- Layout tokens (shared with CLI banner) ---- */

#define WUBU_TOKEN_BANNER_WIDTH  64
#define WUBU_TOKEN_SECTION_PAD   4
#define WUBU_TOKEN_LINE_WIDTH    WUBU_TOKEN_BANNER_WIDTH

/* ---- ANSI escape codes (CLI only, shared with wubu_banner.h) ---- */

#define WUBU_ANSI_GREEN   "\x1b[38;2;200;83;255m"
#define WUBU_ANSI_BOLD    "\x1b[1m"
#define WUBU_ANSI_RESET   "\x1b[0m"

#endif /* WUBU_TOKENS_H */
