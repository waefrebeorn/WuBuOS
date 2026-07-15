/*
 * styx_enc.c -- Styx/9P2000 client request encoders + Rerror builder.
 *
 * Self-contained: every function here packs a T-message (or an Rerror)
 * into a wire buffer using the inline helpers from styx.h /
 * styx_internal.h. These are the bytes a *client* sends; the server
 * side (styx_serve.c) handles inbound dispatch and builds R-messages.
 */

#include "styx.h"
#include "styx_internal.h"
#include <string.h>

/* -- Message Building  --  Client T-message Helpers ---------------- */

void styx_build_tversion(uint8_t *buf, uint32_t *len,
                         uint32_t msize, const char *version) {
    uint32_t pos = 7;
    styx_put32(buf + pos, msize); pos += 4;
    pos += styx_putstr(buf + pos, version);
    styx_write_header(buf, pos, STX_TVERSION, 0); /* Tag=0 for version */
    *len = pos;
}

void styx_build_tattach(uint8_t *buf, uint32_t *len,
                        uint16_t tag, uint32_t fid,
                        uint32_t afid, const char *aname) {
    uint32_t pos = 7;
    styx_put32(buf + pos, fid); pos += 4;
    styx_put32(buf + pos, afid); pos += 4;
    pos += styx_putstr(buf + pos, aname ? aname : "");
    styx_write_header(buf, pos, STX_TATTACH, tag);
    *len = pos;
}

void styx_build_twalk(uint8_t *buf, uint32_t *len,
                      uint16_t tag, uint32_t fid,
                      uint32_t newfid, const char **wname, int nwname) {
    uint32_t pos = 7;
    styx_put32(buf + pos, fid); pos += 4;
    styx_put32(buf + pos, newfid); pos += 4;
    styx_put16(buf + pos, (uint16_t)nwname); pos += 2;
    for (int i = 0; i < nwname && i < 16; i++) {
        pos += styx_putstr(buf + pos, wname[i]);
    }
    styx_write_header(buf, pos, STX_TWALK, tag);
    *len = pos;
}

void styx_build_topen(uint8_t *buf, uint32_t *len,
                      uint16_t tag, uint32_t fid, int mode) {
    uint32_t pos = 7;
    styx_put32(buf + pos, fid); pos += 4;
    buf[pos] = (uint8_t)mode; pos++;
    styx_write_header(buf, pos, STX_TOPEN, tag);
    *len = pos;
}

void styx_build_tread(uint8_t *buf, uint32_t *len,
                      uint16_t tag, uint32_t fid,
                      uint64_t offset, uint32_t count) {
    uint32_t pos = 7;
    styx_put32(buf + pos, fid); pos += 4;
    styx_put64(buf + pos, offset); pos += 8;
    styx_put32(buf + pos, count); pos += 4;
    styx_write_header(buf, pos, STX_TREAD, tag);
    *len = pos;
}

void styx_build_twrite(uint8_t *buf, uint32_t *len,
                       uint16_t tag, uint32_t fid,
                       uint64_t offset, uint32_t count,
                       const uint8_t *data) {
    uint32_t pos = 7;
    styx_put32(buf + pos, fid); pos += 4;
    styx_put64(buf + pos, offset); pos += 8;
    styx_put32(buf + pos, count); pos += 4;
    if (count > 0) memcpy(buf + pos, data, count);
    pos += count;
    styx_write_header(buf, pos, STX_TWRITE, tag);
    *len = pos;
}

void styx_build_tclunk(uint8_t *buf, uint32_t *len,
                       uint16_t tag, uint32_t fid) {
    uint32_t pos = 7;
    styx_put32(buf + pos, fid); pos += 4;
    styx_write_header(buf, pos, STX_TCLUNK, tag);
    *len = pos;
}

void styx_build_tstat(uint8_t *buf, uint32_t *len,
                      uint16_t tag, uint32_t fid) {
    uint32_t pos = 7;
    styx_put32(buf + pos, fid); pos += 4;
    styx_write_header(buf, pos, STX_TSTAT, tag);
    *len = pos;
}

/* -- Rerror Builder ----------------------------------------------- */

void styx_error(uint16_t tag, const char *errmsg,
                uint8_t *outbuf, uint32_t *outlen) {
    uint32_t pos = 7;
    pos += styx_putstr(outbuf + pos, errmsg);
    styx_write_header(outbuf, pos, STX_RERROR, tag);
    *outlen = pos;
}
