/* wubu_manifest_internal.h -- opaque manifest internals (self-contained C11). */
#ifndef WUBU_MANIFEST_INTERNAL_H
#define WUBU_MANIFEST_INTERNAL_H
#include "wubu_manifest.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define WUBU_MANIFEST_MAX_SYSCALLS 512u
#define WUBU_MANIFEST_NAME_MAX     64u

typedef struct {
    uint64_t num;
    char     name[WUBU_MANIFEST_NAME_MAX];
    char     handler[WUBU_MANIFEST_NAME_MAX];
    char     cap[WUBU_MANIFEST_NAME_MAX];
    char     styx[WUBU_MANIFEST_NAME_MAX];
    char     holyc[WUBU_MANIFEST_NAME_MAX];
    uint64_t right;   /* resolved cap-right bitmask from cap string */
} wubu_syscall_entry_t;

struct wubu_manifest {
    wubu_syscall_entry_t entries[WUBU_MANIFEST_MAX_SYSCALLS];
    int                  count;
    /* right-name -> bitmask map (from "rights" object) */
    struct { char name[WUBU_MANIFEST_NAME_MAX]; uint64_t bit; } rights[32];
    int rights_count;
};

/* tiny JSON parser: enough for our manifest shape.
 * Returns 0 on success, -1 on syntax error. Fills `m`. */
int wubu_json_parse_manifest(const char *json, size_t len, wubu_manifest_t *m);

#endif /* WUBU_MANIFEST_INTERNAL_H */
