/* styx_socket_test.c -- 9P over a socketpair, one request at a time. */
#include "styx_socket.h"
#include "styxfs.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

static int pass, fail;
#define T(n) do { printf("  TEST %-52s", n); } while (0)
#define OK() do { printf("✅\n"); pass++; } while (0)
#define NO(m) do { printf("❌ %s\n", m); fail++; } while (0)

static int rq(int cfd, int sfd, styx_server_t *srv,
              const uint8_t *msg, uint32_t len, uint8_t *rep, uint32_t *rlen)
{
    uint8_t hdr[4];
    if (write(cfd, msg, len) != (int)len) { printf("[w]"); return -1; }
    int s1 = styx_serve_one(sfd, srv);
    if (s1 <= 0) { printf("[serve=%d] ", s1); return -1; }
    int r1 = (int)read(cfd, hdr, 4);
    if (r1 != 4) { printf("[rd=%d] ", r1); return -1; }
    uint32_t sz = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) |
                  ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
    if (sz < 7 || sz > STYX_MAX_MSG) return -1;
    memcpy(rep, hdr, 4);
    if (read(cfd, rep + 4, sz - 4) != (int)(sz - 4)) return -1;
    *rlen = sz;
    return 0;
}

int main(void)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { printf("socketpair failed\n"); return 1; }

    styxfs_server_t srv;
    styxfs_init(&srv);

    uint8_t req[STYX_MAX_MSG], rep[STYX_MAX_MSG];
    uint32_t len = 0, rlen = 0;

    T("Tversion over the socket returns Rversion");
    styx_build_tversion(req, &len, 8192, "9P2000");
    int rc = rq(sv[0], sv[1], &srv.base, req, len, rep, &rlen);
    printf("[rc=%d type=0x%02x len=%u] ", rc, rep[4], rlen);
    if (rc == 0 && rep[4] == STX_RVERSION) OK(); else NO("version exchange failed");

    T("Tattach over the socket returns Rattach");
    styx_build_tattach(req, &len, 1, 1, 0xFFFFFFFFu, "/");
    rc = rq(sv[0], sv[1], &srv.base, req, len, rep, &rlen);
    printf("[rc=%d type=0x%02x] ", rc, rep[4]);
    if (rc == 0 && rep[4] == STX_RATTACH) OK(); else NO("attach failed");

    T("mount + walk + open + read serves a file over the socket");
    {
        const char *dir = "/home/wubu/.hermes/profiles/mind-palace/cache/scratch/nkern";
        mkdir(dir, 0755);
        char fp[512];
        snprintf(fp, sizeof fp, "%s/scheduler", dir);
        const char *content = "sched: 3 runnable, 0 blocked\n";
        FILE *f = fopen(fp, "wb");
        if (f) { fwrite(content, 1, strlen(content), f); fclose(f); }

        styxfs_mount(&srv, "/n/kernel", (char *)dir, 0);

        styx_build_tattach(req, &len, 2, 10, 0xFFFFFFFFu, "/n/kernel");
        int r1 = rq(sv[0], sv[1], &srv.base, req, len, rep, &rlen);
        int t1 = (r1 == 0 && rep[4] == STX_RATTACH);

        const char *wn[1] = {"scheduler"};
        styx_build_twalk(req, &len, 3, 10, 11, wn, 1);
        int r2 = rq(sv[0], sv[1], &srv.base, req, len, rep, &rlen);
        int t2 = (r2 == 0 && rep[4] == STX_RWALK);

        styx_build_topen(req, &len, 4, 11, 0);
        int r3 = rq(sv[0], sv[1], &srv.base, req, len, rep, &rlen);
        int t3 = (r3 == 0 && rep[4] == STX_ROPEN);

        styx_build_tread(req, &len, 5, 11, 0, 64);
        int r4 = rq(sv[0], sv[1], &srv.base, req, len, rep, &rlen);
        int got = 0;
        if (r4 == 0 && rep[4] == STX_RREAD && rlen > 15) {
            uint32_t n = (uint32_t)rep[7] | ((uint32_t)rep[8] << 8) |
                         ((uint32_t)rep[9] << 16) | ((uint32_t)rep[10] << 24);
            if (n == strlen(content) && memcmp(rep + 11, content, n) == 0) got = 1;
        }
        printf("[attach=%d walk=%d open=%d read=%d] ", t1, t2, t3, got);
        if (t1 && t2 && t3 && got) OK(); else NO("mount/read sequence failed");
    }

    T("a file larger than msize pages across multiple Rreads (iounit)");
    {
        /* FS #446: a 20 KiB file over an 8192-byte msize cannot arrive in
         * one Rread -- the server must cap each reply at msize-11 and the
         * client pages until an EOF count=0. */
        const char *dir = "/home/wubu/.hermes/profiles/mind-palace/cache/scratch/nkern";
        static char big[20481];
        for (int i = 0; i < 20480; i++) big[i] = (char)('a' + (i % 26));
        big[20480] = 0;
        char fp[512];
        snprintf(fp, sizeof fp, "%s/bigfile", dir);
        FILE *bf = fopen(fp, "wb");
        if (bf) { fwrite(big, 1, 20480, bf); fclose(bf); }

        styxfs_mount(&srv, "/n/big", (char *)dir, 0);
        styx_build_tattach(req, &len, 6, 20, 0xFFFFFFFFu, "/n/big");
        int a1 = rq(sv[0], sv[1], &srv.base, req, len, rep, &rlen);
        int ta = (a1 == 0 && rep[4] == STX_RATTACH);

        const char *wn2[1] = {"bigfile"};
        styx_build_twalk(req, &len, 7, 20, 21, wn2, 1);
        int w1 = rq(sv[0], sv[1], &srv.base, req, len, rep, &rlen);
        int tw = (w1 == 0 && rep[4] == STX_RWALK);

        styx_build_topen(req, &len, 8, 21, 0);
        int o1 = rq(sv[0], sv[1], &srv.base, req, len, rep, &rlen);
        int to = (o1 == 0 && rep[4] == STX_ROPEN);

        uint64_t off = 0; uint32_t total = 0; int rounds = 0, eof = 0, cap_ok = 1;
        uint32_t msize = 8192, per_cap = 0;
        /* the negotiated msize lives in the server struct */
        msize = srv.base.msize;
        per_cap = msize - 11;
        while (rounds < 16) {
            styx_build_tread(req, &len, 9, 21, off, 8192);
            int rr = rq(sv[0], sv[1], &srv.base, req, len, rep, &rlen);
            if (rr != 0 || rep[4] != STX_RREAD) break;
            uint32_t n = (uint32_t)rep[7] | ((uint32_t)rep[8] << 8) |
                         ((uint32_t)rep[9] << 16) | ((uint32_t)rep[10] << 24);
            if (n > per_cap) cap_ok = 0;          /* iounit violated */
            if (n == 0) { eof = 1; break; }
            if (memcmp(rep + 11, big + off, n) != 0) cap_ok = 0;
            total += n; off += n; rounds++;
        }
        printf("[attach=%d walk=%d open=%d rounds=%d total=%u cap=%d eof=%d] ",
               ta, tw, to, rounds, total, cap_ok, eof);
        if (ta && tw && to && rounds == 3 && total == 20480 && cap_ok && eof) OK();
        else NO("large-file paging failed");
    }

    T("a revoked fid capability is denied on the wire (EACCES)");
    {
        /* SECURITY #793/#794: attach mints the fid's capability; opening
         * works while it is live and is refused with 'permission denied'
         * after styxfs_cap_revoke_fid -- enforcement proven end to end. */
        styx_build_tattach(req, &len, 10, 30, 0xFFFFFFFFu, "/n/big");
        int ca = rq(sv[0], sv[1], &srv.base, req, len, rep, &rlen);
        const char *wn3[1] = {"bigfile"};
        styx_build_twalk(req, &len, 11, 30, 31, wn3, 1);
        int cw = rq(sv[0], sv[1], &srv.base, req, len, rep, &rlen);
        styx_build_topen(req, &len, 12, 31, 0);
        int co1 = rq(sv[0], sv[1], &srv.base, req, len, rep, &rlen);
        int open1_ok = (co1 == 0 && rep[4] == STX_ROPEN);

        int revoked = (styxfs_cap_revoke_fid(&srv, 31) == 0);
        styx_build_topen(req, &len, 13, 31, 0);
        int co2 = rq(sv[0], sv[1], &srv.base, req, len, rep, &rlen);
        int denied = 0;
        const char *etxt = "";
        if (co2 == 0 && rep[4] == STX_RERROR) {
            uint16_t esz = (uint16_t)(rep[7] | (rep[8] << 8));
            char buf[64] = {0};
            if (esz < sizeof(buf)) { memcpy(buf, rep + 9, esz); buf[esz] = 0; }
            etxt = buf;
            denied = (strcmp(buf, "permission denied") == 0);
        }
        printf("[attach=%d walk=%d open1=%d revoked=%d denied=%d ename='%s'] ",
               ca == 0, cw == 0, open1_ok, revoked, denied, etxt);
        if (ca == 0 && cw == 0 && open1_ok && revoked && denied) OK();
        else NO("capability enforcement failed on the wire");
    }

    T("a bad size prefix is refused");
    uint8_t bad[4] = {0, 0, 0, 2};
    if (write(sv[0], bad, 4) == 4 && styx_serve_one(sv[1], &srv.base) < 0) OK();
    else NO("bad framing accepted");

    close(sv[0]); close(sv[1]);
    printf("\n=== styx_socket: %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
