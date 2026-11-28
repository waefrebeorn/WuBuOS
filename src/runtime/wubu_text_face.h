/* Runtime text face: one face, one glyph cache, strings drawn into a framebuffer.
 *
 * The baked coverage cells (fonts/wubu_font_text.h) exist because bare metal has no filesystem at
 * draw time. This is the other half: when a face is reachable -- hosted, or embedded as a blob --
 * text can be drawn at ANY ppem instead of only the baked one, which is what a type ramp needs.
 *
 * Coordinates: (x, baseline) in pixels. y is the BASELINE, not the box top, because that is what the
 * glyph cache reports bearings against and what every text layout in this tree needs to align
 * mixed sizes on one line.
 */
#ifndef WUBU_TEXT_FACE_H
#define WUBU_TEXT_FACE_H

#include <stddef.h>
#include <stdint.h>

typedef struct WubuTextFace WubuTextFace;

/* The face's bytes are NOT copied: they must outlive the handle (a blob in .rodata, or a buffer the
 * caller keeps). Returns NULL with *why set when the face cannot be parsed. */
WubuTextFace *wubu_text_face_open_memory(const uint8_t *data, size_t len, const char **why);

/* Reads the whole file, owns the buffer, and frees it on close. */
WubuTextFace *wubu_text_face_open_file(const char *path, const char **why);

void wubu_text_face_close(WubuTextFace *f);

/* Draw utf8 at (x, baseline) in ARGB8888. rgb is 0xRRGGBB; coverage does the blending.
 * Returns the advance consumed in pixels, or a negative value if the face cannot draw. */
int wubu_text_face_draw(WubuTextFace *f, uint32_t *fb, int fb_w, int fb_h,
                        int x, int baseline, const char *utf8, int ppem, uint32_t rgb);

/* Advance only, no drawing. Used by layout to measure before it decides where text goes. */
int wubu_text_face_measure(WubuTextFace *f, const char *utf8, int ppem);

/* Vertical extents at a ppem: ascent above the baseline, descent as a negative number. Needed to
 * centre ink in a band -- a box centred on the em is visibly low. */
int wubu_text_face_ascent_px(const WubuTextFace *f, int ppem);
int wubu_text_face_descent_px(const WubuTextFace *f, int ppem);

/* Ink box of a whole string at a ppem, relative to (pen 0, baseline 0), y down. Returns 1 when any
 * glyph has ink. Centring a heading needs THIS, not the hhea extents: cap height is smaller than the
 * ascender and a descender is deeper than most glyphs' ink, so em-centring leaves text visibly low. */
int wubu_text_face_ink_box(WubuTextFace *f, const char *utf8, int ppem,
                           int *x0, int *y0, int *x1, int *y1);

/* Cached-glyph accounting, so a test can prove the cache is actually being used rather than every
 * draw rasterising from scratch. */
void wubu_text_face_stats(const WubuTextFace *f, unsigned *hits, unsigned *misses, unsigned *glyphs);

#endif
