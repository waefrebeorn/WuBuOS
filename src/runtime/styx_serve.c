/*
 * styx_serve.c -- Styx/9P2000 server: init + inbound dispatch +
 *                 R-message response builders.
 *
 * Self-contained: the response encoders (build_r*) are private to this
 * translation unit; styx_serve() reads a T-message, drives the
 * server callback vector in styx_server_t (or the default ring-0
 * single-user behaviour), and packs the matching R-message. Client
 * request encoders live in styx_enc.c; client parsers in styx_parse.c.
 */

#include "styx.h"
#include "styx_internal.h"
#include <string.h>

/* -- Server Initialization ---------------------------------------- */

void styx_init(styx_server_t *srv) {
    memset(srv, 0, sizeof(*srv));
    srv->next_tag = 1;
    srv->msize = STYX_MAX_MSG;
    strcpy(srv->version, "9P2000");
    srv->connected = 0;
}

/* -- Response Builders (private) ---------------------------------- */

static int build_rversion(uint8_t *buf, uint32_t *len,
                          uint16_t tag, uint32_t msize,
                          const char *version) {
    uint32_t pos = 7; /* After header */
    styx_put32(buf + pos, msize); pos += 4;
    pos += styx_putstr(buf + pos, version);
    styx_write_header(buf, pos, STX_RVERSION, tag);
    *len = pos;
    return 0;
}

static int build_rattach(uint8_t *buf, uint32_t *len,
                         uint16_t tag, const styx_qid_t *qid) {
    uint32_t pos = 7;
    buf[pos] = qid->type; pos++;
    styx_put32(buf + pos, qid->version); pos += 4;
    styx_put64(buf + pos, qid->path); pos += 8;
    styx_write_header(buf, pos, STX_RATTACH, tag);
    *len = pos;
    return 0;
}

static int build_rwalk(uint8_t *buf, uint32_t *len,
                       uint16_t tag, const styx_qid_t *qids,
                       int nqid) {
    uint32_t pos = 7;
    styx_put16(buf + pos, (uint16_t)nqid); pos += 2;
    for (int i = 0; i < nqid; i++) {
        buf[pos] = qids[i].type; pos++;
        styx_put32(buf + pos, qids[i].version); pos += 4;
        styx_put64(buf + pos, qids[i].path); pos += 8;
    }
    styx_write_header(buf, pos, STX_RWALK, tag);
    *len = pos;
    return 0;
}

static int build_ropen(uint8_t *buf, uint32_t *len,
                       uint16_t tag, const styx_qid_t *qid,
                       uint32_t iounit) {
    uint32_t pos = 7;
    buf[pos] = qid->type; pos++;
    styx_put32(buf + pos, qid->version); pos += 4;
    styx_put64(buf + pos, qid->path); pos += 8;
    styx_put32(buf + pos, iounit); pos += 4;
    styx_write_header(buf, pos, STX_ROPEN, tag);
    *len = pos;
    return 0;
}

static int build_rread(uint8_t *buf, uint32_t *len,
                       uint16_t tag, const uint8_t *data,
                       uint32_t count) {
    uint32_t pos = 7;
    styx_put32(buf + pos, count); pos += 4;
    if (count > 0) memcpy(buf + pos, data, count);
    pos += count;
    styx_write_header(buf, pos, STX_RREAD, tag);
    *len = pos;
    return 0;
}

static int build_rwrite(uint8_t *buf, uint32_t *len,
                        uint16_t tag, uint32_t count) {
    uint32_t pos = 7;
    styx_put32(buf + pos, count); pos += 4;
    styx_write_header(buf, pos, STX_RWRITE, tag);
    *len = pos;
    return 0;
}

/* Build Rclunk / Rremove response (empty, just header + tag) */
static int build_rclunk(uint8_t *buf, uint32_t *len, uint16_t tag) {
    styx_write_header(buf, 7, STX_RCLUNK, tag);
    *len = 7;
    return 0;
}

static int build_rremove(uint8_t *buf, uint32_t *len, uint16_t tag) {
    styx_write_header(buf, 7, STX_RREMOVE, tag);
    *len = 7;
    return 0;
}

