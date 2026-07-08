/*
 * wubu_welcome.h  --  WuBuOS Welcome Dialog (UX Stream E)
 *
 * First-run dialog shown on initial boot. Displays keyboard shortcuts,
 * version info, and a "Don't show again" toggle. Creates a marker file
 * at ~/.config/wubu/first-run-done after dismissal.
 */

#ifndef WUBU_WELCOME_H
#define WUBU_WELCOME_H

#include <stdbool.h>

/*
 * Initialize the welcome system. Call once after WM + desktop init.
 * If this is the first run, spawns a modal welcome dialog window.
 * Returns 1 if dialog was shown, 0 if not (already acknowledged).
 */
int wubu_welcome_init(void);

/*
 * Check if the welcome dialog has been acknowledged.
 * Returns true if the marker file exists.
 */
bool wubu_welcome_is_dismissed(void);

/*
 * Mark the welcome dialog as dismissed (creates marker file).
 */
void wubu_welcome_dismiss(void);

#endif /* WUBU_WELCOME_H */