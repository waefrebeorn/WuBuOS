/*
 * run_dialog.h -- the Win+R run dialog (Windows 11 parity).
 *
 * A small modal overlay: type an app name (or a path) and Enter launches it
 * through the same registry path the Start menu uses.  Escape or a press
 * outside the dialog dismisses it.
 */

#ifndef WUBU_RUN_DIALOG_H
#define WUBU_RUN_DIALOG_H

#include <stdint.h>
#include <stdbool.h>

void run_dialog_open(void);
void run_dialog_close(void);
bool run_dialog_is_open(void);

/* Append/backspace/enter/escape.  Returns nothing: Enter launches via
 * launch_app(), which the shell owns. */
void run_dialog_handle_key(uint32_t key, uint32_t mods);

/* A press outside the dialog's box closes it; inside is swallowed. */
bool run_dialog_handle_click(int x, int y);

/* Geometry of the box, so the WM and the pointer path agree. */
void run_dialog_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h);

void run_dialog_render(uint32_t *fb, int fb_w, int fb_h);

#endif /* WUBU_RUN_DIALOG_H */
