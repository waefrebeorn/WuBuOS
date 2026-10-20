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
#include <errno.h>

/* Crash-hardening: every message handler must confirm the declared msg_size
 * covers the fields it reads before touching inbuf. A short/malformed frame
 * must produce a clean Rerror, never an out-of-bounds read (the AGI-facing
 * Styx server must never crash on hostile or truncated input). */
#define STYX_NEED(off) do { \
    if ((uint32_t)(off) > msg_size) { \
        styx_error(tag, "Truncated message", outbuf, outlen); \
        return 0; \
    } \
} while (0)

/* Outbound safety: a response builder must never write past STYX_MAX_MSG.
 * Returns -1 (caller emits Rerror) if it would. */
#define STYX_FITS(off) ((uint32_t)(off) <= STYX_MAX_MSG)

/* -- Server Initialization ---------------------------------------- */

void styx_init(styx_server_t *srv) {
    memset(srv, 0, sizeof(*srv));
    srv->next_tag = 1;
    srv->msize = STYX_MAX_MSG;
    srv->max_msize = STYX_MAX_MSG;
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
    if (count > STYX_MAX_DATA) count = STYX_MAX_DATA;
    /* Defensive: a callback must never ask us to copy more than it claimed. */
    uint32_t pos = 7;
    if (pos + 4 + count > STYX_MAX_MSG) count = STYX_MAX_MSG - pos - 4;
    styx_put32(buf + pos, count); pos += 4;
    if (count > 0 && data) memcpy(buf + pos, data, count);
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

/* Rauth: aqid[13] (0intro:159-170). */
static int build_rauth(uint8_t *buf, uint32_t *len,
                       uint16_t tag, const styx_qid_t *aqid) {
    uint32_t pos = 7;
    buf[pos] = aqid->type; pos++;
    styx_put32(buf + pos, aqid->version); pos += 4;
    styx_put64(buf + pos, aqid->path); pos += 8;
    styx_write_header(buf, pos, STX_RAUTH, tag);
    *len = pos;
    return 0;
}

/* Rcreate: qid[13] iounit[4] -- same body shape as Ropen (open(5):31-40). */
static int build_rcreate(uint8_t *buf, uint32_t *len,
                         uint16_t tag, const styx_qid_t *qid,
                         uint32_t iounit) {
    uint32_t pos = 7;
    buf[pos] = qid->type; pos++;
    styx_put32(buf + pos, qid->version); pos += 4;
    styx_put64(buf + pos, qid->path); pos += 8;
    styx_put32(buf + pos, iounit); pos += 4;
    styx_write_header(buf, pos, STX_RCREATE, tag);
    *len = pos;
    return 0;
}

/* Rflush: empty body (0intro:476-492 -- always Rflush, never Rerror). */
static int build_rflush(uint8_t *buf, uint32_t *len, uint16_t tag) {
    styx_write_header(buf, 7, STX_RFLUSH, tag);
    *len = 7;
    return 0;
}

/* Rwstat: empty body (0intro:296-306). The old code answered Twstat
 * with Rclunk -- wire-level non-conformance a real client rejects. */
static int build_rwstat(uint8_t *buf, uint32_t *len, uint16_t tag) {
    styx_write_header(buf, 7, STX_RWSTAT, tag);
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
    /* Per-connection ceiling: after Tversion the negotiated msize binds, and a
     * peer that ignores it must be told so rather than served. */
    if (srv->msize && msg_size > srv->msize) {
        styx_error(tag, "Message exceeds negotiated msize", outbuf, outlen);
        return 0;
    }

    switch (msg_type) {
    case STX_TVERSION: {
        /* Negotiate protocol version. msize semantics (0intro:138-148):
         * the reply carries min(client msize, server ceiling), a client
         * below the floor cannot even carry an Rversion and is refused,
         * and the reply ECHOES the request's tag (clients send NOTAG;
         * the server must answer with the same tag, not a hardcoded 0). */
        uint32_t c_msize = styx_get32(inbuf + 7);
        STYX_NEED(11 + 2);
        char c_version[16] = {0};
        styx_getstr(inbuf + 11, c_version, sizeof(c_version),
                    (int)msg_size - 11);

        /* Accept if 9P2000 or 9P2000.u */
        if (strncmp(c_version, "9P2000", 6) != 0 &&
            strcmp(c_version, "9P2000.u") != 0) {
            styx_error(tag, "Unknown protocol version", outbuf, outlen);
            return 0;
        }

        /* Floor: below this an Rversion/Rerror reply cannot even fit
         * in the negotiated size; the peer is told so. */
        #define STYX_MSIZE_FLOOR 64
        if (c_msize < STYX_MSIZE_FLOOR) {
            styx_error(tag, "msize too small", outbuf, outlen);
            return 0;
        }
        uint32_t ceiling = srv->max_msize ? srv->max_msize : STYX_MAX_MSG;
        if (ceiling > STYX_MAX_MSG) ceiling = STYX_MAX_MSG;
        srv->msize = (c_msize < ceiling) ? c_msize : ceiling;
        strcpy(srv->version, "9P2000");
        srv->connected = 1;
        return build_rversion(outbuf, outlen, tag, srv->msize, "9P2000");
    }

    case STX_TAUTH: {
        /* 9P2000 conformance (auth(5), 0intro:159-170): Tauth answers
         * with Rauth(aqid) or Rerror -- never Rattach. A server that
         * does not require authentication REFUSES: the client then
         * attaches with afid = NOFID. The old code replied Rattach,
         * which a real client (v9fs/lib9p/9pfuse) rejects. */
        STYX_NEED(11 + 4);
        uint32_t afid = styx_get32(inbuf + 7);
        char uname[STYX_MAX_FNAME] = {0};
        char aname[STYX_MAX_FNAME] = {0};
        const uint8_t *p = styx_getstr(inbuf + 11, uname, sizeof(uname),
                                       (int)msg_size - 11);
        styx_getstr(p, aname, sizeof(aname),
                    (int)msg_size - (int)(p - inbuf));
        (void)afid;
        if (srv->auth) {
            styx_qid_t aqid;
            if (srv->auth(srv, afid, uname, aname, &aqid) != 0) {
                styx_error(tag, "Authentication failed", outbuf, outlen);
                return 0;
            }
            return build_rauth(outbuf, outlen, tag, &aqid);
        }
        styx_error(tag, "authentication not required", outbuf, outlen);
        return 0;
    }

    case STX_TFLUSH: {
        /* 9P2000 conformance (0intro:476-492): the server must not reply
         * to oldtag and must answer Rflush immediately -- never Rerror,
         * even for an unknown/already-answered tag. */
        STYX_NEED(9);
        uint16_t oldtag = styx_get16(inbuf + 7);
        if (srv->flush) srv->flush(srv, oldtag);
        return build_rflush(outbuf, outlen, tag);
    }

    case STX_TCREATE: {
        /* 9P2000 conformance (open(5):18-30, 0intro:224-229):
         * fid[4] name[s] perm[4] mode[1]; success re-roots the fid on the
         * new file and answers Rcreate(qid, iounit). The old dispatch had
         * no Tcreate arm at all (the callback existed but was unreachable
         * on the wire). */
        STYX_NEED(13);
        uint32_t fid = styx_get32(inbuf + 7);
        char name[STYX_MAX_FNAME] = {0};
        const uint8_t *p = styx_getstr(inbuf + 11, name, sizeof(name),
                                       (int)msg_size - 11);
        if ((size_t)(p - inbuf) + 5 > (size_t)msg_size) {
            styx_error(tag, "Bad Tcreate", outbuf, outlen);
            return 0;
        }
        uint32_t perm = styx_get32(p); p += 4;
        uint8_t  mode = *p;
        if (!srv->create) {
            styx_error(tag, "Create not supported", outbuf, outlen);
            return 0;
        }
        styx_qid_t qid;
        if (srv->create(srv, fid, name, perm, mode, &qid) != 0) {
            {
            int e = errno;   /* the callback set the REASON; name it */
            styx_error(tag, e ? styx_errno_ename(e) : "Create failed",
                       outbuf, outlen);
        }
            return 0;
        }
        return build_rcreate(outbuf, outlen, tag, &qid, 0 /* iounit */);
    }

    case STX_TATTACH: {
        STYX_NEED(15);
        uint32_t fid = styx_get32(inbuf + 7);
        (void)styx_get32(inbuf + 11); /* afid  --  unused in ring-0 single-user */
        char aname[STYX_MAX_FNAME] = {0};
        /* aname is a string starting at offset 15; the string reader bounds
         * itself against the buffer end (inbuf+msg_size) so this is safe. */
        styx_getstr(inbuf + 15, aname, sizeof(aname), (int)msg_size - 15);

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
            /* Clone fid. walk(5): a walk to an already-in-use newfid is a
             * protocol error, not a silent second entry -- the old code
             * alloc'd blindly and two slots could carry the same fid. */
            if (styx_fid_lookup(srv, newfid)) {
                styx_error(tag, "fid in use", outbuf, outlen);
                return 0;
            }
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
            int avail = (int)msg_size - (int)(p - inbuf);
            p = styx_getstr(p, wnames[i], STYX_MAX_FNAME, avail);
            if (!p) break;
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

        /* 9P2000 iounit cap (0intro:171-180, read(5)): the Rread data must
         * fit in the negotiated msize minus the 11-byte Rread header -- a
         * client that negotiated 128 bytes must never receive a 1000-byte
         * reply, whatever it asked for. */
        uint32_t cap = (srv->msize && srv->msize > 11) ? srv->msize - 11
                                                       : STYX_MAX_DATA;
        if (cap > STYX_MAX_DATA) cap = STYX_MAX_DATA;
        if (count > cap) count = cap;

        if (!srv->read) {
            /* Default: return nothing (EOF) */
            return build_rread(outbuf, outlen, tag, NULL, 0);
        }

        uint8_t rdata[STYX_MAX_DATA];
        uint32_t nread = 0;
        if (srv->read(srv, fid, offset, count, rdata, &nread) != 0) {
            {
            int e = errno;   /* the callback set the REASON; name it */
            styx_error(tag, e ? styx_errno_ename(e) : "Read failed",
                       outbuf, outlen);
        }
            return 0;
        }
        f->offset = offset + nread;
        return build_rread(outbuf, outlen, tag, rdata, nread);
    }

    case STX_TWRITE: {
        uint32_t fid = styx_get32(inbuf + 7);
        uint64_t offset = styx_get64(inbuf + 11);
        uint32_t count = styx_get32(inbuf + 19);

        /* The write payload begins at inbuf+23 and is `count` bytes long.
         * Never let a malicious/truncated frame read past the message. */
        if ((uint64_t)23 + count > msg_size) {
            styx_error(tag, "Write payload overruns message", outbuf, outlen);
            return 0;
        }

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
            {
            int e = errno;   /* the callback set the REASON; name it */
            styx_error(tag, e ? styx_errno_ename(e) : "Write failed",
                       outbuf, outlen);
        }
            return 0;
        }
        f->offset = offset + nwritten;
        return build_rwrite(outbuf, outlen, tag, nwritten);
    }

    case STX_TCLUNK: {
        uint32_t fid = styx_get32(inbuf + 7);

        /* A clunk on an unknown or already-clunked fid is a protocol error,
         * not a no-op: silently succeeding hides fid leaks and double-clunks. */
        if (!styx_fid_lookup(srv, fid)) {
            styx_error(tag, "Bad fid in clunk", outbuf, outlen);
            return 0;
        }
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
        p = styx_getstr(p, dir.name, STYX_MAX_FNAME, (int)msg_size - (int)(p - inbuf));
        p = styx_getstr(p, dir.uid, STYX_MAX_FNAME, (int)msg_size - (int)(p - inbuf));
        p = styx_getstr(p, dir.gid, STYX_MAX_FNAME, (int)msg_size - (int)(p - inbuf));
        p = styx_getstr(p, dir.muid, STYX_MAX_FNAME, (int)msg_size - (int)(p - inbuf));
        (void)p;

        if (srv->wstat(srv, fid, &dir) != 0) {
            {
            int e = errno;   /* the callback set the REASON; name it */
            styx_error(tag, e ? styx_errno_ename(e) : "Wstat failed",
                       outbuf, outlen);
        }
            return 0;
        }
        /* 9P2000 conformance (0intro:296-306): Twstat answers Rwstat.
         * The old code returned Rclunk here. */
        return build_rwstat(outbuf, outlen, tag);
    }

    default:
        styx_error(tag, "Unknown message type", outbuf, outlen);
        return 0;
    }
}
