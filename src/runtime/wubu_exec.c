/*
 * wubu_exec.c — WuBuOS Universal Executable Dispatcher Implementation
 *
 * One exec to rule them all.
 */

#include "wubu_exec.h"
#include "wubu_host_exec.h"
#include "../compiler/holyc.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

/* ── Format Names ───────────────────────────────────────────────── */

const char *wubu_payload_name(WUBU_PAYLOAD_TYPE type) {
    switch (type) {
        case WUBU_PAYLOAD_NATIVE_EXEC:   return "WuBuOS Native Executable";
        case WUBU_PAYLOAD_HOLYC_SRC:     return "HolyC Source";
        case WUBU_PAYLOAD_C_SRC:         return "C Source";
        case WUBU_PAYLOAD_LINUX_ELF:     return "Linux ELF (VSL)";
        case WUBU_PAYLOAD_WIN_PE:        return "Windows PE (Proton)";
        case WUBU_PAYLOAD_MAC_MACHO:     return "macOS Mach-O (VSL)";
        case WUBU_PAYLOAD_SHELL_SCRIPT:  return "Shell Script (VSL)";
        case WUBU_PAYLOAD_PYTHON:        return "Python Script (VSL)";
        case WUBU_PAYLOAD_WASM:          return "WebAssembly";
        case WUBU_PAYLOAD_BYTECODE:      return "Bytecode";
        case WUBU_PAYLOAD_DATA:          return "Data";
        case WUBU_PAYLOAD_NESTED_WUBU:   return "Nested .wubu Container";
        default:                         return "Unknown";
    }
}

const char *wubu_exec_result_str(WUBU_EXEC_RESULT result) {
    switch (result) {
        case WUBU_EXEC_OK:       return "success";
        case WUBU_EXEC_ERR_FMT:  return "unsupported format";
        case WUBU_EXEC_ERR_LOAD: return "load failed";
        case WUBU_EXEC_ERR_HDR:  return "invalid header";
        case WUBU_EXEC_ERR_MEM:  return "out of memory";
        case WUBU_EXEC_ERR_VSL:  return "VSL error";
        case WUBU_EXEC_ERR_JIT:  return "JIT error";
        case WUBU_EXEC_ERR_PERM: return "permission denied";
        default:                 return "unknown error";
    }
}

/* ── Format Detection ───────────────────────────────────────────── */

WUBU_PAYLOAD_TYPE wubu_detect_format(const void *data, size_t size,
                                     bool *is_wubu) {
    if (is_wubu) *is_wubu = false;

    if (!data || size < 2) return WUBU_PAYLOAD_DATA;

    /* Check for .wubu magic first */
    if (size >= WUBU_MAGIC_SIZE && memcmp(data, WUBU_MAGIC, WUBU_MAGIC_SIZE) == 0) {
        if (is_wubu) *is_wubu = true;

        /* Parse the container to get the real payload type */
        WUBU_HEADER hdr;
        if (wubu_container_parse(data, size, &hdr, NULL, NULL) == 0)
            return (WUBU_PAYLOAD_TYPE)hdr.payload_type;

        return WUBU_PAYLOAD_NESTED_WUBU;
    }

    /* Not a .wubu — detect raw format */
    return wubu_detect_payload_type(data, size);
}

/* ── VSL (Virtualization Substrate Layer) ─────────────────────────
 *
 * VSL is now HOST DELEGATION - fork+exec on the host Linux kernel.
 * This replaces the old in-process syscall translation layer.
 * Architecture: "VSL is NOT a Linux syscall emulation layer —
 * rename to wubu_host_linux.c (platform delegation to host libc)."
 */

static bool g_vsl_initialized = false;

