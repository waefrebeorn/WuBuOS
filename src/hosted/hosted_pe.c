/*
 * hosted_pe.c -- WuBuOS hosted-mode Windows/PE launch executor
 *
 * Self-contained concern split out of hosted.c: the PE executor registered
 * with the launch layer. Following the SteamOS strategy (Windows runs in a
 * container via Proton, never an NT-kernel reimpl), it writes the PE bytes to
 * a temp file, sandboxes the foreign process in a cgroup v2 (Pressure-Vessel
 * analog, best-effort), and launches through the GUI Proton manager.
 *
 * Depends only on the container-isolation + Proton public APIs and the launcher
 * shared state (hosted_internal.h). No Wayland, no render, no run-loop.
 */

#include "../egojar.h"
#include "hosted_internal.h"

#include "../runtime/wubu_ct_isolate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../gui/wubu_proton.h"   /* declares wubu_proton_launch_with_prefix */

/* PE executor registered with wubu_launch_windows (dependency inversion).
 * Writes the PE bytes to a temp file and launches via the GUI Proton manager
 * (real Windows-compat path through the SteamOS-style container). The PE
 * path is argv[0] of the wine command; the launch runs in the resolved-or-
 * created "default" wine prefix and WAITS for the program to exit, so the
 * staged temp file is only unlinked after wine is done with it. Returns the
 * program's exit status (0 = ran), or -1 on a setup/launch failure. */
int hosted_pe_executor(const void *data, size_t size, const char *cmdline) {
    if (!data || size == 0) return -1;
    char tmppath[512];
    snprintf(tmppath, sizeof(tmppath), "/tmp/wubu-pe-%d.bin", (int)getpid());
    FILE *f = fopen(tmppath, "wb");
    if (!f) return -1;
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    if (written != size) { unlink(tmppath); return -1; }

    /* Isolate the foreign process in a cgroup v2 sandbox (Pressure-Vessel
     * analog). Best-effort: if cgroup setup fails (e.g. no privileges in
     * WSL), we still proceed -- the launch is the critical path. The wine
     * child joins this cgroup via WUBU_PE_CGROUP (run_wine_command). */
    char cg_path[256] = {0};
    wubu_ct_cgroup_create("wubu-foreign", cg_path, sizeof(cg_path));
    if (cg_path[0]) {
        wubu_ct_cgroup_set_memory(cg_path, 2048);   /* 2 GB cap */
        wubu_ct_cgroup_set_pids(cg_path, 4096);     /* process cap */
        setenv("WUBU_PE_CGROUP", cg_path, 1);
    }

    /* Launch via the Proton manager: tmppath is argv[0] (the program wine
     * runs), cmdline is an optional extra argument. "default" prefix is
     * resolved-or-created by the proton layer (wineboot -u machinery). */
    char *args[2] = { (char *)cmdline, NULL };
    int rc = wubu_proton_launch_with_prefix(tmppath, "default", cmdline ? args : NULL);
    if (cg_path[0]) unsetenv("WUBU_PE_CGROUP");
    unlink(tmppath);
    return rc;
}
