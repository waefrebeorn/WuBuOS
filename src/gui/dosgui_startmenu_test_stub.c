/*
 * dosgui_startmenu_test_stub.c  --  Stubs for dosgui_startmenu_test without desktop integration
 */

#include "dosgui_startmenu.h"
#include "wubu_theme.h"
#include "wubu_mime.h"
#include "wubu_host_exec.h"
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
void dosgui_platform_shutdown(void) { }

/* -- Bonzi stub (startmenu test doesn't need the full mascot) -- */
#include "wubu_bonzi.h"
#include "dosgui_wm.h"  /* for DosGuiWindow type */
/* wubu_bonzi.h already declares set_enabled/is_enabled; the full
 * implementation is in wubu_bonzi.c (linked in). But the AGI gateway
 * (wubu_bonzi_open_agi) calls dosgui_wm_spawn_holyd_term + set_focus,
 * which are not linked here.  Provide a no-op so the symbol resolves. */
void dosgui_wm_set_focus(DosGuiWindow *w) { (void)w; }
/* dosgui_wm_spawn_holyd_term: no-op for menu test (Bonzi won't be clicked). */
DosGuiWindow *dosgui_wm_spawn_holyd_term(int x, int y, int w, int h) {
    (void)x; (void)y; (void)w; (void)h; return NULL;
}

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

/* ---- era-apps launcher stubs ----
 * This GUI-only test links dosgui_startmenu.c (which calls
 * dosgui_era_apps_register()) and dosgui_era_apps.c, but it does NOT pull in
 * the runtime exec backends (wubu_dos_proc / wubu_exec_*). Those are exercised
 * end-to-end by the runtime-layer test_era_apps target. Here we provide
 * link-compatible no-ops so the startmenu registry test stays self-contained. */
#include "wubu_exec.h"
#include "wubu_dos_proc.h"
WubuDosProc *wubu_dos_proc_launch(const char *dos_path, int fmt) {
    (void)dos_path; (void)fmt; return NULL;
}
int64_t wubu_exec_win_pe(const void *pe_data, size_t pe_size) {
    (void)pe_data; (void)pe_size; return -1;
}
int64_t wubu_exec_linux_elf(const void *elf_data, size_t elf_size) {
    (void)elf_data; (void)elf_size; return -1;
}
int64_t wubu_exec_holyd(const char *source, size_t source_size) {
    (void)source; (void)source_size; return -1;
}
/* mach-o backend + container isolation (referenced by era-apps; the test
 * builds the program DB but never launches). Signatures match wubu_host_exec.h. */
int64_t wubu_exec_macho(const void *macho, size_t macho_size) {
    (void)macho; (void)macho_size; return -1;
}
void wubu_ct_destroy(WubuCt *ct) { (void)ct; }
int  wubu_ct_start(WubuCt *ct) { (void)ct; return -1; }
const char *wubu_ct_runtime_name(CtRuntime runtime) { (void)runtime; return "stub"; }