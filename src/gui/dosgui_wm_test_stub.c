/*
 * dosgui_wm_test_stub.c  --  Stubs for WM unit tests without full desktop
 * integration.
 *
 * Provides no-op implementations of start-menu / launch / shutdown hooks that
 * the WM and context-menu engine call. These let unit/integration tests link
 * without the full hosted binary or start-menu subsystem.
 *
 * IMPORTANT: dosgui_wm_handle_mouse is deliberately NOT defined here. The real
 * implementation in dosgui_wm_input.c must bind, so that input (and the wubu_ui
 * automation layer) drives the genuine window manager rather than a no-op.
 */

#include "dosgui_wm.h"
#include "dosgui_startmenu.h"
#include "wubu_theme.h"
#include "../hosted/hosted.h"
#include <stdbool.h>

/* Stub for startmenu toggle */
void dosgui_startmenu_toggle(void) {
    /* No-op for test */
}

/* Stub for startmenu open/close */
void dosgui_startmenu_open(void) { }
void dosgui_startmenu_close(void) { }
void dosgui_startmenu_handle_click(int x, int y) { }
void dosgui_startmenu_track_hover(int x, int y) { }

/* Stub for launch app */
void dosgui_launch_app(const char *name) { (void)name; }

/* Stub for shutdown */
void dosgui_shutdown(void) { }
void dosgui_platform_shutdown(void) { }

/* Hosted-state getter (Play action): no hosted binary in the test harness,
 * so return NULL -- ctx_action_play treats NULL as "no launch". */
hosted_state_t *dosgui_wm_get_hosted_state(void) {
    return NULL;
}
