/* wubu_agent_log.c -- see wubu_agent_log.h. One line per record, chained with BLAKE2b. */
#include "wubu_agent_log.h"
#include "wubu_blake2b.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void hex(const uint8_t *d, size_t n, char *out) {
    static const char *H = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) { out[i*2] = H[d[i] >> 4]; out[i*2+1] = H[d[i] & 15]; }
    out[n*2] = '\0';
}

static int unhex(const char *h, uint8_t *out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned v = 0;
        for (int k = 0; k < 2; k++) {
            char c = h[i*2+k];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else return -1;
        }
        out[i] = (uint8_t)v;
    }
    return 0;
}

/* The hash covers the record's own fields and the previous hash, so a changed field and
 * a removed record are the same kind of detectable event. */
static void record_hash(const char *head, const uint8_t prev[WUBU_AGENT_HASH], uint8_t out[WUBU_AGENT_HASH]) {
    uint8_t buf[512];
    size_t n = strlen(head);
    if (n > sizeof buf) n = sizeof buf;
    memcpy(buf, head, n);
    memcpy(buf + n, prev, WUBU_AGENT_HASH);
    wubu_blake2b(out, WUBU_AGENT_HASH, buf, n + WUBU_AGENT_HASH, NULL, 0);
}

static unsigned long long now_ms(void) {
    return (unsigned long long)time(NULL) * 1000ull;
}

static int fields_ok(const char *action, const char *verdict, const char *detail) {
    if (!action || !verdict) return 0;
    if (strlen(action) >= WUBU_AGENT_ACTION_MAX) return 0;
    if (strlen(verdict) >= WUBU_AGENT_VERDICT_MAX) return 0;
    if (detail && strlen(detail) >= WUBU_AGENT_DETAIL_MAX) return 0;
    if (strchr(action, '|') || strchr(action, '\n')) return 0;
    if (strchr(verdict, '|') || strchr(verdict, '\n')) return 0;
    if (detail && (strchr(detail, '|') || strchr(detail, '\n'))) return 0;
    return 1;
}

/* Split one line into exactly seven fields. Kept separate because both verify and the
 * resume path need the same notion of a well-formed record. */
static int split_record(char *line, char *f[7]) {
    int n = 0;
    char *p = line;
    while (n < 7) {
        f[n++] = p;
        char *bar = strchr(p, '|');
        if (!bar) break;
        *bar = '\0';
        p = bar + 1;
    }
    return n == 7 ? 0 : -1;   /* the seventh field runs to end of line */
}

