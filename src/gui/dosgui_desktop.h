/*
 * dosgui_desktop.h  --  WuBuOS DosGui Desktop
 *
 * Cell 401: The Win98 desktop that ties everything together.
 * Desktop icons, wallpaper, integration with DosGui WM.
 *
 * This is the "WuBuOS desktop" — the Arch-hosted GUI shell
 * that bundles ZealOS in-process and runs Fable-derived apps
 * in windows. Part of the "lossless C11 wrapper" paradigm:
 * the entire OS is one clickable binary.
 */

#ifndef WUBU_DOSGUI_DESKTOP_H
#define WUBU_DOSGUI_DESKTOP_H

#include <stdint.h>

/* -- Desktop Icon Types ----------------------------------------- */

#define DESK_ICON_MY_COMPUTER   0
#define DESK_ICON_TEMPLE_REPL  1
#define DESK_ICON_NOTEPAD       2
#define DESK_ICON_PAINT         3
#define DESK_ICON_CALCULATOR    4
#define DESK_ICON_TERMINAL      5
#define DESK_ICON_EXPLORER      6
#define DESK_ICON_SETTINGS      7
#define DESK_ICON_DOOM          8
#define DESK_ICON_COUNT         9

/* -- Lifecycle ---------------------------------------------------- */

int  dosgui_desktop_init(void);
void dosgui_desktop_shutdown(void);
void dosgui_desktop_render(uint32_t *fb, int fb_w, int fb_h);

/* -- Launch ------------------------------------------------------ */

/* Launch an app from its desktop icon. Creates a window via dosgui_wm. */
void dosgui_desktop_launch(int icon_id);

/* Launch by name (for start menu) */
void dosgui_launch_app(const char *name);

/* System shutdown (triggers hosted exit) */
void dosgui_shutdown(void);

/* -- Tick (called each frame) ------------------------------------ */

void dosgui_desktop_tick(void);

#endif /* WUBU_DOSGUI_DESKTOP_H */
