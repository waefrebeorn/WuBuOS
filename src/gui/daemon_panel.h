/*
 * daemon_panel.h  --  WuBuOS Desktop Daemon Integration Panel
 *
 * Cell 400-402: Bridges wubu_archd and wubu_holyd events into the DosGui
 * desktop. System tray icons, container list window, HolyC session window.
 */

#ifndef WUBU_DOSGUI_DAEMON_PANEL_H
#define WUBU_DOSGUI_DAEMON_PANEL_H

#include <stdint.h>

/* -- Lifecycle ---------------------------------------------------- */

int  daemon_panel_init(void);
void daemon_panel_shutdown(void);

/* -- Tick (called each frame from desktop_tick) ----------- */

void daemon_panel_tick(void);

/* -- Query ------------------------------------------------------- */

int        daemon_panel_archd_state(void);   /* 0=disconnected, 1=connecting, 2=connected, 3=error */
int        daemon_panel_holyd_state(void);

int        daemon_panel_container_count(void);
const char *daemon_panel_container_name(int idx);
const char *daemon_panel_container_state(int idx);

int        daemon_panel_holyd_session_count(void);
const char *daemon_panel_holyd_session_name(int idx);

#endif /* WUBU_DOSGUI_DAEMON_PANEL_H */
