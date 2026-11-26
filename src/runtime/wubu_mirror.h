/*
 * wubu_mirror.h -- package mirror ranking (reflector parity).
 *
 * CachyOS ships reflector: it fetches a mirror list, measures how fast each mirror answers, and writes
 * the list back sorted. WuBuOS had the list (pacman applet) but never ranked it, so the first mirror
 * in the file was the one used whether or not it could be reached.
 *
 * The ranking is a pure function over (mirror, measured latency): unreachable mirrors sink, then
 * ascending latency, ties keep their file order. Measurement is one TCP connect with a deadline --
 * no shell, no curl, no allocation in the probe loop.
 *
 * C11, no libm.
 */
#ifndef WUBU_MIRROR_H
#define WUBU_MIRROR_H

#include <stdbool.h>
#include <stddef.h>

#define WUBU_MIRROR_MAX        64
#define WUBU_MIRROR_URL_MAX    256
#define WUBU_MIRROR_DEFAULT_MS 1500   /* per-mirror deadline; a rank run stays bounded */

typedef struct {
    char url[WUBU_MIRROR_URL_MAX];
    int  latency_ms;      /* -1 = not measured / unreachable */
    bool reachable;
} WubuMirror;

typedef struct {
    WubuMirror m[WUBU_MIRROR_MAX];
    int        count;
} WubuMirrorList;

/* The mirrorlist in use: $WUBU_MIRRORLIST, else /etc/pacman.d/mirrorlist. */
const char *wubu_mirror_default_path(void);

/* Parse "Server = <url>" lines. Comments, blanks and unrelated lines are skipped; "$repo/os/$arch" is
 * preserved verbatim (the package manager expands it). Returns the number of mirrors kept. */
int wubu_mirror_parse(const char *text, WubuMirrorList *out);

/* Read + parse a mirrorlist file. Returns count, or -1 when the file cannot be read. */
int wubu_mirror_read(const char *path, WubuMirrorList *out);

/* Rank in place: reachable first by ascending latency, unreachable last, stable otherwise.
 * Mirrors with latency -1 are treated as unmeasured-but-reachable only if reachable is set. */
void wubu_mirror_sort(WubuMirrorList *list);

/* One TCP connect to the mirror's host:port with a deadline in ms. Fills latency_ms/reachable.
 * Returns 0 when the connect completed (reachable), -1 otherwise. Never blocks past the deadline. */
int wubu_mirror_probe(WubuMirror *m, int timeout_ms);

/* Probe every mirror (each bounded by timeout_ms), then sort. Returns the reachable count. */
int wubu_mirror_rank(WubuMirrorList *list, int timeout_ms);

/* Write "Server = <url>" lines, best first. Returns 0 on success, -1 when the file cannot be written. */
int wubu_mirror_write(const char *path, const WubuMirrorList *list);

/* host/port out of an URL (http=80, https=443). Exposed for the gate. */
bool wubu_mirror_split_url(const char *url, char *host, size_t host_cap, int *port);

#endif
