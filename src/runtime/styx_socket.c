/* styx_socket.c -- see styx_socket.h. Messages are already size-prefixed by the
 * styx_build_* builders, so nothing here adds a second prefix. */
#include "styx_socket.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int rd_full(int fd, uint8_t *buf, size_t n)
{
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, buf + got, n - got);
        if (r == 0) return (int)got;
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        got += (size_t)r;
    }
    return (int)got;
}

static int wr_full(int fd, const uint8_t *buf, size_t n)
{
    size_t put = 0;
    while (put < n) {
        ssize_t r = write(fd, buf + put, n - put);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        put += (size_t)r;
    }
    return (int)put;
}

/* This tree's styx_put32/styx_get32 are little-endian, so framing follows it
 * rather than network order. (Same trap as the directory records.) */
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int styx_serve_one(int fd, styx_server_t *srv)
{
    if (fd < 0 || !srv) return -1;

    uint8_t in[STYX_MAX_MSG], out[STYX_MAX_MSG];
    int g0 = rd_full(fd, in, 4);
    if (g0 != 4) { fprintf(stderr, "[so hdr=%d] ", g0); return 0; }

    uint32_t sz = le32(in);
    if (sz < 7 || sz > STYX_MAX_MSG) return -1;
    int g1 = rd_full(fd, in + 4, sz - 4);
    if (g1 != (int)(sz - 4)) { fprintf(stderr, "[so body=%d want=%u] ", g1, sz - 4); return -1; }

    uint32_t outlen = 0;
    int sr = styx_serve(srv, in, sz, out, &outlen);
    if (sr != 0) { fprintf(stderr, "[so serve=%d sz=%u] ", sr, sz); return -1; }
    if (outlen < 7) { fprintf(stderr, "[so outlen=%u] ", outlen); return -1; }
    int w = wr_full(fd, out, outlen);
    if (w != (int)outlen) { fprintf(stderr, "[so wr=%d want=%u] ", w, outlen); return -1; }
    return (int)outlen;
}

int styx_socket_request(int fd, const uint8_t *req, uint32_t len,
                        uint8_t *reply, uint32_t cap, uint32_t *rlen)
{
    if (fd < 0 || !req || !reply || !rlen || len < 7) return -1;
    if (wr_full(fd, req, len) != (int)len) return -1;
    if (rd_full(fd, reply, 4) != 4) return -1;
    uint32_t sz = le32(reply);
    if (sz < 7 || sz > cap) return -1;
    if (rd_full(fd, reply + 4, sz - 4) != (int)(sz - 4)) return -1;
    *rlen = sz;
    return 0;
}
