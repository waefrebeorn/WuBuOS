/*
 * control_panel_text.h -- stride-aware text and fill for Control Panel applets.
 *
 * ONE owner for applet drawing. Before this each applet either ignored the
 * framebuffer entirely (display/network/theme drew nothing; hardware's
 * draw_line() voided every argument) or guessed a stride: cp_sound wrote
 * fb[(y) * 800 + x], which is only correct on an 800px-wide screen and lands
 * the volume bar in the wrong place on 1024x768 or 1920x1080.
 *
 * The render callback now receives the surface width, and everything draws
 * through here.
 */
#ifndef WUBU_CP_TEXT_H
#define WUBU_CP_TEXT_H

#include <stdint.h>

/* Fill a clipped rectangle. Stride-aware; safe at any surface size. */
void cp_fill(uint32_t *fb, int fb_w, int fb_h,
             int x, int y, int w, int h, uint32_t colour);

/* Draw one line of text with the kernel's 8x8 font (ASCII 32..127, upper and
 * lower case). Returns the pixel width drawn. Clipped to fb_w/fb_h. */
int cp_text_line_h(void);
int cp_draw_text_bold(uint32_t *fb, int fb_w, int fb_h,
                      int x, int y, const char *s, uint32_t colour, int scale);
int cp_draw_text(uint32_t *fb, int fb_w, int fb_h,
                 int x, int y, const char *s, uint32_t colour, int scale);

/* Width in pixels that cp_text() would draw for `s` at `scale`. */
int cp_text_width(const char *s, int scale);

/* Draw text wrapped on spaces at max_w. Line height is 10*scale. */
void cp_text_wrapped(uint32_t *fb, int fb_w, int fb_h, int x, int y, int max_w,
                     const char *s, uint32_t colour, int scale);

/* A label/value row, the shape every applet needs. */
void cp_row(uint32_t *fb, int fb_w, int fb_h, int x, int y, int max_w,
            const char *label, const char *value,
            uint32_t label_colour, uint32_t value_colour);

#endif /* WUBU_CP_TEXT_H */
