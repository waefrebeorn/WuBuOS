/*
 * wubu_mascot.h -- WuBu Buddy sprite blitter.
 *
 * The desktop mascot is baked clipart (tools/mascot_encode.py turns the
 * official sheet into an RGBA table -- see wubu_buddy_sprite.h). This
 * module owns exactly one thing: putting those pixels on the desktop
 * with alpha. It knows nothing about animation, hit-testing or bubbles.
 *
 * It draws through vbe_* like the rest of the WM, so it always lands in
 * the VBE back buffer regardless of which caller is rendering.
 *
 * C11, opaque API, no god headers.
 */

#ifndef WUBU_MASCOT_H
#define WUBU_MASCOT_H

/* Blit the mascot with its top-left corner at desktop (x,y).
 * Pixels with alpha 0 are skipped, so the clipart's white background
 * never paints a box on the wallpaper. */
void wubu_mascot_blit(int x, int y);

/* Blit any baked pose (RGBA8888, size x size) at desktop (x,y). The
 * buddy's states all share one blitter; only the pixels differ. */
void wubu_mascot_blit_rgba(int x, int y, const unsigned char *rgba, int size);

/* The sprite's square size in pixels. */
int wubu_mascot_size(void);

#endif /* WUBU_MASCOT_H */
