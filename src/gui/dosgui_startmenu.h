/*
 * dosgui_startmenu.h  --  WuBuOS DosGui Start Menu
 *
 * Cell 402: Cascading start menu with program groups.
 * Win98-style: Start button → Programs → Accessories → etc.
 */

#ifndef WUBU_DOSGUI_STARTMENU_H
#define WUBU_DOSGUI_STARTMENU_H

#include <stdint.h>

/* -- Lifecycle ---------------------------------------------------- */

void dosgui_startmenu_init(void);
void dosgui_startmenu_shutdown(void);

/* -- State ------------------------------------------------------- */

int  dosgui_startmenu_is_open(void);
void dosgui_startmenu_toggle(void);
void dosgui_startmenu_open(void);
void dosgui_startmenu_close(void);

/* -- Rendering --------------------------------------------------- */

void dosgui_startmenu_render(uint32_t *fb, int fb_w, int fb_h);

/* -- Input ------------------------------------------------------- */

void dosgui_startmenu_handle_click(int x, int y);

#endif /* WUBU_DOSGUI_STARTMENU_H */
