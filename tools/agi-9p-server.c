/*
 * agi-9p-server.c  --  A3 HOST-side 9P server for the booted kernel.
 *
 * Listens on an AF_UNIX socket, treats it as the COM2 byte stream from the
 * WuBuOS guest, and serves LENGTH-FRAMED 9P2000 messages through the same
 * in-process styxfs server the hosted test uses -- so the on-wire codec and
 * the guest's wbu9pc codec are one and the same framing:
 *
 *   frame := [u32 LE length][9P2000 message bytes]
 *
 *   usage: agi-9p-server <root-dir> <unix-socket-path>
 *
 * Serves the host directory `root-dir` at namespace "/"; the guest walks
 * /apps/<mod> to fetch a published module. Runs until killed.
 * Freestanding of the kernel -- this is a normal Linux-host binary linking
 * the repo's styx runtime (styxfs_serve), exactly as test_w9pc does.
 */
#include "styxfs.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

static styxfs_server_t g_srv;

static int frame_recv(int fd, uint8_t *buf, uint32_t cap, uint32_t *len)
{
    uint8_t h[4];
    size_t got = 0;
    while (got < 4) {
        ssize_t r = read(fd, h + got, 4 - got);
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    uint32_t n = (uint32_t)h[0] | ((uint32_t)h[1] << 8) |
                ((uint32_t)h[2] << 16) | ((uint32_t)h[3] << 24);
    if (n == 0 || n > cap) return -1;
    got = 0;
    while (got < n) {
        ssize_t r = read(fd, buf + got, n - got);
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    *len = n;
    return 0;
}

static int frame_send(int fd, const uint8_t *buf, uint32_t len)
{
    uint8_t h[4];
    h[0] = (uint8_t)(len & 0xFF); h[1] = (uint8_t)((len >> 8) & 0xFF);
    h[2] = (uint8_t)((len >> 16) & 0xFF); h[3] = (uint8_t)((len >> 24) & 0xFF);
    size_t off = 0;
    while (off < 4) { ssize_t r = write(fd, h + off, 4 - off); if (r <= 0) return -1; off += (size_t)r; }
    off = 0;
    while (off < len) { ssize_t r = write(fd, buf + off, len - off); if (r <= 0) return -1; off += (size_t)r; }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <root-dir> <unix-socket-path>\n", argv[0]);
        return 2;
    }
    const char *root = argv[1];
    const char *path = argv[2];

    styxfs_init(&g_srv);
    if (styxfs_mount(&g_srv, "/", root, 0) != 0) {
        fprintf(stderr, "agi-9p-server: mount %s failed\n", root);
        return 1;
    }

    unlink(path);
    struct sockaddr_un sun;
    memset(&sun, 0, sizeof sun);
    sun.sun_family = AF_UNIX;
    strncpy(sun.sun_path, path, sizeof sun.sun_path - 1);
    int lfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (lfd < 0) { perror("socket"); return 1; }
    if (bind(lfd, (struct sockaddr *)&sun, sizeof sun) != 0) { perror("bind"); return 1; }
    if (listen(lfd, 1) != 0) { perror("listen"); return 1; }
    fprintf(stderr, "agi-9p-server: serving %s at %s (waiting)\n", root, path);

    for (;;) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) { perror("accept"); continue; }
        fprintf(stderr, "agi-9p-server: client connected\n");
        uint8_t *req = malloc(65536);
        uint8_t *rsp = malloc(65536);
        if (!req || !rsp) { fprintf(stderr, "agi-9p-server: OOM\n"); return 1; }
        for (;;) {
            uint32_t rlen = 0;
            if (frame_recv(cfd, req, 65536, &rlen) != 0) break;
            uint32_t olen = 0;
            int rc = styxfs_serve(&g_srv, req, rlen, rsp, &olen);
            fprintf(stderr, "serve: inlen=%u rc=%d olen=%u atype=%u\n",
                    rlen, rc, olen, rlen >= 5 ? req[4] : 255);
            if (rc != 0 || olen == 0) {
                /* still send whatever reply the server produced (even Rerror) */
                if (olen > 0) frame_send(cfd, rsp, olen);
                break;
            }
            if (frame_send(cfd, rsp, olen) != 0) break;
        }
        free(req); free(rsp);
        close(cfd);
        fprintf(stderr, "agi-9p-server: client done\n");
    }
    return 0;
}
