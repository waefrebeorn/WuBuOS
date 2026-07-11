/* wubu_exec_macho.c -- WuBuOS exec: Mach-O backend (Darling dispatch).
 * Extracted from wubu_exec.c (separable leaf). Self-contained: validates Mach-O
 * magic, shells out to Darling via wubu_vsl_run. C11, minimal includes.
 */
#include "wubu_exec.h"
#include "wubu_vsl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int64_t wubu_exec_macho(const void *macho_data, size_t macho_size) {
    if (!macho_data || macho_size < 4) return -1;

    /* Validate Mach-O magic */
    const uint8_t *p = (const uint8_t *)macho_data;
    uint32_t magic;
    memcpy(&magic, p, 4);

    /* Mach-O magic numbers: 32-bit (FEEDFACE/FEEDFACF) and 64-bit (FEEDFACF/CFFAEDFE) */
    bool is_macho = (magic == 0xFEEDFACE || magic == 0xFEEDFACF ||
                     magic == 0xCEFAEDFE || magic == 0xCFFAEDFE);

    if (!is_macho) return -1;

    /* Write Mach-O to temp file */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/wubu-macho-%d", getpid());
    FILE *f = fopen(tmp_path, "wb");
    if (!f) return -1;
    fwrite(macho_data, 1, macho_size, f);
    fclose(f);
    chmod(tmp_path, 0755);

    /* Try Darling (macOS binary compatibility on Linux) */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "darling %s 2>/dev/null", tmp_path);
    int64_t result = wubu_vsl_run(cmd);

    /* If Darling not available, try VSL Linux with macOS emulation hint */
    if (result == -127 || result == 127) {
        /* Fallback: run in VSL with Rosetta-2-like hints (stub) */
        fprintf(stderr, "[wubu_exec] Darling not available, Mach-O execution stubbed\n");
        result = WUBU_EXEC_ERR_FMT;
    }

    unlink(tmp_path);
    return result;
}
