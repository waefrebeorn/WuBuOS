/* styx_socket_test.c -- 9P over a socketpair, one request at a time. */
#include "styx_socket.h"
#include "styxfs.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
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

    T("a bad size prefix is refused");
    uint8_t bad[4] = {0, 0, 0, 2};
    if (write(sv[0], bad, 4) == 4 && styx_serve_one(sv[1], &srv.base) < 0) OK();
    else NO("bad framing accepted");

    close(sv[0]); close(sv[1]);
    printf("\n=== styx_socket: %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
