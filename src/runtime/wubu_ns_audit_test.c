/*
 * wubu_ns_audit_test.c -- asserts the namespace audit log contract
 * (NAMESPACE #581, #585, #594).
 */
#include "wubu_ns_audit.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

static int contains(const char *hay, const char *needle)
{
    return strstr(hay, needle) != NULL;
}

int main(void)
{
    wubu_ns_audit_t *a = wubu_ns_audit_new();
    CHECK(a != NULL);
    CHECK(wubu_ns_audit_tx_count(a) == 0);
    CHECK(wubu_ns_audit_denial_count(a) == 0);
    CHECK(wubu_ns_audit_sealed_count(a) == 0);
    CHECK(wubu_ns_audit_verify(a) == 0);            /* empty log is intact */

    /* ---- #581: every 9P transaction lands in the log ---- */
    CHECK(wubu_ns_audit_tx(a, "Tversion", 0, "/", 0) == 0);
    CHECK(wubu_ns_audit_tx(a, "Tattach", 1, "/", 0) == 0);
    CHECK(wubu_ns_audit_tx(a, "Twalk", 1, "/n/dev", 0) == 0);
    CHECK(wubu_ns_audit_tx(a, "Tread", 2, "/n/audit/log", 0) == 0);
    CHECK(wubu_ns_audit_tx(a, "Twrite", 3, "/n/dev/net", -13) == 0);
    CHECK(wubu_ns_audit_tx_count(a) == 5);

    /* the /n/audit/log read-back: formatted, ordered, offset-correct */
    char buf[2048];
    size_t n = wubu_ns_audit_log_read(a, 0, buf, sizeof(buf) - 1);
    buf[n] = 0;
    CHECK(n > 0);
    CHECK(contains(buf, "1 Tversion"));
    CHECK(contains(buf, "5 Twrite"));
    CHECK(contains(buf, "/n/dev/net"));
    CHECK(contains(buf, "status=-13"));
    /* sequential numbers appear in order */
    CHECK(strstr(buf, "1 Tversion") < strstr(buf, "5 Twrite"));
    /* offset read: skip the first line, get the rest */
    size_t first = (size_t)(strchr(buf, '\n') - buf) + 1;
    char buf2[2048];
    size_t n2 = wubu_ns_audit_log_read(a, first, buf2, sizeof(buf2) - 1);
    buf2[n2] = 0;
    CHECK(n2 > 0);
    CHECK(!contains(buf2, "Tversion"));
    CHECK(contains(buf2, "Tattach"));
    /* end of file reads 0 bytes */
    CHECK(wubu_ns_audit_log_read(a, 100000, buf2, sizeof(buf2)) == 0);

    /* ---- #585: the denial trace at /n/audit/denied ---- */
    CHECK(wubu_ns_audit_denial(a, "/n/hostctl", 0x2ULL) == 0);
    CHECK(wubu_ns_audit_denial(a, "/n/audit/log", 0x1ULL) == 0);
    CHECK(wubu_ns_audit_denial_count(a) == 2);
    n = wubu_ns_audit_denied_read(a, 0, buf, sizeof(buf) - 1);
    buf[n] = 0;
    CHECK(n > 0);
    CHECK(contains(buf, "/n/hostctl"));
    CHECK(contains(buf, "missing=2"));

    /* ---- integrity before rotation ---- */
    CHECK(wubu_ns_audit_verify(a) == 0);
    uint64_t d1 = wubu_ns_audit_replay_digest(a);
    uint64_t d2 = wubu_ns_audit_replay_digest(a);
    CHECK(d1 == d2);                                 /* deterministic */

    /* ---- #594: rotation + deterministic replay across it ---- */
    /* fill the active segment: the log refuses to drop records silently */
    int rc = 0;
    for (int i = 0; i < WUBU_NS_AUDIT_CAP - 5; i++) {
        rc = wubu_ns_audit_tx(a, "Tread", 7, "/n/fill", 0);
        if (rc != 0) break;
    }
    CHECK(rc == 0);
    CHECK(wubu_ns_audit_tx(a, "Tread", 7, "/n/over", 0) == -2);  /* full */
    CHECK(wubu_ns_audit_rotate(a) == 0);
    CHECK(wubu_ns_audit_sealed_count(a) == 1);
    CHECK(wubu_ns_audit_tx(a, "Tcreate", 9, "/n/post-rotate", 0) == 0);
    CHECK(wubu_ns_audit_tx_count(a) == WUBU_NS_AUDIT_CAP + 1);
    /* the whole log replays: pre-rotation + post-rotation records
     * (257 records at ~40 bytes a line -- the buffer must span them) */
    static char big[32768];
    n = wubu_ns_audit_log_read(a, 0, big, sizeof(big) - 1);
    big[n] = 0;
    CHECK(contains(big, "Tversion"));                /* segment 0 */
    CHECK(contains(big, "post-rotate"));            /* segment 1 */
    CHECK(wubu_ns_audit_verify(a) == 0);             /* chain crosses over */
    uint64_t d3 = wubu_ns_audit_replay_digest(a);
    CHECK(d3 != d1);                                  /* new records => new digest */
    CHECK(wubu_ns_audit_replay_digest(a) == d3);      /* still deterministic */
    /* a second rotation works; a forged extra record is still impossible */
    CHECK(wubu_ns_audit_rotate(a) == 0);
    CHECK(wubu_ns_audit_sealed_count(a) == 2);
    CHECK(wubu_ns_audit_verify(a) == 0);
    CHECK(wubu_ns_audit_replay_digest(a) == d3);      /* no new records */

    /* ---- fail-closed ---- */
    CHECK(wubu_ns_audit_tx(NULL, "Tread", 1, "/x", 0) == -1);
    CHECK(wubu_ns_audit_tx(a, "", 1, "/x", 0) == -1);
    CHECK(wubu_ns_audit_tx(a, "Tread", 1, NULL, 0) == -1);
    CHECK(wubu_ns_audit_denial(NULL, "/x", 1) == -1);
    CHECK(wubu_ns_audit_denial(a, "", 1) == -1);
    CHECK(wubu_ns_audit_log_read(NULL, 0, buf, sizeof(buf)) == 0);
    CHECK(wubu_ns_audit_verify(NULL) == -1);
    /* rotate out of segments: refuses to wrap silently */
    CHECK(wubu_ns_audit_rotate(a) == 0);
    CHECK(wubu_ns_audit_rotate(a) == -2);
    CHECK(wubu_ns_audit_verify(a) == 0);

    wubu_ns_audit_free(a);

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
