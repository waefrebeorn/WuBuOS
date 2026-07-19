/*
 * app_draw.h  --  WuBuOS App Draw Functions
 *
 * Provides content render callbacks for DosGui WM windows.
 * Each app type gets a draw function compatible with dosgui_wm's on_draw callback.
 */

#ifndef WUBU_APP_DRAW_H
#define WUBU_APP_DRAW_H

#include <stdint.h>
#include "dosgui_wm.h"
#include "../kernel/vbe.h"

/* App content draw functions - signature matches DosGuiWindow.on_draw */
void app_draw_calc(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void app_draw_notepad(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void app_draw_terminal(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void app_draw_explorer(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void app_draw_settings(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void app_draw_repl(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void app_draw_canvas(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void app_draw_editor(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void app_draw_filemgr(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void app_draw_codec(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);

/* App state getters (for input handling) */
void app_calc_init(void);
void app_notepad_init(void);
void app_terminal_init(void);
void app_explorer_init(void);
void app_settings_init(void);
void app_repl_init(void);
void app_canvas_init(void);
void app_editor_init(void);
void app_filemgr_init(void);
void app_codec_init(void);

/*
 * NOTE: The Wolfenstein-3D-style raycaster (src/apps/doom.c) and the MS-Paint
 * toy (src/apps/paint.c) were removed. The image editor is the layered
 * wubu_canvas engine (app_draw_canvas). No paint/doom draw hooks remain.
 */

#endif