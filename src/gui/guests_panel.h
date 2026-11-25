/*
 * guests_panel.h -- L8 desktop Guests window: imported roots + Launch.
 * The window is a view over the ONE import store (wubu_guest_import) and
 * launches through the ONE validated exec path (wubu_guest_launch ->
 * wubu_ns_guests_launch -> wubu_ct_*). No private state, no second API.
 */
#ifndef WUBU_GUESTS_PANEL_H
#define WUBU_GUESTS_PANEL_H

/* Outcomes of the last click on the panel, so a test can name the cause instead of
 * reporting "no container appeared". */
#define GUESTS_NO_CLICK  (-1000)   /* the handler was never reached (wrong button/kind) */
#define GUESTS_NO_HIT    (-1001)   /* reached, but the click missed every Launch button */

#include "wm.h"

/* Create the Guests window (app registry entry point). */
DosGuiWindow *launch_guests(void);

/* Return code of the last wubu_guest_launch() driven by a click, or GUESTS_NO_*. */
int guests_last_launch_rc(void);

#endif /* WUBU_GUESTS_PANEL_H */
