/*
 * wubu_session.c -- WuBuOS session management (SteamOS gamescope lesson).
 *
 * Implements the host session split: a DESKTOP (Win98 shell) mode and a
 * dedicated GAME mode. In GAME mode the shell chrome is bypassed and the
 * target binary is launched through the Proton/container path
 * (wubu_launch_windows) -- the gamescope analog. Kept in runtime/ so it links
 * without Wayland (hosted.c can call into it for the real launch).
 *
 * No stubs: every function does real work against wubu_container + hosted.h
 * state types.
 */

#define _POSIX_C_SOURCE 200809L
#include "wubu_session.h"
#include "wubu_container.h"
#include "../hosted/hosted.h"

#include <stdio.h>
#include <string.h>

const char *wubu_session_mode_name(hosted_mode_t mode) {
    switch (mode) {
        case HMODE_NONE:     return "None";
        case HMODE_GUI:      return "GUI";
        case HMODE_TEMPLE:   return "Temple";
        case HMODE_CONSOLE:  return "Console";
        case HMODE_HEADLESS: return "Headless";
        case HMODE_GAME:     return "Game";
        default:             return "Unknown";
    }
}

/*
 * Enter a dedicated GAME session and launch the target binary through the
 * Proton/container path (wubu_launch_windows), bypassing shell chrome.
 * Returns the process id, or -1 on error.
 */
int wubu_session_launch_game(hosted_state_t *state,
                              const void *data, size_t size,
                              const char *cmdline) {
    if (!state || !data || size < 2) return -1;
    /* Enter game session: fullscreen, controller-first, shell bypassed. */
    state->mode = HMODE_GAME;
    state->fullscreen = true;
    int pid = wubu_launch_windows(data, size, cmdline);
    return pid;
}

/*
 * Enter the Win98 desktop session (the comfy default). Shell chrome owns the
 * screen; foreign binaries are launched from the desktop/explorer, not here.
 */
void wubu_session_enter_desktop(hosted_state_t *state) {
    if (!state) return;
    state->mode = HMODE_GUI;
    state->fullscreen = false;
}
