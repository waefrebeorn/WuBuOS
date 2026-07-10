/* wubu_screenshot_internal.h -- Internal helpers shared by screenshot sub-modules.
 * Public API + types in wubu_screenshot.h. The annotation drawing primitives
 * live in wubu_screenshot_draw.c and are declared here so all submodules link
 * the SAME implementation (no double-coding).
 */

#ifndef WUBU_SCREENSHOT_INTERNAL_H
#define WUBU_SCREENSHOT_INTERNAL_H

#include "wubu_screenshot.h"
#include <stdint.h>

/* -- Annotation drawing primitives (wubu_screenshot_draw.c) ------ */
void draw_line(uint32_t *buf, int w, int h, int x1, int y1, int x2, int y2, uint32_t color, int thickness);
void draw_rect(uint32_t *buf, int w, int h, int x1, int y1, int x2, int y2, uint32_t color, int thickness, uint32_t fill);
void plot_ellipse_points(uint32_t *buf, int w, int h, int cx, int cy, int x, int y, uint32_t color);
void draw_ellipse(uint32_t *buf, int w, int h, int x1, int y1, int x2, int y2, uint32_t color, int thickness, uint32_t fill);
void draw_arrow(uint32_t *buf, int w, int h, int x1, int y1, int x2, int y2, uint32_t color, int thickness);

#endif /* WUBU_SCREENSHOT_INTERNAL_H */
