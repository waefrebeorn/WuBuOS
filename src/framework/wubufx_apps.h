/*
 * wubufx_apps.h -- WuBuFX application registry (real engines, no placeholders)
 *
 * Phase C binding: the desktop shell / start menu launch apps THROUGH
 * WuBuFX. Each app is a content-addressed, capability-scoped, EDR-disclosed
 * namespace that spawns a genuine engine (calc / control / explorer /
 * notepad / terminal / canvas / dos-box / EDR dash). See wubufx_apps.c.
 */

#ifndef WUBUFX_APPS_H
#define WUBUFX_APPS_H

#include "../gui/dosgui_wm.h"   /* DosGuiWindow */

/* Launch a named app as a WuBuFX namespace; returns the engine's window. */
DosGuiWindow *wubufx_app_launch(const char *name);

/* Number of registered apps + name lookup (for shell population). */
int         wubufx_app_count(void);
const char *wubufx_app_name(int i);

#endif /* WUBUFX_APPS_H */
