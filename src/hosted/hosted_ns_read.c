/* hosted_ns_read.c -- namespace BYTES, one owner.
 *
 * The tree had two stores behind a single styxfs_ prefix: styxfs_readdir() enumerated a host
 * directory while styxfs_open() resolved srv->open_files[], whose resolve() searches only
 * files that are already open. Listing and opening could never agree, so the file manager
 * could name a file and then be unable to read it -- the preview pane fell back to metadata
 * for every picture.
 *
 * The 9P callbacks in hosted_styxfs.c already read from g_fs, so this reads from g_fs too.
 * Kept in its own file because hosted_styxfs.c is at the 600-line budget: the standard says
 * dissolve, do not expand.
 */
#include "hosted_internal.h"
#include <stdlib.h>
#include <string.h>

/* Read a file's bytes from the namespace the 9P callbacks serve (g_fs).
 *
 * This exists because the tree had TWO stores behind one prefix: styxfs_readdir() enumerated
 * a host directory while styxfs_open() resolved a different table, so a name could be listed
 * and then fail to open -- which is exactly what the file manager's preview hit. One owner for
 * namespace BYTES is this file, because this is where the 9P callbacks already read them.
 * Returns 0 on success, -1 if the name is not a file in the namespace. */
int styxfs_ns_read(const char *path, unsigned char **out, size_t *out_len) {
    if (!path || !out || !out_len) return -1;
    *out = NULL; *out_len = 0;
    const char *name = path;
    while (*name == '/') name++;                 /* the namespace root has no leading slash */
    for (int i = 0; i < g_nfiles; i++) {
        if (g_fs[i].qtype == STX_QTDIR) continue;
        if (strcmp(g_fs[i].name, name) != 0) continue;
        if (g_fs[i].data_len == 0) return -1;
        unsigned char *buf = malloc(g_fs[i].data_len);
        if (!buf) return -1;
        memcpy(buf, g_fs[i].data, g_fs[i].data_len);
        *out = buf;
        *out_len = g_fs[i].data_len;
        return 0;
    }
    return -1;
}

