/*
 * dosgui_wm_test_stub.c  --  Stubs for dosgui_wm_test without desktop integration
 */

#include "dosgui_wm.h"
#include "dosgui_startmenu.h"
#include "wubu_theme.h"
#include "../hosted/hosted.h"   /* for hosted_state_t (dosgui_wm_get_hosted_state stub) */
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
void dosgui_launch_app(const char *name) { }

/* Stub for shutdown */
void dosgui_shutdown(void) { }

/* Stub for platform shutdown */
void dosgui_platform_shutdown(void) { }

/* Stub for WM mouse handler (called from holyd) — weak to allow override */
__attribute__((weak))
void dosgui_wm_handle_mouse(int x, int y, int btn, int kind) {
    (void)x; (void)y; (void)btn; (void)kind;
}

/* Stub for hosted-state getter (Play action): no hosted binary in the test
 * harness, so return NULL — ctx_action_play treats NULL as "no launch". */
hosted_state_t *dosgui_wm_get_hosted_state(void) {
    return NULL;
}