/* styx_fid.c -- Styx fid (file-identifier) management subsystem.
 *
 * Self-contained: styx_fid_alloc / styx_fid_lookup / styx_fid_free. Uses
 * styx_server_t / styx_fid_t / STYX_MAX_FIDS (styx.h). Minimal includes.
 */

#include "styx.h"
#include <string.h>

styx_fid_t *styx_fid_alloc(styx_server_t *srv, uint32_t fid) {
    for (int i = 0; i < STYX_MAX_FIDS; i++) {
        if (!srv->fids[i].in_use) {
            memset(&srv->fids[i], 0, sizeof(styx_fid_t));
            srv->fids[i].in_use = 1;
            srv->fids[i].fid = fid;
            return &srv->fids[i];
        }
    }
    return NULL;
}

styx_fid_t *styx_fid_lookup(styx_server_t *srv, uint32_t fid) {
    for (int i = 0; i < STYX_MAX_FIDS; i++) {
        if (srv->fids[i].in_use && srv->fids[i].fid == fid)
            return &srv->fids[i];
    }
    return NULL;
}

void styx_fid_free(styx_server_t *srv, uint32_t fid) {
    styx_fid_t *f = styx_fid_lookup(srv, fid);
    if (f) f->in_use = 0;
}
