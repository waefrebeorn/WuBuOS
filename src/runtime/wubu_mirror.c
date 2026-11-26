/*
 * wubu_mirror.c -- see wubu_mirror.h. Parse, measure, rank.
 *
 * The probe is a non-blocking connect plus poll() with a deadline: a mirror that black-holes the SYN
 * costs exactly `timeout_ms`, not the kernel's ~2 minutes. That is the whole point of ranking -- the
 * first list entry used to be used whether or not it answered.
 *
 * C11, no libm, no shell, no allocator.
 */
#include "wubu_mirror.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

const char *wubu_mirror_default_path(void)
{
    const char *env = getenv("WUBU_MIRRORLIST");
    if (env && env[0]) return env;
    return "/etc/pacman.d/mirrorlist";
}

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n > 0) {
        char c = s[n - 1];
        if (c != '\n' && c != '\r' && c != ' ' && c != '\t') break;
        s[--n] = '\0';
    }
}

int wubu_mirror_parse(const char *text, WubuMirrorList *out)
{
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (!text) return 0;

    const char *p = text;
    while (*p && out->count < WUBU_MIRROR_MAX) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);

        char line[512];
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len);
        line[len] = '\0';
        trim(line);

        /* "Server = URL" (spaces optional around '='). Anything else is a comment or noise. */
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (strncmp(s, "Server", 6) == 0) {
            char *q = s + 6;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '=') {
                q++;
                while (*q == ' ' || *q == '\t') q++;
                if (*q) {
                    WubuMirror *m = &out->m[out->count++];
                    snprintf(m->url, sizeof(m->url), "%s", q);
                    m->latency_ms = -1;
                    m->reachable = false;
                }
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    return out->count;
}

int wubu_mirror_read(const char *path, WubuMirrorList *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (!path) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char *buf = malloc(64 * 1024);
    if (!buf) { fclose(f); return -1; }
    size_t got = fread(buf, 1, 64 * 1024 - 1, f);
    fclose(f);
    buf[got] = '\0';

    int n = wubu_mirror_parse(buf, out);
    free(buf);
    return n;
}

/* Stable insertion sort: at 64 entries it is faster than qsort and cannot reorder equal keys. */
void wubu_mirror_sort(WubuMirrorList *list)
{
    if (!list) return;

    /* Reachable (and measured) beats unreachable; then smaller latency. */
    for (int i = 1; i < list->count; i++) {
        WubuMirror key = list->m[i];
        int j = i - 1;
        while (j >= 0) {
            const WubuMirror *o = &list->m[j];
            bool o_first;
            if (o->reachable != key.reachable) {
                o_first = o->reachable;                 /* reachable sorts before unreachable */
            } else if (!o->reachable) {
                o_first = true;                         /* both unreachable: keep file order */
            } else {
                o_first = o->latency_ms <= key.latency_ms;  /* equal latency keeps file order */
            }
            if (o_first) break;
            list->m[j + 1] = list->m[j];
            j--;
        }
        list->m[j + 1] = key;
    }
}

bool wubu_mirror_split_url(const char *url, char *host, size_t host_cap, int *port)
{
    if (!url || !host || host_cap < 2) return false;
    host[0] = '\0';

    const char *p = strstr(url, "://");
    if (!p) return false;
    const char *scheme = url;
    int default_port = 0;
    if (strncmp(scheme, "https://", 8) == 0) default_port = 443;
    else if (strncmp(scheme, "http://", 7) == 0) default_port = 80;
    else return false;                      /* ftp/file: not probed */

    p += 3;
    size_t n = strcspn(p, "/:?#");
    if (n == 0 || n >= host_cap) return false;
    memcpy(host, p, n);
    host[n] = '\0';

    int prt = default_port;
    if (p[n] == ':') {
        prt = atoi(p + n + 1);
        if (prt <= 0 || prt > 65535) prt = default_port;
    }
    if (port) *port = prt;
    return true;
}

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

int wubu_mirror_probe(WubuMirror *m, int timeout_ms)
{
    if (!m) return -1;
    m->reachable = false;
    m->latency_ms = -1;
    if (timeout_ms <= 0) timeout_ms = WUBU_MIRROR_DEFAULT_MS;

    char host[128];
    int port = 0;
    if (!wubu_mirror_split_url(m->url, host, sizeof host, &port)) return -1;

    char portstr[16];
    snprintf(portstr, sizeof portstr, "%d", port);

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, portstr, &hints, &res) != 0 || !res) return -1;

    int rc = -1;
    long start = now_ms();
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        long left = timeout_ms - (now_ms() - start);
        if (left <= 0) break;

        int fd = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK, ai->ai_protocol);
        if (fd < 0) continue;

        int c = connect(fd, ai->ai_addr, ai->ai_addrlen);
        if (c == 0) {
            rc = 0;
        } else if (errno == EINPROGRESS) {
            struct pollfd pfd = { .fd = fd, .events = POLLOUT, .revents = 0 };
            int pr = poll(&pfd, 1, (int)left);
            if (pr > 0) {
                int soerr = 0;
                socklen_t sl = sizeof soerr;
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl) == 0 && soerr == 0)
                    rc = 0;
            }
        }
        close(fd);
        if (rc == 0) break;
        left = timeout_ms - (now_ms() - start);
        if (left <= 0) break;
    }
    freeaddrinfo(res);

    if (rc == 0) {
        m->reachable = true;
        m->latency_ms = (int)(now_ms() - start);
        if (m->latency_ms < 0) m->latency_ms = 0;
    }
    return rc;
}

int wubu_mirror_rank(WubuMirrorList *list, int timeout_ms)
{
    if (!list) return 0;
    int n = 0;
    for (int i = 0; i < list->count; i++)
        if (wubu_mirror_probe(&list->m[i], timeout_ms) == 0) n++;
    wubu_mirror_sort(list);
    return n;
}

int wubu_mirror_write(const char *path, const WubuMirrorList *list)
{
    if (!path || !list) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# ranked by wubu_mirror: reachable first, then fastest\n");
    for (int i = 0; i < list->count; i++) {
        if (!list->m[i].reachable) continue;     /* never hand back a mirror that did not answer */
        fprintf(f, "Server = %s\n", list->m[i].url);
    }
    if (fclose(f) != 0) return -1;
    return 0;
}
