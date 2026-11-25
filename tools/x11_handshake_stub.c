/* x11_handshake_stub.c -- a socket that speaks ONLY the connection setup.
 *
 * Purpose: prove that a REAL X client accepts the bytes we emit. It listens on the
 * unix socket a client dials, reads the client's setup block, answers with our
 * Success reply, then reads whatever the client sends next and exits. A client
 * that accepts the reply moves on to its first request; one that rejects it prints
 * "cannot open display" and never gets that far. The difference is the whole
 * oracle -- this stub does not pretend to be a server. */
#include "wubu_x11_bridge.h"
#include "wubu_x11_setup_reply.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s /path/to/socket\n", argv[0]); return 2; }
    unlink(argv[1]);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 2; }
    struct sockaddr_un a; memset(&a, 0, sizeof a);
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof a.sun_path, "%s", argv[1]);
    if (bind(fd, (struct sockaddr *)&a, sizeof a) < 0) { perror("bind"); return 2; }
    if (listen(fd, 1) < 0) { perror("listen"); return 2; }
    printf("READY\n"); fflush(stdout);

    int c = accept(fd, NULL, NULL);
    if (c < 0) { perror("accept"); return 2; }

    uint8_t in[4096];
    size_t got = 0;
    /* the setup block's size is only known after its first 12 bytes */
    while (got < WUBU_X11_SETUP_FIXED) {
        ssize_t n = read(c, in + got, sizeof in - got);
        if (n <= 0) { printf("FAIL client closed before setup\n"); return 1; }
        got += (size_t)n;
    }
    /* The block's size is not known until its lengths are read, and the lengths
     * are in the CLIENT's byte order -- so compute the needed total from the first
     * 12 bytes before parsing anything, and only then parse the whole block. */
    int lsb = (in[0] == 'l');
    uint16_t name_len  = lsb ? (uint16_t)(in[6] | (in[7] << 8))
                             : (uint16_t)((in[6] << 8) | in[7]);
    uint16_t data_len  = lsb ? (uint16_t)(in[8] | (in[9] << 8))
                             : (uint16_t)((in[8] << 8) | in[9]);
    size_t need = WUBU_X11_SETUP_FIXED + ((name_len + 3u) & ~3u) + ((data_len + 3u) & ~3u);
    while (got < need) {
        ssize_t n = read(c, in + got, sizeof in - got);
        if (n <= 0) { printf("FAIL client closed mid-setup\n"); return 1; }
        got += (size_t)n;
    }
    WubuX11Setup s;
    if (wubu_x11_parse_setup(in, got, &s) != 0) { printf("FAIL setup unparsable\n"); return 1; }
    printf("SETUP order=%s version=%u.%u auth=\"%s\"\n",
           s.lsb_first ? "lsb" : "msb", s.major, s.minor, s.auth_name);

    /* resource ids MUST be base | (n & mask): 0x00400000 root, visual and colormap */
    WubuX11ScreenCfg cfg = { 1024, 768, 271, 203, 24, 0x00400001, 0x00400021, 0x00400020 };
    uint8_t out[WUBU_X11_REPLY_MAX];
    int n = wubu_x11_build_setup_reply(out, sizeof out, &cfg, s.lsb_first);
    if (n <= 0) { printf("FAIL reply encode %d\n", n); return 1; }
    if (write(c, out, (size_t)n) != n) { printf("FAIL short write\n"); return 1; }
    printf("REPLY sent %d bytes\n", n); fflush(stdout);

    /* If the client accepted the reply it now sends its first request. Reading one
     * byte proves it got that far; the stub then closes, and the client reports a
     * broken connection -- NOT "cannot open display". */
    uint8_t one;
    ssize_t r = read(c, &one, 1);
    printf(r > 0 ? "PASS client sent a request (byte %u) after accepting the reply\n"
                 : "PARTIAL client closed after setup\n", (unsigned)one);
    close(c); close(fd); unlink(argv[1]);
    return r > 0 ? 0 : 1;
}
