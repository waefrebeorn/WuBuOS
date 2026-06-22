/*
 * dosgui_startmenu_test_stub.c  --  Stubs for dosgui_startmenu_test without desktop integration
 */

#include "dosgui_startmenu.h"
#include "wubu_theme.h"
#include <stdbool.h>

/* Stub for taskbar height */
int dosgui_taskbar_height(void) {
    return 28;
}

/* Stub for screen h/w */
int dosgui_wm_screen_h(void) {
    return 768;
}
int dosgui_wm_screen_w(void) {
    return 1024;
}

/* Stub for launch app */
void dosgui_launch_app(const char *name) { }

/* Stub for shutdown */
void dosgui_shutdown(void) { }