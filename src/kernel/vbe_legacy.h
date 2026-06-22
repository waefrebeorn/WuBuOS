/*
 * vbe_legacy.h  --  Legacy Win98 VBE API (for backward compatibility)
 *
 * This header provides the old theme-aware VBE API for legacy code
 * (wm.c, theme.c, startmenu.c, wubu_wm.c) that hasn't been migrated
 * to the new theme-agnostic API.
 *
 * NEW CODE SHOULD USE vbe.h with vbe_3d_raised_colors() / vbe_3d_sunken_colors()
 * and pass theme colors from wubu_theme.h.
 */

#ifndef MYSEED_VBE_LEGACY_H
#define MYSEED_VBE_LEGACY_H

#include "vbe.h"

/* Legacy Win98 color constants */
#define C_WIN_DESKTOP   0x008080
#define C_WIN_FACE      0x00C0C0C0
#define C_WIN_TITLE     0x00000080
#define C_WIN_TITLE_INA 0x00808080
#define C_WIN_TITLE_FG  0x00FFFFFF
#define C_WIN_BORDER_LT 0x00FFFFFF
#define C_WIN_BORDER_DK 0x00808080
#define C_WIN_BORDER_DD 0x00000000
#define C_WIN_TEXT       0x00000000
#define C_WIN_HILITE    0x00000080

/* Legacy 3D borders (use hardcoded Win98 colors) */
void vbe_3d_raised(int x, int y, int w, int h);
void vbe_3d_sunken(int x, int y, int w, int h);

#endif /* MYSEED_VBE_LEGACY_H */