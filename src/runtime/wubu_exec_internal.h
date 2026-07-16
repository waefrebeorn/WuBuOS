/* wubu_exec_internal.h -- Internal helpers shared by wubu_exec sub-modules.
 * Public API + types in wubu_exec.h. The format registry/detection lives in
 * wubu_exec_format.c; the shared wubu_detect_payload_type resolver (defined in
 * wubu_container.c) is declared here so all submodules link the SAME
 * implementation (no double-coding / implicit-linkage-by-coincidence).
 */

#ifndef WUBU_EXEC_INTERNAL_H
#define WUBU_EXEC_INTERNAL_H

#include "wubu_exec.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* -- Shared payload-type resolver (defined in wubu_container.c) ---- */
WUBU_PAYLOAD_TYPE wubu_detect_payload_type(const void *data, size_t size);

/* -- DOS 16-bit backend (defined in wubu_exec_dos.c) ----------- */
WUBU_PAYLOAD_TYPE wubu_exec_dos_classify(const void *data, size_t size,
                                          const char *filename);
int64_t wubu_exec_dos(const void *data, size_t size, const char *filename);

#endif /* WUBU_EXEC_INTERNAL_H */
