/* wubu_pkgmgr_manifest.c -- WuBuOS pkgmgr: manifest -> JSON serialization.
 * Extracted from wubu_pkgmgr.c (separable leaf). Self-contained: stdlib only.
 * C11, minimal includes.
 */
#include "wubu_pkgmgr.h"
#include "wubu_pkgmgr_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *manifest_to_json(const wubu_pkg_manifest_t *m) {
    if (!m) return NULL;
    size_t cap = 4096;
    char *json = malloc(cap);
    if (!json) return NULL;
    int pos = 0;

    /* Emit into a growable buffer; if snprintf reports truncation, grow and retry. */
#define EMIT(...) do { \
        int need = snprintf(json + pos, (pos < (int)cap ? cap - (size_t)pos : 1), __VA_ARGS__); \
        if ((size_t)need >= (pos < (int)cap ? cap - (size_t)pos : 1)) { \
            size_t ncap = cap; \
            while ((size_t)need >= ncap - (size_t)pos) ncap *= 2; \
            char *nj = realloc(json, ncap); \
            if (!nj) { free(json); return NULL; } \
            json = nj; cap = ncap; \
            need = snprintf(json + pos, cap - (size_t)pos, __VA_ARGS__); \
        } \
        pos += need; \
    } while (0)

    EMIT("{\"id\":\"%s\",", m->id);
    EMIT("\"name\":\"%s\",", m->name);
    EMIT("\"version\":\"%s\",", m->version);
    EMIT("\"arch\":%d,", (int)m->arch);
    EMIT("\"description\":\"%s\",", m->description);
    EMIT("\"deps\":[");
    for (int i = 0; i < m->n_depends; i++) {
        EMIT("%s\"%s\"", i > 0 ? "," : "", m->depends[i]);
    }
    EMIT("],");
    EMIT("\"files\":[");
    for (int i = 0; i < m->n_files; i++) {
        EMIT("%s\"%s|%s|%o\"", i > 0 ? "," : "", m->files[i].src, m->files[i].dst, m->files[i].mode);
    }
    EMIT("],");
    EMIT("\"entries\":[");
    for (int i = 0; i < m->n_entrypoints; i++) {
        EMIT("%s{\"name\":\"%s\",\"exec\":\"%s\",\"icon\":\"%s\",\"categories\":\"%s\"}",
            i > 0 ? "," : "", m->entrypoints[i].name, m->entrypoints[i].exec,
            m->entrypoints[i].icon, m->entrypoints[i].categories);
    }
    EMIT("]}");
#undef EMIT
    return json;
}
