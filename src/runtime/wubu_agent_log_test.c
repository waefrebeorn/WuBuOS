/* wubu_agent_log_test.c -- the gate for D6, the agent verdict log.
 *
 * The log exists to answer "what did the agent do, and can I believe it afterwards", so
 * the gate is mostly about BELIEF: append, verify, then damage the file three ways --
 * edit a middle record, delete a middle record, truncate the tail -- and require that
 * verification points at the damage instead of passing. A tamper-evident log that
 * accepts a rewritten line is a claim, not a record.
 */
#include "wubu_agent_log.h"
#include "wubu_blake2b.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pass, fail;
static char g_path[512];

static void check(int cond, const char *what) {
    if (cond) { pass++; return; }
    printf("  [FAIL] %s\n", what);
    fail++;
}

static long slurp(char **out) {
    FILE *f = fopen(g_path, "rb");
    if (!f) { *out = NULL; return -1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    char *b = malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); *out = NULL; return -1; }
    b[n] = '\0'; fclose(f); *out = b; return n;
}

static int spit(const char *b) {
    FILE *f = fopen(g_path, "wb");
    if (!f) return -1;
    size_t n = strlen(b);
    int ok = fwrite(b, 1, n, f) == n;
    fclose(f);
    return ok ? 0 : -1;
}

