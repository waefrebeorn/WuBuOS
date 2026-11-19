/*
 * desktop.h  --  WuBuOS DosGui Desktop
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
#include "../../apps/apps.h"   /* DESK_ICON_* ids: ONE owner (apps registry).
                                * This header used to re-define the whole enum
                                * with COUNT 9 while apps.h said 8 — dynamic
                                * icons then collided with DESK_ICON_DOOM. */

/* -- Lifecycle ---------------------------------------------------- */

int  desktop_init(void);
void desktop_shutdown(void);
void desktop_render(uint32_t *fb, int fb_w, int fb_h);

/* -- Launch ------------------------------------------------------ */

/* Launch an app from its desktop icon. Creates a window via wm. */
void desktop_launch(int icon_id);
/* Launch whatever a desktop icon points at, by its TYPE.  Shortcuts run the
 * Exec line of their .desktop file; files and folders open through the MIME
 * system.  Previously DESK_ICON_SHORTCUT was created but never read, so
 * clicking a shortcut fell through to launch_app(name) and did nothing. */
void desktop_launch_icon(int icon_id);

/* Launch by name (for start menu) */
void launch_app(const char *name);

/* System shutdown (triggers hosted exit) */
void wubu_shutdown();

/* -- Tick (called each frame) ------------------------------------ */

void desktop_tick(void);

#endif /* WUBU_DOSGUI_DESKTOP_H */
