/* wubu_exec_container.c -- WuBuOS exec: container-payload execution.
 * Extracted from wubu_exec.c (separable leaf). Self-contained: decodes a
 * WUBU_HEADER container payload and dispatches to the appropriate backend
 * (wubu_exec_c/holyc/linux_elf/win_pe/python/shell/wasm/macho -- all public API).
 * C11, minimal includes.
 */
#include "wubu_exec.h"
#include "wubu_vsl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int64_t wubu_exec_container(const WUBU_HEADER *hdr,
                            const void *payload, size_t payload_size) {
    if (!hdr || (!payload && payload_size > 0)) return -1;

    /* Handler ID takes precedence over payload_type for dispatch.
     * This allows custom runtimes for standard payload types. */
    if (hdr->handler_id != 0) {
        switch (hdr->handler_id) {
            case 1:   /* WuBuOS native */
                return wubu_exec_native(payload, payload_size,
                                       hdr->entry_offset, hdr->arch);
            case 2:   /* HolyC JIT */
                return wubu_exec_holyc((const char *)payload, payload_size);
            case 10:  /* VSL (Linux) */
                return wubu_exec_linux_elf(payload, payload_size);
            case 11:  /* Proton (Windows) */
                return wubu_exec_win_pe(payload, payload_size);
            case 20:  /* Python 3 */
                return wubu_exec_python((const char *)payload, payload_size);
            case 21:  /* Node.js */
                return wubu_exec_shell((const char *)payload, payload_size); /* node script.js */
            case 30:  /* WASM runtime */
                return wubu_exec_wasm(payload, payload_size);
            case 0xFF: /* Custom handler by name */
                if (hdr->handler_name[0]) {
                    fprintf(stderr, "[wubu_exec] Custom handler '%s' not registered\n",
                            hdr->handler_name);
                    return WUBU_EXEC_ERR_FMT;
                }
                return WUBU_EXEC_ERR_FMT;
            default:
                /* Unknown handler_id - fall through to payload_type dispatch */
                break;
        }
    }

    /* Fallback: dispatch by payload_type */
    switch (hdr->payload_type) {
        case WUBU_PAYLOAD_NATIVE_EXEC:
            /* Direct execution -- mmap payload and call entry_offset */
            return wubu_exec_native(payload, payload_size,
                                   hdr->entry_offset, hdr->arch);

        case WUBU_PAYLOAD_HOLYC_SRC:
            return wubu_exec_holyc((const char *)payload, payload_size);

        case WUBU_PAYLOAD_C_SRC:
            return wubu_exec_c((const char *)payload, payload_size);

        case WUBU_PAYLOAD_LINUX_ELF:
            return wubu_exec_linux_elf(payload, payload_size);

        case WUBU_PAYLOAD_WIN_PE:
            return wubu_exec_win_pe(payload, payload_size);

        case WUBU_PAYLOAD_MAC_MACHO:
            return wubu_exec_macho(payload, payload_size);

        case WUBU_PAYLOAD_WASM:
            return wubu_exec_wasm(payload, payload_size);

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
