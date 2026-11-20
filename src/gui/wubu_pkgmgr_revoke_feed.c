/*
 * wubu_pkgmgr_revoke_feed.c -- strict, bounded revocation-feed parser leaf.
 *
 * This file owns the feed GRAMMAR only.  No crypto and no DB access live
 * here: the policy owner (wubu_pkgmgr.c) verifies the admin signature over
 * the exact feed bytes and applies the resulting entries atomically.
 *
 * Grammar (NDJSON): the file is a sequence of newline-delimited lines.  A
 * blank line or a line whose first non-whitespace byte is '#' is ignored.
 * Every other line must be one JSON object with EXACTLY two string members,
 * in either order:
 *
 *     {"op":"revoke","key":"<64 lowercase hex chars>"}
 *     {"op":"unrevoke","key":"<64 lowercase hex chars>"}
 *
 * Whitespace is allowed between JSON tokens.  Anything else rejects the
 * WHOLE feed: unknown/missing/duplicate members, non-string values, string
 * escapes or control/non-ASCII bytes, trailing garbage, a malformed op, a
 * bad key hex, a repeated key (duplicate or conflicting revoke/unrevoke),
 * more than WUBU_PKGMGR_FEED_MAX_ENTRIES entries, or an empty feed.
 */
#include "wubu_pkgmgr_revoke_feed.h"
#include "wubu_pkgmgr.h"

#include <stdio.h>
#include <string.h>

static void feed_seterr(char *err, size_t errsz, const char *msg) {
    if (err && errsz) {
        strncpy(err, msg, errsz - 1);
        err[errsz - 1] = '\0';
    }
}

static bool key_hex_valid(const char *hex) {
    if (!hex || strlen(hex) != 64) return false;
    for (size_t i = 0; i < 64; i++) {
        char c = hex[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

static void skip_ws(const char **p) {
    while (**p == ' ' || **p == '\t') (*p)++;
}

static bool eat(const char **p, char c) {
    if (**p == c) { (*p)++; return true; }
    return false;
}

/* Parse a plain JSON string (no escapes/control/high bytes) into out.
 * Our vocabulary is fixed, so escapes are never needed and are rejected. */
static bool parse_plain_string(const char **p, char *out, size_t outsz) {
    if (**p != '"') return false;
    (*p)++;
    size_t n = 0;
    while (**p && **p != '"') {
        unsigned char c = (unsigned char)**p;
        if (c < 0x20 || c == '\\' || c >= 0x7f) return false;
        if (n + 1 >= outsz) return false;
        out[n++] = (char)c;
        (*p)++;
    }
    if (**p != '"') return false;
    (*p)++;
    out[n] = '\0';
    return true;
}

/* Parse one line into one entry.  Exactly two members: op and key. */
static bool parse_entry(const char *line, wubu_revoke_feed_entry_t *e) {
    const char *p = line;
    bool have_op = false, have_key = false;

    skip_ws(&p);
    if (!eat(&p, '{')) return false;
    for (int field = 0; field < 2; field++) {
        char name[16];
        skip_ws(&p);
        if (!parse_plain_string(&p, name, sizeof(name))) return false;
        skip_ws(&p);
        if (!eat(&p, ':')) return false;
        skip_ws(&p);
        if (strcmp(name, "op") == 0) {
            char op[16];
            if (have_op || !parse_plain_string(&p, op, sizeof(op))) return false;
            if (strcmp(op, "revoke") == 0)      e->op = WUBU_REVOKE_FEED_REVOKE;
            else if (strcmp(op, "unrevoke") == 0) e->op = WUBU_REVOKE_FEED_UNREVOKE;
            else return false;
            have_op = true;
        } else if (strcmp(name, "key") == 0) {
            if (have_key || !parse_plain_string(&p, e->key, sizeof(e->key))) return false;
            have_key = true;
        } else {
            return false;   /* unknown member */
        }
        skip_ws(&p);
        if (field == 0) {
            if (!eat(&p, ',')) return false;
        } else {
            if (!eat(&p, '}')) return false;
        }
    }
    skip_ws(&p);
    if (*p != '\0') return false;   /* trailing garbage */
    if (!have_op || !have_key) return false;
    return key_hex_valid(e->key);
}

bool wubu_pkg_revoke_feed_parse(const uint8_t *data, size_t len,
                                wubu_revoke_feed_entry_t *out,
                                size_t max_entries, size_t *out_n,
                                char *err, size_t errsz) {
    feed_seterr(err, errsz, "");
    if (!out || !out_n || !data) {
        feed_seterr(err, errsz, "bad parser arguments");
        return false;
    }
    *out_n = 0;
    if (len == 0) { feed_seterr(err, errsz, "empty feed"); return false; }
    if (len > WUBU_PKGMGR_FEED_MAX_BYTES) {
        feed_seterr(err, errsz, "feed exceeds byte limit");
        return false;
    }

    size_t n = 0;
    const char *cur = (const char *)data;
    const char *end = cur + len;
    while (cur < end) {
        const char *nl = memchr(cur, '\n', (size_t)(end - cur));
        const char *line_end = nl ? nl : end;
        size_t line_len = (size_t)(line_end - cur);
        if (line_len && cur[line_len - 1] == '\r') line_len--;   /* CRLF */

        /* Reject NUL / control (except TAB) / non-ASCII bytes anywhere. */
        for (size_t i = 0; i < line_len; i++) {
            unsigned char c = (unsigned char)cur[i];
            if (c == 0 || (c < 0x20 && c != '\t') || c >= 0x7f) {
                feed_seterr(err, errsz, "illegal byte in feed");
                return false;
            }
        }

        char line[512];
        if (line_len >= sizeof(line)) {
            feed_seterr(err, errsz, "feed line too long");
            return false;
        }
        memcpy(line, cur, line_len);
        line[line_len] = '\0';

        const char *t = line;
        skip_ws(&t);
        if (*t && *t != '#') {
            wubu_revoke_feed_entry_t e;
            if (!parse_entry(t, &e)) {
                feed_seterr(err, errsz, "malformed feed entry");
                return false;
            }
            /* One key may appear at most once: rejects both a duplicate and
             * a conflicting revoke/unrevoke pair for the same key. */
            for (size_t i = 0; i < n; i++) {
                if (strcmp(out[i].key, e.key) == 0) {
                    feed_seterr(err, errsz, "duplicate or conflicting key in feed");
                    return false;
                }
            }
            if (n >= max_entries) {
                feed_seterr(err, errsz, "feed has too many entries");
                return false;
            }
            out[n++] = e;
        }
        if (!nl) break;
        cur = nl + 1;
    }

    if (n == 0) { feed_seterr(err, errsz, "feed has no entries"); return false; }
    *out_n = n;
    return true;
}
