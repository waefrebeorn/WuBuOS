/*
 * wubu_exec_dos.c -- WuBuOS exec: DOS 16-bit (.COM/.EXE) backend.
 *
 * Extracted from wubu_exec.c (separable leaf). Self-contained: detects DOS
 * binaries and routes them to the in-process 16-bit compat shim (no second
 * OS is booted). The detection is the real, testable work: a 16-bit DOS
 * program MUST NOT be silently passed to the ELF/PE loaders.
 *
 * wubu_exec_dos() loads the program bytes into wubu_dos_proc, which runs
 * them with the real 8086 interpreter and surfaces the result in the
 * desktop "compatible window" + Styx /proc/<pid>/dos. C11, minimal includes.
 */

#include "wubu_exec_internal.h"
#include "wubu_dos_proc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Classify a candidate DOS binary from its bytes + filename.
 *   - ".EXE" / "MZ" / "ZM" header  -> WUBU_PAYLOAD_DOS_EXE
 *   - ".COM" by filename (raw, no magic) -> WUBU_PAYLOAD_DOS_COM
 *   - otherwise -> WUBU_PAYLOAD_DATA (not a DOS binary)
 */
WUBU_PAYLOAD_TYPE wubu_exec_dos_classify(const void *data, size_t size,
                                         const char *filename) {
    const uint8_t *p = (const uint8_t *)data;
    if (!p || size < 2) return WUBU_PAYLOAD_DATA;

    if ((p[0] == 'M' && p[1] == 'Z') || (p[0] == 'Z' && p[1] == 'M'))
        return WUBU_PAYLOAD_DOS_EXE;

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
 * Execute a DOS binary via the in-process 16-bit compat shim.
 *
 * Materializes the binary to a temp file (the shim's reader takes a path),
 * launches it as a WuBuOS process, runs it to completion with the real 8086
 * interpreter, and returns a live WubuDosProc the caller can render
 * (frame_capture), drive (send_key/write_ctl) and kill. Returns NULL on a
 * bad/unsupported image or a load failure.
 */
WubuDosProc *wubu_exec_dos(const void *data, size_t size, const char *filename) {
    WUBU_PAYLOAD_TYPE t = wubu_exec_dos_classify(data, size, filename);
    if (t != WUBU_PAYLOAD_DOS_COM && t != WUBU_PAYLOAD_DOS_EXE)
        return NULL; /* not a DOS binary -> do not claim success */

    char tmppath[512];
    snprintf(tmppath, sizeof(tmppath), "/tmp/wubu_dos_bin_%d.com", (int)getpid());
    FILE *f = fopen(tmppath, "wb");
    if (!f) return NULL;
    if (fwrite(data, 1, size, f) != size) { fclose(f); unlink(tmppath); return NULL; }
    fclose(f);

    /* Launch into the shim (no QEMU, no disk image). fmt selects COM vs EXE. */
    WubuDosProc *p = wubu_dos_proc_launch(tmppath, t);
    unlink(tmppath);
    if (!p || wubu_dos_proc_state(p) == DOS_PROC_FAILED) {
        if (p) wubu_dos_proc_destroy(p);
        return NULL;
    }
    return p;
}
