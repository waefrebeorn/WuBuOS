/*
 * wubu_ai_link.c -- see the header. Two functions that matter: one reads configuration, one measures a
 * connection. The measuring one delegates to wubu_mirror_probe() so this tree has exactly ONE bounded TCP
 * probe.
 */
#include "wubu_ai_link.h"
#include "wubu_mirror.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *wubu_ai_link_default_path(void)
{
    const char *env = getenv("WUBU_AI_CONF");
    if (env && *env) return env;
    static char path[256];
    const char *home = getenv("HOME");
    snprintf(path, sizeof path, "%s/.config/wubu/ai.conf", (home && *home) ? home : "/root");
    return path;
}

/* Trim leading/trailing blanks in place, return the start. */
static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = 0;
    return s;
}

/* "host:port" is not a URL; the shared probe wants one, so give it the scheme it expects. */
static void normalize_endpoint(const char *in, char *out, size_t cap)
{
    if (strstr(in, "://")) snprintf(out, cap, "%s", in);
    else snprintf(out, cap, "http://%s", in);
}

static int read_from_file(WubuAiLink *out)
{
    const char *path = wubu_ai_link_default_path();
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    char line[512];
    int found = 1;
    while (fgets(line, sizeof line, f)) {
        char *s = trim(line);
        if (!*s || *s == '#') continue;
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);
        if (strcmp(k, "endpoint") != 0 || !*v) continue;
        normalize_endpoint(v, out->endpoint, sizeof out->endpoint);
        snprintf(out->source, sizeof out->source, "config");
        found = 0;
        break;
    }
    fclose(f);
    return found;
}

int wubu_ai_link_read(WubuAiLink *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->latency_ms = -1;
    snprintf(out->source, sizeof out->source, "none");

    const char *env = getenv("WUBU_AI_ENDPOINT");
    if (env && *env) {
        normalize_endpoint(env, out->endpoint, sizeof out->endpoint);
        snprintf(out->source, sizeof out->source, "WUBU_AI_ENDPOINT");
        out->configured = true;
        return 0;
    }
    if (read_from_file(out) == 0) {
        out->configured = true;
        return 0;
    }
    return 1;   /* nothing configured: a normal answer, not a failure */
}

int wubu_ai_link_probe(WubuAiLink *out, int timeout_ms)
{
    if (!out) return -1;
    if (wubu_ai_link_read(out) != 0) return 1;      /* unconfigured: nothing to measure */
    if (timeout_ms <= 0) timeout_ms = WUBU_AI_DEFAULT_MS;

    /* The endpoint is just a TCP question at this layer -- "is something listening" -- and the shared
     * probe already answers exactly that with a deadline. Model identity and protocol correctness are not
     * claimed here, and the page says so. */
    WubuMirror probe;
    memset(&probe, 0, sizeof probe);
    snprintf(probe.url, sizeof probe.url, "%s", out->endpoint);
    if (wubu_mirror_probe(&probe, timeout_ms) != 0) {
        out->reachable = false;
        out->latency_ms = -1;
        return 1;
    }
    out->reachable = true;
    out->latency_ms = probe.latency_ms;
    return 0;
}

int wubu_ai_link_save(const char *endpoint)
{
    const char *path = wubu_ai_link_default_path();
    if (!endpoint || !*endpoint) {
        remove(path);                       /* an empty endpoint means "none", not a blank line */
        return 2;
    }
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    fprintf(f, "# wubu AI endpoint (read by the Control Panel AI page)\nendpoint = %s\n", endpoint);
    if (fclose(f) != 0) { remove(tmp); return -1; }
    if (rename(tmp, path) != 0) { remove(tmp); return -1; }
    return 0;
}

int wubu_ai_link_line(const WubuAiLink *l, char *out, size_t cap)
{
    if (!l || !out || cap == 0) return -1;
    if (!l->configured) {
        snprintf(out, cap, "no AI endpoint configured");
        return 0;
    }
    if (l->latency_ms < 0)
        snprintf(out, cap, "%s (%s), not measured", l->endpoint, l->source);
    else if (l->reachable)
        snprintf(out, cap, "%s (%s), reachable in %d ms", l->endpoint, l->source, l->latency_ms);
    else
        snprintf(out, cap, "%s (%s), unreachable", l->endpoint, l->source);
    return 0;
}
