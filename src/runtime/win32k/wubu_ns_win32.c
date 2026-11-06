/*
 * wubu_ns_win32.c -- WuBuOS /n control plane: win32k window-manager surface.
 *
 *   /n/win32/registry -- full live registry text (station/desktop/class/
 *                        window lines + queue depths), from the LIVE
 *                        win32k registry (wubu_win32k_snapshot).
 *   /n/win32/stations -- station lines only.
 *   /n/win32/windows  -- window lines only (title, class, queue depth).
 *
 * The live window-manager store is the wubu_cap authority-backed
 * wubu_win32k module; this module is a thin reader that mirrors it into the
 * namespace. It links only wubu_ns_fs.o + wubu_win32k (which pulls the cap
 * core) — no archd/bottles. Safe no-op when g_ns_root is unset.
 */
#include "wubu_ns_bridge.h"
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_win32.h"
#include "wubu_win32k.h"

#include <stdio.h>
#include <string.h>

/* Append one snapshot line (matching a prefix) into an out buffer, bounded. */
static void collect(const char *line, const char *prefix,
                    char *out, size_t outcap) {
    if (strncmp(line, prefix, strlen(prefix)) != 0) return;
    size_t have = strlen(out);
    size_t need = (size_t)snprintf(NULL, 0, "%s\n", line) + 1;
    if (have + need < outcap) {
        snprintf(out + have, outcap - have, "%s\n", line);
    }
}

int wubu_ns_win32_sync(void) {
    if (!g_ns_root) return 0;               /* bridge not up: no-op */
    if (ns_mkdir("win32") != 0) return -1;

    char snap[8192];
    size_t n = wubu_win32k_snapshot(snap, sizeof(snap));
    (void)n;
    if (ns_write("win32/registry", snap) != 0) return -1;

    char stations[4096] = "";
    char windows[4096]  = "";
    char gdi[4096]      = "";
    const char *p = snap;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t l = eol ? (size_t)(eol - p) : strlen(p);
        char line[200];
        if (l >= sizeof(line)) l = sizeof(line) - 1;
        memcpy(line, p, l);
        line[l] = '\0';
        collect(line, "station ", stations, sizeof(stations));
        collect(line, "window ",  windows,  sizeof(windows));
        collect(line, "gdi ",     gdi,      sizeof(gdi));
        if (!eol) break;
        p = eol + 1;
    }

    if (ns_write("win32/stations", stations) != 0) return -1;
    if (ns_write("win32/windows",  windows)  != 0) return -1;
    if (ns_write("win32/gdi",      gdi)      != 0) return -1;
    return 0;
}
