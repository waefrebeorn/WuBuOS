/*
 * wubu_caption.h -- Luna-style caption painting for WuBuOS.
 *
 * XP's title bars are not a flat colour: they run a vertical gradient from a
 * pale blue at the top edge to a deeper blue at the bottom, and on focus the
 * caption briefly flashes brighter. XPlasma-Plus ships that behaviour as a
 * KDE effect (effects/xp-caption-animation); this is the C11 equivalent with
 * no external state, so it is testable and usable from the kernel WM.
 *
 * Everything is integer maths on 0xRRGGBB values -- no floats in the draw path.
 */
#ifndef WUBU_CAPTION_H
#define WUBU_CAPTION_H

#include <stdint.h>
#include <stdbool.h>
#include "wubu_theme.h"

/* Colour at row y of an h-tall caption. y is clamped to [0, h-1].
 * Active captions run the Luna gradient; inactive ones stay flat and dim so
 * window focus is readable at a glance. */
uint32_t wubu_caption_px(const WubuThemeColors *c, int y, int h, bool active);

/* Focus flash: t in [0,1] over the animation's life. t<=0 or t>=1 returns the
 * base colour exactly (so a stopped animation is indistinguishable from none);
 * the middle of the animation is the brightest. */
uint32_t wubu_caption_focus_glow(uint32_t base, uint32_t accent, double t);

/* Perceived luminance (0..255) of an 0xRRGGBB colour, for contrast checks. */
int wubu_caption_luma(uint32_t rgb);

#endif /* WUBU_CAPTION_H */