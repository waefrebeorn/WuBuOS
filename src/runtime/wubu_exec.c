/*
 * wubu_exec.c — WuBuOS Universal Executable Dispatcher Implementation
 *
 * One exec to rule them all.
 */

#include "wubu_exec.h"
#include "../compiler/holyc.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

/* ── VSL (Virtualization Substrate Layer) ───────────────────────── */

static bool g_vsl_initialized = false;

int wubu_vsl_init(void) {
    if (g_vsl_initialized) return 0;

    /*
     * VSL initialization:
     *   1. Set up lightweight VM structures (not full virtualization)
     *   2. Map Linux syscall interface to WuBuOS kernel calls
     *   3. Initialize driver passthrough (Vulkan, CUDA, networking)
     *   4. Set up shared memory region for WuBuOS ↔ VSL communication
     *
     * For now: stub. Real implementation needs:
     *   - KVM or equivalent lightweight VM
     *   - Linux kernel image (minimal)
     *   - VirtIO drivers for hardware passthrough
     */
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
    /* TODO: actual VSL execution */
    (void)cmd;
    return 0;
}

/* ── Format-Specific Executors ──────────────────────────────────── */

int64_t wubu_exec_linux_elf(const void *elf_data, size_t elf_size) {
    /* Wrap in .wubu container and dispatch */
    /* For now: detect and report */
    if (!elf_data || elf_size < 4) return -1;

    /* Validate ELF magic */
    const uint8_t *p = (const uint8_t *)elf_data;
    if (p[0] != 0x7F || p[1] != 'E' || p[2] != 'L' || p[3] != 'F')
        return -1;

    /* TODO: full ELF loading + VSL execution */
    return wubu_vsl_run("linux_elf_stub");
}

int64_t wubu_exec_win_pe(const void *pe_data, size_t pe_size) {
    (void)pe_data; (void)pe_size;
    /* TODO: PE loading + Proton translation */
    return wubu_vsl_run("proton_pe_stub");
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