int wubu_vsl_init(void) {
    if (g_vsl_initialized) return 0;

    /* 1. Verify host environment has required capabilities */
    /* Check for basic fork/exec support */
    pid_t test_pid = fork();
    if (test_pid < 0) {
        return -1;  /* Host doesn't support fork */
    }
    if (test_pid == 0) {
        _exit(0);  /* Child exits immediately */
    }
    int status;
    waitpid(test_pid, &status, 0);

    /* 2. Verify we can exec basic binaries */
    if (access("/bin/sh", X_OK) != 0 && access("/usr/bin/sh", X_OK) != 0) {
        return -1;  /* No shell available */
    }

    /* 3. Set up shared memory region for WuBuOS ↔ host communication
     * (Currently a placeholder - would use memfd_create or similar) */
    /* TODO: Implement proper shared memory for container communication */

    g_vsl_initialized = true;
    return 0;
}

void wubu_vsl_shutdown(void) {
    g_vsl_initialized = false;
}

bool wubu_vsl_active(void) {
    return g_vsl_initialized;
}

int64_t wubu_vsl_run(const char *cmd) {
    if (!g_vsl_initialized) {
        if (wubu_vsl_init() != 0) return -1;
    }

    if (!cmd || !*cmd) return -1;

    /* Fork and execute the command on the host */
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        /* Child: execute via shell */
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        /* If /bin/sh fails, try /usr/bin/sh */
        execl("/usr/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }

    /* Parent: wait for completion */
    int status = 0;
    pid_t ret = waitpid(pid, &status, 0);
    if (ret != pid) {
        return -1;
    }

    if (WIFEXITED(status)) {
        return (int64_t)WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        return (int64_t)(-WTERMSIG(status));
    }

    return -1;
}

/* ── Format-Specific Executors ──────────────────────────────────── */

int64_t wubu_exec_linux_elf(const void *elf_data, size_t elf_size) {
    if (!elf_data || elf_size < 4) return -1;

    /* Validate ELF magic */
    const uint8_t *p = (const uint8_t *)elf_data;
    if (p[0] != 0x7F || p[1] != 'E' || p[2] != 'L' || p[3] != 'F')
        return -1;

    /* Write ELF to temp file for container execution */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/wubu-elf-%d", getpid());
    FILE *f = fopen(tmp_path, "wb");
    if (!f) return -1;
    fwrite(elf_data, 1, elf_size, f);
    fclose(f);
    chmod(tmp_path, 0755);

    /* Create native Linux container and exec the ELF */
    char *root = getenv("WUBU_ARCH_ROOT");
    if (!root) root = "/";  /* fallback to host root */

    WubuCt *ct = wubu_ct_native("linux-elf", root);
    if (!ct) {
        unlink(tmp_path);
        return -1;
    }

    /* Set up command to run the ELF */
    char *argv[] = {tmp_path, NULL};
    wubu_ct_set_cmd(ct, 1, argv);
    ct->net_enabled = true;

    int rc = wubu_ct_start(ct);
    if (rc != 0) {
        wubu_ct_destroy(ct);
        unlink(tmp_path);
        return -1;
    }

    /* Wait for completion */
    int exit_code = wubu_ct_wait(ct);
    wubu_ct_destroy(ct);
    unlink(tmp_path);

    return exit_code;
}

int64_t wubu_exec_win_pe(const void *pe_data, size_t pe_size) {
    if (!pe_data || pe_size < 2) return -1;

    /* Validate PE magic (MZ) */
    const uint8_t *p = (const uint8_t *)pe_data;
    if (p[0] != 'M' || p[1] != 'Z')
        return -1;

    /* Write PE to temp file for container execution */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/wubu-pe-%d.exe", getpid());
    FILE *f = fopen(tmp_path, "wb");
    if (!f) return -1;
    fwrite(pe_data, 1, pe_size, f);
    fclose(f);

    /* Create Proton/SteamOS container and exec via Wine */
    char *root = getenv("WUBU_ARCH_ROOT");
    if (!root) root = "/";

    WubuCt *ct = wubu_ct_steamos("win-pe", root);
    if (!ct) {
        unlink(tmp_path);
        return -1;
    }

    /* Set up command: wine /path/to/exe */
    char *argv[] = {"/usr/bin/wine", tmp_path, NULL};
    wubu_ct_set_cmd(ct, 2, argv);
    ct->net_enabled = true;
    ct->gpu_passthrough = true;  /* Proton needs GPU */

    int rc = wubu_ct_start(ct);
    if (rc != 0) {
        wubu_ct_destroy(ct);
        unlink(tmp_path);
        return -1;
    }

    /* Wait for completion */
    int exit_code = wubu_ct_wait(ct);
    wubu_ct_destroy(ct);
    unlink(tmp_path);

    return exit_code;
}

int64_t wubu_exec_holyc(const char *source, size_t source_size) {
    if (!source) return -1;
    /* Use our HolyC compiler to JIT compile and execute */
    return hc_eval(source);
}

int64_t wubu_exec_c(const char *source, size_t source_size) {
    (void)source_size;
    /* TODO: compile C via HolyC compiler, then execute */
    /* For now: treat as HolyC (subset) */
    return hc_eval(source);
}

int64_t wubu_exec_shell(const char *script, size_t script_size) {
    (void)script_size;
    /* Run via VSL bash */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "bash -c '%s'", script);
    return wubu_vsl_run(cmd);
}

