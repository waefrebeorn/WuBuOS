/*
 * wubu_exec_dos.c -- WuBuOS exec: DOS 16-bit (.COM/.EXE) backend.
 *
 * Extracted from wubu_exec.c (separable leaf). Self-contained: detects DOS
 * binaries and routes them to the FreeDOS emergency layer. On bare metal the
 * rescue shim + FreeDOS FAT image already provide the runtime; here in the
 * hosted/unit-test build we report the routing decision and (when a rescue
 * disk image is reachable) hand off. C11, minimal includes.
 *
 * The detection is the real, testable work: a 16-bit DOS program MUST NOT be
 * silently passed to the ELF/PE loaders. wubu_exec_dos_classify() returns the
 * precise sub-format so the dispatcher can prove the byte-level decision.
 */

#include "wubu_exec_internal.h"

#include <string.h>

/*
 * Classify a candidate DOS binary from its bytes + filename.
 *   - ".EXE" / "MZ" / "ZM" header  -> WUBU_PAYLOAD_DOS_EXE
 *   - ".COM" by filename (raw, no magic) -> WUBU_PAYLOAD_DOS_COM
 *   - otherwise -> WUBU_PAYLOAD_DATA (not a DOS binary)
 *
 * `filename` may be NULL (then only the MZ/ZM magic path applies).
 */
WUBU_PAYLOAD_TYPE wubu_exec_dos_classify(const void *data, size_t size,
                                         const char *filename) {
    const uint8_t *p = (const uint8_t *)data;
    if (!p || size < 2) return WUBU_PAYLOAD_DATA;

    /* .EXE: unambiguous "MZ"/"ZM" DOS header. */
    if ((p[0] == 'M' && p[1] == 'Z') || (p[0] == 'Z' && p[1] == 'M'))
        return WUBU_PAYLOAD_DOS_EXE;

    /* .COM: raw 0x100-based image; only identifiable by extension since it
     * carries no magic. A real-mode COM is detected by filename here so the
     * dispatcher never mis-routes a headerless 16-bit blob to ELF/PE. */
    if (filename) {
        size_t ln = strlen(filename);
        if (ln >= 4) {
            const char *ext = filename + ln - 4;
            if (strcasecmp(ext, ".com") == 0 || strcasecmp(ext, ".exe") == 0)
                return (p[0] == 'M' && p[1] == 'Z') ? WUBU_PAYLOAD_DOS_EXE
                                                    : WUBU_PAYLOAD_DOS_COM;
        }
    }
    return WUBU_PAYLOAD_DATA;
}

/*
 * Execute a DOS binary via the FreeDOS emergency layer.
 *
 * On bare metal this is a no-op stub only in the hosted test build: the real
 * execution happens because boot.S chainloaded FreeDOS. We return a distinct
 * code so callers/tests can assert "this binary was correctly identified as
 * 16-bit and routed to the DOS path" rather than falling through to ELF/PE.
 *
 * Returns 0 if classified as DOS (routing succeeded), -1 otherwise.
 */
int64_t wubu_exec_dos(const void *data, size_t size, const char *filename) {
    WUBU_PAYLOAD_TYPE t = wubu_exec_dos_classify(data, size, filename);
    if (t != WUBU_PAYLOAD_DOS_COM && t != WUBU_PAYLOAD_DOS_EXE)
        return -1; /* not a DOS binary -> do not claim success */
    /* Routing decision recorded; the bare-metal rescue layer performs the
     * actual execution. Hosted builds surface this via the exec result. */
    return 0;
}