static int build_rstat(uint8_t *buf, uint32_t *len,
                       uint16_t tag, const styx_dir_t *dir) {
    uint32_t pos = 7;
    /* Dir size  --  we need to compute it first, then fill in */
    uint32_t dir_start = pos + 2; /* Skip 2-byte size field */
    pos = dir_start;
    styx_put16(buf + pos, dir->type); pos += 2;
    styx_put32(buf + pos, dir->dev); pos += 4;
    buf[pos] = dir->qid.type; pos++;
    styx_put32(buf + pos, dir->qid.version); pos += 4;
    styx_put64(buf + pos, dir->qid.path); pos += 8;
    styx_put32(buf + pos, dir->mode); pos += 4;
    styx_put32(buf + pos, dir->atime); pos += 4;
    styx_put32(buf + pos, dir->mtime); pos += 4;
    styx_put64(buf + pos, dir->length); pos += 8;
    pos += styx_putstr(buf + pos, dir->name);
    pos += styx_putstr(buf + pos, dir->uid);
    pos += styx_putstr(buf + pos, dir->gid);
    pos += styx_putstr(buf + pos, dir->muid);
    uint32_t dir_total = pos - dir_start;
    styx_put16(buf + dir_start - 2, (uint16_t)dir_total);
    styx_write_header(buf, pos, STX_RSTAT, tag);
    *len = pos;
    return 0;
}

/* -- Server Message Dispatch -------------------------------------- */

