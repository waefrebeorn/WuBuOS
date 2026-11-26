/*
 * wubu_ai_link.h -- is there a brain to talk to?
 *
 * The AIX surface (Windows 11's "AI actions") needs one honest answer before anything else: does this
 * machine have a model endpoint configured, and is it reachable? Without that, an "AI" panel is decoration
 * that either fabricates an answer or reports a green light nobody measured.
 *
 * Configuration, in order of precedence:
 *   $WUBU_AI_ENDPOINT        e.g. http://127.0.0.1:8080/v1   or  host:port
 *   $WUBU_AI_CONF, else ~/.config/wubu/ai.conf     ("endpoint = <url>", "# comments")
 *
 * The probe REUSES wubu_mirror_probe(): one bounded non-blocking connect plus poll() with a deadline, so a
 * black-holed endpoint costs exactly timeout_ms instead of the kernel's ~2 minutes. There is no second
 * TCP probe in this tree and there should not be.
 */
#ifndef WUBU_AI_LINK_H
#define WUBU_AI_LINK_H

#include <stdbool.h>
#include <stddef.h>

#define WUBU_AI_ENDPOINT_MAX 256
#define WUBU_AI_DEFAULT_MS   800

typedef struct {
    bool configured;                        /* an endpoint was found at all */
    char endpoint[WUBU_AI_ENDPOINT_MAX];
    char source[32];                        /* "WUBU_AI_ENDPOINT" | "config" | "none" */
    bool reachable;                         /* measured, never assumed */
    int  latency_ms;                        /* -1 = not measured */
} WubuAiLink;

/* The config path in use: $WUBU_AI_CONF, else ~/.config/wubu/ai.conf. Never cached. */
const char *wubu_ai_link_default_path(void);

/* Read the configuration only -- no network, no delay. Returns 0 when an endpoint was found, 1 when none
 * is configured, -1 on bad arguments. */
int wubu_ai_link_read(WubuAiLink *out);

/* Read + probe within a deadline. timeout_ms <= 0 means WUBU_AI_DEFAULT_MS. Returns 0 when the endpoint is
 * reachable, 1 when unreachable or unconfigured (a normal state, not an error), -1 on bad arguments. */
int wubu_ai_link_probe(WubuAiLink *out, int timeout_ms);

/* Write "endpoint = <url>" to the config path, atomically (temp + rename). An empty endpoint REMOVES the
 * configuration rather than writing a blank one, so "no brain" is representable. Returns 0 ok, 2 empty
 * (removed), -1 I/O. */
int wubu_ai_link_save(const char *endpoint);

/* One line for a page or a log: what is configured and what was measured. */
int wubu_ai_link_line(const WubuAiLink *l, char *out, size_t cap);

#endif
