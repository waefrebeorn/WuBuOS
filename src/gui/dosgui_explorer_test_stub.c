/*
 * dosgui_explorer_test_stub.c  --  Stub functions for dosgui_explorer tests
 *
 * Provides minimal implementations of external dependencies
 * so the explorer test can compile and run without the full WM/Kernel.
 */

#include "dosgui_explorer.h"
#include "dosgui_wm.h"
#include <stdlib.h>
#include <string.h>

/* -- DosGui WM Stubs ---------------------------------------------- */

int dosgui_taskbar_height(void) { return 28; }

void dosgui_taskbar_render(uint32_t *fb, int fb_w, int fb_h) {}

/* -- Styx stubs for drive enumeration (minimal) --- */

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

/* -- DosGui WM Stubs for window management ------------------------ */

DosGuiWindow *dosgui_wm_find_by_id(int id) { return NULL; }

DosGuiWindow *dosgui_wm_create(int x, int y, int w, int h, const char *title) { return NULL; }

void dosgui_wm_destroy(DosGuiWindow *win) {}