int styx_serve(styx_server_t *srv,
               const uint8_t *inbuf, uint32_t inlen,
               uint8_t *outbuf, uint32_t *outlen) {
    if (!srv || !inbuf || !outbuf || inlen < 7) return -1;

    uint32_t msg_size = styx_get32(inbuf);
    uint8_t  msg_type = inbuf[4];
    uint16_t tag = styx_get16(inbuf + 5);

    if (msg_size > inlen || msg_size > STYX_MAX_MSG) {
        styx_error(tag, "Bad message size", outbuf, outlen);
        return 0;
    }

    switch (msg_type) {
    case STX_TVERSION: {
        /* Negotiate protocol version */
        uint32_t c_msize = styx_get32(inbuf + 7);
        char c_version[16] = {0};
        styx_getstr(inbuf + 11, c_version, sizeof(c_version));

        /* Accept if 9P2000 or 9P2000.u */
        if (strncmp(c_version, "9P2000", 6) != 0 &&
            strcmp(c_version, "9P2000.u") != 0) {
            styx_error(0, "Unknown protocol version", outbuf, outlen);
            return 0;
        }

        srv->msize = (c_msize < STYX_MAX_MSG) ? c_msize : STYX_MAX_MSG;
        strcpy(srv->version, "9P2000");
        srv->connected = 1;
        return build_rversion(outbuf, outlen, 0, srv->msize, "9P2000");
    }

    case STX_TAUTH: {
        /* For single-user ring-0, auth is a no-op */
        styx_qid_t qid = {STX_QTFILE, 0, 0};
        return build_rattach(outbuf, outlen, tag, &qid);
    }

    case STX_TATTACH: {
        uint32_t fid = styx_get32(inbuf + 7);
        (void)styx_get32(inbuf + 11); /* afid  --  unused in ring-0 single-user */
        char aname[STYX_MAX_FNAME] = {0};
        styx_getstr(inbuf + 15, aname, sizeof(aname));

        if (!srv->attach) {
            /* Default: allocate fid for root */
            styx_fid_t *f = styx_fid_alloc(srv, fid);
            if (!f) {
                styx_error(tag, "Out of fids", outbuf, outlen);
                return 0;
            }
            styx_qid_t root_qid = {STX_QTDIR, 1, 1};
            f->qid = root_qid;
            return build_rattach(outbuf, outlen, tag, &root_qid);
        }

        styx_qid_t qid;
        if (srv->attach(srv, fid, aname) != 0) {
            styx_error(tag, "Attach failed", outbuf, outlen);
            return 0;
        }
        /* Callback should fill qid */
        styx_fid_t *f = styx_fid_lookup(srv, fid);
        if (f) qid = f->qid;
        else { styx_qid_t z = {STX_QTFILE, 0, 0}; qid = z; }
        return build_rattach(outbuf, outlen, tag, &qid);
    }

    case STX_TWALK: {
        uint32_t fid = styx_get32(inbuf + 7);
        uint32_t newfid = styx_get32(inbuf + 11);
        int nwname = styx_get16(inbuf + 15);

        styx_fid_t *f = styx_fid_lookup(srv, fid);
        if (!f) {
            styx_error(tag, "Bad fid in walk", outbuf, outlen);
            return 0;
        }

        if (nwname == 0) {
            /* Clone fid */
            styx_fid_t *nf = styx_fid_alloc(srv, newfid);
            if (!nf) {
                styx_error(tag, "Out of fids", outbuf, outlen);
                return 0;
            }
            *nf = *f;
            nf->fid = newfid;
            return build_rwalk(outbuf, outlen, tag, &f->qid, 1);
        }

        if (!srv->walk) {
            styx_error(tag, "Walk not supported", outbuf, outlen);
            return 0;
        }

        styx_qid_t qids[16];
        char wnames[16][STYX_MAX_FNAME];
        int nwname_actual = nwname > 16 ? 16 : nwname;
        const char *wname_ptrs[16];

        const uint8_t *p = inbuf + 17;
        for (int i = 0; i < nwname_actual; i++) {
            p = styx_getstr(p, wnames[i], STYX_MAX_FNAME);
            wname_ptrs[i] = wnames[i];
        }

        int nwqid = 0;
        int ret = srv->walk(srv, fid, newfid, wname_ptrs,
                            nwname_actual, qids, &nwqid);
        if (ret != 0 && nwqid == 0) {
            styx_error(tag, "Walk failed", outbuf, outlen);
            return 0;
        }
        return build_rwalk(outbuf, outlen, tag, qids, nwqid);
    }

    case STX_TOPEN: {
        uint32_t fid = styx_get32(inbuf + 7);
        int mode = inbuf[11];

        styx_fid_t *f = styx_fid_lookup(srv, fid);
        if (!f) {
            styx_error(tag, "Bad fid in open", outbuf, outlen);
            return 0;
        }

        if (!srv->open) {
            f->open_mode = mode;
            f->offset = 0;
            f->open_fid = f->fid;
            styx_qid_t qid = f->qid;
            return build_ropen(outbuf, outlen, tag, &qid, STYX_MAX_DATA);
        }

        styx_qid_t qid;
        if (srv->open(srv, fid, mode, &qid) != 0) {
            styx_error(tag, "Open failed", outbuf, outlen);
            return 0;
        }
        f->open_mode = mode;
        f->offset = 0;
        f->open_fid = f->fid;
        return build_ropen(outbuf, outlen, tag, &qid, STYX_MAX_DATA);
    }

    case STX_TREAD: {
        uint32_t fid = styx_get32(inbuf + 7);
        uint64_t offset = styx_get64(inbuf + 11);
        uint32_t count = styx_get32(inbuf + 19);

        styx_fid_t *f = styx_fid_lookup(srv, fid);
        if (!f) {
            styx_error(tag, "Bad fid in read", outbuf, outlen);
            return 0;
        }

        if (count > STYX_MAX_DATA) count = STYX_MAX_DATA;

        if (!srv->read) {
            /* Default: return nothing (EOF) */
            return build_rread(outbuf, outlen, tag, NULL, 0);
        }

        uint8_t rdata[STYX_MAX_DATA];
        uint32_t nread = 0;
        if (srv->read(srv, fid, offset, count, rdata, &nread) != 0) {
            styx_error(tag, "Read failed", outbuf, outlen);
            return 0;
        }
        f->offset = offset + nread;
        return build_rread(outbuf, outlen, tag, rdata, nread);
    }

    case STX_TWRITE: {
        uint32_t fid = styx_get32(inbuf + 7);
        uint64_t offset = styx_get64(inbuf + 11);
        uint32_t count = styx_get32(inbuf + 19);

        styx_fid_t *f = styx_fid_lookup(srv, fid);
        if (!f) {
            styx_error(tag, "Bad fid in write", outbuf, outlen);
            return 0;
        }

        if (count > STYX_MAX_WRITE) count = STYX_MAX_WRITE;

        if (!srv->write) {
            styx_error(tag, "Write not supported", outbuf, outlen);
            return 0;
        }

        const uint8_t *wdata = inbuf + 23;
        uint32_t nwritten = 0;
        if (srv->write(srv, fid, offset, count, wdata, &nwritten) != 0) {
            styx_error(tag, "Write failed", outbuf, outlen);
            return 0;
        }
        f->offset = offset + nwritten;
        return build_rwrite(outbuf, outlen, tag, nwritten);
    }

    case STX_TCLUNK: {
        uint32_t fid = styx_get32(inbuf + 7);

        if (srv->clunk) srv->clunk(srv, fid);
        styx_fid_free(srv, fid);
        return build_rclunk(outbuf, outlen, tag);
    }

    case STX_TREMOVE: {
        uint32_t fid = styx_get32(inbuf + 7);

        if (srv->remove) srv->remove(srv, fid);
        styx_fid_free(srv, fid);
        return build_rremove(outbuf, outlen, tag);
    }

    case STX_TSTAT: {
        uint32_t fid = styx_get32(inbuf + 7);

        styx_fid_t *f = styx_fid_lookup(srv, fid);
        if (!f) {
            styx_error(tag, "Bad fid in stat", outbuf, outlen);
            return 0;
        }

        if (!srv->stat) {
            /* Default stat */
            styx_dir_t dir;
            memset(&dir, 0, sizeof(dir));
            dir.qid = f->qid;
            dir.mode = (f->qid.type & STX_QTDIR) ? STX_DMDIR : 0;
            dir.length = 0;
            strcpy(dir.name, "file");
            strcpy(dir.uid, "wubu");
            strcpy(dir.gid, "wubu");
            dir.size = 0; /* Will be computed */
            return build_rstat(outbuf, outlen, tag, &dir);
        }

        styx_dir_t dir;
        if (srv->stat(srv, fid, &dir) != 0) {
            styx_error(tag, "Stat failed", outbuf, outlen);
            return 0;
        }
        return build_rstat(outbuf, outlen, tag, &dir);
    }

    case STX_TWSTAT: {
        uint32_t fid = styx_get32(inbuf + 7);

        if (!srv->wstat) {
            styx_error(tag, "Wstat not supported", outbuf, outlen);
            return 0;
        }

        styx_dir_t dir;
        memset(&dir, 0, sizeof(dir));
        /* Parse the dir record embedded in the message */
        const uint8_t *p = inbuf + 11;
        uint16_t dirsz = styx_get16(p); p += 2;
        (void)dirsz;
        dir.type = styx_get16(p); p += 2;
        dir.dev = styx_get32(p); p += 4;
        dir.qid.type = *p; p++;
        dir.qid.version = styx_get32(p); p += 4;
        dir.qid.path = styx_get64(p); p += 8;
        dir.mode = styx_get32(p); p += 4;
        dir.atime = styx_get32(p); p += 4;
        dir.mtime = styx_get32(p); p += 4;
        dir.length = styx_get64(p); p += 8;
        p = styx_getstr(p, dir.name, STYX_MAX_FNAME);
        p = styx_getstr(p, dir.uid, STYX_MAX_FNAME);
        p = styx_getstr(p, dir.gid, STYX_MAX_FNAME);
        p = styx_getstr(p, dir.muid, STYX_MAX_FNAME);
        (void)p;

        if (srv->wstat(srv, fid, &dir) != 0) {
            styx_error(tag, "Wstat failed", outbuf, outlen);
            return 0;
        }
        /* Rclunk is sent after wstat in 9P2000 */
        return build_rclunk(outbuf, outlen, tag);
    }

    default:
        styx_error(tag, "Unknown message type", outbuf, outlen);
        return 0;
    }
}
