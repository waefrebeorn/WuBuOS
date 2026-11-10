/*
 * term_test_stub.c  --  Stubs for term_test without full dependencies
 *
 * Provides minimal implementations of external dependencies
 * so the terminal test can compile and run without the full WM/Kernel.
 */

#include "term.h"
#include "wm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -- DosGui WM Stubs --------------------------------------------------------------- */

static DosGuiWindow g_mock_win = {0};
static int g_mock_win_id = 1;

int taskbar_height(void) { return 28; }

void taskbar_render(uint32_t *fb, int fb_w, int fb_h) {}

DosGuiWindow *wm_find_by_id(int id) {
    if (id == g_mock_win_id) return &g_mock_win;
    return NULL;
}

DosGuiWindow *wm_create(int x, int y, int w, int h, const char *title) {
    g_mock_win.x = x; g_mock_win.y = y; g_mock_win.w = w; g_mock_win.h = h;
    g_mock_win.id = g_mock_win_id;
    g_mock_win.alive = true;
    g_mock_win.user_data = NULL;
    if (title) strncpy(g_mock_win.title, title, sizeof(g_mock_win.title)-1);
    return &g_mock_win;
}

void wm_destroy(DosGuiWindow *win) {
    (void)win;
    g_mock_win.alive = false;
}

/* -- Clipboard Stubs ---------------------------------------------- */

void wubu_clipboard_init(void) {}
void wubu_clipboard_shutdown(void) {}
void wubu_clipboard_set_text(const char *text) {}
const char *wubu_clipboard_get_text(void) { return ""; }

/* -- Notify Stubs ------------------------------------------------- */

void wubu_notify_init(void) {}
void wubu_notify_shutdown(void) {}
void wubu_notify_show(const char *title, const char *body, int timeout_ms) {}
void wubu_notify_handle_mouse(int x, int y, int btn, int kind) {}

/* -- Session Stubs ------------------------------------------------ */

void wubu_session_init(void) {}
void wubu_session_shutdown(void) {}
void wubu_session_save(void) {}
void wubu_session_restore(void) {}

/* -- Screenshot Stubs --------------------------------------------- */

void wubu_screenshot_init(void) {}
void wubu_screenshot_take(int x, int y, int w, int h, const char *path) {}

/* -- Styx Stubs --------------------------------------------------- */

void *styx_open(const char *path, int flags) { return NULL; }
int styx_read(void *fh, void *buf, size_t count) { return 0; }
int styx_write(void *fh, const void *buf, size_t count) { return 0; }
int styx_close(void *fh) { return 0; }
int styx_seek(void *fh, long offset, int whence) { return 0; }
int styx_stat(const char *path, void *stbuf) { return -1; }
int styx_readdir(void *fh, void *dirent) { return 0; }
int styx_mkdir(const char *path, int mode) { return 0; }
int styx_unlink(const char *path) { return 0; }
int styx_rmdir(const char *path) { return 0; }
int styx_rename(const char *oldpath, const char *newpath) { return 0; }