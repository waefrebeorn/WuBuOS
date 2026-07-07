/*
 * wubu_session.h -- WuBuOS session management (SteamOS gamescope lesson).
 *
 * DESKTOP (Win98 shell) vs GAME (dedicated, fullscreen, controller-first,
 * shell-bypass) session split. The GAME path launches foreign binaries via
 * the Proton/container route (wubu_launch_windows), never an NT-kernel reimpl.
 */

#ifndef WUBU_SESSION_H
#define WUBU_SESSION_H

#include <stddef.h>
#include "../hosted/hosted.h"   /* hosted_mode_t, hosted_state_t */

/* Human-readable name for a session mode. */
const char *wubu_session_mode_name(hosted_mode_t mode);

/* Enter a dedicated GAME session and launch the binary via the Proton/
 * container path. Returns process id, or -1 on error. */
int wubu_session_launch_game(hosted_state_t *state,
                             const void *data, size_t size,
                             const char *cmdline);

/* Enter the Win98 desktop session (comfy default; shell owns the screen). */
void wubu_session_enter_desktop(hosted_state_t *state);

#endif /* WUBU_SESSION_H */
