/*
 * dos_window.h -- desktop window that hosts a WuBuOS 16-bit DOS
 * process (see wubu_dos_proc.h). Renders the guest framebuffer and forwards
 * input. Self-contained; pairs with dos_window.c.
 */
#ifndef WUBU_DOSGUI_DOS_WINDOW_H
#define WUBU_DOSGUI_DOS_WINDOW_H

#include "wm.h"
#include "wubu_dos_proc.h"

/* Spawn a desktop window hosting `proc`. If mount_styx != 0 the process is
 * also mounted into the Styx /9P namespace at /tmp/wubu_styx. Returns the WM
 * window, or NULL on failure. */
DosGuiWindow *dos_window_spawn(WubuDosProc *proc, int mount_styx);

/* Close the window and free its context (the DOS proc keeps running unless
 * the caller kills it separately). */
void dos_window_close(DosGuiWindow *win);

#endif /* WUBU_DOSGUI_DOS_WINDOW_H */
