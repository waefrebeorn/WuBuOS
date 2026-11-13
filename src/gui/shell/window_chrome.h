/*
 * window_chrome.h  --  Standardized Window Chrome API
 *
 * Centralized window decoration drawing for consistent WuBuOS apps.
 * Note: ChromeButton enum and core chrome functions are declared in ../wm.h
 * This file only declares the supplementary chrome drawing helpers.
 */

#ifndef WUBU_DOSGUI_WINDOW_CHROME_H
#define WUBU_DOSGUI_WINDOW_CHROME_H

#include "../wm.h"

/* -- Standardized button drawing ------------------------------------ */

/* Draw a standard 3D button in app content area.
 * pressed=true -> sunken; pressed=false -> raised. */
void chrome_draw_button(int x, int y, int w, int h,
                               const char *label, bool pressed);

/* -- Title-bar button geometry -------------------------------------- */

/* One owner for the three title-bar buttons, so the renderer and every
 * hit-test (chrome + WM) cannot drift apart.  Rectangles are ABSOLUTE
 * screen coordinates. */
typedef struct {
    int x, y, w, h;
} ChromeButtonRect;

bool chrome_button_rects(DosGuiWindow *win,
                         ChromeButtonRect *min,
                         ChromeButtonRect *max,
                         ChromeButtonRect *close);

/* -- Caption metrics -------------------------------------------------- */

/* The caption geometry as it will actually be drawn.  Tests and any surface
 * that must align with the caption (a snap flyout, a tooltip under the Close
 * button) read this instead of recomputing it. */
typedef struct {
    int bar_h;   /* caption bar height */
    int btn_w;   /* one caption button width */
    int btn_h;   /* caption button height */
    int gap;     /* space between buttons, 0 when flush */
} ChromeCaptionMetrics;

ChromeCaptionMetrics chrome_caption_metrics(void);

/* The frame border width for the current theme (1 or 2). */
int chrome_border_width(void);

/* True when the Windows 11 caption treatment is active: flush full-height
 * buttons at the top-right corner rather than inset square ones. */
bool chrome_is_win11(void);

/* -- Text edit field chrome ----------------------------------------- */

void chrome_draw_edit_field(int x, int y, int w, int h);

/* -- Status bar chrome ---------------------------------------------- */

void chrome_draw_statusbar(int x, int y, int w, int h,
                                  const char *left_text,
                                  const char *right_text);

/* -- Progress bar chrome -------------------------------------------- */

void chrome_draw_progress(int x, int y, int w, int h, int percent);

/* -- Tab control chrome --------------------------------------------- */

void chrome_draw_tabs(int x, int y, int w, int h,
                             const char **labels, int count, int active_tab);

#endif /* WUBU_DOSGUI_WINDOW_CHROME_H */