int wubu_agent_log_verify(const char *path, WubuAgentVerdict *out, int *err) {
    if (err) *err = WUBU_AGENT_LOG_OK;
    if (!path || !out) { if (err) *err = WUBU_AGENT_LOG_ARGS; return -1; }
    out->count = 0; out->bad_index = -1; out->gap = 0;
    FILE *f = fopen(path, "r");
    if (!f) { if (err) *err = WUBU_AGENT_LOG_IO; return -1; }
    uint8_t prev[WUBU_AGENT_HASH];
    memset(prev, 0, sizeof prev);
    char line[1024];
    int expect_seq = 0;
    int rc = 0;
    while (fgets(line, sizeof line, f)) {
        size_t len = strlen(line);
        while (len && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;
        char *fld[7];
        if (split_record(line, fld)) { out->bad_index = out->count; if (err) *err = WUBU_AGENT_LOG_FORMAT; rc = -1; break; }
        int seq = atoi(fld[0]);
        if (seq != expect_seq) { out->gap = 1; }
        uint8_t want[WUBU_AGENT_HASH], got[WUBU_AGENT_HASH];
        if (unhex(fld[5], want, WUBU_AGENT_HASH) || unhex(fld[6], got, WUBU_AGENT_HASH)) {
            out->bad_index = out->count; if (err) *err = WUBU_AGENT_LOG_FORMAT; rc = -1; break;
        }
        if (memcmp(want, prev, WUBU_AGENT_HASH) != 0) {
            out->bad_index = out->count; if (err) *err = WUBU_AGENT_LOG_BROKEN; rc = -1; break;
        }
        char head[640];
        snprintf(head, sizeof head, "%s|%s|%s|%s|%s|", fld[0], fld[1], fld[2], fld[3], fld[4]);
        uint8_t calc[WUBU_AGENT_HASH];
        record_hash(head, prev, calc);
        if (memcmp(calc, got, WUBU_AGENT_HASH) != 0) {
            out->bad_index = out->count; if (err) *err = WUBU_AGENT_LOG_BROKEN; rc = -1; break;
        }
        memcpy(prev, got, WUBU_AGENT_HASH);
        expect_seq = seq + 1;
        out->count++;
    }
    fclose(f);
    if (!rc && out->gap) { if (err) *err = WUBU_AGENT_LOG_RESUME_SEQ; rc = -1; }
    return rc;
}

int wubu_agent_log_open(WubuAgentLog *log, const char *path, int *err) {
    if (err) *err = WUBU_AGENT_LOG_OK;
    if (!log || !path) { if (err) *err = WUBU_AGENT_LOG_ARGS; return -1; }
    if (strlen(path) >= sizeof log->path) { if (err) *err = WUBU_AGENT_LOG_ARGS; return -1; }
    strcpy(log->path, path);
    memset(log->prev, 0, sizeof log->prev);
    log->next_seq = 0;

    FILE *probe = fopen(path, "r");
    if (!probe) return 0;                 /* nothing to resume: a new log starts at seq 0 */
    fclose(probe);

    WubuAgentVerdict v;
    int e = 0;
    if (wubu_agent_log_verify(path, &v, &e)) {
        /* A broken chain is not a log we may append to: appending would produce a file
         * whose damage the chain can no longer point at. */
        if (err) *err = e;
        return -1;
    }
    /* Re-derive the tail hash and the next sequence by reading the last line. */
    FILE *f = fopen(path, "r");
    if (!f) { if (err) *err = WUBU_AGENT_LOG_IO; return -1; }
    char line[768], last[768] = {0};
    while (fgets(line, sizeof line, f)) {
        size_t len = strlen(line);
        if (len > 1) memcpy(last, line, len + 1);
    }
    fclose(f);
    if (last[0]) {
        char *fld[7];
        if (split_record(last, fld)) { if (err) *err = WUBU_AGENT_LOG_FORMAT; return -1; }
        if (unhex(fld[6], log->prev, WUBU_AGENT_HASH)) { if (err) *err = WUBU_AGENT_LOG_FORMAT; return -1; }
        log->next_seq = v.count;
    }
    return 0;
}

int wubu_agent_log_append(WubuAgentLog *log, const char *action, const char *verdict,
                          const char *detail, int *err) {
    if (err) *err = WUBU_AGENT_LOG_OK;
    if (!log || !action || !verdict) { if (err) *err = WUBU_AGENT_LOG_ARGS; return -1; }
    if (!fields_ok(action, verdict, detail)) { if (err) *err = WUBU_AGENT_LOG_FULL; return -1; }
    char head[512], prev_hex[2 * WUBU_AGENT_HASH + 1], hash_hex[2 * WUBU_AGENT_HASH + 1];
    uint8_t hash[WUBU_AGENT_HASH];
    snprintf(head, sizeof head, "%d|%llu|%s|%s|%s|",
             log->next_seq, now_ms(), action, verdict, detail ? detail : "");
    hex(log->prev, WUBU_AGENT_HASH, prev_hex);
    record_hash(head, log->prev, hash);
    hex(hash, WUBU_AGENT_HASH, hash_hex);
    FILE *f = fopen(log->path, "a");
    if (!f) { if (err) *err = WUBU_AGENT_LOG_IO; return -1; }
    int ok = fprintf(f, "%s%s|%s\n", head, prev_hex, hash_hex) > 0;
    if (fclose(f) != 0) ok = 0;
    if (!ok) { if (err) *err = WUBU_AGENT_LOG_IO; return -1; }
    memcpy(log->prev, hash, sizeof hash);
    log->next_seq++;
    return 0;
}

const char *wubu_agent_log_reason(int err) {
    switch (err) {
        case WUBU_AGENT_LOG_OK:         return "ok";
        case WUBU_AGENT_LOG_ARGS:       return "bad arguments";
        case WUBU_AGENT_LOG_IO:         return "cannot read or write the log";
        case WUBU_AGENT_LOG_BROKEN:     return "the chain does not verify: a record was changed or removed";
        case WUBU_AGENT_LOG_FORMAT:     return "a line is not a verdict record";
        case WUBU_AGENT_LOG_FULL:       return "a field is too long or contains a separator";
        case WUBU_AGENT_LOG_RESUME_SEQ: return "the log has a gap in its sequence";
        default:                        return "invalid";
    }
}
