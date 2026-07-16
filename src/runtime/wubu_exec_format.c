/* wubu_exec_format.c -- Payload format registry + detection (self-contained).
 *
 * wubu_payload_name / wubu_exec_result_str / wubu_detect_format. Uses
 * WUBU_PAYLOAD_TYPE / WUBU_EXEC_RESULT / WUBU_HEADER (wubu_exec.h) and the
 * shared wubu_detect_payload_type resolver (wubu_exec_internal.h).
 * Minimal includes.
 */

#include "wubu_exec_internal.h"

const char *wubu_payload_name(WUBU_PAYLOAD_TYPE type) {
    switch (type) {
        case WUBU_PAYLOAD_NATIVE_EXEC:   return "WuBuOS Native Executable";
        case WUBU_PAYLOAD_HOLYC_SRC:     return "HolyC Source";
        case WUBU_PAYLOAD_C_SRC:         return "C Source";
        case WUBU_PAYLOAD_LINUX_ELF:     return "Linux ELF (VSL)";
        case WUBU_PAYLOAD_WIN_PE:        return "Windows PE (Proton)";
        case WUBU_PAYLOAD_DOS_COM:     return "DOS 16-bit .COM (FreeDOS)";
        case WUBU_PAYLOAD_DOS_EXE:     return "DOS 16-bit .EXE (FreeDOS)";
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

    /* Not a .wubu  --  detect raw format */
    return wubu_detect_payload_type(data, size);
}
