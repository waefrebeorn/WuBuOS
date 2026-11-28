/* See shell_text.h. Owns exactly one face for the whole shell. */
#include "shell_text.h"

#include <stddef.h>

#include "wubu_text_face.h"

static WubuTextFace *g_face;

int shell_text_ready(void) { return g_face != NULL; }

int shell_text_heading_px(void) { return SHELL_TEXT_HEADING_PX; }

int shell_text_init(const char *path)
{
    if (g_face) return 1;
    if (!path) return 0;
    g_face = wubu_text_face_open_file(path, NULL);
    return g_face != NULL;
}

struct WubuTextFace *shell_text_face(void) { return g_face; }

void shell_text_shutdown(void)
{
    if (g_face) wubu_text_face_close(g_face);
    g_face = NULL;
}

int shell_text_measure_heading(const char *s)
{
    if (!g_face || !s) return -1;
    return wubu_text_face_measure(g_face, s, SHELL_TEXT_HEADING_PX);
}

int shell_text_draw_heading(uint32_t *fb, int stride_px, int fb_h,
                            int x, int band_y, int band_h, const char *s, uint32_t rgb)
{
    return shell_text_draw_at(fb, stride_px, fb_h, x, band_y, band_h, s, SHELL_TEXT_HEADING_PX, rgb);
}

int shell_text_draw_at(uint32_t *fb, int stride_px, int fb_h,
                       int x, int band_y, int band_h, const char *s, int px, uint32_t rgb)
{
    if (!g_face || !fb || !s || px <= 0) return -1;

    /* Centre the string's real ink, not the hhea extents. Measured on Selawik at 20px: em-centring
     * left "Settings" 6px low (gap above 9, below 3), because cap height is far below the ascender. */
    int y0 = 0, y1 = 0;
    if (!wubu_text_face_ink_box(g_face, s, px, NULL, &y0, NULL, &y1)) return -1;
    const int ink_h = y1 - y0 + 1;
    const int baseline = band_y + (band_h - ink_h) / 2 - y0;
    return wubu_text_face_draw(g_face, fb, stride_px, fb_h, x, baseline, s, px, rgb);
}
