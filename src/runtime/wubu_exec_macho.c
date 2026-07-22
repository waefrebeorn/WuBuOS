/* wubu_exec_macho.c -- WuBuOS exec: Mach-O backend (VSL native + Darling fallback).
 * Validates Mach-O magic, loads via VSL Mach-O loader if available,
 * or shells out to Darling as fallback. C11, minimal includes.
 *
 * Uses a callback mechanism to avoid link-time dependency on VSL:
 *   wubu_exec_register_macho_loader() is called by VSL init to
 *   register the Mach-O process creator. If unregistered, falls
 *   back to Darling.
 */
#include "wubu_exec.h"
#include "wubu_vsl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>

/* Mach-O loader callback — registered by VSL init */
static int (*g_macho_loader)(const void*, size_t) = NULL;

void wubu_exec_register_macho_loader(int (*loader)(const void*, size_t)) {
    g_macho_loader = loader;
}

int64_t wubu_exec_macho(const void *macho_data, size_t macho_size) {
    if (!macho_data || macho_size < 4) return WUBU_EXEC_ERR_FMT;

    /* Validate Mach-O magic */
    const uint8_t *p = (const uint8_t *)macho_data;
    uint32_t magic;
    memcpy(&magic, p, 4);

    bool is_macho = (magic == 0xFEEDFACE || magic == 0xFEEDFACF ||
                     magic == 0xCEFAEDFE || magic == 0xCFFAEDFE);

    if (!is_macho) return WUBU_EXEC_ERR_FMT;

    /* Try VSL native Mach-O loader if registered */
    if (g_macho_loader && wubu_vsl_active()) {
        int pid = g_macho_loader(macho_data, macho_size);
        if (pid >= 0) {
            fprintf(stderr, "[wubu_exec] Loaded Mach-O via VSL (PID %d)\n", pid);
            return (int64_t)pid;
        }
        if (pid == -2) {
            fprintf(stderr, "[wubu_exec] 32-bit Mach-O not supported\n");
            return WUBU_EXEC_ERR_FMT;
        }
        /* Other error: fall through to Darling */
    }

    /* Fallback: Write Mach-O to temp file and try Darling */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/wubu-macho-%d", getpid());
    FILE *f = fopen(tmp_path, "wb");
    if (!f) return -1;
    fwrite(macho_data, 1, macho_size, f);
    fclose(f);
    chmod(tmp_path, 0755);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "darling %s 2>/dev/null", tmp_path);
    int64_t result = wubu_vsl_run(cmd);

    if (result == -127 || result == 127) {
        fprintf(stderr, "[wubu_exec] Darling not available, Mach-O execution requires VSL init\n");
        result = WUBU_EXEC_ERR_FMT;
    }

    unlink(tmp_path);
    return result;
}

/*
 * wubu_exec_launch_macho -- run a Mach-O as a first-class WubuCt
 * process through the VSL exec/container layer (agnostic HW operator).
 *
 * Validates the Mach-O magic, writes it to a temp file, then creates a
 * CT_MACHO container via wubu_ct_macho() and starts it. The container
 * pipeline (fork + chroot + exec + cgroup/seccomp isolation) launches
 * `darling <macho>` as argv[0]; Darling is the Mach-O runtime, VSL
 * serves the GPU capability underneath (Vulkan via lavapipe/D3D12, Metal
 * transposed to SPIR-V by the metal2vulkan shim). On success returns
 * the host PID of the running container (same contract as the ELF/PE paths).
 *
 * This is the "all execution container types through our process. VSL"
 * wiring for Darwin binaries -- no special-casing, just another WubuCt.
 */
#include "wubu_host_exec.h"

int64_t wubu_exec_launch_macho(const void *macho_data, size_t macho_size,
                                const char *root) {
    if (!macho_data || macho_size < 4) return WUBU_EXEC_ERR_FMT;

    /* Validate Mach-O magic */
    const uint8_t *p = (const uint8_t *)macho_data;
    uint32_t magic;
    memcpy(&magic, p, 4);
    bool is_macho = (magic == 0xFEEDFACE || magic == 0xFEEDFACF ||
                     magic == 0xCEFAEDFE || magic == 0xCFFAEDFE);
    if (!is_macho) return WUBU_EXEC_ERR_FMT;

    /* Write Mach-O to a temp file the container can exec. */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/wubu-macho-%d.macho", getpid());
    FILE *f = fopen(tmp_path, "wb");
    if (!f) return -1;
    fwrite(macho_data, 1, macho_size, f);
    fclose(f);
    chmod(tmp_path, 0755);

    /* Build a Mach-O WubuCt and launch it through the container pipeline. */
    WubuCt *ct = wubu_ct_macho("macho", root ? root : "/var/wubu/roots/arch-base");
    if (!ct) { unlink(tmp_path); return -1; }

    char *argv[3];
    argv[0] = "darling";
    argv[1] = tmp_path;
    argv[2] = NULL;
    if (wubu_ct_set_cmd(ct, 2, argv) != 0) {
        wubu_ct_destroy(ct);
        unlink(tmp_path);
        return -1;
    }

    if (wubu_ct_start(ct) != 0) {
        wubu_ct_destroy(ct);
        unlink(tmp_path);
        return -1;
    }

    int64_t pid = (int64_t)ct->pid;
    fprintf(stderr, "[wubu_exec] Mach-O launched as WubuCt (runtime=%s, PID %lld)\n",
            wubu_ct_runtime_name(ct->runtime), (long long)pid);
    wubu_ct_destroy(ct);   /* config freed; the container process lives on */
    return pid;
}
