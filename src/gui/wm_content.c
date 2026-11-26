/* wm_content.c -- where an app may paint, asked once instead of guessed per app.
 *
 * The chrome clips every on_draw to a rect it computes; it stores that rect on the window. Apps that
 * recomputed it from a hardcoded caption height (win->y + 22, win->y + 28, win->y + tbh) drew their
 * top rows underneath the title bar, where the clip silently discarded them: Paint and the generic
 * app placeholder both lost their toolbars that way. This is the one answer, and it pulls in no WM
 * state, so test graphs can link it.
 */
#include "wm.h"

void wm_content_rect(const DosGuiWindow *w, int *x, int *y, int *w_out, int *h) {
    int cx, cy, cw, ch;
    if (w && w->content_w > 0 && w->content_h > 0) {
        cx = w->content_x; cy = w->content_y; cw = w->content_w; ch = w->content_h;
    } else if (w) {
        /* No chrome has drawn yet (a headless test, or the first frame): the whole window is the
         * content. Better a slightly-too-large rect than a negative one. */
        cx = w->x; cy = w->y; cw = w->w; ch = w->h;
        if (cw < 1) cw = 1;
        if (ch < 1) ch = 1;
    } else {
        cx = cy = 0; cw = ch = 0;
    }
    if (x) *x = cx;
    if (y) *y = cy;
    if (w_out) *w_out = cw;
    if (h) *h = ch;
}