int64_t wubu_exec_python(const char *script, size_t script_size) {
    (void)script_size;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "python3 -c '%s'", script);
    return wubu_vsl_run(cmd);
}

/* ── Container Exec ─────────────────────────────────────────────── */

int64_t wubu_exec_container(const WUBU_HEADER *hdr,
                            const void *payload, size_t payload_size) {
    if (!hdr || (!payload && payload_size > 0)) return -1;

    switch (hdr->payload_type) {
        case WUBU_PAYLOAD_NATIVE_EXEC:
            /* Direct execution — entry point at offset */
            /* TODO: map and call */
            return 0;

        case WUBU_PAYLOAD_HOLYC_SRC:
            return wubu_exec_holyc((const char *)payload, payload_size);

        case WUBU_PAYLOAD_C_SRC:
            return wubu_exec_c((const char *)payload, payload_size);

        case WUBU_PAYLOAD_LINUX_ELF:
            return wubu_exec_linux_elf(payload, payload_size);

        case WUBU_PAYLOAD_WIN_PE:
            return wubu_exec_win_pe(payload, payload_size);

        case WUBU_PAYLOAD_SHELL_SCRIPT:
            return wubu_exec_shell((const char *)payload, payload_size);

        case WUBU_PAYLOAD_PYTHON:
            return wubu_exec_python((const char *)payload, payload_size);

        case WUBU_PAYLOAD_NESTED_WUBU:
            /* Recursively parse and execute */
            return wubu_exec(payload, payload_size, "nested.wubu");

        case WUBU_PAYLOAD_DATA:
            /* Not executable */
            return 0;

        default:
            return -1;
    }
}

/* ── Universal Exec ─────────────────────────────────────────────── */

int64_t wubu_exec(const void *data, size_t size, const char *filename) {
    if (!data || size == 0) return -1;

    bool is_wubu = false;
    WUBU_PAYLOAD_TYPE type = wubu_detect_format(data, size, &is_wubu);

    if (filename) {
        printf("[wubu_exec] %s: %s\n", filename, wubu_payload_name(type));
    }

    if (is_wubu) {
        WUBU_HEADER hdr;
        const void *payload;
        size_t payload_size;

        if (wubu_container_parse(data, size, &hdr, &payload, &payload_size) != 0)
            return -1;

        return wubu_exec_container(&hdr, payload, payload_size);
    }

    /* Raw file — wrap in implicit .wubu and dispatch */
    switch (type) {
        case WUBU_PAYLOAD_LINUX_ELF:
            return wubu_exec_linux_elf(data, size);
        case WUBU_PAYLOAD_WIN_PE:
            return wubu_exec_win_pe(data, size);
        case WUBU_PAYLOAD_HOLYC_SRC:
            return wubu_exec_holyc((const char *)data, size);
        case WUBU_PAYLOAD_C_SRC:
            return wubu_exec_c((const char *)data, size);
        case WUBU_PAYLOAD_SHELL_SCRIPT:
            return wubu_exec_shell((const char *)data, size);
        case WUBU_PAYLOAD_PYTHON:
            return wubu_exec_python((const char *)data, size);
        default:
            return -1;
    }
}
