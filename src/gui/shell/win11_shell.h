/*
 * win11_shell.h -- weak glue between the WM and the Win11 shell layers.
 *
 * The WM calls these three hooks.  Each is declared WEAK so a GUI test (or
 * a minimal build) that links wm_render.c / wm_input.c without the Win11
 * modules still links: the hook is simply absent and the layer is skipped.
 * The strong definitions live in win11_shell.c.
 */

#ifndef WUBU_WIN11_SHELL_H
#define WUBU_WIN11_SHELL_H

#include <stdint.h>
#include <stdbool.h>

struct DosGuiWindow;

/* Draw the widgets board and the snap flyout (topmost shell layers). */
extern void win11_shell_render(uint32_t *fb, int fb_w, int fb_h)
    __attribute__((weak));

/* Handle a WM key event; returns true when the shell consumed it. */
extern bool win11_shell_key(uint32_t key, uint32_t mods)
    __attribute__((weak));

/* Handle a WM press; returns true when the shell consumed it. */
extern bool win11_shell_click(int x, int y)
    __attribute__((weak));

#endif /* WUBU_WIN11_SHELL_H */
