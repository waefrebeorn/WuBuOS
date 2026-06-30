/*
 * dosgui_startmenu_test_stub.c  --  Stubs for dosgui_startmenu_test without desktop integration
 */

#include "dosgui_startmenu.h"
#include "wubu_theme.h"
#include "wubu_mime.h"
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

/* Theme functions come from wubu_theme.c - no stubs needed */

/* MIME stubs - needed for desktop parsing */
int wubu_mime_init(void) { return 0; }
void wubu_mime_shutdown(void) { }
MimeSystem *wubu_mime_state(void) { 
    static MimeSystem mime = {0};
    return &mime;
}
int wubu_mime_load_desktop_file(const char *path) { return 0; }

/* Inline layout helpers - from dosgui_startmenu.c */
static inline int menu_item_h(void) {
    return 24;  /* EX_ROW_H equivalent */
}

static inline int submenu_w(void) {
    return 180;  /* EX_SUBMENU_W equivalent */
}

static inline int menu_w(void) {
    return 200;  /* EX_MENU_W equivalent */
}

static inline int sidebar_w(void) {
    return 48;  /* EX_SIDEBAR_W equivalent */
}

static inline int menu_border_w(void) {
    return 2;
}

static inline int ex_tree_indent(void) {
    return 14;
}

static inline int ex_title_h(void) {
    return 22;
}

static inline int ex_toolbar_h(void) {
    return 28;
}

static inline int ex_breadcrumb_h(void) {
    return 24;
}

static inline int ex_statusbar_h(void) {
    return 24;
}

static inline int ex_border_w(void) {
    return 2;
}


int wubu_mime_scan_desktop_dirs(void) { return 0; }