int main(void) {
    printf("=== wubu_agent_log (gate D6: tamper-evident verdicts) ===\n");
    const char *tmp = getenv("TMPDIR");
    if (!tmp) tmp = "/tmp";
    snprintf(g_path, sizeof g_path, "%s/wubu_agent_log_test.txt", tmp);
    remove(g_path);

    WubuAgentLog log;
    int err = 0;
    check(wubu_agent_log_open(&log, g_path, &err) == 0, "a new log opens");
    check(wubu_agent_log_append(&log, "fs.write", "allowed", "/home/wubu/notes.txt", &err) == 0, "append 1");
    check(wubu_agent_log_append(&log, "fs.delete", "denied", "capability expired", &err) == 0, "append 2");
    check(wubu_agent_log_append(&log, "net.connect", "allowed", "example.invalid:443", &err) == 0, "append 3");

    WubuAgentVerdict v;
    check(wubu_agent_log_verify(g_path, &v, &err) == 0 && v.count == 3 && v.bad_index == -1, "three records verify");

    /* resume: a reopened log continues the chain instead of restarting it */
    check(wubu_agent_log_open(&log, g_path, &err) == 0 && log.next_seq == 3, "reopen resumes at seq 3");
    check(wubu_agent_log_append(&log, "proc.spawn", "allowed", "holyc /tmp/x.c", &err) == 0, "append 4");
    check(wubu_agent_log_verify(g_path, &v, &err) == 0 && v.count == 4, "four records verify");

    /* refusal: a field that would break the format is rejected, not escaped away */
    err = 0;
    check(wubu_agent_log_append(&log, "bad|action", "allowed", NULL, &err) != 0 &&
          err == WUBU_AGENT_LOG_FULL, "a '|' in a field is refused");
    check(wubu_agent_log_verify(g_path, &v, &err) == 0 && v.count == 4, "the refusal did not write a record");

    /* damage 1: rewrite a middle record's verdict */
    char *orig = NULL;
    long n = slurp(&orig);
    check(n > 0, "read the log back");
    char *mut = malloc((size_t)n + 1);
    memcpy(mut, orig, (size_t)n + 1);
    char *hit = strstr(mut, "|denied|");
    check(hit != NULL, "found the record to edit");
    if (hit) {
        memcpy(hit + 1, "allowed", 7);          /* denied -> allowed */
        check(spit(mut) == 0, "wrote the edited log");
        err = 0;
        int rc = wubu_agent_log_verify(g_path, &v, &err);
        check(rc != 0 && v.bad_index == 1, "an edited record is reported at its index");
    }

    /* damage 2: delete a middle line entirely */
    {
        char *start = strchr(orig, '\n');
        char *second_end = start ? strchr(start + 1, '\n') : NULL;
        if (start && second_end) {
            char *cut = malloc((size_t)n + 1);
            size_t head = (size_t)(start - orig) + 1;
            memcpy(cut, orig, head);
            strcpy(cut + head, second_end + 1);
            check(spit(cut) == 0, "wrote the log with a record removed");
            err = 0;
            int rc = wubu_agent_log_verify(g_path, &v, &err);
            check(rc != 0, "a removed record is detected");
            free(cut);
        }
    }

    /* damage 3: truncate the tail mid-record */
    {
        char *cut = malloc((size_t)n + 1);
        memcpy(cut, orig, (size_t)n - 20);
        cut[n - 20] = '\0';
        check(spit(cut) == 0, "wrote a truncated log");
        err = 0;
        int rc = wubu_agent_log_verify(g_path, &v, &err);
        check(rc != 0, "a truncated tail is detected");
        free(cut);
    }


    /* damage 4: a SELF-CONSISTENT forgery -- rewrite a middle record's verdict AND
     * recompute its own hash, exactly as an attacker with the format would. The record
     * then verifies against itself; only the next record's commitment to it can catch
     * this, which is the whole reason the log is chained. Without this case the gate
     * would pass on a "chain" that never actually commits to anything. */
    {
        /* rebuild the log from the untouched original first */
        check(spit(orig) == 0, "restored the original log");
        char *copy = malloc((size_t)n + 1);
        memcpy(copy, orig, (size_t)n + 1);
        char *line2 = strchr(copy, '\n');
        line2 = line2 ? line2 + 1 : NULL;
        char *line2_end = line2 ? strchr(line2, '\n') : NULL;
        if (line2 && line2_end) {
            *line2_end = '\0';
            char work[768];
            snprintf(work, sizeof work, "%s", line2);
            char *f[7];
            int ok = 1;
            {
                char *p = work;
                for (int k = 0; k < 7; k++) {
                    f[k] = p;
                    char *bar = strchr(p, '|');
                    if (k < 6) { if (!bar) { ok = 0; break; } *bar = '\0'; p = bar + 1; }
                }
            }
            if (ok) {
                uint8_t prev[32], digest[32];
                int ph_ok = 1;
                for (int i = 0; i < 32; i++) {
                    unsigned v = 0;
                    for (int k = 0; k < 2; k++) {
                        char c = f[5][i*2+k];
                        v <<= 4;
                        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
                        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
                        else { ph_ok = 0; break; }
                    }
                    prev[i] = (uint8_t)v;
                }
                if (ph_ok) {
                    /* change "allowed" to "denied", then re-hash the head + prev */
                    char head[640];
                    snprintf(head, sizeof head, "%s|%s|%s|%s|%s|", f[0], f[1], f[2], "allowed", f[4]);   /* the record really changes: denied -> allowed */
                    uint8_t buf[640 + 32];
                    size_t hl = strlen(head);
                    memcpy(buf, head, hl);
                    memcpy(buf + hl, prev, 32);
                    wubu_blake2b(digest, 32, buf, hl + 32, NULL, 0);
                    char forged[900];
                    char prev_hex[65], hash_hex[65];
                    for (int i = 0; i < 32; i++) {
                        static const char *H = "0123456789abcdef";
                        prev_hex[i*2] = H[prev[i] >> 4]; prev_hex[i*2+1] = H[prev[i] & 15];
                        hash_hex[i*2] = H[digest[i] >> 4]; hash_hex[i*2+1] = H[digest[i] & 15];
                    }
                    prev_hex[64] = hash_hex[64] = '\0';
                    snprintf(forged, sizeof forged, "%s%s|%s", head, prev_hex, hash_hex);
                    /* splice the forged line back over the second line */
                    char *rebuilt = malloc((size_t)n + 1024);
                    size_t head_len = (size_t)(line2 - copy);
                    memcpy(rebuilt, copy, head_len);
                    strcpy(rebuilt + head_len, forged);
                    strcat(rebuilt, "\n");
                    strcat(rebuilt, line2_end + 1);
                    check(spit(rebuilt) == 0, "wrote the self-consistent forgery");
                    err = 0;
                    int rc = wubu_agent_log_verify(g_path, &v, &err);
                    check(rc != 0 && v.bad_index == 2, "a self-consistent forgery is caught by the NEXT record");
                    free(rebuilt);
                }
            }
        }
        free(copy);
    }

    /* a broken log must not accept new records */
    {
        WubuAgentLog bad;
        err = 0;
        check(wubu_agent_log_open(&bad, g_path, &err) != 0, "opening a broken log refuses");
    }

    /* a missing file is an IO error for verify, and a fresh start for open */
    {
        char missing[512];
        snprintf(missing, sizeof missing, "%s/wubu_agent_log_absent.txt", tmp);
        remove(missing);
        err = 0;
        check(wubu_agent_log_verify(missing, &v, &err) != 0 && err == WUBU_AGENT_LOG_IO, "verify reports a missing file");
        WubuAgentLog fresh;
        check(wubu_agent_log_open(&fresh, missing, &err) == 0, "open starts a new log when none exists");
    }

    remove(g_path);
    free(orig); free(mut);
    printf("\n=== %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
