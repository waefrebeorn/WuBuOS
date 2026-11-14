/*
 * wubu_spell_scan.c -- directory discovery for .spell modules.
 *
 * WHY A SEPARATE FILE
 *   wubu_spell_resolve.c is pure graph work: no filesystem, no allocation of module
 *   bytes, nothing that could be linked into a bare environment.  Discovery touches
 *   the filesystem and reads files, which is a different concern with a different set
 *   of failure modes (unreadable directories, huge files, unrelated entries).  Keeping
 *   it apart means the resolver stays testable without a disk and can be reused by
 *   callers that get their modules some other way -- an install step, a package
 *   manager, an in-memory build.
 */
#include "wubu_spell_resolve.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int wubu_spell_default_dir(char *out, size_t outsz)
{
    if (!out || outsz == 0) return -1;

    const char *env = getenv("WUBU_SPELL_DIR");
    if (env && *env) {
        snprintf(out, outsz, "%s", env);
        return 0;
    }

    const char *home = getenv("HOME");
    if (!home || !*home) { out[0] = '\0'; return -1; }

    snprintf(out, outsz, "%s/.local/share/wubu/spells", home);
    return 0;
}

static bool has_spell_suffix(const char *name)
{
    size_t n = strlen(name), s = strlen(".spell");
    return n > s && strcmp(name + n - s, ".spell") == 0;
}

/* Read at most WUBU_SPELL_FILE_MAX bytes.  Returns length, or -1 on any failure.
 * The caller owns the returned buffer. */
static long read_bounded(const char *path, uint8_t **out)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return -1; }
    long sz = ftell(fp);
    if (sz < 0) { fclose(fp); return -1; }
    /* Refuse up front rather than discovering the size only after allocating. */
    if ((unsigned long)sz > WUBU_SPELL_FILE_MAX) { fclose(fp); return -1; }
    rewind(fp);

    uint8_t *buf = malloc((size_t)sz ? (size_t)sz : 1);
    if (!buf) { fclose(fp); return -1; }

    size_t got = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (got != (size_t)sz) { free(buf); return -1; }

    *out = buf;
    return sz;
}

int wubu_spell_scan(wubu_spell_resolve *r, const char *dir)
{
    if (!r || !dir || !*dir) return -1;

    DIR *d = opendir(dir);
    if (!d) return -1;                      /* missing dir is reported, not fatal */

    int registered = 0;

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;              /* skip . and .. */
        if (!has_spell_suffix(e->d_name)) continue;

        char path[1024];
        int n = snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        if (n <= 0 || (size_t)n >= sizeof(path)) continue;

        uint8_t *buf = NULL;
        long len = read_bounded(path, &buf);
        if (len < 0) continue;                          /* unreadable/oversized */

        wubu_spellmod_t *m = wubu_spellmod_open(buf, (size_t)len);
        if (!m) { free(buf); continue; }                /* not our format */

        int rc = wubu_spell_registry_add_owned(r, m, buf);
        if (rc != WUBU_SPELL_OK && rc != WUBU_SPELL_NOMEM) {
            /* invalid handle: nothing was taken, release what we allocated */
            wubu_spellmod_close(m);
            free(buf);
            continue;
        }
        if (rc == WUBU_SPELL_NOMEM) break;      /* registry full */
        registered++;
    }

    closedir(d);
    return registered;
}
