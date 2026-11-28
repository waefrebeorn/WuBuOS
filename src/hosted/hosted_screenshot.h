/* Screenshot writing for the hosted binary: format chosen by the path's extension.
 *
 * This existed as a PPM-only writer that ignored the extension, so `--screenshot shot.png` produced
 * PPM bytes under a .png name -- a file no viewer opens, and every tool that trusts the name fails
 * confusingly. The tree already had a PNG encoder; the defect was that this path never called it.
 */
#ifndef WUBU_HOSTED_SCREENSHOT_H
#define WUBU_HOSTED_SCREENSHOT_H

#include <stdint.h>

/* Write fb (XRGB8888, w*h) to path. ".png" (case-insensitive) encodes PNG through png_encode_rgba;
 * anything else writes binary PPM (P6). Returns 1 on success, 0 on failure. */
int hosted_screenshot_write(const char *path, const uint32_t *fb, int w, int h);

#endif
