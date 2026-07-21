/*
 * dosgui_window_chrome.h  --  Standardized Window Chrome API
 *
 * Centralized window decoration drawing for consistent WuBuOS apps.
 * Note: ChromeButton enum and core chrome functions are declared in dosgui_wm.h
 * This file only declares the supplementary chrome drawing helpers.
 */

#ifndef WUBU_DOSGUI_WINDOW_CHROME_H
#define WUBU_DOSGUI_WINDOW_CHROME_H

#include "dosgui_wm.h"

/* -- Standardized button drawing ------------------------------------ */

/* Draw a standard 3D button in app content area.
 * pressed=true -> sunken; pressed=false -> raised. */
void dosgui_chrome_draw_button(int x, int y, int w, int h,
                               const char *label, bool pressed);

/* -- Text edit field chrome ----------------------------------------- */

void dosgui_chrome_draw_edit_field(int x, int y, int w, int h);

/* -- Status bar chrome ---------------------------------------------- */

void dosgui_chrome_draw_statusbar(int x, int y, int w, int h,
                                  const char *left_text,
                                  const char *right_text);

/* -- Progress bar chrome -------------------------------------------- */

void dosgui_chrome_draw_progress(int x, int y, int w, int h, int percent);

/* -- Tab control chrome --------------------------------------------- */

void dosgui_chrome_draw_tabs(int x, int y, int w, int h,
                             const char **labels, int count, int active_tab);

#endif /* WUBU_DOSGUI_WINDOW_CHROME_H */