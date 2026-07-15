/*
 * styx_parse.c -- Styx/9P2000 client-side response/send parsers.
 *
 * Self-contained: each function validates a received T/R message and
 * extracts its fields using the inline unpack helpers from styx.h.
 * Pairs with the request encoders in styx_enc.c; the server dispatch
 * lives in styx_serve.c.
 */

#include "styx.h"
#include <string.h>

/* -- Response Parsing  --  Client Helpers ---------------------------- */

int styx_parse_version(const uint8_t *buf, uint32_t len,
                       uint32_t *msize, char *version) {
    (void)len;
    if (buf[4] != STX_RVERSION) return -1;
    *msize = styx_get32(buf + 7);
    const uint8_t *p = styx_getstr(buf + 11, version, STYX_MAX_FNAME);
    (void)p;
    return 0;
}

int styx_parse_attach(const uint8_t *buf, uint32_t len,
                      uint32_t *fid, uint32_t *afid, char *aname) {
    (void)len; (void)fid; (void)afid; (void)aname;
    if (buf[4] != STX_TATTACH) return -1;
    *fid = styx_get32(buf + 7);
    *afid = styx_get32(buf + 11);
    styx_getstr(buf + 15, aname, STYX_MAX_FNAME);
    return 0;
}

/* -- Parsing Helpers for Client Messages -------------------------- */

int styx_parse_open(const uint8_t *buf, uint32_t len,
                    uint32_t *fid, int *mode) {
    if (!buf || len < 12 || buf[4] != STX_TOPEN) return -1;
    *fid = styx_get32(buf + 7);
    *mode = buf[11];
    return 0;
}

int styx_parse_read(const uint8_t *buf, uint32_t len,
                    uint32_t *fid, uint64_t *offset, uint32_t *count) {
    if (!buf || len < 23 || buf[4] != STX_TREAD) return -1;
    *fid = styx_get32(buf + 7);
    *offset = styx_get64(buf + 11);
    *count = styx_get32(buf + 19);
    return 0;
}

int styx_parse_write(const uint8_t *buf, uint32_t len,
                     uint32_t *fid, uint64_t *offset,
                     uint32_t *count, const uint8_t **data) {
    if (!buf || len < 24 || buf[4] != STX_TWRITE) return -1;
    *fid = styx_get32(buf + 7);
    *offset = styx_get64(buf + 11);
    *count = styx_get32(buf + 19);
    *data = buf + 23;
    return 0;
}

int styx_parse_clunk(const uint8_t *buf, uint32_t len,
                     uint32_t *fid) {
    if (!buf || len < 11 || buf[4] != STX_TCLUNK) return -1;
    *fid = styx_get32(buf + 7);
    return 0;
}

int styx_parse_stat(const uint8_t *buf, uint32_t len,
                    uint32_t *fid) {
    if (!buf || len < 11 || buf[4] != STX_TSTAT) return -1;
    *fid = styx_get32(buf + 7);
    return 0;
}

int styx_parse_walk(const uint8_t *buf, uint32_t len,
                    uint32_t *fid, uint32_t *newfid,
                    char wnames[][STYX_MAX_FNAME], int *nwname) {
    if (!buf || len < 18 || buf[4] != STX_TWALK) return -1;
    *fid = styx_get32(buf + 7);
    *newfid = styx_get32(buf + 11);
    *nwname = styx_get16(buf + 15);
    if (*nwname > 16) *nwname = 16;
    const uint8_t *p = buf + 17;
    for (int i = 0; i < *nwname; i++) {
        p = styx_getstr(p, wnames[i], STYX_MAX_FNAME);
    }
    return 0;
}
