/*
 * wubu_mirror_test.c -- mirror parsing, ranking and the deadline.
 *
 * The probe half is tested against sockets this process owns: a listening socket (reachable) and a
 * closed port (unreachable), so the result does not depend on the network. The DEADLINE is tested
 * against TEST-NET-1 (192.0.2.1), which is specified to be unroutable -- it must come back unreachable
 * in about the timeout, not in the kernel's two minutes.
 *
 * C11.
 */
#include "wubu_mirror.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static int g_pass, g_fail;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  \u274c %s\n", msg); } \
} while (0)

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

int main(void)
{
    printf("=== Test: parsing a mirrorlist ===\n");

    const char *text =
        "# CachyOS mirrorlist\n"
        "\n"
        "Server = https://mirror.example.org/cachyos/$repo/os/$arch\n"
        "  Server   =   http://fast.mirror.example/cachyos/$repo/os/$arch   \n"
        "Server=ftp://not-probed.example/cachyos\n"
        "this line is not a server\n"
        "Server =\n"
        "Server = https://last.example/cachyos/$repo/os/$arch\n";

    WubuMirrorList l;
    int n = wubu_mirror_parse(text, &l);
    CHECK(n == 4, "four Server lines are kept");
    CHECK(strcmp(l.m[0].url, "https://mirror.example.org/cachyos/$repo/os/$arch") == 0,
          "a plain Server line parses");
    CHECK(strcmp(l.m[1].url, "http://fast.mirror.example/cachyos/$repo/os/$arch") == 0,
          "spaces around Server and = are tolerated, trailing space trimmed");
    CHECK(strcmp(l.m[2].url, "ftp://not-probed.example/cachyos") == 0,
          "an ftp mirror is kept in the list (it just cannot be probed)");
    CHECK(strcmp(l.m[3].url, "https://last.example/cachyos/$repo/os/$arch") == 0,
          "a later Server line still parses");
    CHECK(strstr(l.m[0].url, "$repo") != NULL && strstr(l.m[0].url, "$arch") != NULL,
          "$repo/$arch are preserved for the package manager");
    CHECK(!l.m[0].reachable && l.m[0].latency_ms == -1, "an unprobed mirror is not claimed reachable");

    /* An empty "Server =" is not a mirror; a list of noise parses to nothing. */
    WubuMirrorList empty;
    CHECK(wubu_mirror_parse("Server =\n#Server = http://x/\n", &empty) == 0,
          "an empty Server and a commented Server are not mirrors");

    /* Overflow is bounded, not a crash. */
    WubuMirrorList many;
    char big[8192];
    big[0] = '\0';
    for (int i = 0; i < WUBU_MIRROR_MAX + 10; i++) {
        char line[96];
        snprintf(line, sizeof line, "Server = http://m%d.example/x\n", i);
        strncat(big, line, sizeof(big) - strlen(big) - 1);
    }
    int nb = wubu_mirror_parse(big, &many);
    CHECK(nb == WUBU_MIRROR_MAX, "the list is capped at WUBU_MIRROR_MAX");

    printf("\n=== Test: ranking ===\n");
    WubuMirrorList r;
    memset(&r, 0, sizeof r);
    r.count = 4;
    snprintf(r.m[0].url, sizeof r.m[0].url, "http://slow.example/");
    r.m[0].reachable = true; r.m[0].latency_ms = 300;
    snprintf(r.m[1].url, sizeof r.m[1].url, "http://dead.example/");
    r.m[1].reachable = false; r.m[1].latency_ms = -1;
    snprintf(r.m[2].url, sizeof r.m[2].url, "http://fast.example/");
    r.m[2].reachable = true; r.m[2].latency_ms = 12;
    snprintf(r.m[3].url, sizeof r.m[3].url, "http://tie.example/");
    r.m[3].reachable = true; r.m[3].latency_ms = 12;

    wubu_mirror_sort(&r);
    CHECK(strcmp(r.m[0].url, "http://fast.example/") == 0, "the fastest mirror sorts first");
    CHECK(strcmp(r.m[1].url, "http://tie.example/") == 0, "a tie keeps file order (stable)");
    CHECK(strcmp(r.m[2].url, "http://slow.example/") == 0, "the slower mirror follows");
    CHECK(strcmp(r.m[3].url, "http://dead.example/") == 0, "an unreachable mirror sinks to the bottom");

    printf("\n=== Test: URL splitting ===\n");
    char host[128];
    int port = 0;
    CHECK(wubu_mirror_split_url("https://mirror.example.org/cachyos/$repo/os/$arch", host, sizeof host, &port)
          && strcmp(host, "mirror.example.org") == 0 && port == 443,
          "https defaults to port 443");
    CHECK(wubu_mirror_split_url("http://example.net:8080/x", host, sizeof host, &port)
          && strcmp(host, "example.net") == 0 && port == 8080,
          "an explicit port is honoured");
    CHECK(!wubu_mirror_split_url("ftp://example.net/x", host, sizeof host, &port),
          "ftp is not probed (no false reachability)");

    printf("\n=== Test: the probe answers, and the deadline holds ===\n");
    /* A listening socket this process owns: the probe must reach it. */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sa.sin_port = 0;
    int bound = bind(srv, (struct sockaddr *)&sa, sizeof sa);
    socklen_t sl = sizeof sa;
    int live_port = 0;
    if (bound == 0 && listen(srv, 4) == 0 && getsockname(srv, (struct sockaddr *)&sa, &sl) == 0)
        live_port = ntohs(sa.sin_port);
    CHECK(live_port > 0, "a local listener is available for the probe test");

    if (live_port > 0) {
        WubuMirror live;
        memset(&live, 0, sizeof live);
        snprintf(live.url, sizeof live.url, "http://127.0.0.1:%d/x", live_port);
        CHECK(wubu_mirror_probe(&live, 500) == 0, "a listening local mirror probes reachable");
        CHECK(live.reachable && live.latency_ms >= 0, "the probe records a latency");
        if (live.latency_ms >= 0)
            printf("  measured loopback latency: %d ms\n", live.latency_ms);
    }
    close(srv);

    /* A port nobody listens on: refused quickly, and reported unreachable. */
    WubuMirror closed;
    memset(&closed, 0, sizeof closed);
    snprintf(closed.url, sizeof closed.url, "http://127.0.0.1:1/x");
    CHECK(wubu_mirror_probe(&closed, 500) != 0, "a closed port is not reachable");
    CHECK(!closed.reachable, "the closed port is not claimed reachable");

    /* TEST-NET-1 black-holes: the deadline must decide, not the kernel. */
    WubuMirror blackhole;
    memset(&blackhole, 0, sizeof blackhole);
    snprintf(blackhole.url, sizeof blackhole.url, "http://192.0.2.1:80/x");
    long t0 = now_ms();
    int rc = wubu_mirror_probe(&blackhole, 300);
    long elapsed = now_ms() - t0;
    CHECK(rc != 0, "an unroutable mirror is not reachable");
    CHECK(elapsed < 1500, "the probe honours its deadline (did not wait on the kernel)");
    printf("  measured deadline: rc=%d elapsed=%ld ms for a 300 ms budget\n", rc, elapsed);

    printf("\n=== Test: writing the ranked list ===\n");
    char path[] = "/tmp/mirrortestXXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0, "a temp mirrorlist can be created");
    if (fd >= 0) {
        close(fd);
        CHECK(wubu_mirror_write(path, &r) == 0, "the ranked list writes");
        WubuMirrorList back;
        int nb2 = wubu_mirror_read(path, &back);
        CHECK(nb2 == 3, "only reachable mirrors are written back");
        CHECK(nb2 > 0 && strcmp(back.m[0].url, "http://fast.example/") == 0,
              "the written file is in ranked order");
        CHECK(strstr(back.count ? back.m[0].url : "", "dead") == NULL,
              "the dead mirror is not handed back");
        unlink(path);
    }
    {
        WubuMirrorList missing;
        CHECK(wubu_mirror_read("/nonexistent/mirrorlist", &missing) == -1,
              "a missing mirrorlist reports -1 rather than an empty success");
    }

    printf("\n  mirror: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
