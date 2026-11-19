/*
 * guests_panel.h -- L8 desktop Guests window: imported roots + Launch.
 * The window is a view over the ONE import store (wubu_guest_import) and
 * launches through the ONE validated exec path (wubu_guest_launch ->
 * wubu_ns_guests_launch -> wubu_ct_*). No private state, no second API.
 */
#ifndef WUBU_GUESTS_PANEL_H
#define WUBU_GUESTS_PANEL_H

#include "wm.h"

/* Create the Guests window (app registry entry point). */
DosGuiWindow *launch_guests(void);

#endif /* WUBU_GUESTS_PANEL_H */
