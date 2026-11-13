/*
 * hosted_fs_persist.c -- image file for the guest namespace.
 *
 * Wire format (little-endian, fixed-width fields, no padding games):
 *
 *   0  char     magic[8]   "WUBFS01\0"
 *   8  uint32   count       number of records; must be <= STYXFS_MAX_FILES
 *  12  record[]             one per entry, in namespace order:
 *          uint64 path
 *          uint8  qtype      STX_QTDIR or STX_QTFILE
 *          uint32 data_len   must be <= sizeof(f->data)
 *          char   name[256]  NUL-terminated within the field
 *          uint8  data[data_len]
 *
 * Every multi-byte integer is written with explicit byte loops rather than a
 * struct dump.  Dumping a struct would bake in host padding and endianness, and
 * this file is meant to survive a rebuild and ideally another machine.
 */

#include "hosted.h"
#include "hosted_internal.h"
#include "hosted_fs_persist.h"
#include "../runtime/styx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FS_PERSIST_MAGIC "WUBFS01"
#define FS_PERSIST_HDR   12

static const char *kStoreEnv = "WUBU_FS_STORE";

/* ------------------------------------------------------------------ */
/* where the image lives                                               */
/* ------------------------------------------------------------------ */

const char *fs_persist_path(void)
{
    static char path[1024];
    const char *override = getenv(kStoreEnv);
    if (override && *override) {
        snprintf(path, sizeof(path), "%s", override);
        return path;
    }

    const char *base = getenv("XDG_DATA_HOME");
    if (!base || !*base) {
        const char *home = getenv("HOME");
        if (!home || !*home) return NULL;
        snprintf(path, sizeof(path), "%s/.local/share/wubu/fs.img", home);
    } else {
        snprintf(path, sizeof(path), "%s/wubu/fs.img", base);
    }
    return path;
}

long fs_persist_size(void)
{
    const char *p = fs_persist_path();
    if (!p) return -1;
    struct stat st;
    if (stat(p, &st) != 0) return -1;
    return (long)st.st_size;
}

static int ensure_parent_dir(const char *path)
{
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char *slash = strrchr(tmp, '/');
    if (!slash) return -1;
    *slash = '\0';

    /* mkdir -p, one component at a time.  EEXIST at any step is fine: another
     * process may have created it, or it was already there. */
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(tmp, 0700) != 0 && errno != EEXIST) return -1;
        *p = '/';
    }
    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) return -1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* little-endian helpers                                               */
/* ------------------------------------------------------------------ */

static void put_u32(uint8_t *b, uint32_t v)
{
    b[0] = (uint8_t)(v & 0xff);
    b[1] = (uint8_t)((v >> 8) & 0xff);
    b[2] = (uint8_t)((v >> 16) & 0xff);
    b[3] = (uint8_t)((v >> 24) & 0xff);
}

static void put_u64(uint8_t *b, uint64_t v)
{
    for (int i = 0; i < 8; i++) b[i] = (uint8_t)((v >> (8 * i)) & 0xff);
}

static int get_u32(const uint8_t *b, uint32_t *out)
{
    *out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return 0;
}

static int get_u64(const uint8_t *b, uint64_t *out)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | b[i];
    *out = v;
    return 0;
}

/* ------------------------------------------------------------------ */
/* save                                                                */
/* ------------------------------------------------------------------ */

int fs_persist_save(void)
{
    const char *path = fs_persist_path();
    if (!path) return -1;

    if (ensure_parent_dir(path) != 0) return -1;

    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *fp = fopen(tmp_path, "wb");
    if (!fp) return -1;

    uint8_t hdr[FS_PERSIST_HDR];
    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, FS_PERSIST_MAGIC, 7);          /* eighth byte stays NUL */
    put_u32(hdr + 8, (uint32_t)g_nfiles);

    int ok = 1;
    if (fwrite(hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) ok = 0;

    for (int i = 0; ok && i < g_nfiles; i++) {
        const styxfs_file_t *f = &g_fs[i];

        /* Refuse to serialise an entry that cannot be represented.  A name
         * longer than the field would truncate silently on load, so treat it
         * as a failure instead of writing a lie. */
        size_t nlen = strlen(f->name);
        if (nlen >= sizeof(f->name)) { ok = 0; break; }
        if (f->data_len > sizeof(f->data)) { ok = 0; break; }

        uint8_t rh[13];
        put_u64(rh, f->path);
        rh[8] = f->qtype;
        put_u32(rh + 9, f->data_len);
        if (fwrite(rh, 1, sizeof(rh), fp) != sizeof(rh)) { ok = 0; break; }

        uint8_t namebuf[sizeof(f->name)];
        memset(namebuf, 0, sizeof(namebuf));
        memcpy(namebuf, f->name, nlen);
        if (fwrite(namebuf, 1, sizeof(namebuf), fp) != sizeof(namebuf)) { ok = 0; break; }

        if (f->data_len > 0 &&
            fwrite(f->data, 1, f->data_len, fp) != f->data_len) { ok = 0; break; }
    }

    if (ok && fflush(fp) != 0) ok = 0;
    if (fclose(fp) != 0) ok = 0;

    if (!ok) {
        unlink(tmp_path);
        return -1;
    }

    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* load                                                                */
/* ------------------------------------------------------------------ */

int fs_persist_load(int replace)
{
    const char *path = fs_persist_path();
    if (!path) return -1;

    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;                 /* first boot: nothing to restore */

    int rc = -1;
    styxfs_file_t *restore = NULL;

    uint8_t hdr[FS_PERSIST_HDR];
    if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) goto done;
    if (memcmp(hdr, FS_PERSIST_MAGIC, 7) != 0) goto done;

    uint32_t count = 0;
    get_u32(hdr + 8, &count);
    if (count > STYXFS_MAX_FILES) goto done;

    /* Decode into a scratch array first: a malformed tail must not leave the
     * live namespace half-replaced. */
    restore = calloc(count ? count : 1, sizeof(*restore));
    if (!restore) goto done;

    for (uint32_t i = 0; i < count; i++) {
        uint8_t rh[13];
        if (fread(rh, 1, sizeof(rh), fp) != sizeof(rh)) goto done;

        uint64_t path_no = 0;
        uint32_t dlen = 0;
        get_u64(rh, &path_no);
        uint8_t qtype = rh[8];
        get_u32(rh + 9, &dlen);

        if (qtype != STX_QTDIR && qtype != STX_QTFILE) goto done;
        if (dlen > sizeof(restore[i].data)) goto done;

        uint8_t namebuf[sizeof(restore[i].name)];
        if (fread(namebuf, 1, sizeof(namebuf), fp) != sizeof(namebuf)) goto done;
        namebuf[sizeof(namebuf) - 1] = '\0';
        if (strlen((char *)namebuf) >= sizeof(restore[i].name)) goto done;

        styxfs_file_t *f = &restore[i];
        f->path = path_no;
        f->qtype = qtype;
        f->length = (qtype & STX_QTDIR) ? 0 : (uint64_t)dlen;
        f->data_len = dlen;
        memcpy(f->name, namebuf, sizeof(f->name));

        if (dlen > 0) {
            if (fread(f->data, 1, dlen, fp) != dlen) goto done;
        }
    }

    if (replace) fs_reset();
    for (uint32_t i = 0; i < count; i++) {
        if (g_nfiles >= STYXFS_MAX_FILES) break;
        g_fs[g_nfiles++] = restore[i];
    }
    rc = (int)count;

done:
    free(restore);
    fclose(fp);
    return rc;
}
