/*
 * wubu_ai_link_test.c -- the AI link must answer three questions honestly:
 *   1. nothing configured      -> says so, and does NOT claim reachability
 *   2. configured, nothing there -> unreachable, and the probe RETURNS inside its deadline
 *   3. configured and listening  -> reachable, latency measured
 *
 * A test that only checked (3) would pass on a machine where the panel also lights up green for a
 * black-holed endpoint, which is the failure this module exists to prevent.
 */
#include "wubu_ai_link.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL %s\n", m); fails++; } \
                         else printf("  ok   %s\n", m); } while (0)

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

int main(void)
{
    char tmp[] = "/tmp/wubu_ai_conf_XXXXXX";
    int fd = mkstemp(tmp);
    CHECK(fd >= 0, "temp config created");
    if (fd < 0) return 1;
    close(fd);

    setenv("WUBU_AI_CONF", tmp, 1);
    unsetenv("WUBU_AI_ENDPOINT");

    /* 1. nothing configured */
    WubuAiLink l;
    char line[512];
    CHECK(wubu_ai_link_read(&l) == 1, "no endpoint configured is reported, not invented");
    CHECK(wubu_ai_link_line(&l, line, sizeof line) == 0 && strstr(line, "no AI endpoint") != NULL,
          "the line says there is no endpoint");
    CHECK(wubu_ai_link_probe(&l, 200) == 1 && !l.reachable,
          "an unconfigured probe never claims reachability");

    /* 2. configured to a port nothing listens on: must return within the deadline, not the kernel's ~2 min */
    FILE *f = fopen(tmp, "w");
    CHECK(f != NULL, "config is writable");
    if (f) {
        fprintf(f, "# local brain\nendpoint = http://127.0.0.1:9/v1\n");
        fclose(f);
    }
    CHECK(wubu_ai_link_read(&l) == 0 && l.configured &&
          strcmp(l.endpoint, "http://127.0.0.1:9/v1") == 0, "the endpoint is read from the config");
    CHECK(strcmp(l.source, "config") == 0, "the source of the endpoint is named");

    long t0 = now_ms();
    int rc = wubu_ai_link_probe(&l, 300);
    long dt = now_ms() - t0;
    char msg[160];
    snprintf(msg, sizeof msg, "a dead endpoint is unreachable, measured in %ld ms (deadline 300)", dt);
    CHECK(rc == 1 && !l.reachable && dt <= 900, msg);

    /* 3. a real listener on a port we open ourselves */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = 0;
    int bound = srv >= 0 && bind(srv, (struct sockaddr *)&a, sizeof a) == 0 && listen(srv, 1) == 0;
    CHECK(bound, "a listener is opened for the reachable case");
    if (bound) {
        socklen_t sl = sizeof a;
        getsockname(srv, (struct sockaddr *)&a, &sl);
        char ep[128];
        snprintf(ep, sizeof ep, "127.0.0.1:%d", ntohs(a.sin_port));
        setenv("WUBU_AI_ENDPOINT", ep, 1);
        CHECK(wubu_ai_link_read(&l) == 0 && l.configured &&
              strcmp(l.source, "WUBU_AI_ENDPOINT") == 0, "the environment overrides the config file");
        CHECK(wubu_ai_link_probe(&l, 300) == 0 && l.reachable && l.latency_ms >= 0,
              "a listening endpoint is reachable, with latency measured");
        CHECK(wubu_ai_link_line(&l, line, sizeof line) == 0 && strstr(line, "reachable in") != NULL,
              "the line reports the measured latency");
        close(srv);
    }

    unlink(tmp);
    printf("wubu_ai_link: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
