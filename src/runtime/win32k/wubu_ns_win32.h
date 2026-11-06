/*
 * wubu_ns_win32.h -- WuBuOS /n control plane: win32k window-manager surface.
 *
 * Publishes the live win32k-lite registry as /n/win32/* files, so the shell
 * can `cat /n/win32/registry` (stations/desktops/classes/windows + queue
 * depths) the same way it inspects any other /n subsystem. Reuses the
 * g_ns_root / ns_mkdir / ns_write primitives from wubu_ns_fs.c.
 */
#ifndef WUBU_NS_WIN32_H
#define WUBU_NS_WIN32_H

/* (Re)publish /n/win32/registry + /n/win32/stations + /n/win32/windows.
 * Reads the live win32k registry. Returns 0 on success, -1 on ns error,
 * 0 (no-op) when g_ns_root is unset. */
int wubu_ns_win32_sync(void);

#endif /* WUBU_NS_WIN32_H */
