/* wubu_exec_wasm.c -- WuBuOS exec: WASM backend (wasmtime/wasm3/wasmer).
 * Extracted from wubu_exec.c (separable leaf). Self-contained: validates WASM
 * magic, shells out to a WASM runtime via wubu_vsl_run. C11, minimal includes.
 */
#include "wubu_exec.h"
#include "wubu_vsl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int64_t wubu_exec_wasm(const void *wasm_data, size_t wasm_size) {
    if (!wasm_data || wasm_size < 8) return -1;

    /* Validate WASM magic: 0x00 'a' 's' 'm' */
    const uint8_t *p = (const uint8_t *)wasm_data;
    if (p[0] != 0x00 || p[1] != 'a' || p[2] != 's' || p[3] != 'm')
        return -1;

    /* Write WASM to temp file for wasmtime execution */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/wubu-wasm-%d.wasm", getpid());
    FILE *f = fopen(tmp_path, "wb");
    if (!f) return -1;
    fwrite(wasm_data, 1, wasm_size, f);
    fclose(f);

    /* Try wasmtime first (industry standard) */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "wasmtime %s 2>/dev/null", tmp_path);
    int64_t result = wubu_vsl_run(cmd);

    /* If wasmtime not available, try wasm3 */
    if (result == -127 || result == 127) {
        snprintf(cmd, sizeof(cmd), "wasm3 %s 2>/dev/null", tmp_path);
        result = wubu_vsl_run(cmd);
    }

    /* If wasm3 not available, try wasmer */
    if (result == -127 || result == 127) {
        snprintf(cmd, sizeof(cmd), "wasmer run %s 2>/dev/null", tmp_path);
        result = wubu_vsl_run(cmd);
    }

    unlink(tmp_path);

    /* Return meaningful error if no runtime found */
    if (result == -127 || result == 127) {
        fprintf(stderr, "[wubu_exec] No WASM runtime found (tried: wasmtime, wasm3, wasmer)\n");
        return WUBU_EXEC_ERR_VSL;
    }

    return result;
}
