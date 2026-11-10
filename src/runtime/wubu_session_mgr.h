/*
 * wubu_session_mgr.h -- P3 session-boot glue: the ONE entry point a login
 * calls to bring up the /n/background rich runtime and mount its full control
 * surface.
 *
 * A real login would call this after the display comes up (the hosted desktop
 * boot); it composes the shipped P3 primitives into the running session:
 *   1. creates the session-manager daemon (bus + logind + polkit wired as the
 *      login power gate),
 *   2. registers the ACTIVE login session,
 *   3. installs the default power policy (uid 1000 may suspend; interactive-
 *      agent required for reboot/shutdown so a user prompt can gate them),
 *   4. starts the XDG autostart apps through an injectable launcher,
 *   5. registers the desktop owner (nemo-desktop role) + the tray slots the
 *      desktop owns (battery/network/heat) into the /n/background/de surface,
 *   6. publishes the WHOLE /n/background tree (bus + session/ + auth/ + de/ +
 *      autostart + session_boot) under g_ns_root.
 *
 * The launcher is injectable so a host can route autostart apps through
 * wubu_exec / a container. The desktop/tray registration lets the GUI painter
 * (systray renders; this records WHO OWNS each slot).
 *
 * C11, runtime host, single-threaded.
 */
#ifndef WUBU_BACKGROUND_H
#define WUBU_BACKGROUND_H

#include <stddef.h>
#include "wubu_session_daemon.h"
#include "wubu_deskenv.h"

typedef struct wubu_session_mgr wubu_session_mgr_t;

/* bring-up entry point. `autostart_dirs` = the XDG autostart dirs to scan,
 * `n_dirs` = their count. Returns 0 on success; -1 on setup failure. The
 * returned handle owns the daemon + de-surface; destroy it to tear down. */
wubu_session_mgr_t *wubu_session_mgr_boot(char *const *autostart_dirs,
                                        int n_dirs,
                                        wubu_session_daemon_launch_fn launcher,
                                        void *launch_ud);
void wubu_session_mgr_shutdown(wubu_session_mgr_t *bg);

/* Registered desktop + tray owners for the /n/background/de surface. */
int wubu_session_mgr_register_desktop(wubu_session_mgr_t *bg, const char *owner);
int wubu_session_mgr_register_tray(wubu_session_mgr_t *bg, const char *slot,
                                  const char *owner);

/* owned components (for policy tweaks / inspection) */
wubu_session_daemon_t *wubu_session_mgr_daemon(wubu_session_mgr_t *bg);
wubu_deskenv_t *wubu_session_mgr_deskenv(wubu_session_mgr_t *bg);

#endif /* WUBU_BACKGROUND_H */
