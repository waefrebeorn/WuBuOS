/* Shell text: display-size headings drawn through our own TrueType engine.
 *
 * The baked 13px cell is the right face for body text and window captions -- Win11's caption step is
 * 12 epx, so 13 is not the deficit. The deficit is HIERARCHY: every heading in the shell is also
 * 13px, so nothing on screen looks like a title. Win11's ramp has 20/28 for exactly that job.
 *
 * This module owns the one place the shell may draw display text. When no face is available (bare
 * metal before the filesystem is up) it reports not-ready and every caller falls back to the baked
 * cell -- the failure mode must never be "blank space where a title should be".
 */
#ifndef WUBU_SHELL_TEXT_H
#define WUBU_SHELL_TEXT_H

#include <stdint.h>

/* The ramp step headings use: "Display semibold 20/28" in Microsoft's epx table. */
#define SHELL_TEXT_HEADING_PX 20

/* 1 when a face is loaded and display text will be drawn, 0 otherwise. */
int shell_text_ready(void);

/* Load the face from path. Returns 1 on success, 0 if it cannot be opened (the caller keeps its
 * baked fallback). Safe to call twice. */
int shell_text_init(const char *path);
void shell_text_shutdown(void);

int shell_text_heading_px(void);

/* Advance of s at the heading size, or -1 when no face is loaded. */
int shell_text_measure_heading(const char *s);

/* Draw s centred vertically in the band [band_y, band_y + band_h) starting at x. Returns the advance
 * consumed, or -1 when no face is loaded -- callers MUST treat -1 as "use the baked cell". */
int shell_text_draw_heading(uint32_t *fb, int stride_px, int fb_h,
                            int x, int band_y, int band_h, const char *s, uint32_t rgb);

#endif
