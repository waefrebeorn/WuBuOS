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

#include "hosted_internal.h"

#include "../runtime/wubu_proton.h"
#include "../runtime/wubu_ct_isolate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* PE executor registered with wubu_launch_windows (dependency inversion).
 * Writes the PE bytes to a temp file and launches via the GUI Proton manager
 * (real Windows-compat path through the SteamOS-style container). Returns a
 * process handle or -1. */
int hosted_pe_executor(const void *data, size_t size, const char *cmdline) {
    char tmppath[512];
    snprintf(tmppath, sizeof(tmppath), "/tmp/wubu-pe-%d.bin", (int)getpid());
    FILE *f = fopen(tmppath, "wb");
    if (!f) return -1;
    fwrite(data, 1, size, f);
    fclose(f);

    /* Isolate the foreign process in a cgroup v2 sandbox (Pressure-Vessel
     * analog). Best-effort: if cgroup setup fails (e.g. no privileges in
     * WSL), we still proceed -- the launch is the critical path. */
    char cg_path[256] = {0};
    wubu_ct_cgroup_create("wubu-foreign", cg_path, sizeof(cg_path));
    if (cg_path[0]) {
        wubu_ct_cgroup_set_memory(cg_path, 2048);   /* 2 GB cap */
        wubu_ct_cgroup_set_pids(cg_path, 4096);     /* process cap */
    }

    /* Launch via the Proton manager (prefix_id NULL -> default prefix). */
    char *argv[2] = { (char *)cmdline, NULL };
    int rc = wubu_proton_launch_with_prefix(tmppath, NULL, cmdline ? argv : NULL);
    unlink(tmppath);
    return rc;
}
